/*****************************************************************************
 * vp9_frame.h: VP9 uncompressed header parser
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
#ifndef VLC_VP9_FRAME_H
#define VLC_VP9_FRAME_H

#include <vlc_common.h>
#include <vlc_es.h>

struct vp9_uncompressed_header
{
    uint8_t profile;
    uint8_t show_existing_frame;
    enum
    {
        VP9_KEY_FRAME = 0,
        VP9_NON_KEY_FRAME = 1,
    } frame_type;
    uint8_t show_frame;
    uint8_t intra_only;
    uint8_t bit_depth;
    enum
    {
        VP9_CS_UNKNOWN = 0,
        VP9_CS_BT_601  = 1,
        VP9_CS_BT_709  = 2,
        VP9_CS_SMPTE_170 = 3,
        VP9_CS_SMPTE_240 = 4,
        VP9_CS_BT_2020  = 5,
        VP9_CS_RESERVED = 6,
        VP9_CS_RGB = 7,
    } color_space;
    uint8_t color_range;
    enum
    {
        VP9_YUV_444 = (0 << 1) | 0,
        VP9_YUV_440 = (0 << 1) | 1,
        VP9_YUV_422 = (1 << 1) | 0,
        VP9_YUV_420 = (1 << 1) | 1,
    } subsampling_xy;
    uint16_t frame_width_minus1;
    uint16_t frame_height_minus1;
    uint16_t render_width_minus1;
    uint16_t render_height_minus1;
};

#define VP9_DECODER_CONFIG_SIZE 12

bool vp9_parse_uncompressed_header(const uint8_t *p, size_t sz,
                                   struct vp9_uncompressed_header *h);

bool vp9_configs_equals(const struct vp9_uncompressed_header *a,
                        const struct vp9_uncompressed_header *b);

bool vp9_fill_decoder_config(const struct vp9_uncompressed_header *hdr,
                             int level,
                             video_chroma_location_t chromaloc,
                             uint8_t dcr[VP9_DECODER_CONFIG_SIZE]);

void vp9_get_colorimetry(const struct vp9_uncompressed_header *h,
                         video_color_primaries_t *p_primaries,
                         video_transfer_func_t *p_transfer,
                         video_color_space_t *p_colorspace,
                         video_color_range_t *p_full_range);

#endif
