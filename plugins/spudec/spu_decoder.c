/*****************************************************************************
 * spu_decoder.c : spu decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: spu_decoder.c,v 1.5 2001/12/30 05:38:44 sam Exp $
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

#define MODULE_NAME spudec
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#ifdef WIN32                   /* getpid() for win32 is located in process.h */
#include <process.h>
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"
#include "video_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "spu_decoder.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  decoder_Probe ( probedata_t * );
static int  decoder_Run   ( decoder_config_t * );
static int  InitThread    ( spudec_thread_t * );
static void EndThread     ( spudec_thread_t * );

static int  SyncPacket           ( spudec_thread_t * );
static void ParsePacket          ( spudec_thread_t * );
static int  ParseControlSequences( spudec_thread_t *, subpicture_t * );
static int  ParseRLE             ( spudec_thread_t *, subpicture_t *, u8 * );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( spudec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for SPU decoder module" )
    ADD_COMMENT( "Nothing to configure" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_DEC;
    p_module->psz_longname = "subtitles decoder module";
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
static int decoder_Probe( probedata_t *p_data )
{
    if( p_data->i_type == DVD_SPU_ES )
        return( 50 );
    else
        return( 0 );
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run( decoder_config_t * p_config )
{
    spudec_thread_t *     p_spudec;
   
    intf_WarnMsg( 3, "spudec: thread launched. Initializing ..." );

    /* Allocate the memory needed to store the thread's structure */
    p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) );

    if ( p_spudec == NULL )
    {
        intf_ErrMsg( "spudec error: not enough memory "
                     "for spudec_CreateThread() to create the new thread" );
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }
    
    /*
     * Initialize the thread properties
     */
    p_spudec->p_config = p_config;

    p_spudec->p_fifo = p_config->p_decoder_fifo;
        
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
    }

    /* End of thread */
    EndThread( p_spudec );

    if( p_spudec->p_fifo->b_error )
    {
        return( -1 );
    }
   
    return( 0 );

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
    int i_retry = 0;

    /* Spawn a video output if there is none */
    vlc_mutex_lock( &p_vout_bank->lock );

    while( p_vout_bank->i_count == 0 )
    {
        vlc_mutex_unlock( &p_vout_bank->lock );

        if( i_retry++ > 10 )
        {
            intf_WarnMsg( 1, "spudec: waited too long for vout, aborting" );
            free( p_spudec );

            return( -1 );
        }

        msleep( VOUT_OUTMEM_SLEEP );
        vlc_mutex_lock( &p_vout_bank->lock );
    }

    /* Take the first video output FIXME: take the best one */
    p_spudec->p_vout = p_vout_bank->pp_vout[ 0 ];
    vlc_mutex_unlock( &p_vout_bank->lock );
    p_spudec->p_config->pf_init_bit_stream(
            &p_spudec->bit_stream,
            p_spudec->p_config->p_decoder_fifo, NULL, NULL );

    /* Mark thread as running and return */
    return( 0 );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
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

    intf_WarnMsg( 3, "spudec: trying to gather a 0x%.2x long subtitle",
                  p_spudec->i_spu_size );

    /* We cannot display a subpicture with no date */
    if( p_spudec->p_fifo->p_first->i_pts == 0 )
    {
        intf_WarnMsg( 3, "spudec error: subtitle without a date" );
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

    /* Get display time now. If we do it later, we may miss the PTS. */
    p_spudec->i_pts = p_spudec->p_fifo->p_first->i_pts;

    /* Allocate the temporary buffer we will parse */
    p_src = malloc( p_spudec->i_rle_size );

    if( p_src == NULL )
    {
        intf_ErrMsg( "spudec error: could not allocate p_src" );
        vout_DestroySubPicture( p_spudec->p_vout, p_spu );
        return;
    }

    /* Get RLE data */
    for( i_offset = 0;
         i_offset + SPU_CHUNK_SIZE < p_spudec->i_rle_size;
         i_offset += SPU_CHUNK_SIZE )
    {
        GetChunk( &p_spudec->bit_stream, p_src + i_offset, SPU_CHUNK_SIZE );

        /* Abort subtitle parsing if we were requested to stop */
        if( p_spudec->p_fifo->b_die )
        {
            free( p_src );
            vout_DestroySubPicture( p_spudec->p_vout, p_spu );
            return;
        }
    }

    GetChunk( &p_spudec->bit_stream, p_src + i_offset,
              p_spudec->i_rle_size - i_offset );

#if 0
    /* Dump the subtitle info */
    intf_WarnHexDump( 5, p_spu->p_data, p_spudec->i_rle_size );
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

    intf_WarnMsg( 3, "spudec: total size: 0x%x, RLE offsets: 0x%x 0x%x",
                  p_spudec->i_spu_size,
                  p_spu->type.spu.i_offset[0], p_spu->type.spu.i_offset[1] );

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

    /* XXX: temporary variables */
    boolean_t b_force_display = 0;

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
                    p_spu->i_start = p_spudec->i_pts + ( i_date * 11000 );
                    b_force_display = 1;
 
                    break;
 
                /* Convert the dates in seconds to PTS values */
                case SPU_CMD_START_DISPLAY:
 
                    /* 01 (start displaying) */
                    p_spu->i_start = p_spudec->i_pts + ( i_date * 11000 );
 
                    break;
 
                case SPU_CMD_STOP_DISPLAY:
 
                    /* 02 (stop displaying) */
                    p_spu->i_stop = p_spudec->i_pts + ( i_date * 11000 );
 
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
 
                    /* xx (unknown command) */
                    intf_ErrMsg( "spudec error: unknown command 0x%.2x",
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
        intf_ErrMsg( "spudec error: index mismatch (0x%.4x != 0x%.4x)",
                     i_next_seq, i_cur_seq );
        return( 1 );
    }

    if( i_index > p_spudec->i_spu_size )
    {
        intf_ErrMsg( "spudec error: uh-oh, we went too far (0x%.4x > 0x%.4x)",
                     i_index, p_spudec->i_spu_size );
        return( 1 );
    }

    if( !p_spu->i_start )
    {
        intf_ErrMsg( "spudec error: no `start display' command" );
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
            intf_WarnMsg( 2, "spudec warning: %i padding bytes, we usually "
                             "get 0 or 1 of them",
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
        intf_ErrMsg( "spudec: \"force display\" command" );
        intf_ErrMsg( "spudec: send mail to <sam@zoy.org> if you "
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

    u16 *p_dest = (u16 *)p_spu->p_data;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  i_id = 0;                   /* Start on the even SPU layer */
    unsigned int  pi_table[ 2 ];
    unsigned int *pi_offset;

    boolean_t b_empty_top = 1,
              b_empty_bottom = 0;
    unsigned int i_skipped_top = 0,
                 i_skipped_bottom = 0;

    pi_table[ 0 ] = p_spu->type.spu.i_offset[ 0 ] << 1;
    pi_table[ 1 ] = p_spu->type.spu.i_offset[ 1 ] << 1;

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
                                intf_ErrMsg( "spudec error: unknown RLE code "
                                             "0x%.4x", i_code );
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
                             i_code >> 2, i_x, i_y, i_width, i_height );
                return( 1 );
            }

            if( i_code == (i_width << 2) ) /* FIXME: we assume 0 is transp */
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

    /* We shouldn't get any padding bytes */
    if( i_y < i_height )
    {
        intf_ErrMsg( "spudec: padding bytes found in RLE sequence" );
        intf_ErrMsg( "spudec: send mail to <sam@zoy.org> if you "
                     "want to help debugging this" );

        /* Skip them just in case */
        while( i_y < i_height )
	{
            *p_dest++ = i_width << 2;
            i_y++;
	}

        return( 1 );
    }

    intf_WarnMsg( 3, "spudec: valid subtitle, size: %ix%i, position: %i,%i",
                  p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );

    /* Crop if necessary */
    if( i_skipped_top || i_skipped_bottom )
    {
        p_spu->i_y += i_skipped_top;
        p_spu->i_height -= i_skipped_top + i_skipped_bottom;

        intf_WarnMsg( 3, "spudec: cropped to: %ix%i, position: %i,%i",
                      p_spu->i_width, p_spu->i_height, p_spu->i_x, p_spu->i_y );
    }

    return( 0 );
}

