/*****************************************************************************
 * spu_decoder.c : spu decoder thread
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
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
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <unistd.h>                                              /* getpid() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "spu_decoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  InitThread  ( spudec_thread_t * );
static void RunThread   ( spudec_thread_t * );
static void ErrorThread ( spudec_thread_t * );
static void EndThread   ( spudec_thread_t * );

static int  SyncPacket           ( spudec_thread_t * );
static void ParsePacket          ( spudec_thread_t * );
static int  ParseControlSequences( spudec_thread_t *, subpicture_t * );
static int  ParseRLE             ( u8 *,              subpicture_t * );

/*****************************************************************************
 * spudec_CreateThread: create a spu decoder thread
 *****************************************************************************/
vlc_thread_t spudec_CreateThread( vdec_config_t * p_config )
{
    spudec_thread_t *     p_spudec;

    /* Allocate the memory needed to store the thread's structure */
    p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) );

    if ( p_spudec == NULL )
    {
        intf_ErrMsg( "spudec error: not enough memory "
                     "for spudec_CreateThread() to create the new thread" );
        return( 0 );
    }

    /*
     * Initialize the thread properties
     */
    p_spudec->p_config = p_config;
    p_spudec->p_fifo = p_config->decoder_config.p_decoder_fifo;

    /* Get the video output informations */
    p_spudec->p_vout = p_config->p_vout;

    /* Spawn the spu decoder thread */
    if ( vlc_thread_create(&p_spudec->thread_id, "spu decoder",
         (vlc_thread_func_t)RunThread, (void *)p_spudec) )
    {
        intf_ErrMsg( "spudec error: can't spawn spu decoder thread" );
        free( p_spudec );
        return( 0 );
    }

    return( p_spudec->thread_id );
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
    p_spudec->p_config->decoder_config.pf_init_bit_stream(
            &p_spudec->bit_stream,
            p_spudec->p_config->decoder_config.p_decoder_fifo );

    /* Mark thread as running and return */
    return( 0 );
}

/*****************************************************************************
 * RunThread: spu decoder thread
 *****************************************************************************
 * spu decoder thread. This function only returns when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( spudec_thread_t *p_spudec )
{
    intf_WarnMsg( 1, "spudec: spu decoder thread %i spawned", getpid() );

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
        ErrorThread( p_spudec );
    }

    /* End of thread */
    intf_WarnMsg( 1, "spudec: destroying spu decoder thread %i", getpid() );
    EndThread( p_spudec );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( spudec_thread_t *p_spudec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_spudec->p_fifo->data_lock );

    /* Wait until a `die' order is sent */
    while( !p_spudec->p_fifo->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(*p_spudec->p_fifo) )
        {
            p_spudec->p_fifo->pf_delete_pes( p_spudec->p_fifo->p_packets_mgt,
                    DECODER_FIFO_START(*p_spudec->p_fifo) );
            DECODER_FIFO_INCSTART( *p_spudec->p_fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_spudec->p_fifo->data_wait,
                       &p_spudec->p_fifo->data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_spudec->p_fifo->data_lock );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    free( p_spudec->p_config );
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
    if( p_spudec->i_rle_size >= p_spudec->i_spu_size )
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

    /* We cannot display a subpicture with no date */
    if( DECODER_FIFO_START(*p_spudec->p_fifo)->i_pts == 0 )
    {
        return;
    }

    /* Allocate the subpicture internal data. */
    p_spu = vout_CreateSubPicture( p_spudec->p_vout, DVD_SUBPICTURE,
                                   p_spudec->i_rle_size * 4 );
    /* Rationale for the "p_spudec->i_rle_size * 4": we are going to
     * expand the RLE stuff so that we won't need to read nibbles later
     * on. This will speed things up a lot. Plus, we'll only need to do
     * this stupid interlacing stuff once. */

    if( p_spu == NULL )
    {
        return;
    }

    /* Get display time now. If we do it later, we may miss a PTS. */
    p_spu->begin_date = p_spu->end_date
                    = DECODER_FIFO_START(*p_spudec->p_fifo)->i_pts;

    /* Allocate the temporary buffer we will parse */
    p_src = malloc( p_spudec->i_rle_size );

    if( p_src == NULL )
    {
        intf_ErrMsg( "spudec error: could not allocate p_src" );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    /* Get RLE data */
    GetChunk( &p_spudec->bit_stream, p_src, p_spudec->i_rle_size );

#if 0
    /* Dump the subtitle info */
    intf_WarnHexDump( 0, p_spu->p_data, p_spudec->i_rle_size );
#endif

    /* Getting the control part */
    if( ParseControlSequences( p_spudec, p_spu ) )
    {
        /* There was a parse error, delete the subpicture */
        free( p_src );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    if( ParseRLE( p_src, p_spu ) )
    {
        /* There was a parse error, delete the subpicture */
        free( p_src );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    intf_WarnMsg( 1, "spudec: got a valid %ix%i subtitle at (%i,%i), "
                     "RLE offsets: 0x%x 0x%x",
                  p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y,
                  p_spu->type.spu.i_offset[0], p_spu->type.spu.i_offset[1] );

    /* SPU is finished - we can tell the video output to display it */
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
    int i_next_index = 0, i_prev_index;

    /* Command time and date */
    u8  i_command;
    int i_date;

    do
    {
        /* Get the control sequence date */
        i_date = GetBits( &p_spudec->bit_stream, 16 );
 
        /* Next offset */
        i_prev_index = i_next_index;
        i_next_index = GetBits( &p_spudec->bit_stream, 16 );
 
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
 
                    break;
 
                /* FIXME: here we have to calculate dates. It's around
                 * i_date * 12000 but I don't know how much exactly. */
                case SPU_CMD_START_DISPLAY:
 
                    /* 01 (start displaying) */
                    p_spu->begin_date += ( i_date * 11000 );
 
                    break;
 
                case SPU_CMD_STOP_DISPLAY:
 
                    /* 02 (stop displaying) */
                    p_spu->end_date += ( i_date * 11000 );
 
                    break;
 
                case SPU_CMD_SET_PALETTE:
 
                    /* 03xxxx (palette) - trashed */
                    RemoveBits( &p_spudec->bit_stream, 16 );
                    i_index += 2;
 
                    break;
 
                case SPU_CMD_SET_ALPHACHANNEL:
 
                    /* 04xxxx (alpha channel) - trashed */
                    RemoveBits( &p_spudec->bit_stream, 16 );
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
                    p_spu->type.spu.i_offset[0] =
                        GetBits( &p_spudec->bit_stream, 16 ) - 4;
 
                    p_spu->type.spu.i_offset[1] =
                        GetBits( &p_spudec->bit_stream, 16 ) - 4;
 
                    i_index += 4;
 
                    break;
 
                case SPU_CMD_END:
 
                    /* ff (end) */
 
                    break;
 
                default:
 
                    /* ?? (unknown command) */
                    intf_ErrMsg( "spudec error: unknown command 0x%.2x",
                                 i_command );
                    return( 1 );
            }

        } while( i_command != SPU_CMD_END );

    } while( i_index == i_next_index );

    /* Check that the last index matches the previous one */
    if( i_next_index != i_prev_index )
    {
        intf_ErrMsg( "spudec error: index mismatch (0x%.4x != 0x%.4x)",
                     i_next_index, i_prev_index );
        return( 1 );
    }

    if( i_index > p_spudec->i_spu_size )
    {
        intf_ErrMsg( "spudec error: uh-oh, we went too far (0x%.4x > 0x%.4x)",
                     i_index, p_spudec->i_spu_size );
        return( 1 );
    }

    /* Get rid of padding bytes */
    switch( p_spudec->i_spu_size - i_index )
    {
        case 1:

            RemoveBits( &p_spudec->bit_stream, 8 );
            i_index++;

        case 0:

            /* Zero or one padding byte, quite usual */

            break;

        default:

            /* More than one padding byte - this is very strange, but
             * we can deal with it */
            intf_WarnMsg( 2, "spudec warning: %i padding bytes",
                          p_spudec->i_spu_size - i_index );

            while( i_index < p_spudec->i_spu_size )
            {
                RemoveBits( &p_spudec->bit_stream, 8 );
                i_index++;
            }

            break;
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
static int ParseRLE( u8 *p_src, subpicture_t * p_spu )
{
    unsigned int i_code;
    unsigned int i_id = 0;

    unsigned int i_width = p_spu->i_width;
    unsigned int i_height = p_spu->i_height;
    unsigned int i_x, i_y;

    u16 *p_dest = (u16 *)p_spu->p_data;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  pi_table[2];
    unsigned int *pi_offset;
    pi_table[0] = p_spu->type.spu.i_offset[0] << 1;
    pi_table[1] = p_spu->type.spu.i_offset[1] << 1;

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
                                intf_ErrMsg( "spudec error: unknown code %.4x",
                                             i_code );
                                return( 1 );
                            }
                        }
                    }
                }
            }

            if( ( (i_code >> 2) + i_x + i_y * i_width ) > i_height * i_width )
            {
                intf_ErrMsg( "spudec error: out of bounds, %i at (%i,%i) is "
                             "out of %ix%i",
                             i_code >> 2, i_x, i_y, i_width, i_height);
                return( 1 );
            }

            /* We got a valid code, store it */
            *p_dest++ = i_code;
        }

        /* Check that we didn't go too far */
        if( i_x > i_width )
        {
            intf_ErrMsg( "spudec error: i_x overflowed, %i > %i",
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

    /* FIXME: we shouldn't need these padding bytes */
    while( i_y < i_height )
    {
        *p_dest++ = i_width << 2;
        i_y++;
    }

    return( 0 );
}

