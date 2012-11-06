/*****************************************************************************
 * asf.h: wma decoder using integer decoder from Rockbox, based on FFmpeg
 *****************************************************************************
 * Copyright (C) 2008 M2X
 *
 * Authors: Rafaël Carré <rcarre@m2x.nl>
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

#ifndef _ASF_H
#define _ASF_H

#include <inttypes.h>

/* ASF codec IDs */
#define ASF_CODEC_ID_WMAV1 0x160
#define ASF_CODEC_ID_WMAV2 0x161

struct asf_waveformatex_s {
    uint32_t packet_size;
    int audiostream;
    uint16_t codec_id;
    uint16_t channels;
    uint32_t rate;
    uint32_t bitrate;
    uint16_t blockalign;
    uint16_t bitspersample;
    uint16_t datalen;
    uint8_t data[6];
};
typedef struct asf_waveformatex_s asf_waveformatex_t;

#endif
