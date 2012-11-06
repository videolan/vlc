/*****************************************************************************
 * cvdsub.c : CVD Subtitle decoder
 *****************************************************************************
 * Copyright (C) 2003, 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Rocky Bernstein
 *          Gildas Bazin <gbazin@videolan.org>
 *          Julio Sanchez Fernandez (http://subhandler.sourceforge.net)
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include <vlc_bits.h>

#define DEBUG_CVDSUB 1

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  DecoderOpen   ( vlc_object_t * );
static int  PacketizerOpen( vlc_object_t * );
static void DecoderClose  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("CVD subtitle decoder") )
    set_capability( "decoder", 50 )
    set_callbacks( DecoderOpen, DecoderClose )

    add_submodule ()
    set_description( N_("Chaoji VCD subtitle packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( PacketizerOpen, DecoderClose )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *, block_t ** );
static block_t *Packetize  ( decoder_t *, block_t ** );
static block_t *Reassemble ( decoder_t *, block_t * );
static void ParseMetaInfo  ( decoder_t *, block_t * );
static void ParseHeader    ( decoder_t *, block_t * );
static subpicture_t *DecodePacket( decoder_t *, block_t * );
static void RenderImage( decoder_t *, block_t *, subpicture_region_t * );

#define SUBTITLE_BLOCK_EMPTY 0
#define SUBTITLE_BLOCK_PARTIAL 1
#define SUBTITLE_BLOCK_COMPLETE 2

struct decoder_sys_t
{
  int      b_packetizer;

  int      i_state;    /* data-gathering state for this subtitle */

  block_t  *p_spu;   /* Bytes of the packet. */

  size_t   i_spu_size;     /* goal for subtitle_data_pos while gathering,
                             size of used subtitle_data later */

  uint16_t i_image_offset;      /* offset from subtitle_data to compressed
                                   image data */
  size_t i_image_length;           /* size of the compressed image data */
  size_t first_field_offset;       /* offset of even raster lines */
  size_t second_field_offset;      /* offset of odd raster lines */
  size_t metadata_offset;          /* offset to data describing the image */
  size_t metadata_length;          /* length of metadata */

  mtime_t i_duration;   /* how long to display the image, 0 stands
                           for "until next subtitle" */

  uint16_t i_x_start, i_y_start; /* position of top leftmost pixel of
                                    image when displayed */
  uint16_t i_width, i_height;    /* dimensions in pixels of image */

  uint8_t p_palette[4][4];       /* Palette of colors used in subtitle */
  uint8_t p_palette_highlight[4][4];
};

/*****************************************************************************
 * DecoderOpen: open/initialize the cvdsub decoder.
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_CVD )
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->b_packetizer  = false;

    p_sys->i_state = SUBTITLE_BLOCK_EMPTY;
    p_sys->p_spu   = NULL;

    p_dec->pf_decode_sub = Decode;
    p_dec->pf_packetize  = Packetize;

    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_YUVP;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * PacketizerOpen: open/initialize the cvdsub packetizer.
 *****************************************************************************/
static int PacketizerOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( DecoderOpen( p_this ) != VLC_SUCCESS ) return VLC_EGENERIC;

    p_dec->p_sys->b_packetizer = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecoderClose: closes the cvdsub decoder/packetizer.
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

 Data for single screen subtitle may come in several non-contiguous
 packets of a stream. This routine is called when the next packet in
 the stream comes in. The job of this routine is to parse the header,
 if this is the beginning, and combine the packets into one complete
 subtitle unit.

 If everything is complete, we will return a block. Otherwise return
 NULL.

 *****************************************************************************/
#define SPU_HEADER_LEN 1

static block_t *Reassemble( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block->i_buffer < SPU_HEADER_LEN )
    {
        msg_Dbg( p_dec, "invalid packet header (size %zu < %u)" ,
                 p_block->i_buffer, SPU_HEADER_LEN );
        block_Release( p_block );
        return NULL;
    }

    /* From the scant data on the format, there is only only way known
     * to detect the first packet in a subtitle.  The first packet
     * seems to have a valid PTS while later packets for the same
     * image don't. */
    if( p_sys->i_state == SUBTITLE_BLOCK_EMPTY && p_block->i_pts <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "first packet expected but no PTS present");
        return NULL;
    }

    p_block->p_buffer += SPU_HEADER_LEN;
    p_block->i_buffer -= SPU_HEADER_LEN;

    /* First packet in the subtitle block */
    if( p_sys->i_state == SUBTITLE_BLOCK_EMPTY ) ParseHeader( p_dec, p_block );

    block_ChainAppend( &p_sys->p_spu, p_block );
    p_sys->p_spu = block_ChainGather( p_sys->p_spu );

    if( p_sys->p_spu->i_buffer >= p_sys->i_spu_size )
    {
        block_t *p_spu = p_sys->p_spu;

        if( p_spu->i_buffer != p_sys->i_spu_size )
        {
            msg_Warn( p_dec, "SPU packets size=%zu should be %zu",
                      p_spu->i_buffer, p_sys->i_spu_size );
        }

        msg_Dbg( p_dec, "subtitle packet complete, size=%zuu", p_spu->i_buffer);

        ParseMetaInfo( p_dec, p_spu );

        p_sys->i_state = SUBTITLE_BLOCK_EMPTY;
        p_sys->p_spu = 0;
        return p_spu;
    }
    else
    {
        /* Not last block in subtitle, so wait for another. */
        p_sys->i_state = SUBTITLE_BLOCK_PARTIAL;
    }

    return NULL;
}

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

static void ParseHeader( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p = p_block->p_buffer;

    p_sys->i_spu_size = (p[0] << 8) + p[1] + 4; p += 2;

    /* FIXME: check data sanity */
    p_sys->metadata_offset = (p[0] <<  8) +   p[1]; p +=2;
    p_sys->metadata_length = p_sys->i_spu_size - p_sys->metadata_offset;

    p_sys->i_image_offset = 4;
    p_sys->i_image_length = p_sys->metadata_offset - p_sys->i_image_offset;

#ifdef DEBUG_CVDSUB
    msg_Dbg( p_dec, "total size: %zu  image size: %zu",
             p_sys->i_spu_size, p_sys->i_image_length );
#endif
}

/*
  We parse the metadata information here.

  Although metadata information does not have to come in a fixed field
  order, every metadata field consists of a tag byte followed by
  parameters. In all cases known, the size including tag byte is
  exactly four bytes in length.
*/

#define ExtractXY(x, y) x = ((p[1]&0x0f)<<6) + (p[2]>>2); \
                        y = ((p[2]&0x03)<<8) + p[3];

static void ParseMetaInfo( decoder_t *p_dec, block_t *p_spu  )
{
    /* Last packet in subtitle block. */

    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t       *p     = p_spu->p_buffer + p_sys->metadata_offset;
    uint8_t       *p_end = p + p_sys->metadata_length;

    for( ; p < p_end; p += 4 )
    {
        switch( p[0] )
        {
        case 0x04: /* subtitle duration in 1/90000ths of a second */
            p_sys->i_duration = (p[1]<<16) + (p[2]<<8) + p[3];

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "subtitle display duration %lu secs",
                     (long unsigned int)(p_sys->i_duration / 90000) );
#endif
            p_sys->i_duration *= 100 / 9;
            break;

        case 0x0c: /* unknown */
#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "subtitle command unknown 0x%0x 0x%0x 0x%0x 0x%0x",
                     (int)p[0], (int)p[1], (int)p[2], (int)p[3] );
#endif
            break;

        case 0x17: /* coordinates of subtitle upper left x, y position */
            ExtractXY(p_sys->i_x_start, p_sys->i_y_start);

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "start position (%d,%d)",
                     p_sys->i_x_start, p_sys->i_y_start );
#endif
            break;

        case 0x1f: /* coordinates of subtitle bottom right x, y position */
        {
            int lastx;
            int lasty;
            ExtractXY(lastx, lasty);
            p_sys->i_width  = lastx - p_sys->i_x_start + 1;
            p_sys->i_height = lasty - p_sys->i_y_start + 1;

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "end position (%d,%d), w x h: %dx%d",
                     lastx, lasty, p_sys->i_width, p_sys->i_height );
#endif
            break;
        }

        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        {
            uint8_t v = p[0] - 0x24;

#ifdef DEBUG_CVDSUB
            /* Primary Palette */
            msg_Dbg( p_dec, "primary palette %d (y,u,v): (0x%0x,0x%0x,0x%0x)",
                     (int)v, (int)p[1], (int)p[2], (int)p[3] );
#endif

            p_sys->p_palette[v][0] = p[1]; /* Y */
            p_sys->p_palette[v][1] = p[3]; /* Cr / V */
            p_sys->p_palette[v][2] = p[2]; /* Cb / U */
            break;
        }

        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f:
        {
            uint8_t v = p[0] - 0x2c;

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec,"highlight palette %d (y,u,v): (0x%0x,0x%0x,0x%0x)",
                     (int)v, (int)p[1], (int)p[2], (int)p[3] );
#endif

            /* Highlight Palette */
            p_sys->p_palette_highlight[v][0] = p[1]; /* Y */
            p_sys->p_palette_highlight[v][1] = p[3]; /* Cr / V */
            p_sys->p_palette_highlight[v][2] = p[2]; /* Cb / U */
            break;
        }

        case 0x37:
            /* transparency for primary palette */
            p_sys->p_palette[0][3] = (p[3] & 0x0f) << 4;
            p_sys->p_palette[1][3] = (p[3] >> 4) << 4;
            p_sys->p_palette[2][3] = (p[2] & 0x0f) << 4;
            p_sys->p_palette[3][3] = (p[2] >> 4) << 4;

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "transparency for primary palette 0..3: "
                     "0x%0x 0x%0x 0x%0x 0x%0x",
                     (int)p_sys->p_palette[0][3], (int)p_sys->p_palette[1][3],
                     (int)p_sys->p_palette[2][3], (int)p_sys->p_palette[3][3]);
#endif
            break;

        case 0x3f:
            /* transparency for highlight palette */
            p_sys->p_palette_highlight[0][3] = (p[2] & 0x0f) << 4;
            p_sys->p_palette_highlight[1][3] = (p[2] >> 4) << 4;
            p_sys->p_palette_highlight[2][3] = (p[1] & 0x0f) << 4;
            p_sys->p_palette_highlight[3][3] = (p[1] >> 4) << 4;

#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "transparency for highlight palette 0..3: "
                     "0x%0x 0x%0x 0x%0x 0x%0x",
                     (int)p_sys->p_palette_highlight[0][3],
                     (int)p_sys->p_palette_highlight[1][3],
                     (int)p_sys->p_palette_highlight[2][3],
                     (int)p_sys->p_palette_highlight[3][3] );
#endif
            break;

        case 0x47:
            /* offset to start of even rows of interlaced image, we correct
             * to make it relative to i_image_offset (usually 4) */
            p_sys->first_field_offset =
                (p[2] << 8) + p[3] - p_sys->i_image_offset;
#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "1st_field_offset %zu",
                     p_sys->first_field_offset );
#endif
            break;

        case 0x4f:
            /* offset to start of odd rows of interlaced image, we correct
             * to make it relative to i_image_offset (usually 4) */
            p_sys->second_field_offset =
                (p[2] << 8) + p[3] - p_sys->i_image_offset;
#ifdef DEBUG_CVDSUB
            msg_Dbg( p_dec, "2nd_field_offset %zu",
                     p_sys->second_field_offset);
#endif
            break;

        default:
#ifdef DEBUG_CVDSUB
            msg_Warn( p_dec, "unknown sequence in control header "
                      "0x%0x 0x%0x 0x%0x 0x%0x", p[0], p[1], p[2], p[3]);
#endif
        }
    }
}

/*****************************************************************************
 * DecodePacket: parse and decode an SPU packet
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
    video_palette_t palette;
    int i;

    /* Allocate the subpicture internal data. */
    p_spu = decoder_NewSubpicture( p_dec, NULL );
    if( !p_spu ) return NULL;

    p_spu->i_start = p_data->i_pts;
    p_spu->i_stop  = p_data->i_pts + p_sys->i_duration;
    p_spu->b_ephemer = true;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVP;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_sys->i_width;
    fmt.i_height = fmt.i_visible_height = p_sys->i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.p_palette = &palette;
    fmt.p_palette->i_entries = 4;
    for( i = 0; i < fmt.p_palette->i_entries; i++ )
    {
        fmt.p_palette->palette[i][0] = p_sys->p_palette[i][0];
        fmt.p_palette->palette[i][1] = p_sys->p_palette[i][1];
        fmt.p_palette->palette[i][2] = p_sys->p_palette[i][2];
        fmt.p_palette->palette[i][3] = p_sys->p_palette[i][3];
    }

    p_region = subpicture_region_New( &fmt );
    if( !p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        decoder_DeleteSubpicture( p_dec, p_spu );
        return NULL;
    }

    p_spu->p_region = p_region;
    p_region->i_x = p_sys->i_x_start;
    p_region->i_x = p_region->i_x * 3 / 4; /* FIXME: use aspect ratio for x? */
    p_region->i_y = p_sys->i_y_start;

    RenderImage( p_dec, p_data, p_region );

    return p_spu;
}

/*****************************************************************************
 * ParseImage: parse and render the image part of the subtitle
 *****************************************************************************
 This part parses the subtitle graphical data and renders it.

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
static void RenderImage( decoder_t *p_dec, block_t *p_data,
                         subpicture_region_t *p_region )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_dest = p_region->p_picture->Y_PIXELS;
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
                uint8_t i_val = bs_read( &bs, 4 );

                if( i_val == 0 )
                {
                    /* Fill the rest of the line with next color */
                    i_color = bs_read( &bs, 4 );

                    memset( &p_dest[i_row * p_region->p_picture->Y_PITCH +
                                    i_column], i_color,
                            p_sys->i_width - i_column );
                    i_column = p_sys->i_width;
                    continue;
                }
                else
                {
                    /* Normal case: get color and repeat count */
                    i_count = (i_val >> 2);
                    i_color = i_val & 0x3;

                    i_count = __MIN( i_count, p_sys->i_width - i_column );

                    memset( &p_dest[i_row * p_region->p_picture->Y_PITCH +
                                    i_column], i_color, i_count );
                    i_column += i_count - 1;
                    continue;
                }
            }

            bs_align( &bs );
        }
    }
}
