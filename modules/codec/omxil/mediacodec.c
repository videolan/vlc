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

#include <jni.h>
#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>
#include <vlc_memory.h>

#include "mediacodec.h"
#include "../../packetizer/h264_nal.h"
#include "../../packetizer/hevc_nal.h"
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"
#include "android_opaque.h"
#include "../../video_output/android/android_window.h"

/* JNI functions to get/set an Android Surface object. */
extern jobject jni_LockAndGetAndroidJavaSurface();
extern void jni_UnlockAndroidSurface();
extern void jni_EventHardwareAccelerationError();

/* Implementation of a circular buffer of timestamps with overwriting
 * of older values. MediaCodec has only one type of timestamp, if a
 * block has no PTS, we send the DTS instead. Some hardware decoders
 * cannot cope with this situation and output the frames in the wrong
 * order. As a workaround in this case, we use a FIFO of timestamps in
 * order to remember which input packets had no PTS.  Since an
 * hardware decoder can silently drop frames, this might cause a
 * growing desynchronization with the actual timestamp. Thus the
 * circular buffer has a limited size and will overwrite older values.
 */
typedef struct
{
    uint32_t          begin;
    uint32_t          size;
    uint32_t          capacity;
    int64_t           *buffer;
} timestamp_fifo_t;

static timestamp_fifo_t *timestamp_FifoNew(uint32_t capacity)
{
    timestamp_fifo_t *fifo = calloc(1, sizeof(*fifo));
    if (!fifo)
        return NULL;
    fifo->buffer = malloc(capacity * sizeof(*fifo->buffer));
    if (!fifo->buffer) {
        free(fifo);
        return NULL;
    }
    fifo->capacity = capacity;
    return fifo;
}

static void timestamp_FifoRelease(timestamp_fifo_t *fifo)
{
    free(fifo->buffer);
    free(fifo);
}

static bool timestamp_FifoIsEmpty(timestamp_fifo_t *fifo)
{
    return fifo->size == 0;
}

static bool timestamp_FifoIsFull(timestamp_fifo_t *fifo)
{
    return fifo->size == fifo->capacity;
}

static void timestamp_FifoEmpty(timestamp_fifo_t *fifo)
{
    fifo->size = 0;
}

static void timestamp_FifoPut(timestamp_fifo_t *fifo, int64_t ts)
{
    uint32_t end = (fifo->begin + fifo->size) % fifo->capacity;
    fifo->buffer[end] = ts;
    if (!timestamp_FifoIsFull(fifo))
        fifo->size += 1;
    else
        fifo->begin = (fifo->begin + 1) % fifo->capacity;
}

static int64_t timestamp_FifoGet(timestamp_fifo_t *fifo)
{
    if (timestamp_FifoIsEmpty(fifo))
        return VLC_TS_INVALID;

    int64_t result = fifo->buffer[fifo->begin];
    fifo->begin = (fifo->begin + 1) % fifo->capacity;
    fifo->size -= 1;
    return result;
}

/* Codec Specific Data */
struct csd
{
    uint8_t *p_buf;
    size_t i_size;
};

struct decoder_sys_t
{
    mc_api *api;
    uint32_t nal_size;

    int pixel_format;
    int stride, slice_height;
    const char *mime;

    /* Codec Specific Data buffer: sent in PutInput after a start or a flush
     * with the BUFFER_FLAG_CODEC_CONFIG flag.*/
    struct csd *p_csd;
    size_t i_csd_count;
    size_t i_csd_send;

    bool b_update_format;
    int i_width;
    int i_height;

    bool decoded;
    bool error_state;
    bool b_new_block;
    bool b_support_interlaced;

    ArchitectureSpecificCopyData architecture_specific_data;

    /* Direct rendering members. */
    bool direct_rendering;
    picture_t** pp_inflight_pictures; /**< stores the inflight picture for each output buffer or NULL */
    unsigned int i_inflight_pictures;

    timestamp_fifo_t *timestamp_fifo;

    int64_t i_preroll_end;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoderJni(vlc_object_t *);
static int  OpenDecoderNdk(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

static picture_t *DecodeVideo(decoder_t *, block_t **);

static void InvalidateAllPictures(decoder_t *);
static int InsertInflightPicture(decoder_t *, picture_t *, unsigned int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DIRECTRENDERING_TEXT N_("Android direct rendering")
#define DIRECTRENDERING_LONGTEXT N_(\
        "Enable Android direct rendering using opaque buffers.")

#define CFG_PREFIX "mediacodec-"

vlc_module_begin ()
    set_description( N_("Video decoder using Android MediaCodec") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_section( N_("Decoding") , NULL )
    set_capability( "decoder", 0 ) /* Only enabled via commandline arguments */
    add_bool(CFG_PREFIX "dr", true,
             DIRECTRENDERING_TEXT, DIRECTRENDERING_LONGTEXT, true)
    set_callbacks( OpenDecoderNdk, CloseDecoder )
    add_shortcut( "mediacodec_ndk" )
    add_submodule ()
        set_capability( "decoder", 0 )
        set_callbacks( OpenDecoderJni, CloseDecoder )
        add_shortcut( "mediacodec_jni" )
vlc_module_end ()


static void CSDFree(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->p_csd)
    {
        for (unsigned int i = 0; i < p_sys->i_csd_count; ++i)
            free(p_sys->p_csd[i].p_buf);
        free(p_sys->p_csd);
        p_sys->p_csd = NULL;
    }
    p_sys->i_csd_count = 0;
}

/* Create the p_sys->p_csd that will be sent via PutInput */
static int CSDDup(decoder_t *p_dec, const struct csd *p_csd, size_t i_count)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    unsigned int i_last_csd_count = p_sys->i_csd_count;

    p_sys->i_csd_count = i_count;
    /* free previous p_buf if old count is bigger */
    for (size_t i = p_sys->i_csd_count; i < i_last_csd_count; ++i)
        free(p_sys->p_csd[i].p_buf);

    p_sys->p_csd = realloc_or_free(p_sys->p_csd, p_sys->i_csd_count *
                                   sizeof(struct csd));
    if (!p_sys->p_csd)
    {
        CSDFree(p_dec);
        return VLC_ENOMEM;
    }

    if (p_sys->i_csd_count > i_last_csd_count)
        memset(&p_sys->p_csd[i_last_csd_count], 0,
               (p_sys->i_csd_count - i_last_csd_count) * sizeof(struct csd));

    for (size_t i = 0; i < p_sys->i_csd_count; ++i)
    {
        p_sys->p_csd[i].p_buf = realloc_or_free(p_sys->p_csd[i].p_buf,
                                                p_csd[i].i_size);
        if (!p_sys->p_csd[i].p_buf)
        {
            CSDFree(p_dec);
            return VLC_ENOMEM;
        }
        memcpy(p_sys->p_csd[i].p_buf, p_csd[i].p_buf, p_csd[i].i_size);
        p_sys->p_csd[i].i_size = p_csd[i].i_size;
    }
    return VLC_SUCCESS;
}

static bool CSDCmp(decoder_t *p_dec, struct csd *p_csd, size_t i_csd_count)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->i_csd_count != i_csd_count)
        return false;
    for (size_t i = 0; i < i_csd_count; ++i)
    {
        if (p_sys->p_csd[i].i_size != p_csd[i].i_size
         || memcmp(p_sys->p_csd[i].p_buf, p_csd[i].p_buf, p_csd[i].i_size) != 0)
            return false;
    }
    return true;
}

/* Fill the p_sys->p_csd struct with H264 Parameter Sets */
static int H264SetCSD(decoder_t *p_dec, void *p_buf, size_t i_size,
                      bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct nal_sps sps;
    uint8_t *p_sps_buf = NULL, *p_pps_buf = NULL;
    size_t i_sps_size = 0, i_pps_size = 0;

    /* Check if p_buf contains a valid SPS PPS */
    if (h264_get_spspps(p_buf, i_size, &p_sps_buf, &i_sps_size,
                        &p_pps_buf, &i_pps_size) == 0
     && h264_parse_sps(p_sps_buf, i_sps_size, &sps) == 0
     && sps.i_width && sps.i_height)
    {
        struct csd csd[2];
        int i_csd_count = 0;

        if (i_sps_size)
        {
            csd[i_csd_count].p_buf = p_sps_buf;
            csd[i_csd_count].i_size = i_sps_size;
            i_csd_count++;
        }
        if (i_pps_size)
        {
            csd[i_csd_count].p_buf = p_pps_buf;
            csd[i_csd_count].i_size = i_pps_size;
            i_csd_count++;
        }

        /* Compare the SPS PPS with the old one */
        if (!CSDCmp(p_dec, csd, i_csd_count))
        {
            msg_Warn(p_dec, "New SPS/PPS found, id: %d size: %dx%d sps: %d pps: %d",
                     sps.i_id, sps.i_width, sps.i_height,
                     i_sps_size, i_pps_size);

            /* In most use cases, p_sys->p_csd[0] contains a SPS, and
             * p_sys->p_csd[1] contains a PPS */
            if (CSDDup(p_dec, csd, i_csd_count))
                return VLC_ENOMEM;

            if (p_size_changed)
                *p_size_changed = (sps.i_width != p_sys->i_width
                                || sps.i_height != p_sys->i_height);

            p_sys->i_csd_send = 0;
            p_sys->i_width = sps.i_width;
            p_sys->i_height = sps.i_height;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * StartMediaCodec: Create the mediacodec instance
 *****************************************************************************/
static int StartMediaCodec(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_angle = 0, i_ret;
    size_t h264_profile = 0;
    jobject jsurface = NULL;

    if (p_dec->fmt_in.i_extra && !p_sys->p_csd)
    {
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264
         || p_dec->fmt_in.i_codec == VLC_CODEC_HEVC)
        {
            int buf_size = p_dec->fmt_in.i_extra + 20;
            uint32_t size = p_dec->fmt_in.i_extra;
            void *p_buf = malloc(buf_size);

            if (!p_buf)
            {
                msg_Warn(p_dec, "extra buffer allocation failed");
                return VLC_EGENERIC;
            }

            if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
            {
                if (((uint8_t*)p_dec->fmt_in.p_extra)[0] == 1
                 && convert_sps_pps(p_dec, p_dec->fmt_in.p_extra,
                                    p_dec->fmt_in.i_extra,
                                    p_buf, buf_size, &size,
                                    &p_sys->nal_size) == VLC_SUCCESS)
                    H264SetCSD(p_dec, p_buf, size, NULL);
            } else
            {
                if (convert_hevc_nal_units(p_dec, p_dec->fmt_in.p_extra,
                                           p_dec->fmt_in.i_extra,
                                           p_buf, buf_size, &size,
                                           &p_sys->nal_size) == VLC_SUCCESS)
                {
                    struct csd csd;

                    csd.p_buf = p_buf;
                    csd.i_size = size;
                    CSDDup(p_dec, &csd, 1);
                }
            }
            free(p_buf);
        }
        if (!p_sys->p_csd)
        {
            struct csd csd;

            csd.p_buf = p_dec->fmt_in.p_extra;
            csd.i_size = p_dec->fmt_in.i_extra;
            CSDDup(p_dec, &csd, 1);
        }

        p_sys->i_csd_send = 0;
    }
    if (!p_sys->i_width || !p_sys->i_height)
    {
        msg_Err(p_dec, "invalid size, abort MediaCodec");
        return VLC_EGENERIC;
    }

    if ( p_dec->fmt_in.video.orientation != ORIENT_NORMAL)
    {
        switch (p_dec->fmt_in.video.orientation)
        {
            case ORIENT_ROTATED_90:
                i_angle = 90;
                break;
            case ORIENT_ROTATED_180:
                i_angle = 180;
                break;
            case ORIENT_ROTATED_270:
                i_angle = 270;
                break;
            default:
                i_angle = 0;
        }
    }

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
        h264_get_profile_level(&p_dec->fmt_in, &h264_profile, NULL, NULL);

    if (var_InheritBool(p_dec, CFG_PREFIX "dr"))
        jsurface = jni_LockAndGetAndroidJavaSurface();
    i_ret = p_sys->api->start(p_sys->api, jsurface, p_sys->mime, p_sys->i_width,
                              p_sys->i_height, h264_profile, i_angle);
    if (jsurface)
        jni_UnlockAndroidSurface();

    if (i_ret == VLC_SUCCESS)
    {
        if (p_sys->api->b_direct_rendering)
            p_dec->fmt_out.i_codec = VLC_CODEC_ANDROID_OPAQUE;
        p_sys->b_update_format = true;
        return VLC_SUCCESS;
    } else
        return VLC_EGENERIC;
}

/*****************************************************************************
 * StopMediaCodec: Close the mediacodec instance
 *****************************************************************************/
static void StopMediaCodec(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Invalidate all pictures that are currently in flight in order
     * to prevent the vout from using destroyed output buffers. */
    if (p_sys->api->b_direct_rendering)
        InvalidateAllPictures(p_dec);

    p_sys->api->stop(p_sys->api);
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this, pf_MediaCodecApi_init pf_init)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    mc_api *api;
    const char *mime;

    if (p_dec->fmt_in.i_cat != VIDEO_ES && !p_dec->b_force)
        return VLC_EGENERIC;

    switch (p_dec->fmt_in.i_codec) {
    case VLC_CODEC_HEVC: mime = "video/hevc"; break;
    case VLC_CODEC_H264: mime = "video/avc"; break;
    case VLC_CODEC_H263: mime = "video/3gpp"; break;
    case VLC_CODEC_MP4V: mime = "video/mp4v-es"; break;
    case VLC_CODEC_WMV3: mime = "video/x-ms-wmv"; break;
    case VLC_CODEC_VC1:  mime = "video/wvc1"; break;
    case VLC_CODEC_VP8:  mime = "video/x-vnd.on2.vp8"; break;
    case VLC_CODEC_VP9:  mime = "video/x-vnd.on2.vp9"; break;
    default:
        msg_Dbg(p_dec, "codec %4.4s not supported",
                (char *)&p_dec->fmt_in.i_codec);
        return VLC_EGENERIC;
    }

    if (!p_dec->fmt_in.video.i_width || !p_dec->fmt_in.video.i_height)
    {
        /* We can handle h264 without a valid video size */
        if (p_dec->fmt_in.i_codec != VLC_CODEC_H264)
        {
            msg_Dbg(p_dec, "resolution (%dx%d) not supported",
                    p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height);
            return VLC_EGENERIC;
        }
    }

    api = calloc(1, sizeof(mc_api));
    if (!api)
        return VLC_ENOMEM;
    api->p_obj = p_this;
    if (pf_init(api) != VLC_SUCCESS)
    {
        free(api);
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_dec->p_sys = calloc(1, sizeof(*p_dec->p_sys))) == NULL)
    {
        api->clean(api);
        free(api);
        return VLC_ENOMEM;
    }
    p_dec->p_sys->api = api;

    p_dec->pf_decode_video = DecodeVideo;

    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->b_need_packetized = true;

    p_dec->p_sys->i_width = p_dec->fmt_in.video.i_width;
    p_dec->p_sys->i_height = p_dec->fmt_in.video.i_height;

    p_dec->p_sys->timestamp_fifo = timestamp_FifoNew(32);
    if (!p_dec->p_sys->timestamp_fifo)
    {
        CloseDecoder(p_this);
        return VLC_ENOMEM;
    }

    p_dec->p_sys->mime = mime;
    p_dec->p_sys->b_new_block = true;

    switch (p_dec->fmt_in.i_codec)
    {
    case VLC_CODEC_H264:
        if (!p_dec->p_sys->i_width || !p_dec->p_sys->i_height)
        {
            msg_Warn(p_dec, "waiting for sps/pps for codec %4.4s",
                     (const char *)&p_dec->fmt_in.i_codec);
            return VLC_SUCCESS;
        }
    case VLC_CODEC_VC1:
        if (!p_dec->fmt_in.i_extra)
        {
            msg_Warn(p_dec, "waiting for extra data for codec %4.4s",
                     (const char *)&p_dec->fmt_in.i_codec);
            return VLC_SUCCESS;
        }
        break;
    }
    return StartMediaCodec(p_dec);
}

static int OpenDecoderNdk(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecNdk_Init);
}

static int OpenDecoderJni(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecJni_Init);
}

/*****************************************************************************
 * CloseDecoder: Close the decoder instance
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_sys)
        return;

    StopMediaCodec(p_dec);

    CSDFree(p_dec);
    ArchitectureSpecificCopyHooksDestroy(p_sys->pixel_format, &p_sys->architecture_specific_data);
    free(p_sys->pp_inflight_pictures);
    if (p_sys->timestamp_fifo)
        timestamp_FifoRelease(p_sys->timestamp_fifo);
    p_sys->api->clean(p_sys->api);
    free(p_sys->api);
    free(p_sys);
}

/*****************************************************************************
 * vout callbacks
 *****************************************************************************/
static void UnlockPicture(picture_t* p_pic, bool b_render)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    decoder_t *p_dec = p_picsys->priv.hw.p_dec;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_picsys->priv.hw.b_valid)
        return;

    vlc_mutex_lock(get_android_opaque_mutex());

    /* Picture might have been invalidated while waiting on the mutex. */
    if (!p_picsys->priv.hw.b_valid) {
        vlc_mutex_unlock(get_android_opaque_mutex());
        return;
    }

    uint32_t i_index = p_picsys->priv.hw.i_index;
    InsertInflightPicture(p_dec, NULL, i_index);

    /* Release the MediaCodec buffer. */
    p_sys->api->release_out(p_sys->api, i_index, b_render);
    p_picsys->priv.hw.b_valid = false;

    vlc_mutex_unlock(get_android_opaque_mutex());
}

static void InvalidateAllPictures(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(get_android_opaque_mutex());

    for (unsigned int i = 0; i < p_sys->i_inflight_pictures; ++i) {
        picture_t *p_pic = p_sys->pp_inflight_pictures[i];
        if (p_pic) {
            p_pic->p_sys->priv.hw.b_valid = false;
            p_sys->pp_inflight_pictures[i] = NULL;
        }
    }
    vlc_mutex_unlock(get_android_opaque_mutex());
}

static int InsertInflightPicture(decoder_t *p_dec, picture_t *p_pic,
                                 unsigned int i_index)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (i_index >= p_sys->i_inflight_pictures) {
        picture_t **pp_pics = realloc(p_sys->pp_inflight_pictures,
                                      (i_index + 1) * sizeof (picture_t *));
        if (!pp_pics)
            return -1;
        if (i_index - p_sys->i_inflight_pictures > 0)
            memset(&pp_pics[p_sys->i_inflight_pictures], 0,
                   (i_index - p_sys->i_inflight_pictures) * sizeof (picture_t *));
        p_sys->pp_inflight_pictures = pp_pics;
        p_sys->i_inflight_pictures = i_index + 1;
    }
    p_sys->pp_inflight_pictures[i_index] = p_pic;
    return 0;
}

static int PutInput(decoder_t *p_dec, block_t *p_block, mtime_t timeout)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;
    const void *p_buf;
    size_t i_size;
    bool b_config = false;
    mtime_t i_ts = 0;

    assert(p_sys->i_csd_send < p_sys->i_csd_count || p_block);

    if (p_sys->i_csd_send < p_sys->i_csd_count)
    {
        /* Try to send Codec Specific Data */
        p_buf = p_sys->p_csd[p_sys->i_csd_send].p_buf;
        i_size = p_sys->p_csd[p_sys->i_csd_send].i_size;
        b_config = true;
    } else
    {
        /* Try to send p_block input buffer */
        p_buf = p_block->p_buffer;
        i_size = p_block->i_buffer;
        i_ts = p_block->i_pts;
        if (!i_ts && p_block->i_dts)
            i_ts = p_block->i_dts;
    }

    i_ret = p_sys->api->put_in(p_sys->api, p_buf, i_size, i_ts, b_config,
                               timeout);
    if (i_ret != 1)
        return i_ret;

    if (p_sys->i_csd_send < p_sys->i_csd_count)
    {
        msg_Dbg(p_dec, "sent codec specific data(%d) of size %d "
                "via BUFFER_FLAG_CODEC_CONFIG flag",
                p_sys->i_csd_send, i_size);
        p_sys->i_csd_send++;
        return 0;
    }
    else
    {
        p_sys->decoded = true;
        if (p_block->i_flags & BLOCK_FLAG_PREROLL )
            p_sys->i_preroll_end = i_ts;
        timestamp_FifoPut(p_sys->timestamp_fifo,
                          p_block->i_pts ? VLC_TS_INVALID : p_block->i_dts);
        return 1;
    }
}

static int GetOutput(decoder_t *p_dec, picture_t *p_pic,
                     mtime_t i_timeout)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    mc_api_out out;
    int i_ret = p_sys->api->get_out(p_sys->api, &out, i_timeout);

    if (i_ret != 1)
        return i_ret;
    if (out.type == MC_OUT_TYPE_BUF)
    {
        /* If the oldest input block had no PTS, the timestamp of
         * the frame returned by MediaCodec might be wrong so we
         * overwrite it with the corresponding dts. Call FifoGet
         * first in order to avoid a gap if buffers are released
         * due to an invalid format or a preroll */
        int64_t forced_ts = timestamp_FifoGet(p_sys->timestamp_fifo);

        if (!p_sys->pixel_format || !p_pic) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return p_sys->api->release_out(p_sys->api, out.u.buf.i_index, false);
        }

        if (out.u.buf.i_ts <= p_sys->i_preroll_end)
            return p_sys->api->release_out(p_sys->api, out.u.buf.i_index, false);

        if (forced_ts == VLC_TS_INVALID)
            p_pic->date = out.u.buf.i_ts;
        else
            p_pic->date = forced_ts;

        if (p_sys->api->b_direct_rendering)
        {
            picture_sys_t *p_picsys = p_pic->p_sys;
            p_picsys->pf_lock_pic = NULL;
            p_picsys->pf_unlock_pic = UnlockPicture;
            p_picsys->priv.hw.p_dec = p_dec;
            p_picsys->priv.hw.i_index = out.u.buf.i_index;
            p_picsys->priv.hw.b_valid = true;

            vlc_mutex_lock(get_android_opaque_mutex());
            InsertInflightPicture(p_dec, p_pic, out.u.buf.i_index);
            vlc_mutex_unlock(get_android_opaque_mutex());
        } else {
            unsigned int chroma_div;
            GetVlcChromaSizes(p_dec->fmt_out.i_codec,
                              p_dec->fmt_out.video.i_width,
                              p_dec->fmt_out.video.i_height,
                              NULL, NULL, &chroma_div);
            CopyOmxPicture(p_sys->pixel_format, p_pic,
                           p_sys->slice_height, p_sys->stride,
                           (uint8_t *)out.u.buf.p_ptr, chroma_div,
                           &p_sys->architecture_specific_data);

            if (p_sys->api->release_out(p_sys->api, out.u.buf.i_index, false))
                return -1;
        }
        return 1;
    } else {
        assert(out.type == MC_OUT_TYPE_CONF);
        p_sys->pixel_format = out.u.conf.pixel_format;
        ArchitectureSpecificCopyHooksDestroy(p_sys->pixel_format,
                                             &p_sys->architecture_specific_data);

        const char *name = "unknown";
        if (!p_sys->api->b_direct_rendering) {
            if (!GetVlcChromaFormat(p_sys->pixel_format,
                                    &p_dec->fmt_out.i_codec, &name)) {
                msg_Err(p_dec, "color-format not recognized");
                return -1;
            }
        }

        msg_Err(p_dec, "output: %d %s, %dx%d stride %d %d, crop %d %d %d %d",
                p_sys->pixel_format, name, out.u.conf.width, out.u.conf.height,
                out.u.conf.stride, out.u.conf.slice_height,
                out.u.conf.crop_left, out.u.conf.crop_top,
                out.u.conf.crop_right, out.u.conf.crop_bottom);

        p_dec->fmt_out.video.i_width = out.u.conf.crop_right + 1 - out.u.conf.crop_left;
        p_dec->fmt_out.video.i_height = out.u.conf.crop_bottom + 1 - out.u.conf.crop_top;
        if (p_dec->fmt_out.video.i_width <= 1
            || p_dec->fmt_out.video.i_height <= 1) {
            p_dec->fmt_out.video.i_width = out.u.conf.width;
            p_dec->fmt_out.video.i_height = out.u.conf.height;
        }
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;

        p_sys->stride = out.u.conf.stride;
        p_sys->slice_height = out.u.conf.slice_height;
        if (p_sys->stride <= 0)
            p_sys->stride = out.u.conf.width;
        if (p_sys->slice_height <= 0)
            p_sys->slice_height = out.u.conf.height;

        ArchitectureSpecificCopyHooks(p_dec, out.u.conf.pixel_format,
                                      out.u.conf.slice_height,
                                      p_sys->stride, &p_sys->architecture_specific_data);
        if (p_sys->pixel_format == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
            p_sys->slice_height -= out.u.conf.crop_top/2;
        if (IgnoreOmxDecoderPadding(p_sys->api->psz_name)) {
            p_sys->slice_height = 0;
            p_sys->stride = p_dec->fmt_out.video.i_width;
        }
        p_sys->b_update_format = true;
        return 0;
    }
}

static void H264ProcessBlock(decoder_t *p_dec, block_t *p_block,
                             bool *p_csd_changed, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct H264ConvertState convert_state = { 0, 0 };

    assert(p_dec->fmt_in.i_codec == VLC_CODEC_H264 && p_block);

    if (p_sys->nal_size)
    {
        convert_h264_to_annexb(p_block->p_buffer, p_block->i_buffer,
                               p_sys->nal_size, &convert_state);
    } else if (H264SetCSD(p_dec, p_block->p_buffer, p_block->i_buffer,
                          p_size_changed) == VLC_SUCCESS)
    {
        *p_csd_changed = true;
    }
}

static void HEVCProcessBlock(decoder_t *p_dec, block_t *p_block,
                             bool *p_csd_changed, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct H264ConvertState convert_state = { 0, 0 };

    assert(p_dec->fmt_in.i_codec == VLC_CODEC_HEVC && p_block);

    if (p_sys->nal_size)
    {
        convert_h264_to_annexb(p_block->p_buffer, p_block->i_buffer,
                               p_sys->nal_size, &convert_state);
    }

    /* TODO */
    VLC_UNUSED(p_csd_changed);
    VLC_UNUSED(p_size_changed);
}

static int DecodeFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->decoded)
    {
        p_sys->i_preroll_end = 0;
        timestamp_FifoEmpty(p_sys->timestamp_fifo);
        /* Invalidate all pictures that are currently in flight
         * since flushing make all previous indices returned by
         * MediaCodec invalid. */
        if (p_sys->api->b_direct_rendering)
            InvalidateAllPictures(p_dec);
    }

    if (p_sys->decoded || p_sys->i_csd_send > 0)
    {
        if (p_sys->api->flush(p_sys->api) != VLC_SUCCESS)
            return VLC_EGENERIC;
        /* resend CODEC_CONFIG buffer after a flush */
        p_sys->i_csd_send = 0;
    }
    p_sys->decoded = false;
    return VLC_SUCCESS;
}

static picture_t *DecodeVideo(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = NULL;
    block_t *p_block = pp_block ? *pp_block : NULL;
    unsigned int i_attempts = 0;
    jlong timeout = 0;
    int i_output_ret = 0;
    int i_input_ret = 0;
    bool b_error = false;
    bool b_delayed_start = false;
    bool b_new_block = p_block ? p_sys->b_new_block : false;

    if (p_sys->error_state)
        goto endclean;

    if (p_block && p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK
        && !p_sys->api->b_support_interlaced)
    {
        b_error = true;
        goto endclean;
    }

    if (p_block && p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        if (DecodeFlush(p_dec) != VLC_SUCCESS)
            b_error = true;
        goto endclean;
    }

    if (b_new_block)
    {
        bool b_csd_changed = false, b_size_changed = false;

        p_sys->b_new_block = false;
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
            H264ProcessBlock(p_dec, p_block, &b_csd_changed, &b_size_changed);
        else if (p_dec->fmt_in.i_codec == VLC_CODEC_HEVC)
            HEVCProcessBlock(p_dec, p_block, &b_csd_changed, &b_size_changed);

        if (p_sys->api->b_started && b_csd_changed)
        {
            if (b_size_changed)
            {
                msg_Err(p_dec, "SPS/PPS changed during playback and "
                        "video size are different. Restart it !");
                StopMediaCodec(p_dec);
            } else
            {
                msg_Err(p_dec, "SPS/PPS changed during playback. Flush it");
                if (DecodeFlush(p_dec) != VLC_SUCCESS)
                    b_error = true;
            }
        }
        if (!p_sys->api->b_started)
            b_delayed_start = true;
    }

    /* try delayed opening if there is a new extra data */
    if (!p_sys->api->b_started)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_VC1:
            if (p_dec->fmt_in.i_extra)
                b_delayed_start = true;
        default:
            break;
        }
        if (b_delayed_start && StartMediaCodec(p_dec) != VLC_SUCCESS)
        {
            b_error = true;
            goto endclean;
        }
    }
    if (!p_sys->api->b_started)
        goto endclean;

    /* Use the aspect ratio provided by the input (ie read from packetizer).
     * Don't check the current value of the aspect ratio in fmt_out, since we
     * want to allow changes in it to propagate. */
    if (p_dec->fmt_in.video.i_sar_num != 0 && p_dec->fmt_in.video.i_sar_den != 0
     && (p_dec->fmt_out.video.i_sar_num != p_dec->fmt_in.video.i_sar_num ||
         p_dec->fmt_out.video.i_sar_den != p_dec->fmt_in.video.i_sar_den))
    {
        p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in.video.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in.video.i_sar_den;
        p_sys->b_update_format = true;
    }

    do
    {
        if ((p_sys->i_csd_send < p_sys->i_csd_count || p_block)
         && i_input_ret == 0)
        {
            i_input_ret = PutInput(p_dec, p_block, timeout);
            if (!p_sys->decoded)
                    continue;
        }

        if (i_input_ret != -1 && p_sys->decoded && i_output_ret == 0)
        {
            /* FIXME: A new picture shouldn't be created each time.
             * If decoder_NewPicture fails because the decoder is
             * flushing/exiting, GetOutput will either fail (or crash in
             * function of devices), or never return an output buffer. Indeed,
             * if the Decoder is flushing, MediaCodec can be stalled since the
             * input is waiting for the output or vice-versa.  Therefore, call
             * decoder_NewPicture before GetOutput as a safeguard. */

            if (p_sys->pixel_format)
            {
                if (p_sys->b_update_format)
                {
                    p_sys->b_update_format = false;
                    if (decoder_UpdateVideoFormat(p_dec) != 0)
                    {
                        msg_Err(p_dec, "decoder_UpdateVideoFormat failed");
                        b_error = true;
                        break;
                    }
                }
                p_pic = decoder_NewPicture(p_dec);
                if (!p_pic) {
                    msg_Warn(p_dec, "NewPicture failed");
                    break;
                }
            }
            i_output_ret = GetOutput(p_dec, p_pic, timeout);
            if (p_pic)
            {
                if (i_output_ret != 1) {
                    picture_Release(p_pic);
                    p_pic = NULL;
                }
            } else
            {
                if (i_output_ret == 0 && i_input_ret == 0 && ++i_attempts > 100)
                {
                    /* No p_pic, so no pixel_format, thereforce mediacodec
                     * didn't produce any output or events yet. Don't wait
                     * indefinitely and abort after 2seconds (100 * 2 * 10ms)
                     * without any data. Indeed, MediaCodec can fail without
                     * throwing any exception or error returns... */
                    msg_Err(p_dec, "No output/input for %lld ms, abort",
                                    i_attempts * timeout);
                    b_error = true;
                    break;
                }
            }
        }
        timeout = 10 * 1000; // 10 ms
        /* loop until either the input or the output are processed (i_input_ret
         * or i_output_ret == 1 ) or caused an error (i_input_ret or
         * i_output_ret == -1 )*/
    } while (p_block && i_input_ret == 0 && i_output_ret == 0);

    if (i_input_ret == -1 || i_output_ret == -1)
    {
        msg_Err(p_dec, "%s failed",
                i_input_ret == -1 ? "PutInput" : "GetOutput");
        b_error = true;
    }

endclean:

    /* If pf_decode returns NULL, we'll get a new p_block from the next
     * pf_decode call. Therefore we need to release the current one even if we
     * couldn't process it (it happens in case or error or if MediaCodec is
     * still not opened). We also must release the current p_block if we were
     * able to process it. */
    if (p_block && (p_pic == NULL || i_input_ret != 0))
    {
        block_Release(p_block);
        *pp_block = NULL;
        p_sys->b_new_block = true;
    }
    if (b_error && !p_sys->error_state) {
        /* Signal the error to the Java. */
        jni_EventHardwareAccelerationError();
        p_sys->error_state = true;
    }

    return p_pic;
}
