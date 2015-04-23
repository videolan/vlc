/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef H264_NAL_H
# define H264_NAL_H

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <vlc_common.h>
# include <vlc_codec.h>

#define PROFILE_H264_BASELINE             66
#define PROFILE_H264_MAIN                 77
#define PROFILE_H264_EXTENDED             88
#define PROFILE_H264_HIGH                 100
#define PROFILE_H264_HIGH_10              110
#define PROFILE_H264_HIGH_422             122
#define PROFILE_H264_HIGH_444             144
#define PROFILE_H264_HIGH_444_PREDICTIVE  244

#define PROFILE_H264_CAVLC_INTRA          44
#define PROFILE_H264_SVC_BASELINE         83
#define PROFILE_H264_SVC_HIGH             86
#define PROFILE_H264_MVC_STEREO_HIGH      128
#define PROFILE_H264_MVC_MULTIVIEW_HIGH   118

/* Parse the SPS/PPS Metadata and convert it to annex b format */
int convert_sps_pps( decoder_t *p_dec, const uint8_t *p_buf,
                     uint32_t i_buf_size, uint8_t *p_out_buf,
                     uint32_t i_out_buf_size, uint32_t *p_sps_pps_size,
                     uint32_t *p_nal_size);

/* Convert H.264 NAL format to annex b in-place */
struct H264ConvertState {
    uint32_t nal_len;
    uint32_t nal_pos;
};

void convert_h264_to_annexb( uint8_t *p_buf, uint32_t i_len,
                             size_t i_nal_size,
                             struct H264ConvertState *state );

/* Get level and Profile */
bool h264_get_profile_level(const es_format_t *p_fmt, size_t *p_profile,
                            size_t *p_level, size_t *p_nal_size);

#endif /* H264_NAL_H */
