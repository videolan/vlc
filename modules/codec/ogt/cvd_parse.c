/*****************************************************************************
 * parse.c: Philips OGT (SVCD subtitle) packet parser
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cvd_parse.c,v 1.1 2003/12/28 04:51:52 rocky Exp $
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

/* An image color is a two-bit palette entry: 0..3 */ 
typedef uint8_t ogt_color_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  ParseImage         ( decoder_t *, subpicture_t * );

/*
 * We do not have information on the subtitle format used on CVD's
 * except the submux sample code and a couple of samples of dubious
 * origin. Thus, this is the result of reading some code whose
 * correctness is not known and some experimentation.
 * 
 * CVD subtitles present several differences compared to SVCD OGT
 * subtitles.  Firstly, the image comes first and the metadata is at
 * the end.  So that the metadata can be found easily, the subtitle
 * begins with two two-byte (everything is big-endian again) that
 * describe, the total size of the subtitle data and the offset to the
 * metadata (size of the image data plus the four bytes at the
 * beginning.
 *
 * Image data comes interlaced and uses RLE.  Coding is based in
 * four-bit nibbles that are further subdivided in a two-bit repeat
 * count and a two-bit color number so that up to three pixels can be
 * describe with a total of four bits.  The function of a 0 repeat
 * count is unknown.  It might be used for RLE extension.  There is a
 * special case, though.  When the full nibble is zero, the rest of
 * the line is filled with the color value in the next nibble.  It is
 * unknown what happens if the color value is greater than three.  The
 * rest seems to use a 4-entries palette.  It is not impossible that
 * the fill-line complete case above is not as described and the zero
 * repeat count means fill line.  The sample code never produces this,
 * so it may be untested.
 *
 * The metadata section does not follow a fixed pattern, every
 * metadata item consists of a tag byte followed by parameters. In all
 * cases known, the block (including the tag byte) is exactly four
 * bytes in length.  Read the code for the rest.
 */

/* FIXME: do we really need p_buffer and p? 
   Can't all of thes _offset's and _lengths's get removed? 
*/
void E_(ParseHeader)( decoder_t *p_dec, uint8_t *p_buffer, block_t *p_block )
{
  decoder_sys_t *p_sys = p_dec->p_sys;

  dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

  /* To be finished...*/
  return;
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

    dbg_print( (DECODE_DBG_CALL|DECODE_DBG_EXT) , "");

    /* To be completed... */
    return;

}

/* Advance pointer to image pointer, update internal i_remaining counter
   and check that we haven't goine too far  in the image data. */
#define advance_color_pointer_byte					\
  p++;									\
  i_remaining=4;							\
  if (p >= maxp) {							\
    msg_Warn( p_dec,							\
	      "broken subtitle - tried to access beyond end "		\
	      "in image extraction");					\
    return VLC_EGENERIC;						\
  }									\

#define advance_color_pointer						\
  i_remaining--;							\
  if ( i_remaining == 0 ) {						\
    advance_color_pointer_byte;						\
  }									

/* Get the next field - either a palette index or a RLE count for
   color 0.  To do this we use byte image pointer p, and i_remaining
   which indicates where we are in the byte.
*/
static inline ogt_color_t 
ExtractField(uint8_t *p, unsigned int i_remaining) 
{
  return ( ( *p >> 2*(i_remaining-1) ) & 0x3 );
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
 an 8-bit alpha, y, u, and v entry.

 *****************************************************************************/
static int 
ParseImage( decoder_t *p_dec, subpicture_t * p_spu )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    dbg_print( (DECODE_DBG_CALL) , "");
    /* To be finished...*/
    return VLC_EGENERIC;

}

