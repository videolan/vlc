/*****************************************************************************
 * parse.c: SPU parser
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: parse.c,v 1.4 2002/11/06 18:07:57 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#ifdef WIN32                   /* getpid() for win32 is located in process.h */
#   include <process.h>
#endif

#include "spudec.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  ParseControlSeq  ( spudec_thread_t *, subpicture_t * );
static int  ParseRLE         ( spudec_thread_t *, subpicture_t *, uint8_t * );

static void DestroySPU       ( subpicture_t * );

static void UpdateSPU        ( subpicture_t *, vlc_object_t * );
static int  CropCallback     ( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );

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
 * SyncPacket: get in sync with the stream
 *****************************************************************************
 * This function makes a few sanity checks and returns 0 if it looks like we
 * are at the beginning of a subpicture packet.
 *****************************************************************************/
int E_(SyncPacket)( spudec_thread_t *p_spudec )
{
    /* Re-align the buffer on an 8-bit boundary */
    RealignBits( &p_spudec->bit_stream );

    /* The total SPU packet size, often bigger than a PS packet */
    p_spudec->i_spu_size = GetBits( &p_spudec->bit_stream, 16 );

    /* The RLE stuff size (remove 4 because we just read 32 bits) */
    p_spudec->i_rle_size = ShowBits( &p_spudec->bit_stream, 16 ) - 4;

    /* If the values we got are a bit strange, skip packet */
    if( !p_spudec->i_spu_size
         || ( p_spudec->i_rle_size >= p_spudec->i_spu_size ) )
    {
        return VLC_EGENERIC;
    }

    RemoveBits( &p_spudec->bit_stream, 16 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParsePacket: parse an SPU packet and send it to the video output
 *****************************************************************************
 * This function parses the SPU packet and, if valid, sends it to the
 * video output.
 *****************************************************************************/
void E_(ParsePacket)( spudec_thread_t *p_spudec )
{
    subpicture_t * p_spu;
    uint8_t      * p_src;
    unsigned int   i_offset;
    mtime_t        i_pts;

    msg_Dbg( p_spudec->p_fifo, "trying to gather a 0x%.2x long subtitle",
                               p_spudec->i_spu_size );

    /* We cannot display a subpicture with no date */
    NextPTS( &p_spudec->bit_stream, &i_pts, NULL );
    if( i_pts == 0 )
    {
        msg_Warn( p_spudec->p_fifo, "subtitle without a date" );
        return;
    }

    /* Allocate the subpicture internal data. */
    p_spu = vout_CreateSubPicture( p_spudec->p_vout, MEMORY_SUBPICTURE );

    if( p_spu == NULL )
    {
        return;
    }

    /* Rationale for the "p_spudec->i_rle_size * 4": we are going to
     * expand the RLE stuff so that we won't need to read nibbles later
     * on. This will speed things up a lot. Plus, we'll only need to do
     * this stupid interlacing stuff once. */
    p_spu->p_sys = malloc( sizeof( subpicture_sys_t )
                            + p_spudec->i_rle_size * 4 );

    if( p_spu->p_sys == NULL )
    {
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    /* Fill the p_spu structure */
    vlc_mutex_init( p_spudec->p_fifo, &p_spu->p_sys->lock );

    p_spu->pf_render = E_(RenderSPU);
    p_spu->pf_destroy = DestroySPU;
    p_spu->p_sys->p_data = (uint8_t*)p_spu->p_sys + sizeof( subpicture_sys_t );
    p_spu->p_sys->b_palette = VLC_FALSE;

    p_spu->p_sys->pi_alpha[0] = 0x00;
    p_spu->p_sys->pi_alpha[1] = 0x0f;
    p_spu->p_sys->pi_alpha[2] = 0x0f;
    p_spu->p_sys->pi_alpha[3] = 0x0f;

    p_spu->p_sys->b_crop = VLC_FALSE;

    /* Get display time now. If we do it later, we may miss the PTS. */
    p_spu->p_sys->i_pts = i_pts;

    /* Attach to our input thread */
    p_spu->p_sys->p_input = vlc_object_find( p_spudec->p_fifo,
                                             VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_spu->p_sys->p_input )
    {
        vlc_value_t val;

        if( !var_Get( p_spu->p_sys->p_input, "highlight-mutex", &val ) )
        {
            vlc_mutex_t *p_mutex = val.p_address;

            vlc_mutex_lock( p_mutex );
            UpdateSPU( p_spu, VLC_OBJECT(p_spu->p_sys->p_input) );
            var_AddCallback( p_spu->p_sys->p_input,
                             "highlight", CropCallback, p_spu );
            vlc_mutex_unlock( p_mutex );
        }
    }

    /* Allocate the temporary buffer we will parse */
    p_src = malloc( p_spudec->i_rle_size );

    if( p_src == NULL )
    {
        msg_Err( p_spudec->p_fifo, "out of memory" );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    /* Get RLE data */
    for( i_offset = 0; i_offset < p_spudec->i_rle_size;
         i_offset += SPU_CHUNK_SIZE )
    {
        GetChunk( &p_spudec->bit_stream, p_src + i_offset,
                  ( i_offset + SPU_CHUNK_SIZE < p_spudec->i_rle_size ) ?
                  SPU_CHUNK_SIZE : p_spudec->i_rle_size - i_offset );

        /* Abort subtitle parsing if we were requested to stop */
        if( p_spudec->p_fifo->b_die )
        {
            free( p_src );
            vout_DestroySubPicture( p_spudec->p_vout, p_spu );
            return;
        }
    }

#if 0
    /* Dump the subtitle info */
    intf_WarnHexDump( 5, p_spu->p_sys->p_data, p_spudec->i_rle_size );
#endif

    /* Getting the control part */
    if( ParseControlSeq( p_spudec, p_spu ) )
    {
        /* There was a parse error, delete the subpicture */
        free( p_src );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    /* At this point, no more GetBit() command is needed, so we have all
     * the data we need to tell whether the subtitle is valid. Thus we
     * try to display it and we ignore b_die. */

    if( ParseRLE( p_spudec, p_spu, p_src ) )
    {
        /* There was a parse error, delete the subpicture */
        free( p_src );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    msg_Dbg( p_spudec->p_fifo, "total size: 0x%x, RLE offsets: 0x%x 0x%x",
             p_spudec->i_spu_size,
             p_spu->p_sys->pi_offset[0], p_spu->p_sys->pi_offset[1] );

    /* SPU is finished - we can ask the video output to display it */
    vout_DisplaySubPicture( p_spudec->p_vout, p_spu );

    /* TODO: do stuff! */

    /* Clean up */
    free( p_src );
}

/*****************************************************************************
 * ParseControlSeq: parse all SPU control sequences
 *****************************************************************************
 * This is the most important part in SPU decoding. We get dates, palette
 * information, coordinates, and so on. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseControlSeq( spudec_thread_t *p_spudec,
                                  subpicture_t * p_spu )
{
    /* Our current index in the SPU packet */
    int i_index = p_spudec->i_rle_size + 4;

    /* The next start-of-control-sequence index and the previous one */
    int i_next_seq = 0, i_cur_seq = 0;

    /* Command and date */
    uint8_t i_command = SPU_CMD_END;
    mtime_t date = 0;

    int i, pi_alpha[4];

    /* Initialize the structure */
    p_spu->i_start = p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_FALSE;

    do
    {
        /* If we just read a command sequence, read the next one;
         * otherwise, go on with the commands of the current sequence. */
        if( i_command == SPU_CMD_END )
        {
            /* Get the control sequence date */
            date = GetBits( &p_spudec->bit_stream, 16 );
 
            /* Next offset */
            i_cur_seq = i_index;
            i_next_seq = GetBits( &p_spudec->bit_stream, 16 );
 
            /* Skip what we just read */
            i_index += 4;
        }
 
        i_command = GetBits( &p_spudec->bit_stream, 8 );
        i_index++;
 
        switch( i_command )
        {
        case SPU_CMD_FORCE_DISPLAY: /* 00 (force displaying) */
            p_spu->i_start = p_spu->p_sys->i_pts + ( date * 11000 );
            p_spu->b_ephemer = VLC_TRUE;
            break;

        /* Convert the dates in seconds to PTS values */
        case SPU_CMD_START_DISPLAY: /* 01 (start displaying) */
            p_spu->i_start = p_spu->p_sys->i_pts + ( date * 11000 );
            break;

        case SPU_CMD_STOP_DISPLAY: /* 02 (stop displaying) */
            p_spu->i_stop = p_spu->p_sys->i_pts + ( date * 11000 );
            break;

        case SPU_CMD_SET_PALETTE:

            /* 03xxxx (palette) */
            if( p_spudec->p_fifo->p_demux_data
                 && *(int*)p_spudec->p_fifo->p_demux_data == 0xBeeF )
            {
                uint32_t i_color;

                p_spu->p_sys->b_palette = VLC_TRUE;
                for( i = 0; i < 4 ; i++ )
                {
                    i_color = ((uint32_t*)((char*)p_spudec->p_fifo->
                                p_demux_data + sizeof(int)))[
                                  GetBits(&p_spudec->bit_stream, 4) ];

                    /* FIXME: this job should be done sooner */
#ifndef WORDS_BIGENDIAN
                    p_spu->p_sys->pi_yuv[3-i][0] = (i_color>>16) & 0xff;
                    p_spu->p_sys->pi_yuv[3-i][1] = (i_color>>0) & 0xff;
                    p_spu->p_sys->pi_yuv[3-i][2] = (i_color>>8) & 0xff;
#else
                    p_spu->p_sys->pi_yuv[3-i][0] = (i_color>>8) & 0xff;
                    p_spu->p_sys->pi_yuv[3-i][1] = (i_color>>24) & 0xff;
                    p_spu->p_sys->pi_yuv[3-i][2] = (i_color>>16) & 0xff;
#endif
                }
            }
            else
            {
                RemoveBits( &p_spudec->bit_stream, 16 );
            }
            i_index += 2;

            break;

        case SPU_CMD_SET_ALPHACHANNEL: /* 04xxxx (alpha channel) */
            pi_alpha[3] = GetBits( &p_spudec->bit_stream, 4 );
            pi_alpha[2] = GetBits( &p_spudec->bit_stream, 4 );
            pi_alpha[1] = GetBits( &p_spudec->bit_stream, 4 );
            pi_alpha[0] = GetBits( &p_spudec->bit_stream, 4 );

            /* Ignore blank alpha palette. Sometimes spurious blank
             * alpha palettes are present - dunno why. */
            if( pi_alpha[0] | pi_alpha[1] | pi_alpha[2] | pi_alpha[3] )
            {
                p_spu->p_sys->pi_alpha[0] = pi_alpha[0];
                p_spu->p_sys->pi_alpha[1] = pi_alpha[1];
                p_spu->p_sys->pi_alpha[2] = pi_alpha[2];
                p_spu->p_sys->pi_alpha[3] = pi_alpha[3];
            }
            else
            {
                msg_Warn( p_spudec->p_fifo, "ignoring blank alpha palette" );
            }

            i_index += 2;
            break;

        case SPU_CMD_SET_COORDINATES: /* 05xxxyyyxxxyyy (coordinates) */
            p_spu->i_x = GetBits( &p_spudec->bit_stream, 12 );
            p_spu->i_width = GetBits( &p_spudec->bit_stream, 12 )
                              - p_spu->i_x + 1;

            p_spu->i_y = GetBits( &p_spudec->bit_stream, 12 );
            p_spu->i_height = GetBits( &p_spudec->bit_stream, 12 )
                               - p_spu->i_y + 1;

            i_index += 6;
            break;

        case SPU_CMD_SET_OFFSETS: /* 06xxxxyyyy (byte offsets) */
            p_spu->p_sys->pi_offset[0] =
                GetBits( &p_spudec->bit_stream, 16 ) - 4;

            p_spu->p_sys->pi_offset[1] =
                GetBits( &p_spudec->bit_stream, 16 ) - 4;

            i_index += 4;
            break;

        case SPU_CMD_END: /* ff (end) */
            break;

        default: /* xx (unknown command) */
            msg_Err( p_spudec->p_fifo, "unknown command 0x%.2x",
                                       i_command );
            return VLC_EGENERIC;
        }

        /* We need to check for quit commands here */
        if( p_spudec->p_fifo->b_die )
        {
            return VLC_EGENERIC;
        }

    } while( i_command != SPU_CMD_END || i_index == i_next_seq );

    /* Check that the next sequence index matches the current one */
    if( i_next_seq != i_cur_seq )
    {
        msg_Err( p_spudec->p_fifo, "index mismatch (0x%.4x != 0x%.4x)",
                                   i_next_seq, i_cur_seq );
        return VLC_EGENERIC;
    }

    if( i_index > p_spudec->i_spu_size )
    {
        msg_Err( p_spudec->p_fifo, "uh-oh, we went too far (0x%.4x > 0x%.4x)",
                                   i_index, p_spudec->i_spu_size );
        return VLC_EGENERIC;
    }

    if( !p_spu->i_start )
    {
        msg_Err( p_spudec->p_fifo, "no `start display' command" );
    }

    if( !p_spu->i_stop && !p_spu->b_ephemer )
    {
        /* This subtitle will live for 5 seconds or until the next subtitle */
        p_spu->i_stop = p_spu->i_start + 500 * 11000;
        p_spu->b_ephemer = VLC_TRUE;
    }

    /* Get rid of padding bytes */
    switch( p_spudec->i_spu_size - i_index )
    {
        /* Zero or one padding byte, quite usual */
        case 1:
            RemoveBits( &p_spudec->bit_stream, 8 );
            i_index++;
        case 0:
            break;

        /* More than one padding byte - this is very strange, but
         * we can deal with it */
        default:
            msg_Warn( p_spudec->p_fifo,
                      "%i padding bytes, we usually get 0 or 1 of them",
                      p_spudec->i_spu_size - i_index );

            while( i_index < p_spudec->i_spu_size )
            {
                RemoveBits( &p_spudec->bit_stream, 8 );
                i_index++;
            }

            break;
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
static int ParseRLE( spudec_thread_t *p_spudec,
                     subpicture_t * p_spu, uint8_t * p_src )
{
    unsigned int i_code;

    unsigned int i_width = p_spu->i_width;
    unsigned int i_height = p_spu->i_height;
    unsigned int i_x, i_y;

    uint16_t *p_dest = (uint16_t *)p_spu->p_sys->p_data;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  i_id = 0;                   /* Start on the even SPU layer */
    unsigned int  pi_table[ 2 ];
    unsigned int *pi_offset;

#if 0 /* cropping */
    vlc_bool_t b_empty_top = VLC_TRUE,
               b_empty_bottom = VLC_FALSE;
    unsigned int i_skipped_top = 0,
                 i_skipped_bottom = 0;
#endif

    /* Colormap statistics */
    int i_border = -1;
    int stats[4]; stats[0] = stats[1] = stats[2] = stats[3] = 0;

    pi_table[ 0 ] = p_spu->p_sys->pi_offset[ 0 ] << 1;
    pi_table[ 1 ] = p_spu->p_sys->pi_offset[ 1 ] << 1;

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
                                msg_Err( p_spudec->p_fifo, "unknown RLE code "
                                         "0x%.4x", i_code );
                                return VLC_EGENERIC;
                            }
                        }
                    }
                }
            }

            if( ( (i_code >> 2) + i_x + i_y * i_width ) > i_height * i_width )
            {
                msg_Err( p_spudec->p_fifo,
                         "out of bounds, %i at (%i,%i) is out of %ix%i",
                         i_code >> 2, i_x, i_y, i_width, i_height );
                return VLC_EGENERIC;
            }

            /* Try to find the border color */
            if( p_spu->p_sys->pi_alpha[ i_code & 0x3 ] != 0x00 )
            {
                i_border = i_code & 0x3;
                stats[i_border] += i_code >> 2;
            }

#if 0 /* cropping */
            if( (i_code >> 2) == i_width
                 && p_spu->p_sys->pi_alpha[ i_code & 0x3 ] == 0x00 )
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

                    b_empty_bottom = VLC_TRUE;
                    i_skipped_bottom++;
                }
            }
            else
            {
                /* We got a valid code, store it */
                *p_dest++ = i_code;

                /* Valid code means no blank line */
                b_empty_top = VLC_FALSE;
                b_empty_bottom = VLC_FALSE;
                i_skipped_bottom = 0;
            }
#else
            *p_dest++ = i_code;
#endif
        }

        /* Check that we didn't go too far */
        if( i_x > i_width )
        {
            msg_Err( p_spudec->p_fifo, "i_x overflowed, %i > %i",
                                       i_x, i_width );
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
        msg_Err( p_spudec->p_fifo, "padding bytes found in RLE sequence" );
        msg_Err( p_spudec->p_fifo, "send mail to <sam@zoy.org> if you "
                                   "want to help debugging this" );

        /* Skip them just in case */
        while( i_y < i_height )
        {
            *p_dest++ = i_width << 2;
            i_y++;
        }

        return VLC_EGENERIC;
    }

    msg_Dbg( p_spudec->p_fifo, "valid subtitle, size: %ix%i, position: %i,%i",
             p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );

#if 0 /* cropping */
    /* Crop if necessary */
    if( i_skipped_top || i_skipped_bottom )
    {
        p_spu->i_y += i_skipped_top;
        p_spu->i_height -= i_skipped_top + i_skipped_bottom;

        msg_Dbg( p_spudec->p_fifo, "cropped to: %ix%i, position: %i,%i",
                 p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );
    }
#endif

    /* Handle color if no palette was found */
    if( !p_spu->p_sys->b_palette )
    {
        int i, i_inner = -1, i_shade = -1;

        /* Set the border color */
        p_spu->p_sys->pi_yuv[i_border][0] = 0x00;
        p_spu->p_sys->pi_yuv[i_border][1] = 0x80;
        p_spu->p_sys->pi_yuv[i_border][2] = 0x80;
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
            p_spu->p_sys->pi_yuv[i_inner][0] = 0xff;
            p_spu->p_sys->pi_yuv[i_inner][1] = 0x80;
            p_spu->p_sys->pi_yuv[i_inner][2] = 0x80;
        }

        /* Set the anti-aliasing color */
        if( i_shade != -1 )
        {
            p_spu->p_sys->pi_yuv[i_shade][0] = 0x80;
            p_spu->p_sys->pi_yuv[i_shade][1] = 0x80;
            p_spu->p_sys->pi_yuv[i_shade][2] = 0x80;
        }

        msg_Dbg( p_spudec->p_fifo,
                 "using custom palette (border %i, inner %i, shade %i)",
                 i_border, i_inner, i_shade );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroySPU: subpicture destructor
 *****************************************************************************/
static void DestroySPU( subpicture_t *p_spu )
{
    if( p_spu->p_sys->p_input )
    {
        /* Detach from our input thread */
        var_DelCallback( p_spu->p_sys->p_input, "highlight",
                         CropCallback, p_spu );
        vlc_object_release( p_spu->p_sys->p_input );
    }

    vlc_mutex_destroy( &p_spu->p_sys->lock );
    free( p_spu->p_sys );
}

/*****************************************************************************
 * UpdateSPU: update subpicture settings
 *****************************************************************************
 * This function is called from CropCallback and at initialization time, to
 * retrieve crop information from the input.
 *****************************************************************************/
static void UpdateSPU( subpicture_t *p_spu, vlc_object_t *p_object )
{
    vlc_value_t val;

    if( var_Get( p_object, "highlight", &val ) )
    {
        return;
    }

    p_spu->p_sys->b_crop = val.b_bool;
    if( !p_spu->p_sys->b_crop )
    {
        return;
    }

    var_Get( p_object, "x-start", &val );
    p_spu->p_sys->i_x_start = val.i_int;
    var_Get( p_object, "y-start", &val );
    p_spu->p_sys->i_y_start = val.i_int;
    var_Get( p_object, "x-end", &val );
    p_spu->p_sys->i_x_end = val.i_int;
    var_Get( p_object, "y-end", &val );
    p_spu->p_sys->i_y_end = val.i_int;

#if 0
    if( var_Get( p_object, "color", &val ) == VLC_SUCCESS )
    {
        p_spu->p_sys->pi_color[0] = ((uint8_t *)val.p_address)[0];
        p_spu->p_sys->pi_color[1] = ((uint8_t *)val.p_address)[1];
        p_spu->p_sys->pi_color[2] = ((uint8_t *)val.p_address)[2];
        p_spu->p_sys->pi_color[3] = ((uint8_t *)val.p_address)[3];
    }
#endif

    if( var_Get( p_object, "contrast", &val ) == VLC_SUCCESS )
    {
        p_spu->p_sys->pi_alpha[0] = ((uint8_t *)val.p_address)[0];
        p_spu->p_sys->pi_alpha[1] = ((uint8_t *)val.p_address)[1];
        p_spu->p_sys->pi_alpha[2] = ((uint8_t *)val.p_address)[2];
        p_spu->p_sys->pi_alpha[3] = ((uint8_t *)val.p_address)[3];
    }
}

/*****************************************************************************
 * CropCallback: called when the highlight properties are changed
 *****************************************************************************
 * This callback is called from the input thread when we need cropping
 *****************************************************************************/
static int CropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    UpdateSPU( (subpicture_t *)p_data, p_object );

    return VLC_SUCCESS;
}

