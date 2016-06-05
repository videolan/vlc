/*****************************************************************************
 * vpx.c: libvpx decoder (VP8/VP9) module
 *****************************************************************************
 * Copyright (C) 2013 Rafaël Carré
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

#ifdef ENABLE_SOUT
# include <vpx/vpx_encoder.h>
# include <vpx/vp8cx.h>
#endif

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static const char *const ppsz_sout_options[] = { "quality-mode", NULL };
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
#ifdef ENABLE_SOUT
static int OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict);

#define QUALITY_MODE_TEXT N_("Quality mode")
#define QUALITY_MODE_LONGTEXT N_("Quality setting which will determine max encoding time\n" \
        " - 0: Good quality\n"\
        " - 1: Realtime\n"\
        " - 2: Best quality")
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname("vpx")
    set_description(N_("WebM video decoder"))
    set_capability("decoder", 100)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
#ifdef ENABLE_SOUT
    add_submodule()
    set_shortname("vpx")
    set_capability("encoder", 60)
    set_description(N_("WebM video encoder"))
    set_callbacks(OpenEncoder, CloseEncoder)
#   define ENC_CFG_PREFIX "sout-vpx-"
    add_integer( ENC_CFG_PREFIX "quality-mode", VPX_DL_GOOD_QUALITY, QUALITY_MODE_TEXT,
                 QUALITY_MODE_LONGTEXT, true )
        change_integer_range( 0, 2 )
#endif
vlc_module_end ()

static void vpx_err_msg(vlc_object_t *this, struct vpx_codec_ctx *ctx,
                        const char *msg)
{
    const char *error  = vpx_codec_error(ctx);
    const char *detail = vpx_codec_error_detail(ctx);
    if (!detail)
        detail = "no specific information";
    msg_Err(this, msg, error, detail);
}

#define VPX_ERR(this, ctx, msg) vpx_err_msg(VLC_OBJECT(this), ctx, msg ": %s (%s)")

/*****************************************************************************
 * decoder_sys_t: libvpx decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    struct vpx_codec_ctx ctx;
};

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************/
static picture_t *Decode(decoder_t *dec, block_t **pp_block)
{
    struct vpx_codec_ctx *ctx = &dec->p_sys->ctx;

    if( !pp_block || !*pp_block )
        return NULL;
    block_t *block = *pp_block;

    if (block->i_flags & (BLOCK_FLAG_CORRUPTED)) {
        block_Release(block);
        return NULL;
    }

    /* Associate packet PTS with decoded frame */
    mtime_t *pkt_pts = malloc(sizeof(*pkt_pts));
    if (!pkt_pts) {
        block_Release(block);
        *pp_block = NULL;
        return NULL;
    }

    *pkt_pts = block->i_pts;

    vpx_codec_err_t err;
    err = vpx_codec_decode(ctx, block->p_buffer, block->i_buffer, pkt_pts, 0);

    block_Release(block);
    *pp_block = NULL;

    if (err != VPX_CODEC_OK) {
        free(pkt_pts);
        VPX_ERR(dec, ctx, "Failed to decode frame");
        return NULL;
    }

    const void *iter = NULL;
    struct vpx_image *img = vpx_codec_get_frame(ctx, &iter);
    if (!img) {
        free(pkt_pts);
        return NULL;
    }

    /* fetches back the PTS */
    pkt_pts = img->user_priv;
    mtime_t pts = *pkt_pts;
    free(pkt_pts);

    if (img->fmt != VPX_IMG_FMT_I420) {
        msg_Err(dec, "Unsupported output colorspace %d", img->fmt);
        return NULL;
    }

    video_format_t *v = &dec->fmt_out.video;

    if (img->d_w != v->i_visible_width || img->d_h != v->i_visible_height) {
        v->i_visible_width = img->d_w;
        v->i_visible_height = img->d_h;
    }

    if( !dec->fmt_out.video.i_sar_num || !dec->fmt_out.video.i_sar_den )
    {
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    }

    picture_t *pic = decoder_NewPicture(dec);
    if (!pic)
        return NULL;

    for (int plane = 0; plane < pic->i_planes; plane++ ) {
        uint8_t *src = img->planes[plane];
        uint8_t *dst = pic->p[plane].p_pixels;
        int src_stride = img->stride[plane];
        int dst_stride = pic->p[plane].i_pitch;

        int size = __MIN( src_stride, dst_stride );
        for( int line = 0; line < pic->p[plane].i_visible_lines; line++ ) {
            memcpy( dst, src, size );
            src += src_stride;
            dst += dst_stride;
        }
    }

    pic->b_progressive = true; /* codec does not support interlacing */
    pic->date = pts;

    return pic;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    const struct vpx_codec_iface *iface;
    int vp_version;

    switch (dec->fmt_in.i_codec)
    {
#ifdef ENABLE_VP8_DECODER
    case VLC_CODEC_VP8:
        iface = &vpx_codec_vp8_dx_algo;
        vp_version = 8;
        break;
#endif
#ifdef ENABLE_VP9_DECODER
    case VLC_CODEC_VP9:
        iface = &vpx_codec_vp9_dx_algo;
        vp_version = 9;
        break;
#endif
    default:
        return VLC_EGENERIC;
    }

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    dec->p_sys = sys;

    struct vpx_codec_dec_cfg deccfg = {
        .threads = __MIN(vlc_GetCPUCount(), 16)
    };

    msg_Dbg(p_this, "VP%d: using libvpx version %s (build options %s)",
        vp_version, vpx_codec_version_str(), vpx_codec_build_config());

    if (vpx_codec_dec_init(&sys->ctx, iface, &deccfg, 0) != VPX_CODEC_OK) {
        VPX_ERR(p_this, &sys->ctx, "Failed to initialize decoder");
        free(sys);
        return VLC_EGENERIC;;
    }

    dec->pf_decode_video = Decode;

    dec->fmt_out.i_cat = VIDEO_ES;
    dec->fmt_out.video.i_width = dec->fmt_in.video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in.video.i_height;
    dec->fmt_out.i_codec = VLC_CODEC_I420;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *sys = dec->p_sys;

    /* Free our PTS */
    const void *iter = NULL;
    for (;;) {
        struct vpx_image *img = vpx_codec_get_frame(&sys->ctx, &iter);
        if (!img)
            break;
        free(img->user_priv);
    }

    vpx_codec_destroy(&sys->ctx);

    free(sys);
}

#ifdef ENABLE_SOUT

/*****************************************************************************
 * encoder_sys_t: libvpx encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    struct vpx_codec_ctx ctx;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    /* Allocate the memory needed to store the encoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    const struct vpx_codec_iface *iface;
    int vp_version;

    switch (p_enc->fmt_out.i_codec)
    {
#ifdef ENABLE_VP8_ENCODER
    case VLC_CODEC_VP8:
        iface = &vpx_codec_vp8_cx_algo;
        vp_version = 8;
        break;
#endif
#ifdef ENABLE_VP9_ENCODER
    case VLC_CODEC_VP9:
        iface = &vpx_codec_vp9_cx_algo;
        vp_version = 9;
        break;
#endif
    default:
        free(p_sys);
        return VLC_EGENERIC;
    }

    struct vpx_codec_enc_cfg enccfg = {};
    vpx_codec_enc_config_default(iface, &enccfg, 0);
    enccfg.g_threads = __MIN(vlc_GetCPUCount(), 4);
    enccfg.g_w = p_enc->fmt_in.video.i_visible_width;
    enccfg.g_h = p_enc->fmt_in.video.i_visible_height;

    msg_Dbg(p_this, "VP%d: using libvpx version %s (build options %s)",
        vp_version, vpx_codec_version_str(), vpx_codec_build_config());

    struct vpx_codec_ctx *ctx = &p_sys->ctx;
    if (vpx_codec_enc_init(ctx, iface, &enccfg, 0) != VPX_CODEC_OK) {
        VPX_ERR(p_this, ctx, "Failed to initialize encoder");
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_enc->pf_encode_video = Encode;
    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
    config_ChainParse(p_enc, ENC_CFG_PREFIX, ppsz_sout_options, p_enc->p_cfg);

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    struct vpx_codec_ctx *ctx = &p_sys->ctx;

    if (!p_pict) return NULL;

    vpx_image_t img = {};
    unsigned i_w = p_enc->fmt_in.video.i_visible_width;
    unsigned i_h = p_enc->fmt_in.video.i_visible_height;

    /* Create and initialize the vpx_image */
    if (!vpx_img_alloc(&img, VPX_IMG_FMT_I420, i_w, i_h, 1)) {
        VPX_ERR(p_enc, ctx, "Failed to allocate image");
        return NULL;
    }
    for (int plane = 0; plane < p_pict->i_planes; plane++) {
        uint8_t *src = p_pict->p[plane].p_pixels;
        uint8_t *dst = img.planes[plane];
        int src_stride = p_pict->p[plane].i_pitch;
        int dst_stride = img.stride[plane];

        int size = __MIN(src_stride, dst_stride);
        for (int line = 0; line < p_pict->p[plane].i_visible_lines; line++)
        {
            memcpy(dst, src, size);
            src += src_stride;
            dst += dst_stride;
        }
    }

    int flags = 0;
    /* Deadline (in ms) to spend in encoder */
    int quality = VPX_DL_GOOD_QUALITY;
    switch (var_GetInteger(p_enc, ENC_CFG_PREFIX "quality-mode")) {
        case 1:
            quality = VPX_DL_REALTIME;
            break;
        case 2:
            quality = VPX_DL_BEST_QUALITY;
            break;
        default:
            break;
    }

    vpx_codec_err_t res = vpx_codec_encode(ctx, &img, p_pict->date, 1,
     flags, quality);
    if (res != VPX_CODEC_OK) {
        VPX_ERR(p_enc, ctx, "Failed to encode frame");
        return NULL;
    }

    const vpx_codec_cx_pkt_t *pkt = NULL;
    vpx_codec_iter_t iter = NULL;
    block_t *p_out = NULL;
    while ((pkt = vpx_codec_get_cx_data(ctx, &iter)) != NULL)
    {
        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            int keyframe = pkt->data.frame.flags & VPX_FRAME_IS_KEY;
            block_t *p_block = block_Alloc(pkt->data.frame.sz);

            memcpy(p_block->p_buffer, pkt->data.frame.buf, pkt->data.frame.sz);
            p_block->i_dts = p_block->i_pts = pkt->data.frame.pts;
            if (keyframe)
                p_block->i_flags |= BLOCK_FLAG_TYPE_I;
            block_ChainAppend(&p_out, p_block);
        }
    }
    vpx_img_free(&img);
    return p_out;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    if (vpx_codec_destroy(&p_sys->ctx))
        VPX_ERR(p_this, &p_sys->ctx, "Failed to destroy codec");
    free(p_sys);
}

#endif  /* ENABLE_SOUT */
