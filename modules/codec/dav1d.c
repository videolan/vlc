/*****************************************************************************
 * dav1d.c: dav1d decoder (AV1) module
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 * Based on aom.c by: Tristan Matthews <tmatth@videolan.org>
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
#include <vlc_timestamp_helper.h>

#include <errno.h>
#include <dav1d/dav1d.h>

#include "../packetizer/iso_color_tables.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define THREAD_FRAMES_TEXT N_("Frames Threads")
#define THREAD_FRAMES_LONGTEXT N_( "Max number of threads used for frame decoding, default 0=auto" )
#define THREAD_TILES_TEXT N_("Tiles Threads")
#define THREAD_TILES_LONGTEXT N_( "Max number of threads used for tile decoding, default 0=auto" )


vlc_module_begin ()
    set_shortname("dav1d")
    set_description(N_("Dav1d video decoder"))
    set_capability("video decoder", 10000)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)

#if DAV1D_API_VERSION_MAJOR >= 6
    add_integer_with_range("dav1d-thread-frames", 0, 0, DAV1D_MAX_THREADS,
                THREAD_FRAMES_TEXT, THREAD_FRAMES_LONGTEXT, false)
    add_obsolete_string("dav1d-thread-tiles") // unused with dav1d 1.0
#else
    add_integer_with_range("dav1d-thread-frames", 0, 0, DAV1D_MAX_FRAME_THREADS,
                THREAD_FRAMES_TEXT, THREAD_FRAMES_LONGTEXT, false)
    add_integer_with_range("dav1d-thread-tiles", 0, 0, DAV1D_MAX_TILE_THREADS,
                THREAD_TILES_TEXT, THREAD_TILES_LONGTEXT, false)
#endif
vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t: libaom decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    Dav1dSettings s;
    Dav1dContext *c;
};

static const struct
{
    vlc_fourcc_t          i_chroma;
    enum Dav1dPixelLayout i_chroma_id;
    uint8_t               i_bitdepth;
    enum Dav1dTransferCharacteristics transfer_characteristics;
} chroma_table[] =
{
    /* Transfer characteristic-dependent mappings must come first */
    {VLC_CODEC_GBR_PLANAR, DAV1D_PIXEL_LAYOUT_I444, 8, DAV1D_TRC_SRGB},
    {VLC_CODEC_GBR_PLANAR_10L, DAV1D_PIXEL_LAYOUT_I444, 10, DAV1D_TRC_SRGB},

    {VLC_CODEC_GREY, DAV1D_PIXEL_LAYOUT_I400, 8, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I420, DAV1D_PIXEL_LAYOUT_I420, 8, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I422, DAV1D_PIXEL_LAYOUT_I422, 8, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I444, DAV1D_PIXEL_LAYOUT_I444, 8, DAV1D_TRC_UNKNOWN},

    {VLC_CODEC_I420_10L, DAV1D_PIXEL_LAYOUT_I420, 10, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I422_10L, DAV1D_PIXEL_LAYOUT_I422, 10, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I444_10L, DAV1D_PIXEL_LAYOUT_I444, 10, DAV1D_TRC_UNKNOWN},

    {VLC_CODEC_I420_12L, DAV1D_PIXEL_LAYOUT_I420, 12, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I422_12L, DAV1D_PIXEL_LAYOUT_I422, 12, DAV1D_TRC_UNKNOWN},
    {VLC_CODEC_I444_12L, DAV1D_PIXEL_LAYOUT_I444, 12, DAV1D_TRC_UNKNOWN},
};

static vlc_fourcc_t FindVlcChroma(const Dav1dPicture *img)
{

    for (unsigned int i = 0; i < ARRAY_SIZE(chroma_table); i++)
        if (chroma_table[i].i_chroma_id == img->p.layout &&
            chroma_table[i].i_bitdepth == img->p.bpc &&
            (chroma_table[i].transfer_characteristics == DAV1D_TRC_UNKNOWN ||
             chroma_table[i].transfer_characteristics == img->seq_hdr->trc))
            return chroma_table[i].i_chroma;

    return 0;
}

static int NewPicture(Dav1dPicture *img, void *cookie)
{
    decoder_t *dec = cookie;

    video_format_t *v = &dec->fmt_out.video;

    v->i_visible_width  = img->p.w;
    v->i_visible_height = img->p.h;
    v->i_width  = (img->p.w + 0x7F) & ~0x7F;
    v->i_height = (img->p.h + 0x7F) & ~0x7F;

    if( !v->i_sar_num || !v->i_sar_den )
    {
        v->i_sar_num = 1;
        v->i_sar_den = 1;
    }

    if(dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF && img->seq_hdr)
    {
        v->primaries = iso_23001_8_cp_to_vlc_primaries(img->seq_hdr->pri);
        v->transfer = iso_23001_8_tc_to_vlc_xfer(img->seq_hdr->trc);
        v->space = iso_23001_8_mc_to_vlc_coeffs(img->seq_hdr->mtrx);
        v->b_color_range_full = img->seq_hdr->color_range;
    }

    const Dav1dMasteringDisplay *md = img->mastering_display;
    if( dec->fmt_in.video.mastering.max_luminance == 0 && md )
    {
        for( size_t i=0;i<6; i++ )
            v->mastering.primaries[i] = md->primaries[i >> 1][i % 2];
        v->mastering.min_luminance = md->min_luminance;
        v->mastering.max_luminance = md->max_luminance;
        v->mastering.white_point[0] = md->white_point[0];
        v->mastering.white_point[1] = md->white_point[1];
    }

    const Dav1dContentLightLevel *cll = img->content_light;
    if( dec->fmt_in.video.lighting.MaxCLL == 0 && cll )
    {
        v->lighting.MaxCLL = cll->max_content_light_level;
        v->lighting.MaxFALL = cll->max_frame_average_light_level;
    }

    v->projection_mode = dec->fmt_in.video.projection_mode;
    v->multiview_mode = dec->fmt_in.video.multiview_mode;
    v->pose = dec->fmt_in.video.pose;
    dec->fmt_out.video.i_chroma = dec->fmt_out.i_codec = FindVlcChroma(img);

    if (decoder_UpdateVideoFormat(dec) == VLC_SUCCESS)
    {
        picture_t *pic = decoder_NewPicture(dec);
        if (likely(pic != NULL))
        {
            img->data[0] = pic->p[0].p_pixels;
            img->stride[0] = pic->p[0].i_pitch;
            img->data[1] = pic->p[1].p_pixels;
            img->data[2] = pic->p[2].p_pixels;
            assert(pic->p[1].i_pitch == pic->p[2].i_pitch);
            img->stride[1] = pic->p[1].i_pitch;
            img->allocator_data = pic;

            return 0;
        }
    }
    return -1;
}

static void FreePicture(Dav1dPicture *data, void *cookie)
{
    picture_t *pic = data->allocator_data;
    decoder_t *dec = cookie;
    VLC_UNUSED(dec);
    picture_Release(pic);
}

/****************************************************************************
 * Flush: clears decoder between seeks
 ****************************************************************************/

static void FlushDecoder(decoder_t *dec)
{
    decoder_sys_t *p_sys = dec->p_sys;
    dav1d_flush(p_sys->c);
}

static void release_block(const uint8_t *buf, void *b)
{
    VLC_UNUSED(buf);
    block_t *block = b;
    block_Release(block);
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************/
static int Decode(decoder_t *dec, block_t *block)
{
    decoder_sys_t *p_sys = dec->p_sys;

    if (block && block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    bool b_eos = false;
    Dav1dData data;
    Dav1dData *p_data = NULL;

    if (block)
    {
        p_data = &data;
        if (unlikely(dav1d_data_wrap(&data, block->p_buffer, block->i_buffer,
                                     release_block, block) != 0))
        {
            block_Release(block);
            return VLCDEC_ECRITICAL;
        }
        vlc_tick_t pts = block->i_pts == VLC_TICK_INVALID ? block->i_dts : block->i_pts;
        p_data->m.timestamp = pts;
        b_eos = (block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE);
    }

    Dav1dPicture img = { 0 };

    bool b_draining = false;
    int i_ret = VLCDEC_SUCCESS;
    int res;
    do {
        if( p_data )
        {
            res = dav1d_send_data(p_sys->c, p_data);
            if (res < 0 && res != DAV1D_ERR(EAGAIN))
            {
                msg_Err(dec, "Decoder feed error %d!", res);
                /* bitstream decoding errors (typically DAV1D_ERR(EINVAL), are assumed
                 * to be recoverable. Other errors returned from this function are either
                 * unexpected within the VLC configuration, or considered critical failures:
                 * - EAGAIN is handled above.
                 * - ENOMEM means out-of-memory and is unrecoverable.
                 * - ENOPROTOOPT is a build or configuration error (invalid demuxer/muxer or unsupported bitdepth) and is unrecoverable.
                 * - ERANGE means frame size limits exceeded. VLC doesn't use this so we can ignore this, but unless size changes, it would be unrecoverable.
                 * - EINVAL is any other bitstream error which is basically what this is about.
                 * - EIO means file count not be opened and is unrecoverable.
                 * - ENOENT  is actually only returned by dav1d_parse_sequence_header(), which is outside this context (I think?).
                 * - read() can return other values but it's OK to consider these critical for now. */
                i_ret = res == DAV1D_ERR(EINVAL) ? VLCDEC_SUCCESS : VLCDEC_ECRITICAL;
                break;
            }
        }

        res = dav1d_get_picture(p_sys->c, &img);
        if (res == 0)
        {
            picture_t *_pic = img.allocator_data;
            picture_t *pic = picture_Clone(_pic);
            if (unlikely(pic == NULL))
            {
                i_ret = VLC_EGENERIC;
                picture_Release(_pic);
                break;
            }
            pic->b_progressive = true; /* codec does not support interlacing */
            pic->date = img.m.timestamp;
            decoder_QueueVideo(dec, pic);
            dav1d_picture_unref(&img);
        }
        else if (res != DAV1D_ERR(EAGAIN))
        {
            msg_Warn(dec, "Decoder error %d!", res);
            break;
        }

        /* on drain, we must ignore the 1st EAGAIN */
        if(!b_draining && (res == DAV1D_ERR(EAGAIN) || res == 0)
                       && (p_data == NULL||b_eos))
        {
            b_draining = true;
            res = 0;
        }
    } while (res == 0 || (p_data && p_data->sz != 0));


    return i_ret;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    unsigned i_core_count = vlc_GetCPUCount();

    if (dec->fmt_in.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    dav1d_default_settings(&p_sys->s);
#if DAV1D_API_VERSION_MAJOR >= 6
    p_sys->s.n_threads = var_InheritInteger(p_this, "dav1d-thread-frames");
    if (p_sys->s.n_threads == 0)
        p_sys->s.n_threads = (i_core_count < 16) ? i_core_count : 16;

#if DAV1D_API_VERSION_MAJOR > 6 || DAV1D_API_VERSION_MINOR >= 7
    // after dav1d 1.0.0
    p_sys->s.max_frame_delay = dav1d_get_frame_delay( &p_sys->s );
#else // 1.0.0
    // corresponds to c->n_fc when max_frame_delay is 0 in dav1d 1.0.0
    static const uint8_t fc_lut[49] = {
        1,                                     /*     1 */
        2, 2, 2,                               /*  2- 4 */
        3, 3, 3, 3, 3,                         /*  5- 9 */
        4, 4, 4, 4, 4, 4, 4,                   /* 10-16 */
        5, 5, 5, 5, 5, 5, 5, 5, 5,             /* 17-25 */
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,       /* 26-36 */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, /* 37-49 */
    };
    if (p_sys->s.n_threads >= 50)
        p_sys->s.max_frame_delay = 8;
    else
        p_sys->s.max_frame_delay = fc_lut[p_sys->s.n_threads - 1];
#endif

#else // before dav1d 1.0.0
    p_sys->s.n_tile_threads = var_InheritInteger(p_this, "dav1d-thread-tiles");
    if (p_sys->s.n_tile_threads == 0)
        p_sys->s.n_tile_threads =
            (i_core_count > 4) ? 4 :
            (i_core_count > 1) ? i_core_count :
            1;
    p_sys->s.n_frame_threads = var_InheritInteger(p_this, "dav1d-thread-frames");
    if (p_sys->s.n_frame_threads == 0)
        p_sys->s.n_frame_threads = (i_core_count < 16) ? i_core_count : 16;
#endif
    p_sys->s.allocator.cookie = dec;
    p_sys->s.allocator.alloc_picture_callback = NewPicture;
    p_sys->s.allocator.release_picture_callback = FreePicture;

    if (dav1d_open(&p_sys->c, &p_sys->s) < 0)
    {
        msg_Err(p_this, "Could not open the Dav1d decoder");
        return VLC_EGENERIC;
    }

#if DAV1D_API_VERSION_MAJOR >= 6
    msg_Dbg(p_this, "Using dav1d version %s with %d threads",
            dav1d_version(), p_sys->s.n_threads);

    dec->i_extra_picture_buffers = p_sys->s.max_frame_delay;
#else
    msg_Dbg(p_this, "Using dav1d version %s with %d/%d frame/tile threads",
            dav1d_version(), p_sys->s.n_frame_threads, p_sys->s.n_tile_threads);

    dec->i_extra_picture_buffers = (p_sys->s.n_frame_threads - 1);
#endif

    dec->pf_decode = Decode;
    dec->pf_flush = FlushDecoder;

    dec->fmt_out.video.i_width = dec->fmt_in.video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in.video.i_height;
    dec->fmt_out.i_codec = VLC_CODEC_I420;
    dec->p_sys = p_sys;

    if (dec->fmt_in.video.i_sar_num > 0 && dec->fmt_in.video.i_sar_den > 0) {
        dec->fmt_out.video.i_sar_num = dec->fmt_in.video.i_sar_num;
        dec->fmt_out.video.i_sar_den = dec->fmt_in.video.i_sar_den;
    }
    dec->fmt_out.video.primaries   = dec->fmt_in.video.primaries;
    dec->fmt_out.video.transfer    = dec->fmt_in.video.transfer;
    dec->fmt_out.video.space       = dec->fmt_in.video.space;
    dec->fmt_out.video.b_color_range_full = dec->fmt_in.video.b_color_range_full;
    dec->fmt_out.video.mastering   = dec->fmt_in.video.mastering;
    dec->fmt_out.video.lighting    = dec->fmt_in.video.lighting;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = dec->p_sys;

    /* Flush decoder */
    FlushDecoder(dec);

    dav1d_close(&p_sys->c);
}

