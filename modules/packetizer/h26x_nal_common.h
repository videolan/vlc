/*****************************************************************************
 * h26x_nal_common.h: h26x shared code
 *****************************************************************************
 * Copyright © 2010-2024 VideoLabs, VideoLAN and VLC Authors
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
#ifndef H26X_NAL_COMMON_H
# define H26X_NAL_COMMON_H

#include <assert.h>
#include <vlc_common.h>
#include "iso_color_tables.h"

typedef uint8_t  nal_u1_t;
typedef uint8_t  nal_u2_t;
typedef uint8_t  nal_u3_t;
typedef uint8_t  nal_u4_t;
typedef uint8_t  nal_u5_t;
typedef uint8_t  nal_u6_t;
typedef uint8_t  nal_u7_t;
typedef uint8_t  nal_u8_t;
typedef int32_t  nal_se_t;
typedef uint32_t nal_ue_t;

typedef struct
{
    nal_ue_t left_offset;
    nal_ue_t right_offset;
    nal_ue_t top_offset;
    nal_ue_t bottom_offset;
} h26x_conf_window_t;

static inline
bool h26x_get_picture_size( nal_u2_t chroma_format_idc,
                            nal_ue_t pic_width_in_luma_samples,
                            nal_ue_t pic_height_in_luma_samples,
                            const h26x_conf_window_t *conf_win,
                            unsigned *p_ox, unsigned *p_oy,
                            unsigned *p_w, unsigned *p_h,
                            unsigned *p_vw, unsigned *p_vh )
{
    unsigned ox, oy, w, h, vw, vh;
    w = vw = pic_width_in_luma_samples;
    h = vh = pic_height_in_luma_samples;

    if( chroma_format_idc > 3 )
    {
        assert( chroma_format_idc <= 3 );
        return false;
    }

    static const uint8_t SubWidthHeight[4][2] = { {1, 1}, {2, 2}, {2, 1}, {1, 1} };

    ox = conf_win->left_offset * SubWidthHeight[chroma_format_idc][0];
    oy = conf_win->top_offset * SubWidthHeight[chroma_format_idc][1];

    vw -= (conf_win->left_offset +  conf_win->right_offset) *
          SubWidthHeight[chroma_format_idc][0];
    vh -= (conf_win->bottom_offset + conf_win->top_offset) *
          SubWidthHeight[chroma_format_idc][1];

    if ( vw > w || vh > h )
        return false;

    *p_ox = ox; *p_oy = oy;
    *p_w = w; *p_h = h;
    *p_vw = vw; *p_vh = vh;
    return true;
}

typedef struct
{
    nal_u8_t aspect_ratio_idc;
    uint16_t sar_width;
    uint16_t sar_height;
} h26x_aspect_ratio_t;

static inline
bool h26x_get_aspect_ratio( const h26x_aspect_ratio_t *ar,
                            unsigned *num, unsigned *den )
{
    if( ar->aspect_ratio_idc != 255 )
    {
        static const uint8_t ar_table[16][2] =
            {
             {    1,      1 },
             {   12,     11 },
             {   10,     11 },
             {   16,     11 },
             {   40,     33 },
             {   24,     11 },
             {   20,     11 },
             {   32,     11 },
             {   80,     33 },
             {   18,     11 },
             {   15,     11 },
             {   64,     33 },
             {  160,     99 },
             {    4,      3 },
             {    3,      2 },
             {    2,      1 },
             };
        if( ar->aspect_ratio_idc == 0 ||
            ar->aspect_ratio_idc >= 17 )
            return false;
        *num = ar_table[ar->aspect_ratio_idc - 1][0];
        *den = ar_table[ar->aspect_ratio_idc - 1][1];
    }
    else
    {
        *num = ar->sar_width;
        *den = ar->sar_height;
    }
    return true;
}

typedef struct
{
    nal_u8_t colour_primaries;
    nal_u8_t transfer_characteristics;
    nal_u8_t matrix_coeffs;
    nal_u1_t full_range_flag;
} h26x_colour_description_t;

static inline
bool h26x_get_colorimetry( const h26x_colour_description_t *colour,
                           video_color_primaries_t *p_primaries,
                           video_transfer_func_t *p_transfer,
                           video_color_space_t *p_colorspace,
                           video_color_range_t *p_full_range )
{
    *p_primaries =
        iso_23001_8_cp_to_vlc_primaries( colour->colour_primaries );
    *p_transfer =
        iso_23001_8_tc_to_vlc_xfer( colour->transfer_characteristics );
    *p_colorspace =
        iso_23001_8_mc_to_vlc_coeffs( colour->matrix_coeffs );
    *p_full_range = colour->full_range_flag ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    return true;
}

#endif
