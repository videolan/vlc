/*****************************************************************************
 * Philips OGT (SVCD subtitle) packet parser
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id: ogt_parse.c,v 1.9 2004/01/14 11:47:19 rocky Exp $
 *
 * Author: Rocky Bernstein 
 *   based on code from: 
 *       Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *       Sam Hocevar <sam@zoy.org>
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
#include "common.h"
#include "render.h"
#include "ogt.h"

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

/* An image color is a two-bit palette entry: 0..3 */ 
typedef uint8_t ogt_color_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  ParseImage         ( decoder_t *, subpicture_t * );

/*
  The format is roughly as follows (everything is big-endian):
 
   size     description
   -------------------------------------------
   byte     subtitle channel (0..7) in bits 0-3 
   byte     subtitle packet number of this subtitle image 0-N,
            if the subtitle packet is complete, the top bit of the byte is 1.
   u_int16  subtitle image number
   u_int16  length in bytes of the rest
   byte     option flags, unknown meaning except bit 3 (0x08) indicates
 	    presence of the duration field
   byte     unknown 
   u_int32  duration in 1/90000ths of a second (optional), start time
 	    is as indicated by the PTS in the PES header
   u_int32  xpos
   u_int32  ypos
   u_int32  width (must be even)
   u_int32  height (must be even)
   byte[16] palette, 4 palette entries, each contains values for
 	    Y, U, V and transparency, 0 standing for transparent
   byte     command,
 	    cmd>>6==1 indicates shift
 	    (cmd>>4)&3 is direction from, (0=top,1=left,2=right,3=bottom)
   u_int32  shift duration in 1/90000ths of a second
   u_int16  offset of odd-numbered scanlines - subtitle images are 
            given in interlace order
   byte[]   limited RLE image data in interlace order (0,2,4... 1,3,5) with
            2-bits per palette number
*/

/* FIXME: do we really need p_buffer and p? 
   Can't all of thes _offset's and _lengths's get removed? 
*/
void E_(ParseHeader)( decoder_t *p_dec, uint8_t *p_buffer, block_t *p_block )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t *p = p_buffer;
  int i;

  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

  p_sys->i_pts    = p_block->i_pts;
  p_sys->i_spu_size = GETINT16(p);
  p_sys->i_options  = *p++;
  p_sys->i_options2 = *p++;
  
  if ( p_sys->i_options & 0x08 ) {
    p_sys->i_duration = GETINT32(p);
    p_sys->i_duration *= config_GetInt( p_dec, MODULE_STRING 
                                        "-duration-scaling" );
  } else {
    /* 0 means display until next subtitle comes in. */
    p_sys->i_duration = 0;
  }
  p_sys->i_x_start= GETINT16(p);
  p_sys->i_y_start= GETINT16(p);
  p_sys->i_width  = GETINT16(p);
  p_sys->i_height = GETINT16(p);
  
  for (i=0; i<4; i++) {
    p_sys->p_palette[i].s.y = *p++;
    p_sys->p_palette[i].s.u = *p++;
    p_sys->p_palette[i].s.v = *p++;
    /* OGT has 8-bit resolution for alpha, but DVD's and CVDS use 4-bits.
       Since we want to use the same render routine, rather than scale up
       CVD (and DVD) subtitles, we'll scale down ours. 
    */
    p_sys->p_palette[i].s.t = (*p++) >> 4;
  }
  p_sys->i_cmd = *p++;
      /* We do not really know this, FIXME */
  if ( p_sys->i_cmd ) {
    p_sys->i_cmd_arg = GETINT32(p);
  }

  /* Actually, this is measured against a different origin, so we have to
     adjust it */
  p_sys->second_field_offset = GETINT16(p);
  p_sys->i_image_offset  = p - p_buffer;
  p_sys->i_image_length  = p_sys->i_spu_size - p_sys->i_image_offset;
  p_sys->metadata_length = p_sys->i_image_offset;

  if (p_sys && p_sys->i_debug & DECODE_DBG_PACKET) {
    msg_Dbg( p_dec, "x-start: %d, y-start: %d, width: %d, height %d, "
	     "spu size: %d, duration: %lu (d:%d p:%d)",
	     p_sys->i_x_start, p_sys->i_y_start, 
	     p_sys->i_width, p_sys->i_height, 
	     p_sys->i_spu_size, (long unsigned int) p_sys->i_duration,
	     p_sys->i_image_length, p_sys->i_image_offset);
    
    for (i=0; i<4; i++) {
      msg_Dbg( p_dec, "palette[%d]= T: %2x, Y: %2x, u: %2x, v: %2x", i,
	       p_sys->p_palette[i].s.t, p_sys->p_palette[i].s.y, 
	       p_sys->p_palette[i].s.u, p_sys->p_palette[i].s.v );
    }
  }
}


/*****************************************************************************
 * ParsePacket: parse an SPU packet and send it to the video output
 *****************************************************************************
 * This function parses the SPU packet and, if valid, sends it to the
 * video output.
 *****************************************************************************/
void 
E_(ParsePacket)( decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    subpicture_t  *p_spu;

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");
    
    /* Allocate the subpicture internal data. */
    p_spu = vout_CreateSubPicture( p_sys->p_vout, MEMORY_SUBPICTURE );
    if( p_spu == NULL )
    {
        return;
    }

    /* In ParseImage we expand the run-length encoded color 0's; also
       we expand pixels and remove the color palette. This should
       facilitate scaling and antialiasing and speed up rendering.
    */
    p_spu->p_sys = malloc( sizeof( subpicture_sys_t ) 
			   + PIXEL_SIZE * (p_sys->i_width * p_sys->i_height) );

    /* Fill the p_spu structure */
    vlc_mutex_init( p_dec, &p_spu->p_sys->lock );

    p_spu->pf_render  = VCDSubRender;
    p_spu->pf_destroy = VCDSubDestroySPU;
    p_spu->p_sys->p_data = (uint8_t*)p_spu->p_sys + sizeof( subpicture_sys_t );

    p_spu->p_sys->i_x_end        = p_sys->i_x_start + p_sys->i_width - 1;
    p_spu->p_sys->i_y_end        = p_sys->i_y_start + p_sys->i_height - 1;

    /* FIXME: use aspect ratio for x? */
    p_spu->i_x        = p_sys->i_x_start * 3 / 4; 
    p_spu->i_y        = p_sys->i_y_start;
    p_spu->i_width    = p_sys->i_width;
    p_spu->i_height   = p_sys->i_height;

    p_spu->i_start    = p_sys->i_pts;
    p_spu->i_stop     = p_sys->i_pts + p_sys->i_duration;
    
    p_spu->p_sys->b_crop  = VLC_FALSE;
    p_spu->p_sys->i_debug = p_sys->i_debug;

    /* Get display time now. If we do it later, we may miss the PTS. */
    p_spu->p_sys->i_pts = p_sys->i_pts;

    /* Attach to our input thread */
    p_spu->p_sys->p_input = vlc_object_find( p_dec,
                                             VLC_OBJECT_INPUT, FIND_PARENT );

    /* We try to display it */
    if( ParseImage( p_dec, p_spu ) )
    {
        /* There was a parse error, delete the subpicture */
        vout_DestroySubPicture( p_sys->p_vout, p_spu );
        return;
    }

    /* SPU is finished - we can ask the video output to display it */
    vout_DisplaySubPicture( p_sys->p_vout, p_spu );

}

/* Advance pointer to image pointer, update internal i_2bit_field counter
   and check that we haven't goine too far  in the image data. */
#define advance_color_pointer_byte					\
  p++;									\
  i_2bit_field=4;							\
  if (p >= maxp) {							\
    msg_Warn( p_dec,							\
	      "broken subtitle - tried to access beyond end "		\
	      "in image extraction");					\
    return VLC_EGENERIC;						\
  }									\

#define advance_color_pointer						\
  i_2bit_field--;							\
  if ( i_2bit_field == 0 ) {						\
    advance_color_pointer_byte;						\
  }									

#define OGT_FIELD_BITS (2)
#define OGT_FIELD_MASK  ((1<<OGT_FIELD_BITS) - 1) 

/* Get the next field - either a palette index or a RLE count for
   color 0.  To do this we use byte image pointer p, and i_2bit_field
   which indicates where we are in the byte.
*/
static inline ogt_color_t 
ExtractField(uint8_t *p, unsigned int i_2bit_field) 
{
  return ( ( *p >> (OGT_FIELD_BITS*(i_2bit_field-1)) ) & OGT_FIELD_MASK );
}

/*****************************************************************************
 * ParseImage: parse the image part of the subtitle
 *****************************************************************************
 This part parses the subtitle graphical data and stores it in a more
 convenient structure for later rendering. 

 The image is encoded using two bits per pixel that select a palette
 entry except that value 0 starts a limited run-length encoding for
 color 0.  When 0 is seen, the next two bits encode one less than the
 number of pixels, so we can encode run lengths from 1 to 4. These get
 filled with the color in palette entry 0.

 The encoding of each line is padded to a whole number of bytes.  The
 first field is padded to an even byte length and the complete subtitle
 is padded to a 4-byte multiple that always include one zero byte at
 the end.

 However we'll transform this so that that the RLE is expanded and
 interlacing will also be removed. On output each pixel entry will by
 an 4-bit alpha (filling 8 bits), and 8-bit y, u, and v entry.

 *****************************************************************************/
static int 
ParseImage( decoder_t *p_dec, subpicture_t * p_spu )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    unsigned int i_field;  /* The subtitles are interlaced, are we on an
			      even or odd scanline?  */

    unsigned int i_row;    /* scanline row number */
    unsigned int i_column; /* scanline column number */

    unsigned int i_width  = p_sys->i_width;
    unsigned int i_height = p_sys->i_height;

    uint8_t *p_dest = (uint8_t *)p_spu->p_sys->p_data;

    uint8_t i_2bit_field;           /* The 2-bit field to sue in byte of *p.
				       Has value 0..4. */
    uint8_t i_pending_zero = 0;    /* number of pixels to fill with 
				      color zero 0..3 */
    ogt_color_t i_color;           /* current pixel color: 0..3 */
    uint8_t *p = p_sys->subtitle_data  + p_sys->i_image_offset;
    uint8_t *maxp = p + p_sys->i_image_length;

    dbg_print( (DECODE_DBG_CALL) , "width x height: %dx%d ",
	       i_width, i_height);

    if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
      printf("\n");

    for ( i_field=0; i_field < 2; i_field++ ) {
      i_2bit_field = 4;
      for ( i_row=i_field; i_row < i_height; i_row += 2 ) {
	for ( i_column=0; i_column<i_width; i_column++ ) {

	  if ( i_pending_zero ) {
	    /* We are in the middle of a RLE expansion, just decrement and 
	       fall through with current color value */
	    i_pending_zero--;
	    i_color = 0;
	  } else {
	    i_color = ExtractField( p, i_2bit_field );
	    advance_color_pointer;
	    if ( i_color == 0 ) {
	      i_pending_zero = ExtractField( p, i_2bit_field );
	      advance_color_pointer;
	      /* Fall through with i_color == 0 to output the first cell */
	    }
	  }

	  /* Color is 0-3. */
	  p_dest[i_row*i_width+i_column] = i_color;

	  if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
	    printf("%1d", i_color);

	}

	if (p_sys && (p_sys->i_debug & DECODE_DBG_IMAGE))
	  printf("\n");

	if ( i_2bit_field != 4 ) {
	  /* Lines are padded to complete bytes, ignore padding */
	  advance_color_pointer_byte;
	}
      }
      p = p_sys->subtitle_data + p_sys->i_image_offset 
	+ p_sys->second_field_offset;
    }

    if (p_sys && (p_sys->i_debug & DECODE_DBG_IMAGE)) {
      /* Dump out image not interlaced... */
      VCDSubDumpImage( p_dest, i_height, i_width );
    }

#ifdef HAVE_LIBPNG
    if (p_sys && (p_sys->i_debug & DECODE_DBG_PNG)) {
#define TEXT_COUNT 2
      /* Dump image to a file in PNG format. */
      char filename[300];
      png_text text_ptr[TEXT_COUNT];

      text_ptr[0].key = "Preparer";
      text_ptr[0].text = "VLC";
      text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
      text_ptr[1].key = "Description";
      text_ptr[1].text = "SVCD Subtitle";
      text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;

      snprintf(filename, 300, "%s%d.png", "/tmp/vlc-svcd-sub", p_sys->i_image);
      VCDSubDumpPNG( p_dest, p_dec, i_height, i_width, filename,
		     text_ptr, TEXT_COUNT );
    }
#endif /*HAVE_LIBPNG*/
    
    VCDInlinePalette( p_dest, p_sys, i_height, i_width );
    VCDSubHandleScaling( p_spu, p_dec );
    return VLC_SUCCESS;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
