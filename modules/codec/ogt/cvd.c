/*****************************************************************************
 * cvd.c : CVD Subtitle decoder thread
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cvd.c,v 1.3 2003/12/29 04:47:44 rocky Exp $
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

#include "subtitle.h"
#include "cvd.h"
#include "common.h"

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "external call          1\n" \
    "all calls              2\n" \
    "packet assembly info   4\n" \
    "image bitmaps          8\n" \
    "image transformations 16\n" \
    "misc info             32\n" )

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  DecoderOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );

vlc_module_begin();
    set_description( _("CVD subtitle decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( DecoderOpen, VCDSubClose );

    add_integer ( MODULE_STRING "-debug", 0, NULL,
                  N_("set debug mask for additional debugging."),
                  N_(DEBUG_LONGTEXT), VLC_TRUE );

    add_submodule();
    set_description( _("Chaoji VCD subtitle packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, VCDSubClose );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

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

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'c','v','d',' ' ) )
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

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 'c','v','d',' ' ) );


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

        if( ( p_sys->p_vout = VCDSubFindVout( p_dec ) ) )
        {
            /* Parse and decode */
            E_(ParsePacket)( p_dec );

            vlc_object_release( p_sys->p_vout );
        }

        VCDSubInitSubtitleBlock ( p_sys );
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

        VCDSubInitSubtitleBlock( p_sys );

        return block_ChainGather( p_spu );
    }
    return NULL;
}

/* following functions are local */

#define SPU_HEADER_LEN 1

/*****************************************************************************
 Reassemble:

 The data for single screen subtitle may come in one of many
 non-contiguous packets of a stream. This routine is called when the
 next packet in the stream comes in. The job of this routine is to
 parse the header, if this is the beginning, and combine the packets
 into one complete subtitle unit.

 If everything is complete, we will return a block. Otherwise return
 NULL.

 *****************************************************************************/
static block_t *
Reassemble( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    uint8_t *p_buffer;

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
	       "header: 0x%02x 0x%02x 0x%02x 0x%02x, 0x%02x, 0x%02x, size: %i",
	       p_buffer[1], p_buffer[2], p_buffer[3], p_buffer[4],
	       p_buffer[5], p_buffer[6],
	       p_block->i_buffer);

    if( config_GetInt( p_dec, "spu-channel" ) != p_buffer[0] )
      return NULL;

    /* There is little data on the format, but it does not seem to have a
       good way to detect the first packet in the subtitle.  It seems,
       however, that it has a valid pts while later packets for the same
       image don't */

    if ( p_sys->state == SUBTITLE_BLOCK_EMPTY && p_block->i_pts == 0 ) {
      msg_Warn( p_dec, 
		"first packet expected but no PTS present -- skipped\n");
      return NULL;
    }

    if ( p_sys->subtitle_data_pos == 0 ) {
      /* First packet in the subtitle block */
      E_(ParseHeader)( p_dec, p_buffer, p_block );
      VCDSubInitSubtitleData(p_sys);
    }

    /* FIXME - remove append_data and use chainappend */
    VCDSubAppendData( p_dec, p_buffer, p_block->i_buffer - 1 );

    block_ChainAppend( &p_sys->p_block, p_block );

    p_sys->i_spu += p_block->i_buffer - SPU_HEADER_LEN;

    if ( p_sys->subtitle_data_pos == p_sys->i_spu_size ) {
      /* last packet in subtitle block. */

      uint8_t *p     = p_sys->subtitle_data + p_sys->metadata_offset+1;
      uint8_t *p_end = p + p_sys->metadata_length;

      dbg_print( (DECODE_DBG_PACKET),
                 "subtitle packet complete, size=%d", p_sys->i_spu );

      p_sys->state = SUBTITLE_BLOCK_COMPLETE;
      p_sys->i_image++;


      for ( ; p < p_end; p += 4 ) {

	switch ( p[0] ) {
	  
	case 0x04:	/* Display duration in 1/90000ths of a second */

	  p_sys->i_duration = (p[1]<<16) + (p[2]<<8) + p[3];
	  
	  dbg_print( DECODE_DBG_PACKET, 
		     "subtitle display duration %u", p_sys->i_duration);
	  break;
	  
	case 0x0c:	/* Unknown */
	  dbg_print( DECODE_DBG_PACKET, 
		     "subtitle command unknown 0x%0x 0x%0x 0x%0x 0x%0x\n",
		    p[0], p[1], p[2], p[3]);
	  break;
	  
	case 0x17:	/* Position */
	  p_sys->i_x_start = ((p[1]&0x0f)<<6) + (p[2]>>2);
	  p_sys->i_y_start = ((p[2]&0x03)<<8) + p[3];
	  dbg_print( DECODE_DBG_PACKET, 
		     "start position (%d,%d): %.2x %.2x %.2x", 
		     p_sys->i_x_start, p_sys->i_y_start,
		     p[1], p[2], p[3] );
	  break;
	  
	case 0x1f:	/* Coordinates of the image bottom right */
	  {
	    int lastx = ((p[1]&0x0f)<<6) + (p[2]>>2);
	    int lasty = ((p[2]&0x03)<<8) + p[3];
	    p_sys->i_width  = lastx - p_sys->i_x_start + 1;
	    p_sys->i_height = lasty - p_sys->i_y_start + 1;
	    dbg_print( DECODE_DBG_PACKET, 
		       "end position: (%d,%d): %.2x %.2x %.2x, w x h: %d x %d",
		       lastx, lasty, p[1], p[2], p[3], 
		       p_sys->i_width, p_sys->i_height );
	    break;
	  }
	  
	  
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27: 
	  {
	    uint8_t v = p[0]-0x24;
	    
	    /* Primary Palette */
	    dbg_print( DECODE_DBG_PACKET,
		       "primary palette %d (y,u,v): (0x%0x,0x%0x,0x%0x)",
		       v, p[1], p[2], p[3]);
	    
	    p_sys->pi_palette[v].s.y = p[1];
	    p_sys->pi_palette[v].s.u = p[2];
	    p_sys->pi_palette[v].s.v = p[3];
	    break;
	  }
	  
	  
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
	  {
	    uint8_t v = p[0]-0x2c;
	    
	    dbg_print( DECODE_DBG_PACKET,
		       "highlight palette %d (y,u,v): (0x%0x,0x%0x,0x%0x)",
		       v, p[1], p[2], p[3]);
	    
	    /* Highlight Palette */
	    p_sys->pi_palette_highlight[v].s.y = p[1];
	    p_sys->pi_palette_highlight[v].s.u = p[2];
	    p_sys->pi_palette_highlight[v].s.v = p[3];
	    break;
	  }

	case 0x37:
	  /* transparency for primary palette */
	  p_sys->pi_palette[0].s.t = p[3] & 0x0f;
	  p_sys->pi_palette[1].s.t = p[3] >> 4;
	  p_sys->pi_palette[2].s.t = p[2] & 0x0f;
	  p_sys->pi_palette[3].s.t = p[2] >> 4;

	  dbg_print( DECODE_DBG_PACKET,
		     "transparancy for primary palette (y,u,v): "
		     "0x%0x 0x%0x 0x%0x",
		     p[1], p[2], p[3]);

	  break;

	case 0x3f:
	  /* transparency for highlight palette */
	  p_sys->pi_palette_highlight[0].s.t = p[2] & 0x0f;
	  p_sys->pi_palette_highlight[1].s.t = p[2] >> 4;
	  p_sys->pi_palette_highlight[2].s.t = p[1] & 0x0f;
	  p_sys->pi_palette_highlight[3].s.t = p[1] >> 4;

	  dbg_print( DECODE_DBG_PACKET,
		     "transparancy for highlight palette (y,u,v): "
		     "0x%0x 0x%0x 0x%0x",
		     p[1], p[2], p[3]);

	  break;
	  
	case 0x47:
	  /* offset to first field data, we correct to make it relative
	     to comp_image_offset (usually 4) */
	  p_sys->first_field_offset =
	    (p[2] << 8) + p[3] - p_sys->comp_image_offset;
	  dbg_print( DECODE_DBG_PACKET, 
		     "first_field_offset %d", p_sys->first_field_offset);
	  break;
	  
	case 0x4f:
	  /* offset to second field data, we correct to make it relative to
	     comp_image_offset (usually 4) */
	  p_sys->second_field_offset =
	    (p[2] << 8) + p[3] - p_sys->comp_image_offset;
	  dbg_print( DECODE_DBG_PACKET, 
		     "second_field_offset %d", p_sys->second_field_offset);
	  break;
	  
	default:
	  msg_Warn( p_dec, 
		    "unknown sequence in control header " 
		    "0x%0x 0x%0x 0x%0x 0x%0x",
		    p[0], p[1], p[2], p[3]);
	  
	  p_sys->subtitle_data_pos = 0;
	}
      }
      return p_sys->p_block;
    } else {
      /* Not last block in subtitle, so wait for another. */
      p_sys->state = SUBTITLE_BLOCK_PARTIAL;
    }

    
    return NULL;
}
