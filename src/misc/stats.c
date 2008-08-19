/*****************************************************************************
 * stats.c: Statistics handling
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdio.h>                                               /* required */

#include "input/input_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int CounterUpdate( vlc_object_t *p_this,
                          counter_t *p_counter,
                          vlc_value_t val, vlc_value_t * );
static void TimerDump( vlc_object_t *p_this, counter_t *p_counter, bool);

/*****************************************************************************
 * Exported functions
 *****************************************************************************/

/**
 * Create a statistics counter
 * \param p_this a VLC object
 * \param i_type the type of stored data. One of VLC_VAR_STRING,
 * VLC_VAR_INTEGER, VLC_VAR_FLOAT
 * \param i_compute_type the aggregation type. One of STATS_LAST (always
 * keep the last value), STATS_COUNTER (increment by the passed value),
 * STATS_MAX (keep the maximum passed value), STATS_MIN, or STATS_DERIVATIVE
 * (keep a time derivative of the value)
 */
counter_t * __stats_CounterCreate( vlc_object_t *p_this,
                                   int i_type, int i_compute_type )
{
    counter_t *p_counter = (counter_t*) malloc( sizeof( counter_t ) ) ;
    (void)p_this;

    if( !p_counter ) return NULL;
    p_counter->i_compute_type = i_compute_type;
    p_counter->i_type = i_type;
    p_counter->i_samples = 0;
    p_counter->pp_samples = NULL;
    p_counter->psz_name = NULL;

    p_counter->update_interval = 0;
    p_counter->last_update = 0;

    return p_counter;
}

/** Update a counter element with new values
 * \param p_this a VLC object
 * \param p_counter the counter to update
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see __stats_Create
 * \param val_new a pointer that will be filled with new data
 */
int __stats_Update( vlc_object_t *p_this, counter_t *p_counter,
                    vlc_value_t val, vlc_value_t *val_new )
{
    if( !libvlc_stats (p_this) || !p_counter ) return VLC_EGENERIC;
    return CounterUpdate( p_this, p_counter, val, val_new );
}

/** Get the aggregated value for a counter
 * \param p_this an object
 * \param p_counter the counter
 * \param val a pointer to an initialized vlc_value union. It will contain the
 * retrieved value
 * \return an error code
 */
int __stats_Get( vlc_object_t *p_this, counter_t *p_counter, vlc_value_t *val )
{
    if( !libvlc_stats (p_this) || !p_counter || p_counter->i_samples == 0 )
    {
        val->i_int = val->f_float = 0.0;
        return VLC_EGENERIC;
    }

    switch( p_counter->i_compute_type )
    {
    case STATS_LAST:
    case STATS_MIN:
    case STATS_MAX:
    case STATS_COUNTER:
        *val = p_counter->pp_samples[0]->value;
        break;
    case STATS_DERIVATIVE:
        /* Not ready yet */
        if( p_counter->i_samples < 2 )
        {
            val->i_int = 0; val->f_float = 0.0;
            return VLC_EGENERIC;
        }
        if( p_counter->i_type == VLC_VAR_INTEGER )
        {
            float f = ( p_counter->pp_samples[0]->value.i_int -
                        p_counter->pp_samples[1]->value.i_int ) /
                    (float)(  p_counter->pp_samples[0]->date -
                              p_counter->pp_samples[1]->date );
            val->i_int = (int)f;
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
    input_stats_t *p_stats = malloc( sizeof(input_stats_t) );

    if( !p_stats )
        return NULL;

    memset( p_stats, 0, sizeof(*p_stats) );
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
    stats_GetInteger( p_input, p_input->p->counters.p_read_packets,
                      &p_stats->i_read_packets );
    stats_GetInteger( p_input, p_input->p->counters.p_read_bytes,
                      &p_stats->i_read_bytes );
    stats_GetFloat( p_input, p_input->p->counters.p_input_bitrate,
                    &p_stats->f_input_bitrate );
    stats_GetInteger( p_input, p_input->p->counters.p_demux_read,
                      &p_stats->i_demux_read_bytes );
    stats_GetFloat( p_input, p_input->p->counters.p_demux_bitrate,
                    &p_stats->f_demux_bitrate );

    /* Decoders */
    stats_GetInteger( p_input, p_input->p->counters.p_decoded_video,
                      &p_stats->i_decoded_video );
    stats_GetInteger( p_input, p_input->p->counters.p_decoded_audio,
                      &p_stats->i_decoded_audio );

    /* Sout */
    if( p_input->p->counters.p_sout_send_bitrate )
    {
        stats_GetInteger( p_input, p_input->p->counters.p_sout_sent_packets,
                          &p_stats->i_sent_packets );
        stats_GetInteger( p_input, p_input->p->counters.p_sout_sent_bytes,
                          &p_stats->i_sent_bytes );
        stats_GetFloat  ( p_input, p_input->p->counters.p_sout_send_bitrate,
                          &p_stats->f_send_bitrate );
    }

    /* Aout */
    stats_GetInteger( p_input, p_input->p->counters.p_played_abuffers,
                      &p_stats->i_played_abuffers );
    stats_GetInteger( p_input, p_input->p->counters.p_lost_abuffers,
                      &p_stats->i_lost_abuffers );

    /* Vouts */
    stats_GetInteger( p_input, p_input->p->counters.p_displayed_pictures,
                      &p_stats->i_displayed_pictures );
    stats_GetInteger( p_input, p_input->p->counters.p_lost_pictures,
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
    p_stats->i_displayed_pictures = p_stats->i_lost_pictures =
    p_stats->i_played_abuffers = p_stats->i_lost_abuffers =
    p_stats->i_decoded_video = p_stats->i_decoded_audio =
    p_stats->i_sent_bytes = p_stats->i_sent_packets = p_stats->f_send_bitrate
     = 0;
    vlc_mutex_unlock( &p_stats->lock );
}

void stats_DumpInputStats( input_stats_t *p_stats  )
{
    vlc_mutex_lock( &p_stats->lock );
    /* f_bitrate is in bytes / microsecond
     * *1000 => bytes / millisecond => kbytes / seconds */
    fprintf( stderr, "Input : %i (%i bytes) - %f kB/s - "
                     "Demux : %i (%i bytes) - %f kB/s\n"
                     " - Vout : %i/%i - Aout : %i/%i - Sout : %f\n",
                    p_stats->i_read_packets, p_stats->i_read_bytes,
                    p_stats->f_input_bitrate * 1000,
                    p_stats->i_demux_read_packets, p_stats->i_demux_read_bytes,
                    p_stats->f_demux_bitrate * 1000,
                    p_stats->i_displayed_pictures, p_stats->i_lost_pictures,
                    p_stats->i_played_abuffers, p_stats->i_lost_abuffers,
                    p_stats->f_send_bitrate );
    vlc_mutex_unlock( &p_stats->lock );
}

void __stats_ComputeGlobalStats( vlc_object_t *p_obj, global_stats_t *p_stats )
{
    vlc_list_t *p_list;
    int i_index;

    if( !libvlc_stats (p_obj) ) return;

    vlc_mutex_lock( &p_stats->lock );

    p_list = vlc_list_find( p_obj, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_list )
    {
        float f_total_in = 0, f_total_out = 0,f_total_demux = 0;
        for( i_index = 0; i_index < p_list->i_count ; i_index ++ )
        {
            float f_in = 0, f_out = 0, f_demux = 0;
            input_thread_t *p_input = (input_thread_t *)
                             p_list->p_values[i_index].p_object;
            vlc_mutex_lock( &p_input->p->counters.counters_lock );
            stats_GetFloat( p_obj, p_input->p->counters.p_input_bitrate, &f_in );
            if( p_input->p->counters.p_sout_send_bitrate )
                stats_GetFloat( p_obj, p_input->p->counters.p_sout_send_bitrate,
                                    &f_out );
            stats_GetFloat( p_obj, p_input->p->counters.p_demux_bitrate,
                                &f_demux );
            vlc_mutex_unlock( &p_input->p->counters.counters_lock );
            f_total_in += f_in; f_total_out += f_out;f_total_demux += f_demux;
        }
        p_stats->f_input_bitrate = f_total_in;
        p_stats->f_output_bitrate = f_total_out;
        p_stats->f_demux_bitrate = f_total_demux;
        vlc_list_release( p_list );
    }

    vlc_mutex_unlock( &p_stats->lock );
}

void __stats_TimerStart( vlc_object_t *p_obj, const char *psz_name,
                         unsigned int i_id )
{
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);
    counter_t *p_counter = NULL;

    if( !priv->b_stats ) return;

    vlc_mutex_lock( &priv->timer_lock );

    for( int i = 0 ; i < priv->i_timers; i++ )
    {
        if( priv->pp_timers[i]->i_id == i_id
            && priv->pp_timers[i]->p_obj == p_obj )
        {
            p_counter = priv->pp_timers[i];
            break;
        }
    }
    if( !p_counter )
    {
        counter_sample_t *p_sample;
        p_counter = stats_CounterCreate( p_obj->p_libvlc, VLC_VAR_TIME,
                                         STATS_TIMER );
        if( !p_counter )
            goto out;
        p_counter->psz_name = strdup( psz_name );
        p_counter->i_id = i_id;
        p_counter->p_obj = p_obj;
        INSERT_ELEM( priv->pp_timers, priv->i_timers,
                     priv->i_timers, p_counter );

        /* 1st sample : if started: start_date, else last_time, b_started */
        p_sample = (counter_sample_t *)malloc( sizeof( counter_sample_t ) );
        INSERT_ELEM( p_counter->pp_samples, p_counter->i_samples,
                     p_counter->i_samples, p_sample );
        p_sample->date = 0; p_sample->value.b_bool = 0;
        /* 2nd sample : global_time, i_samples */
        p_sample = (counter_sample_t *)malloc( sizeof( counter_sample_t ) );
        INSERT_ELEM( p_counter->pp_samples, p_counter->i_samples,
                     p_counter->i_samples, p_sample );
        p_sample->date = 0; p_sample->value.i_int = 0;
    }
    if( p_counter->pp_samples[0]->value.b_bool == true )
    {
        msg_Warn( p_obj, "timer '%s' was already started !", psz_name );
        goto out;
    }
    p_counter->pp_samples[0]->value.b_bool = true;
    p_counter->pp_samples[0]->date = mdate();
out:
    vlc_mutex_unlock( &priv->timer_lock );
}

void __stats_TimerStop( vlc_object_t *p_obj, unsigned int i_id )
{
    counter_t *p_counter = NULL;
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    if( !priv->b_stats ) return;
    vlc_mutex_lock( &priv->timer_lock );
    for( int i = 0 ; i < priv->i_timers; i++ )
    {
        if( priv->pp_timers[i]->i_id == i_id
            && priv->pp_timers[i]->p_obj == p_obj )
        {
            p_counter = priv->pp_timers[i];
            break;
        }
    }
    if( !p_counter || p_counter->i_samples != 2 )
    {
        msg_Err( p_obj, "timer does not exist" );
        goto out;
    }
    p_counter->pp_samples[0]->value.b_bool = false;
    p_counter->pp_samples[1]->value.i_int += 1;
    p_counter->pp_samples[0]->date = mdate() - p_counter->pp_samples[0]->date;
    p_counter->pp_samples[1]->date += p_counter->pp_samples[0]->date;
out:
    vlc_mutex_unlock( &priv->timer_lock );
}

void __stats_TimerDump( vlc_object_t *p_obj, unsigned int i_id )
{
    counter_t *p_counter = NULL;
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    if( !priv->b_stats ) return;
    vlc_mutex_lock( &priv->timer_lock );
    for( int i = 0 ; i < priv->i_timers; i++ )
    {
        if( priv->pp_timers[i]->i_id == i_id
            && priv->pp_timers[i]->p_obj == p_obj )
        {
            p_counter = priv->pp_timers[i];
            break;
        }
    }
    TimerDump( p_obj, p_counter, true );
    vlc_mutex_unlock( &priv->timer_lock );
}

void __stats_TimersDumpAll( vlc_object_t *p_obj )
{
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    if( !priv->b_stats ) return;
    vlc_mutex_lock( &priv->timer_lock );
    for ( int i = 0 ; i < priv->i_timers ; i++ )
        TimerDump( p_obj, priv->pp_timers[i], false );
    vlc_mutex_unlock( &priv->timer_lock );
}

void __stats_TimerClean( vlc_object_t *p_obj, unsigned int i_id )
{
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    vlc_mutex_lock( &priv->timer_lock );
    for ( int i = priv->i_timers -1 ; i >= 0; i-- )
    {
        counter_t *p_counter = priv->pp_timers[i];
        if( p_counter->i_id == i_id && p_counter->p_obj == p_obj )
        {
            REMOVE_ELEM( priv->pp_timers, priv->i_timers, i );
            stats_CounterClean( p_counter );
        }
    }
    vlc_mutex_unlock( &priv->timer_lock );
}

void __stats_TimersCleanAll( vlc_object_t *p_obj )
{
    libvlc_priv_t *priv = libvlc_priv (p_obj->p_libvlc);

    vlc_mutex_lock( &priv->timer_lock );
    for ( int i = priv->i_timers -1 ; i >= 0; i-- )
    {
        counter_t *p_counter = priv->pp_timers[i];
        REMOVE_ELEM( priv->pp_timers, priv->i_timers, i );
        stats_CounterClean( p_counter );
    }
    vlc_mutex_unlock( &priv->timer_lock );
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
        free( p_c->psz_name );
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
static int CounterUpdate( vlc_object_t *p_handler,
                          counter_t *p_counter,
                          vlc_value_t val, vlc_value_t *new_val )
{
    switch( p_counter->i_compute_type )
    {
    case STATS_LAST:
    case STATS_MIN:
    case STATS_MAX:
        if( p_counter->i_samples > 1)
        {
            msg_Err( p_handler, "LAST counter has several samples !" );
            return VLC_EGENERIC;
        }
        if( p_counter->i_type != VLC_VAR_FLOAT &&
            p_counter->i_type != VLC_VAR_INTEGER &&
            p_counter->i_compute_type != STATS_LAST )
        {
            msg_Err( p_handler, "unable to compute MIN or MAX for this type");
            return VLC_EGENERIC;
        }

        if( p_counter->i_samples == 0 )
        {
            counter_sample_t *p_new = (counter_sample_t*)malloc(
                                               sizeof( counter_sample_t ) );
            p_new->value.psz_string = NULL;

            INSERT_ELEM( p_counter->pp_samples, p_counter->i_samples,
                         p_counter->i_samples, p_new );
        }
        if( p_counter->i_samples == 1 )
        {
            /* Update if : LAST or (MAX and bigger) or (MIN and bigger) */
            if( p_counter->i_compute_type == STATS_LAST ||
                ( p_counter->i_compute_type == STATS_MAX &&
                   ( ( p_counter->i_type == VLC_VAR_INTEGER &&
                       p_counter->pp_samples[0]->value.i_int > val.i_int ) ||
                     ( p_counter->i_type == VLC_VAR_FLOAT &&
                       p_counter->pp_samples[0]->value.f_float > val.f_float )
                   ) ) ||
                ( p_counter->i_compute_type == STATS_MIN &&
                   ( ( p_counter->i_type == VLC_VAR_INTEGER &&
                       p_counter->pp_samples[0]->value.i_int < val.i_int ) ||
                     ( p_counter->i_type == VLC_VAR_FLOAT &&
                       p_counter->pp_samples[0]->value.f_float < val.f_float )
                   ) ) )
            {
                if( p_counter->i_type == VLC_VAR_STRING &&
                    p_counter->pp_samples[0]->value.psz_string )
                {
                    free( p_counter->pp_samples[0]->value.psz_string );
                }
                p_counter->pp_samples[0]->value = val;
                *new_val = p_counter->pp_samples[0]->value;
            }
        }
        break;
    case STATS_DERIVATIVE:
    {
        counter_sample_t *p_new, *p_old;
        mtime_t now = mdate();
        if( now - p_counter->last_update < p_counter->update_interval )
        {
            return VLC_EGENERIC;
        }
        p_counter->last_update = now;
        if( p_counter->i_type != VLC_VAR_FLOAT &&
            p_counter->i_type != VLC_VAR_INTEGER )
        {
            msg_Err( p_handler, "Unable to compute DERIVATIVE for this type");
            return VLC_EGENERIC;
        }
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
        if( p_counter->i_samples > 1)
        {
            msg_Err( p_handler, "LAST counter has several samples !" );
            return VLC_EGENERIC;
        }
        if( p_counter->i_samples == 0 )
        {
            counter_sample_t *p_new = (counter_sample_t*)malloc(
                                               sizeof( counter_sample_t ) );
            p_new->value.psz_string = NULL;

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
            default:
                msg_Err( p_handler, "Trying to increment invalid variable %s",
                         p_counter->psz_name );
                return VLC_EGENERIC;
            }
        }
        break;
    }
    return VLC_SUCCESS;
}

static void TimerDump( vlc_object_t *p_obj, counter_t *p_counter,
                       bool b_total )
{
    if( !p_counter )
        return;

    mtime_t last, total;
    int i_total;
    if( p_counter->i_samples != 2 )
    {
        msg_Err( p_obj, "timer %s does not exist", p_counter->psz_name );
        return;
    }
    i_total = p_counter->pp_samples[1]->value.i_int;
    total = p_counter->pp_samples[1]->date;
    if( p_counter->pp_samples[0]->value.b_bool == true )
    {
        last = mdate() - p_counter->pp_samples[0]->date;
        i_total += 1;
        total += last;
    }
    else
    {
        last = p_counter->pp_samples[0]->date;
    }
    if( b_total )
    {
        msg_Dbg( p_obj,
             "TIMER %s : %.3f ms - Total %.3f ms / %i intvls (Avg %.3f ms)",
             p_counter->psz_name, (float)last/1000, (float)total/1000, i_total,
             (float)(total)/(1000*(float)i_total ) );
    }
    else
    {
        msg_Dbg( p_obj,
             "TIMER %s : Total %.3f ms / %i intvls (Avg %.3f ms)",
             p_counter->psz_name, (float)total/1000, i_total,
             (float)(total)/(1000*(float)i_total ) );
    }
}
