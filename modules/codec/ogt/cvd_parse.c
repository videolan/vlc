/*****************************************************************************
 * parse.c: Philips OGT (SVCD subtitle) packet parser
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cvd_parse.c,v 1.4 2003/12/30 04:43:52 rocky Exp $
 *
 * Authors: Rocky Bernstein 
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
#include "render.h"
#include "cvd.h"
#include "common.h"

/* An image color is a two-bit palette entry: 0..3 */ 
typedef uint8_t ogt_color_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  ParseImage         ( decoder_t *, subpicture_t * );

/*
  We do not have information on the subtitle format used on CVD's
  except the submux sample code and a couple of samples of dubious
  origin. Thus, this is the result of reading some code whose
  correctness is not known and some experimentation.
  
  CVD subtitles are different in severl ways from SVCD OGT subtitles.
  First, the image comes first and the metadata is at the end.  So
  that the metadata can be found easily, the subtitle packet starts
  with two bytes (everything is big-endian again) that give the total
  size of the subtitle data and the offset to the metadata - i.e. size
  of the image data plus the four bytes at the beginning.
 
  Image data comes interlaced is run-length encoded.  Each field is a
  four-bit nibble. Each nibble contains a two-bit repeat count and a
  two-bit color number so that up to three pixels can be described in
  four bits.  The function of a 0 repeat count is unknown; it might be
  used for RLE extension.  However when the full nibble is zero, the
  rest of the line is filled with the color value in the next nibble.
  It is unknown what happens if the color value is greater than three.
  The rest seems to use a 4-entries palette.  It is not impossible
  that the fill-line complete case above is not as described and the
  zero repeat count means fill line.  The sample code never produces
  this, so it may be untested.
 
  The metadata section does not follow a fixed pattern, every
  metadata item consists of a tag byte followed by parameters. In all
  cases known, the block (including the tag byte) is exactly four
  bytes in length.  Read the code for the rest.
*/

void E_(ParseHeader)( decoder_t *p_dec, uint8_t *p_buffer, block_t *p_block )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  u_int8_t *p = p_buffer+1;

  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_PACKET), 
	     "header: 0x%02x 0x%02x 0x%02x 0x%02x, 0x%02x, 0x%02x, size: %i",
	     p_buffer[0], p_buffer[1], p_buffer[2], p_buffer[3],
	     p_buffer[4], p_buffer[5],
	     p_block->i_buffer);
  
  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

  p_sys->i_pts    = p_block->i_pts;
  p_sys->i_spu_size = (p[0] << 8) + p[1] + 4; p += 2;

  /* FIXME: check data sanity */
  p_sys->metadata_offset = GETINT16(p);
  p_sys->metadata_length = p_sys->i_spu_size - p_sys->metadata_offset;

  p_sys->comp_image_offset = 4;
  p_sys->comp_image_length = p_sys->metadata_offset - p_sys->comp_image_offset;
  
  dbg_print(DECODE_DBG_PACKET, "total size: %d  image size: %d\n",
	    p_sys->i_spu_size, p_sys->comp_image_length);

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
    p_spu->i_stop     = p_sys->i_pts + (p_sys->i_duration * 5);
    
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

#define advance_color_byte_pointer					\
  p++;									\
  i_nibble_field = 2;							\
  /*									\
   * This is wrong, it may exceed maxp if it is the last, check		\
   * should be moved to use location or the algorithm changed to	\
   * that in vob2sub							\
  */									\
  if (p >= maxp) {							\
    msg_Warn( p_dec,							\
	      "broken subtitle - overflow while decoding "		\
	      " padding (%d,%d,%d)\n",					\
	      i_field, i_row, i_column );				\
    return VLC_EGENERIC;						\
  }									

#define CVD_FIELD_BITS (4)
#define CVD_FIELD_MASK  ((1<<CVD_FIELD_BITS) - 1) 

/* Get the next field - a 2-bit palette index and a run count.  To do
   this we use byte image pointer p, and i_nibble_field which
   indicates where we are in the byte.
*/
static inline uint8_t
ExtractField(uint8_t *p, uint8_t i_nibble_field) 
{
  return ( ( *p >> (CVD_FIELD_BITS*(i_nibble_field-1)) ) & CVD_FIELD_MASK );
}

/*****************************************************************************
 * ParseImage: parse the image part of the subtitle
 *****************************************************************************
 This part parses the subtitle graphical data and stores it in a more
 convenient structure for later rendering. 

 Image data comes interlaced and is run-length encoded (RLE). Each
 field is a four-bit nibbles that is further subdivided in a two-bit
 repeat count and a two-bit color number - up to three pixels can be
 described in four bits.  What a 0 repeat count means is unknown.  It
 might be used for RLE extension.  There is a special case of a 0
 repeat count though.  When the full nibble is zero, the rest of the
 line is filled with the color value in the next nibble.  It is
 unknown what happens if the color value is greater than three.  The
 rest seems to use a 4-entries palette.  It is not impossible that the
 fill-line complete case above is not as described and the zero repeat
 count means fill line.  The sample code never produces this, so it
 may be untested.

 However we'll transform this so that that the RLE is expanded and
 interlacing will also be removed. On output each pixel entry will by 
 a 4-bit alpha (filling 8 bits), and 8-bit y, u, and v entry.

 *****************************************************************************/
static int 
ParseImage( decoder_t *p_dec, subpicture_t * p_spu )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    uint8_t i_field;       /* The subtitles are interlaced, are we on an
			      even or odd scanline?  */
    unsigned int i_row;    /* scanline row number */
    unsigned int i_column; /* scanline column number */

    unsigned int i_width  = p_sys->i_width;
    unsigned int i_height = p_sys->i_height;

    uint8_t *p_dest = (uint8_t *)p_spu->p_sys->p_data;

    uint8_t i_nibble_field;    /* The 2-bit pixels remaining in byte of *p.
				  Has value 0..2. */
    vlc_bool_t b_filling;      /* Filling i_color to the of the line. */
    uint8_t i_pending = 0;     /* number of pixels to fill with 
				  color zero 0..3 */
    ogt_color_t i_color=0;     /* current pixel color: 0..3 */
    uint8_t *p = p_sys->subtitle_data  + p_sys->comp_image_offset;
    uint8_t *maxp = p + p_sys->comp_image_length;

    dbg_print( (DECODE_DBG_CALL) , "width x height: %dx%d",
	       i_width, i_height);

    if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
      printf("\n");

    i_pending = 0;

    for ( i_field=0; i_field < 2; i_field++ ) {
      i_nibble_field = 2;  /* 4-bit pieces available in *p */

      for ( i_row=i_field; i_row < i_height; i_row += 2 ) {
	b_filling   = VLC_FALSE;
	for ( i_column=0; i_column<i_width; i_column++ ) {
	  if ( i_pending ) {
	    /* We are in the middle of a RLE expansion, just decrement and 
	       fall through with current color value */
	    i_pending--;
	  } else if ( b_filling ) {
	    /* We are just filling to the end of line with one color, just
	       reuse current color value */
	  } else {
	    uint8_t i_val = ExtractField(p, i_nibble_field--);
	    if ( i_nibble_field == 0 ) {
	      advance_color_byte_pointer;
	    }
	    if ( i_val == 0 ) {
	      /* fill the rest of the line with next color */
	      i_color = ExtractField( p, i_nibble_field-- );
	      if ( i_nibble_field == 0 ) {
		p++;
		i_nibble_field=2;
		/*
		  This is wrong, it may exceed maxp if it is the
		  last, check should be moved to use location or the
		  algorithm changed to that in vob2sub
		*/
		if (p >= maxp) {
		  msg_Warn( p_dec, 
			    "broken subtitle - overflow while decoding "
			    " filling (%d,%d,%d)", 
			      i_field, i_row, i_column);
		  /* return VLC_EGENERIC; */
		}
	      }
	      b_filling = VLC_TRUE;
	    } else {
	      /* Normal case: get color and repeat count, 
		 this iteration will  output the first (or only) 
		 instance */
	      i_pending = (i_val >> 2);
	      i_color = i_val & 0x3;
	      /* This time counts against the total */
	      i_pending--;
	    }
	  }
	  /* Color is 0-3. */
	  p_dest[i_row*i_width+i_column] = i_color;
	  
	  if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
	    printf("%1d", i_color);
	  
	}
	
	if ( i_nibble_field == 1 ) {
	  advance_color_byte_pointer;
	}

	if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
	  printf("\n");
      }
    }

    /* Dump out image not interlaced... */
    if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE) {
      uint8_t *p = p_dest;
      printf("-------------------------------------\n++");
      for ( i_row=0; i_row < i_height; i_row ++ ) {
	for ( i_column=0; i_column<i_width; i_column++ ) {
	  printf("%1d", *p++ & 0x03);
	}
	printf("\n++");
      }
      printf("\n-------------------------------------\n");
    }

    VCDInlinePalette( p_dest, p_sys, i_height, i_width );

    /* The video is automatically scaled. However subtitle bitmaps
       assume a 1:1 aspect ratio. So we need to scale to compensate for
       or undo the effects of video output scaling. 
    */
    /* FIXME do the right scaling depending on vout. It may not be 4:3 */
    VCDSubScaleX( p_dec, p_spu, 3, 4 );

    /* To be finished...*/
    return VLC_SUCCESS;

}

