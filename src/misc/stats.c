/*****************************************************************************
 * stats.c: Statistics handling
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdio.h>                                               /* required */

#include "input/input_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int CounterUpdate( counter_t *p_counter,
                          vlc_value_t val, vlc_value_t * );

/*****************************************************************************
 * Exported functions
 *****************************************************************************/

/**
 * Create a statistics counter
 * \param i_type the type of stored data. One of VLC_VAR_STRING,
 * VLC_VAR_INTEGER, VLC_VAR_FLOAT
 * \param i_compute_type the aggregation type. One of STATS_LAST (always
 * keep the last value), STATS_COUNTER (increment by the passed value),
 * STATS_MAX (keep the maximum passed value), STATS_MIN, or STATS_DERIVATIVE
 * (keep a time derivative of the value)
 */
counter_t * stats_CounterCreate( int i_type, int i_compute_type )
{
    counter_t *p_counter = (counter_t*) malloc( sizeof( counter_t ) ) ;

    if( !p_counter ) return NULL;
    p_counter->i_compute_type = i_compute_type;
    p_counter->i_type = i_type;
    p_counter->i_samples = 0;
    p_counter->pp_samples = NULL;

    p_counter->update_interval = 0;
    p_counter->last_update = 0;

    return p_counter;
}

/** Update a counter element with new values
 * \param p_counter the counter to update
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see stats_Create
 * \param val_new a pointer that will be filled with new data
 */
int stats_Update( counter_t *p_counter,
                  vlc_value_t val, vlc_value_t *val_new )
{
    if( !p_counter ) return VLC_EGENERIC;
    return CounterUpdate( p_counter, val, val_new );
}

/** Get the aggregated value for a counter
 * \param p_this an object
 * \param p_counter the counter
 * \param val a pointer to an initialized vlc_value union. It will contain the
 * retrieved value
 * \return an error code
 */
int stats_Get( counter_t *p_counter, vlc_value_t *val )
{
    if( !p_counter || p_counter->i_samples == 0 )
    {
        val->i_int = 0;
        return VLC_EGENERIC;
    }

    switch( p_counter->i_compute_type )
    {
    case STATS_COUNTER:
        *val = p_counter->pp_samples[0]->value;
        break;
    case STATS_DERIVATIVE:
        /* Not ready yet */
        if( p_counter->i_samples < 2 )
        {
            val->i_int = 0;
            return VLC_EGENERIC;
        }
        if( p_counter->i_type == VLC_VAR_INTEGER )
        {
            float f = ( p_counter->pp_samples[0]->value.i_int -
                        p_counter->pp_samples[1]->value.i_int ) /
                    (float)(  p_counter->pp_samples[0]->date -
                              p_counter->pp_samples[1]->date );
            val->i_int = (int64_t)f;
        }
        else
        {
            float f = (float)( p_counter->pp_samples[0]->value.f_float -
                               p_counter->pp_samples[1]->value.f_float ) /
                      (float)( p_counter->pp_samples[0]->date -
                               p_counter->pp_samples[1]->date );
            val->f_float = f;
        }
        break;
    }
    return VLC_SUCCESS;;
}

input_stats_t *stats_NewInputStats( input_thread_t *p_input )
{
    (void)p_input;
    input_stats_t *p_stats = calloc( 1, sizeof(input_stats_t) );
    if( !p_stats )
        return NULL;

    vlc_mutex_init( &p_stats->lock );
    stats_ReinitInputStats( p_stats );

    return p_stats;
}

void stats_ComputeInputStats( input_thread_t *p_input, input_stats_t *p_stats )
{
    if( !libvlc_stats (p_input) ) return;

    vlc_mutex_lock( &p_input->p->counters.counters_lock );
    vlc_mutex_lock( &p_stats->lock );

    /* Input */
    stats_GetInteger( p_input->p->counters.p_read_packets,
                      &p_stats->i_read_packets );
    stats_GetInteger( p_input->p->counters.p_read_bytes,
                      &p_stats->i_read_bytes );
    stats_GetFloat( p_input->p->counters.p_input_bitrate,
                    &p_stats->f_input_bitrate );
    stats_GetInteger( p_input->p->counters.p_demux_read,
                      &p_stats->i_demux_read_bytes );
    stats_GetFloat( p_input->p->counters.p_demux_bitrate,
                    &p_stats->f_demux_bitrate );
    stats_GetInteger( p_input->p->counters.p_demux_corrupted,
                      &p_stats->i_demux_corrupted );
    stats_GetInteger( p_input->p->counters.p_demux_discontinuity,
                      &p_stats->i_demux_discontinuity );

    /* Decoders */
    stats_GetInteger( p_input->p->counters.p_decoded_video,
                      &p_stats->i_decoded_video );
    stats_GetInteger( p_input->p->counters.p_decoded_audio,
                      &p_stats->i_decoded_audio );

    /* Sout */
    if( p_input->p->counters.p_sout_send_bitrate )
    {
        stats_GetInteger( p_input->p->counters.p_sout_sent_packets,
                          &p_stats->i_sent_packets );
        stats_GetInteger( p_input->p->counters.p_sout_sent_bytes,
                          &p_stats->i_sent_bytes );
        stats_GetFloat  ( p_input->p->counters.p_sout_send_bitrate,
                          &p_stats->f_send_bitrate );
    }

    /* Aout */
    stats_GetInteger( p_input->p->counters.p_played_abuffers,
                      &p_stats->i_played_abuffers );
    stats_GetInteger( p_input->p->counters.p_lost_abuffers,
                      &p_stats->i_lost_abuffers );

    /* Vouts */
    stats_GetInteger( p_input->p->counters.p_displayed_pictures,
                      &p_stats->i_displayed_pictures );
    stats_GetInteger( p_input->p->counters.p_lost_pictures,
                      &p_stats->i_lost_pictures );

    vlc_mutex_unlock( &p_stats->lock );
    vlc_mutex_unlock( &p_input->p->counters.counters_lock );
}

void stats_ReinitInputStats( input_stats_t *p_stats )
{
    vlc_mutex_lock( &p_stats->lock );
    p_stats->i_read_packets = p_stats->i_read_bytes =
    p_stats->f_input_bitrate = p_stats->f_average_input_bitrate =
    p_stats->i_demux_read_packets = p_stats->i_demux_read_bytes =
    p_stats->f_demux_bitrate = p_stats->f_average_demux_bitrate =
    p_stats->i_demux_corrupted = p_stats->i_demux_discontinuity =
    p_stats->i_displayed_pictures = p_stats->i_lost_pictures =
    p_stats->i_played_abuffers = p_stats->i_lost_abuffers =
    p_stats->i_decoded_video = p_stats->i_decoded_audio =
    p_stats->i_sent_bytes = p_stats->i_sent_packets = p_stats->f_send_bitrate
     = 0;
    vlc_mutex_unlock( &p_stats->lock );
}

void stats_CounterClean( counter_t *p_c )
{
    if( p_c )
    {
        int i = p_c->i_samples - 1 ;
        while( i >= 0 )
        {
            counter_sample_t *p_s = p_c->pp_samples[i];
            REMOVE_ELEM( p_c->pp_samples, p_c->i_samples, i );
            free( p_s );
            i--;
        }
        free( p_c );
    }
}


/********************************************************************
 * Following functions are local
 ********************************************************************/

/**
 * Update a statistics counter, according to its type
 * If needed, perform a bit of computation (derivative, mostly)
 * This function must be entered with stats handler lock
 * \param p_counter the counter to update
 * \param val the "new" value
 * \return an error code
 */
static int CounterUpdate( counter_t *p_counter,
                          vlc_value_t val, vlc_value_t *new_val )
{
    switch( p_counter->i_compute_type )
    {
    case STATS_DERIVATIVE:
    {
        counter_sample_t *p_new, *p_old;
        mtime_t now = mdate();
        if( now - p_counter->last_update < p_counter->update_interval )
        {
            return VLC_EGENERIC;
        }
        p_counter->last_update = now;
        /* Insert the new one at the beginning */
        p_new = (counter_sample_t*)malloc( sizeof( counter_sample_t ) );
        p_new->value = val;
        p_new->date = p_counter->last_update;
        INSERT_ELEM( p_counter->pp_samples, p_counter->i_samples,
                     0, p_new );

        if( p_counter->i_samples == 3 )
        {
            p_old = p_counter->pp_samples[2];
            REMOVE_ELEM( p_counter->pp_samples, p_counter->i_samples, 2 );
            free( p_old );
        }
        break;
    }
    case STATS_COUNTER:
        if( p_counter->i_samples == 0 )
        {
            counter_sample_t *p_new = (counter_sample_t*)malloc(
                                               sizeof( counter_sample_t ) );
            p_new->value.i_int = 0;

            INSERT_ELEM( p_counter->pp_samples, p_counter->i_samples,
                         p_counter->i_samples, p_new );
        }
        if( p_counter->i_samples == 1 )
        {
            switch( p_counter->i_type )
            {
            case VLC_VAR_INTEGER:
                p_counter->pp_samples[0]->value.i_int += val.i_int;
                if( new_val )
                    new_val->i_int = p_counter->pp_samples[0]->value.i_int;
                break;
            case VLC_VAR_FLOAT:
                p_counter->pp_samples[0]->value.f_float += val.f_float;
                if( new_val )
                    new_val->f_float = p_counter->pp_samples[0]->value.f_float;
            }
        }
        break;
    }
    return VLC_SUCCESS;
}
