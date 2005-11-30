/*****************************************************************************
 * timer.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include "timer.hpp"
#include "input_manager.hpp"
#include "interface.hpp"
#include "vlc_meta.h"

//void DisplayStreamDate( wxControl *, intf_thread_t *, int );

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Timer::Timer( intf_thread_t *_p_intf, Interface *_p_main_interface )
{
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
    b_init = 0;

    /* Register callback for the intf-popupmenu variable */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_AddCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        var_AddCallback( p_playlist, "intf-show", IntfShowCB, p_intf );
        vlc_object_release( p_playlist );
    }

    Start( 100 /*milliseconds*/, wxTIMER_CONTINUOUS );
}

Timer::~Timer()
{
    /* Unregister callback */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_DelCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        var_DelCallback( p_playlist, "intf-show", IntfShowCB, p_intf );
        vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
void Timer::Notify()
{
#if defined( __WXMSW__ ) /* Work-around a bug with accelerators */
    if( !b_init )
    {
        p_main_interface->Init();
        b_init = VLC_TRUE;
    }
#endif

    vlc_mutex_lock( &p_intf->change_lock );

    /* Call update */
    p_main_interface->input_manager->Update();

    if( p_main_interface->input_manager->IsPlaying() )
    {
        /* Take care of the volume, etc... */
        p_main_interface->Update();
    }

    /* Show the interface, if requested */
    if( p_intf->p_sys->b_intf_show )
    {
        p_main_interface->Raise();
        p_intf->p_sys->b_intf_show = VLC_FALSE;
    }

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        p_main_interface->Close(TRUE);
        return;
    }

    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    if( p_intf->p_sys->pf_show_dialog )
    {
        p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU,
                                       new_val.b_bool, 0 );
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-show playlist variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->b_intf_show = VLC_TRUE;

    return VLC_SUCCESS;
}
