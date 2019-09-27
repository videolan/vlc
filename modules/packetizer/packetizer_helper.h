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

enum
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
    int i_state;
    block_bytestream_t bytestream;
    size_t i_offset;

    int i_startcode;
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
                                    const uint8_t *p_startcode, int i_startcode,
                                    block_startcode_helper_t pf_start_helper,
                                    const uint8_t *p_au_prepend, int i_au_prepend,
                                    unsigned i_au_min_size,
                                    packetizer_reset_t pf_reset,
                                    packetizer_parse_t pf_parse,
                                    packetizer_validate_t pf_validate,
                                    packetizer_drain_t pf_drain,
                                    void *p_private )
{
    p_pack->i_state = STATE_NOSYNC;
    block_BytestreamInit( &p_pack->bytestream );
    p_pack->i_offset = 0;

    p_pack->i_au_prepend = i_au_prepend;
    p_pack->p_au_prepend = p_au_prepend;
    p_pack->i_au_min_size = i_au_min_size;

    p_pack->i_startcode = i_startcode;
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
    p_pack->i_state = STATE_NOSYNC;
    block_BytestreamEmpty( &p_pack->bytestream );
    p_pack->i_offset = 0;
    p_pack->pf_reset( p_pack->p_private, true );
}

static block_t *packetizer_PacketizeBlock( packetizer_t *p_pack, block_t **pp_block )
{
    block_t *p_block = ( pp_block ) ? *pp_block : NULL;

    if( p_block == NULL && p_pack->bytestream.p_block == NULL )
        return NULL;

    if( p_block && unlikely( p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) )
    {
        block_t *p_drained = packetizer_PacketizeBlock( p_pack, NULL );
        if( p_drained )
            return p_drained;

        p_pack->i_state = STATE_NOSYNC;
        block_BytestreamEmpty( &p_pack->bytestream );
        p_pack->i_offset = 0;
        p_pack->pf_reset( p_pack->p_private, false );
    }

    if( p_block )
        block_BytestreamPush( &p_pack->bytestream, p_block );

    for( ;; )
    {
        bool b_used_ts;
        block_t *p_pic;

        switch( p_pack->i_state )
        {
        case STATE_NOSYNC:
            /* Find a startcode */
            if( !block_FindStartcodeFromOffset( &p_pack->bytestream, &p_pack->i_offset,
                                                p_pack->p_startcode, p_pack->i_startcode,
                                                p_pack->pf_startcode_helper, NULL ) )
                p_pack->i_state = STATE_NEXT_SYNC;

            if( p_pack->i_offset )
            {
                block_SkipBytes( &p_pack->bytestream, p_pack->i_offset );
                p_pack->i_offset = 0;
                block_BytestreamFlush( &p_pack->bytestream );
            }

            if( p_pack->i_state != STATE_NEXT_SYNC )
                return NULL; /* Need more data */

            p_pack->i_offset = 1; /* To find next startcode */
            /* fallthrough */

        case STATE_NEXT_SYNC:
            /* Find the next startcode */
            if( block_FindStartcodeFromOffset( &p_pack->bytestream, &p_pack->i_offset,
                                               p_pack->p_startcode, p_pack->i_startcode,
                                               p_pack->pf_startcode_helper, NULL ) )
            {
                if( pp_block /* not flushing */ || !p_pack->bytestream.p_chain )
                    return NULL; /* Need more data */

                /* When flusing and we don't find a startcode, suppose that
                 * the data extend up to the end */
                p_pack->i_offset = block_BytestreamRemaining(&p_pack->bytestream);
                if( p_pack->i_offset == 0 )
                    return NULL;

                if( p_pack->i_offset <= (size_t)p_pack->i_startcode &&
                    (p_pack->bytestream.p_block->i_flags & BLOCK_FLAG_AU_END) == 0 )
                    return NULL;
            }

            block_BytestreamFlush( &p_pack->bytestream );

            /* Get the new fragment and set the pts/dts */
            block_t *p_block_bytestream = p_pack->bytestream.p_block;

            p_pic = block_Alloc( p_pack->i_offset + p_pack->i_au_prepend );
            p_pic->i_pts = p_block_bytestream->i_pts;
            p_pic->i_dts = p_block_bytestream->i_dts;

            /* Do not wait for next sync code if notified block ends AU */
            if( (p_block_bytestream->i_flags & BLOCK_FLAG_AU_END) &&
                 p_block_bytestream->i_buffer == p_pack->i_offset )
            {
                p_pic->i_flags |= BLOCK_FLAG_AU_END;
            }

            block_GetBytes( &p_pack->bytestream, &p_pic->p_buffer[p_pack->i_au_prepend],
                            p_pic->i_buffer - p_pack->i_au_prepend );
            if( p_pack->i_au_prepend > 0 )
                memcpy( p_pic->p_buffer, p_pack->p_au_prepend, p_pack->i_au_prepend );

            p_pack->i_offset = 0;

            /* Parse the NAL */
            if( p_pic->i_buffer < p_pack->i_au_min_size )
            {
                block_Release( p_pic );
                p_pic = NULL;
            }
            else
            {
                p_pic = p_pack->pf_parse( p_pack->p_private, &b_used_ts, p_pic );
                if( b_used_ts )
                {
                    p_block_bytestream->i_dts = VLC_TICK_INVALID;
                    p_block_bytestream->i_pts = VLC_TICK_INVALID;
                }
            }

            if( !p_pic )
            {
                p_pack->i_state = STATE_NOSYNC;
                break;
            }
            if( p_pack->pf_validate( p_pack->p_private, p_pic ) )
            {
                p_pack->i_state = STATE_NOSYNC;
                block_Release( p_pic );
                break;
            }

            /* So p_block doesn't get re-added several times */
            if( pp_block )
                *pp_block = block_BytestreamPop( &p_pack->bytestream );

            p_pack->i_state = STATE_NOSYNC;

            return p_pic;
        }
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

    p_pack->i_state = STATE_NOSYNC;
    block_BytestreamEmpty( &p_pack->bytestream );
    p_pack->i_offset = 0;
}

#endif

