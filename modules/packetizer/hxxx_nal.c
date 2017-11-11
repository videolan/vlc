/*****************************************************************************
 * Copyright © 2015 VideoLAN Authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "hxxx_nal.h"

#include <vlc_block.h>

static bool block_WillRealloc( block_t *p_block, ssize_t i_prebody, size_t i_body )
{
    if( i_prebody <= 0 && i_body <= (size_t)(-i_prebody) )
        return false;
    else
        return ( i_prebody + i_body <= p_block->i_size );
}

static inline void hxxx_WritePrefix( uint8_t i_nal_length_size, uint8_t *p_dest, uint32_t i_payload )
{
    if( i_nal_length_size == 4 )
        SetDWBE( p_dest, i_payload );
    else if( i_nal_length_size == 2 )
        SetWBE( p_dest, i_payload );
    else
        *p_dest = i_payload;
}

block_t *hxxx_AnnexB_to_xVC( block_t *p_block, uint8_t i_nal_length_size )
{
    unsigned i_nalcount = 0;
    unsigned i_list = 16;
    struct nalmoves_e
    {
        const uint8_t *p; /* start of prefixed nal */
        uint8_t  prefix; /* startcode length */
        off_t    move; /* move offset */
    } *p_list = NULL;

    if(!p_block->i_buffer || p_block->p_buffer[0])
        goto error;

    if(! (p_list = vlc_alloc( i_list, sizeof(*p_list) )) )
        goto error;

    /* Search all startcode of size 3 */
    const uint8_t *p_buf = p_block->p_buffer;
    const uint8_t *p_end = &p_block->p_buffer[p_block->i_buffer];
    unsigned i_bitflow = 0;
    off_t i_move = 0;
    while( p_buf != p_end )
    {
        i_bitflow <<= 1;
        if( !*p_buf )
        {
            i_bitflow |= 1;
        }
        else if( *p_buf == 0x01 && (i_bitflow & 0x06) == 0x06 ) /* >= two zero prefixed 1 */
        {
            if( i_bitflow & 0x08 ) /* three zero prefixed 1 */
            {
                p_list[i_nalcount].p = &p_buf[-3];
                p_list[i_nalcount].prefix = 4;
            }
            else /* two zero prefixed 1 */
            {
                p_list[i_nalcount].p = &p_buf[-2];
                p_list[i_nalcount].prefix = 3;
            }
            i_move += (off_t) i_nal_length_size - p_list[i_nalcount].prefix;
            p_list[i_nalcount++].move = i_move;

            /* Check and realloc our list */
            if(i_nalcount == i_list)
            {
                i_list += 16;
                struct nalmoves_e *p_new = realloc( p_list, sizeof(*p_new) * i_list );
                if(unlikely(!p_new))
                    goto error;
                p_list = p_new;
            }
        }
        p_buf++;
    }

    if( !i_nalcount )
        goto error;

    /* Optimization for 1 NAL block only case */
    if( i_nalcount == 1 && block_WillRealloc( p_block, p_list[0].move, p_block->i_buffer ) )
    {
        uint32_t i_payload = p_block->i_buffer - p_list[0].prefix;
        block_t *p_newblock = block_Realloc( p_block, p_list[0].move, p_block->i_buffer );
        if( unlikely(!p_newblock) )
            goto error;
        p_block = p_newblock;
        hxxx_WritePrefix( i_nal_length_size, p_block->p_buffer , i_payload );
        free( p_list );
        return p_block;
    }

    block_t *p_release = NULL;
    const uint8_t *p_source = NULL;
    const uint8_t *p_sourceend = NULL;
    uint8_t *p_dest = NULL;
    const size_t i_dest = p_block->i_buffer + p_list[i_nalcount - 1].move;

    if( p_list[i_nalcount - 1].move != 0 || i_nal_length_size != 4 )  /* We'll need to grow or shrink */
    {
        /* If we grow in size, try using realloc to avoid memcpy */
        if( p_list[i_nalcount - 1].move > 0 && block_WillRealloc( p_block, 0, i_dest ) )
        {
            uint32_t i_sizebackup = p_block->i_buffer;
            block_t *p_newblock = block_Realloc( p_block, 0, i_dest );
            if( unlikely(!p_newblock) )
                goto error;

            p_block = p_newblock;
            p_sourceend = &p_block->p_buffer[i_sizebackup];
            p_source = p_dest = p_block->p_buffer;
        }
        else
        {
            block_t *p_newblock = block_Alloc( i_dest );
            if( unlikely(!p_newblock) )
                goto error;

            p_release = p_block; /* Will be released after use */
            p_source = p_release->p_buffer;
            p_sourceend = &p_release->p_buffer[p_release->i_buffer];

            p_block = p_newblock;
            p_dest = p_newblock->p_buffer;
        }
    }
    else
    {
        p_source = p_dest = p_block->p_buffer;
        p_sourceend = &p_block->p_buffer[p_block->i_buffer];
    }

    if(!p_dest)
        goto error;

    /* Do reverse order moves, so we never overlap when growing only */
    for( unsigned i=i_nalcount; i!=0; i-- )
    {
        const uint8_t *p_readstart = p_list[i - 1].p;
        uint32_t i_payload = p_sourceend - p_readstart - p_list[i - 1].prefix;
        off_t offset = p_list[i - 1].p - p_source + p_list[i - 1].prefix + p_list[i - 1].move;
//        printf(" move offset %ld, length = %ld  prefix %ld move %ld\n", p_readstart - p_source, i_payload, p_list[i - 1].prefix, p_list[i-1].move);

        /* move in same / copy between buffers */
        memmove( &p_dest[ offset ], &p_list[i - 1].p[ p_list[i - 1].prefix ], i_payload );

        hxxx_WritePrefix( i_nal_length_size, &p_dest[ offset - i_nal_length_size ] , i_payload );

        p_sourceend = p_readstart;
    }

    if( p_release )
        block_Release( p_release );
    free( p_list );
    return p_block;

error:
    free( p_list );
    block_Release( p_block );
    return NULL;
}
