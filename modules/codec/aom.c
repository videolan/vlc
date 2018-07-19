/*****************************************************************************
 * aom.c: libaom decoder (AV1) module
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Tristan Matthews <tmatth@videolan.org>
 * Based on vpx.c by: Rafaël Carré <funman@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <aom/aom_decoder.h>
#include <aom/aomdx.h>

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname("aom")
    set_description(N_("AOM video decoder"))
    set_capability("video decoder", 100)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end ()

static void aom_err_msg(vlc_object_t *this, aom_codec_ctx_t *ctx,
                        const char *msg)
{
    const char *error  = aom_codec_error(ctx);
    const char *detail = aom_codec_error_detail(ctx);
    if (!detail)
        detail = "no specific information";
    msg_Err(this, msg, error, detail);
}

#define AOM_ERR(this, ctx, msg) aom_err_msg(VLC_OBJECT(this), ctx, msg ": %s (%s)")
#define AOM_MAX_FRAMES_DEPTH 64

/*****************************************************************************
 * decoder_sys_t: libaom decoder descriptor
 *****************************************************************************/
struct frame_priv_s
{
    mtime_t pts;
};

struct decoder_sys_t
{
    aom_codec_ctx_t ctx;
    struct frame_priv_s frame_priv[AOM_MAX_FRAMES_DEPTH];
    unsigned i_next_frame_priv;
};

static const struct
{
    vlc_fourcc_t     i_chroma;
    enum aom_img_fmt i_chroma_id;
    uint8_t          i_bitdepth;
    uint8_t          i_needs_hack;

} chroma_table[] =
{
    { VLC_CODEC_I420, AOM_IMG_FMT_I420, 8, 0 },
    { VLC_CODEC_I422, AOM_IMG_FMT_I422, 8, 0 },
    { VLC_CODEC_I444, AOM_IMG_FMT_I444, 8, 0 },

    { VLC_CODEC_YV12, AOM_IMG_FMT_YV12, 8, 0 },
    { VLC_CODEC_YUVA, AOM_IMG_FMT_444A, 8, 0 },

    { VLC_CODEC_GBR_PLANAR, AOM_IMG_FMT_I444, 8, 1 },
    { VLC_CODEC_GBR_PLANAR_10L, AOM_IMG_FMT_I44416, 10, 1 },

    { VLC_CODEC_I420_10L, AOM_IMG_FMT_I42016, 10, 0 },
    { VLC_CODEC_I422_10L, AOM_IMG_FMT_I42216, 10, 0 },
    { VLC_CODEC_I444_10L, AOM_IMG_FMT_I44416, 10, 0 },

    { VLC_CODEC_I420_12L, AOM_IMG_FMT_I42016, 12, 0 },
    { VLC_CODEC_I422_12L, AOM_IMG_FMT_I42216, 12, 0 },
    { VLC_CODEC_I444_12L, AOM_IMG_FMT_I44416, 12, 0 },

    { VLC_CODEC_I444_16L, AOM_IMG_FMT_I44416, 16, 0 },
};

static vlc_fourcc_t FindVlcChroma( struct aom_image *img )
{
    uint8_t hack = (img->fmt & AOM_IMG_FMT_I444) && (img->tc == AOM_CICP_TC_SRGB);

    for( unsigned int i = 0; i < ARRAY_SIZE(chroma_table); i++ )
        if( chroma_table[i].i_chroma_id == img->fmt &&
            chroma_table[i].i_bitdepth == img->bit_depth &&
            chroma_table[i].i_needs_hack == hack )
            return chroma_table[i].i_chroma;

    return 0;
}

static void CopyPicture(const struct aom_image *img, picture_t *pic)
{
    for (int plane = 0; plane < pic->i_planes; plane++ ) {
        plane_t src_plane = pic->p[plane];
        src_plane.p_pixels = img->planes[plane];
        src_plane.i_pitch = img->stride[plane];
        plane_CopyPixels(&pic->p[plane], &src_plane);
    }
}

static int PushFrame(decoder_t *dec, block_t *block)
{
    decoder_sys_t *p_sys = dec->p_sys;
    aom_codec_ctx_t *ctx = &p_sys->ctx;
    const uint8_t *p_buffer;
    size_t i_buffer;

    /* Associate packet PTS with decoded frame */
    struct frame_priv_s *priv = &p_sys->frame_priv[p_sys->i_next_frame_priv++ % AOM_MAX_FRAMES_DEPTH];

    if(likely(block))
    {
        p_buffer = block->p_buffer;
        i_buffer = block->i_buffer;
        priv->pts = (block->i_pts != VLC_TS_INVALID) ? block->i_pts : block->i_dts;
    }
    else
    {
        p_buffer = NULL;
        i_buffer = 0;
    }

    aom_codec_err_t err;
    err = aom_codec_decode(ctx, p_buffer, i_buffer, priv);

    if(block)
        block_Release(block);

    if (err != AOM_CODEC_OK) {
        AOM_ERR(dec, ctx, "Failed to decode frame");
        if (err == AOM_CODEC_UNSUP_BITSTREAM)
            return VLCDEC_ECRITICAL;
	}
    return VLCDEC_SUCCESS;
}

static void OutputFrame(decoder_t *dec, const struct aom_image *img)
{
    video_format_t *v = &dec->fmt_out.video;

    if (img->d_w != v->i_visible_width || img->d_h != v->i_visible_height)
    {
        v->i_visible_width = dec->fmt_out.video.i_width = img->d_w;
        v->i_visible_height = dec->fmt_out.video.i_height = img->d_h;
    }

    if( !dec->fmt_out.video.i_sar_num || !dec->fmt_out.video.i_sar_den )
    {
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    }

    v->b_color_range_full = img->range == AOM_CR_FULL_RANGE;

    switch( img->mc )
    {
        case AOM_CICP_MC_BT_709:
            v->space = COLOR_SPACE_BT709;
            break;
        case AOM_CICP_MC_BT_601:
        case AOM_CICP_MC_SMPTE_240:
            v->space = COLOR_SPACE_BT601;
            break;
        case AOM_CICP_MC_BT_2020_CL:
        case AOM_CICP_MC_BT_2020_NCL:
            v->space = COLOR_SPACE_BT2020;
            break;
        default:
            break;
    }

    dec->fmt_out.video.projection_mode = dec->fmt_in.video.projection_mode;
    dec->fmt_out.video.multiview_mode = dec->fmt_in.video.multiview_mode;
    dec->fmt_out.video.pose = dec->fmt_in.video.pose;

    if (decoder_UpdateVideoFormat(dec) == VLC_SUCCESS)
    {
        picture_t *pic = decoder_NewPicture(dec);
        if (pic)
        {
            CopyPicture(img, pic);

            /* fetches back the PTS */
            mtime_t pts = ((struct frame_priv_s *) img->user_priv)->pts;

            pic->b_progressive = true; /* codec does not support interlacing */
            pic->date = pts;

            decoder_QueueVideo(dec, pic);
        }
    }
}

static int PopFrames(decoder_t *dec,
                     void(*pf_output)(decoder_t *, const struct aom_image *))
{
    decoder_sys_t *p_sys = dec->p_sys;
    aom_codec_ctx_t *ctx = &p_sys->ctx;

    for(const void *iter = NULL;; )
    {
        struct aom_image *img = aom_codec_get_frame(ctx, &iter);
        if (!img)
            break;

        dec->fmt_out.i_codec = FindVlcChroma(img);
        if (dec->fmt_out.i_codec == 0) {
            msg_Err(dec, "Unsupported output colorspace %d", img->fmt);
            continue;
        }

        pf_output(dec, img);
    }

    return VLCDEC_SUCCESS;
}

/****************************************************************************
 * Flush: clears decoder between seeks
 ****************************************************************************/
static void DropFrame(decoder_t *dec, const struct aom_image *img)
{
    VLC_UNUSED(dec);
    VLC_UNUSED(img);
    /* do nothing for now */
}

static void FlushDecoder(decoder_t *dec)
{
    decoder_sys_t *p_sys = dec->p_sys;
    aom_codec_ctx_t *ctx = &p_sys->ctx;

    if(PushFrame(dec, NULL) != VLCDEC_SUCCESS)
        AOM_ERR(dec, ctx, "Failed to flush decoder");
    else
        PopFrames(dec, DropFrame);
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************/
static int Decode(decoder_t *dec, block_t *block)
{
    if (block && block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    int i_ret = PushFrame(dec, block);

    PopFrames(dec, OutputFrame);

    return i_ret;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    const aom_codec_iface_t *iface;
    int av_version;

    if (dec->fmt_in.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    iface = &aom_codec_av1_dx_algo;
    av_version = 1;

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    dec->p_sys = sys;

    sys->i_next_frame_priv = 0;

    struct aom_codec_dec_cfg deccfg = {
        .threads = __MIN(vlc_GetCPUCount(), 16),
        .allow_lowbitdepth = 1
    };

    msg_Dbg(p_this, "AV%d: using libaom version %s (build options %s)",
        av_version, aom_codec_version_str(), aom_codec_build_config());

    if (aom_codec_dec_init(&sys->ctx, iface, &deccfg, 0) != AOM_CODEC_OK) {
        AOM_ERR(p_this, &sys->ctx, "Failed to initialize decoder");
        free(sys);
        return VLC_EGENERIC;;
    }

    dec->pf_decode = Decode;
    dec->pf_flush = FlushDecoder;

    dec->fmt_out.video.i_width = dec->fmt_in.video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in.video.i_height;
    dec->fmt_out.i_codec = VLC_CODEC_I420;

    if (dec->fmt_in.video.i_sar_num > 0 && dec->fmt_in.video.i_sar_den > 0) {
        dec->fmt_out.video.i_sar_num = dec->fmt_in.video.i_sar_num;
        dec->fmt_out.video.i_sar_den = dec->fmt_in.video.i_sar_den;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *sys = dec->p_sys;

    /* Flush decoder */
    FlushDecoder(dec);

    aom_codec_destroy(&sys->ctx);

    free(sys);
}
