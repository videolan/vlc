/*****************************************************************************
 * ogt.c : Overlay Graphics Text (SVCD subtitles) decoder thread
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ogt.c,v 1.1 2003/12/07 22:14:26 rocky Exp $
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
    set_description( _("SVCD subtitle decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( DecoderOpen, Close );

    add_integer ( MODULE_STRING "-debug", 0, NULL, 
		  N_("set debug mask for additional debugging."),
                  N_(DEBUG_LONGTEXT), VLC_TRUE );

    add_submodule();
    set_description( _("SVCD subtitle packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static vout_thread_t *FindVout( decoder_t *);

static block_t *Reassemble( decoder_t *, block_t ** );

static void     Decode   ( decoder_t *, block_t ** );
static block_t *Packetize( decoder_t *, block_t ** );


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

}


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

    p_sys->i_debug      = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_sys->b_packetizer = VLC_FALSE;
    p_sys->p_vout       = NULL;
    p_sys->i_image      = -1;

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
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
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

#define SPU_HEADER_LEN 5 

/*****************************************************************************
 Reassemble:
 
 The data for single screen subtitle may come in one of many
 non-contiguous packets of a stream. This routine is called when the
 next packet in the stream comes in. The job of this routine is to
 parse the header if this is the beginning) and combine the packets into one
 complete subtitle unit.

 If everything is complete, we will return a block. Otherwise return
 NULL.

 The following information is mostly extracted from the SubMux package of
 unknown author with additional experimentation.
 
 The format is roughly as follows (everything is big-endian):
 
      byte subtitle channel 0-3
 	byte subtitle packet number in this subtitle image 0-N,
 	last is XOR'ed 0x80
        u_int16 subtitle image number
        u_int16 length in bytes of the rest
        byte option flags, unknown meaning except bit 3 (0x08) indicates
 	     presence of the duration field
        byte unknown meaning
 	u_int32 duration in 1/90000ths of a second (optional), start time
 		is as indicated by the PTS in the PES header
 	u_int32 xpos
 	u_int32 ypos
 	u_int32 width (must be even)
 	u_int32 height (must be even)
 	byte[16] palette, 4 palette entries, each contains values for
 		Y, U, V and transparency, 0 standing for transparent
 	byte command,
 		cmd>>6==1 indicates shift
 		(cmd>>4)&3 is direction from, (0=top,1=left,2=right,3=bottom)
 	u_int32 shift duration in 1/90000ths of a second
 	u_int16 offset of odd field (subtitle image is presented interlaced)
 	byte[] image
 
  The image is encoded using two bits per pixel that select a palette
  entry except that value 00 starts a limited rle.  When 00 is seen,
  the next two bits (00-11) encode the number of pixels (1-4, add one to
  the indicated value) to fill with the color in palette entry 0).
  The encoding of each line is padded to a whole number of bytes.  The
  first field is padded to an even byte lenght and the complete subtitle
  is padded to a 4-byte multiple that always include one zero byte at
  the end.


 *****************************************************************************/
static block_t *
Reassemble( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    uint8_t *p_buffer;
    u_int16_t i_expected_image;
    u_int8_t  i_packet, i_expected_packet;

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
	       "header: 0x%02x 0x%02x 0x%02x 0x%02x\n",
	       p_buffer[1], p_buffer[2], p_buffer[3], p_buffer[4] );


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
      msg_Warn( p_dec, "expecting subtitle image %u but found %u\n",
		i_expected_image, p_sys->i_image );
    }

    if ( i_packet != i_expected_packet ) {
      msg_Warn( p_dec, "expecting subtitle image packet %u but found %u",
		i_expected_packet, i_packet);
    }

    p_sys->i_packet = i_packet;

    if ( p_sys->i_packet == 0 ) {
      /* First packet in the subtitle block */
      /* FIXME: Put all of this into a subroutine. */
      u_int8_t *p = p_buffer;
      p = p_buffer;
      int i;

      p_sys->i_pts    = p_block->i_pts;
      
      p_sys->i_spu_size = GETINT16(p);
      p_sys->i_options  = *p++;
      p_sys->i_options2 = *p++;

      if ( p_sys->i_options & 0x08 ) {
	p_sys->duration = GETINT32(p);
      } else {
	p_sys->duration = 0;
      }
      p_sys->i_x_start= GETINT16(p);
      p_sys->i_y_start= GETINT16(p);
      p_sys->i_width  = GETINT16(p);
      p_sys->i_height = GETINT16(p);
      
      for (i=0; i<4; i++) {
	p_sys->pi_palette[i].y = *p++;
	p_sys->pi_palette[i].u = *p++;
	p_sys->pi_palette[i].v = *p++;
	/* We have just 4-bit resolution for alpha, but the value for SVCD
	 * has 8 bits so we scale down the values to the acceptable range */
	p_sys->pi_palette[i].t = (*p++) >> 4;
      }
      p_sys->i_cmd = *p++;
      /* We do not really know this, FIXME */
      if ( p_sys->i_cmd ) {
	p_sys->i_cmd_arg = GETINT32(p);
      }
      /* Image starts just after skipping next short */
      p_sys->comp_image_offset = p + 2 - p_buffer;
      /* There begins the first field, so no correction needed */
      p_sys->first_field_offset = 0;
      /* Actually, this is measured against a different origin, so we have to
	 adjust it */
      p_sys->second_field_offset = GETINT16(p);
      p_sys->comp_image_offset = p - p_buffer;
      p_sys->comp_image_length =
	p_sys->subtitle_data_length - p_sys->comp_image_offset;
      p_sys->metadata_length   = p_sys->comp_image_offset;
      
      /*spuogt_init_subtitle_data(p_sys);*/
      
      p_sys->subtitle_data_pos = 0;

      dbg_print( (DECODE_DBG_PACKET), 
		 "x-start: %d, y-start: %d, width: %d, height %d, "
		 "spu size: %d",
		 p_sys->i_x_start, p_sys->i_y_start, 
		 p_sys->i_width, p_sys->i_height, 
		 p_sys->i_spu_size );
    }

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

/* following functions are local */

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

