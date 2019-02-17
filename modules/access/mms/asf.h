/*****************************************************************************
 * asf.h: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/****************************************************************************
 * XXX:
 *  Definitions and data duplicated from asf demuxers but I want access
 * and demux plugins to be independent
 *
 ****************************************************************************/

#ifndef VLC_MMS_ASF_H_
#define VLC_MMS_ASF_H_

#include <vlc_codecs.h>

#define ASF_CODEC_TYPE_VIDEO   0x0001
#define ASF_CODEC_TYPE_AUDIO   0x0002
#define ASF_CODEC_TYPE_UNKNOWN 0xffff

typedef struct
{
    int i_cat;      /* ASF_CODEC_TYPE_VIDEO, ASF_CODEC_TYPE_AUDIO, */
    int i_bitrate;  /* -1 if unknown */
    int i_selected;
} asf_stream_t;

typedef struct
{
    int64_t      i_file_size;
    int64_t      i_data_packets_count;
    int32_t      i_min_data_packet_size;

    asf_stream_t stream[128];

} asf_header_t;


void  GenerateGuid      ( vlc_guid_t * );
void  asf_HeaderParse   ( asf_header_t *, uint8_t *, int );
void  asf_StreamSelect  ( asf_header_t *,
                              int i_bitrate_max, bool b_all, bool b_audio,
                              bool b_video );

#endif
