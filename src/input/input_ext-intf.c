/*****************************************************************************
 * input_ext-intf.c: services to the interface
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
 * $Id$
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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
        return secstotimestr( psz_buffer, i_seconds );
    }
    else
    {
        /* Divide by zero is not my friend. */
        sprintf( psz_buffer, "-:--:--" );
        return( psz_buffer );
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
    vlc_value_t val;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_program = input_FindProgram( p_input, i_program_number );

    if ( p_program == NULL )
    {
        msg_Err( p_input, "could not find selected program" );
        return -1;
    }

    p_input->stream.p_new_program = p_program;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update the navigation variables without triggering a callback */
    val.i_int = i_program_number;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    return 0;
}
