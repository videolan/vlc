/*****************************************************************************
 * mediacodec.c: Video decoder module using the Android MediaCodec API
 *****************************************************************************
 * Copyright (C) 2012 Martin Storsjo
 *
 * Authors: Martin Storsjo <martin@martin.st>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_timestamp_helper.h>
#include <vlc_threads.h>
#include <vlc_bits.h>

#include "mediacodec.h"
#include "../codec/hxxx_helper.h"
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"

#define BLOCK_FLAG_CSD (0x01 << BLOCK_FLAG_PRIVATE_SHIFT)

#define DECODE_FLAG_RESTART (0x01)
#define DECODE_FLAG_DRAIN (0x02)

#define MAX_PIC 64

/**
 * Callback called when a new block is processed from DecodeBlock.
 * It returns -1 in case of error, 0 if block should be dropped, 1 otherwise.
 */
typedef int (*dec_on_new_block_cb)(decoder_t *, block_t **);

/**
 * Callback called when decoder is flushing.
 */
typedef void (*dec_on_flush_cb)(decoder_t *);

/**
 * Callback called when DecodeBlock try to get an output buffer (pic or block).
 * It returns -1 in case of error, or the number of output buffer returned.
 */
typedef int (*dec_process_output_cb)(decoder_t *, mc_api_out *, picture_t **,
                                     block_t **);

struct android_picture_ctx
{
    picture_context_t s;
    atomic_uint refs;
    atomic_int index;
};

typedef struct
{
    mc_api api;

    /* Codec Specific Data buffer: sent in DecodeBlock after a start or a flush
     * with the BUFFER_FLAG_CODEC_CONFIG flag.*/
    #define MAX_CSD_COUNT 3
    block_t *pp_csd[MAX_CSD_COUNT];
    size_t i_csd_count;
    size_t i_csd_send;

    bool b_has_format;

    int64_t i_preroll_end;
    int     i_quirks;

    /* Specific Audio/Video callbacks */
    dec_on_new_block_cb     pf_on_new_block;
    dec_on_flush_cb         pf_on_flush;
    dec_process_output_cb   pf_process_output;

    vlc_mutex_t     lock;
    vlc_thread_t    out_thread;
    /* Cond used to signal the output thread */
    vlc_cond_t      cond;
    /* Cond used to signal the decoder thread */
    vlc_cond_t      dec_cond;
    /* Set to true by pf_flush to signal the output thread to flush */
    bool            b_flush_out;
    /* If true, the output thread will start to dequeue output pictures */
    bool            b_output_ready;
    /* If true, the first input block was successfully dequeued */
    bool            b_input_dequeued;
    bool            b_aborted;
    bool            b_drained;
    bool            b_adaptive;
    int             i_decode_flags;

    enum es_format_category_e cat;
    union
    {
        struct
        {
            vlc_video_context *ctx;
            struct android_picture_ctx apic_ctxs[MAX_PIC];
            void *p_surface, *p_jsurface;
            unsigned i_angle;
            unsigned i_input_width, i_input_height;
            unsigned int i_stride, i_slice_height;
            int i_pixel_format;
            struct hxxx_helper hh;
            timestamp_fifo_t *timestamp_fifo;
            int i_mpeg_dar_num, i_mpeg_dar_den;
            struct vlc_asurfacetexture *surfacetexture;
        } video;
        struct {
            date_t i_end_date;
            int i_channels;
            bool b_extract;
            /* Some audio decoders need a valid channel count */
            bool b_need_channels;
            int pi_extraction[AOUT_CHAN_MAX];
        } audio;
    };
} decoder_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoderJni(vlc_object_t *);
static int  OpenDecoderNdk(vlc_object_t *);
static void CleanDecoder(decoder_sys_t *);
static void CloseDecoder(vlc_object_t *);

static int Video_OnNewBlock(decoder_t *, block_t **);
static int VideoHXXX_OnNewBlock(decoder_t *, block_t **);
static int VideoMPEG2_OnNewBlock(decoder_t *, block_t **);
static int VideoVC1_OnNewBlock(decoder_t *, block_t **);
static void Video_OnFlush(decoder_t *);
static int Video_ProcessOutput(decoder_t *, mc_api_out *, picture_t **,
                               block_t **);
static int DecodeBlock(decoder_t *, block_t *);

static int Audio_OnNewBlock(decoder_t *, block_t **);
static void Audio_OnFlush(decoder_t *);
static int Audio_ProcessOutput(decoder_t *, mc_api_out *, picture_t **,
                               block_t **);

static void DecodeFlushLocked(decoder_t *);
static void DecodeFlush(decoder_t *);
static void StopMediaCodec(decoder_sys_t *);
static void *OutThread(void *);

static void ReleaseAllPictureContexts(decoder_sys_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MEDIACODEC_ENABLE_TEXT N_("Enable hardware acceleration")

#define DIRECTRENDERING_TEXT "Android direct rendering"
#define DIRECTRENDERING_LONGTEXT \
    "Enable Android direct rendering using opaque buffers."

#define MEDIACODEC_AUDIO_TEXT "Use MediaCodec for audio decoding"
#define MEDIACODEC_AUDIO_LONGTEXT "Still experimental."

#define MEDIACODEC_TUNNELEDPLAYBACK_TEXT "Use a tunneled surface for playback"

#define CFG_PREFIX "mediacodec-"

vlc_module_begin ()
    set_description("Video decoder using Android MediaCodec via NDK")
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_section(N_("Decoding"), NULL)
    set_capability("video decoder", 800)
    add_bool("mediacodec", true, MEDIACODEC_ENABLE_TEXT, NULL, false)
    add_bool(CFG_PREFIX "dr", true,
             DIRECTRENDERING_TEXT, DIRECTRENDERING_LONGTEXT, true)
    add_bool(CFG_PREFIX "audio", false,
             MEDIACODEC_AUDIO_TEXT, MEDIACODEC_AUDIO_LONGTEXT, true)
    add_bool(CFG_PREFIX "tunneled-playback", false,
             MEDIACODEC_TUNNELEDPLAYBACK_TEXT, NULL, true)
    set_callbacks(OpenDecoderNdk, CloseDecoder)
    add_shortcut("mediacodec_ndk")
    add_submodule ()
        set_capability("audio decoder", 0)
        set_callbacks(OpenDecoderNdk, CloseDecoder)
        add_shortcut("mediacodec_ndk")
    add_submodule ()
        set_description("Video decoder using Android MediaCodec via JNI")
        set_capability("video decoder", 0)
        set_callbacks(OpenDecoderJni, CloseDecoder)
        add_shortcut("mediacodec_jni")
    add_submodule ()
        set_capability("audio decoder", 0)
        set_callbacks(OpenDecoderJni, CloseDecoder)
        add_shortcut("mediacodec_jni")
vlc_module_end ()

static void CSDFree(decoder_sys_t *p_sys)
{
    for (unsigned int i = 0; i < p_sys->i_csd_count; ++i)
        block_Release(p_sys->pp_csd[i]);
    p_sys->i_csd_count = 0;
}

/* Init the p_sys->p_csd that will be sent from DecodeBlock */
static void CSDInit(decoder_sys_t *p_sys, block_t *p_blocks, size_t i_count)
{
    assert(i_count <= MAX_CSD_COUNT);

    CSDFree(p_sys);

    for (size_t i = 0; i < i_count; ++i)
    {
        assert(p_blocks != NULL);
        p_sys->pp_csd[i] = p_blocks;
        p_sys->pp_csd[i]->i_flags = BLOCK_FLAG_CSD;
        p_blocks = p_blocks->p_next;
        p_sys->pp_csd[i]->p_next = NULL;
    }

    p_sys->i_csd_count = i_count;
    p_sys->i_csd_send = 0;
}

static int CSDDup(decoder_sys_t *p_sys, const void *p_buf, size_t i_buf)
{
    block_t *p_block = block_Alloc(i_buf);
    if (!p_block)
        return VLC_ENOMEM;
    memcpy(p_block->p_buffer, p_buf, i_buf);

    CSDInit(p_sys, p_block, 1);
    return VLC_SUCCESS;
}

static void HXXXInitSize(decoder_t *p_dec, bool *p_size_changed)
{
    if (p_size_changed)
    {
        decoder_sys_t *p_sys = p_dec->p_sys;
        struct hxxx_helper *hh = &p_sys->video.hh;
        unsigned i_w, i_h, i_vw, i_vh;
        hxxx_helper_get_current_picture_size(hh, &i_w, &i_h, &i_vw, &i_vh);

        *p_size_changed = (i_w != p_sys->video.i_input_width
                        || i_h != p_sys->video.i_input_height);
        p_sys->video.i_input_width = i_w;
        p_sys->video.i_input_height = i_h;
        /* fmt_out video size will be updated by mediacodec output callback */
    }
}

/* Fill the p_sys->p_csd struct with H264 Parameter Sets */
static int H264SetCSD(decoder_t *p_dec, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;
    assert(hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0);

    block_t *p_spspps_blocks = h264_helper_get_annexb_config(hh);

    if (p_spspps_blocks != NULL)
        CSDInit(p_sys, p_spspps_blocks, 2);

    HXXXInitSize(p_dec, p_size_changed);

    return VLC_SUCCESS;
}

/* Fill the p_sys->p_csd struct with HEVC Parameter Sets */
static int HEVCSetCSD(decoder_t *p_dec, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    assert(hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
           hh->hevc.i_pps_count > 0 );

    block_t *p_xps_blocks = hevc_helper_get_annexb_config(hh);
    if (p_xps_blocks != NULL)
    {
        block_t *p_monolith = block_ChainGather(p_xps_blocks);
        if (p_monolith == NULL)
        {
            block_ChainRelease(p_xps_blocks);
            return VLC_ENOMEM;
        }
        CSDInit(p_sys, p_monolith, 1);
    }

    HXXXInitSize(p_dec, p_size_changed);
    return VLC_SUCCESS;
}

static int ParseVideoExtraH264(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    int i_ret = hxxx_helper_set_extra(hh, p_extra, i_extra);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    assert(hh->pf_process_block != NULL);

    if (p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
        p_sys->b_adaptive = true;

    p_sys->pf_on_new_block = VideoHXXX_OnNewBlock;

    if (hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0)
        return H264SetCSD(p_dec, NULL);
    return VLC_SUCCESS;
}

static int ParseVideoExtraHEVC(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    int i_ret = hxxx_helper_set_extra(hh, p_extra, i_extra);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    assert(hh->pf_process_block != NULL);

    if (p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
        p_sys->b_adaptive = true;

    p_sys->pf_on_new_block = VideoHXXX_OnNewBlock;

    if (hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
        hh->hevc.i_pps_count > 0 )
        return HEVCSetCSD(p_dec, NULL);
    return VLC_SUCCESS;
}

static int ParseVideoExtraVc1(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int offset = 0;

    if (i_extra < 4)
        return VLC_EGENERIC;

    /* Initialisation data starts with : 0x00 0x00 0x01 0x0f */
    /* Skipping unecessary data */
    static const uint8_t vc1_start_code[4] = {0x00, 0x00, 0x01, 0x0f};
    for (; offset < i_extra - 4 ; ++offset)
    {
        if (!memcmp(&p_extra[offset], vc1_start_code, 4))
            break;
    }

    /* Could not find the sequence header start code */
    if (offset >= i_extra - 4)
        return VLC_EGENERIC;

    p_sys->pf_on_new_block = VideoVC1_OnNewBlock;
    return CSDDup(p_sys, p_extra + offset, i_extra - offset);
}

static int ParseVideoExtraWmv3(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    /* WMV3 initialisation data :
     * 8 fixed bytes
     * 4 extradata bytes
     * 4 height bytes (little endian)
     * 4 width bytes (little endian)
     * 16 fixed bytes */

    if (i_extra < 4)
        return VLC_EGENERIC;

    uint8_t p_data[36] = {
        0x8e, 0x01, 0x00, 0xc5, /* Fixed bytes values */
        0x04, 0x00, 0x00, 0x00, /* Same */
        0x00, 0x00, 0x00, 0x00, /* extradata emplacement */
        0x00, 0x00, 0x00, 0x00, /* height emplacement (little endian) */
        0x00, 0x00, 0x00, 0x00, /* width emplacement (little endian) */
        0x0c, 0x00, 0x00, 0x00, /* Fixed byte pattern */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    /* Adding extradata */
    memcpy(&p_data[8], p_extra, 4);
    /* Adding height and width, little endian */
    SetDWLE(&(p_data[12]), p_dec->fmt_in.video.i_height);
    SetDWLE(&(p_data[16]), p_dec->fmt_in.video.i_width);

    return CSDDup(p_dec->p_sys, p_data, sizeof(p_data));
}

static int ParseExtra(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_extra = p_dec->fmt_in.p_extra;
    int i_extra = p_dec->fmt_in.i_extra;

    switch (p_dec->fmt_in.i_codec)
    {
    case VLC_CODEC_H264:
        return ParseVideoExtraH264(p_dec, p_extra, i_extra);
    case VLC_CODEC_HEVC:
        return ParseVideoExtraHEVC(p_dec, p_extra, i_extra);
    case VLC_CODEC_WMV3:
        return ParseVideoExtraWmv3(p_dec, p_extra, i_extra);
    case VLC_CODEC_VC1:
        return ParseVideoExtraVc1(p_dec, p_extra, i_extra);
    case VLC_CODEC_MP4V:
        if (!i_extra && p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
            p_sys->b_adaptive = true;
        break;
    case VLC_CODEC_MPGV:
    case VLC_CODEC_MP2V:
        p_sys->pf_on_new_block = VideoMPEG2_OnNewBlock;
        break;
    }
    /* Set default CSD */
    if (p_dec->fmt_in.i_extra)
        return CSDDup(p_sys, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
    else
        return VLC_SUCCESS;
}

/*****************************************************************************
 * StartMediaCodec: Create the mediacodec instance
 *****************************************************************************/
static int StartMediaCodec(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    union mc_api_args args;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        args.video.i_width = p_dec->fmt_out.video.i_width;
        args.video.i_height = p_dec->fmt_out.video.i_height;
        args.video.i_angle = p_sys->video.i_angle;

        args.video.p_surface = p_sys->video.p_surface;
        args.video.p_jsurface = p_sys->video.p_jsurface;

        args.video.b_tunneled_playback = args.video.p_surface ?
                var_InheritBool(p_dec, CFG_PREFIX "tunneled-playback") : false;
        if (p_sys->b_adaptive)
            msg_Dbg(p_dec, "mediacodec configured for adaptative playback");
        args.video.b_adaptive_playback = p_sys->b_adaptive;
    }
    else
    {
        date_Set(&p_sys->audio.i_end_date, VLC_TICK_INVALID);

        args.audio.i_sample_rate    = p_dec->fmt_in.audio.i_rate;
        args.audio.i_channel_count  = p_sys->audio.i_channels;
    }

    if (p_sys->api.configure_decoder(&p_sys->api, &args) != 0)
    {
        return MC_API_ERROR;
    }

    return p_sys->api.start(&p_sys->api);
}

/*****************************************************************************
 * StopMediaCodec: Close the mediacodec instance
 *****************************************************************************/
static void StopMediaCodec(decoder_sys_t *p_sys)
{
    /* Remove all pictures that are currently in flight in order
     * to prevent the vout from using destroyed output buffers. */
    if (p_sys->cat == VIDEO_ES)
        ReleaseAllPictureContexts(p_sys);

    p_sys->api.stop(&p_sys->api);
}

static bool AndroidPictureContextRelease(struct android_picture_ctx *apctx,
                                         bool render)
{
    int index = atomic_exchange(&apctx->index, -1);
    if (index >= 0)
    {
        android_video_context_t *avctx =
            vlc_video_context_GetPrivate(apctx->s.vctx, VLC_VIDEO_CONTEXT_AWINDOW);
        decoder_sys_t *p_sys = avctx->dec_opaque;

        p_sys->api.release_out(&p_sys->api, index, render);
        return true;
    }
    return false;
}

static bool PictureContextRenderPic(struct picture_context_t *ctx)
{
    struct android_picture_ctx *apctx =
        container_of(ctx, struct android_picture_ctx, s);

    return AndroidPictureContextRelease(apctx, true);
}

static bool PictureContextRenderPicTs(struct picture_context_t *ctx,
                                      vlc_tick_t ts)
{
    struct android_picture_ctx *apctx =
        container_of(ctx, struct android_picture_ctx, s);

    int index = atomic_exchange(&apctx->index, -1);
    if (index >= 0)
    {
        android_video_context_t *avctx =
            vlc_video_context_GetPrivate(ctx->vctx, VLC_VIDEO_CONTEXT_AWINDOW);
        decoder_sys_t *p_sys = avctx->dec_opaque;

        p_sys->api.release_out_ts(&p_sys->api, index, ts * INT64_C(1000));
        return true;
    }
    return false;
}

static void PictureContextDestroy(struct picture_context_t *ctx)
{
    struct android_picture_ctx *apctx =
        container_of(ctx, struct android_picture_ctx, s);

    if (atomic_fetch_sub_explicit(&apctx->refs, 1, memory_order_acq_rel) == 1)
        AndroidPictureContextRelease(apctx, false);
}

static struct picture_context_t *PictureContextCopy(struct picture_context_t *ctx)
{
    struct android_picture_ctx *apctx =
        container_of(ctx, struct android_picture_ctx, s);

    atomic_fetch_add_explicit(&apctx->refs, 1, memory_order_relaxed);
    vlc_video_context_Hold(ctx->vctx);
    return ctx;
}

static void CleanFromVideoContext(void *priv)
{
    android_video_context_t *avctx = priv;
    decoder_sys_t *p_sys = avctx->dec_opaque;

    CleanDecoder(p_sys);
}

static void ReleaseAllPictureContexts(decoder_sys_t *p_sys)
{
    for (size_t i = 0; i < ARRAY_SIZE(p_sys->video.apic_ctxs); ++i)
    {
        struct android_picture_ctx *apctx = &p_sys->video.apic_ctxs[i];

        /* Don't decrement apctx->refs, the picture_context should stay valid
         * even if the underlying buffer is released since it might still be
         * used by the vout (but the vout won't be able to render it). */
        AndroidPictureContextRelease(apctx, false);
    }
}

static struct android_picture_ctx *
GetPictureContext(decoder_t *p_dec, unsigned index)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bool slept = false;

    for (;;)
    {
        for (size_t i = 0; i < ARRAY_SIZE(p_sys->video.apic_ctxs); ++i)
        {
            struct android_picture_ctx *apctx = &p_sys->video.apic_ctxs[i];
            /* Find an available picture context (ie. refs == 0) */
            unsigned expected_refs = 0;
            if (atomic_compare_exchange_strong(&apctx->refs, &expected_refs, 1))
            {
                int expected_index = -1;
                /* Store the new index */
                if (likely(atomic_compare_exchange_strong(&apctx->index,
                                                          &expected_index, index)))
                    return apctx;

                /* Unlikely: Restore the ref count and try a next one, since
                 * this picture context is being released. Cf.
                 * PictureContextDestroy(), this function first decrement the
                 * ref count before releasing the index.  */
                atomic_store(&apctx->refs, 0);
            }
        }

        /* This is very unlikely since there are generally more picture
         * contexts than android MediaCodec buffers */
        if (!slept)
            msg_Warn(p_dec, "waiting for more picture contexts (unlikely)");
        vlc_tick_sleep(VOUT_OUTMEM_SLEEP);
        slept = true;
    }
}

static int
CreateVideoContext(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
    if (!dec_dev || dec_dev->type != VLC_DECODER_DEVICE_AWINDOW)
    {
        msg_Err(p_dec, "Could not find an AWINDOW decoder device");
        return VLC_EGENERIC;
    }

    assert(dec_dev->opaque);
    AWindowHandler *awh = dec_dev->opaque;

    const bool has_subtitle_surface =
        AWindowHandler_getANativeWindow(awh, AWindow_Subtitles) != NULL;

    /* Force OpenGL interop (via AWindow_SurfaceTexture) if there is a
     * projection or an orientation to handle, if the Surface owner is not able
     * to modify its layout, or if there is no external subtitle surfaces. */

    bool use_surfacetexture =
        p_dec->fmt_out.video.projection_mode != PROJECTION_MODE_RECTANGULAR
     || (!p_sys->api.b_support_rotation && p_dec->fmt_out.video.orientation != ORIENT_NORMAL)
     || !AWindowHandler_canSetVideoLayout(awh)
     || !has_subtitle_surface;

    if (use_surfacetexture)
    {
        p_sys->video.surfacetexture = vlc_asurfacetexture_New(awh);
        if (p_sys->video.surfacetexture == NULL)
            goto error;
        p_sys->video.p_surface = p_sys->video.surfacetexture->window;
        p_sys->video.p_jsurface = p_sys->video.surfacetexture->jsurface;
    }
    else
    {
        p_sys->video.p_surface = AWindowHandler_getANativeWindow(awh, AWindow_Video);
        p_sys->video.p_jsurface = AWindowHandler_getSurface(awh, AWindow_Video);
        assert (p_sys->video.p_surface);
        if (!p_sys->video.p_surface)
        {
            msg_Err(p_dec, "Could not find a valid ANativeWindow");
            goto error;
        }
    }

    static const struct vlc_video_context_operations ops =
    {
        .destroy = CleanFromVideoContext,
    };
    p_sys->video.ctx =
        vlc_video_context_Create(dec_dev, VLC_VIDEO_CONTEXT_AWINDOW,
                                 sizeof(android_video_context_t), &ops);
    vlc_decoder_device_Release(dec_dev);

    if (!p_sys->video.ctx)
        return VLC_EGENERIC;

    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(p_sys->video.ctx, VLC_VIDEO_CONTEXT_AWINDOW);
    avctx->texture = p_sys->video.surfacetexture;
    avctx->dec_opaque = p_dec->p_sys;
    avctx->render = PictureContextRenderPic;
    avctx->render_ts = p_sys->api.release_out_ts ? PictureContextRenderPicTs : NULL;

    for (size_t i = 0; i < ARRAY_SIZE(p_sys->video.apic_ctxs); ++i)
    {
        struct android_picture_ctx *apctx = &p_sys->video.apic_ctxs[i];

        apctx->s = (picture_context_t) {
            PictureContextDestroy, PictureContextCopy,
            p_sys->video.ctx,
        };
        atomic_init(&apctx->index, -1);
        atomic_init(&apctx->refs, 0);
    }

    return VLC_SUCCESS;

error:
    vlc_decoder_device_Release(dec_dev);
    return VLC_EGENERIC;
}

static void CleanInputVideo(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264
         || p_dec->fmt_in.i_codec == VLC_CODEC_HEVC)
            hxxx_helper_clean(&p_sys->video.hh);

        if (p_sys->video.timestamp_fifo)
            timestamp_FifoRelease(p_sys->video.timestamp_fifo);
    }
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this, pf_MediaCodecApi_init pf_init)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (!var_InheritBool(p_dec, "mediacodec"))
        return VLC_EGENERIC;

    decoder_sys_t *p_sys;
    int i_ret;
    int i_profile = p_dec->fmt_in.i_profile;
    const char *mime = NULL;

    /* Video or Audio if "mediacodec-audio" bool is true */
    if (p_dec->fmt_in.i_cat != VIDEO_ES && (p_dec->fmt_in.i_cat != AUDIO_ES
     || !var_InheritBool(p_dec, CFG_PREFIX "audio")))
        return VLC_EGENERIC;

    /* Fail if this module already failed to decode this ES */
    if (var_Type(p_dec, "mediacodec-failed") != 0)
        return VLC_EGENERIC;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        /* Not all mediacodec versions can handle a size of 0. Hopefully, the
         * packetizer will trigger a decoder restart when a new video size is
         * found. */
        if (!p_dec->fmt_in.video.i_width || !p_dec->fmt_in.video.i_height)
            return VLC_EGENERIC;

        switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_HEVC:
            if (i_profile == -1)
            {
                uint8_t i_hevc_profile;
                if (hevc_get_profile_level(&p_dec->fmt_in, &i_hevc_profile, NULL, NULL))
                    i_profile = i_hevc_profile;
            }
            mime = "video/hevc";
            break;
        case VLC_CODEC_H264:
            if (i_profile == -1)
            {
                uint8_t i_h264_profile;
                if (h264_get_profile_level(&p_dec->fmt_in, &i_h264_profile, NULL, NULL))
                    i_profile = i_h264_profile;
            }
            mime = "video/avc";
            break;
        case VLC_CODEC_H263: mime = "video/3gpp"; break;
        case VLC_CODEC_MP4V: mime = "video/mp4v-es"; break;
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP2V:
            mime = "video/mpeg2";
            break;
        case VLC_CODEC_WMV3: mime = "video/x-ms-wmv"; break;
        case VLC_CODEC_VC1:  mime = "video/wvc1"; break;
        case VLC_CODEC_VP8:  mime = "video/x-vnd.on2.vp8"; break;
        case VLC_CODEC_VP9:  mime = "video/x-vnd.on2.vp9"; break;
        }
    }
    else
    {
        switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_AMR_NB: mime = "audio/3gpp"; break;
        case VLC_CODEC_AMR_WB: mime = "audio/amr-wb"; break;
        case VLC_CODEC_MPGA:
        case VLC_CODEC_MP3:    mime = "audio/mpeg"; break;
        case VLC_CODEC_MP2:    mime = "audio/mpeg-L2"; break;
        case VLC_CODEC_MP4A:   mime = "audio/mp4a-latm"; break;
        case VLC_CODEC_QCELP:  mime = "audio/qcelp"; break;
        case VLC_CODEC_VORBIS: mime = "audio/vorbis"; break;
        case VLC_CODEC_OPUS:   mime = "audio/opus"; break;
        case VLC_CODEC_ALAW:   mime = "audio/g711-alaw"; break;
        case VLC_CODEC_MULAW:  mime = "audio/g711-mlaw"; break;
        case VLC_CODEC_FLAC:   mime = "audio/flac"; break;
        case VLC_CODEC_GSM:    mime = "audio/gsm"; break;
        case VLC_CODEC_A52:    mime = "audio/ac3"; break;
        case VLC_CODEC_EAC3:   mime = "audio/eac3"; break;
        case VLC_CODEC_ALAC:   mime = "audio/alac"; break;
        case VLC_CODEC_DTS:    mime = "audio/vnd.dts"; break;
        /* case VLC_CODEC_: mime = "audio/mpeg-L1"; break; */
        /* case VLC_CODEC_: mime = "audio/aac-adts"; break; */
        }
    }
    if (!mime)
    {
        msg_Dbg(p_dec, "codec %4.4s not supported",
                (char *)&p_dec->fmt_in.i_codec);
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_sys = calloc(1, sizeof(*p_sys))) == NULL)
        return VLC_ENOMEM;

    p_sys->api.p_obj = p_this;
    p_sys->api.i_codec = p_dec->fmt_in.i_codec;
    p_sys->api.i_cat = p_dec->fmt_in.i_cat;
    p_sys->api.psz_mime = mime;
    p_sys->video.i_mpeg_dar_num = 0;
    p_sys->video.i_mpeg_dar_den = 0;
    p_sys->video.surfacetexture = NULL;

    if (pf_init(&p_sys->api) != 0)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }
    if (p_sys->api.prepare(&p_sys->api, i_profile) != 0)
    {
        /* If the device can't handle video/wvc1,
         * it can probably handle video/x-ms-wmv */
        if (!strcmp(mime, "video/wvc1") && p_dec->fmt_in.i_codec == VLC_CODEC_VC1)
        {
            p_sys->api.psz_mime = "video/x-ms-wmv";
            if (p_sys->api.prepare(&p_sys->api, i_profile) != 0)
            {
                p_sys->api.clean(&p_sys->api);
                free(p_sys);
                return (VLC_EGENERIC);
            }
        }
        else
        {
            p_sys->api.clean(&p_sys->api);
            free(p_sys);
            return VLC_EGENERIC;
        }
    }

    p_dec->p_sys = p_sys;

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->cond);
    vlc_cond_init(&p_sys->dec_cond);

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            hxxx_helper_init(&p_sys->video.hh, VLC_OBJECT(p_dec),
                             p_dec->fmt_in.i_codec, false);
            break;
        }
        p_sys->pf_on_new_block = Video_OnNewBlock;
        p_sys->pf_on_flush = Video_OnFlush;
        p_sys->pf_process_output = Video_ProcessOutput;

        p_sys->video.timestamp_fifo = timestamp_FifoNew(32);
        if (!p_sys->video.timestamp_fifo)
            goto bailout;

        if (var_InheritBool(p_dec, CFG_PREFIX "dr"))
        {
            /* Direct rendering: Request a valid OPAQUE Vout in order to get
             * the surface attached to it */
            p_dec->fmt_out.i_codec = VLC_CODEC_ANDROID_OPAQUE;

            p_dec->fmt_out.video = p_dec->fmt_in.video;
            if (p_dec->fmt_out.video.i_sar_num * p_dec->fmt_out.video.i_sar_den == 0)
            {
                p_dec->fmt_out.video.i_sar_num = 1;
                p_dec->fmt_out.video.i_sar_den = 1;
            }

            p_sys->video.i_input_width =
            p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
            p_sys->video.i_input_height =
            p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;

            if (CreateVideoContext(p_dec) != VLC_SUCCESS)
            {
                msg_Err(p_dec, "video context creation failed");
                goto bailout;
            }

            android_video_context_t *avctx =
                vlc_video_context_GetPrivate(p_sys->video.ctx,
                                             VLC_VIDEO_CONTEXT_AWINDOW);

            if (p_sys->api.b_support_rotation && avctx->texture == NULL)
            {
                switch (p_dec->fmt_in.video.orientation)
                {
                    case ORIENT_ROTATED_90:
                        p_sys->video.i_angle = 90;
                        break;
                    case ORIENT_ROTATED_180:
                        p_sys->video.i_angle = 180;
                        break;
                    case ORIENT_ROTATED_270:
                        p_sys->video.i_angle = 270;
                        break;
                    default:
                        p_sys->video.i_angle = 0;
                        break;
                }
            }
            else
            {
                /* Let the GL vout handle the rotation */
                p_sys->video.i_angle = 0;
            }

        }
        p_sys->cat = VIDEO_ES;
    }
    else
    {
        p_sys->pf_on_new_block = Audio_OnNewBlock;
        p_sys->pf_on_flush = Audio_OnFlush;
        p_sys->pf_process_output = Audio_ProcessOutput;
        p_sys->audio.i_channels = p_dec->fmt_in.audio.i_channels;

        if ((p_sys->api.i_quirks & MC_API_AUDIO_QUIRKS_NEED_CHANNELS)
         && !p_sys->audio.i_channels)
        {
            msg_Warn(p_dec, "codec need a valid channel count");
            goto bailout;
        }

        p_dec->fmt_out.audio = p_dec->fmt_in.audio;
        p_sys->cat = AUDIO_ES;
    }

    /* Try first to configure CSD */
    if (ParseExtra(p_dec) != VLC_SUCCESS)
        goto bailout;

    if ((p_sys->api.i_quirks & MC_API_QUIRKS_NEED_CSD) && !p_sys->i_csd_count
     && !p_sys->b_adaptive)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            break; /* CSDs will come from hxxx_helper */
        default:
            msg_Warn(p_dec, "Not CSD found for %4.4s",
                     (const char *) &p_dec->fmt_in.i_codec);
            goto bailout;
        }
    }

    i_ret = StartMediaCodec(p_dec);
    if (i_ret != VLC_SUCCESS)
    {
        msg_Err(p_dec, "StartMediaCodec failed");
        goto bailout;
    }

    if (vlc_clone(&p_sys->out_thread, OutThread, p_dec,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err(p_dec, "vlc_clone failed");
        vlc_mutex_unlock(&p_sys->lock);
        goto bailout;
    }

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = DecodeFlush;

    return VLC_SUCCESS;

bailout:
    CleanInputVideo(p_dec);
    CleanDecoder(p_sys);
    return VLC_EGENERIC;
}

static int OpenDecoderNdk(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecNdk_Init);
}

static int OpenDecoderJni(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecJni_Init);
}

static void AbortDecoderLocked(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_sys->b_aborted)
    {
        p_sys->b_aborted = true;
        vlc_cond_broadcast(&p_sys->cond);
    }
}

static void CleanDecoder(decoder_sys_t *p_sys)
{
    StopMediaCodec(p_sys);

    CSDFree(p_sys);
    p_sys->api.clean(&p_sys->api);

    if (p_sys->video.surfacetexture)
        vlc_asurfacetexture_Delete(p_sys->video.surfacetexture);

    free(p_sys);
}

/*****************************************************************************
 * CloseDecoder: Close the decoder instance
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    /* Unblock output thread waiting in dequeue_out */
    DecodeFlushLocked(p_dec);
    /* Cancel the output thread */
    AbortDecoderLocked(p_dec);
    vlc_mutex_unlock(&p_sys->lock);

    vlc_join(p_sys->out_thread, NULL);

    CleanInputVideo(p_dec);

    if (p_sys->video.ctx)
        vlc_video_context_Release(p_sys->video.ctx);
    else
        CleanDecoder(p_sys);
}

static int Video_ProcessOutput(decoder_t *p_dec, mc_api_out *p_out,
                               picture_t **pp_out_pic, block_t **pp_out_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void) pp_out_block;
    assert(pp_out_pic);

    if (p_out->type == MC_OUT_TYPE_BUF)
    {
        picture_t *p_pic = NULL;

        /* If the oldest input block had no PTS, the timestamp of
         * the frame returned by MediaCodec might be wrong so we
         * overwrite it with the corresponding dts. Call FifoGet
         * first in order to avoid a gap if buffers are released
         * due to an invalid format or a preroll */
        int64_t forced_ts = timestamp_FifoGet(p_sys->video.timestamp_fifo);

        if (!p_sys->b_has_format) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (p_out->buf.i_ts <= p_sys->i_preroll_end)
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);

        if (!p_sys->api.b_direct_rendering && p_out->buf.p_ptr == NULL)
        {
            /* This can happen when receiving an EOS buffer */
            msg_Warn(p_dec, "Invalid buffer, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        p_pic = decoder_NewPicture(p_dec);
        if (!p_pic) {
            msg_Warn(p_dec, "NewPicture failed");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (forced_ts == VLC_TICK_INVALID)
            p_pic->date = p_out->buf.i_ts;
        else
            p_pic->date = forced_ts;
        p_pic->b_progressive = true;

        if (p_sys->api.b_direct_rendering)
        {
            struct android_picture_ctx *apctx =
                GetPictureContext(p_dec,p_out->buf.i_index);
            assert(apctx);
            vlc_video_context_Hold(apctx->s.vctx);
            p_pic->context = &apctx->s;
        } else {
            unsigned int chroma_div;
            GetVlcChromaSizes(p_dec->fmt_out.i_codec,
                              p_dec->fmt_out.video.i_width,
                              p_dec->fmt_out.video.i_height,
                              NULL, NULL, &chroma_div);
            CopyOmxPicture(p_sys->video.i_pixel_format, p_pic,
                           p_sys->video.i_slice_height, p_sys->video.i_stride,
                           (uint8_t *)p_out->buf.p_ptr, chroma_div, NULL);

            if (p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false))
            {
                picture_Release(p_pic);
                return -1;
            }
        }
        assert(!(*pp_out_pic));
        *pp_out_pic = p_pic;
        return 1;
    } else {
        assert(p_out->type == MC_OUT_TYPE_CONF);
        p_sys->video.i_pixel_format = p_out->conf.video.pixel_format;

        const char *name = "unknown";
        if (!p_sys->api.b_direct_rendering
         && !GetVlcChromaFormat(p_sys->video.i_pixel_format,
                                &p_dec->fmt_out.i_codec, &name))
        {
            msg_Err(p_dec, "color-format not recognized");
            return -1;
        }

        msg_Err(p_dec, "output: %d %s, %dx%d stride %d %d, crop %d %d %d %d",
                p_sys->video.i_pixel_format, name,
                p_out->conf.video.width, p_out->conf.video.height,
                p_out->conf.video.stride, p_out->conf.video.slice_height,
                p_out->conf.video.crop_left, p_out->conf.video.crop_top,
                p_out->conf.video.crop_right, p_out->conf.video.crop_bottom);

        int i_width  = p_out->conf.video.crop_right + 1
                     - p_out->conf.video.crop_left;
        int i_height = p_out->conf.video.crop_bottom + 1
                     - p_out->conf.video.crop_top;
        if (i_width <= 1 || i_height <= 1)
        {
            i_width = p_out->conf.video.width;
            i_height = p_out->conf.video.height;
        }

        if (!(p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_IGNORE_SIZE))
        {
            p_dec->fmt_out.video.i_visible_width =
            p_dec->fmt_out.video.i_width = i_width;
            p_dec->fmt_out.video.i_visible_height =
            p_dec->fmt_out.video.i_height = i_height;
        }
        else
        {
            p_dec->fmt_out.video.i_visible_width =
            p_dec->fmt_out.video.i_width = p_sys->video.i_input_width;
            p_dec->fmt_out.video.i_visible_height =
            p_dec->fmt_out.video.i_height = p_sys->video.i_input_height;
            msg_Dbg(p_dec, "video size ignored from MediaCodec");
        }

        p_sys->video.i_stride = p_out->conf.video.stride;
        p_sys->video.i_slice_height = p_out->conf.video.slice_height;
        if (p_sys->video.i_stride <= 0)
            p_sys->video.i_stride = p_out->conf.video.width;
        if (p_sys->video.i_slice_height <= 0)
            p_sys->video.i_slice_height = p_out->conf.video.height;

        if (p_sys->video.i_pixel_format == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
            p_sys->video.i_slice_height -= p_out->conf.video.crop_top/2;
        if ((p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_IGNORE_PADDING))
        {
            p_sys->video.i_slice_height = 0;
            p_sys->video.i_stride = p_dec->fmt_out.video.i_width;
        }


        if ((p_dec->fmt_in.i_codec == VLC_CODEC_MPGV ||
             p_dec->fmt_in.i_codec == VLC_CODEC_MP2V) &&
            (p_sys->video.i_mpeg_dar_num * p_sys->video.i_mpeg_dar_den != 0))
        {
            p_dec->fmt_out.video.i_sar_num =
                p_sys->video.i_mpeg_dar_num * p_dec->fmt_out.video.i_height;
            p_dec->fmt_out.video.i_sar_den =
                p_sys->video.i_mpeg_dar_den * p_dec->fmt_out.video.i_width;
        }

        /* If MediaCodec can handle the rotation, reset the orientation to
         * Normal in order to ask the vout not to rotate. */
        if (p_sys->video.i_angle != 0)
        {
            assert(p_dec->fmt_out.i_codec == VLC_CODEC_ANDROID_OPAQUE);
            p_dec->fmt_out.video.orientation = p_dec->fmt_in.video.orientation;
            video_format_TransformTo(&p_dec->fmt_out.video, ORIENT_NORMAL);
        }

        if (decoder_UpdateVideoOutput(p_dec, p_sys->video.ctx) != 0)
        {
            msg_Err(p_dec, "UpdateVout failed");
            return -1;
        }

        p_sys->b_has_format = true;
        return 0;
    }
}

/* samples will be in the following order: FL FR FC LFE BL BR BC SL SR */
static uint32_t pi_audio_order_src[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER, AOUT_CHAN_LFE,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
};

static int Audio_ProcessOutput(decoder_t *p_dec, mc_api_out *p_out,
                               picture_t **pp_out_pic, block_t **pp_out_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void) pp_out_pic;
    assert(pp_out_block);

    if (p_out->type == MC_OUT_TYPE_BUF)
    {
        block_t *p_block = NULL;
        if (p_out->buf.p_ptr == NULL)
        {
            /* This can happen when receiving an EOS buffer */
            msg_Warn(p_dec, "Invalid buffer, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (!p_sys->b_has_format) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        p_block = block_Alloc(p_out->buf.i_size);
        if (!p_block)
            return -1;
        p_block->i_nb_samples = p_out->buf.i_size
                              / p_dec->fmt_out.audio.i_bytes_per_frame;

        if (p_sys->audio.b_extract)
        {
            aout_ChannelExtract(p_block->p_buffer,
                                p_dec->fmt_out.audio.i_channels,
                                p_out->buf.p_ptr, p_sys->audio.i_channels,
                                p_block->i_nb_samples, p_sys->audio.pi_extraction,
                                p_dec->fmt_out.audio.i_bitspersample);
        }
        else
            memcpy(p_block->p_buffer, p_out->buf.p_ptr, p_out->buf.i_size);

        if (p_out->buf.i_ts != 0
         && p_out->buf.i_ts != date_Get(&p_sys->audio.i_end_date))
            date_Set(&p_sys->audio.i_end_date, p_out->buf.i_ts);

        p_block->i_pts = date_Get(&p_sys->audio.i_end_date);
        p_block->i_length = date_Increment(&p_sys->audio.i_end_date,
                                           p_block->i_nb_samples)
                          - p_block->i_pts;

        if (p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false))
        {
            block_Release(p_block);
            return -1;
        }
        *pp_out_block = p_block;
        return 1;
    } else {
        uint32_t i_layout_dst;
        int      i_channels_dst;

        assert(p_out->type == MC_OUT_TYPE_CONF);

        if (p_out->conf.audio.channel_count <= 0
         || p_out->conf.audio.channel_count > 8
         || p_out->conf.audio.sample_rate <= 0)
        {
            msg_Warn(p_dec, "invalid audio properties channels count %d, sample rate %d",
                     p_out->conf.audio.channel_count,
                     p_out->conf.audio.sample_rate);
            return -1;
        }

        msg_Err(p_dec, "output: channel_count: %d, channel_mask: 0x%X, rate: %d",
                p_out->conf.audio.channel_count, p_out->conf.audio.channel_mask,
                p_out->conf.audio.sample_rate);

        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;

        p_dec->fmt_out.audio.i_rate = p_out->conf.audio.sample_rate;
        date_Init(&p_sys->audio.i_end_date, p_out->conf.audio.sample_rate, 1);

        p_sys->audio.i_channels = p_out->conf.audio.channel_count;
        p_sys->audio.b_extract =
            aout_CheckChannelExtraction(p_sys->audio.pi_extraction,
                                        &i_layout_dst, &i_channels_dst,
                                        NULL, pi_audio_order_src,
                                        p_sys->audio.i_channels);

        if (p_sys->audio.b_extract)
            msg_Warn(p_dec, "need channel extraction: %d -> %d",
                     p_sys->audio.i_channels, i_channels_dst);

        p_dec->fmt_out.audio.i_physical_channels = i_layout_dst;
        aout_FormatPrepare(&p_dec->fmt_out.audio);

        if (decoder_UpdateAudioFormat(p_dec))
            return -1;

        p_sys->b_has_format = true;
        return 0;
    }
}

static void DecodeFlushLocked(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bool b_had_input = p_sys->b_input_dequeued;

    p_sys->b_input_dequeued = false;
    p_sys->b_flush_out = true;
    p_sys->i_preroll_end = 0;
    p_sys->b_output_ready = false;
    /* Resend CODEC_CONFIG buffer after a flush */
    p_sys->i_csd_send = 0;

    p_sys->pf_on_flush(p_dec);

    if (b_had_input && p_sys->api.flush(&p_sys->api) != VLC_SUCCESS)
    {
        AbortDecoderLocked(p_dec);
        return;
    }

    vlc_cond_broadcast(&p_sys->cond);

    while (!p_sys->b_aborted && p_sys->b_flush_out)
        vlc_cond_wait(&p_sys->dec_cond, &p_sys->lock);
}

static void DecodeFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    DecodeFlushLocked(p_dec);
    vlc_mutex_unlock(&p_sys->lock);
}

static void *OutThread(void *data)
{
    decoder_t *p_dec = data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    while (!p_sys->b_aborted)
    {
        int i_index;

        /* Wait for output ready */
        if (!p_sys->b_flush_out && !p_sys->b_output_ready) {
            vlc_cond_wait(&p_sys->cond, &p_sys->lock);
            continue;
        }

        if (p_sys->b_flush_out)
        {
            /* Acknowledge flushed state */
            p_sys->b_flush_out = false;
            vlc_cond_broadcast(&p_sys->dec_cond);
            continue;
        }

        vlc_mutex_unlock(&p_sys->lock);

        /* Wait for an output buffer. This function returns when a new output
         * is available or if output is flushed. */
        i_index = p_sys->api.dequeue_out(&p_sys->api, -1);

        vlc_mutex_lock(&p_sys->lock);

        /* Ignore dequeue_out errors caused by flush */
        if (p_sys->b_flush_out)
        {
            /* If i_index >= 0, Release it. There is no way to know if i_index
             * is owned by us, so don't check the error. */
            if (i_index >= 0)
                p_sys->api.release_out(&p_sys->api, i_index, false);

            /* Parse output format/buffers even when we are flushing */
            if (i_index != MC_API_INFO_OUTPUT_FORMAT_CHANGED
             && i_index != MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
                continue;
        }

        /* Process output returned by dequeue_out */
        if (i_index >= 0 || i_index == MC_API_INFO_OUTPUT_FORMAT_CHANGED
         || i_index == MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
        {
            struct mc_api_out out;
            int i_ret = p_sys->api.get_out(&p_sys->api, i_index, &out);

            if (i_ret == 1)
            {
                picture_t *p_pic = NULL;
                block_t *p_block = NULL;

                if (p_sys->pf_process_output(p_dec, &out, &p_pic,
                                             &p_block) == -1 && !out.b_eos)
                {
                    msg_Err(p_dec, "pf_process_output failed");
                    break;
                }
                if (p_pic)
                    decoder_QueueVideo(p_dec, p_pic);
                else if (p_block)
                    decoder_QueueAudio(p_dec, p_block);

                if (out.b_eos)
                {
                    msg_Warn(p_dec, "EOS received");
                    p_sys->b_drained = true;
                    vlc_cond_signal(&p_sys->dec_cond);
                }
            } else if (i_ret != 0)
            {
                msg_Err(p_dec, "get_out failed");
                break;
            }
        }
        else
            break;
    }
    msg_Warn(p_dec, "OutThread stopped");

    /* Signal DecoderFlush that the output thread aborted */
    p_sys->b_aborted = true;
    vlc_cond_signal(&p_sys->dec_cond);
    vlc_mutex_unlock(&p_sys->lock);

    return NULL;
}

static block_t *GetNextBlock(decoder_sys_t *p_sys, block_t *p_block)
{
    if (p_sys->i_csd_send < p_sys->i_csd_count)
        return p_sys->pp_csd[p_sys->i_csd_send++];
    else
        return p_block;
}

static int QueueBlockLocked(decoder_t *p_dec, block_t *p_in_block,
                            bool b_drain)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = NULL;

    assert(p_sys->api.b_started);

    if ((p_sys->api.i_quirks & MC_API_QUIRKS_NEED_CSD) && !p_sys->i_csd_count
     && !p_sys->b_adaptive)
        return VLC_EGENERIC; /* Wait for CSDs */

    /* Queue CSD blocks and input blocks */
    while (b_drain || (p_block = GetNextBlock(p_sys, p_in_block)))
    {
        vlc_mutex_unlock(&p_sys->lock);
        int i_index = p_sys->api.dequeue_in(&p_sys->api, -1);
        vlc_mutex_lock(&p_sys->lock);

        if (p_sys->b_aborted)
            return VLC_EGENERIC;

        bool b_config = false;
        vlc_tick_t i_ts = 0;
        p_sys->b_input_dequeued = true;
        const void *p_buf = NULL;
        size_t i_size = 0;

        if (i_index >= 0)
        {
            assert(b_drain || p_block != NULL);
            if (p_block != NULL)
            {
                b_config = (p_block->i_flags & BLOCK_FLAG_CSD);
                if (!b_config)
                {
                    i_ts = p_block->i_pts;
                    if (!i_ts && p_block->i_dts)
                        i_ts = p_block->i_dts;
                }
                p_buf = p_block->p_buffer;
                i_size = p_block->i_buffer;
            }

            if (p_sys->api.queue_in(&p_sys->api, i_index, p_buf, i_size,
                                    i_ts, b_config) == 0)
            {
                if (!b_config && p_block != NULL)
                {
                    if (p_block->i_flags & BLOCK_FLAG_PREROLL)
                        p_sys->i_preroll_end = i_ts;

                    /* One input buffer is queued, signal OutThread that will
                     * fetch output buffers */
                    p_sys->b_output_ready = true;
                    vlc_cond_broadcast(&p_sys->cond);

                    assert(p_block == p_in_block),
                    p_in_block = NULL;
                }
                if (b_drain)
                    break;
            } else
            {
                msg_Err(p_dec, "queue_in failed");
                goto error;
            }
        }
        else
        {
            msg_Err(p_dec, "dequeue_in failed");
            goto error;
        }
    }

    if (b_drain)
    {
        msg_Warn(p_dec, "EOS sent, waiting for OutThread");

        /* Wait for the OutThread to stop (and process all remaining output
         * frames. Use a timeout here since we can't know if all decoders will
         * behave correctly. */
        vlc_tick_t deadline = vlc_tick_now() + INT64_C(3000000);
        while (!p_sys->b_aborted && !p_sys->b_drained
            && vlc_cond_timedwait(&p_sys->dec_cond, &p_sys->lock, deadline) == 0);

        if (!p_sys->b_drained)
        {
            msg_Err(p_dec, "OutThread timed out");
            AbortDecoderLocked(p_dec);
        }
        p_sys->b_drained = false;
    }

    return VLC_SUCCESS;

error:
    AbortDecoderLocked(p_dec);
    return VLC_EGENERIC;
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_in_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;

    vlc_mutex_lock(&p_sys->lock);

    if (p_sys->b_aborted)
    {
        if (p_sys->b_has_format)
            goto end;
        else
            goto reload;
    }

    if (p_in_block == NULL)
    {
        /* No input block, decoder is draining */
        msg_Err(p_dec, "Decoder is draining");

        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        goto end;
    }

    if (p_in_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        DecodeFlushLocked(p_dec);
        if (p_sys->b_aborted)
            goto end;
        if (p_in_block->i_flags & BLOCK_FLAG_CORRUPTED)
            goto end;
    }

    if (p_in_block->i_flags & BLOCK_FLAG_INTERLACED_MASK
     && !(p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED))
    {
        /* Before Android 21 and depending on the vendor, MediaCodec can
         * crash or be in an inconsistent state when decoding interlaced
         * videos. See OMXCodec_GetQuirks() for a white list of decoders
         * that supported interlaced videos before Android 21. */
        msg_Warn(p_dec, "codec doesn't support interlaced videos");
        goto reload;
    }

    /* Parse input block */
    if ((i_ret = p_sys->pf_on_new_block(p_dec, &p_in_block)) != 1)
    {
        if (i_ret != 0)
        {
            AbortDecoderLocked(p_dec);
            msg_Err(p_dec, "pf_on_new_block failed");
        }
        goto end;
    }
    if (p_sys->i_decode_flags & (DECODE_FLAG_DRAIN|DECODE_FLAG_RESTART))
    {
        msg_Warn(p_dec, "Draining from DecodeBlock");
        const bool b_restart = p_sys->i_decode_flags & DECODE_FLAG_RESTART;
        p_sys->i_decode_flags = 0;

        /* Drain and flush before restart to unblock OutThread */
        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        DecodeFlushLocked(p_dec);
        if (p_sys->b_aborted)
            goto end;

        if (b_restart)
        {
            StopMediaCodec(p_sys);

            int i_ret = StartMediaCodec(p_dec);
            switch (i_ret)
            {
            case VLC_SUCCESS:
                msg_Warn(p_dec, "Restarted from DecodeBlock");
                break;
            case VLC_ENOOBJ:
                break;
            default:
                msg_Err(p_dec, "StartMediaCodec failed");
                AbortDecoderLocked(p_dec);
                goto end;
            }
        }
    }

    /* Abort if MediaCodec is not yet started */
    if (p_sys->api.b_started)
        QueueBlockLocked(p_dec, p_in_block, false);

end:
    if (p_in_block)
        block_Release(p_in_block);
    /* Too late to reload here, we already modified/released the input block,
     * do it next time. */
    int ret = p_sys->b_aborted && p_sys->b_has_format ? VLCDEC_ECRITICAL
                                                      : VLCDEC_SUCCESS;
    vlc_mutex_unlock(&p_sys->lock);
    return ret;

reload:
    vlc_mutex_unlock(&p_sys->lock);
    /* Add an empty variable so that mediacodec won't be loaded again
     * for this ES */
    var_Create(p_dec, "mediacodec-failed", VLC_VAR_VOID);
    return VLCDEC_RELOAD;
}

static int Video_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    timestamp_FifoPut(p_sys->video.timestamp_fifo,
                      p_block->i_pts ? VLC_TICK_INVALID : p_block->i_dts);

    return 1;
}

static int VideoHXXX_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;
    bool b_config_changed = false;
    bool *p_config_changed = p_sys->b_adaptive ? NULL : &b_config_changed;

    *pp_block = hh->pf_process_block(hh, *pp_block, p_config_changed);
    if (!*pp_block)
        return 0;
    if (b_config_changed)
    {
        bool b_size_changed;
        int i_ret;
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
            if (hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0)
                i_ret = H264SetCSD(p_dec, &b_size_changed);
            else
                i_ret = VLC_EGENERIC;
            break;
        case VLC_CODEC_HEVC:
            if (hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
                hh->hevc.i_pps_count > 0 )
                i_ret = HEVCSetCSD(p_dec, &b_size_changed);
            else
                i_ret = VLC_EGENERIC;
            break;
        }
        if (i_ret != VLC_SUCCESS)
            return i_ret;
        if (b_size_changed || !p_sys->api.b_started)
        {
            if (p_sys->api.b_started)
                msg_Err(p_dec, "SPS/PPS changed during playback and "
                        "video size are different. Restart it !");
            p_sys->i_decode_flags |= DECODE_FLAG_RESTART;
        } else
        {
            msg_Err(p_dec, "SPS/PPS changed during playback. Drain it");
            p_sys->i_decode_flags |= DECODE_FLAG_DRAIN;
        }
    }

    return Video_OnNewBlock(p_dec, pp_block);
}

static int VideoMPEG2_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    if (pp_block == NULL || (*pp_block)->i_buffer <= 7)
        return 1;

    decoder_sys_t *p_sys = p_dec->p_sys;
    const int startcode = (*pp_block)->p_buffer[3];

    /* DAR aspect ratio from the DVD MPEG2 standard */
    static const int mpeg2_aspect[16][2] =
    {
        {0,0}, /* reserved */
        {0,0}, /* DAR = 0:0 will result in SAR = 1:1 */
        {4,3}, {16,9}, {221,100},
        /* reserved */
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}
    };

    if (startcode == 0xB3 /* SEQUENCE_HEADER_STARTCODE */)
    {
        int mpeg_dar_code = (*pp_block)->p_buffer[7] >> 4;

        if (mpeg_dar_code >= 16)
            return 0;

        p_sys->video.i_mpeg_dar_num = mpeg2_aspect[mpeg_dar_code][0];
        p_sys->video.i_mpeg_dar_den = mpeg2_aspect[mpeg_dar_code][1];
    }

    return 1;
}

static int VideoVC1_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    block_t *p_block = *pp_block;

    /* Adding frame start code */
    p_block = *pp_block = block_Realloc(p_block, 4, p_block->i_buffer);
    if (p_block == NULL)
        return VLC_ENOMEM;
    p_block->p_buffer[0] = 0x00;
    p_block->p_buffer[1] = 0x00;
    p_block->p_buffer[2] = 0x01;
    p_block->p_buffer[3] = 0x0d;

    return Video_OnNewBlock(p_dec, pp_block);
}

static void Video_OnFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    timestamp_FifoEmpty(p_sys->video.timestamp_fifo);
    /* Invalidate all pictures that are currently in flight
     * since flushing make all previous indices returned by
     * MediaCodec invalid. */
    ReleaseAllPictureContexts(p_sys);
}

static int Audio_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    /* We've just started the stream, wait for the first PTS. */
    if (date_Get(&p_sys->audio.i_end_date) == VLC_TICK_INVALID)
    {
        if (p_block->i_pts == VLC_TICK_INVALID)
            return 0;
        date_Set(&p_sys->audio.i_end_date, p_block->i_pts);
    }

    return 1;
}

static void Audio_OnFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set(&p_sys->audio.i_end_date, VLC_TICK_INVALID);
}
