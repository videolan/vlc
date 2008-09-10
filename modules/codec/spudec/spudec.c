/*****************************************************************************
 * spudec.c : SPU decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include "spudec.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  DecoderOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );
static void Close         ( vlc_object_t * );

vlc_module_begin();
    set_description( N_("DVD subtitles decoder") );
    set_capability( "decoder", 50 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    set_callbacks( DecoderOpen, Close );

    add_submodule();
    set_description( N_("DVD subtitles packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *      Reassemble( decoder_t *, block_t ** );
static subpicture_t * Decode    ( decoder_t *, block_t ** );
static block_t *      Packetize ( decoder_t *, block_t ** );

/*****************************************************************************
 * DecoderOpen
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 's','p','u',' ' ) &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 's','p','u','b' ) )
    {
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );

    p_sys->b_packetizer = false;
    p_sys->i_spu_size = 0;
    p_sys->i_spu      = 0;
    p_sys->p_block    = NULL;

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 's','p','u',' ' ) );

    p_dec->pf_decode_sub = Decode;
    p_dec->pf_packetize  = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * PacketizerOpen
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int PacketizerOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( DecoderOpen( p_this ) )
    {
        return VLC_EGENERIC;
    }
    p_dec->pf_packetize  = Packetize;
    p_dec->p_sys->b_packetizer = true;
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_FOURCC( 's','p','u',' ' );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_block )
    {
        block_ChainRelease( p_sys->p_block );
    }

    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_spu_block;
    subpicture_t  *p_spu;

    p_spu_block = Reassemble( p_dec, pp_block );

    if( ! p_spu_block )
    {
        return NULL;
    }

    /* FIXME: what the, we shouldn’t need to allocate 64k of buffer --sam. */
    p_sys->i_spu = block_ChainExtract( p_spu_block, p_sys->buffer, 65536 );
    p_sys->i_pts = p_spu_block->i_pts;
    p_sys->i_rate = p_spu_block->i_rate;
    block_ChainRelease( p_spu_block );

    /* Parse and decode */
    p_spu = ParsePacket( p_dec );

    /* reinit context */
    p_sys->i_spu_size = 0;
    p_sys->i_rle_size = 0;
    p_sys->i_spu      = 0;
    p_sys->p_block    = NULL;

    return p_spu;
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_spu = Reassemble( p_dec, pp_block );

    if( ! p_spu )
    {
        return NULL;
    }

    p_spu->i_dts = p_spu->i_pts;
    p_spu->i_length = 0;

    /* reinit context */
    p_sys->i_spu_size = 0;
    p_sys->i_rle_size = 0;
    p_sys->i_spu      = 0;
    p_sys->p_block    = NULL;

    return block_ChainGather( p_spu );
}

/*****************************************************************************
 * Reassemble:
 *****************************************************************************/
static block_t *Reassemble( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( pp_block == NULL || *pp_block == NULL ) return NULL;
    p_block = *pp_block;
    *pp_block = NULL;

    if( p_sys->i_spu_size <= 0 &&
        ( p_block->i_pts <= 0 || p_block->i_buffer < 4 ) )
    {
        msg_Dbg( p_dec, "invalid starting packet (size < 4 or pts <=0)" );
        msg_Dbg( p_dec, "spu size: %d, i_pts: %"PRId64" i_buffer: %zu",
                 p_sys->i_spu_size, p_block->i_pts, p_block->i_buffer );
        block_Release( p_block );
        return NULL;
    }

    block_ChainAppend( &p_sys->p_block, p_block );
    p_sys->i_spu += p_block->i_buffer;

    if( p_sys->i_spu_size <= 0 )
    {
        p_sys->i_spu_size = ( p_block->p_buffer[0] << 8 )|
            p_block->p_buffer[1];
        p_sys->i_rle_size = ( ( p_block->p_buffer[2] << 8 )|
            p_block->p_buffer[3] ) - 4;

        /* msg_Dbg( p_dec, "i_spu_size=%d i_rle=%d",
                    p_sys->i_spu_size, p_sys->i_rle_size ); */

        if( p_sys->i_spu_size <= 0 || p_sys->i_rle_size >= p_sys->i_spu_size )
        {
            p_sys->i_spu_size = 0;
            p_sys->i_rle_size = 0;
            p_sys->i_spu      = 0;
            p_sys->p_block    = NULL;

            block_Release( p_block );
            return NULL;
        }
    }

    if( p_sys->i_spu >= p_sys->i_spu_size )
    {
        /* We have a complete sub */
        if( p_sys->i_spu > p_sys->i_spu_size )
            msg_Dbg( p_dec, "SPU packets size=%d should be %d",
                     p_sys->i_spu, p_sys->i_spu_size );

        return p_sys->p_block;
    }
    return NULL;
}
