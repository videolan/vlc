/*****************************************************************************
 * stats.c: Statistics handling
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id: messages.c 12729 2005-10-02 08:00:06Z courmisch $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>                                               /* required */

#include <vlc/vlc.h>
#include <vlc_input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static counter_t *GetCounter( stats_handler_t *p_handler, int i_object_id,
                            char *psz_name );
static int stats_CounterUpdate( stats_handler_t *p_handler,
                                counter_t *p_counter,
                                vlc_value_t val );
static stats_handler_t* stats_HandlerCreate( vlc_object_t *p_this );
static stats_handler_t *stats_HandlerGet( vlc_object_t *p_this );

/*****************************************************************************
 * Exported functions
 *****************************************************************************/

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
int __stats_Create( vlc_object_t *p_this, char *psz_name, int i_type,
                    int i_compute_type )
{
    counter_t *p_counter;
    stats_handler_t *p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;

    vlc_mutex_lock( &p_handler->object_lock );

    p_counter = (counter_t*) malloc( sizeof( counter_t ) ) ;

    p_counter->psz_name = strdup( psz_name );
    p_counter->i_source_object = p_this->i_object_id;
    p_counter->i_compute_type = i_compute_type;
    p_counter->i_type = i_type;
    p_counter->i_samples = 0;
    p_counter->pp_samples = NULL;

    p_counter->update_interval = 0;
    p_counter->last_update = 0;

    INSERT_ELEM( p_handler->pp_counters,
                 p_handler->i_counters,
                 p_handler->i_counters,
                 p_counter );

    vlc_mutex_unlock( &p_handler->object_lock );

    return VLC_SUCCESS;
}

/** Update a counter element with new values
 * \param p_this the object in which to update
 * \param psz_name the name
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see __stats_Create
 */
int __stats_Update( vlc_object_t *p_this, char *psz_name, vlc_value_t val )
{
    int i_ret;
    counter_t *p_counter;

    /* Get stats handler singleton */
    stats_handler_t *p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;

    vlc_mutex_lock( &p_handler->object_lock );

    /* Look for existing element */
    p_counter = GetCounter( p_handler, p_this->i_object_id,
                            psz_name );
    if( !p_counter )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
        vlc_object_release( p_handler );
        return VLC_ENOOBJ;
    }

    i_ret = stats_CounterUpdate( p_handler, p_counter, val );
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
int __stats_Get( vlc_object_t *p_this, int i_object_id, char *psz_name, vlc_value_t *val )
{
    counter_t *p_counter;

    /* Get stats handler singleton */
    stats_handler_t *p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return VLC_ENOMEM;
    vlc_mutex_lock( &p_handler->object_lock );


    /* Look for existing element */
    p_counter = GetCounter( p_handler, i_object_id,
                            psz_name );
    if( !p_counter )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
        vlc_object_release( p_handler );
        return VLC_ENOOBJ;
    }

    if( p_counter->i_samples == 0 )
    {
        vlc_mutex_unlock( &p_handler->object_lock );
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
            val->i_int = (int)f;
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
                             char *psz_name )
{
    counter_t *p_counter;

    /* Get stats handler singleton */
    stats_handler_t *p_handler = stats_HandlerGet( p_this );
    if( !p_handler ) return NULL;

    vlc_mutex_lock( &p_handler->object_lock );

    /* Look for existing element */
    p_counter = GetCounter( p_handler, p_this->i_object_id,
                            psz_name );
    vlc_mutex_unlock( &p_handler->object_lock );
    vlc_object_release( p_handler );

    return p_counter;
}


void stats_ComputeInputStats( input_thread_t *p_input,
                              input_stats_t *p_stats )
{
    vlc_mutex_lock( &p_stats->lock );
    /* read_packets and read_bytes are common to all streams */
    stats_GetInteger( p_input, p_input->i_object_id, "read_packets",
                       &p_stats->i_read_packets );
    stats_GetInteger( p_input, p_input->i_object_id, "read_bytes",
                       &p_stats->i_read_bytes );
    stats_GetFloat( p_input, p_input->i_object_id, "input_bitrate",
                       &p_stats->f_bitrate );
    vlc_mutex_unlock( &p_stats->lock );
}

void stats_ReinitInputStats( input_stats_t *p_stats )
{
    p_stats->i_read_packets = p_stats->i_read_bytes =
        p_stats->f_bitrate = p_stats->f_average_bitrate =
        p_stats->i_displayed_pictures = p_stats->i_lost_pictures = 0;
}

void stats_DumpInputStats( input_stats_t *p_stats  )
{
    vlc_mutex_lock( &p_stats->lock );
    /* f_bitrate is in bytes / microsecond
     * *1000 => bytes / millisecond => kbytes / seconds */
    fprintf( stderr, "Read packets : %i (%i bytes) - %f kB/s\n",
                    p_stats->i_read_packets, p_stats->i_read_bytes,
                    p_stats->f_bitrate * 1000 );
    vlc_mutex_unlock( &p_stats->lock );
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
                                vlc_value_t val )
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
            case VLC_VAR_FLOAT:
                p_counter->pp_samples[0]->value.i_int += val.i_int;
                break;
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
                             char *psz_name )
{
    int i;
    for( i = 0; i< p_handler->i_counters; i++ )
    {
        counter_t *p_counter = p_handler->pp_counters[i];
        if( p_counter->i_source_object == i_object_id &&
            !strcmp( p_counter->psz_name, psz_name ) )
        {
            return p_counter;
        }
    }
    return NULL;
}




static stats_handler_t *stats_HandlerGet( vlc_object_t *p_this )
{
    stats_handler_t *p_handler = (stats_handler_t*)
                          vlc_object_find( p_this->p_vlc, VLC_OBJECT_STATS,
                                           FIND_ANYWHERE );
    if( !p_handler )
    {
        p_handler = stats_HandlerCreate( p_this );
        if( !p_handler )
        {
            return NULL;
        }
        vlc_object_yield( p_handler );
    }
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

    return p_handler;
}


