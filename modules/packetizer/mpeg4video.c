/*****************************************************************************
 * mpeg4video.c: mpeg 4 video packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpeg4video.c,v 1.15 2003/11/17 18:48:08 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "codecs.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Common properties
     */
    mtime_t i_pts;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenPacketizer ( vlc_object_t * );
static void ClosePacketizer( vlc_object_t * );

static block_t *PacketizeBlock( decoder_t *, block_t ** );

static int m4v_FindVol( decoder_t *p_dec, block_t *p_block );

#define VIDEO_OBJECT_MASK                       0x01f
#define VIDEO_OBJECT_LAYER_MASK                 0x00f

#define VIDEO_OBJECT_START_CODE                 0x100
#define VIDEO_OBJECT_LAYER_START_CODE           0x120
#define VISUAL_OBJECT_SEQUENCE_START_CODE       0x1b0
#define VISUAL_OBJECT_SEQUENCE_END_CODE         0x1b1
#define USER_DATA_START_CODE                    0x1b2
#define GROUP_OF_VOP_START_CODE                 0x1b3
#define VIDEO_SESSION_ERROR_CODE                0x1b4
#define VISUAL_OBJECT_START_CODE                0x1b5
#define VOP_START_CODE                          0x1b6
#define FACE_OBJECT_START_CODE                  0x1ba
#define FACE_OBJECT_PLANE_START_CODE            0x1bb
#define MESH_OBJECT_START_CODE                  0x1bc
#define MESH_OBJECT_PLANE_START_CODE            0x1bd
#define STILL_TEXTURE_OBJECT_START_CODE         0x1be
#define TEXTURE_SPATIAL_LAYER_START_CODE        0x1bf
#define TEXTURE_SNR_LAYER_START_CODE            0x1c0

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG4 Video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( OpenPacketizer, ClosePacketizer );
vlc_module_end();

/*****************************************************************************
 * OpenPacketizer: probe the packetizer and return score
 *****************************************************************************/
static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->p_fifo->i_fourcc )
    {
        case VLC_FOURCC( 'm', '4', 's', '2'):
        case VLC_FOURCC( 'M', '4', 'S', '2'):
        case VLC_FOURCC( 'm', 'p', '4', 's'):
        case VLC_FOURCC( 'M', 'P', '4', 'S'):
        case VLC_FOURCC( 'm', 'p', '4', 'v'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
        case VLC_FOURCC( 0x04, 0,   0,   0):
        case VLC_FOURCC( '3', 'I', 'V', '2'):
            break;

        default:
            return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Setup properties */
    p_dec->fmt_out = p_dec->fmt_in;
    p_dec->fmt_out.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

    if( p_dec->fmt_in.i_extra )
    {
        /* We have a vol */
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = malloc( p_dec->fmt_in.i_extra );
        memcpy( p_dec->fmt_out.p_extra, p_dec->fmt_in.p_extra,
                p_dec->fmt_in.i_extra );

        msg_Dbg( p_dec, "opening with vol size:%d", p_dec->fmt_in.i_extra );
    }
    else
    {
        /* No vol, we'll have to look for one later on */
        p_dec->fmt_out.i_extra = 0;
        p_dec->fmt_out.p_extra = 0;
    }

    /* Set callback */
    p_dec->pf_packetize = PacketizeBlock;

    return VLC_SUCCESS;
}

/****************************************************************************
 * PacketizeBlock: the whole thing
 ****************************************************************************/
static block_t *PacketizeBlock( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    if( !p_dec->fmt_out.i_extra )
    {
        m4v_FindVol( p_dec, p_block );
    }

    /* Drop blocks until we have a VOL */
    if( !p_dec->fmt_out.i_extra )
    {
        block_Release( p_block );
        return NULL;
    }

    /* TODO: Date management */
    p_block->i_length = 1000000 / 25;

    *pp_block = NULL;
    return p_block;
}

/****************************************************************************
 * m4v_FindStartCode
 ****************************************************************************/
static int m4v_FindStartCode( uint8_t **pp_data, uint8_t *p_end )
{
    for( ; *pp_data < p_end - 4; (*pp_data)++ )
    {
        if( (*pp_data)[0] == 0 && (*pp_data)[1] == 0 && (*pp_data)[2] == 1 )
        {
            return 0;
        }
    }
    return -1;
}

static int m4v_FindVol( decoder_t *p_dec, block_t *p_block )
{
    uint8_t *p_vol_begin, *p_vol_end, *p_end;

    /* search if p_block contains with a vol */
    p_vol_begin = p_block->p_buffer;
    p_vol_end   = NULL;
    p_end       = p_block->p_buffer + p_block->i_buffer;

    for( ;; )
    {
        if( m4v_FindStartCode( &p_vol_begin, p_end ) )
        {
            break;
        }

        msg_Dbg( p_dec, "starcode 0x%2.2x%2.2x%2.2x%2.2x",
                 p_vol_begin[0], p_vol_begin[1],
                 p_vol_begin[2], p_vol_begin[3] );

        if( ( p_vol_begin[3] & ~VIDEO_OBJECT_MASK ) ==
            ( VIDEO_OBJECT_START_CODE&0xff ) )
        {
            p_vol_end = p_vol_begin + 4;
            if( m4v_FindStartCode( &p_vol_end, p_end ) )
            {
                p_vol_begin++;
                continue;
            }
            if( ( p_vol_end[3] & ~VIDEO_OBJECT_LAYER_MASK ) ==
                ( VIDEO_OBJECT_LAYER_START_CODE&0xff ) )
            {
                p_vol_end += 4;
                if( m4v_FindStartCode( &p_vol_end, p_end ) )
                {
                    p_vol_end = p_end;
                }
            }
            else
            {
                p_vol_begin++;
                continue;
            }
        }
        else if( ( p_vol_begin[3] & ~VIDEO_OBJECT_LAYER_MASK ) ==
                 ( VIDEO_OBJECT_LAYER_START_CODE&0xff) )
        {
            p_vol_end = p_vol_begin + 4;
            if( m4v_FindStartCode( &p_vol_end, p_end ) )
            {
                p_vol_end = p_end;
            }
        }

        if( p_vol_end != NULL && p_vol_begin < p_vol_end )
        {
            p_dec->fmt_out.i_extra = p_vol_end - p_vol_begin;
            msg_Dbg( p_dec, "Found VOL" );

            p_dec->fmt_out.p_extra = malloc( p_dec->fmt_out.i_extra );
            memcpy( p_dec->fmt_out.p_extra, p_vol_begin,
                    p_dec->fmt_out.i_extra );
            return VLC_SUCCESS;
        }
        else
        {
            p_vol_begin++;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ClosePacketizer: clean up the packetizer
 *****************************************************************************/
static void ClosePacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    free( p_dec->p_sys );
}
