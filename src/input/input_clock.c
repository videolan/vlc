/*****************************************************************************
 * input_clock.c: Clock/System date conversions, stream management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_clock.c,v 1.2 2001/02/07 15:32:26 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

/*
 *   DISCUSSION : SYNCHRONIZATION METHOD
 *
 *   In some cases we can impose the pace of reading (when reading from a
 *   file or a pipe), and for the synchronization we simply sleep() until
 *   it is time to deliver the packet to the decoders. When reading from
 *   the network, we must be read at the same pace as the server writes,
 *   otherwise the kernel's buffer will trash packets. The risk is now to
 *   overflow the input buffers in case the server goes too fast, that is
 *   why we do these calculations :
 *
 *   We compute a mean for the pcr because we want to eliminate the
 *   network jitter and keep the low frequency variations. The mean is
 *   in fact a low pass filter and the jitter is a high frequency signal
 *   that is why it is eliminated by the filter/average.
 *
 *   The low frequency variations enable us to synchronize the client clock
 *   with the server clock because they represent the time variation between
 *   the 2 clocks. Those variations (ie the filtered pcr) are used to compute
 *   the presentation dates for the audio and video frames. With those dates
 *   we can decode (or trash) the MPEG2 stream at "exactly" the same rate
 *   as it is sent by the server and so we keep the synchronization between
 *   the server and the client.
 *
 *   It is a very important matter if you want to avoid underflow or overflow
 *   in all the FIFOs, but it may be not enough.
 */

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum number of samples used to compute the dynamic average value.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1) */
#define CR_MAX_AVERAGE_COUNTER 40

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP 1000000

/*****************************************************************************
 * ClockToSysdate: converts a movie clock to system date
 *****************************************************************************/
static mtime_t ClockToSysdate( input_thread_t * p_input,
                               pgrm_descriptor_t * p_pgrm, mtime_t i_clock )
{
    mtime_t     i_sysdate = 0;

    if( p_pgrm->i_synchro_state == SYNCHRO_OK )
    {
        i_sysdate = (i_clock - p_pgrm->cr_ref) 
                        * p_input->stream.control.i_rate
                        * 300
                        / 27
                        / DEFAULT_RATE
                        + p_pgrm->sysdate_ref;
    }

    return( i_sysdate );
}

/*****************************************************************************
 * ClockCurrent: converts current system date to clock units
 *****************************************************************************
 * Caution : the synchro state must be SYNCHRO_OK for this to operate.
 *****************************************************************************/
static mtime_t ClockCurrent( input_thread_t * p_input,
                             pgrm_descriptor_t * p_pgrm )
{
    return( (mdate() - p_pgrm->sysdate_ref) * 27 * DEFAULT_RATE
             / p_input->stream.control.i_rate / 300
             + p_pgrm->cr_ref );
}

/*****************************************************************************
 * input_ClockNewRef: writes a new clock reference
 *****************************************************************************/
void input_ClockNewRef( input_thread_t * p_input, pgrm_descriptor_t * p_pgrm,
                        mtime_t i_clock )
{
    p_pgrm->cr_ref = i_clock;
    p_pgrm->sysdate_ref = mdate();
}

/*****************************************************************************
 * input_EscapeDiscontinuity: send a NULL packet to the decoders
 *****************************************************************************/
void input_EscapeDiscontinuity( input_thread_t * p_input,
                                pgrm_descriptor_t * p_pgrm )
{
    int     i_es;

    for( i_es = 0; i_es < p_pgrm->i_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_pgrm->pp_es[i_es];

        if( p_es->p_decoder_fifo != NULL )
        {
            input_NullPacket( p_input, p_es );
        }
    }
}

/*****************************************************************************
 * input_ClockInit: reinitializes the clock reference after a stream
 *                  discontinuity
 *****************************************************************************/
void input_ClockInit( pgrm_descriptor_t * p_pgrm )
{
    p_pgrm->last_cr = 0;
    p_pgrm->cr_ref = 0;
    p_pgrm->sysdate_ref = 0;
    p_pgrm->delta_cr = 0;
    p_pgrm->c_average_count = 0;
}

/*****************************************************************************
 * input_ClockManageRef: manages a clock reference
 *****************************************************************************/
void input_ClockManageRef( input_thread_t * p_input,
                           pgrm_descriptor_t * p_pgrm, mtime_t i_clock )
{
    if( p_pgrm->i_synchro_state != SYNCHRO_OK )
    {
        /* Feed synchro with a new reference point. */
        input_ClockNewRef( p_input, p_pgrm, i_clock );
        p_pgrm->i_synchro_state = SYNCHRO_OK;
    }
    else
    {
        if ( p_pgrm->last_cr != 0 &&
               (    (p_pgrm->last_cr - i_clock) > CR_MAX_GAP
                 || (p_pgrm->last_cr - i_clock) < - CR_MAX_GAP ) )
        {
            /* Stream discontinuity, for which we haven't received a
             * warning from the stream control facilities (dd-edited
             * stream ?). */
            intf_WarnMsg( 3, "Clock gap, unexpected stream discontinuity" );
            input_ClockInit( p_pgrm );
            p_pgrm->i_synchro_state = SYNCHRO_START;
            input_EscapeDiscontinuity( p_input, p_pgrm );
        }

        p_pgrm->last_cr = i_clock;

        if( p_input->stream.b_pace_control
             && p_input->stream.pp_programs[0] == p_pgrm )
        {
            /* Wait a while before delivering the packets to the decoder.
             * In case of multiple programs, we arbitrarily follow the
             * clock of the first program. */
            mwait( ClockToSysdate( p_input, p_pgrm, i_clock ) );
        }
        else
        {
            /* Smooth clock reference variations. */
            mtime_t     i_extrapoled_clock = ClockCurrent( p_input, p_pgrm );

            /* Bresenham algorithm to smooth variations. */
            if( p_pgrm->c_average_count == CR_MAX_AVERAGE_COUNTER )
            {
                p_pgrm->delta_cr = ( p_pgrm->delta_cr
                                        * (CR_MAX_AVERAGE_COUNTER - 1)
                                      + i_extrapoled_clock )
                                    / CR_MAX_AVERAGE_COUNTER;
            }
            else
            {
                p_pgrm->delta_cr = ( p_pgrm->delta_cr
                                        * p_pgrm->c_average_count
                                      + i_extrapoled_clock )
                                    / (p_pgrm->c_average_count + 1);
                p_pgrm->c_average_count++;
            }
        }
    }
}

/*****************************************************************************
 * input_ClockGetTS: manages a PTS or DTS
 *****************************************************************************/
mtime_t input_ClockGetTS( input_thread_t * p_input,
                          pgrm_descriptor_t * p_pgrm, mtime_t i_ts )
{
    if( p_pgrm->i_synchro_state == SYNCHRO_OK )
    {
        return( ClockToSysdate( p_input, p_pgrm, i_ts + p_pgrm->delta_cr )
                 + DEFAULT_PTS_DELAY );
    }
    else
    {
        return 0;
    }
}

