/*****************************************************************************
 * ogt.c : Overlay Graphics Text (SVCD subtitles) decoder thread
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id$
 *
 * Author: Rocky Bernstein
 *   based on code from:
 *       Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *       Samuel Hocevar <sam@zoy.org>
 *       Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "subtitle.h"
#include "ogt.h"
#include "common.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  VCDSubOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );

vlc_module_begin();
    set_description( _("Philips OGT (SVCD subtitle) decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( VCDSubOpen, VCDSubClose );

    add_integer ( MODULE_STRING "-debug", 0, NULL,
                  DEBUG_TEXT, DEBUG_LONGTEXT, VLC_TRUE );

    add_integer ( MODULE_STRING "-horizontal-correct", 0, NULL,
                  HORIZONTAL_CORRECT, HORIZONTAL_CORRECT_LONGTEXT, VLC_FALSE );

    add_integer ( MODULE_STRING "-vertical-correct", 0, NULL,
                  VERTICAL_CORRECT, VERTICAL_CORRECT_LONGTEXT, VLC_FALSE );

    add_string( MODULE_STRING "-aspect-ratio", "", NULL,
                SUB_ASPECT_RATIO_TEXT, SUB_ASPECT_RATIO_LONGTEXT, VLC_TRUE );

    add_integer( MODULE_STRING "-duration-scaling", 9, NULL,
                 DURATION_SCALE_TEXT, DURATION_SCALE_LONGTEXT, VLC_TRUE );

    add_submodule();
    set_description( _("Philips OGT (SVCD subtitle) packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, VCDSubClose );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static block_t *Reassemble ( decoder_t *, block_t ** );
static subpicture_t *Decode( decoder_t *, block_t ** );
static block_t *Packetize  ( decoder_t *, block_t ** );

/*****************************************************************************
 * VCDSubOpen
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int
VCDSubOpen( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'o','g','t',' ' ) )
    {
        return VLC_EGENERIC;
    }


    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );

    p_sys->i_debug       = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_sys->b_packetizer  = VLC_FALSE;
    p_sys->p_vout        = NULL;
    p_sys->i_image       = -1;
    p_sys->subtitle_data = NULL;

    VCDSubInitSubtitleBlock( p_sys );

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 'o','g','t',' ' ) );


    p_dec->pf_decode_sub = Decode;
    p_dec->pf_packetize  = Packetize;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

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

    if( VCDSubOpen( p_this ) )
    {
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *
Decode ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_spu = Reassemble( p_dec, pp_block );
    vout_thread_t *p_last_vout = p_dec->p_sys->p_vout;

    dbg_print( (DECODE_DBG_CALL) , "");

    if( p_spu )
    {
        p_sys->i_spu = block_ChainExtract( p_spu, p_sys->buffer, 65536 );
        p_sys->i_pts = p_spu->i_pts;
        block_ChainRelease( p_spu );

        if( ( p_sys->p_vout = VCDSubFindVout( p_dec ) ) )
        {
            if( p_last_vout != p_sys->p_vout )
            {
                spu_Control( p_sys->p_vout->p_spu, SPU_CHANNEL_REGISTER,
                             &p_sys->i_subpic_channel );
            }

            /* Parse and decode */
            E_(ParsePacket)( p_dec );

            vlc_object_release( p_sys->p_vout );
        }

        VCDSubInitSubtitleBlock ( p_sys );
    }

    return NULL;
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *
Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_spu = Reassemble( p_dec, pp_block );

    if( p_spu )
    {
        p_spu->i_dts = p_spu->i_pts;
        p_spu->i_length = 0;

        VCDSubInitSubtitleBlock( p_sys );

        return block_ChainGather( p_spu );
    }
    return NULL;
}

#define SPU_HEADER_LEN 5

/*****************************************************************************
 Reassemble:

 The data for single screen subtitle may come in one of many
 non-contiguous packets of a stream. This routine is called when the
 next packet in the stream comes in. The job of this routine is to
 parse the header, if this is the beginning, and combine the packets
 into one complete subtitle unit.

 If everything is complete, we will return a block. Otherwise return
 NULL.


 The format of the beginning of the subtitle packet that is used here.

   size    description
   -------------------------------------------
   byte    subtitle channel (0..7) in bits 0-3
   byte    subtitle packet number of this subtitle image 0-N,
           if the subtitle packet is complete, the top bit of the byte is 1.
   uint16  subtitle image number

 *****************************************************************************/
static block_t *
Reassemble( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    uint8_t *p_buffer;
    uint16_t i_expected_image;
    uint8_t  i_packet, i_expected_packet;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->i_buffer < SPU_HEADER_LEN )
    {
      msg_Dbg( p_dec, "invalid packet header (size %d < %d)" ,
               p_block->i_buffer, SPU_HEADER_LEN );
      block_Release( p_block );
      return NULL;
    }

    p_buffer = p_block->p_buffer;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_PACKET),
               "header: 0x%02x 0x%02x 0x%02x 0x%02x, size: %i",
               p_buffer[1], p_buffer[2], p_buffer[3], p_buffer[4],
               p_block->i_buffer);

    /* Attach to our input thread and see if subtitle is selected. */
    {
        vlc_object_t * p_input;
        vlc_value_t val;

        p_input = vlc_object_find( p_dec, VLC_OBJECT_INPUT, FIND_PARENT );

        if( !p_input ) return NULL;

        if( var_Get( p_input, "spu-channel", &val ) )
        {
            vlc_object_release( p_input );
          return NULL;
        }

        vlc_object_release( p_input );
        dbg_print( (DECODE_DBG_PACKET),
                   "val.i_int %x p_buffer[i] %x", val.i_int, p_buffer[1]);

        /* The dummy ES that the menu selection uses has an 0x70 at
           the head which we need to strip off. */
        if( val.i_int == -1 || (val.i_int & 0x03) != p_buffer[1] )
        {
            dbg_print( DECODE_DBG_PACKET, "subtitle not for us.\n");
            return NULL;
        }
    }

    if ( p_sys->state == SUBTITLE_BLOCK_EMPTY ) {
      i_expected_image  = p_sys->i_image+1;
      i_expected_packet = 0;
    } else {
      i_expected_image  = p_sys->i_image;
      i_expected_packet = p_sys->i_packet+1;
    }

    p_buffer += 2;

    if ( *p_buffer & 0x80 ) {
      p_sys->state = SUBTITLE_BLOCK_COMPLETE;
      i_packet     = ( *p_buffer++ & 0x7F );
    } else {
      p_sys->state = SUBTITLE_BLOCK_PARTIAL;
      i_packet     = *p_buffer++;
    }

    p_sys->i_image = GETINT16(p_buffer);

    if ( p_sys->i_image != i_expected_image ) {
      msg_Warn( p_dec, "expecting subtitle image %u but found %u",
                i_expected_image, p_sys->i_image );
    }

    if ( i_packet != i_expected_packet ) {
      msg_Warn( p_dec, "expecting subtitle image packet %u but found %u",
                i_expected_packet, i_packet);
    }

    p_sys->i_packet = i_packet;

    if ( p_sys->i_packet == 0 ) {
      /* First packet in the subtitle block */
      E_(ParseHeader)( p_dec, p_buffer, p_block );
      VCDSubInitSubtitleData(p_sys);
    }

    /* FIXME - remove append_data and use chainappend */
    VCDSubAppendData( p_dec, p_buffer, p_block->i_buffer - SPU_HEADER_LEN );

    block_ChainAppend( &p_sys->p_block, p_block );

    p_sys->i_spu += p_block->i_buffer - SPU_HEADER_LEN;

    if (p_sys->state == SUBTITLE_BLOCK_COMPLETE)
    {
      if( p_sys->i_spu != p_sys->i_spu_size )
        {
          msg_Warn( p_dec, "SPU packets size=%d should be %d",
                   p_sys->i_spu, p_sys->i_spu_size );
        }

      dbg_print( (DECODE_DBG_PACKET),
                 "subtitle packet complete, size=%d", p_sys->i_spu );

      return p_sys->p_block;
    }
    return NULL;
}


/*
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
