/*****************************************************************************
 * mpegvideo.h: MPEG1/2 video stream definitions
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VLC authors and VideoLAN
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

#define PROFILE_MPEG2_HIGH                  1
#define PROFILE_MPEG2_SPATIALLY_SCALABLE    2
#define PROFILE_MPEG2_SNR_SCALABLE          3
#define PROFILE_MPEG2_MAIN                  4
#define PROFILE_MPEG2_SIMPLE                5
#define PROFILE_MPEG2_422           (0x80 + 2)
#define PROFILE_MPEG2_MULTIVIEW     (0x80 +10)

#define LEVEL_MPEG2_HIGH                    4
#define LEVEL_MPEG2_HIGH_1440               6
#define LEVEL_MPEG2_MAIN                    8
#define LEVEL_MPEG2_LOW                    10
