/*****************************************************************************
 * util.cpp : matroska demuxer
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
#include "util.hpp"
#include "demux.hpp"

#include <stdint.h>
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#ifdef HAVE_ZLIB_H
int32_t zlib_decompress_extra( demux_t * p_demux, mkv_track_t * tk )
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
        msg_Err( p_demux, "Couldn't initiate inflation ignore track %d",
                 tk->i_number );
        free(tk->p_extra_data);
        delete tk;
        return 1;
    }

    d_stream.next_in = tk->p_extra_data;
    d_stream.avail_in = tk->i_extra_data;
    do
    {
        n++;
        p_new_extra = (uint8_t *) realloc(p_new_extra, n*1024);
        if( !p_new_extra )
        {
            msg_Err( p_demux, "Couldn't allocate buffer to inflate data, ignore track %d",
                      tk->i_number );
            inflateEnd( &d_stream );
            free(tk->p_extra_data);
            delete tk;
            return 1;
        }
        d_stream.next_out = &p_new_extra[(n - 1) * 1024];
        d_stream.avail_out = 1024;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( result != Z_OK && result != Z_STREAM_END )
        {
            msg_Err( p_demux, "Zlib decompression failed. Result: %d", result );
            inflateEnd( &d_stream );
            free(p_new_extra);
            free(tk->p_extra_data);
            delete tk;
            return 1;
        }
    }
    while ( d_stream.avail_out == 0 && d_stream.avail_in != 0  &&
            result != Z_STREAM_END );

    free( tk->p_extra_data );
    tk->i_extra_data = d_stream.total_out;
    p_new_extra = (uint8_t *) realloc(p_new_extra, tk->i_extra_data);
    if( !p_new_extra )
    {
        msg_Err( p_demux, "Couldn't allocate buffer to inflate data, ignore track %d",
                 tk->i_number );
        inflateEnd( &d_stream );
        free(p_new_extra);
        delete tk;
        return 1;
    }

    tk->p_extra_data = p_new_extra;
    
    inflateEnd( &d_stream );
    return 0;
}

block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block ) {
    int result, dstsize, n;
    unsigned char *dst;
    block_t *p_block;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
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
        dst = (unsigned char *)p_block->p_buffer;
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


void handle_real_audio(demux_t * p_demux, mkv_track_t * p_tk, block_t * p_blk, mtime_t i_pts)
{
    uint8_t * p_frame = p_blk->p_buffer;
    Cook_PrivateTrackData * p_sys = (Cook_PrivateTrackData *) p_tk->p_sys;
    size_t size = p_blk->i_buffer;

    if( p_tk->i_last_dts == VLC_TS_INVALID )
    {
        for( size_t i = 0; i < p_sys->i_subpackets; i++)
            if( p_sys->p_subpackets[i] )
            {
                block_Release(p_sys->p_subpackets[i]);
                p_sys->p_subpackets[i] = NULL;
            }
        p_sys->i_subpacket = 0;
    }

    if( p_tk->fmt.i_codec == VLC_CODEC_COOK ||
        p_tk->fmt.i_codec == VLC_CODEC_ATRAC3 )
    {
        const uint32_t i_num = p_sys->i_frame_size / p_sys->i_subpacket_size;
        const int y = p_sys->i_subpacket / ( p_sys->i_frame_size / p_sys->i_subpacket_size );

        for( int i = 0; i < i_num; i++ )
        {
            int i_index = p_sys->i_sub_packet_h * i +
                          ((p_sys->i_sub_packet_h + 1) / 2) * (y&1) + (y>>1);
            if( i_index >= p_sys->i_subpackets )
                return;

            block_t *p_block = block_Alloc( p_sys->i_subpacket_size );
            if( !p_block )
                return;

            if( size < p_sys->i_subpacket_size )
                return;

            memcpy( p_block->p_buffer, p_frame, p_sys->i_subpacket_size );
            p_block->i_dts = VLC_TS_INVALID;
            p_block->i_pts = VLC_TS_INVALID;
            if( !p_sys->i_subpacket )
            {
                p_tk->i_last_dts = 
                p_block->i_pts = i_pts + VLC_TS_0;
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
            es_out_Send( p_demux->out, p_tk->p_es,  p_sys->p_subpackets[i]);
            p_sys->p_subpackets[i] = NULL;
        }
        p_sys->i_subpacket = 0;
    }
}

int32_t Cook_PrivateTrackData::Init()
{
    i_subpackets = (size_t) i_sub_packet_h * (size_t) i_frame_size / (size_t) i_subpacket_size;
    p_subpackets = (block_t**) calloc(i_subpackets, sizeof(block_t*));

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

block_t * packetize_wavpack( mkv_track_t * p_tk, uint8_t * buffer, size_t  size)
{
    uint16_t version = 0x403;
    uint32_t block_samples;
    uint32_t flags;
    uint32_t crc;
    block_t * p_block = NULL;
    
    if( p_tk->i_extra_data >= 2 )
        version = GetWLE( p_tk->p_extra_data );

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
