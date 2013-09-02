
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "mkv.hpp"

#ifdef HAVE_ZLIB_H
int32_t zlib_decompress_extra( demux_t * p_demux, mkv_track_t * tk );
block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block );
#endif

block_t *MemToBlock( uint8_t *p_mem, size_t i_mem, size_t offset);
void handle_real_audio(demux_t * p_demux, mkv_track_t * p_tk, block_t * p_blk, mtime_t i_pts);


struct real_audio_private
{
    uint32_t fourcc;
    uint16_t version;
    uint16_t unknown1;
    uint8_t  unknown2[12];
    uint16_t unknown3;
    uint16_t flavor;
    uint32_t coded_frame_size;
    uint32_t unknown4[3];
    uint16_t sub_packet_h;
    uint16_t frame_size;
    uint16_t sub_packet_size;
    uint16_t unknown5;
};

struct real_audio_private_v4
{
    real_audio_private header;
    uint16_t sample_rate;
    uint16_t unknown;
    uint16_t sample_size;
    uint16_t channels;
};


struct real_audio_private_v5
{
    real_audio_private header;
    uint32_t unknown1;
    uint16_t unknown2;
    uint16_t sample_rate;
    uint16_t unknown3;
    uint16_t sample_size;
    uint16_t channels;
};

class Cook_PrivateTrackData : public PrivateTrackData
{
public:
    Cook_PrivateTrackData(uint16_t sph, uint16_t fs, uint16_t sps):
        i_sub_packet_h(sph), i_frame_size(fs), i_subpacket_size(sps),
        p_subpackets(NULL), i_subpackets(0), i_subpacket(0){}
    ~Cook_PrivateTrackData();
    int32_t Init();

    uint16_t i_sub_packet_h;
    uint16_t i_frame_size;
    uint16_t i_subpacket_size;
    block_t  **p_subpackets;
    size_t   i_subpackets;
    size_t   i_subpacket;
};

block_t * packetize_wavpack( mkv_track_t *, uint8_t *, size_t);
