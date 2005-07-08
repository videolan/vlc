/*****************************************************************************
 * copy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("Copy packetizer") );
    set_capability( "packetizer", 1 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct decoder_sys_t
{
    block_t *p_block;
};

static block_t *Packetize   ( decoder_t *, block_t ** );
static block_t *PacketizeSub( decoder_t *, block_t ** );

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_cat != AUDIO_ES &&
        p_dec->fmt_in.i_cat != VIDEO_ES &&
        p_dec->fmt_in.i_cat != SPU_ES )
    {
        msg_Err( p_dec, "invalid ES type" );
        return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.i_cat == SPU_ES )
        p_dec->pf_packetize = PacketizeSub;
    else
        p_dec->pf_packetize = Packetize;

    /* Create the output format */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    /* Fix the value of the fourcc */
    switch( p_dec->fmt_in.i_codec )
    {
        /* video */
        case VLC_FOURCC( 'm', '4', 's', '2'):
        case VLC_FOURCC( 'M', '4', 'S', '2'):
        case VLC_FOURCC( 'm', 'p', '4', 's'):
        case VLC_FOURCC( 'M', 'P', '4', 'S'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
        case VLC_FOURCC( 0x04, 0,   0,   0):
        case VLC_FOURCC( '3', 'I', 'V', '2'):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v');
            break;

        case VLC_FOURCC( 'm', 'p', 'g', '1' ):
        case VLC_FOURCC( 'm', 'p', 'g', '2' ):
        case VLC_FOURCC( 'm', 'p', '1', 'v' ):
        case VLC_FOURCC( 'm', 'p', '2', 'v' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
            break;

        case VLC_FOURCC( 'd', 'i', 'v', '1' ):
        case VLC_FOURCC( 'M', 'P', 'G', '4' ):
        case VLC_FOURCC( 'm', 'p', 'g', '4' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'D', 'I', 'V', '1' );
            break;

        case VLC_FOURCC( 'd', 'i', 'v', '2' ):
        case VLC_FOURCC( 'M', 'P', '4', '2' ):
        case VLC_FOURCC( 'm', 'p', '4', '2' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'D', 'I', 'V', '2' );
            break;

        case VLC_FOURCC( 'd', 'i', 'v', '3' ):
        case VLC_FOURCC( 'd', 'i', 'v', '4' ):
        case VLC_FOURCC( 'D', 'I', 'V', '4' ):
        case VLC_FOURCC( 'd', 'i', 'v', '5' ):
        case VLC_FOURCC( 'D', 'I', 'V', '5' ):
        case VLC_FOURCC( 'd', 'i', 'v', '6' ):
        case VLC_FOURCC( 'D', 'I', 'V', '6' ):
        case VLC_FOURCC( 'M', 'P', '4', '3' ):
        case VLC_FOURCC( 'm', 'p', '4', '3' ):
        case VLC_FOURCC( 'm', 'p', 'g', '3' ):
        case VLC_FOURCC( 'M', 'P', 'G', '3' ):
        case VLC_FOURCC( 'A', 'P', '4', '1' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'D', 'I', 'V', '3' );
            break;

        case VLC_FOURCC( 'h', '2', '6', '3' ):
        case VLC_FOURCC( 'U', '2', '6', '3' ):
        case VLC_FOURCC( 'u', '2', '6', '3' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'H', '2', '6', '3' );
            break;

        case VLC_FOURCC( 'i', '2', '6', '3' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'I', '2', '6', '3' );
            break;

        case VLC_FOURCC( 'm', 'j', 'p', 'g' ):
        case VLC_FOURCC( 'm', 'j', 'p', 'a' ):
        case VLC_FOURCC( 'j', 'p', 'e', 'g' ):
        case VLC_FOURCC( 'J', 'P', 'E', 'G' ):
        case VLC_FOURCC( 'J', 'F', 'I', 'F' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'M', 'J', 'P', 'G' );
            break;

        case VLC_FOURCC( 'd', 'v', 's', 'd' ):
        case VLC_FOURCC( 'D', 'V', 'S', 'D' ):
        case VLC_FOURCC( 'd', 'v', 'h', 'd' ):
            p_dec->fmt_out.i_codec = VLC_FOURCC( 'd', 'v', 's', 'l' );
            break;

        /* audio */
        case VLC_FOURCC( 'a', 'r', 'a', 'w' ):
            switch( ( p_dec->fmt_in.audio.i_bitspersample + 7 ) / 8 )
            {
                case 1:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('u','8',' ',' ');
                    break;
                case 2:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','l');
                    break;
                case 3:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','l');
                    break;
                case 4:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_dec, "unknown raw audio sample size" );
                    return VLC_EGENERIC;
            }
            break;

        case VLC_FOURCC( 't', 'w', 'o', 's' ):
            switch( ( p_dec->fmt_in.audio.i_bitspersample + 7 ) / 8 )
            {
                case 1:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','8',' ',' ');
                    break;
                case 2:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','b');
                    break;
                case 3:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
                    break;
                case 4:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','3','2','b');
                    break;
                default:
                    msg_Err( p_dec, "unknown raw audio sample size" );
                    return VLC_EGENERIC;
            }
            break;

        case VLC_FOURCC( 's', 'o', 'w', 't' ):
            switch( ( p_dec->fmt_in.audio.i_bitspersample + 7 ) / 8 )
            {
                case 1:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','8',' ',' ');
                    break;
                case 2:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','l');
                    break;
                case 3:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','l');
                    break;
                case 4:
                    p_dec->fmt_out.i_codec = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_dec, "unknown raw audio sample size" );
                    return VLC_EGENERIC;
            }
            break;
    }

    p_dec->p_sys = p_sys = malloc( sizeof( block_t ) );
    p_sys->p_block    = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;

    if( p_dec->p_sys->p_block )
    {
        block_ChainRelease( p_dec->p_sys->p_block );
    }

    free( p_dec->p_sys );
}

/*****************************************************************************
 * Packetize: packetize an unit (here copy a complete block )
 *****************************************************************************/
static block_t *Packetize ( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block;
    block_t *p_ret = p_dec->p_sys->p_block;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->i_dts <= 0 )
    {
        p_block->i_dts = p_block->i_pts;
    }

    if( p_block->i_dts <= 0 )
    {
        msg_Dbg( p_dec, "need dts > 0" );
        block_Release( p_block );
        return NULL;
    }

    if( p_ret != NULL && p_block->i_pts > p_ret->i_pts )
    {
        p_ret->i_length = p_block->i_pts - p_ret->i_pts;
    }
    p_dec->p_sys->p_block = p_block;

    return p_ret;
}

/*****************************************************************************
 * PacketizeSub: packetize an unit (here copy a complete block )
 *****************************************************************************/
static block_t *PacketizeSub( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->i_dts <= 0 )
    {
        p_block->i_dts = p_block->i_pts;
    }

    if( p_block->i_dts <= 0 )
    {
        msg_Dbg( p_dec, "need dts > 0" );
        block_Release( p_block );
        return NULL;
    }

    return p_block;
}
