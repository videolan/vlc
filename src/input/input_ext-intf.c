/*****************************************************************************
 * input_ext-intf.c: services to the interface
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_ext-intf.c,v 1.31 2001/12/09 17:01:37 sam Exp $
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

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>                                              /* off_t */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * input_SetStatus: change the reading status
 *****************************************************************************/
void input_SetStatus( input_thread_t * p_input, int i_mode )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );

    switch( i_mode )
    {
    case INPUT_STATUS_END:
        p_input->stream.i_new_status = PLAYING_S;
        p_input->b_eof = 1;
        intf_Msg( "input: end of stream" );
        break;

    case INPUT_STATUS_PLAY:
        p_input->stream.i_new_status = PLAYING_S;
        intf_Msg( "input: playing at normal rate" );
        break;

    case INPUT_STATUS_PAUSE:
        /* XXX: we don't need to check i_status, because input_clock.c
         * does it for us */
        p_input->stream.i_new_status = PAUSE_S;
        intf_Msg( "input: toggling pause" );
        break;

    case INPUT_STATUS_FASTER:
        /* If we are already going too fast, go back to default rate */
        if( p_input->stream.control.i_rate * 8 <= DEFAULT_RATE )
        {
            p_input->stream.i_new_status = PLAYING_S;
            intf_Msg( "input: playing at normal rate" );
        }
        else
        {
            p_input->stream.i_new_status = FORWARD_S;

            if( p_input->stream.control.i_rate < DEFAULT_RATE
                    && p_input->stream.control.i_status == FORWARD_S )
            {
                p_input->stream.i_new_rate =
                                    p_input->stream.control.i_rate / 2;
            }
            else
            {
                p_input->stream.i_new_rate = DEFAULT_RATE / 2;
            }
            intf_Msg( "input: playing at %i:1 fast forward",
                      DEFAULT_RATE / p_input->stream.i_new_rate );
        }
        break;

    case INPUT_STATUS_SLOWER:
        /* If we are already going too slow, go back to default rate */
        if( p_input->stream.control.i_rate >= 8 * DEFAULT_RATE )
        {
            p_input->stream.i_new_status = PLAYING_S;
            intf_Msg( "input: playing at normal rate" );
        }
        else
        {
            p_input->stream.i_new_status = FORWARD_S;

            if( p_input->stream.control.i_rate > DEFAULT_RATE
                    && p_input->stream.control.i_status == FORWARD_S )
            {
                p_input->stream.i_new_rate =
                                    p_input->stream.control.i_rate * 2;
            }
            else
            {
                p_input->stream.i_new_rate = DEFAULT_RATE * 2;
            }
            intf_Msg( "input: playing at 1:%i slow motion",
                      p_input->stream.i_new_rate / DEFAULT_RATE );
        }
        break;

    default:
        break;
    }

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_Seek: changes the stream postion
 *****************************************************************************/
void input_Seek( input_thread_t * p_input, off_t i_position )
{
    char        psz_time1[OFFSETTOTIME_MAX_SIZE];
    char        psz_time2[OFFSETTOTIME_MAX_SIZE];

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_seek = i_position;

    intf_WarnMsg( 3, "input: seeking position %lld/%lld (%s/%s)", i_position,
                  p_input->stream.p_selected_area->i_size,
                  input_OffsetToTime( p_input, psz_time1, i_position ),
                  input_OffsetToTime( p_input, psz_time2,
                              p_input->stream.p_selected_area->i_size ) );

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_OffsetToTime : converts an off_t value to a time indicator, using
 *                      mux_rate
 *****************************************************************************
 * BEWARE : this function assumes that you already own the lock on
 * p_input->stream.stream_lock
 *****************************************************************************/
char * input_OffsetToTime( input_thread_t * p_input, char * psz_buffer,
                           off_t i_offset )
{
    mtime_t         i_seconds;

    if( p_input->stream.i_mux_rate )
    {
        i_seconds = i_offset / 50 / p_input->stream.i_mux_rate;
        snprintf( psz_buffer, OFFSETTOTIME_MAX_SIZE, "%d:%02d:%02d",
                 (int) (i_seconds / (60 * 60)),
                 (int) (i_seconds / 60 % 60),
                 (int) (i_seconds % 60) );
        return( psz_buffer );
    }
    else
    {
        /* Divide by zero is not my friend. */
        sprintf( psz_buffer, "-:--:--" );
        return( psz_buffer );
    }
}

/*****************************************************************************
 * input_DumpStream: dumps the contents of a stream descriptor
 *****************************************************************************
 * BEWARE : this function assumes that you already own the lock on
 * p_input->stream.stream_lock
 *****************************************************************************/
void input_DumpStream( input_thread_t * p_input )
{
    int         i, j;
    char        psz_time1[OFFSETTOTIME_MAX_SIZE];
    char        psz_time2[OFFSETTOTIME_MAX_SIZE];

#define S   p_input->stream
    intf_Msg( "input info: Dumping stream ID 0x%x [OK:%d/D:%d]", S.i_stream_id,
              S.c_packets_read, S.c_packets_trashed );
    if( S.b_seekable )
        intf_Msg( "input info: seekable stream, position: %lld/%lld (%s/%s)",
                  S.p_selected_area->i_tell, S.p_selected_area->i_size,
                  input_OffsetToTime( p_input, psz_time1,
                                      S.p_selected_area->i_tell ),
                  input_OffsetToTime( p_input, psz_time2,
                                      S.p_selected_area->i_size ) );
    else
        intf_Msg( "input info: %s", S.b_pace_control ? "pace controlled" :
                  "pace un-controlled" );
#undef S
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
#define P   p_input->stream.pp_programs[i]
        intf_Msg( "input info: Dumping program 0x%x, version %d (%s)",
                  P->i_number, P->i_version,
                  P->b_is_ok ? "complete" : "partial" );
#undef P
        for( j = 0; j < p_input->stream.pp_programs[i]->i_es_number; j++ )
        {
#define ES  p_input->stream.pp_programs[i]->pp_es[j]
            intf_Msg( "input info: ES 0x%x, stream 0x%x, type 0x%x, %s [OK:%d/ERR:%d]",
                      ES->i_id, ES->i_stream_id, ES->i_type,
                      ES->p_decoder_fifo != NULL ? "selected" : "not selected",
                      ES->c_packets, ES->c_invalid_packets );
#undef ES
        }
    }
}

/*****************************************************************************
 * input_ChangeES: answers to a user request with calls to (Un)SelectES
 *****************************************************************************
 * Useful since the interface plugins know p_es
 * This functon is deprecated, use input_ToggleEs instead.
 *****************************************************************************/
int input_ChangeES( input_thread_t * p_input, es_descriptor_t * p_es,
                    u8 i_cat )
{
    int                     i_index;
    int                     i;

    i_index = -1;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    for( i = 0 ; i < p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_input->stream.pp_selected_es[i]->i_cat == i_cat )
        {
            i_index = i;
            break;
        }
    }


    if( p_es != NULL )
    {

    
        if( i_index != -1 )
        {
            
            if( p_input->stream.pp_selected_es[i_index] != p_es )
            {
                input_UnselectES( p_input,
                                  p_input->stream.pp_selected_es[i_index] );
                input_SelectES( p_input, p_es );
                intf_WarnMsg( 3, "input info: es selected -> %s (0x%x)",
                                                p_es->psz_desc, p_es->i_id );
            }
        }
        else
        {
            input_SelectES( p_input, p_es );
            intf_WarnMsg( 3, "input info: es selected -> %s (0x%x)",
                          p_es->psz_desc, p_es->i_id );
        }
    }
    else
    {
        if( i_index != -1 )
        {
            intf_WarnMsg( 3, "input info: es unselected -> %s (0x%x)",
                          p_input->stream.pp_selected_es[i_index]->psz_desc,
                          p_input->stream.pp_selected_es[i_index]->i_id );

            input_UnselectES( p_input,
                              p_input->stream.pp_selected_es[i_index] );
        }
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * input_ToggleES: answers to a user request with calls to (Un)SelectES
 *****************************************************************************
 * Useful since the interface plugins know p_es.
 * It only works for audio & spu ( to be sure nothing nasty is being done ).
 * b_select is a boolean to know if we have to select or unselect ES
 *****************************************************************************/
int input_ToggleES( input_thread_t * p_input, es_descriptor_t * p_es,
                    boolean_t b_select )
{

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( p_es != NULL )
    {
        if( b_select )
        {
            p_input->stream.p_newly_selected_es = p_es;
        }
        else
        {
            p_input->stream.p_removed_es = p_es;
        }
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/****************************************************************************
 * input_ChangeArea: interface request an area change
 ****************************************************************************/
int input_ChangeArea( input_thread_t * p_input, input_area_t * p_area )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_new_area = p_area;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/****************************************************************************
 * input_ChangeProgram: interface request an area change
 ****************************************************************************/
int input_ChangeProgram( input_thread_t * p_input, 
            pgrm_descriptor_t * p_program )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_new_program = p_program;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/****************************************************************************
 * input_ToggleGrayscale: change to grayscale or color output
 ****************************************************************************/
int input_ToggleGrayscale( input_thread_t * p_input )
{
    /* No need to warn the input thread since only the decoders and outputs
     * worry about it. */
    vlc_mutex_lock( &p_input->stream.control.control_lock );
    p_input->stream.control.b_grayscale =
                    !p_input->stream.control.b_grayscale;

    intf_WarnMsg( 3, "input warning: changing to %s output",
            p_input->stream.control.b_grayscale ? "grayscale" : "color" );

    vlc_mutex_unlock( &p_input->stream.control.control_lock );

    return 0;
}

/****************************************************************************
 * input_ToggleMute: activate/deactivate mute mode
 ****************************************************************************/
int input_ToggleMute( input_thread_t * p_input )
{
    /* We need to feed the decoders with 0, and only input can do that, so
     * pass the message to the input thread. */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_new_mute = !p_input->stream.control.b_mute;

    intf_WarnMsg( 3, "input warning: %s mute mode",
            p_input->stream.control.b_mute ? "activating" : "deactivating" );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/****************************************************************************
 * input_SetSMP: change the number of video decoder threads
 ****************************************************************************/
int input_SetSMP( input_thread_t * p_input, int i_smp )
{
    /* No need to warn the input thread since only the decoders
     * worry about it. */
    vlc_mutex_lock( &p_input->stream.control.control_lock );
    p_input->stream.control.i_smp = i_smp;
    vlc_mutex_unlock( &p_input->stream.control.control_lock );

    return 0;
}

