/*****************************************************************************
 * input_clock.c: Clock/System date convertions, stream management
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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
#include <vlc_input.h>
#include "input_clock.h"

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
#define CR_MEAN_PTS_GAP (300000)

/*****************************************************************************
 * Structures
 *****************************************************************************/
struct input_clock_t
{
    /* Reference point */
    bool                    b_has_reference;
    struct
    {
        mtime_t i_clock;
        mtime_t i_system;
    } ref;

    /* Last point
     * It is used to detect unexpected stream discontinuities */
    struct
    {
        mtime_t i_clock;
        mtime_t i_system;
    } last;

    mtime_t last_pts;

    /* Clock drift */
    mtime_t i_delta_update; /* System time to wait for drift update */ 
    mtime_t i_delta;
    int     i_delta_residue;

    /* Current modifiers */
    bool    b_master;
    int     i_rate;

    /* Static configuration */
    int     i_cr_average;
};

/*****************************************************************************
 * ClockStreamToSystem: converts a movie clock to system date
 *****************************************************************************/
static mtime_t ClockStreamToSystem( input_clock_t *cl, mtime_t i_clock )
{
    if( !cl->b_has_reference )
        return 0;

    return ( i_clock - cl->ref.i_clock ) * cl->i_rate / INPUT_RATE_DEFAULT +
           cl->ref.i_system;
}

/*****************************************************************************
 * ClockSystemToStream: converts a system date to movie clock
 *****************************************************************************
 * Caution : a valid reference point is needed for this to operate.
 *****************************************************************************/
static mtime_t ClockSystemToStream( input_clock_t *cl, mtime_t i_system )
{
    assert( cl->b_has_reference );
    return ( i_system - cl->ref.i_system ) * INPUT_RATE_DEFAULT / cl->i_rate +
            cl->ref.i_clock;
}

/*****************************************************************************
 * ClockSetReference: writes a new clock reference
 *****************************************************************************/
static void ClockSetReference( input_clock_t *cl,
                               mtime_t i_clock, mtime_t i_system )
{
    cl->b_has_reference = true;
    cl->ref.i_clock = i_clock;
    cl->ref.i_system = i_system;
}

/*****************************************************************************
 * input_clock_New: create a new clock
 *****************************************************************************/
input_clock_t *input_clock_New( bool b_master, int i_cr_average, int i_rate )
{
    input_clock_t *cl = malloc( sizeof(*cl) );
    if( !cl )
        return NULL;

    cl->b_has_reference = false;
    cl->ref.i_clock = 0;
    cl->ref.i_system = 0;

    cl->last.i_clock = 0;
    cl->last.i_system = 0;
    cl->last_pts = 0;

    cl->i_delta = 0;
    cl->i_delta_residue = 0;

    cl->b_master = b_master;
    cl->i_rate = i_rate;

    cl->i_cr_average = i_cr_average;

    return cl;
}

/*****************************************************************************
 * input_clock_Delete: destroy a new clock
 *****************************************************************************/
void input_clock_Delete( input_clock_t *cl )
{
    free( cl );
}

/*****************************************************************************
 * input_clock_SetPCR: manages a clock reference
 *
 *  i_ck_stream: date in stream clock
 *  i_ck_system: date in system clock
 *****************************************************************************/
void input_clock_SetPCR( input_clock_t *cl,
                         vlc_object_t *p_log, bool b_can_pace_control,
                         mtime_t i_ck_stream, mtime_t i_ck_system )
{
    const bool b_synchronize = b_can_pace_control && cl->b_master;
    bool b_reset_reference = false;

    if( ( !cl->b_has_reference ) ||
        ( i_ck_stream == 0 && cl->last.i_clock != 0 ) )
    {
        cl->i_delta_update = 0;

        /* */
        b_reset_reference= true;
    }
    else if( cl->last.i_clock != 0 &&
             ( (cl->last.i_clock - i_ck_stream) > CR_MAX_GAP ||
               (cl->last.i_clock - i_ck_stream) < -CR_MAX_GAP ) )
    {
        /* Stream discontinuity, for which we haven't received a
         * warning from the stream control facilities (dd-edited
         * stream ?). */
        msg_Warn( p_log, "clock gap, unexpected stream discontinuity" );
        cl->last_pts = 0;

        /* */
        msg_Warn( p_log, "feeding synchro with a new reference point trying to recover from clock gap" );
        b_reset_reference= true;
    }
    if( b_reset_reference )
    {
        cl->i_delta = 0;
        cl->i_delta_residue = 0;

        /* Feed synchro with a new reference point. */
        ClockSetReference( cl, i_ck_stream,
                         __MAX( cl->last_pts + CR_MEAN_PTS_GAP, i_ck_system ) );
    }

    cl->last.i_clock = i_ck_stream;
    cl->last.i_system = i_ck_system;

    if( !b_synchronize && i_ck_system - cl->i_delta_update > 200000 )
    {
        /* Smooth clock reference variations. */
        const mtime_t i_extrapoled_clock = ClockSystemToStream( cl, i_ck_system );
        /* Bresenham algorithm to smooth variations. */
        const mtime_t i_tmp = cl->i_delta * (cl->i_cr_average - 1) +
                              ( i_extrapoled_clock - i_ck_stream ) * 1  +
                              cl->i_delta_residue;

        cl->i_delta_residue = i_tmp % cl->i_cr_average;
        cl->i_delta         = i_tmp / cl->i_cr_average;

        cl->i_delta_update = i_ck_system;
    }
}

/*****************************************************************************
 * input_clock_ResetPCR:
 *****************************************************************************/
void input_clock_ResetPCR( input_clock_t *cl )
{
    cl->b_has_reference = false;
    cl->last_pts = 0;
}

/*****************************************************************************
 * input_clock_GetTS: manages a PTS or DTS
 *****************************************************************************/
mtime_t input_clock_GetTS( input_clock_t *cl,
                           mtime_t i_pts_delay, mtime_t i_ts )
{
    if( !cl->b_has_reference )
        return 0;

    cl->last_pts = ClockStreamToSystem( cl, i_ts + cl->i_delta );
    return cl->last_pts + i_pts_delay;
}

/*****************************************************************************
 * input_clock_SetRate:
 *****************************************************************************/
void input_clock_SetRate( input_clock_t *cl, int i_rate )
{
    /* Move the reference point */
    if( cl->b_has_reference )
        ClockSetReference( cl, cl->last.i_clock, cl->last.i_system );

    cl->i_rate = i_rate;
}

/*****************************************************************************
 * input_clock_SetMaster:
 *****************************************************************************/
void input_clock_SetMaster( input_clock_t *cl, bool b_master )
{
    cl->b_master = b_master;
}

/*****************************************************************************
 * input_clock_GetWakeup
 *****************************************************************************/
mtime_t input_clock_GetWakeup( input_clock_t *cl )
{
    /* Not synchronized, we cannot wait */
    if( !cl->b_has_reference )
        return 0;

    /* We must not wait if we are not the master clock */
    if( !cl->b_master  )
        return 0;

    /* */
    return ClockStreamToSystem( cl, cl->last.i_clock );
}

