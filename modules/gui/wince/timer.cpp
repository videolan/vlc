/*****************************************************************************
 * timer.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

#include "wince.h"

#include <commctrl.h>

/* Callback prototype */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void * );

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Timer::Timer( intf_thread_t *_p_intf, HWND hwnd, Interface *_p_main_interface)
{
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
    i_old_playing_status = PAUSE_S;
    i_old_rate = INPUT_RATE_DEFAULT;

    /* Register callback for the intf-popupmenu variable */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_AddCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        vlc_object_release( p_playlist );
    }

    SetTimer( hwnd, 1, 200 /*milliseconds*/, NULL );
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
void Timer::Notify( void )
{
    vlc_value_t val;
    char *shortname;

    vlc_mutex_lock( &p_intf->change_lock );

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input =
            (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                               FIND_ANYWHERE );

        /* Show slider */
        if( p_intf->p_sys->p_input )
        {
            ShowWindow( p_main_interface->hwndSlider, SW_SHOW );
            ShowWindow( p_main_interface->hwndLabel, SW_SHOW );
            ShowWindow( p_main_interface->hwndVol, SW_SHOW );

            // only for local file, check if works well with net url
            shortname = strrchr( p_intf->p_sys->p_input->input.p_item->psz_name, '\\' );
            if (! shortname)
                shortname = p_intf->p_sys->p_input->input.p_item->psz_name;
            else shortname++;
                        
            SendMessage( p_main_interface->hwndSB, SB_SETTEXT, 
                         (WPARAM) 0, (LPARAM)_FROMMB(shortname) );

            p_main_interface->TogglePlayButton( PLAYING_S );
            i_old_playing_status = PLAYING_S;
        }
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        /* Hide slider */
        ShowWindow( p_main_interface->hwndSlider, SW_HIDE);
        ShowWindow( p_main_interface->hwndLabel, SW_HIDE);
        ShowWindow( p_main_interface->hwndVol, SW_HIDE);

        p_main_interface->TogglePlayButton( PAUSE_S );
        i_old_playing_status = PAUSE_S;

        SendMessage( p_main_interface->hwndSB, SB_SETTEXT, 
                     (WPARAM) 0, (LPARAM)(LPCTSTR) TEXT(""));

        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;

        if( !p_input->b_die )
        {
            /* New input or stream map change */
            p_intf->p_sys->b_playing = 1;

            /* Manage the slider */
            if( /*p_input->stream.b_seekable &&*/ p_intf->p_sys->b_playing )
            {
                /* Update the slider if the user isn't dragging it. */
                if( p_intf->p_sys->b_slider_free )
                {
                    vlc_value_t pos;
                    char psz_time[ MSTRTIME_MAX_SIZE ];
                    vlc_value_t time;
                    mtime_t i_seconds;

                    /* Update the value */
                    var_Get( p_input, "position", &pos );
                    if( pos.f_float >= 0.0 )
                    {
                        p_intf->p_sys->i_slider_pos =
                            (int)(SLIDER_MAX_POS * pos.f_float);

                        SendMessage( p_main_interface->hwndSlider, TBM_SETPOS, 
                                     1, p_intf->p_sys->i_slider_pos );

                        var_Get( p_intf->p_sys->p_input, "time", &time );
                        i_seconds = time.i_time / 1000000;
                        secstotimestr ( psz_time, i_seconds );

                        SendMessage( p_main_interface->hwndLabel, WM_SETTEXT, 
                                     (WPARAM)1, (LPARAM)_FROMMB(psz_time) );
                    }
                }
            }

            /* Take care of the volume, etc... */
            p_main_interface->Update();

            /* Manage Playing status */
            var_Get( p_input, "state", &val );
            if( i_old_playing_status != val.i_int )
            {
                if( val.i_int == PAUSE_S )
                {
                    p_main_interface->TogglePlayButton( PAUSE_S );
                }
                else
                {
                    p_main_interface->TogglePlayButton( PLAYING_S );
                }
                i_old_playing_status = val.i_int;
            }

            /* Manage Speed status */
            var_Get( p_input, "rate", &val );
            if( i_old_rate != val.i_int )
            {
                TCHAR psz_text[15];
                _stprintf( psz_text + 2, _T("x%.2f"), 1000.0 / val.i_int );
                psz_text[0] = psz_text[1] = _T('\t');

                SendMessage( p_main_interface->hwndSB, SB_SETTEXT, 
                             (WPARAM) 1, (LPARAM)(LPCTSTR) psz_text );

                i_old_rate = val.i_int;
            }
        }
    }
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        p_intf->p_sys->b_playing = 0;
        p_main_interface->TogglePlayButton( PAUSE_S );
        i_old_playing_status = PAUSE_S;
    }

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
/*        p_main_interface->Close(TRUE);*/
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
