/*****************************************************************************
 * spu_decoder.c : spu decoder thread
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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
#include "debug.h"                                                 /* ASSERT */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "spu_decoder.h"

/*
 * Local prototypes
 */
static int      InitThread          ( spudec_thread_t *p_spudec );
static void     RunThread           ( spudec_thread_t *p_spudec );
static void     ErrorThread         ( spudec_thread_t *p_spudec );
static void     EndThread           ( spudec_thread_t *p_spudec );

/*****************************************************************************
 * spudec_CreateThread: create a spu decoder thread
 *****************************************************************************/
vlc_thread_t spudec_CreateThread( vdec_config_t * p_config )
{
    spudec_thread_t *     p_spudec;

    intf_DbgMsg("spudec debug: creating spu decoder thread");

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
        intf_ErrMsg("spudec error: can't spawn spu decoder thread");
        free( p_spudec );
        return( 0 );
    }

    intf_DbgMsg("spudec debug: spu decoder thread (%p) created", p_spudec);
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
    intf_DbgMsg("spudec debug: initializing spu decoder thread %p", p_spudec);

    p_spudec->p_config->decoder_config.pf_init_bit_stream(
            &p_spudec->bit_stream,
            p_spudec->p_config->decoder_config.p_decoder_fifo );

    /* Mark thread as running and return */
    intf_DbgMsg( "spudec debug: InitThread(%p) succeeded", p_spudec );
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
    intf_DbgMsg("spudec debug: running spu decoder thread (%p) (pid == %i)",
        p_spudec, getpid());

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
        int i_packet_size;
        int i_rle_size;
        int i_index, i_next;
        boolean_t b_valid;
        subpicture_t * p_spu = NULL;

        /* wait for the next SPU ID.
         * XXX: We trash 0xff bytes since they probably come from
         * an incomplete previous packet */
        do
        {
            i_packet_size = GetBits( &p_spudec->bit_stream, 8 );
        }
        while( i_packet_size == 0xff );

        if( p_spudec->p_fifo->b_die )
        {
            break;
        }

        /* the total size - should equal the sum of the
         * PES packet size that form the SPU packet */
        i_packet_size = i_packet_size << 8
                         | GetBits( &p_spudec->bit_stream, 8 );

        /* the RLE stuff size */
        i_rle_size = GetBits( &p_spudec->bit_stream, 16 );

        /* if the values we got aren't too strange, decode the data */
        if( i_rle_size < i_packet_size )
        {
            /* allocate the subpicture.
             * FIXME: we should check if the allocation failed */
            p_spu = vout_CreateSubPicture( p_spudec->p_vout,
                                           DVD_SUBPICTURE, i_rle_size );
            /* get display time */
            p_spu->begin_date = p_spu->end_date
                            = DECODER_FIFO_START(*p_spudec->p_fifo)->i_pts;

            /* get RLE data, skip 4 bytes for the first two read offsets */
            GetChunk( &p_spudec->bit_stream, p_spu->p_data,
                      i_rle_size - 4 );

            if( p_spudec->p_fifo->b_die )
            {
                break;
            }

            /* continue parsing after the RLE part */
            i_index = i_rle_size;

            /* assume packet is valid */
            b_valid = 1;

            /* getting the control part */
            do
            {
                unsigned char   i_cmd;
                u16             i_date;

                /* Get the sequence date */
                i_date = GetBits( &p_spudec->bit_stream, 16 );

                /* Next offset */
                i_next = GetBits( &p_spudec->bit_stream, 16 );

                i_index += 4;

                do
                {
                    i_cmd = GetBits( &p_spudec->bit_stream, 8 );
                    i_index++;

                    switch( i_cmd )
                    {
                        case SPU_CMD_FORCE_DISPLAY:
                            /* 00 (force displaying) */
                            break;
                        /* FIXME: here we have to calculate dates. It's
                         * around i_date * 12000 but I don't know
                         * how much exactly.
                         */
                        case SPU_CMD_START_DISPLAY:
                            /* 01 (start displaying) */
                            p_spu->begin_date += ( i_date * 12000 );
                            break;
                        case SPU_CMD_STOP_DISPLAY:
                            /* 02 (stop displaying) */
                            p_spu->end_date += ( i_date * 12000 );
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
                            p_spu->i_x =
                                GetBits( &p_spudec->bit_stream, 12 );

                            p_spu->i_width = p_spu->i_x -
                                GetBits( &p_spudec->bit_stream, 12 ) + 1;

                            p_spu->i_y =
                                GetBits( &p_spudec->bit_stream, 12 );

                            p_spu->i_height = p_spu->i_y -
                                GetBits( &p_spudec->bit_stream, 12 ) + 1;

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
                            intf_ErrMsg( "spudec: unknown command 0x%.2x",
                                         i_cmd );
                            b_valid = 0;
                            break;
                    }
                }
                while( b_valid && ( i_cmd != SPU_CMD_END ) );
            }
            while( b_valid && ( i_index == i_next ) );

            if( b_valid )
            {
                /* SPU is finished - we can tell the video output
                 * to display it */
                vout_DisplaySubPicture( p_spudec->p_vout, p_spu );
            }
            else
            {
                vout_DestroySubPicture( p_spudec->p_vout, p_spu );
            }
        }
        else 
        {
            /* Unexpected PES packet - trash it */
            intf_ErrMsg( "spudec: trying to recover from bad packet" );
            vlc_mutex_lock( &p_spudec->p_fifo->data_lock );
            p_spudec->p_fifo->pf_delete_pes( p_spudec->p_fifo->p_packets_mgt,
                                  DECODER_FIFO_START(*p_spudec->p_fifo) );
            DECODER_FIFO_INCSTART( *p_spudec->p_fifo );
            vlc_mutex_unlock( &p_spudec->p_fifo->data_lock );
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
        vlc_cond_wait( &p_spudec->p_fifo->data_wait, &p_spudec->p_fifo->data_lock );
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
    intf_DbgMsg( "spudec debug: destroying spu decoder thread %p", p_spudec );
    free( p_spudec->p_config );
    free( p_spudec );
    intf_DbgMsg( "spudec debug: spu decoder thread %p destroyed", p_spudec);
}

