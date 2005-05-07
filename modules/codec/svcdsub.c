/*****************************************************************************
 * svcdsub.c : Overlay Graphics Text (SVCD subtitles) decoder
 *****************************************************************************
 * Copyright (C) 2003, 2004 VideoLAN
 * $Id$
 *
 * Authors: Rocky Bernstein
 *          Gildas Bazin <gbazin@videolan.org>
 *          Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>

#include "vlc_bits.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  DecoderOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );
static void DecoderClose  ( vlc_object_t * );

#define DEBUG_TEXT \
     "If nonzero, this gives additional debug information." \

#define DEBUG_LONGTEXT \
    "This integer when viewed in binary is a debugging mask\n" \
    "calls                 1\n" \
    "packet assembly info  2\n"

vlc_module_begin();
    set_description( _("Philips OGT (SVCD subtitle) decoder") );
    set_shortname( N_("SVCD subtitles"));
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    set_capability( "decoder", 50 );
    set_callbacks( DecoderOpen, DecoderClose );

    add_integer ( MODULE_STRING "-debug", 0, NULL,
                  DEBUG_TEXT, DEBUG_LONGTEXT, VLC_TRUE );

    add_submodule();
    set_description( _("Philips OGT (SVCD subtitle) packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( PacketizerOpen, DecoderClose );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *, block_t ** );
static block_t *Packetize  ( decoder_t *, block_t ** );
static block_t *Reassemble ( decoder_t *, block_t * );
static void ParseHeader( decoder_t *, block_t * );
static subpicture_t *DecodePacket( decoder_t *, block_t * );
static void SVCDSubRenderImage( decoder_t *, block_t *, subpicture_region_t * );

#define DECODE_DBG_CALL        1 /* calls */
#define DECODE_DBG_PACKET      2 /* packet assembly info */

#define GETINT16(p) ( (p[0] <<  8) +   p[1] )  ; p +=2;

#define GETINT32(p) ( (p[0] << 24) +  (p[1] << 16) +    \
                      (p[2] <<  8) +  (p[3]) ) ; p += 4;

typedef enum  {
  SUBTITLE_BLOCK_EMPTY    = 0,
  SUBTITLE_BLOCK_PARTIAL  = 1,
  SUBTITLE_BLOCK_COMPLETE = 2
} packet_state_t;

#ifndef DECODE_DEBUG
#define DECODE_DEBUG 1
#endif
#if DECODE_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_sys && p_sys->i_debug & mask) \
     msg_Dbg(p_dec, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...)
#endif

struct decoder_sys_t
{
  int      i_debug;       /* debugging mask */

  packet_state_t i_state; /* data-gathering state for this subtitle */

  block_t  *p_spu;        /* Bytes of the packet. */

  uint16_t i_image;       /* image number in the subtitle stream */
  uint8_t  i_packet;      /* packet number for above image number */

  int     i_spu_size;     /* goal for subtitle_data_pos while gathering,
                             size of used subtitle_data later */

  uint16_t i_image_offset;      /* offset from subtitle_data to compressed
                                   image data */
  int i_image_length;           /* size of the compressed image data */
  int second_field_offset;      /* offset of odd raster lines */
  int metadata_offset;          /* offset to data describing the image */
  int metadata_length;          /* length of metadata */

  mtime_t i_duration;   /* how long to display the image, 0 stands
                           for "until next subtitle" */

  uint16_t i_x_start, i_y_start; /* position of top leftmost pixel of
                                    image when displayed */
  uint16_t i_width, i_height;    /* dimensions in pixels of image */

  uint8_t p_palette[4][4];       /* Palette of colors used in subtitle */
};

/*****************************************************************************
 * DecoderOpen: open/initialize the svcdsub decoder.
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'o','g','t',' ' ) )
    {
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys = calloc( 1, sizeof( decoder_sys_t ) );
    p_sys->i_debug       = config_GetInt( p_this, MODULE_STRING "-debug" );

    p_sys->i_image       = -1;

    p_sys->i_state = SUBTITLE_BLOCK_EMPTY;
    p_sys->p_spu   = NULL;

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 'o','g','t',' ' ) );

    p_dec->pf_decode_sub = Decode;
    p_dec->pf_packetize  = Packetize;

    dbg_print( (DECODE_DBG_CALL) , "");
    return VLC_SUCCESS;
}

/*****************************************************************************
 * PacketizerOpen: open/initialize the svcdsub packetizer.
 *****************************************************************************/
static int PacketizerOpen( vlc_object_t *p_this )
{
    if( DecoderOpen( p_this ) != VLC_SUCCESS ) return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecoderClose: closes the svcdsub decoder/packetizer.
 *****************************************************************************/
void DecoderClose( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_spu ) block_ChainRelease( p_sys->p_spu );
    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block, *p_spu;
    decoder_sys_t *p_sys = p_dec->p_sys;

    dbg_print( (DECODE_DBG_CALL) , "");

    if( pp_block == NULL || *pp_block == NULL ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL;

    if( !(p_spu = Reassemble( p_dec, p_block )) ) return NULL;

    /* Parse and decode */
    return DecodePacket( p_dec, p_spu );
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block, *p_spu;

    if( pp_block == NULL || *pp_block == NULL ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL;

    if( !(p_spu = Reassemble( p_dec, p_block )) ) return NULL;

    p_spu->i_dts = p_spu->i_pts;
    p_spu->i_length = 0;

    return p_spu;
}

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
#define SPU_HEADER_LEN 5

static block_t *Reassemble( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buffer;
    uint16_t i_expected_image;
    uint8_t  i_packet, i_expected_packet;

    if( p_block->i_buffer < SPU_HEADER_LEN )
    {
        msg_Dbg( p_dec, "invalid packet header (size %d < %d)" ,
                 p_block->i_buffer, SPU_HEADER_LEN );
        block_Release( p_block );
        return NULL;
    }

    p_buffer = p_block->p_buffer;

    /* Attach to our input thread and see if subtitle is selected. */
    {
        vlc_object_t * p_input;
        vlc_value_t val;

        p_input = vlc_object_find( p_dec, VLC_OBJECT_INPUT, FIND_PARENT );

        if( !p_input ) return NULL;

        if( var_Get( p_input, "sub-track", &val ) )
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

    if( p_sys->i_state == SUBTITLE_BLOCK_EMPTY )
    {
        i_expected_image  = p_sys->i_image + 1;
        i_expected_packet = 0;
    }
    else
    {
        i_expected_image  = p_sys->i_image;
        i_expected_packet = p_sys->i_packet + 1;
    }

    p_buffer += 2;

    if( *p_buffer & 0x80 )
    {
        p_sys->i_state = SUBTITLE_BLOCK_COMPLETE;
        i_packet       = *p_buffer++ & 0x7F;
    }
    else
    {
        p_sys->i_state = SUBTITLE_BLOCK_PARTIAL;
        i_packet       = *p_buffer++;
    }

    p_sys->i_image = GETINT16(p_buffer);

    if( p_sys->i_image != i_expected_image )
    {
        msg_Warn( p_dec, "expected subtitle image %u but found %u",
                  i_expected_image, p_sys->i_image );
    }

    if( i_packet != i_expected_packet )
    {
        msg_Warn( p_dec, "expected subtitle image packet %u but found %u",
                  i_expected_packet, i_packet );
    }

    p_block->p_buffer += SPU_HEADER_LEN;
    p_block->i_buffer -= SPU_HEADER_LEN;

    p_sys->i_packet = i_packet;
    /* First packet in the subtitle block */
    if( !p_sys->i_packet ) ParseHeader( p_dec, p_block );

    block_ChainAppend( &p_sys->p_spu, p_block );

    if( p_sys->i_state == SUBTITLE_BLOCK_COMPLETE )
    {
        block_t *p_spu = block_ChainGather( p_sys->p_spu );

        if( p_spu->i_buffer != p_sys->i_spu_size )
        {
            msg_Warn( p_dec, "subtitle packets size=%d should be %d",
                      p_spu->i_buffer, p_sys->i_spu_size );
        }

	dbg_print( (DECODE_DBG_PACKET),
                 "subtitle packet complete, size=%d", p_spu->i_buffer );

        p_sys->i_state = SUBTITLE_BLOCK_EMPTY;
        p_sys->p_spu = 0;
        return p_spu;
    }

    return NULL;
}

/******************************************************************************
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
******************************************************************************/
static void ParseHeader( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p = p_block->p_buffer;
    uint8_t i_options, i_options2, i_cmd, i_cmd_arg;
    int i;

    p_sys->i_spu_size = GETINT16(p);
    i_options  = *p++;
    i_options2 = *p++;

    if( i_options & 0x08 ) { p_sys->i_duration = GETINT32(p); }
    else p_sys->i_duration = 0; /* Ephemer subtitle */
    p_sys->i_duration *= 100 / 9;

    p_sys->i_x_start = GETINT16(p);
    p_sys->i_y_start = GETINT16(p);
    p_sys->i_width   = GETINT16(p);
    p_sys->i_height  = GETINT16(p);

    for( i = 0; i < 4; i++ )
    {
        p_sys->p_palette[i][0] = *p++; /* Y */
        p_sys->p_palette[i][2] = *p++; /* Cr / V */
        p_sys->p_palette[i][1] = *p++; /* Cb / U */
        p_sys->p_palette[i][3] = *p++; /* T */
    }

    i_cmd = *p++;
    /* We do not really know this, FIXME */
    if( i_cmd ) {i_cmd_arg = GETINT32(p);}

    /* Actually, this is measured against a different origin, so we have to
     * adjust it */
    p_sys->second_field_offset = GETINT16(p);
    p_sys->i_image_offset  = p - p_block->p_buffer;
    p_sys->i_image_length  = p_sys->i_spu_size - p_sys->i_image_offset;
    p_sys->metadata_length = p_sys->i_image_offset;

  if (p_sys && p_sys->i_debug & DECODE_DBG_PACKET) 
  {
      msg_Dbg( p_dec, "x-start: %d, y-start: %d, width: %d, height %d, "
	       "spu size: %d, duration: %lu (d:%d p:%d)",
	       p_sys->i_x_start, p_sys->i_y_start, 
	       p_sys->i_width, p_sys->i_height, 
	       p_sys->i_spu_size, (long unsigned int) p_sys->i_duration,
	       p_sys->i_image_length, p_sys->i_image_offset);
      
      for( i = 0; i < 4; i++ )
      {
          msg_Dbg( p_dec, "palette[%d]= T: %2x, Y: %2x, u: %2x, v: %2x", i,
		   p_sys->p_palette[i][3], p_sys->p_palette[i][0], 
		   p_sys->p_palette[i][1], p_sys->p_palette[i][2] );
      }
  }
}

/*****************************************************************************
 * DecodePacket: parse and decode an subtitle packet
 *****************************************************************************
 * This function parses and decodes an SPU packet and, if valid, returns a
 * subpicture.
 *****************************************************************************/
static subpicture_t *DecodePacket( decoder_t *p_dec, block_t *p_data )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t  *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;
    int i;

    /* Allocate the subpicture internal data. */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu ) return NULL;

    p_spu->i_x = p_sys->i_x_start;
    p_spu->i_y = p_sys->i_y_start;
    p_spu->i_start = p_data->i_pts;
    p_spu->i_stop  = p_data->i_pts + p_sys->i_duration;
    p_spu->b_ephemer = VLC_TRUE;

    /* Create new subtitle region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','P');

    /**
       The video on which the subtitle sits, is scaled, probably
       4:3. However subtitle bitmaps assume an 1:1 aspect ratio.

       FIXME: We should get the video aspect ratio from somewhere.
       Two candidates are the video and the other possibility would be
       the access module.
    */
    fmt.i_aspect = VOUT_ASPECT_FACTOR;

    fmt.i_width = fmt.i_visible_width = p_sys->i_width;
    fmt.i_height = fmt.i_visible_height = p_sys->i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
    if( !p_region )
    {
        msg_Err( p_dec, "cannot allocate SVCD subtitle region" );
        //goto error;
    }

    p_region->fmt.i_aspect = VOUT_ASPECT_FACTOR;
    
    p_spu->p_region = p_region;
    p_region->i_x = p_region->i_y = 0;

    /* Build palette */
    fmt.p_palette->i_entries = 4;
    for( i = 0; i < fmt.p_palette->i_entries; i++ )
    {
        fmt.p_palette->palette[i][0] = p_sys->p_palette[i][0];
        fmt.p_palette->palette[i][1] = p_sys->p_palette[i][1];
        fmt.p_palette->palette[i][2] = p_sys->p_palette[i][2];
        fmt.p_palette->palette[i][3] = p_sys->p_palette[i][3];
    }

    SVCDSubRenderImage( p_dec, p_data, p_region );

    return p_spu;
}

/*****************************************************************************
 * SVCDSubRenderImage: reorders bytes of image data in subpicture region.
 *****************************************************************************

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
 interlacing will also be removed.
 *****************************************************************************/
static void SVCDSubRenderImage( decoder_t *p_dec, block_t *p_data,
				subpicture_region_t *p_region )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_dest = p_region->picture.Y_PIXELS;
    int i_field;            /* The subtitles are interlaced */
    int i_row, i_column;    /* scanline row/column number */
    uint8_t i_color, i_count;
    bs_t bs;

    bs_init( &bs, p_data->p_buffer + p_sys->i_image_offset,
             p_data->i_buffer - p_sys->i_image_offset );

    for( i_field = 0; i_field < 2; i_field++ )
    {
        for( i_row = i_field; i_row < p_sys->i_height; i_row += 2 )
        {
            for( i_column = 0; i_column < p_sys->i_width; i_column++ )
            {
                i_color = bs_read( &bs, 2 );
                if( i_color == 0 && (i_count = bs_read( &bs, 2 )) )
                {
                    i_count = __MIN( i_count, p_sys->i_width - i_column );
                    memset( &p_dest[i_row * p_region->picture.Y_PITCH +
                                    i_column], 0, i_count + 1 );
                    i_column += i_count;
                    continue;
                }

                p_dest[i_row * p_region->picture.Y_PITCH + i_column] = i_color;
            }

            bs_align( &bs );
        }

        /* odd field */
        bs_init( &bs, p_data->p_buffer + p_sys->i_image_offset +
                 p_sys->second_field_offset,
                 p_data->i_buffer - p_sys->i_image_offset -
                 p_sys->second_field_offset );
    }
}
