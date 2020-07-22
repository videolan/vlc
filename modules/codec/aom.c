/*****************************************************************************
 * aom.c: libaom encoder and decoder (AV1) module
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

#ifdef ENABLE_SOUT
# include <aom/aom_encoder.h>
# include <aom/aomcx.h>
# include <aom/aom_image.h>
# define SOUT_CFG_PREFIX "sout-aom-"
#endif

#include "../packetizer/iso_color_tables.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
#ifdef ENABLE_SOUT
static int OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict);

static const int pi_enc_bitdepth_values_list[] =
  { 8, 10, 12 };
static const char *const ppsz_enc_bitdepth_text [] =
  { N_("8 bpp"), N_("10 bpp"), N_("12 bpp") };
#endif

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
#ifdef ENABLE_SOUT
    add_submodule()
        set_shortname("aom")
        set_capability("encoder", 101)
        set_description(N_("AOM video encoder"))
        set_callbacks(OpenEncoder, CloseEncoder)
        add_integer( SOUT_CFG_PREFIX "profile", 0, "Profile", NULL, true )
            change_integer_range( 0, 3 )
        add_integer( SOUT_CFG_PREFIX "bitdepth", 8, "Bit Depth", NULL, true )
            change_integer_list( pi_enc_bitdepth_values_list, ppsz_enc_bitdepth_text )
        add_integer( SOUT_CFG_PREFIX "tile-rows", 0, "Tile Rows (in log2 units)", NULL, true )
            change_integer_range( 0, 6 ) /* 1 << 6 == MAX_TILE_ROWS */
        add_integer( SOUT_CFG_PREFIX "tile-columns", 0, "Tile Columns (in log2 units)", NULL, true )
            change_integer_range( 0, 6 ) /* 1 << 6 == MAX_TILE_COLS */
        add_integer( SOUT_CFG_PREFIX "cpu-used", 1, "Speed setting", NULL, true )
            change_integer_range( 0, 8 ) /* good: 0-5, realtime: 6-8 */
        add_integer( SOUT_CFG_PREFIX "lag-in-frames", 16, "Maximum number of lookahead frames", NULL, true )
            change_integer_range(0, 70 /* MAX_LAG_BUFFERS + MAX_LAP_BUFFERS */ )
        add_integer( SOUT_CFG_PREFIX "usage", 0, "Usage (0: good, 1: realtime)", NULL, true )
            change_integer_range( 0, 1 )
        add_integer( SOUT_CFG_PREFIX "rc-end-usage", 1, "Usage (0: VBR, 1: CBR, 2: CQ, 3: Q)", NULL, true )
            change_integer_range( 0, 4 )
#ifdef AOM_CTRL_AV1E_SET_ROW_MT
        add_bool( SOUT_CFG_PREFIX "row-mt", false, "Row Multithreading", NULL, true )
#endif
#endif
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
    vlc_tick_t pts;
};

typedef struct
{
    aom_codec_ctx_t ctx;
    struct frame_priv_s frame_priv[AOM_MAX_FRAMES_DEPTH];
    unsigned i_next_frame_priv;
} decoder_sys_t;

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
    uintptr_t priv_index = p_sys->i_next_frame_priv++ % AOM_MAX_FRAMES_DEPTH;

    if(likely(block))
    {
        p_buffer = block->p_buffer;
        i_buffer = block->i_buffer;
        p_sys->frame_priv[priv_index].pts = (block->i_pts != VLC_TICK_INVALID) ? block->i_pts : block->i_dts;
    }
    else
    {
        p_buffer = NULL;
        i_buffer = 0;
    }

    aom_codec_err_t err;
    err = aom_codec_decode(ctx, p_buffer, i_buffer, (void*)priv_index);

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

    if(dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF)
    {
        v->primaries = iso_23001_8_cp_to_vlc_primaries(img->cp);
        v->transfer = iso_23001_8_tc_to_vlc_xfer(img->tc);
        v->space = iso_23001_8_mc_to_vlc_coeffs(img->mc);
        v->color_range = img->range == AOM_CR_FULL_RANGE ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    }

    dec->fmt_out.video.projection_mode = dec->fmt_in.video.projection_mode;
    dec->fmt_out.video.multiview_mode = dec->fmt_in.video.multiview_mode;
    dec->fmt_out.video.pose = dec->fmt_in.video.pose;

    if (decoder_UpdateVideoFormat(dec) == 0)
    {
        picture_t *pic = decoder_NewPicture(dec);
        if (pic)
        {
            decoder_sys_t *p_sys = dec->p_sys;
            CopyPicture(img, pic);

            /* fetches back the PTS */

            pic->b_progressive = true; /* codec does not support interlacing */
            pic->date = p_sys->frame_priv[(uintptr_t)img->user_priv].pts;

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
    dec->fmt_out.video.primaries   = dec->fmt_in.video.primaries;
    dec->fmt_out.video.transfer    = dec->fmt_in.video.transfer;
    dec->fmt_out.video.space       = dec->fmt_in.video.space;
    dec->fmt_out.video.color_range = dec->fmt_in.video.color_range;

    return VLC_SUCCESS;
}

static void destroy_context(vlc_object_t *p_this, aom_codec_ctx_t *context)
{
    if (aom_codec_destroy(context))
        AOM_ERR(p_this, context, "Failed to destroy codec context");
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

    destroy_context(p_this, &sys->ctx);

    free(sys);
}

#ifdef ENABLE_SOUT

#ifndef AOM_USAGE_REALTIME
# define AOM_USAGE_REALTIME 1
#endif

/*****************************************************************************
 * encoder_sys_t: libaom encoder descriptor
 *****************************************************************************/
typedef struct
{
    struct aom_codec_ctx ctx;
} encoder_sys_t;

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if (p_enc->fmt_out.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the encoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_enc->p_sys = p_sys;

    const struct aom_codec_iface *iface = &aom_codec_av1_cx_algo;

    struct aom_codec_enc_cfg enccfg = { 0 };
    aom_codec_enc_config_default(iface, &enccfg, 0);
    /* TODO: implement 2-pass */
    enccfg.g_pass = AOM_RC_ONE_PASS;
    enccfg.g_timebase.num = p_enc->fmt_in.video.i_frame_rate_base;
    enccfg.g_timebase.den = p_enc->fmt_in.video.i_frame_rate;
    enccfg.g_threads = __MIN(vlc_GetCPUCount(), 4);
    enccfg.g_w = p_enc->fmt_in.video.i_visible_width;
    enccfg.g_h = p_enc->fmt_in.video.i_visible_height;
    enccfg.rc_end_usage = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "rc-end-usage" );
    enccfg.g_usage = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "usage" );
    /* we have no pcr on sout, hence this defaulting to 16 */
    enccfg.g_lag_in_frames = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "lag-in-frames" );
    if( enccfg.g_usage == AOM_USAGE_REALTIME && enccfg.g_lag_in_frames != 0 )
    {
        msg_Warn( p_enc, "Non-zero lag in frames is not supported for realtime, forcing 0" );
        enccfg.g_lag_in_frames = 0;
    }

    int enc_flags;
    int i_profile = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "profile" );
    int i_bit_depth = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "bitdepth" );
    int i_tile_rows = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "tile-rows" );
    int i_tile_columns = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "tile-columns" );
#ifdef AOM_CTRL_AV1E_SET_ROW_MT
    bool b_row_mt = var_GetBool( p_enc, SOUT_CFG_PREFIX "row-mt" );
#endif

    /* TODO: implement higher profiles, bit depths and other pixformats. */
    switch( i_profile )
    {
        case 0:
            /* Main Profile: 8 and 10-bit 4:2:0. */
            enccfg.g_profile = 0;
            switch( i_bit_depth )
            {
                case 10:
                    p_enc->fmt_in.i_codec = VLC_CODEC_I420_10L;
                    enc_flags = AOM_CODEC_USE_HIGHBITDEPTH;
                    break;
                case 8:
                    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
                    enc_flags = 0;
                    break;
                default:
                    msg_Err( p_enc, "%d bit is unsupported for profile %d", i_bit_depth, i_profile );
                    free( p_sys );
                    return VLC_EGENERIC;
            }
            enccfg.g_bit_depth = i_bit_depth;
            break;

        case 1:
            /* High Profile: 8 and 10-bit 4:4:4 */
            /* fallthrough */
        case 2:
            /* Professional Profile: 8, 10 and 12-bit for 4:2:2, otherwise 12-bit. */
            /* fallthrough */
        default:
            msg_Err( p_enc, "Unsupported profile %d", i_profile );
            free( p_sys );
            return VLC_EGENERIC;
    }

    msg_Dbg(p_this, "AV1: using libaom version %s (build options %s)",
        aom_codec_version_str(), aom_codec_build_config());

    struct aom_codec_ctx *ctx = &p_sys->ctx;
    if (aom_codec_enc_init(ctx, iface, &enccfg, enc_flags) != AOM_CODEC_OK)
    {
        AOM_ERR(p_this, ctx, "Failed to initialize encoder");
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (i_tile_rows >= 0 &&
        aom_codec_control(ctx, AV1E_SET_TILE_ROWS, i_tile_rows))
    {
        AOM_ERR(p_this, ctx, "Failed to set tile rows");
        destroy_context(p_this, ctx);
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (i_tile_columns >= 0 &&
        aom_codec_control(ctx, AV1E_SET_TILE_COLUMNS, i_tile_columns))
    {
        AOM_ERR(p_this, ctx, "Failed to set tile columns");
        destroy_context(p_this, ctx);
        free(p_sys);
        return VLC_EGENERIC;
    }

#ifdef AOM_CTRL_AV1E_SET_ROW_MT
    if (b_row_mt &&
        aom_codec_control(ctx, AV1E_SET_ROW_MT, b_row_mt))
    {
        AOM_ERR(p_this, ctx, "Failed to set row-multithreading");
        destroy_context(p_this, ctx);
        free(p_sys);
        return VLC_EGENERIC;
    }
#endif

    int i_cpu_used = var_InheritInteger( p_enc, SOUT_CFG_PREFIX "cpu-used" );
    if (aom_codec_control(ctx, AOME_SET_CPUUSED, i_cpu_used))
    {
        AOM_ERR(p_this, ctx, "Failed to set cpu-used");
        destroy_context(p_this, ctx);
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_enc->pf_encode_video = Encode;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    struct aom_codec_ctx *ctx = &p_sys->ctx;

    if (!p_pict) return NULL;

    aom_image_t img = { 0 };
    unsigned i_w = p_enc->fmt_in.video.i_visible_width;
    unsigned i_h = p_enc->fmt_in.video.i_visible_height;
    const aom_img_fmt_t img_fmt = p_enc->fmt_in.i_codec == VLC_CODEC_I420_10L ?
        AOM_IMG_FMT_I42016 : AOM_IMG_FMT_I420;

    /* Create and initialize the aom_image */
    if (!aom_img_wrap(&img, img_fmt, i_w, i_h, 32, p_pict->p[0].p_pixels))
    {
        AOM_ERR(p_enc, ctx, "Failed to wrap image");
        return NULL;
    }

    /* Correct chroma plane offsets. */
    for (int plane = 1; plane < p_pict->i_planes; plane++) {
        img.planes[plane] = p_pict->p[plane].p_pixels;
        img.stride[plane] = p_pict->p[plane].i_pitch;
    }

    aom_codec_err_t res = aom_codec_encode(ctx, &img, US_FROM_VLC_TICK(p_pict->date), 1, 0);
    if (res != AOM_CODEC_OK) {
        AOM_ERR(p_enc, ctx, "Failed to encode frame");
        aom_img_free(&img);
        return NULL;
    }

    const aom_codec_cx_pkt_t *pkt = NULL;
    aom_codec_iter_t iter = NULL;
    block_t *p_out = NULL;
    while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL)
    {
        if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
        {
            int keyframe = pkt->data.frame.flags & AOM_FRAME_IS_KEY;
            block_t *p_block = block_Alloc(pkt->data.frame.sz);
            if (unlikely(p_block == NULL)) {
                block_ChainRelease(p_out);
                p_out = NULL;
                break;
            }

            /* FIXME: do this in-place */
            memcpy(p_block->p_buffer, pkt->data.frame.buf, pkt->data.frame.sz);
            p_block->i_dts = p_block->i_pts = VLC_TICK_FROM_US(pkt->data.frame.pts);
            if (keyframe)
                p_block->i_flags |= BLOCK_FLAG_TYPE_I;
            block_ChainAppend(&p_out, p_block);
        }
    }
    aom_img_free(&img);
    return p_out;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    destroy_context(p_this, &p_sys->ctx);
    free(p_sys);
}

#endif  /* ENABLE_SOUT */
