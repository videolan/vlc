/*****************************************************************************
 * vp9_frame.c: VP9 uncompressed header parser
 *****************************************************************************
 * Copyright (C) 2023-2026 VideoLabs, VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_bits.h>
#include <vlc_es.h>

#include "vp9_frame.h"
#include "iso_color_tables.h"

struct
{
    video_color_primaries_t prim;
    video_transfer_func_t xfer;
    video_color_space_t mc;
} static const vp9color_space_tovlc[8] = {
    { COLOR_PRIMARIES_UNDEF, TRANSFER_FUNC_UNDEF, COLOR_SPACE_UNDEF },
    { COLOR_PRIMARIES_BT601_525, TRANSFER_FUNC_BT709, COLOR_SPACE_BT601 },
    { COLOR_PRIMARIES_BT709, TRANSFER_FUNC_BT709, COLOR_SPACE_BT709 },
    { COLOR_PRIMARIES_SMTPE_170, TRANSFER_FUNC_SMPTE_170, COLOR_SPACE_SMPTE_170 },
    { COLOR_PRIMARIES_SMTPE_240, TRANSFER_FUNC_SMPTE_240, COLOR_SPACE_SMPTE_240 },
    { COLOR_PRIMARIES_BT2020, TRANSFER_FUNC_BT2020, COLOR_SPACE_BT2020 },
    { COLOR_PRIMARIES_UNDEF, TRANSFER_FUNC_UNDEF, COLOR_SPACE_UNDEF },
    { COLOR_PRIMARIES_SRGB, TRANSFER_FUNC_SRGB, COLOR_SPACE_SRGB },
};

bool vp9_configs_equals(const struct vp9_uncompressed_header *a,
                        const struct vp9_uncompressed_header *b)
{
    return a->profile == b->profile &&
           a->bit_depth == b->bit_depth &&
           a->color_space == b->color_space &&
           a->color_range == b->color_range &&
           a->subsampling_xy == b->subsampling_xy &&
           a->frame_width_minus1 == b->frame_width_minus1 &&
           a->frame_height_minus1 == b->frame_height_minus1 &&
           a->render_width_minus1 == b->render_width_minus1 &&
           a->render_height_minus1 == b->render_height_minus1;
}

bool vp9_fill_decoder_config(const struct vp9_uncompressed_header *hdr,
                             int level,
                             video_chroma_location_t chromaloc,
                             uint8_t dcr[VP9_DECODER_CONFIG_SIZE])
{
    memset(dcr, 0, VP9_DECODER_CONFIG_SIZE);
    dcr[0] = 0x01;
    dcr[4] = hdr->profile;
    dcr[5] = level > 0 ? level : 0;
    /* There is no mapping from YUV440 to DCR */
    uint8_t chroma;
    if(hdr->subsampling_xy == VP9_YUV_440)
        return false;
    else if(hdr->subsampling_xy == VP9_YUV_420)
        chroma = (chromaloc == CHROMA_LOCATION_LEFT) ? 0 : 1;
    else
        chroma = hdr->subsampling_xy == VP9_YUV_422 ? 2 : 3;
    dcr[6] = (hdr->bit_depth << 4)
           | (chroma << 1)
           | hdr->color_range;
    dcr[7] = vlc_primaries_to_iso_23001_8_cp( vp9color_space_tovlc[hdr->color_space].prim );
    dcr[8] = vlc_xfer_to_iso_23001_8_tc( vp9color_space_tovlc[hdr->color_space].xfer );
    dcr[9] = vlc_coeffs_to_iso_23001_8_mc( vp9color_space_tovlc[hdr->color_space].mc );
    return true;
}

void vp9_get_colorimetry(const struct vp9_uncompressed_header *h,
                         video_color_primaries_t *p_primaries,
                         video_transfer_func_t *p_transfer,
                         video_color_space_t *p_colorspace,
                         video_color_range_t *p_full_range)
{
    *p_primaries = vp9color_space_tovlc[h->color_space].prim;
    *p_transfer = vp9color_space_tovlc[h->color_space].xfer;
    *p_colorspace = vp9color_space_tovlc[h->color_space].mc;
    *p_full_range = h->color_range ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
}

static bool vp9_parse_frame_sync_code(bs_t *bs)
{
    return bs_read(bs, 8) == 0x49 &&
           bs_read(bs, 8) == 0x83 &&
           bs_read(bs, 8) == 0x42;
}

static void vp9_parse_color_config(bs_t *bs,
                                   struct vp9_uncompressed_header *h)
{
    if(h->profile >= 2)
        h->bit_depth = bs_read1(bs) ? 12 : 10;
    else
        h->bit_depth = 8;
    h->color_space = bs_read(bs, 3);
    if(h->color_space != VP9_CS_RGB)
    {
        h->color_range = bs_read1(bs);
        if(h->profile == 1 || h->profile == 3)
        {
            h->subsampling_xy = bs_read(bs, 2);
            bs_skip(bs, 1);
        }
        else
            h->subsampling_xy = VP9_YUV_420;
    }
    else
    {
        h->color_range = 1;
        if(h->profile == 1 || h->profile == 3)
        {
            h->subsampling_xy = VP9_YUV_444;
            bs_skip(bs, 1);
        }
    }
}

static void vp9_parse_frame_render_sizes(bs_t *bs,
                                         struct vp9_uncompressed_header *h)
{
    /* frame size */
    h->frame_width_minus1 = bs_read(bs, 16);
    h->frame_height_minus1 = bs_read(bs, 16);
    /* render size */
    if(bs_read1(bs))
    {
        h->render_width_minus1 = bs_read(bs, 16);
        h->render_height_minus1 = bs_read(bs, 16);
    }
    else
    {
        h->render_width_minus1 = h->frame_width_minus1;
        h->render_height_minus1 = h->frame_height_minus1;
    }
}

bool vp9_parse_uncompressed_header(const uint8_t *p, size_t sz,
                                   struct vp9_uncompressed_header *h)
{
    bs_t bs;
    bs_init(&bs, p, sz);
    bs_skip(&bs, 2);
    h->profile = bs_read1(&bs) | (bs_read1(&bs) << 1);
    if(h->profile == 3)
        bs_skip(&bs, 1);
    h->show_existing_frame = bs_read1(&bs);
    if(h->show_existing_frame)
    {
        bs_skip(&bs, 3); /* frame to show idx */
        return bs_error(&bs) ? false : true;
    }
    h->frame_type = bs_read1(&bs);
    h->show_frame = bs_read1(&bs);
    uint8_t error_resilient_mode = bs_read(&bs, 1);
    if(h->frame_type == VP9_KEY_FRAME)
    {
        if(!vp9_parse_frame_sync_code(&bs))
            return false;
        vp9_parse_color_config(&bs, h);
        vp9_parse_frame_render_sizes(&bs, h);
    }
    else
    {
        if(h->show_frame == 0)
            h->intra_only = bs_read1(&bs);
        else
            h->intra_only = 0;
        if(error_resilient_mode == 0)
            bs_skip(&bs, 2);
        if(h->intra_only == 1)
        {
            if(!vp9_parse_frame_sync_code(&bs))
                return false;
            if(h->profile > 0)
            {
                vp9_parse_color_config(&bs, h);
            }
            else
            {
                h->color_space = VP9_CS_BT_601;
                h->subsampling_xy = VP9_YUV_420;
                h->bit_depth = 8;
            }
            bs_skip(&bs, 8);
            vp9_parse_frame_render_sizes(&bs, h);
        }
    }
    return (bs_eof(&bs) || bs_error(&bs)) ? false : true;
}
