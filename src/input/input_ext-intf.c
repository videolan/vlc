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
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.pp_areas[0]->i_seek = i_position;

    intf_Msg( "input: seeking position %lld/%lld", i_position,
                                          p_input->stream.pp_areas[0]->i_size );

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

