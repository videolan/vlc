/*****************************************************************************
 * input_ext-intf.c: services to the interface
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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
#include "input_ext-dec.h"
#include "input_ext-intf.h"

#include "input.h"

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
    }

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_SetRate: change the reading rate
 *****************************************************************************/
void input_SetRate( input_thread_t * p_input, int i_mode )
{
    ; /* FIXME: stub */
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

    intf_Msg( "input: seeking position %lld/%lld (%s/%s)", i_position,
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
        i_seconds = i_offset * 50 / p_input->stream.i_mux_rate;
        snprintf( psz_buffer, OFFSETTOTIME_MAX_SIZE, "%d:%02d:%02d",
                 (int) (i_seconds / (60 * 60)),
                 (int) (i_seconds / 60 % 60),
                 (int) (i_seconds % 60) );
        return( psz_buffer );
    }
    else
    {
        /* Divide by zero is not my friend. */
        sprintf( psz_buffer, "NA" );
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
    intf_Msg( "input info: Dumping stream ID 0x%x", S.i_stream_id );
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
            intf_Msg( "input info: ES 0x%x, stream 0x%x, type 0x%x, %s",
                      ES->i_id, ES->i_stream_id, ES->i_type,
                      ES->p_decoder_fifo != NULL ? "selected" : "not selected");
#undef ES
        }
    }
}

