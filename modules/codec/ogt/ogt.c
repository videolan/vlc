/*****************************************************************************
 * ogt.c : Overlay Graphics Text (SVCD subtitles) decoder thread
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ogt.c,v 1.4 2003/12/26 02:47:59 rocky Exp $
 *
 * Authors: Rocky Bernstein
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

#include "ogt.h"

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "external call     1\n" \
    "all calls         2\n" \
    "misc              4\n" )


/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  DecoderOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );

static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("Philips OGT (SVCD subtitle) decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( DecoderOpen, Close );

    add_integer ( MODULE_STRING "-debug", 0, NULL,
                  N_("set debug mask for additional debugging."),
                  N_(DEBUG_LONGTEXT), VLC_TRUE );

    add_submodule();
    set_description( _("Philips OGT (SVCD subtitle) packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void     InitSubtitleBlock( decoder_sys_t * p_sys );
static vout_thread_t *FindVout( decoder_t *);

static block_t *Reassemble( decoder_t *, block_t ** );

static void     Decode   ( decoder_t *, block_t ** );
static block_t *Packetize( decoder_t *, block_t ** );


/*****************************************************************************
 * DecoderOpen
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int
DecoderOpen( vlc_object_t *p_this )
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

    InitSubtitleBlock( p_sys );

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

    if( DecoderOpen( p_this ) )
    {
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

    if( !p_sys->b_packetizer )
    {
        /* FIXME check if it's ok to not lock vout */
        if( p_sys->p_vout != NULL && p_sys->p_vout->p_subpicture != NULL )
        {
            subpicture_t *  p_subpic;
            int             i_subpic;

            for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
            {
                p_subpic = &p_sys->p_vout->p_subpicture[i_subpic];

                if( p_subpic != NULL &&
                    ( ( p_subpic->i_status == RESERVED_SUBPICTURE ) ||
                      ( p_subpic->i_status == READY_SUBPICTURE ) ) )
                {
                    vout_DestroySubPicture( p_sys->p_vout, p_subpic );
                }
            }
        }
    }

    if( p_sys->p_block )
    {
        block_ChainRelease( p_sys->p_block );
    }

    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static void
Decode ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_spu = Reassemble( p_dec, pp_block );

    dbg_print( (DECODE_DBG_CALL) , "");

    if( p_spu )
    {
        p_sys->i_spu = block_ChainExtract( p_spu, p_sys->buffer, 65536 );
        p_sys->i_pts = p_spu->i_pts;
        block_ChainRelease( p_spu );

        if( ( p_sys->p_vout = FindVout( p_dec ) ) )
        {
            /* Parse and decode */
            E_(ParsePacket)( p_dec );

            vlc_object_release( p_sys->p_vout );
        }

        InitSubtitleBlock ( p_sys );
    }

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

        InitSubtitleBlock( p_sys );

        return block_ChainGather( p_spu );
    }
    return NULL;
}

/* following functions are local */

static void 
InitSubtitleData(decoder_sys_t *p_sys)
{
  if ( p_sys->subtitle_data ) {
    if ( p_sys->subtitle_data_size < p_sys->i_spu_size ) {
      p_sys->subtitle_data = realloc(p_sys->subtitle_data,
				    p_sys->i_spu_size);
      p_sys->subtitle_data_size = p_sys->i_spu_size;
    }
  } else {
    p_sys->subtitle_data = malloc(p_sys->i_spu_size);
    p_sys->subtitle_data_size = p_sys->i_spu_size;
    /* FIXME: wrong place to get p_sys */
    p_sys->i_image = 0;
  }
  p_sys->subtitle_data_pos = 0;
}

static void 
AppendData ( decoder_t *p_dec, uint8_t *buffer, uint32_t buf_len )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  int chunk_length = buf_len;

  if ( chunk_length > p_sys->i_spu_size - p_sys->subtitle_data_pos ) {
    msg_Warn( p_dec, "too much data (%d) expecting at most %u",
	      chunk_length, p_sys->i_spu_size - p_sys->subtitle_data_pos );

    chunk_length = p_sys->i_spu_size - p_sys->subtitle_data_pos;
  }

  if ( chunk_length > 0 ) {
    memcpy(p_sys->subtitle_data + p_sys->subtitle_data_pos,
	   buffer, chunk_length);
    p_sys->subtitle_data_pos += chunk_length;
    dbg_print(DECODE_DBG_PACKET, "%d bytes appended, pointer now %d",
	      chunk_length, p_sys->subtitle_data_pos);
  }
}

/*****************************************************************************
 InitSubtitleBlock:

Initialize so the next packet will start off a new one.

 *****************************************************************************/
static void 
InitSubtitleBlock( decoder_sys_t * p_sys ) 
{
  p_sys->i_spu_size = 0;
  p_sys->state      = SUBTITLE_BLOCK_EMPTY;
  p_sys->i_spu      = 0;
  p_sys->p_block    = NULL;
  p_sys->subtitle_data_pos = 0;

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

    if( config_GetInt( p_dec, "spu-channel" ) != p_buffer[1] )
      return NULL;

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
      InitSubtitleData(p_sys);
    }

    /* FIXME - remove append_data and use chainappend */
    AppendData( p_dec, p_buffer, p_block->i_buffer - 5 );

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

/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
static vout_thread_t *FindVout( decoder_t *p_dec )
{
    vout_thread_t *p_vout = NULL;

    /* Find an available video output */
    do
    {
        if( p_dec->b_die || p_dec->b_error )
        {
            break;
        }

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    return p_vout;
}

