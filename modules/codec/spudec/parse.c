/*****************************************************************************
 * parse.c: SPU parser
 *****************************************************************************
 * Copyright (C) 2000-2001, 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include "spudec.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  ParseControlSeq( decoder_t *, subpicture_t *, subpicture_data_t *);
static int  ParseRLE       ( decoder_t *, subpicture_t *, subpicture_data_t *);
static void Render         ( decoder_t *, subpicture_t *, subpicture_data_t *);

/*****************************************************************************
 * AddNibble: read a nibble from a source packet and add it to our integer.
 *****************************************************************************/
static inline unsigned int AddNibble( unsigned int i_code,
                                      uint8_t *p_src, int *pi_index )
{
    if( *pi_index & 0x1 )
    {
        return( i_code << 4 | ( p_src[(*pi_index)++ >> 1] & 0xf ) );
    }
    else
    {
        return( i_code << 4 | p_src[(*pi_index)++ >> 1] >> 4 );
    }
}

/*****************************************************************************
 * ParsePacket: parse an SPU packet and send it to the video output
 *****************************************************************************
 * This function parses the SPU packet and, if valid, sends it to the
 * video output.
 *****************************************************************************/
subpicture_t * E_(ParsePacket)( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_data_t *p_spu_data;
    subpicture_t *p_spu;

    /* Allocate the subpicture internal data. */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu ) return NULL;

    /* Rationale for the "p_spudec->i_rle_size * 4": we are going to
     * expand the RLE stuff so that we won't need to read nibbles later
     * on. This will speed things up a lot. Plus, we'll only need to do
     * this stupid interlacing stuff once. */
    p_spu_data = malloc( sizeof(subpicture_data_t) + 4 * p_sys->i_rle_size );
    p_spu_data->p_data = (uint8_t *)p_spu_data + sizeof(subpicture_data_t);
    p_spu_data->b_palette = VLC_FALSE;
    p_spu_data->b_auto_crop = VLC_FALSE;
    p_spu_data->i_y_top_offset = 0;
    p_spu_data->i_y_bottom_offset = 0;

    p_spu_data->pi_alpha[0] = 0x00;
    p_spu_data->pi_alpha[1] = 0x0f;
    p_spu_data->pi_alpha[2] = 0x0f;
    p_spu_data->pi_alpha[3] = 0x0f;

    /* Get display time now. If we do it later, we may miss the PTS. */
    p_spu_data->i_pts = p_sys->i_pts;

    p_spu->i_original_picture_width =
        p_dec->fmt_in.subs.spu.i_original_frame_width;
    p_spu->i_original_picture_height =
        p_dec->fmt_in.subs.spu.i_original_frame_height;

    /* Getting the control part */
    if( ParseControlSeq( p_dec, p_spu, p_spu_data ) )
    {
        /* There was a parse error, delete the subpicture */
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        return NULL;
    }

    /* We try to display it */
    if( ParseRLE( p_dec, p_spu, p_spu_data ) )
    {
        /* There was a parse error, delete the subpicture */
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        return NULL;
    }

    msg_Dbg( p_dec, "total size: 0x%x, RLE offsets: 0x%x 0x%x",
             p_sys->i_spu_size,
             p_spu_data->pi_offset[0], p_spu_data->pi_offset[1] );

    Render( p_dec, p_spu, p_spu_data );
    free( p_spu_data );

    return p_spu;
}

/*****************************************************************************
 * ParseControlSeq: parse all SPU control sequences
 *****************************************************************************
 * This is the most important part in SPU decoding. We get dates, palette
 * information, coordinates, and so on. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseControlSeq( decoder_t *p_dec, subpicture_t *p_spu,
                            subpicture_data_t *p_spu_data )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Our current index in the SPU packet */
    unsigned int i_index = p_sys->i_rle_size + 4;

    /* The next start-of-control-sequence index and the previous one */
    unsigned int i_next_seq = 0, i_cur_seq = 0;

    /* Command and date */
    uint8_t i_command = SPU_CMD_END;
    mtime_t date = 0;

    unsigned int i, pi_alpha[4];

    /* Initialize the structure */
    p_spu->i_start = p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_FALSE;

    do
    {
        if( (int)i_index >= p_sys->i_spu_size + 1 )
        {
            /* sanity
             * XXX only on test by loop as p_sys->buffer is bigger than needed
             * to avoid checking at each access
             */
            break;
        }

        /* If we just read a command sequence, read the next one;
         * otherwise, go on with the commands of the current sequence. */
        if( i_command == SPU_CMD_END )
        {
            /* Get the control sequence date */
            date = (mtime_t)GetWBE( &p_sys->buffer[i_index] ) * 11000;
            /* FIXME How to access i_rate
                    * p_spudec->bit_stream.p_pes->i_rate / DEFAULT_RATE;
            */

            /* Next offset */
            i_cur_seq = i_index;
            i_next_seq = GetWBE( &p_sys->buffer[i_index+2] );

            /* Skip what we just read */
            i_index += 4;
        }

        i_command = p_sys->buffer[i_index++];

        switch( i_command )
        {
        case SPU_CMD_FORCE_DISPLAY: /* 00 (force displaying) */
            p_spu->i_start = p_spu_data->i_pts + date;
            p_spu->b_ephemer = VLC_TRUE;
            break;

        /* Convert the dates in seconds to PTS values */
        case SPU_CMD_START_DISPLAY: /* 01 (start displaying) */
            p_spu->i_start = p_spu_data->i_pts + date;
            break;

        case SPU_CMD_STOP_DISPLAY: /* 02 (stop displaying) */
            p_spu->i_stop = p_spu_data->i_pts + date;
            break;

        case SPU_CMD_SET_PALETTE:

            /* 03xxxx (palette) */
            if( p_dec->fmt_in.subs.spu.palette[0] == 0xBeeF )
            {
                unsigned int idx[4];

                p_spu_data->b_palette = VLC_TRUE;

                idx[0] = (p_sys->buffer[i_index+0]>>4)&0x0f;
                idx[1] = (p_sys->buffer[i_index+0])&0x0f;
                idx[2] = (p_sys->buffer[i_index+1]>>4)&0x0f;
                idx[3] = (p_sys->buffer[i_index+1])&0x0f;

                for( i = 0; i < 4 ; i++ )
                {
                    uint32_t i_color = p_dec->fmt_in.subs.spu.palette[1+idx[i]];

                    /* FIXME: this job should be done sooner */
                    p_spu_data->pi_yuv[3-i][0] = (i_color>>16) & 0xff;
                    p_spu_data->pi_yuv[3-i][1] = (i_color>>0) & 0xff;
                    p_spu_data->pi_yuv[3-i][2] = (i_color>>8) & 0xff;
                }
            }
            i_index += 2;

            break;

        case SPU_CMD_SET_ALPHACHANNEL: /* 04xxxx (alpha channel) */
            pi_alpha[3] = (p_sys->buffer[i_index+0]>>4)&0x0f;
            pi_alpha[2] = (p_sys->buffer[i_index+0])&0x0f;
            pi_alpha[1] = (p_sys->buffer[i_index+1]>>4)&0x0f;
            pi_alpha[0] = (p_sys->buffer[i_index+1])&0x0f;

            /* Ignore blank alpha palette. Sometimes spurious blank
             * alpha palettes are present - dunno why. */
            if( pi_alpha[0] | pi_alpha[1] | pi_alpha[2] | pi_alpha[3] )
            {
                p_spu_data->pi_alpha[0] = pi_alpha[0];
                p_spu_data->pi_alpha[1] = pi_alpha[1];
                p_spu_data->pi_alpha[2] = pi_alpha[2];
                p_spu_data->pi_alpha[3] = pi_alpha[3];
            }
            else
            {
                msg_Warn( p_dec, "ignoring blank alpha palette" );
            }

            i_index += 2;
            break;

        case SPU_CMD_SET_COORDINATES: /* 05xxxyyyxxxyyy (coordinates) */
            p_spu->i_x = (p_sys->buffer[i_index+0]<<4)|
                         ((p_sys->buffer[i_index+1]>>4)&0x0f);
            p_spu->i_width = (((p_sys->buffer[i_index+1]&0x0f)<<8)|
                              p_sys->buffer[i_index+2]) - p_spu->i_x + 1;

            p_spu->i_y = (p_sys->buffer[i_index+3]<<4)|
                         ((p_sys->buffer[i_index+4]>>4)&0x0f);
            p_spu->i_height = (((p_sys->buffer[i_index+4]&0x0f)<<8)|
                              p_sys->buffer[i_index+5]) - p_spu->i_y + 1;

            /* Auto crop fullscreen subtitles */
            if( p_spu->i_height > 250 )
                p_spu_data->b_auto_crop = VLC_TRUE;

            i_index += 6;
            break;

        case SPU_CMD_SET_OFFSETS: /* 06xxxxyyyy (byte offsets) */
            p_spu_data->pi_offset[0] = GetWBE(&p_sys->buffer[i_index+0]) - 4;
            p_spu_data->pi_offset[1] = GetWBE(&p_sys->buffer[i_index+2]) - 4;
            i_index += 4;
            break;

        case SPU_CMD_END: /* ff (end) */
            break;

        default: /* xx (unknown command) */
            msg_Warn( p_dec, "unknown command 0x%.2x", i_command );
            return VLC_EGENERIC;
        }

        /* We need to check for quit commands here */
        if( p_dec->b_die )
        {
            return VLC_EGENERIC;
        }

    } while( i_command != SPU_CMD_END || i_index == i_next_seq );

    /* Check that the next sequence index matches the current one */
    if( i_next_seq != i_cur_seq )
    {
        msg_Err( p_dec, "index mismatch (0x%.4x != 0x%.4x)",
                 i_next_seq, i_cur_seq );
        return VLC_EGENERIC;
    }

    if( (int)i_index > p_sys->i_spu_size )
    {
        msg_Err( p_dec, "uh-oh, we went too far (0x%.4x > 0x%.4x)",
                 i_index, p_sys->i_spu_size );
        return VLC_EGENERIC;
    }

    if( !p_spu->i_start )
    {
        msg_Err( p_dec, "no `start display' command" );
    }

    if( p_spu->i_stop <= p_spu->i_start && !p_spu->b_ephemer )
    {
        /* This subtitle will live for 5 seconds or until the next subtitle */
        p_spu->i_stop = p_spu->i_start + (mtime_t)500 * 11000;
        p_spu->b_ephemer = VLC_TRUE;
    }

    /* Get rid of padding bytes */
    if( p_sys->i_spu_size > (int)i_index + 1 )
    {
        /* Zero or one padding byte, are quite usual
         * More than one padding byte - this is very strange, but
         * we can deal with it */
        msg_Warn( p_dec, "%i padding bytes, we usually get 0 or 1 of them",
                  p_sys->i_spu_size - i_index );
    }

    /* Successfully parsed ! */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseRLE: parse the RLE part of the subtitle
 *****************************************************************************
 * This part parses the subtitle graphical data and stores it in a more
 * convenient structure for later decoding. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseRLE( decoder_t *p_dec, subpicture_t * p_spu,
                     subpicture_data_t *p_spu_data )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t       *p_src = &p_sys->buffer[4];

    unsigned int i_code;

    unsigned int i_width = p_spu->i_width;
    unsigned int i_height = p_spu->i_height;
    unsigned int i_x, i_y;

    uint16_t *p_dest = (uint16_t *)p_spu_data->p_data;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  i_id = 0;                   /* Start on the even SPU layer */
    unsigned int  pi_table[ 2 ];
    unsigned int *pi_offset;

    /* Cropping */
    vlc_bool_t b_empty_top = VLC_TRUE;
    unsigned int i_skipped_top = 0, i_skipped_bottom = 0;
    unsigned int i_transparent_code = 0;
 
    /* Colormap statistics */
    int i_border = -1;
    int stats[4]; stats[0] = stats[1] = stats[2] = stats[3] = 0;

    pi_table[ 0 ] = p_spu_data->pi_offset[ 0 ] << 1;
    pi_table[ 1 ] = p_spu_data->pi_offset[ 1 ] << 1;

    for( i_y = 0 ; i_y < i_height ; i_y++ )
    {
        pi_offset = pi_table + i_id;

        for( i_x = 0 ; i_x < i_width ; i_x += i_code >> 2 )
        {
            i_code = AddNibble( 0, p_src, pi_offset );

            if( i_code < 0x04 )
            {
                i_code = AddNibble( i_code, p_src, pi_offset );

                if( i_code < 0x10 )
                {
                    i_code = AddNibble( i_code, p_src, pi_offset );

                    if( i_code < 0x040 )
                    {
                        i_code = AddNibble( i_code, p_src, pi_offset );

                        if( i_code < 0x0100 )
                        {
                            /* If the 14 first bits are set to 0, then it's a
                             * new line. We emulate it. */
                            if( i_code < 0x0004 )
                            {
                                i_code |= ( i_width - i_x ) << 2;
                            }
                            else
                            {
                                /* We have a boo boo ! */
                                msg_Err( p_dec, "unknown RLE code "
                                         "0x%.4x", i_code );
                                return VLC_EGENERIC;
                            }
                        }
                    }
                }
            }

            if( ( (i_code >> 2) + i_x + i_y * i_width ) > i_height * i_width )
            {
                msg_Err( p_dec, "out of bounds, %i at (%i,%i) is out of %ix%i",
                         i_code >> 2, i_x, i_y, i_width, i_height );
                return VLC_EGENERIC;
            }

            /* Try to find the border color */
            if( p_spu_data->pi_alpha[ i_code & 0x3 ] != 0x00 )
            {
                i_border = i_code & 0x3;
                stats[i_border] += i_code >> 2;
            }

            /* Auto crop subtitles (a lot more optimized) */
            if( p_spu_data->b_auto_crop )
            {
                if( !i_y )
                {
                    /* We assume that if the first line is transparent, then
                     * it is using the palette index for the
                     * (background) transparent color */
                    if( (i_code >> 2) == i_width &&
                        p_spu_data->pi_alpha[ i_code & 0x3 ] == 0x00 )
                    {
                        i_transparent_code = i_code;
                    }
                    else
                    {
                        p_spu_data->b_auto_crop = VLC_FALSE;
                    }
                }

                if( i_code == i_transparent_code )
                {
                    if( b_empty_top )
                    {
                        /* This is a blank top line, we skip it */
                      i_skipped_top++;
                    }
                    else
                    {
                        /* We can't be sure the current lines will be skipped,
                         * so we store the code just in case. */
                      *p_dest++ = i_code;
                      i_skipped_bottom++;
                    }
                }
                else
                {
                    /* We got a valid code, store it */
                    *p_dest++ = i_code;

                    /* Valid code means no blank line */
                    b_empty_top = VLC_FALSE;
                    i_skipped_bottom = 0;
                }
            }
            else
            {
                *p_dest++ = i_code;
            }
        }

        /* Check that we didn't go too far */
        if( i_x > i_width )
        {
            msg_Err( p_dec, "i_x overflowed, %i > %i", i_x, i_width );
            return VLC_EGENERIC;
        }

        /* Byte-align the stream */
        if( *pi_offset & 0x1 )
        {
            (*pi_offset)++;
        }

        /* Swap fields */
        i_id = ~i_id & 0x1;
    }

    /* We shouldn't get any padding bytes */
    if( i_y < i_height )
    {
        msg_Err( p_dec, "padding bytes found in RLE sequence" );
        msg_Err( p_dec, "send mail to <sam@zoy.org> if you "
                        "want to help debugging this" );

        /* Skip them just in case */
        while( i_y < i_height )
        {
            *p_dest++ = i_width << 2;
            i_y++;
        }

        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "valid subtitle, size: %ix%i, position: %i,%i",
             p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );

    /* Crop if necessary */
    if( i_skipped_top || i_skipped_bottom )
    {
        int i_y = p_spu->i_y + i_skipped_top;
        int i_height = p_spu->i_height - (i_skipped_top + i_skipped_bottom);

        p_spu_data->i_y_top_offset = i_skipped_top;
        p_spu_data->i_y_bottom_offset = i_skipped_bottom;
        msg_Dbg( p_dec, "cropped to: %ix%i, position: %i,%i",
                 p_spu->i_width, i_height, p_spu->i_x, i_y );
    }
 
    /* Handle color if no palette was found */
    if( !p_spu_data->b_palette )
    {
        int i, i_inner = -1, i_shade = -1;

        /* Set the border color */
        p_spu_data->pi_yuv[i_border][0] = 0x00;
        p_spu_data->pi_yuv[i_border][1] = 0x80;
        p_spu_data->pi_yuv[i_border][2] = 0x80;
        stats[i_border] = 0;

        /* Find the inner colors */
        for( i = 0 ; i < 4 && i_inner == -1 ; i++ )
        {
            if( stats[i] )
            {
                i_inner = i;
            }
        }

        for(       ; i < 4 && i_shade == -1 ; i++ )
        {
            if( stats[i] )
            {
                if( stats[i] > stats[i_inner] )
                {
                    i_shade = i_inner;
                    i_inner = i;
                }
                else
                {
                    i_shade = i;
                }
            }
        }

        /* Set the inner color */
        if( i_inner != -1 )
        {
            p_spu_data->pi_yuv[i_inner][0] = 0xff;
            p_spu_data->pi_yuv[i_inner][1] = 0x80;
            p_spu_data->pi_yuv[i_inner][2] = 0x80;
        }

        /* Set the anti-aliasing color */
        if( i_shade != -1 )
        {
            p_spu_data->pi_yuv[i_shade][0] = 0x80;
            p_spu_data->pi_yuv[i_shade][1] = 0x80;
            p_spu_data->pi_yuv[i_shade][2] = 0x80;
        }

        msg_Dbg( p_dec, "using custom palette (border %i, inner %i, shade %i)",
                 i_border, i_inner, i_shade );
    }

    return VLC_SUCCESS;
}

static void Render( decoder_t *p_dec, subpicture_t *p_spu,
                    subpicture_data_t *p_spu_data )
{
    uint8_t *p_p;
    int i_x, i_y, i_len, i_color, i_pitch;
    uint16_t *p_source = (uint16_t *)p_spu_data->p_data;
    video_format_t fmt;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','P');
    fmt.i_aspect = 0; /* 0 means use aspect ratio of background video */
    fmt.i_width = fmt.i_visible_width = p_spu->i_width;
    fmt.i_height = fmt.i_visible_height = p_spu->i_height -
        p_spu_data->i_y_top_offset - p_spu_data->i_y_bottom_offset;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        return;
    }

    p_spu->p_region->i_x = 0;
    p_spu->p_region->i_y = p_spu_data->i_y_top_offset;
    p_p = p_spu->p_region->picture.p->p_pixels;
    i_pitch = p_spu->p_region->picture.p->i_pitch;

    /* Build palette */
    fmt.p_palette->i_entries = 4;
    for( i_x = 0; i_x < fmt.p_palette->i_entries; i_x++ )
    {
        fmt.p_palette->palette[i_x][0] = p_spu_data->pi_yuv[i_x][0];
        fmt.p_palette->palette[i_x][1] = p_spu_data->pi_yuv[i_x][1];
        fmt.p_palette->palette[i_x][2] = p_spu_data->pi_yuv[i_x][2];
        fmt.p_palette->palette[i_x][3] =
            p_spu_data->pi_alpha[i_x] == 0xf ? 0xff :
            p_spu_data->pi_alpha[i_x] << 4;
    }

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < (int)fmt.i_height * i_pitch; i_y += i_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0 ; i_x < (int)fmt.i_width; i_x += i_len )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;
            memset( p_p + i_x + i_y, i_color, i_len );
        }
    }
}
