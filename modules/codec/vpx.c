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

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname("vpx")
    set_description(N_("WebM video decoder"))
    set_capability("decoder", 60)
    set_callbacks(Open, Close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end ()

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

    block_t *block = *pp_block;
    if (!block)
        return NULL;

    if (block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
        return NULL;

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
        const char *error  = vpx_codec_error(ctx);
        const char *detail = vpx_codec_error_detail(ctx);
        if (!detail)
            detail = "no specific information";
        msg_Err(dec, "Failed to decode frame: %s (%s)", error, detail);
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
 * Open: probe the decoder
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
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
        const char *error = vpx_codec_error(&sys->ctx);
        msg_Err(p_this, "Failed to initialize decoder: %s\n", error);
        free(sys);
        return VLC_EGENERIC;;
    }

    dec->pf_decode_video = Decode;

    dec->fmt_out.i_cat = VIDEO_ES;
    dec->fmt_out.video.i_width = dec->fmt_in.video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in.video.i_height;
    dec->fmt_out.i_codec = VLC_CODEC_I420;
    dec->b_need_packetized = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: decoder destruction
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
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
