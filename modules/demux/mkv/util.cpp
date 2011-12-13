/*****************************************************************************
 * util.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "util.hpp"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#ifdef HAVE_ZLIB_H
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
    p_block = block_New( p_this, 0 );
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
            msg_Dbg( p_this, "Zlib decompression failed. Result: %d", result );
            return NULL;
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


