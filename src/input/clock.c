/*****************************************************************************
 * input_clock.c: Clock/System date convertions, stream management
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
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
#include <stdlib.h>
#include <vlc/vlc.h>

#include <vlc/input.h>
#include "input_internal.h"

/*
 * DISCUSSION : SYNCHRONIZATION METHOD
 *
 * In some cases we can impose the pace of reading (when reading from a
 * file or a pipe), and for the synchronization we simply sleep() until
 * it is time to deliver the packet to the decoders. When reading from
 * the network, we must be read at the same pace as the server writes,
 * otherwise the kernel's buffer will trash packets. The risk is now to
 * overflow the input buffers in case the server goes too fast, that is
 * why we do these calculations :
 *
 * We compute a mean for the pcr because we want to eliminate the
 * network jitter and keep the low frequency variations. The mean is
 * in fact a low pass filter and the jitter is a high frequency signal
 * that is why it is eliminated by the filter/average.
 *
 * The low frequency variations enable us to synchronize the client clock
 * with the server clock because they represent the time variation between
 * the 2 clocks. Those variations (ie the filtered pcr) are used to compute
 * the presentation dates for the audio and video frames. With those dates
 * we can decode (or trash) the MPEG2 stream at "exactly" the same rate
 * as it is sent by the server and so we keep the synchronization between
 * the server and the client.
 *
 * It is a very important matter if you want to avoid underflow or overflow
 * in all the FIFOs, but it may be not enough.
 */

/* p_input->i_cr_average : Maximum number of samples used to compute the
 * dynamic average value.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1)
 */


static void ClockNewRef( input_clock_t * p_pgrm,
                         mtime_t i_clock, mtime_t i_sysdate );

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP 2000000

/* Latency introduced on DVDs with CR == 0 on chapter change - this is from
 * my dice --Meuuh */
#define CR_MEAN_PTS_GAP 300000

/*****************************************************************************
 * ClockToSysdate: converts a movie clock to system date
 *****************************************************************************/
static mtime_t ClockToSysdate( input_thread_t *p_input,
                               input_clock_t *cl, mtime_t i_clock )
{
    mtime_t     i_sysdate = 0;

    if( cl->i_synchro_state == SYNCHRO_OK )
    {
        i_sysdate = (mtime_t)(i_clock - cl->cr_ref)
                        * (mtime_t)p_input->i_rate
                        * (mtime_t)300;
        i_sysdate /= 27;
        i_sysdate /= 1000;
        i_sysdate += (mtime_t)cl->sysdate_ref;
    }

    return( i_sysdate );
}

/*****************************************************************************
 * ClockCurrent: converts current system date to clock units
 *****************************************************************************
 * Caution : the synchro state must be SYNCHRO_OK for this to operate.
 *****************************************************************************/
static mtime_t ClockCurrent( input_thread_t *p_input,
                             input_clock_t *cl )
{
    return( (mdate() - cl->sysdate_ref) * 27 * INPUT_RATE_DEFAULT
             / p_input->i_rate / 300
             + cl->cr_ref );
}

/*****************************************************************************
 * ClockNewRef: writes a new clock reference
 *****************************************************************************/
static void ClockNewRef( input_clock_t *cl,
                         mtime_t i_clock, mtime_t i_sysdate )
{
    cl->cr_ref = i_clock;
    cl->sysdate_ref = i_sysdate ;
}

/*****************************************************************************
 * input_ClockInit: reinitializes the clock reference after a stream
 *                  discontinuity
 *****************************************************************************/
void input_ClockInit( input_clock_t *cl, vlc_bool_t b_master, int i_cr_average )
{
    cl->i_synchro_state = SYNCHRO_START;

    cl->last_cr = 0;
    cl->last_pts = 0;
    cl->cr_ref = 0;
    cl->sysdate_ref = 0;
    cl->delta_cr = 0;
    cl->i_delta_cr_residue = 0;

    cl->i_cr_average = i_cr_average;

    cl->b_master = b_master;
}

#if 0
/*****************************************************************************
 * input_ClockManageControl: handles the messages from the interface
 *****************************************************************************
 * Returns UNDEF_S if nothing happened, PAUSE_S if the stream was paused
 *****************************************************************************/
int input_ClockManageControl( input_thread_t * p_input,
                               input_clock_t *cl, mtime_t i_clock )
{
#if 0
    vlc_value_t val;
    int i_return_value = UNDEF_S;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( p_input->stream.i_new_status == PAUSE_S )
    {
        int i_old_status;

        vlc_mutex_lock( &p_input->stream.control.control_lock );
        i_old_status = p_input->stream.control.i_status;
        p_input->stream.control.i_status = PAUSE_S;
        vlc_mutex_unlock( &p_input->stream.control.control_lock );

        vlc_cond_wait( &p_input->stream.stream_wait,
                       &p_input->stream.stream_lock );
        ClockNewRef( p_pgrm, i_clock, p_pgrm->last_pts > mdate() ?
                                      p_pgrm->last_pts : mdate() );

        if( p_input->stream.i_new_status == PAUSE_S )
        {
            /* PAUSE_S undoes the pause state: Return to old state. */
            vlc_mutex_lock( &p_input->stream.control.control_lock );
            p_input->stream.control.i_status = i_old_status;
            vlc_mutex_unlock( &p_input->stream.control.control_lock );

            p_input->stream.i_new_status = UNDEF_S;
            p_input->stream.i_new_rate = UNDEF_S;
        }

        /* We handle i_new_status != PAUSE_S below... */

        i_return_value = PAUSE_S;
    }

    if( p_input->stream.i_new_status != UNDEF_S )
    {
        vlc_mutex_lock( &p_input->stream.control.control_lock );

        p_input->stream.control.i_status = p_input->stream.i_new_status;

        ClockNewRef( p_pgrm, i_clock,
                     ClockToSysdate( p_input, p_pgrm, i_clock ) );

        if( p_input->stream.control.i_status == PLAYING_S )
        {
            p_input->stream.control.i_rate = DEFAULT_RATE;
            p_input->stream.control.b_mute = 0;
        }
        else
        {
            p_input->stream.control.i_rate = p_input->stream.i_new_rate;
            p_input->stream.control.b_mute = 1;

            /* Feed the audio decoders with a NULL packet to avoid
             * discontinuities. */
            input_EscapeAudioDiscontinuity( p_input );
        }

        val.i_int = p_input->stream.control.i_rate;
        var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );

        val.i_int = p_input->stream.control.i_status;
        var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

        p_input->stream.i_new_status = UNDEF_S;
        p_input->stream.i_new_rate = UNDEF_S;

        vlc_mutex_unlock( &p_input->stream.control.control_lock );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_return_value );
#endif
    return UNDEF_S;
}
#endif

/*****************************************************************************
 * input_ClockSetPCR: manages a clock reference
 *****************************************************************************/
void input_ClockSetPCR( input_thread_t *p_input,
                        input_clock_t *cl, mtime_t i_clock )
{
    if( ( cl->i_synchro_state != SYNCHRO_OK ) ||
        ( i_clock == 0 && cl->last_cr != 0 ) )
    {
        /* Feed synchro with a new reference point. */
        ClockNewRef( cl, i_clock,
                     cl->last_pts + CR_MEAN_PTS_GAP > mdate() ?
                     cl->last_pts + CR_MEAN_PTS_GAP : mdate() );
        cl->i_synchro_state = SYNCHRO_OK;

        if( p_input->b_can_pace_control && cl->b_master )
        {
            cl->last_cr = i_clock;
            if( !p_input->b_out_pace_control )
            {
                mtime_t i_wakeup = ClockToSysdate( p_input, cl, i_clock );
                while( (i_wakeup - mdate()) / CLOCK_FREQ > 1 )
                {
                    msleep( CLOCK_FREQ );
                    if( p_input->b_die ) i_wakeup = mdate();
                }
                mwait( i_wakeup );
            }
        }
        else
        {
            cl->last_cr = 0;
            cl->delta_cr = 0;
            cl->i_delta_cr_residue = 0;
        }
    }
    else
    {
        if ( cl->last_cr != 0 &&
               (    (cl->last_cr - i_clock) > CR_MAX_GAP
                 || (cl->last_cr - i_clock) < - CR_MAX_GAP ) )
        {
            /* Stream discontinuity, for which we haven't received a
             * warning from the stream control facilities (dd-edited
             * stream ?). */
            msg_Warn( p_input, "clock gap, unexpected stream discontinuity" );
            input_ClockInit( cl, cl->b_master, cl->i_cr_average );
            /* FIXME needed ? */
#if 0
            input_EscapeDiscontinuity( p_input );
#endif
        }

        cl->last_cr = i_clock;

        if( p_input->b_can_pace_control && cl->b_master )
        {
            /* Wait a while before delivering the packets to the decoder.
             * In case of multiple programs, we arbitrarily follow the
             * clock of the selected program. */
            if( !p_input->b_out_pace_control )
            {
                mtime_t i_wakeup = ClockToSysdate( p_input, cl, i_clock );
                while( (i_wakeup - mdate()) / CLOCK_FREQ > 1 )
                {
                    msleep( CLOCK_FREQ );
                    if( p_input->b_die ) i_wakeup = mdate();
                }
                mwait( i_wakeup );
            }
            /* FIXME Not needed anymore ? */
#if 0
            /* Now take into account interface changes. */
            input_ClockManageControl( p_input, cl, i_clock );
#endif
        }
        else
        {
            /* Smooth clock reference variations. */
            mtime_t i_extrapoled_clock = ClockCurrent( p_input, cl );
            mtime_t delta_cr;

            /* Bresenham algorithm to smooth variations. */
            delta_cr = ( cl->delta_cr * (cl->i_cr_average - 1)
                               + ( i_extrapoled_clock - i_clock )
                               + cl->i_delta_cr_residue )
                           / cl->i_cr_average;
            cl->i_delta_cr_residue = ( cl->delta_cr * (cl->i_cr_average - 1)
                                       + ( i_extrapoled_clock - i_clock )
                                       + cl->i_delta_cr_residue )
                                    % cl->i_cr_average;
            cl->delta_cr = delta_cr;
        }
    }
}

/*****************************************************************************
 * input_ClockGetTS: manages a PTS or DTS
 *****************************************************************************/
mtime_t input_ClockGetTS( input_thread_t * p_input,
                          input_clock_t *cl, mtime_t i_ts )
{
    if( cl->i_synchro_state != SYNCHRO_OK )
        return 0;

    cl->last_pts = ClockToSysdate( p_input, cl, i_ts + cl->delta_cr );
    return cl->last_pts + p_input->i_pts_delay;
}

