/*****************************************************************************
 * parse.c: Philips OGT (SVCD subtitle) packet parser
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id$
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
  We do not have information on the subtitle format used on CVD's
  except the submux sample code and a couple of samples of dubious
  origin. Thus, this is the result of reading some code whose
  correctness is not known and some experimentation.
  
  CVD subtitles are different in several ways from SVCD OGT subtitles.
  Image comes first and metadata is at the end.  So that the metadata
  can be found easily, the subtitle packet starts with two bytes
  (everything is big-endian again) that give the total size of the
  subtitle data and the offset to the metadata - i.e. size of the
  image data plus the four bytes at the beginning.
 
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
*/

void E_(ParseHeader)( decoder_t *p_dec, uint8_t *p_buffer, block_t *p_block )
{
  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t *p = p_buffer+1;

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

  p_sys->i_image_offset = 4;
  p_sys->i_image_length = p_sys->metadata_offset - p_sys->i_image_offset;
  
  dbg_print(DECODE_DBG_PACKET, "total size: %d  image size: %d\n",
	    p_sys->i_spu_size, p_sys->i_image_length);

}

#define ExtractXY(x, y)		    \
  x = ((p[1]&0x0f)<<6) + (p[2]>>2); \
  y = ((p[2]&0x03)<<8) + p[3];


/* 
  We parse the metadata information here. 

  Although metadata information does not have to come in a fixed field
  order, every metadata field consists of a tag byte followed by
  parameters. In all cases known, the size including tag byte is
  exactly four bytes in length.
*/

void E_(ParseMetaInfo)( decoder_t *p_dec  )
{
  /* last packet in subtitle block. */
  
  decoder_sys_t *p_sys = p_dec->p_sys;
  uint8_t       *p     = p_sys->subtitle_data + p_sys->metadata_offset;
  uint8_t       *p_end = p + p_sys->metadata_length;
  
  dbg_print( (DECODE_DBG_PACKET),
	     "subtitle packet complete, size=%d", p_sys->i_spu );
  
  p_sys->state = SUBTITLE_BLOCK_COMPLETE;
  p_sys->i_image++;
  
  for ( ; p < p_end; p += 4 ) {
    
    switch ( p[0] ) {
      
    case 0x04:	/* subtitle duration in 1/90000ths of a second */
      {
	mtime_t i_duration = (p[1]<<16) + (p[2]<<8) + p[3];
	mtime_t i_duration_scale = config_GetInt( p_dec, MODULE_STRING 
				     "-duration-scaling" );
		
	dbg_print( DECODE_DBG_PACKET, 
		   "subtitle display duration %lu secs  (scaled %lu secs)", 
		   (long unsigned int) (i_duration / 90000), 
		   (long unsigned int) (i_duration * i_duration_scale / 90000)
		   );
	p_sys->i_duration = i_duration * i_duration_scale ;
	break;
      }
      
      
    case 0x0c:	/* unknown */
      dbg_print( DECODE_DBG_PACKET, 
		 "subtitle command unknown 0x%0x 0x%0x 0x%0x 0x%0x\n",
		 p[0], p[1], p[2], p[3]);
      break;
      
    case 0x17:	/* coordinates of subtitle upper left x, y position */
      ExtractXY(p_sys->i_x_start, p_sys->i_y_start);
      break;
      
    case 0x1f:	/* coordinates of subtitle bottom right x, y position */
      {
	int lastx;
	int lasty;
	ExtractXY(lastx, lasty);
	p_sys->i_width  = lastx - p_sys->i_x_start + 1;
	p_sys->i_height = lasty - p_sys->i_y_start + 1;
	dbg_print( DECODE_DBG_PACKET, 
		   "end position: (%d,%d): %.2x %.2x %.2x, w x h: %dx%d",
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
	
	p_sys->p_palette[v].s.y = p[1];
	p_sys->p_palette[v].s.u = p[2];
	p_sys->p_palette[v].s.v = p[3];
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
	p_sys->p_palette_highlight[v].s.y = p[1];
	p_sys->p_palette_highlight[v].s.u = p[2];
	p_sys->p_palette_highlight[v].s.v = p[3];
	break;
      }
      
    case 0x37:
      /* transparency for primary palette */
      p_sys->p_palette[0].s.t = p[3] & 0x0f;
      p_sys->p_palette[1].s.t = p[3] >> 4;
      p_sys->p_palette[2].s.t = p[2] & 0x0f;
      p_sys->p_palette[3].s.t = p[2] >> 4;
      
      dbg_print( DECODE_DBG_PACKET,
		 "transparency for primary palette 0..3: "
		 "0x%0x 0x%0x 0x%0x 0x%0x",
		 p_sys->p_palette[0].s.t,
		 p_sys->p_palette[1].s.t,
		 p_sys->p_palette[2].s.t,
		 p_sys->p_palette[3].s.t );
      
      break;
      
    case 0x3f:
      /* transparency for highlight palette */
      p_sys->p_palette_highlight[0].s.t = p[2] & 0x0f;
      p_sys->p_palette_highlight[1].s.t = p[2] >> 4;
      p_sys->p_palette_highlight[2].s.t = p[1] & 0x0f;
      p_sys->p_palette_highlight[3].s.t = p[1] >> 4;
      
      dbg_print( DECODE_DBG_PACKET,
		 "transparency for primary palette 0..3: "
		 "0x%0x 0x%0x 0x%0x 0x%0x",
		 p_sys->p_palette_highlight[0].s.t,
		 p_sys->p_palette_highlight[1].s.t,
		 p_sys->p_palette_highlight[2].s.t,
		 p_sys->p_palette_highlight[3].s.t );
      
      break;
      
    case 0x47:
      /* offset to start of even rows of interlaced image, we correct
	 to make it relative to i_image_offset (usually 4) */
      p_sys->first_field_offset =
	(p[2] << 8) + p[3] - p_sys->i_image_offset;
      dbg_print( DECODE_DBG_PACKET, 
		 "first_field_offset %d", p_sys->first_field_offset);
      break;
      
    case 0x4f:
      /* offset to start of odd rows of interlaced image, we correct
	 to make it relative to i_image_offset (usually 4) */
      p_sys->second_field_offset =
	(p[2] << 8) + p[3] - p_sys->i_image_offset;
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
    p_spu = vout_CreateSubPicture( p_sys->p_vout, p_sys->i_subpic_channel,
                                   MEMORY_SUBPICTURE );
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

    p_spu->pf_render  = VCDSubBlend;
    p_spu->pf_destroy = VCDSubDestroySPU;
    p_spu->p_sys->p_data = (uint8_t*)p_spu->p_sys + sizeof( subpicture_sys_t );

    p_spu->p_sys->i_x_end        = p_sys->i_x_start + p_sys->i_width - 1;
    p_spu->p_sys->i_y_end        = p_sys->i_y_start + p_sys->i_height - 1;

    p_spu->i_x        = p_sys->i_x_start 
      + config_GetInt( p_dec, MODULE_STRING "-horizontal-correct" );

    p_spu->p_sys->p_palette[0] = p_sys->p_palette[0];
    p_spu->p_sys->p_palette[1] = p_sys->p_palette[1];
    p_spu->p_sys->p_palette[2] = p_sys->p_palette[2];
    p_spu->p_sys->p_palette[3] = p_sys->p_palette[3];

    /* FIXME: use aspect ratio for x? */
    p_spu->i_x        = (p_spu->i_x * 3) / 4; 
    p_spu->i_y        = p_sys->i_y_start 
      + config_GetInt( p_dec, MODULE_STRING "-vertical-correct" );

    p_spu->i_width    = p_sys->i_width;
    p_spu->i_height   = p_sys->i_height;

    p_spu->i_start    = p_sys->i_pts;
    p_spu->i_stop     = p_sys->i_pts + (p_sys->i_duration);
    
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
    uint8_t *p = p_sys->subtitle_data  + p_sys->i_image_offset;
    uint8_t *maxp = p + p_sys->i_image_length;

    dbg_print( (DECODE_DBG_CALL) , "width x height: %dx%d",
	       i_width, i_height);

    if (p_sys && p_sys->i_debug & DECODE_DBG_IMAGE)	
      printf("\n");

    i_pending = 0;

    for ( i_field=0; i_field < 2; i_field++ ) {
      i_nibble_field = 2;  /* 4-bit pieces available in *p */

#if 0
      unsigned int i;
      int8_t *b=p;
      for (i=0; i< i_width * i_height; i++)
	printf ("%02x", b[i]);
      printf("\n");
#endif
    
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
      text_ptr[1].text = "CVD Subtitle";
      text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;

      snprintf(filename, 300, "%s%d.png", "/tmp/vlc-cvd-sub", p_sys->i_image);
      VCDSubDumpPNG( p_dest, p_dec, i_height, i_width, filename,
		     text_ptr, TEXT_COUNT );
    }
#endif /*HAVE_LIBPNG*/

    VCDSubHandleScaling( p_spu, p_dec );

    return VLC_SUCCESS;

}

