/*****************************************************************************
 * control.cpp: functions to handle stream control buttons.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#include <vcl.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "win32_common.h"

extern  intf_thread_t *p_intfGlobal;

/****************************************************************************
 * Control functions: this is where the functions are defined
 ****************************************************************************
 * These functions are used by toolbuttons callbacks
 ****************************************************************************/
bool ControlBack( TObject *Sender )
{
    /* FIXME: TODO */
    
    return false;
}


bool ControlStop( TObject *Sender )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intfGlobal, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return false;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );

    return true;
}


bool ControlPlay( TObject *Sender )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intfGlobal, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        p_intfGlobal->p_sys->p_window->MenuOpenFileClick( Sender );
        return false;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        p_intfGlobal->p_sys->p_window->MenuOpenFileClick( Sender );
    }

    return true;
}


bool ControlPause( TObject *Sender )
{
    if( p_intfGlobal->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_PAUSE );
    }

    return true;
}


bool ControlSlow( TObject *Sender )
{
    if( p_intfGlobal->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_SLOWER );
    }

    return true;
}


bool ControlFast( TObject *Sender )
{
    if( p_intfGlobal->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_FASTER );
    }

    return true;
}

