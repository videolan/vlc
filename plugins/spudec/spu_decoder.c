/*****************************************************************************
 * spu_decoder.c : spu decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: spu_decoder.c,v 1.29 2002/06/27 19:46:32 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Rudolf Cornelissen <rag.cornelissen@inter.nl.net>
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

#include "spu_decoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  decoder_Probe ( u8 * );
static int  decoder_Run   ( decoder_fifo_t * );
static int  InitThread    ( spudec_thread_t * );
static void EndThread     ( spudec_thread_t * );

static int  SyncPacket           ( spudec_thread_t * );
static void ParsePacket          ( spudec_thread_t * );
static int  ParseControlSequences( spudec_thread_t *, subpicture_t * );
static int  ParseRLE             ( spudec_thread_t *, subpicture_t *, u8 * );
static void RenderSPU            ( vout_thread_t *, picture_t *,
                                   const subpicture_t * );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( spudec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("DVD subtitles decoder module") )
    ADD_CAPABILITY( DECODER, 50 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( spudec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    return ( *pi_type == DVD_SPU_ES ) ? 0 : -1;
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run( decoder_fifo_t * p_fifo )
{
    spudec_thread_t *     p_spudec;
   
    /* Allocate the memory needed to store the thread's structure */
    p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) );

    if ( p_spudec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    
    /*
     * Initialize the thread properties
     */
    p_spudec->p_vout = NULL;
    p_spudec->p_fifo = p_fifo;
        
    /*
     * Initialize thread and free configuration
     */
    p_spudec->p_fifo->b_error = InitThread( p_spudec );

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_spudec->p_fifo->b_die) && (!p_spudec->p_fifo->b_error) )
    {
        if( !SyncPacket( p_spudec ) )
        {
            ParsePacket( p_spudec );
        }
    }

    /*
     * Error loop
     */
    if( p_spudec->p_fifo->b_error )
    {
        DecoderError( p_spudec->p_fifo );

        /* End of thread */
        EndThread( p_spudec );
        return -1;
    }

    /* End of thread */
    EndThread( p_spudec );
    return 0;
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize spu decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( spudec_thread_t *p_spudec )
{
    /* Find an available video output */
    do
    {
        if( p_spudec->p_fifo->b_die || p_spudec->p_fifo->b_error )
        {
            return -1;
        }

        p_spudec->p_vout = vlc_object_find( p_spudec->p_fifo, VLC_OBJECT_VOUT,
                                                              FIND_ANYWHERE );

        if( p_spudec->p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    InitBitstream( &p_spudec->bit_stream, p_spudec->p_fifo, NULL, NULL );

    /* Mark thread as running and return */
    return 0;
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    if( p_spudec->p_vout != NULL 
     && p_spudec->p_vout->p_subpicture != NULL )
    {
        subpicture_t *  p_subpic;
        int             i_subpic;
    
        for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
        {
            p_subpic = &p_spudec->p_vout->p_subpicture[i_subpic];

            if( p_subpic != NULL &&
              ( ( p_subpic->i_status == RESERVED_SUBPICTURE )
             || ( p_subpic->i_status == READY_SUBPICTURE ) ) )
            {
                vout_DestroySubPicture( p_spudec->p_vout, p_subpic );
            }
        }

        vlc_object_release( p_spudec->p_vout );
    }
    
    free( p_spudec );
}

/*****************************************************************************
 * SyncPacket: get in sync with the stream
 *****************************************************************************
 * This function makes a few sanity checks and returns 0 if it looks like we
 * are at the beginning of a subpicture packet.
 *****************************************************************************/
static int SyncPacket( spudec_thread_t *p_spudec )
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
        return( 1 );
    }

    RemoveBits( &p_spudec->bit_stream, 16 );

    return( 0 );
}

/*****************************************************************************
 * ParsePacket: parse an SPU packet and send it to the video output
 *****************************************************************************
 * This function parses the SPU packet and, if valid, sends it to the
 * video output.
 *****************************************************************************/
static void ParsePacket( spudec_thread_t *p_spudec )
{
    subpicture_t * p_spu;
    u8           * p_src;
    unsigned int   i_offset;

    msg_Dbg( p_spudec->p_fifo, "trying to gather a 0x%.2x long subtitle",
                               p_spudec->i_spu_size );

    /* We cannot display a subpicture with no date */
    if( p_spudec->p_fifo->p_first->i_pts == 0 )
    {
        msg_Warn( p_spudec->p_fifo, "subtitle without a date" );
        return;
    }

    /* Allocate the subpicture internal data. */
    p_spu = vout_CreateSubPicture( p_spudec->p_vout, MEMORY_SUBPICTURE,
                                   sizeof( subpicture_sys_t )
                                    + p_spudec->i_rle_size * 4 );
    /* Rationale for the "p_spudec->i_rle_size * 4": we are going to
     * expand the RLE stuff so that we won't need to read nibbles later
     * on. This will speed things up a lot. Plus, we'll only need to do
     * this stupid interlacing stuff once. */

    if( p_spu == NULL )
    {
        return;
    }

    /* Fill the p_spu structure */
    p_spu->pf_render = RenderSPU;
    p_spu->p_sys->p_data = (u8*)p_spu->p_sys + sizeof( subpicture_sys_t );
    p_spu->p_sys->b_palette = 0;

    /* Get display time now. If we do it later, we may miss the PTS. */
    p_spu->p_sys->i_pts = p_spudec->p_fifo->p_first->i_pts;

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
    if( ParseControlSequences( p_spudec, p_spu ) )
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

    /* Clean up */
    free( p_src );
}

/*****************************************************************************
 * ParseControlSequences: parse all SPU control sequences
 *****************************************************************************
 * This is the most important part in SPU decoding. We get dates, palette
 * information, coordinates, and so on. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseControlSequences( spudec_thread_t *p_spudec,
                                  subpicture_t * p_spu )
{
    /* Our current index in the SPU packet */
    int i_index = p_spudec->i_rle_size + 4;

    /* The next start-of-control-sequence index and the previous one */
    int i_next_seq, i_cur_seq;

    /* Command time and date */
    u8  i_command;
    int i_date;

    int i, pi_alpha[4];

    /* XXX: temporary variables */
    vlc_bool_t b_force_display = 0;

    /* Initialize the structure */
    p_spu->i_start = p_spu->i_stop = 0;
    p_spu->b_ephemer = 0;

    do
    {
        /* Get the control sequence date */
        i_date = GetBits( &p_spudec->bit_stream, 16 );
 
        /* Next offset */
        i_cur_seq = i_index;
        i_next_seq = GetBits( &p_spudec->bit_stream, 16 );
 
        /* Skip what we just read */
        i_index += 4;
 
        do
        {
            i_command = GetBits( &p_spudec->bit_stream, 8 );
            i_index++;
 
            switch( i_command )
            {
                case SPU_CMD_FORCE_DISPLAY:

                    /* 00 (force displaying) */
                    p_spu->i_start = p_spu->p_sys->i_pts + ( i_date * 11000 );
                    b_force_display = 1;
 
                    break;
 
                /* Convert the dates in seconds to PTS values */
                case SPU_CMD_START_DISPLAY:
 
                    /* 01 (start displaying) */
                    p_spu->i_start = p_spu->p_sys->i_pts + ( i_date * 11000 );
 
                    break;
 
                case SPU_CMD_STOP_DISPLAY:
 
                    /* 02 (stop displaying) */
                    p_spu->i_stop = p_spu->p_sys->i_pts + ( i_date * 11000 );
 
                    break;
 
                case SPU_CMD_SET_PALETTE:
 
                    /* 03xxxx (palette) */
                    if( p_spudec->p_fifo->p_demux_data &&
                         *(int*)p_spudec->p_fifo->p_demux_data == 0xBeeF )
                    {
                        u32 i_color;

                        p_spu->p_sys->b_palette = 1;
                        for( i = 0; i < 4 ; i++ )
                        {
                            i_color = ((u32*)((char*)p_spudec->p_fifo->
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
 
                case SPU_CMD_SET_ALPHACHANNEL:
 
                    /* 04xxxx (alpha channel) */
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
                        msg_Warn( p_spudec->p_fifo,
                                  "ignoring blank alpha palette" );
                    }

                    i_index += 2;
 
                    break;
 
                case SPU_CMD_SET_COORDINATES:
 
                    /* 05xxxyyyxxxyyy (coordinates) */
                    p_spu->i_x = GetBits( &p_spudec->bit_stream, 12 );
                    p_spu->i_width = GetBits( &p_spudec->bit_stream, 12 )
                                      - p_spu->i_x + 1;
 
                    p_spu->i_y = GetBits( &p_spudec->bit_stream, 12 );
                    p_spu->i_height = GetBits( &p_spudec->bit_stream, 12 )
                                       - p_spu->i_y + 1;
 
                    i_index += 6;
 
                    break;
 
                case SPU_CMD_SET_OFFSETS:
 
                    /* 06xxxxyyyy (byte offsets) */
                    p_spu->p_sys->pi_offset[0] =
                        GetBits( &p_spudec->bit_stream, 16 ) - 4;
 
                    p_spu->p_sys->pi_offset[1] =
                        GetBits( &p_spudec->bit_stream, 16 ) - 4;
 
                    i_index += 4;
 
                    break;
 
                case SPU_CMD_END:
 
                    /* ff (end) */
                    break;
 
                default:
 
                    /* xx (unknown command) */
                    msg_Err( p_spudec->p_fifo, "unknown command 0x%.2x",
                                               i_command );
                    return( 1 );
            }

            /* We need to check for quit commands here */
            if( p_spudec->p_fifo->b_die )
            {
                return( 1 );
            }

        } while( i_command != SPU_CMD_END );

    } while( i_index == i_next_seq );

    /* Check that the next sequence index matches the current one */
    if( i_next_seq != i_cur_seq )
    {
        msg_Err( p_spudec->p_fifo, "index mismatch (0x%.4x != 0x%.4x)",
                                   i_next_seq, i_cur_seq );
        return( 1 );
    }

    if( i_index > p_spudec->i_spu_size )
    {
        msg_Err( p_spudec->p_fifo, "uh-oh, we went too far (0x%.4x > 0x%.4x)",
                                   i_index, p_spudec->i_spu_size );
        return( 1 );
    }

    if( !p_spu->i_start )
    {
        msg_Err( p_spudec->p_fifo, "no `start display' command" );
    }

    if( !p_spu->i_stop )
    {
        /* This subtitle will live for 5 seconds or until the next subtitle */
        p_spu->i_stop = p_spu->i_start + 500 * 11000;
        p_spu->b_ephemer = 1;
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

    if( b_force_display )
    {
        msg_Err( p_spudec->p_fifo, "\"force display\" command" );
        msg_Err( p_spudec->p_fifo, "send mail to <sam@zoy.org> if you "
                                   "want to help debugging this" );
    }

    /* Successfully parsed ! */
    return( 0 );
}

/*****************************************************************************
 * ParseRLE: parse the RLE part of the subtitle
 *****************************************************************************
 * This part parses the subtitle graphical data and stores it in a more
 * convenient structure for later decoding. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseRLE( spudec_thread_t *p_spudec,
                     subpicture_t * p_spu, u8 * p_src )
{
    unsigned int i_code;

    unsigned int i_width = p_spu->i_width;
    unsigned int i_height = p_spu->i_height;
    unsigned int i_x, i_y;

    u16 *p_dest = (u16 *)p_spu->p_sys->p_data;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  i_id = 0;                   /* Start on the even SPU layer */
    unsigned int  pi_table[ 2 ];
    unsigned int *pi_offset;

    vlc_bool_t b_empty_top = 1,
               b_empty_bottom = 0;
    unsigned int i_skipped_top = 0,
                 i_skipped_bottom = 0;

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
                                return( 1 );
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
                return( 1 );
            }

            /* Try to find the border color */
            if( p_spu->p_sys->pi_alpha[ i_code & 0x3 ] != 0x00 )
            {
                i_border = i_code & 0x3;
                stats[i_border] += i_code >> 2;
            }

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

                    b_empty_bottom = 1;
                    i_skipped_bottom++;
                }
            }
            else
            {
                /* We got a valid code, store it */
                *p_dest++ = i_code;

                /* Valid code means no blank line */
                b_empty_top = 0;
                b_empty_bottom = 0;
                i_skipped_bottom = 0;
            }
        }

        /* Check that we didn't go too far */
        if( i_x > i_width )
        {
            msg_Err( p_spudec->p_fifo, "i_x overflowed, %i > %i",
                                       i_x, i_width );
            return( 1 );
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

        return( 1 );
    }

    msg_Dbg( p_spudec->p_fifo, "valid subtitle, size: %ix%i, position: %i,%i",
             p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );

    /* Crop if necessary */
    if( i_skipped_top || i_skipped_bottom )
    {
        p_spu->i_y += i_skipped_top;
        p_spu->i_height -= i_skipped_top + i_skipped_bottom;

        msg_Dbg( p_spudec->p_fifo, "cropped to: %ix%i, position: %i,%i",
                 p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );
    }

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

    return( 0 );
}

/*****************************************************************************
 * RenderSPU: draw an SPU on a picture
 *****************************************************************************
 * This is a fast implementation of the subpicture drawing code. The data
 * has been preprocessed once, so we don't need to parse the RLE buffer again
 * and again. Most sanity checks are already done so that this routine can be
 * as fast as possible.
 *****************************************************************************/
static void RenderSPU( vout_thread_t *p_vout, picture_t *p_pic,
                       const subpicture_t *p_spu )
{
    /* Common variables */
    u16  p_clut16[4];
    u32  p_clut32[4];
    u8  *p_dest;
    u8  *p_destptr = (u8 *)p_dest;
    u16 *p_source = (u16 *)p_spu->p_sys->p_data;

    int i_x, i_y;
    int i_len, i_color, i_colprecomp, i_destalpha;
    u8  i_cnt;

    /* RGB-specific */
    int i_xscale, i_yscale, i_width, i_height, i_ytmp, i_yreal, i_ynext;

    switch( p_vout->output.i_chroma )
    {
    /* I420 target, no scaling */
    case FOURCC_I420:
    case FOURCC_IYUV:
    case FOURCC_YV12:

    p_dest = p_pic->Y_PIXELS + p_spu->i_x + p_spu->i_width
              + p_pic->Y_PITCH * ( p_spu->i_y + p_spu->i_height );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = p_spu->i_height * p_pic->Y_PITCH ;
         i_y ;
         i_y -= p_pic->Y_PITCH )
    {
        /* Draw until we reach the end of the line */
        for( i_x = p_spu->i_width ; i_x ; )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;

            switch( p_spu->p_sys->pi_alpha[ i_color ] )
            {
                case 0x00:
                    break;

                case 0x0f:
                    memset( p_dest - i_x - i_y,
                            p_spu->p_sys->pi_yuv[i_color][0], i_len );
                    break;

                default:
                    /* To be able to divide by 16 (>>4) we add 1 to the alpha.
                     * This means Alpha 0 won't be completely transparent, but
                     * that's handled in a special case above anyway. */
                    i_colprecomp = p_spu->p_sys->pi_yuv[i_color][0]
                                    * (p_spu->p_sys->pi_alpha[ i_color ] + 1);
                    i_destalpha = 15 - p_spu->p_sys->pi_alpha[ i_color ];

                    for ( p_destptr = p_dest - i_x - i_y;
                          p_destptr < p_dest - i_x - i_y + i_len;
                          p_destptr++ )
                    {
                        *p_destptr = ( i_colprecomp +
                                        *p_destptr * i_destalpha ) >> 4;
                    }
                    break;

            }
            i_x -= i_len;
        }
    }

    break;

    /* RV16 target, scaling */
    case FOURCC_RV16:

    /* XXX: this is a COMPLETE HACK, memcpy is unable to do u16s anyway */
    /* FIXME: get this from the DVD */
    for( i_color = 0; i_color < 4; i_color++ )
    {
        p_clut16[i_color] = 0x1111
                             * ( (u16)p_spu->p_sys->pi_yuv[i_color][0] >> 4 );
    }

    i_xscale = ( p_vout->output.i_width << 6 ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << 6 ) / p_vout->render.i_height;

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;

    p_dest = p_pic->p->p_pixels + ( i_width >> 6 ) * 2
              /* Add the picture coordinates and the SPU coordinates */
              + ( (p_spu->i_x * i_xscale) >> 6 ) * 2
              + ( (p_spu->i_y * i_yscale) >> 6 ) * p_pic->p->i_pitch;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0 ; i_y < i_height ; )
    {
        i_ytmp = i_y >> 6;
        i_y += i_yscale;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Just one line : we precalculate i_y >> 6 */
            i_yreal = p_pic->p->i_pitch * i_ytmp;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; )
            {
                /* Get the RLE part, then draw the line */
                i_color = *p_source & 0x3;

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    i_x -= i_xscale * ( *p_source++ >> 2 );
                    break;

                case 0x0f:
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    memset( p_dest - 2 * ( i_x >> 6 ) + i_yreal,
                            p_clut16[ i_color ],
                            2 * ( ( i_len >> 6 ) + 1 ) );
                    i_x -= i_len;
                    break;

                default:
                    /* FIXME: we should do transparency */
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    memset( p_dest - 2 * ( i_x >> 6 ) + i_yreal,
                            p_clut16[ i_color ],
                            2 * ( ( i_len >> 6 ) + 1 ) );
                    i_x -= i_len;
                    break;
                }

            }
        }
        else
        {
            i_yreal = p_pic->p->i_pitch * i_ytmp;
            i_ynext = p_pic->p->i_pitch * i_y >> 6;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; )
            {
                /* Get the RLE part, then draw as many lines as needed */
                i_color = *p_source & 0x3;

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    i_x -= i_xscale * ( *p_source++ >> 2 );
                    break;

                case 0x0f:
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 2 * ( i_x >> 6 ) + i_ytmp,
                                p_clut16[ i_color ],
                                2 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    i_x -= i_len;
                    break;

                default:
                    /* FIXME: we should do transparency */
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 2 * ( i_x >> 6 ) + i_ytmp,
                                p_clut16[ i_color ],
                                2 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    i_x -= i_len;
                    break;
                }
            }
        }
    }

    break;

    /* RV32 target, scaling */
    case FOURCC_RV24:
    case FOURCC_RV32:

    /* XXX: this is a COMPLETE HACK, memcpy is unable to do u32s anyway */
    /* FIXME: get this from the DVD */
    for( i_color = 0; i_color < 4; i_color++ )
    {
        p_clut32[i_color] = 0x11111111
                             * ( (u16)p_spu->p_sys->pi_yuv[i_color][0] >> 4 );
    }

    i_xscale = ( p_vout->output.i_width << 6 ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << 6 ) / p_vout->render.i_height;

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;

    p_dest = p_pic->p->p_pixels + ( i_width >> 6 ) * 4
              /* Add the picture coordinates and the SPU coordinates */
              + ( (p_spu->i_x * i_xscale) >> 6 ) * 4
              + ( (p_spu->i_y * i_yscale) >> 6 ) * p_pic->p->i_pitch;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0 ; i_y < i_height ; )
    {
        i_ytmp = i_y >> 6;
        i_y += i_yscale;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Just one line : we precalculate i_y >> 6 */
            i_yreal = p_pic->p->i_pitch * i_ytmp;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; )
            {
                /* Get the RLE part, then draw the line */
                i_color = *p_source & 0x3;

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    i_x -= i_xscale * ( *p_source++ >> 2 );
                    break;

                case 0x0f:
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    memset( p_dest - 4 * ( i_x >> 6 ) + i_yreal,
                            p_clut32[ i_color ], 4 * ( ( i_len >> 6 ) + 1 ) );
                    i_x -= i_len;
                    break;

                default:
                    /* FIXME: we should do transparency */
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    memset( p_dest - 4 * ( i_x >> 6 ) + i_yreal,
                            p_clut32[ i_color ], 4 * ( ( i_len >> 6 ) + 1 ) );
                    i_x -= i_len;
                    break;
                }

            }
        }
        else
        {
            i_yreal = p_pic->p->i_pitch * i_ytmp;
            i_ynext = p_pic->p->i_pitch * i_y >> 6;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; )
            {
                /* Get the RLE part, then draw as many lines as needed */
                i_color = *p_source & 0x3;

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    i_x -= i_xscale * ( *p_source++ >> 2 );
                    break;

                case 0x0f:
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 4 * ( i_x >> 6 ) + i_ytmp,
                                p_clut32[ i_color ],
                                4 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    i_x -= i_len;
                    break;

                default:
                    /* FIXME: we should do transparency */
                    i_len = i_xscale * ( *p_source++ >> 2 );
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 4 * ( i_x >> 6 ) + i_ytmp,
                                p_clut32[ i_color ],
                                4 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    i_x -= i_len;
                    break;
                }
            }
        }
    }

    break;

    /* NVidia overlay, no scaling */
    case FOURCC_YUY2:

    p_dest = p_pic->p->p_pixels +
              (p_spu->i_x + p_spu->i_width +
               p_vout->output.i_width * ( p_spu->i_y + p_spu->i_height )) * 2;
    /* Draw until we reach the bottom of the subtitle */
    for( i_y = p_spu->i_height * p_vout->output.i_width;
         i_y ;
         i_y -= p_vout->output.i_width )
    {
        /* Draw until we reach the end of the line */
        for( i_x = p_spu->i_width ; i_x ; )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;

            switch( p_spu->p_sys->pi_alpha[ i_color ] )
            {
            case 0x00:
                i_x -= *p_source++ >> 2;
                break;

            case 0x0f:
                i_len = *p_source++ >> 2;
                for( i_cnt = 0; i_cnt < i_len; i_cnt++ )
                {
                    /* draw a pixel */
                    /* Y */
                    memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2,
                            p_spu->p_sys->pi_yuv[i_color][0], 1);

                    if (!(i_cnt & 0x01))
                    {
                        /* U and V */
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 1,
                                0x80, 1);
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 3,
                                0x80, 1);
                    }
                }
                i_x -= i_len;
                break;

            default:
                /* FIXME: we should do transparency */
                i_len = *p_source++ >> 2;
                for( i_cnt = 0; i_cnt < i_len; i_cnt++ )
                {
                    /* draw a pixel */
                    /* Y */
                    memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2,
                            p_spu->p_sys->pi_yuv[i_color][0], 1);

                    if (!(i_cnt & 0x01))
                    {
                        /* U and V */
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 1,
                                0x80, 1);
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 3,
                                0x80, 1);
                    }
                }
                i_x -= i_len;
                break;
            }
        }
    }

    break;


    default:
        msg_Err( p_vout, "unknown chroma, can't render SPU" );
        break;
    }
}
