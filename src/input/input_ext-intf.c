/*****************************************************************************
 * input_ext-intf.c: services to the interface
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_ext-intf.c,v 1.46 2002/12/25 23:39:01 sam Exp $
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * input_SetStatus: change the reading status
 *****************************************************************************/
void __input_SetStatus( vlc_object_t * p_this, int i_mode )
{
    input_thread_t *p_input;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );

    if( p_input == NULL )
    {
        msg_Err( p_this, "no input found" );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    switch( i_mode )
    {
    case INPUT_STATUS_END:
        p_input->stream.i_new_status = PLAYING_S;
        p_input->b_eof = 1;
        msg_Dbg( p_input, "end of stream" );
        break;

    case INPUT_STATUS_PLAY:
        p_input->stream.i_new_status = PLAYING_S;
        msg_Dbg( p_input, "playing at normal rate" );
        break;

    case INPUT_STATUS_PAUSE:
        /* XXX: we don't need to check i_status, because input_clock.c
         * does it for us */
        p_input->stream.i_new_status = PAUSE_S;
        msg_Dbg( p_input, "toggling pause" );
        break;

    case INPUT_STATUS_FASTER:
        /* If we are already going too fast, go back to default rate */
        if( p_input->stream.control.i_rate * 8 <= DEFAULT_RATE )
        {
            p_input->stream.i_new_status = PLAYING_S;
            msg_Dbg( p_input, "playing at normal rate" );
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
            msg_Dbg( p_input, "playing at %i:1 fast forward",
                     DEFAULT_RATE / p_input->stream.i_new_rate );
        }
        break;

    case INPUT_STATUS_SLOWER:
        /* If we are already going too slow, go back to default rate */
        if( p_input->stream.control.i_rate >= 8 * DEFAULT_RATE )
        {
            p_input->stream.i_new_status = PLAYING_S;
            msg_Dbg( p_input, "playing at normal rate" );
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
            msg_Dbg( p_input, "playing at 1:%i slow motion",
                      p_input->stream.i_new_rate / DEFAULT_RATE );
        }
        break;

    default:
        break;
    }

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    vlc_object_release( p_input );
}

/*****************************************************************************
 * input_Seek: changes the stream postion
 *****************************************************************************/
void __input_Seek( vlc_object_t * p_this, off_t i_position, int i_whence )
{
    input_thread_t *p_input;

    char psz_time1[OFFSETTOTIME_MAX_SIZE];
    char psz_time2[OFFSETTOTIME_MAX_SIZE];

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );

    if( p_input == NULL )
    {
        msg_Err( p_this, "no input found" );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

#define A p_input->stream.p_selected_area
    switch( i_whence & 0x30 )
    {
        case INPUT_SEEK_SECONDS:
            i_position *= (off_t)50 * p_input->stream.i_mux_rate;
            break;

        case INPUT_SEEK_PERCENT:
            i_position = A->i_size * i_position / (off_t)100;
            break;

        case INPUT_SEEK_BYTES:
        default:
            break;
    }

    switch( i_whence & 0x03 )
    {
        case INPUT_SEEK_CUR:
            A->i_seek = A->i_tell + i_position;
            break;

        case INPUT_SEEK_END:
            A->i_seek = A->i_size + i_position;
            break;

        case INPUT_SEEK_SET:
        default:
            A->i_seek = i_position;
            break;
    }

    if( A->i_seek < 0 )
    {
        A->i_seek = 0;
    }
    else if( A->i_seek > A->i_size )
    {
        A->i_seek = A->i_size;
    }

    msg_Dbg( p_input, "seeking position "I64Fd"/"I64Fd" (%s/%s)",
             A->i_seek, A->i_size,
             input_OffsetToTime( p_input, psz_time1, i_position ),
             input_OffsetToTime( p_input, psz_time2, A->i_size ) );
#undef A

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    vlc_object_release( p_input );
}

/*****************************************************************************
 * input_Tell: requests the stream postion
 *****************************************************************************/
void __input_Tell( vlc_object_t * p_this, stream_position_t * p_position )
{
    input_thread_t *p_input;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );

    if( p_input == NULL )
    {
        p_position->i_tell = 0;
        p_position->i_size = 0;
        p_position->i_mux_rate = 0;
        msg_Err( p_this, "no input found" );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

#define A p_input->stream.p_selected_area
    p_position->i_tell = A->i_tell;
    p_position->i_size = A->i_size;
    p_position->i_mux_rate = p_input->stream.i_mux_rate;
#undef A

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    vlc_object_release( p_input );
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
    char psz_time1[OFFSETTOTIME_MAX_SIZE];
    char psz_time2[OFFSETTOTIME_MAX_SIZE];
    unsigned int i, j;

#define S   p_input->stream
    msg_Dbg( p_input, "dumping stream ID 0x%x [OK:%ld/D:%ld]", S.i_stream_id,
             S.c_packets_read, S.c_packets_trashed );
    if( S.b_seekable )
        msg_Dbg( p_input, "seekable stream, position: "I64Fd"/"I64Fd" (%s/%s)",
                 S.p_selected_area->i_tell, S.p_selected_area->i_size,
                 input_OffsetToTime( p_input, psz_time1,
                                     S.p_selected_area->i_tell ),
                 input_OffsetToTime( p_input, psz_time2,
                                     S.p_selected_area->i_size ) );
    else
        msg_Dbg( p_input, "pace %scontrolled", S.b_pace_control ? "" : "un-" );
#undef S
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
#define P   p_input->stream.pp_programs[i]
        msg_Dbg( p_input, "dumping program 0x%x, version %d (%s)",
                 P->i_number, P->i_version,
                 P->b_is_ok ? "complete" : "partial" );
#undef P
        for( j = 0; j < p_input->stream.pp_programs[i]->i_es_number; j++ )
        {
#define ES  p_input->stream.pp_programs[i]->pp_es[j]
            msg_Dbg( p_input, "ES 0x%x, "
                     "stream 0x%x, fourcc `%4.4s', %s [OK:%ld/ERR:%ld]",
                     ES->i_id, ES->i_stream_id, (char*)&ES->i_fourcc,
                     ES->p_decoder_fifo != NULL ? "selected" : "not selected",
                     ES->c_packets, ES->c_invalid_packets );
#undef ES
        }
    }
}

/*****************************************************************************
 * input_ToggleES: answers to a user request with calls to (Un)SelectES
 *****************************************************************************
 * Useful since the interface plugins know p_es.
 * It only works for audio & spu ( to be sure nothing nasty is being done ).
 * b_select is a boolean to know if we have to select or unselect ES
 *****************************************************************************/
int input_ToggleES( input_thread_t * p_input, es_descriptor_t * p_es,
                    vlc_bool_t b_select )
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
 * input_ChangeProgram: interface request a program change
 ****************************************************************************/
int input_ChangeProgram( input_thread_t * p_input, uint16_t i_program_number )
{
    pgrm_descriptor_t *       p_program;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_program = input_FindProgram( p_input, i_program_number );

    if ( p_program == NULL )
    {
        msg_Err( p_input, "could not find selected program" );
        return -1;
    }

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

    msg_Dbg( p_input, "changing to %s output",
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

    msg_Dbg( p_input, "%s mute mode",
             p_input->stream.control.b_mute ? "activating" : "deactivating" );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

