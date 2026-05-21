/*****************************************************************************
 * packetizer_helper.h: Packetizer helpers
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_PACKETIZER_HELPER_H_
#define VLC_PACKETIZER_HELPER_H_

#include <vlc_block.h>

enum vlc_packetizer_state
{
    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA,
    STATE_CUSTOM_FIRST,
};

typedef void (*packetizer_reset_t)( void *p_private, bool b_flush );
typedef block_t *(*packetizer_parse_t)( void *p_private, bool *pb_ts_used, block_t * );
typedef block_t *(*packetizer_drain_t)( void *p_private );
typedef int (*packetizer_validate_t)( void *p_private, block_t * );

typedef struct
{
    block_bytestream_t bytestream;
    bool b_synched;

    size_t startcode_len;
    const uint8_t *p_startcode;
    block_startcode_helper_t pf_startcode_helper;

    int i_au_prepend;
    const uint8_t *p_au_prepend;

    unsigned i_au_min_size;

    void *p_private;
    packetizer_reset_t    pf_reset;
    packetizer_parse_t    pf_parse;
    packetizer_validate_t pf_validate;
    packetizer_drain_t    pf_drain;

} packetizer_t;

static inline void packetizer_Init( packetizer_t *p_pack,
                                    const uint8_t *p_startcode, size_t i_startcode,
                                    block_startcode_helper_t pf_start_helper,
                                    const uint8_t *p_au_prepend, int i_au_prepend,
                                    unsigned i_au_min_size,
                                    packetizer_reset_t pf_reset,
                                    packetizer_parse_t pf_parse,
                                    packetizer_validate_t pf_validate,
                                    packetizer_drain_t pf_drain,
                                    void *p_private )
{
    p_pack->b_synched = false;
    block_BytestreamInit( &p_pack->bytestream );

    p_pack->i_au_prepend = i_au_prepend;
    p_pack->p_au_prepend = p_au_prepend;
    p_pack->i_au_min_size = i_au_min_size;

    p_pack->startcode_len = i_startcode;
    p_pack->p_startcode = p_startcode;
    p_pack->pf_startcode_helper = pf_start_helper;
    p_pack->pf_reset = pf_reset;
    p_pack->pf_parse = pf_parse;
    p_pack->pf_validate = pf_validate;
    p_pack->pf_drain = pf_drain;
    p_pack->p_private = p_private;
}

static inline void packetizer_Clean( packetizer_t *p_pack )
{
    block_BytestreamRelease( &p_pack->bytestream );
}

static inline void packetizer_Flush( packetizer_t *p_pack )
{
    p_pack->b_synched = false;
    block_BytestreamEmpty( &p_pack->bytestream );
    p_pack->pf_reset( p_pack->p_private, true );
}

/**
 * Return a block of the data between 2 startcodes in the block stream.
 * The block contains the startcode at the beginning.
 */
static block_t *packetizer_PacketizeBlock( packetizer_t *p_pack, block_t **pp_block )
{
    block_t *p_block = ( pp_block ) ? *pp_block : NULL;

    if( p_block == NULL && p_pack->bytestream.p_block == NULL )
        return NULL;

    if( p_block && unlikely( p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) )
    {
        block_t *p_drained = packetizer_PacketizeBlock( p_pack, NULL );
        if( p_drained )
            return p_drained; // FIXME do we lose p_block ? Can we output more than one drain output ?

        p_pack->b_synched = false;
        block_BytestreamEmpty( &p_pack->bytestream );
        p_pack->pf_reset( p_pack->p_private, false );
    }

    if( p_block )
        block_BytestreamPush( &p_pack->bytestream, p_block );

    for( ;; )
    {
        bool b_used_ts;
        block_t *p_pic;

        if( !p_pack->b_synched )
        {
            size_t block_startcode_offset = 0;
            /* Find a startcode */
            if( block_FindStartcodeFromOffset( &p_pack->bytestream, &block_startcode_offset,
                                                p_pack->p_startcode, p_pack->startcode_len,
                                                p_pack->pf_startcode_helper, NULL ) )
            {
                return NULL; /* Need more data */
            }

            p_pack->b_synched = true;

            if( block_startcode_offset )
            {
                // remove junk data before the startcode
                block_SkipBytes( &p_pack->bytestream, block_startcode_offset );
                block_BytestreamFlush( &p_pack->bytestream );
            }
        }

        size_t block_size = p_pack->startcode_len;
        /* Find the next startcode */
        if( block_FindStartcodeFromOffset( &p_pack->bytestream, &block_size,
                                           p_pack->p_startcode, p_pack->startcode_len,
                                           p_pack->pf_startcode_helper, NULL ) )
        {
            if( pp_block /* not draining */ || !p_pack->bytestream.p_chain )
            {
                return NULL; /* Need more data */
            }

            /* When draining and we don't find a second startcode, suppose that
             * the data extend up to the end of the bytestream */
            block_size = block_BytestreamRemaining(&p_pack->bytestream);
            if( block_size == 0 )
                return NULL;

            if( block_size <= p_pack->startcode_len &&
                (p_pack->bytestream.p_block->i_flags & BLOCK_FLAG_AU_END) == 0 )
            {
                block_SkipBytes( &p_pack->bytestream, block_size );
                return NULL;
            }
        }

        block_BytestreamFlush( &p_pack->bytestream );
        p_pack->b_synched = false; // look for 2 startcodes on next call/loop

        if ( block_size + p_pack->i_au_prepend < p_pack->i_au_min_size )
        {
            // we found 2 startcodes but the amount of data between them is too
            // small, discard data until the second startcode
            block_SkipBytes( &p_pack->bytestream, block_size );
            return NULL;
        }

        /* Get the new fragment and set the pts/dts */
        block_t *p_block_bytestream = p_pack->bytestream.p_block;

        p_pic = block_Alloc( block_size + p_pack->i_au_prepend );
        if( p_pic == NULL )
        {
            // we can't output this block, maybe it's too big, we should not try
            // to read it will we are fed more data
            block_SkipBytes( &p_pack->bytestream, block_size );
            return NULL;
        }
        p_pic->i_pts = p_block_bytestream->i_pts;
        p_pic->i_dts = p_block_bytestream->i_dts;

        /* Do not wait for next sync code if notified block ends AU */
        if( (p_block_bytestream->i_flags & BLOCK_FLAG_AU_END) &&
             p_block_bytestream->i_buffer == block_size )
        {
            p_pic->i_flags |= BLOCK_FLAG_AU_END;
        }

        block_GetBytes( &p_pack->bytestream, &p_pic->p_buffer[p_pack->i_au_prepend],
                        p_pic->i_buffer - p_pack->i_au_prepend );
        if( p_pack->i_au_prepend > 0 )
            memcpy( p_pic->p_buffer, p_pack->p_au_prepend, p_pack->i_au_prepend );

        /* Parse the NAL */
        p_pic = p_pack->pf_parse( p_pack->p_private, &b_used_ts, p_pic );
        if( b_used_ts )
        {
            p_block_bytestream->i_dts = VLC_TICK_INVALID;
            p_block_bytestream->i_pts = VLC_TICK_INVALID;
        }

        if( !p_pic )
        {
            continue;
        }
        if( p_pack->pf_validate( p_pack->p_private, p_pic ) )
        {
            block_Release( p_pic );
            continue;
        }

        /* So p_block doesn't get re-added several times */
        if( pp_block )
            *pp_block = block_BytestreamPop( &p_pack->bytestream );

        return p_pic;
    }
}

static block_t *packetizer_Packetize( packetizer_t *p_pack, block_t **pp_block )
{
    block_t *p_out = packetizer_PacketizeBlock( p_pack, pp_block );
    if( p_out )
        return p_out;
    /* handle caller drain */
    if( pp_block == NULL && p_pack->pf_drain )
    {
        p_out = p_pack->pf_drain( p_pack->p_private );
        if( p_out && p_pack->pf_validate( p_pack->p_private, p_out ) )
        {
            block_Release( p_out );
            p_out = NULL;
        }
    }
    return p_out;
}

static inline void packetizer_Header( packetizer_t *p_pack,
                                      const uint8_t *p_header, int i_header )
{
    block_t *p_init = block_Alloc( i_header );
    if( !p_init )
        return;

    memcpy( p_init->p_buffer, p_header, i_header );

    block_t *p_pic;
    while( ( p_pic = packetizer_Packetize( p_pack, &p_init ) ) )
        block_Release( p_pic ); /* Should not happen (only sequence header) */
    while( ( p_pic = packetizer_Packetize( p_pack, NULL ) ) )
        block_Release( p_pic );

    p_pack->b_synched = false;
    block_BytestreamEmpty( &p_pack->bytestream );
}

#endif
