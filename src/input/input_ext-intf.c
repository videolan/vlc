/*****************************************************************************
 * input_ext-intf.c: services to the interface
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors: 
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
 * input_Play: comes back to the normal pace of reading
 *****************************************************************************/
void input_Play( input_thread_t * p_input )
{
    intf_Msg( "input: playing at normal rate" );
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_new_status = PLAYING_S;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_Forward: manages fast forward and slow motion
 *****************************************************************************
 * Note that if i_rate > DEFAULT_RATE, the pace is slower.
 *****************************************************************************/
void input_Forward( input_thread_t * p_input, int i_rate )
{
    if ( i_rate > DEFAULT_RATE )
    {
        intf_Msg( "input: playing at 1:%i slow motion", i_rate / 1000 );
    }
    else if( i_rate < DEFAULT_RATE )
    {
        intf_Msg( "input: playing at %i:1 fast forward", 1000 / i_rate );
    }
    else
    {
        /* Not very joli, but this is going to disappear soon anyway */
        input_Play( p_input );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_new_status = FORWARD_S;
    p_input->stream.i_new_rate = i_rate;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

