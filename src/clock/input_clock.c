/*****************************************************************************
 * input_clock.c: Input clock/System date convertions, stream management
 *****************************************************************************
 * Copyright (C) 1999-2018 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "input_clock.h"
#include "clock_internal.h"
#include <assert.h>

/* TODO:
 * - clean up locking once clock code is stable
 *
 */

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

/* i_cr_average : Maximum number of samples used to compute the
 * dynamic average value.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1)
 */


/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP VLC_TICK_FROM_SEC(60)

/* Latency introduced on DVDs with CR == 0 on chapter change - this is from
 * my dice --Meuuh */
#define CR_MEAN_PTS_GAP VLC_TICK_FROM_MS(300)

/* Rate (in 1/256) at which we will read faster to try to increase our
 * internal buffer (if we control the pace of the source).
 */
#define CR_BUFFERING_RATE (48)

/* Extra internal buffer value (in CLOCK_FREQ)
 * It is 60s max, remember as it is limited by the size it takes by es_out.c
 * it can be really large.
 */
//#define CR_BUFFERING_TARGET VLC_TICK_FROM_SEC(60)
/* Due to some problems in es_out, we cannot use a large value yet */
#define CR_BUFFERING_TARGET VLC_TICK_FROM_MS(100)

/* */
#define INPUT_CLOCK_LATE_COUNT (3)

/* */
struct input_clock_t
{
    /* */
    vlc_mutex_t lock;

    /* Last point
     * It is used to detect unexpected stream discontinuities */
    clock_point_t last;

    /* Maximal timestamp returned by input_clock_ConvertTS (in system unit) */
    vlc_tick_t i_ts_max;

    /* Amount of extra buffering expressed in stream clock */
    vlc_tick_t i_buffering_duration;

    /* Clock drift */
    vlc_tick_t i_next_drift_update;
    average_t drift;

    /* Late statistics */
    struct
    {
        vlc_tick_t  pi_value[INPUT_CLOCK_LATE_COUNT];
        unsigned i_index;
    } late;

    /* Reference point */
    clock_point_t ref;
    bool          b_has_reference;

    /* External clock drift */
    vlc_tick_t    i_external_clock;
    bool          b_has_external_clock;

    /* Current modifiers */
    bool    b_paused;
    float   rate;
    vlc_tick_t i_pts_delay;
    vlc_tick_t i_pause_date;
};

static vlc_tick_t ClockStreamToSystem( input_clock_t *, vlc_tick_t i_stream );
static vlc_tick_t ClockSystemToStream( input_clock_t *, vlc_tick_t i_system );

static vlc_tick_t ClockGetTsOffset( input_clock_t * );

/*****************************************************************************
 * input_clock_New: create a new clock
 *****************************************************************************/
input_clock_t *input_clock_New( float rate )
{
    input_clock_t *cl = malloc( sizeof(*cl) );
    if( !cl )
        return NULL;

    vlc_mutex_init( &cl->lock );
    cl->b_has_reference = false;
    cl->ref = clock_point_Create( VLC_TICK_INVALID, VLC_TICK_INVALID );
    cl->b_has_external_clock = false;

    cl->last = clock_point_Create( VLC_TICK_INVALID, VLC_TICK_INVALID );

    cl->i_ts_max = VLC_TICK_INVALID;

    cl->i_buffering_duration = 0;

    cl->i_next_drift_update = VLC_TICK_INVALID;
    AvgInit( &cl->drift, 10 );

    cl->late.i_index = 0;
    for( int i = 0; i < INPUT_CLOCK_LATE_COUNT; i++ )
        cl->late.pi_value[i] = 0;

    cl->rate = rate;
    cl->i_pts_delay = 0;
    cl->b_paused = false;
    cl->i_pause_date = VLC_TICK_INVALID;

    return cl;
}

/*****************************************************************************
 * input_clock_Delete: destroy a new clock
 *****************************************************************************/
void input_clock_Delete( input_clock_t *cl )
{
    AvgClean( &cl->drift );
    free( cl );
}

/*****************************************************************************
 * input_clock_Update: manages a clock reference
 *
 *  i_ck_stream: date in stream clock
 *  i_ck_system: date in system clock
 *****************************************************************************/
vlc_tick_t input_clock_Update( input_clock_t *cl, vlc_object_t *p_log,
                         bool b_can_pace_control, bool b_buffering_allowed,
                         vlc_tick_t i_ck_stream, vlc_tick_t i_ck_system )
{
    bool b_reset_reference = false;

    assert( i_ck_stream != VLC_TICK_INVALID && i_ck_system != VLC_TICK_INVALID );

    vlc_mutex_lock( &cl->lock );

    if( !cl->b_has_reference )
    {
        /* */
        b_reset_reference= true;
    }
    else if( cl->last.stream != VLC_TICK_INVALID &&
             ( (cl->last.stream - i_ck_stream) > CR_MAX_GAP ||
               (cl->last.stream - i_ck_stream) < -CR_MAX_GAP ) )
    {
        /* Stream discontinuity, for which we haven't received a
         * warning from the stream control facilities (dd-edited
         * stream ?). */
        msg_Warn( p_log, "clock gap, unexpected stream discontinuity" );
        cl->i_ts_max = VLC_TICK_INVALID;

        /* */
        msg_Warn( p_log, "feeding synchro with a new reference point trying to recover from clock gap" );
        b_reset_reference= true;
    }

    /* */
    if( b_reset_reference )
    {
        cl->i_next_drift_update = VLC_TICK_INVALID;
        AvgReset( &cl->drift );

        /* Feed synchro with a new reference point. */
        cl->b_has_reference = true;
        cl->ref = clock_point_Create( __MAX( cl->i_ts_max + CR_MEAN_PTS_GAP, i_ck_system ),
                                      i_ck_stream );
        cl->b_has_external_clock = false;
    }

    /* Compute the drift between the stream clock and the system clock
     * when we don't control the source pace */
    if( !b_can_pace_control && cl->i_next_drift_update < i_ck_system )
    {
        const vlc_tick_t i_converted = ClockSystemToStream( cl, i_ck_system );

        AvgUpdate( &cl->drift, i_converted - i_ck_stream );

        cl->i_next_drift_update = i_ck_system + VLC_TICK_FROM_MS(200); /* FIXME why that */
    }

    /* Update the extra buffering value */
    if( !b_can_pace_control || b_reset_reference )
    {
        cl->i_buffering_duration = 0;
    }
    else if( b_buffering_allowed )
    {
        /* Try to bufferize more than necessary by reading
         * CR_BUFFERING_RATE/256 faster until we have CR_BUFFERING_TARGET.
         */
        const vlc_tick_t i_duration = __MAX( i_ck_stream - cl->last.stream, 0 );

        cl->i_buffering_duration += ( i_duration * CR_BUFFERING_RATE + 255 ) / 256;
        if( cl->i_buffering_duration > CR_BUFFERING_TARGET )
            cl->i_buffering_duration = CR_BUFFERING_TARGET;
    }
    //fprintf( stderr, "input_clock_Update: %d :: %lld\n", b_buffering_allowed, cl->i_buffering_duration/1000 );

    /* */
    cl->last = clock_point_Create( i_ck_system, i_ck_stream );

    /* It does not take the decoder latency into account but it is not really
     * the goal of the clock here */
    const vlc_tick_t i_system_expected = ClockStreamToSystem( cl, i_ck_stream + AvgGet( &cl->drift ) );
    const vlc_tick_t i_late = ( i_ck_system - cl->i_pts_delay ) - i_system_expected;
    if( i_late > 0 )
    {
        cl->late.pi_value[cl->late.i_index] = i_late;
        cl->late.i_index = ( cl->late.i_index + 1 ) % INPUT_CLOCK_LATE_COUNT;
    }

    vlc_mutex_unlock( &cl->lock );

    return i_late > 0 ? i_late : 0;
}

/*****************************************************************************
 * input_clock_Reset:
 *****************************************************************************/
void input_clock_Reset( input_clock_t *cl )
{
    vlc_mutex_lock( &cl->lock );

    cl->b_has_reference = false;
    cl->ref = clock_point_Create( VLC_TICK_INVALID, VLC_TICK_INVALID );
    cl->b_has_external_clock = false;
    cl->i_ts_max = VLC_TICK_INVALID;

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_ChangeRate:
 *****************************************************************************/
void input_clock_ChangeRate( input_clock_t *cl, float rate )
{
    vlc_mutex_lock( &cl->lock );

    if( cl->b_has_reference )
    {
        /* Move the reference point (as if we were playing at the new rate
         * from the start */
        cl->ref.system = cl->last.system - (vlc_tick_t) ((cl->last.system - cl->ref.system) / rate * cl->rate);
    }
    cl->rate = rate;

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_ChangePause:
 *****************************************************************************/
void input_clock_ChangePause( input_clock_t *cl, bool b_paused, vlc_tick_t i_date )
{
    vlc_mutex_lock( &cl->lock );
    assert( (!cl->b_paused) != (!b_paused) );

    if( cl->b_paused )
    {
        const vlc_tick_t i_duration = i_date - cl->i_pause_date;

        if( cl->b_has_reference && i_duration > 0 )
        {
            cl->ref.system += i_duration;
            cl->last.system += i_duration;
        }
    }
    cl->i_pause_date = i_date;
    cl->b_paused = b_paused;

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_GetWakeup
 *****************************************************************************/
vlc_tick_t input_clock_GetWakeup( input_clock_t *cl )
{
    vlc_tick_t i_wakeup = 0;

    vlc_mutex_lock( &cl->lock );

    /* Synchronized, we can wait */
    if( cl->b_has_reference )
        i_wakeup = ClockStreamToSystem( cl, cl->last.stream + AvgGet( &cl->drift ) - cl->i_buffering_duration );

    vlc_mutex_unlock( &cl->lock );

    return i_wakeup;
}

/*****************************************************************************
 * input_clock_ConvertTS
 *****************************************************************************/
int input_clock_ConvertTS( vlc_object_t *p_object, input_clock_t *cl,
                           float *p_rate, vlc_tick_t *pi_ts0, vlc_tick_t *pi_ts1,
                           vlc_tick_t i_ts_bound )
{
    assert( pi_ts0 );
    vlc_mutex_lock( &cl->lock );

    if( p_rate )
        *p_rate = cl->rate;

    if( !cl->b_has_reference )
    {
        vlc_mutex_unlock( &cl->lock );
        msg_Err(p_object, "Timestamp conversion failed for %"PRId64": "
                "no reference clock", *pi_ts0);
        *pi_ts0 = VLC_TICK_INVALID;
        if( pi_ts1 )
            *pi_ts1 = VLC_TICK_INVALID;
        return VLC_EGENERIC;
    }

    /* */
    const vlc_tick_t i_ts_buffering = cl->i_buffering_duration / cl->rate;
    const vlc_tick_t i_ts_delay = cl->i_pts_delay + ClockGetTsOffset( cl );

    /* */
    if( *pi_ts0 != VLC_TICK_INVALID )
    {
        *pi_ts0 = ClockStreamToSystem( cl, *pi_ts0 + AvgGet( &cl->drift ) );
        if( *pi_ts0 > cl->i_ts_max )
            cl->i_ts_max = *pi_ts0;
        *pi_ts0 += i_ts_delay;
    }

    /* XXX we do not update i_ts_max on purpose */
    if( pi_ts1 && *pi_ts1 != VLC_TICK_INVALID )
    {
        *pi_ts1 = ClockStreamToSystem( cl, *pi_ts1 + AvgGet( &cl->drift ) ) +
                  i_ts_delay;
    }

    vlc_mutex_unlock( &cl->lock );

    /* Check ts validity */
    if (i_ts_bound != INT64_MAX && *pi_ts0 != VLC_TICK_INVALID) {
        if (*pi_ts0 >= vlc_tick_now() + i_ts_delay + i_ts_buffering + i_ts_bound) {
            msg_Err(p_object,
                "Timestamp conversion failed (delay %"PRId64", buffering "
                "%"PRId64", bound %"PRId64")",
                i_ts_delay, i_ts_buffering, i_ts_bound);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}
/*****************************************************************************
 * input_clock_GetRate: Return current rate
 *****************************************************************************/
float input_clock_GetRate( input_clock_t *cl )
{
    vlc_mutex_lock( &cl->lock );
    float rate = cl->rate;
    vlc_mutex_unlock( &cl->lock );

    return rate;
}

int input_clock_GetState( input_clock_t *cl,
                          vlc_tick_t *pi_stream_start, vlc_tick_t *pi_system_start,
                          vlc_tick_t *pi_stream_duration, vlc_tick_t *pi_system_duration )
{
    vlc_mutex_lock( &cl->lock );

    if( !cl->b_has_reference )
    {
        vlc_mutex_unlock( &cl->lock );
        return VLC_EGENERIC;
    }

    *pi_stream_start = cl->ref.stream;
    *pi_system_start = cl->ref.system;

    *pi_stream_duration = cl->last.stream - cl->ref.stream;
    *pi_system_duration = cl->last.system - cl->ref.system;

    vlc_mutex_unlock( &cl->lock );

    return VLC_SUCCESS;
}

void input_clock_ChangeSystemOrigin( input_clock_t *cl, bool b_absolute, vlc_tick_t i_system )
{
    vlc_mutex_lock( &cl->lock );

    assert( cl->b_has_reference );
    vlc_tick_t i_offset;
    if( b_absolute )
    {
        i_offset = i_system - cl->ref.system - ClockGetTsOffset( cl );
    }
    else
    {
        if( !cl->b_has_external_clock )
        {
            cl->b_has_external_clock = true;
            cl->i_external_clock     = i_system;
        }
        i_offset = i_system - cl->i_external_clock;
    }

    cl->ref.system += i_offset;
    cl->last.system += i_offset;

    vlc_mutex_unlock( &cl->lock );
}

void input_clock_GetSystemOrigin( input_clock_t *cl, vlc_tick_t *pi_system, vlc_tick_t *pi_delay )
{
    vlc_mutex_lock( &cl->lock );

    assert( cl->b_has_reference );

    *pi_system = cl->ref.system;
    if( pi_delay )
        *pi_delay  = cl->i_pts_delay;

    vlc_mutex_unlock( &cl->lock );
}

#warning "input_clock_SetJitter needs more work"
void input_clock_SetJitter( input_clock_t *cl,
                            vlc_tick_t i_pts_delay, int i_cr_average )
{
    vlc_mutex_lock( &cl->lock );

    /* Update late observations */
    const vlc_tick_t i_delay_delta = i_pts_delay - cl->i_pts_delay;
    vlc_tick_t pi_late[INPUT_CLOCK_LATE_COUNT];
    for( int i = 0; i < INPUT_CLOCK_LATE_COUNT; i++ )
        pi_late[i] = __MAX( cl->late.pi_value[(cl->late.i_index + 1 + i)%INPUT_CLOCK_LATE_COUNT] - i_delay_delta, 0 );

    for( int i = 0; i < INPUT_CLOCK_LATE_COUNT; i++ )
        cl->late.pi_value[i] = 0;
    cl->late.i_index = 0;

    for( int i = 0; i < INPUT_CLOCK_LATE_COUNT; i++ )
    {
        if( pi_late[i] <= 0 )
            continue;
        cl->late.pi_value[cl->late.i_index] = pi_late[i];
        cl->late.i_index = ( cl->late.i_index + 1 ) % INPUT_CLOCK_LATE_COUNT;
    }

    /* TODO always save the value, and when rebuffering use the new one if smaller
     * TODO when increasing -> force rebuffering
     */
    if( cl->i_pts_delay < i_pts_delay )
        cl->i_pts_delay = i_pts_delay;

    /* */
    if( i_cr_average < 10 )
        i_cr_average = 10;

    if( cl->drift.range != i_cr_average )
        AvgRescale( &cl->drift, i_cr_average );

    vlc_mutex_unlock( &cl->lock );
}

vlc_tick_t input_clock_GetJitter( input_clock_t *cl )
{
    vlc_mutex_lock( &cl->lock );

#if INPUT_CLOCK_LATE_COUNT != 3
#   error "unsupported INPUT_CLOCK_LATE_COUNT"
#endif
    /* Find the median of the last late values
     * It works pretty well at rejecting bad values
     *
     * XXX we only increase pts_delay over time, decreasing it is
     * not that easy if we want to be robust.
     */
    const vlc_tick_t *p = cl->late.pi_value;
    vlc_tick_t i_late_median = p[0] + p[1] + p[2] - __MIN(__MIN(p[0],p[1]),p[2]) - __MAX(__MAX(p[0],p[1]),p[2]);
    vlc_tick_t i_pts_delay = cl->i_pts_delay ;

    vlc_mutex_unlock( &cl->lock );

    return i_pts_delay + i_late_median;
}

/*****************************************************************************
 * ClockStreamToSystem: converts a movie clock to system date
 *****************************************************************************/
static vlc_tick_t ClockStreamToSystem( input_clock_t *cl, vlc_tick_t i_stream )
{
    if( !cl->b_has_reference )
        return VLC_TICK_INVALID;

    return (vlc_tick_t) (( i_stream - cl->ref.stream ) / cl->rate) + cl->ref.system;
}

/*****************************************************************************
 * ClockSystemToStream: converts a system date to movie clock
 *****************************************************************************
 * Caution : a valid reference point is needed for this to operate.
 *****************************************************************************/
static vlc_tick_t ClockSystemToStream( input_clock_t *cl, vlc_tick_t i_system )
{
    assert( cl->b_has_reference );
    return (vlc_tick_t) (( i_system - cl->ref.system ) * cl->rate) + cl->ref.stream;
}

/**
 * It returns timestamp display offset due to ref/last modfied on rate changes
 * It ensures that currently converted dates are not changed.
 */
static vlc_tick_t ClockGetTsOffset( input_clock_t *cl )
{
    return cl->i_pts_delay * ( 1.0f / cl->rate - 1.0f );
}

