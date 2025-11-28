
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
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

namespace mkv {

#ifdef HAVE_ZLIB_H
int32_t zlib_decompress_extra( demux_t * p_demux, mkv_track_t & tk );
block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block );
#endif
bool lzo1x_decompress_extra( demux_t * p_demux, mkv_track_t & tk );
block_t *block_lzo1x_decompress( vlc_object_t *p_this, block_t *p_in_block );

block_t *MemToBlock( uint8_t *p_mem, size_t i_mem, size_t offset);
void handle_real_audio(demux_t * p_demux, mkv_track_t * p_tk, block_t * p_blk, vlc_tick_t i_pts);
block_t *WEBVTT_Repack_Sample(block_t *p_block, bool b_webm = false,
                              const uint8_t * = NULL, size_t = 0);
void send_Block( demux_t * p_demux, mkv_track_t * p_tk, block_t * p_block, unsigned int i_number_frames, int64_t i_duration );
int UpdatePCR( demux_t * p_demux );

class ByteReader
{
public:
    ByteReader(const uint8_t *reader_, size_t reader_len_):
        reader(reader_), reader_left(reader_len_) {}

    bool skip(size_t bytes) {
        if (error) return false;
        if (reader_left < bytes) {
            reader_left = 0;
            error = true;
            return false;
        }
        reader_left -= bytes;
        reader += bytes;
        return true;
    }

    uint16_t GetBE16() {
        if (error) return 0;
        if (reader_left < 2) {
            reader_left = 0;
            error = true;
            return 0;
        }
        uint16_t v = GetWBE(reader);
        reader_left -= 2;
        reader += 2;
        return v;
    }

    uint32_t GetBE32() {
        if (error) return 0;
        if (reader_left < 4) {
            reader_left = 0;
            error = true;
            return 0;
        }
        uint16_t v = GetDWBE(reader);
        reader_left -= 4;
        reader += 4;
        return v;
    }

    bool hasErrors() const { return error; }

private:
    const uint8_t *reader;
    size_t        reader_left;
    bool          error = false;
};

#define SIZEOF_REALAUDIO_PRIVATE  (4+2+2+12+2+2+4+(3*4)+2+2+2+2)

class Cook_PrivateTrackData : public PrivateTrackData
{
public:
    Cook_PrivateTrackData(const uint8_t *reader, size_t reader_len):
        bytes(reader, reader_len) {}
    ~Cook_PrivateTrackData();
    int32_t Init();

    uint32_t coded_frame_size;
    uint16_t i_sub_packet_h;
    uint16_t i_frame_size;
    uint16_t i_subpacket_size;

    uint16_t i_rate;
    uint16_t i_bitspersample;
    uint16_t i_channels;

    std::vector<block_t *> p_subpackets;
    size_t   i_subpacket = 0;
private:
    ByteReader bytes;
};

block_t * packetize_wavpack( const mkv_track_t &, uint8_t *, size_t);

/* helper functions to print the mkv parse tree */
void MkvTree_va( demux_t& demuxer, int i_level, const char* fmt, va_list args);
void MkvTree( demux_t & demuxer, int i_level, const char *psz_format, ... );

} // namespace
