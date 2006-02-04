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
#include <stdio.h>                                               /* required */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static counter_t *GetCounter( stats_handler_t *p_handler, int i_object_id,
                              unsigned int i_counter );
static int stats_CounterUpdate( stats_handler_t *p_handler,
                                counter_t *p_counter,
                                vlc_value_t val, vlc_value_t * );
static stats_handler_t* stats_HandlerCreate( vlc_object_t *p_this );
static stats_handler_t *stats_HandlerGet( vlc_object_t *p_this );

static void TimerDump( vlc_object_t *p_this, counter_t *p_counter, vlc_bool_t);

/*****************************************************************************
 * Exported functions
 *****************************************************************************/

/**
 * Cleanup statistics handler stuff
 * \param p_stats the handler to clean
 * \return nothing
 */
void stats_HandlerDestroy( stats_handler_t *p_stats )
{
    int i;
    for ( i =  p_stats->i_counters - 1 ; i >= 0 ; i-- )
    {
        int j;
        counter_t *p_counter = p_stats->pp_counters[i];

        for( j = p_counter->i_samples -1; j >= 0 ; j-- )
        {
            counter_sample_t *p_sample = p_counter->pp_samples[j];
            REMOVE_ELEM( p_counter->pp_samples, p_counter->i_samples, j );
            free( p_sample );
        }
        free( p_counter->psz_name );
        REMOVE_ELEM( p_stats->pp_counters, p_stats->i_counters, i );
        free( p_counter );
    }
}

/**
 * Create a statistics counter
 * \param p_this the object for which to create the counter
 * \param psz_name the name
 * \param i_type the type of stored data. One of VLC_VAR_STRING,
 * VLC_VAR_INTEGER, VLC_VAR_FLOAT
 * \param i_compute_type the aggregation type. One of STATS_LAST (always
 * keep the last value), STATS_COUNTER (increment by the passed value),
 * STATS_MAX (keep the maximum passed value), STATS_MIN, or STATS_DERIVATIVE
 * (keep a time derivative of the value)
 */
int __stats_Create( vlc_object_t *p_this, const char *psz_name, unsigned int i_id,
                    int i_type, int i_compute_type )
{
    counter_t *p_counter;
    stats_handler_t *p_handler;

    if( p_this->p_libvlc->b_stats == VLC_FALSE )
    {
        return VLC_EGENERIC;
    }
    p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;

    vlc_mutex_lock( &p_handler->object_lock );

    p_counter = (counter_t*) malloc( sizeof( counter_t ) ) ;

    p_counter->psz_name = strdup( psz_name );
    p_counter->i_index = ((uint64_t)p_this->i_object_id << 32 ) + i_id;
    p_counter->i_compute_type = i_compute_type;
    p_counter->i_type = i_type;
    p_counter->i_samples = 0;
    p_counter->pp_samples = NULL;

    p_counter->update_interval = 0;
    p_counter->last_update = 0;

    INSERT_ELEM( p_handler->pp_counters, p_handler->i_counters,
                 p_handler->i_counters, p_counter );

    vlc_mutex_unlock( &p_handler->object_lock );

    return VLC_SUCCESS;
}

/** Update a counter element with new values
 * \param p_this the object in which to update
 * \param psz_name the name
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see __stats_Create
 */
int __stats_Update( vlc_object_t *p_this, unsigned int i_counter,
                    vlc_value_t val, vlc_value_t *val_new )
{
    int i_ret;
    counter_t *p_counter;

    /* Get stats handler singleton */
    stats_handler_t *p_handler;
    if( p_this->p_libvlc->b_stats == VLC_FALSE )
    {
        return VLC_EGENERIC;
    }
    p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;

    vlc_mutex_lock( &p_handler->object_lock );
    /* Look for existing element */
    p_counter = GetCounter( p_handler, p_this->i_object_id, i_counter );
    if( !p_counter )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
        vlc_object_release( p_handler );
        return VLC_ENOOBJ;
    }

    i_ret = stats_CounterUpdate( p_handler, p_counter, val, val_new );
    vlc_mutex_unlock( &p_handler->object_lock );

    return i_ret;
}

/** Get the aggregated value for a counter
 * \param p_this an object
 * \param i_object_id the object id from which we want the data
 * \param psz_name the name of the couner
 * \param val a pointer to an initialized vlc_value union. It will contain the
 * retrieved value
 * \return an error code
 */
int __stats_Get( vlc_object_t *p_this, int i_object_id,
                 unsigned int i_counter, vlc_value_t *val )
{
    counter_t *p_counter;

    /* Get stats handler singleton */
    stats_handler_t *p_handler;
    if( p_this->p_libvlc->b_stats == VLC_FALSE )
    {
        return VLC_EGENERIC;
    }
    p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;
    vlc_mutex_lock( &p_handler->object_lock );

    /* Look for existing element */
    p_counter = GetCounter( p_handler, i_object_id, i_counter );
    if( !p_counter )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
        vlc_object_release( p_handler );
        val->i_int = val->f_float = 0.0;
        return VLC_ENOOBJ;
    }

    if( p_counter->i_samples == 0 )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
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
            vlc_mutex_unlock( &p_handler->object_lock );
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
    vlc_object_release( p_handler );

    vlc_mutex_unlock( &p_handler->object_lock );
    return VLC_SUCCESS;;
}

/** Get a statistics counter structure. This allows for low-level modifications
 * \param p_this a parent object
 * \param i_object_id the object from which to retrieve data
 * \param psz_name the name
 * \return the counter, or NULL if not found (or handler not created yet)
 */
counter_t *__stats_CounterGet( vlc_object_t *p_this, int i_object_id,
                               unsigned int i_counter )
{
    counter_t *p_counter;

    stats_handler_t *p_handler;
    if( p_this->p_libvlc->b_stats == VLC_FALSE )
    {
        return NULL;
    }
    p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return NULL;

    vlc_mutex_lock( &p_handler->object_lock );

    /* Look for existing element */
    p_counter = GetCounter( p_handler, i_object_id, i_counter );
    vlc_mutex_unlock( &p_handler->object_lock );
    vlc_object_release( p_handler );

    return p_counter;
}


void stats_ComputeInputStats( input_thread_t *p_input,
                              input_stats_t *p_stats )
{
    vlc_object_t *p_obj;
    vlc_list_t *p_list;
    int i_index;
    vlc_mutex_lock( &p_stats->lock );

    /* Input */
    stats_GetInteger( p_input, p_input->i_object_id, STATS_READ_PACKETS,
                       &p_stats->i_read_packets );
    stats_GetInteger( p_input, p_input->i_object_id, STATS_READ_BYTES,
                       &p_stats->i_read_bytes );
    stats_GetFloat( p_input, p_input->i_object_id, STATS_INPUT_BITRATE,
                       &p_stats->f_input_bitrate );

    stats_GetInteger( p_input, p_input->i_object_id, STATS_DEMUX_READ,
                      &p_stats->i_demux_read_bytes );
    stats_GetFloat( p_input, p_input->i_object_id, STATS_DEMUX_BITRATE,
                      &p_stats->f_demux_bitrate );

    stats_GetInteger( p_input, p_input->i_object_id, STATS_DECODED_VIDEO,
                      &p_stats->i_decoded_video );
    stats_GetInteger( p_input, p_input->i_object_id, STATS_DECODED_AUDIO,
                      &p_stats->i_decoded_audio );

    /* Sout */
    stats_GetInteger( p_input, p_input->i_object_id, STATS_SOUT_SENT_PACKETS,
                      &p_stats->i_sent_packets );
    stats_GetInteger( p_input, p_input->i_object_id, STATS_SOUT_SENT_BYTES,
                      &p_stats->i_sent_bytes );
    stats_GetFloat  ( p_input, p_input->i_object_id, STATS_SOUT_SEND_BITRATE,
                      &p_stats->f_send_bitrate );

    /* Aout - We store in p_input because aout is shared */
    stats_GetInteger( p_input, p_input->i_object_id, STATS_PLAYED_ABUFFERS,
                      &p_stats->i_played_abuffers );
    stats_GetInteger( p_input, p_input->i_object_id, STATS_LOST_ABUFFERS,
                      &p_stats->i_lost_abuffers );

    /* Vouts - FIXME: Store all in input */
    p_list = vlc_list_find( p_input, VLC_OBJECT_VOUT, FIND_CHILD );
    if( p_list )
    {
        p_stats->i_displayed_pictures  = 0 ;
        p_stats->i_lost_pictures = 0;
        for( i_index = 0; i_index < p_list->i_count ; i_index ++ )
        {
            int i_displayed = 0, i_lost = 0;
            p_obj = (vlc_object_t *)p_list->p_values[i_index].p_object;
            stats_GetInteger( p_obj, p_obj->i_object_id,
                              STATS_DISPLAYED_PICTURES,
                              &i_displayed );
            stats_GetInteger( p_obj, p_obj->i_object_id, STATS_LOST_PICTURES,
                              &i_lost );
            p_stats->i_displayed_pictures += i_displayed;
            p_stats->i_lost_pictures += i_lost;
         }
        vlc_list_release( p_list );
    }

    vlc_mutex_unlock( &p_stats->lock );
}


void stats_ReinitInputStats( input_stats_t *p_stats )
{
    p_stats->i_read_packets = p_stats->i_read_bytes =
    p_stats->f_input_bitrate = p_stats->f_average_input_bitrate =
    p_stats->i_demux_read_packets = p_stats->i_demux_read_bytes =
    p_stats->f_demux_bitrate = p_stats->f_average_demux_bitrate =
    p_stats->i_displayed_pictures = p_stats->i_lost_pictures =
    p_stats->i_played_abuffers = p_stats->i_lost_abuffers =
    p_stats->i_decoded_video = p_stats->i_decoded_audio =
    p_stats->i_sent_bytes = p_stats->i_sent_packets = p_stats->f_send_bitrate
     = 0;
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

void __stats_ComputeGlobalStats( vlc_object_t *p_obj,
                                global_stats_t *p_stats )
{
    vlc_list_t *p_list;
    int i_index;
    vlc_mutex_lock( &p_stats->lock );

    p_list = vlc_list_find( p_obj, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_list )
    {
        float f_total_in = 0, f_total_out = 0,f_total_demux = 0;
        for( i_index = 0; i_index < p_list->i_count ; i_index ++ )
        {
            float f_in = 0, f_out = 0, f_demux = 0;
            p_obj = (vlc_object_t *)p_list->p_values[i_index].p_object;
            stats_GetFloat( p_obj, p_obj->i_object_id, STATS_INPUT_BITRATE,
                            &f_in );
            stats_GetFloat( p_obj, p_obj->i_object_id, STATS_SOUT_SEND_BITRATE,
                            &f_out );
            stats_GetFloat( p_obj, p_obj->i_object_id, STATS_DEMUX_BITRATE,
                            &f_demux );
            f_total_in += f_in; f_total_out += f_out;f_total_demux += f_demux;
        }
        p_stats->f_input_bitrate = f_total_in;
        p_stats->f_output_bitrate = f_total_out;
        p_stats->f_demux_bitrate = f_total_demux;
        vlc_list_release( p_list );
    }

    vlc_mutex_unlock( &p_stats->lock );
}

void stats_ReinitGlobalStats( global_stats_t *p_stats )
{
    p_stats->f_input_bitrate = p_stats->f_output_bitrate = 0.0;
}


void __stats_TimerStart( vlc_object_t *p_obj, const char *psz_name,
                         unsigned int i_id )
{
    counter_t *p_counter = stats_CounterGet( p_obj,
                                             p_obj->p_vlc->i_object_id, i_id );
    if( !p_counter )
    {
        counter_sample_t *p_sample;
        stats_Create( p_obj->p_vlc, psz_name, i_id, VLC_VAR_TIME, STATS_TIMER );
        p_counter = stats_CounterGet( p_obj,  p_obj->p_vlc->i_object_id,
                                      i_id );
        if( !p_counter ) return;
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
    if( p_counter->pp_samples[0]->value.b_bool == VLC_TRUE )
    {
        msg_Warn( p_obj, "timer %s was already started !", psz_name );
        return;
    }
    p_counter->pp_samples[0]->value.b_bool = VLC_TRUE;
    p_counter->pp_samples[0]->date = mdate();
}

void __stats_TimerStop( vlc_object_t *p_obj, unsigned int i_id )
{
    counter_t *p_counter = stats_CounterGet( p_obj,
                                             p_obj->p_vlc->i_object_id,
                                             i_id );
    if( !p_counter || p_counter->i_samples != 2 )
    {
        msg_Err( p_obj, "timer does not exist" );
        return;
    }
    p_counter->pp_samples[0]->value.b_bool = VLC_FALSE;
    p_counter->pp_samples[1]->value.i_int += 1;
    p_counter->pp_samples[0]->date = mdate() - p_counter->pp_samples[0]->date;
    p_counter->pp_samples[1]->date += p_counter->pp_samples[0]->date;
}

void __stats_TimerDump( vlc_object_t *p_obj, unsigned int i_id )
{
    counter_t *p_counter = stats_CounterGet( p_obj,
                                             p_obj->p_vlc->i_object_id,
                                             i_id );
    TimerDump( p_obj, p_counter, VLC_TRUE );
}


void __stats_TimersDumpAll( vlc_object_t *p_obj )
{
    int i;
    stats_handler_t *p_handler = stats_HandlerGet( p_obj );
    if( !p_handler ) return;

    vlc_mutex_lock( &p_handler->object_lock );
    for ( i = 0 ; i< p_handler->i_counters; i++ )
    {
        counter_t * p_counter = p_handler->pp_counters[i];
        if( p_counter->i_compute_type == STATS_TIMER )
        {
            TimerDump( p_obj, p_counter, VLC_FALSE );
        }
    }
    vlc_mutex_unlock( &p_handler->object_lock );
}


/********************************************************************
 * Following functions are local
 ********************************************************************/

/**
 * Update a statistics counter, according to its type
 * If needed, perform a bit of computation (derivative, mostly)
 * This function must be entered with stats handler lock
 * \param p_handler stats handler singleton
 * \param p_counter the counter to update
 * \param val the "new" value
 * \return an error code
 */
static int stats_CounterUpdate( stats_handler_t *p_handler,
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
            msg_Err( p_handler, "Unable to compute MIN or MAX for this type");
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
        if( mdate() - p_counter->last_update < p_counter->update_interval )
        {
            return VLC_EGENERIC;
        }
        p_counter->last_update = mdate();
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

static counter_t *GetCounter( stats_handler_t *p_handler, int i_object_id,
                              unsigned int i_counter )
{
    int i;
    uint64_t i_index = ((uint64_t) i_object_id << 32 ) + i_counter;
    for (i = 0 ; i < p_handler->i_counters ; i++ )
    {
         if( i_index == p_handler->pp_counters[i]->i_index )
             return p_handler->pp_counters[i];
    }
    return NULL;
}


static stats_handler_t *stats_HandlerGet( vlc_object_t *p_this )
{
    stats_handler_t *p_handler = p_this->p_libvlc->p_stats;
    if( !p_handler )
    {
        p_handler = stats_HandlerCreate( p_this );
        if( !p_handler )
        {
            return NULL;
        }
    }
    vlc_object_yield( p_handler );
    return p_handler;
}

/**
 * Initialize statistics handler
 *
 * This function initializes the global statistics handler singleton,
 * \param p_this the parent VLC object
 */
static stats_handler_t* stats_HandlerCreate( vlc_object_t *p_this )
{
    stats_handler_t *p_handler;

    msg_Dbg( p_this, "creating statistics handler" );

    p_handler = (stats_handler_t*) vlc_object_create( p_this,
                                                      VLC_OBJECT_STATS );

    if( !p_handler )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }
    p_handler->i_counters = 0;
    p_handler->pp_counters = NULL;

    /// \bug is it p_vlc or p_libvlc ?
    vlc_object_attach( p_handler, p_this->p_vlc );

    p_this->p_libvlc->p_stats = p_handler;

    return p_handler;
}

static void TimerDump( vlc_object_t *p_obj, counter_t *p_counter,
                       vlc_bool_t b_total )
{
    mtime_t last, total;
    int i_total;
    if( !p_counter || p_counter->i_samples != 2 )
    {
        msg_Err( p_obj, "timer %s does not exist", p_counter->psz_name );
        return;
    }
    i_total = p_counter->pp_samples[1]->value.i_int;
    total = p_counter->pp_samples[1]->date;
    if( p_counter->pp_samples[0]->value.b_bool == VLC_TRUE )
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
