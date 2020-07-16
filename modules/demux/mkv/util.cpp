/*****************************************************************************
 * util.cpp : matroska demuxer
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
#include "util.hpp"
#include "demux.hpp"
#include "../../codec/webvtt/helpers.h"

namespace mkv {

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#ifdef HAVE_ZLIB_H
int32_t zlib_decompress_extra( demux_t * p_demux, mkv_track_t & tk )
{
    int result;
    z_stream d_stream;
    size_t n = 0;
    uint8_t * p_new_extra = NULL;

    msg_Dbg(p_demux,"Inflating private data");

    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    if( inflateInit( &d_stream ) != Z_OK )
    {
        msg_Err( p_demux, "Couldn't initiate inflation ignore track %u",
                 tk.i_number );
        return 1;
    }

    d_stream.next_in = tk.p_extra_data;
    d_stream.avail_in = tk.i_extra_data;
    do
    {
        n++;
        void *alloc = realloc(p_new_extra, n*1024);
        if( alloc == NULL )
        {
            msg_Err( p_demux, "Couldn't allocate buffer to inflate data, ignore track %u",
                      tk.i_number );
            free(p_new_extra);
            inflateEnd( &d_stream );
            return 1;
        }

        p_new_extra = static_cast<uint8_t *>( alloc );
        d_stream.next_out = &p_new_extra[(n - 1) * 1024];
        d_stream.avail_out = 1024;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( result != Z_OK && result != Z_STREAM_END )
        {
            msg_Err( p_demux, "Zlib decompression failed. Result: %d", result );
            inflateEnd( &d_stream );
            free(p_new_extra);
            return 1;
        }
    }
    while ( d_stream.avail_out == 0 && d_stream.avail_in != 0  &&
            result != Z_STREAM_END );

    free( tk.p_extra_data );
    tk.i_extra_data = d_stream.total_out;
    p_new_extra = static_cast<uint8_t *>( realloc(p_new_extra, tk.i_extra_data) );
    if( !p_new_extra )
    {
        msg_Err( p_demux, "Couldn't allocate buffer to inflate data, ignore track %u",
                 tk.i_number );
        inflateEnd( &d_stream );
        tk.p_extra_data = NULL;
        return 1;
    }

    tk.p_extra_data = p_new_extra;

    inflateEnd( &d_stream );
    return 0;
}

block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block ) {
    int result, dstsize, n;
    unsigned char *dst;
    block_t *p_block;
    z_stream d_stream;

    d_stream.zalloc = NULL;
    d_stream.zfree = NULL;
    d_stream.opaque = NULL;
    result = inflateInit(&d_stream);
    if( result != Z_OK )
    {
        msg_Dbg( p_this, "inflateInit() failed. Result: %d", result );
        return NULL;
    }

    d_stream.next_in = (Bytef *)p_in_block->p_buffer;
    d_stream.avail_in = p_in_block->i_buffer;
    n = 0;
    p_block = block_Alloc( 0 );
    dst = NULL;
    do
    {
        n++;
        p_block = block_Realloc( p_block, 0, n * 1000 );
        dst = static_cast<unsigned char *>( p_block->p_buffer );
        d_stream.next_out = (Bytef *)&dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( result != Z_OK ) && ( result != Z_STREAM_END ) )
        {
            msg_Err( p_this, "Zlib decompression failed. Result: %d", result );
            inflateEnd( &d_stream );
            block_Release( p_block );
            return p_in_block;
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( result != Z_STREAM_END ) );

    dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    p_block = block_Realloc( p_block, 0, dstsize );
    p_block->i_buffer = dstsize;
    block_Release( p_in_block );

    return p_block;
}
#endif

/* Utility function for BlockDecode */
block_t *MemToBlock( uint8_t *p_mem, size_t i_mem, size_t offset)
{
    if( unlikely( i_mem > SIZE_MAX - offset ) )
        return NULL;

    block_t *p_block = block_Alloc( i_mem + offset );
    if( likely(p_block != NULL) )
    {
        memcpy( p_block->p_buffer + offset, p_mem, i_mem );
    }
    return p_block;
}


void handle_real_audio(demux_t * p_demux, mkv_track_t * p_tk, block_t * p_blk, vlc_tick_t i_pts)
{
    uint8_t * p_frame = p_blk->p_buffer;
    Cook_PrivateTrackData * p_sys = (Cook_PrivateTrackData *) p_tk->p_sys;
    size_t size = p_blk->i_buffer;

    if( p_tk->i_last_dts == VLC_TICK_INVALID )
    {
        for( size_t i = 0; i < p_sys->i_subpackets; i++)
            if( p_sys->p_subpackets[i] )
            {
                block_Release(p_sys->p_subpackets[i]);
                p_sys->p_subpackets[i] = NULL;
            }
        p_sys->i_subpacket = 0;
        p_sys->i_subpackets = 0;

        if ( !( p_blk->i_flags & BLOCK_FLAG_TYPE_I) )
        {
            msg_Dbg( p_demux, "discard non-key preroll block in track %u at %" PRId64,
                     p_tk->i_number, i_pts );
            return;
        }
    }

    if( p_tk->fmt.i_codec == VLC_CODEC_COOK ||
        p_tk->fmt.i_codec == VLC_CODEC_ATRAC3 )
    {
        const uint16_t i_num = p_sys->i_frame_size / p_sys->i_subpacket_size;
        if ( i_num == 0 )
            return;
        const size_t y = p_sys->i_subpacket / i_num;

        for( uint16_t i = 0; i < i_num; i++ )
        {
            size_t i_index = (size_t) p_sys->i_sub_packet_h * i +
                          ((p_sys->i_sub_packet_h + 1) / 2) * (y&1) + (y>>1);
            if( i_index >= p_sys->i_subpackets )
                return;

            block_t *p_block = block_Alloc( p_sys->i_subpacket_size );
            if( !p_block )
                return;

            if( size < p_sys->i_subpacket_size )
                return;

            memcpy( p_block->p_buffer, p_frame, p_sys->i_subpacket_size );
            p_block->i_dts = VLC_TICK_INVALID;
            p_block->i_pts = VLC_TICK_INVALID;
            if( !p_sys->i_subpacket )
            {
                p_tk->i_last_dts =
                p_block->i_pts = i_pts;
            }

            p_frame += p_sys->i_subpacket_size;
            size -=  p_sys->i_subpacket_size;

            p_sys->i_subpacket++;
            p_sys->p_subpackets[i_index] = p_block;
        }
    }
    else
    {
        /*TODO*/
    }
    if( p_sys->i_subpacket == p_sys->i_subpackets )
    {
        for( size_t i = 0; i < p_sys->i_subpackets; i++)
        {
            send_Block( p_demux, p_tk, p_sys->p_subpackets[i], 1, 0 );
            p_sys->p_subpackets[i] = NULL;
        }
        p_sys->i_subpacket = 0;
        p_sys->i_subpackets = 0;
    }
}

block_t *WEBVTT_Repack_Sample(block_t *p_block, bool b_webm,
                              const uint8_t *p_add, size_t i_add)
{
    struct webvtt_cueelements_s els;
    memset(&els, 0, sizeof(els));
    size_t newsize = 0;
    block_t *newblock = nullptr;
    /* Repack to ISOBMFF samples format */
    if( !b_webm ) /* S_TEXT/WEBVTT */
    {
        /* process addition fields */
        if( i_add )
        {
            const uint8_t *end = p_add + i_add;
            const uint8_t *iden =
                    reinterpret_cast<const uint8_t *>(std::memchr( p_add, '\n', i_add ));
            if( iden && ++iden != end )
            {
                els.sttg.p_data = p_add;
                els.sttg.i_data = &iden[-1] - p_add;
                const uint8_t *comm =
                        reinterpret_cast<const uint8_t *>(std::memchr( iden, '\n', end - iden ));
                els.iden.p_data = iden;
                if( comm )
                    els.iden.i_data = comm - iden;
                else
                    els.iden.i_data = end - iden;
            }
        }
        /* the payload being in the block */
        els.payl.p_data = p_block->p_buffer;
        els.payl.i_data = p_block->i_buffer;
    }
    else /* deprecated D_WEBVTT/ */
    {
        const uint8_t *start = p_block->p_buffer;
        const uint8_t *end = p_block->p_buffer + p_block->i_buffer;
        const uint8_t *sttg =
                reinterpret_cast<const uint8_t *>(std::memchr( start, '\n', p_block->i_buffer ));
        if( !sttg || ++sttg == end )
            goto error;
        const uint8_t *payl =
                reinterpret_cast<const uint8_t *>(std::memchr( sttg, '\n', end - sttg ));
        if( !payl || ++payl == end )
            goto error;
        els.iden.p_data = start;
        els.iden.i_data = &sttg[-1] - start;
        els.sttg.p_data = sttg;
        els.sttg.i_data = &payl[-1] - sttg;
        els.payl.p_data = payl;
        els.payl.i_data = end - payl;
    }

    newsize = WEBVTT_Pack_CueElementsGetNewSize( &els );
    newblock = block_Alloc( newsize );
    if( !newblock )
        goto error;
    WEBVTT_Pack_CueElements( &els, newblock->p_buffer );
    block_CopyProperties( newblock, p_block );
    block_Release( p_block );
    return newblock;

error:
    block_Release( p_block );
    return NULL;
}

void send_Block( demux_t * p_demux, mkv_track_t * p_tk, block_t * p_block, unsigned int i_number_frames, int64_t i_duration )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    matroska_segment_c *p_segment = p_sys->p_current_vsegment->CurrentSegment();

    if( p_tk->fmt.i_cat == AUDIO_ES && p_tk->i_chans_to_reorder )
    {
        aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                             p_tk->fmt.audio.i_channels,
                             p_tk->pi_chan_table, p_tk->fmt.i_codec );
    }

    if( p_block->i_dts != VLC_TICK_INVALID &&
        ( p_tk->fmt.i_cat == VIDEO_ES || p_tk->fmt.i_cat == AUDIO_ES ) )
    {
        p_tk->i_last_dts = p_block->i_dts;
    }

    if( !p_tk->b_no_duration )
    {
        p_block->i_length = VLC_TICK_FROM_NS(i_duration * p_tk->f_timecodescale *
                                             p_segment->i_timescale) / i_number_frames;
    }

    if( p_tk->b_discontinuity )
    {
        p_block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        p_tk->b_discontinuity = false;
    }

    es_out_Send( p_demux->out, p_tk->p_es, p_block);
}

int32_t Cook_PrivateTrackData::Init()
{
    i_subpackets = (size_t) i_sub_packet_h * (size_t) i_frame_size / (size_t) i_subpacket_size;
    p_subpackets = static_cast<block_t**> ( calloc(i_subpackets, sizeof(block_t*)) );

    if( unlikely( !p_subpackets ) )
    {
        i_subpackets = 0;
        return 1;
    }

    return 0;
}

Cook_PrivateTrackData::~Cook_PrivateTrackData()
{
    for( size_t i = 0; i < i_subpackets; i++ )
        if( p_subpackets[i] )
            block_Release( p_subpackets[i] );

    free( p_subpackets );
}

static inline void fill_wvpk_block(uint16_t version, uint32_t block_samples, uint32_t flags,
                                   uint32_t crc, uint8_t * src, size_t srclen, uint8_t * dst)
{
    const uint8_t wvpk_header[] = {'w','v','p','k',         /* ckId */
                                    0x0, 0x0, 0x0, 0x0,     /* ckSize */
                                    0x0, 0x0,               /* version */
                                    0x0,                    /* track_no */
                                    0x0,                    /* index_no */
                                    0xFF, 0xFF, 0xFF, 0xFF, /* total_samples */
                                    0x0, 0x0, 0x0, 0x0 };   /* block_index */
    memcpy( dst, wvpk_header, sizeof( wvpk_header ) );
    SetDWLE( dst + 4, srclen + 24 );
    SetWLE( dst + 8, version );
    SetDWLE( dst + 20, block_samples );
    SetDWLE( dst + 24, flags );
    SetDWLE( dst + 28, crc );
    memcpy( dst + 32, src, srclen );
}

block_t * packetize_wavpack( const mkv_track_t & tk, uint8_t * buffer, size_t  size)
{
    uint16_t version = 0x403;
    uint32_t block_samples;
    uint32_t flags;
    uint32_t crc;
    block_t * p_block = NULL;

    if( tk.i_extra_data >= 2 )
        version = GetWLE( tk.p_extra_data );

    if( size < 12 )
        return NULL;

    block_samples = GetDWLE(buffer);
    buffer += 4;
    flags = GetDWLE(buffer);
    size -= 4;

    /* Check if WV_INITIAL_BLOCK and WV_FINAL_BLOCK are present */
    if( ( flags & 0x1800 ) == 0x1800 )
    {
        crc = GetDWLE(buffer+4);
        buffer += 8;
        size -= 8;

        p_block = block_Alloc( size + 32 );
        if( !p_block )
            return NULL;

        fill_wvpk_block(version, block_samples, flags, crc, buffer, size, p_block->p_buffer);
    }
    else
    {
        /* Multiblock */
        size_t total_size = 0;

        p_block = block_Alloc( 0 );
        if( !p_block )
            return NULL;

        while(size >= 12)
        {
            flags = GetDWLE(buffer);
            buffer += 4;
            crc = GetDWLE(buffer);
            buffer += 4;
            uint32_t bsz = GetDWLE(buffer);
            buffer+= 4;
            size -= 12;

            bsz = (bsz < size)?bsz:size;

            total_size += bsz + 32;

            assert(total_size >= p_block->i_buffer);

            p_block = block_Realloc( p_block, 0, total_size );

            if( !p_block )
                return NULL;

            fill_wvpk_block(version, block_samples, flags, crc, buffer, bsz,
                            p_block->p_buffer + total_size - bsz - 32 );
            buffer += bsz;
            size -= bsz;
        }
    }

    return p_block;
}

void MkvTree_va( demux_t& demuxer, int i_level, const char* fmt, va_list args)
{
    static const char indent[] = "|   ";
    static const char prefix[] = "+ ";
    static int  const   indent_len = sizeof( indent ) - 1;
    static int  const   prefix_len = sizeof( prefix ) - 1;

    char   fixed_buffer[256] = {};
    size_t const  static_len = sizeof( fixed_buffer );
    char *            buffer = fixed_buffer;
    size_t         total_len = indent_len * i_level + prefix_len + strlen( fmt ) + 1;

    if( total_len >= static_len ) {
        buffer = new (std::nothrow) char[total_len] ();

        if (buffer == NULL) {
            msg_Err (&demuxer, "Unable to allocate memory for format string");
            return;
        }
    }

    char * dst = buffer;

    for (int i = 0; i < i_level; ++i, dst += indent_len)
        memcpy( dst, indent, indent_len );

    strcat( dst, prefix );
    strcat( dst, fmt );

    msg_GenericVa( &demuxer, VLC_MSG_DBG, buffer, args );

    if (buffer != fixed_buffer)
        delete [] buffer;
}

void MkvTree( demux_t & demuxer, int i_level, const char *psz_format, ... )
{
    va_list args; va_start( args, psz_format );
    MkvTree_va( demuxer, i_level, psz_format, args );
    va_end( args );
}

} // namespace
