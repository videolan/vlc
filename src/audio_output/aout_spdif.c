/*****************************************************************************
 * aout_spdif: ac3 passthrough output
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: aout_spdif.c,v 1.5 2001/05/15 01:01:44 stef Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>                                              /* memset() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"                             /* mtime_t, mdate(), msleep() */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "aout_common.h"

#define BLANK_FRAME_MAX 1000

/*****************************************************************************
 * aout_SpdifThread: audio output thread that sends raw spdif data
 *                   to an external decoder
 *****************************************************************************
 * This output thread is quite specific as it can only handle one fifo now.
 *
 * Note: spdif can demux up to 8 ac3 streams, and can even take
 * care of time stamps (cf ac3 spec).
 *****************************************************************************/
void aout_SpdifThread( aout_thread_t * p_aout )
{
    u8          pi_spdif_blank [9] =
                    { 0x72, 0xf8, 0x1f, 0x4e, 0x01, 0x00, 0x08, 0x00, 0x77 };
    u8          pi_blank[SPDIF_FRAME_SIZE];
    int         i_fifo;
    int         i_frame;
    int         i_blank;

    /* get a blank frame ready */
    memset( pi_blank, 0, sizeof(pi_blank) );
    memcpy( pi_blank, pi_spdif_blank, sizeof(pi_spdif_blank) );
   
    intf_WarnMsg( 3, "aout info: starting spdif output loop" );

    /* variable used to compute the nnumber of blank frames since the
     * last significant frame */
    i_blank = 0;

    while( !p_aout->b_die )
    {
        /* we leave some time for aout fifo to fill and not to stress
         * the external decoder too much */
        msleep( 10000 );

        /* variable to check that we send data to the decoder
         * once per loop at least */
        i_frame = 0;

        /* FIXME: find a way to hnadle the locks here */
        for( i_fifo = 0 ; i_fifo < AOUT_MAX_FIFOS ; i_fifo++ )
        {
            /* the loop read each fifo so that we can change the stream
             * on the fly but mulitplexing is not handled yet so
             * the sound will be broken is more than one fifo has data */ 
            /* TODO: write the muliplexer :) */
            if( p_aout->fifo[i_fifo].i_type == AOUT_ADEC_SPDIF_FIFO )
            {
//                vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                if( p_aout->fifo[i_fifo].b_die )
                {
//                    vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
                    aout_FreeFifo( &p_aout->fifo[i_fifo] );
                }
                else if( !AOUT_FIFO_ISEMPTY( p_aout->fifo[i_fifo] ) )
                {
//                    vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
//fprintf(stderr, "delay %lld\n",p_aout->fifo[i_fifo].date[p_aout->fifo[i_fifo].l_start_frame]  -mdate() );
                   
                    /* play spdif frame to the external decoder */
                    p_aout->pf_play( p_aout,
                                     p_aout->fifo[i_fifo].buffer +
                                        p_aout->fifo[i_fifo].l_start_frame*
                                        SPDIF_FRAME_SIZE,
                                     p_aout->fifo[i_fifo].l_frame_size );
        
//                    vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                    p_aout->fifo[i_fifo].l_start_frame = 
                        (p_aout->fifo[i_fifo].l_start_frame + 1 )
                        & AOUT_FIFO_SIZE;
//                    vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
                    
                    i_frame++;
                    i_blank = 0;
                }
            }
        }
        if( !i_frame )
        {
            /* insert blank frame for stream continuity to
             * the external decoder */
            p_aout->pf_play( p_aout, pi_blank, SPDIF_FRAME_SIZE );

            /* we kill the output if we don't have any stream */
            if( ++i_blank > BLANK_FRAME_MAX )
            {
                p_aout->b_die = 1;
            }
        }
    }
    
    intf_WarnMsg( 3, "aout info: exiting spdif loop" );

    return;
}

