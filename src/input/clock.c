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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

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

/* p_input->p->i_cr_average : Maximum number of samples used to compute the
 * dynamic average value.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1)
 */

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP (INT64_C(2000000)*100/9)

/* Latency introduced on DVDs with CR == 0 on chapter change - this is from
 * my dice --Meuuh */
#define CR_MEAN_PTS_GAP 300000

/*****************************************************************************
 * ClockToSysdate: converts a movie clock to system date
 *****************************************************************************/
static mtime_t ClockToSysdate( input_clock_t *cl, mtime_t i_clock )
{
    if( !cl->b_has_reference )
        return 0;

    return (i_clock - cl->cr_ref) * cl->i_rate / INPUT_RATE_DEFAULT +
           cl->sysdate_ref;
}

/*****************************************************************************
 * ClockCurrent: converts current system date to clock units
 *****************************************************************************
 * Caution : a valid reference point is needed for this to operate.
 *****************************************************************************/
static mtime_t ClockCurrent( input_clock_t *cl )
{
    assert( cl->b_has_reference );
    return (mdate() - cl->sysdate_ref) * INPUT_RATE_DEFAULT / cl->i_rate +
           cl->cr_ref;
}

/*****************************************************************************
 * ClockNewRef: writes a new clock reference
 *****************************************************************************/
static void ClockNewRef( input_clock_t *cl,
                         mtime_t i_clock, mtime_t i_sysdate )
{
    cl->b_has_reference = true;
    cl->cr_ref = i_clock;
    cl->sysdate_ref = i_sysdate ;
}

/*****************************************************************************
 * input_ClockInit: reinitializes the clock reference after a stream
 *                  discontinuity
 *****************************************************************************/
void input_ClockInit( input_clock_t *cl, bool b_master, int i_cr_average, int i_rate )
{
    cl->b_has_reference = false;

    cl->last_cr = 0;
    cl->last_pts = 0;
    cl->last_sysdate = 0;
    cl->cr_ref = 0;
    cl->sysdate_ref = 0;
    cl->delta_cr = 0;
    cl->i_delta_cr_residue = 0;
    cl->i_rate = i_rate;

    cl->i_cr_average = i_cr_average;

    cl->b_master = b_master;
}

/*****************************************************************************
 * input_ClockSetPCR: manages a clock reference
 *
 *  i_ck_stream: date in stream clock
 *  i_ck_system: date in system clock
 *****************************************************************************/
void input_ClockSetPCR( input_thread_t *p_input,
                        input_clock_t *cl,
                        mtime_t i_ck_stream, mtime_t i_ck_system )
{
    const bool b_synchronize = p_input->b_can_pace_control && cl->b_master;
    bool b_reset_reference = false;

    if( ( !cl->b_has_reference ) ||
        ( i_ck_stream == 0 && cl->last_cr != 0 ) )
    {
        cl->last_update = 0;

        /* */
        b_reset_reference= true;
    }
    else if ( cl->last_cr != 0 &&
              ( (cl->last_cr - i_ck_stream) > CR_MAX_GAP ||
                (cl->last_cr - i_ck_stream) < - CR_MAX_GAP ) )
    {
        /* Stream discontinuity, for which we haven't received a
         * warning from the stream control facilities (dd-edited
         * stream ?). */
        msg_Warn( p_input, "clock gap, unexpected stream discontinuity" );
        cl->last_pts = 0;

        /* */
        msg_Warn( p_input, "feeding synchro with a new reference point trying to recover from clock gap" );
        b_reset_reference= true;
    }
    if( b_reset_reference )
    {
        cl->delta_cr = 0;
        cl->i_delta_cr_residue = 0;

        /* Feed synchro with a new reference point. */
        ClockNewRef( cl, i_ck_stream,
                         __MAX( cl->last_pts + CR_MEAN_PTS_GAP, i_ck_system ) );
    }

    cl->last_cr = i_ck_stream;
    cl->last_sysdate = i_ck_system;

    if( !b_synchronize && i_ck_system - cl->last_update > 200000 )
    {
        /* Smooth clock reference variations. */
        const mtime_t i_extrapoled_clock = ClockCurrent( cl );
        /* Bresenham algorithm to smooth variations. */
        const mtime_t i_tmp = cl->delta_cr * (cl->i_cr_average - 1) +
                              ( i_extrapoled_clock - i_ck_stream ) * 1  +
                              cl->i_delta_cr_residue;

        cl->i_delta_cr_residue = i_tmp % cl->i_cr_average;
        cl->delta_cr           = i_tmp / cl->i_cr_average;

        cl->last_update = i_ck_system;
    }
}

/*****************************************************************************
 * input_ClockResetPCR:
 *****************************************************************************/
void input_ClockResetPCR( input_clock_t *cl )
{
    cl->b_has_reference = false;
    cl->last_pts = 0;
}

/*****************************************************************************
 * input_ClockGetTS: manages a PTS or DTS
 *****************************************************************************/
mtime_t input_ClockGetTS( input_thread_t * p_input,
                          input_clock_t *cl, mtime_t i_ts )
{
    if( !cl->b_has_reference )
        return 0;

    cl->last_pts = ClockToSysdate( cl, i_ts + cl->delta_cr );
    return cl->last_pts + p_input->i_pts_delay;
}

/*****************************************************************************
 * input_ClockSetRate:
 *****************************************************************************/
void input_ClockSetRate( input_clock_t *cl, int i_rate )
{
    /* Move the reference point */
    if( cl->b_has_reference )
        ClockNewRef( cl, cl->last_cr, cl->last_sysdate );

    cl->i_rate = i_rate;
}

/*****************************************************************************
 * input_ClockGetWakeup
 *****************************************************************************/
mtime_t input_ClockGetWakeup( input_thread_t *p_input, input_clock_t *cl )
{
    /* Not synchronized, we cannot wait */
    if( !cl->b_has_reference )
        return 0;

    /* We must not wait if not pace controled, or we are not the
     * master clock */
    if( !p_input->b_can_pace_control || !cl->b_master ||
        p_input->p->b_out_pace_control )
        return 0;

    /* */
    return ClockToSysdate( cl, cl->last_cr );
}

