/*****************************************************************************
 * timer.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: timer.cpp,v 1.23 2003/06/12 21:28:39 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/timer.h>

#include <vlc/intf.h>

#include "wxwindows.h"

void DisplayStreamDate( wxControl *, intf_thread_t *, int );

/* Callback prototype */
int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                 vlc_value_t old_val, vlc_value_t new_val, void *param );

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Timer::Timer( intf_thread_t *_p_intf, Interface *_p_main_interface )
{
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
    i_old_playing_status = PAUSE_S;
    i_old_rate = DEFAULT_RATE;

    /* Register callback for the intf-popupmenu variable */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_AddCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        vlc_object_release( p_playlist );
    }

    Start( 100 /*milliseconds*/, wxTIMER_CONTINUOUS );
}

Timer::~Timer()
{
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
    vlc_bool_t b_pace_control;

    vlc_mutex_lock( &p_intf->change_lock );

    /* If the "display popup" flag has changed */
    if( p_intf->p_sys->b_popup_change )
    {
        wxPoint mousepos = wxGetMousePosition();

        wxMouseEvent event = wxMouseEvent( wxEVT_RIGHT_UP );
        event.m_x = p_main_interface->ScreenToClient(mousepos).x;
        event.m_y = p_main_interface->ScreenToClient(mousepos).y;

        p_main_interface->AddPendingEvent(event);

        p_intf->p_sys->b_popup_change = VLC_FALSE;
    }

    /* Update the log window */
    p_intf->p_sys->p_messages_window->UpdateLog();

    /* Update the playlist */
    p_intf->p_sys->p_playlist_window->UpdatePlaylist();

    /* Update the fileinfo windows */
    p_intf->p_sys->p_fileinfo_window->UpdateFileInfo();

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)vlc_object_find( p_intf,
                                                       VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );

        /* Show slider */
        if( p_intf->p_sys->p_input )
        {
            //if( p_intf->p_sys->p_input->stream.b_seekable )
            {
                p_main_interface->slider_frame->Show();
                p_main_interface->frame_sizer->Show(
                    p_main_interface->slider_frame );
                p_main_interface->frame_sizer->Layout();
                p_main_interface->frame_sizer->Fit( p_main_interface );
            }

            p_main_interface->statusbar->SetStatusText(
                wxU(p_intf->p_sys->p_input->psz_source), 2 );

            p_main_interface->TogglePlayButton( PLAYING_S );
            i_old_playing_status = PLAYING_S;

            /* Take care of the volume */
            audio_volume_t i_volume;
            aout_VolumeGet( p_intf, &i_volume );
            p_main_interface->volctrl->SetValue( i_volume * 200 /
                                                 AOUT_VOLUME_MAX );
        }

        /* control buttons for free pace streams */
        b_pace_control = p_intf->p_sys->p_input->stream.b_pace_control;

    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        /* Hide slider */
        //if( p_intf->p_sys->p_input->stream.b_seekable )
        {
            p_main_interface->slider_frame->Hide();
            p_main_interface->frame_sizer->Hide(
                p_main_interface->slider_frame );
            p_main_interface->frame_sizer->Layout();
            p_main_interface->frame_sizer->Fit( p_main_interface );

            p_main_interface->TogglePlayButton( PAUSE_S );
            i_old_playing_status = PAUSE_S;
        }

        p_main_interface->statusbar->SetStatusText( wxT(""), 2 );

        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }



    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( !p_input->b_die )
        {
            /* New input or stream map change */
            p_intf->p_sys->b_playing = 1;
#if 0
            if( p_input->stream.b_changed )
            {
                wxModeManage( p_intf );
                wxSetupMenus( p_intf );
                p_intf->p_sys->b_playing = 1;

                p_main_interface->TogglePlayButton( PLAYING_S );
                i_old_playing_status = PLAYING_S;
            }
#endif

            /* Manage the slider */
            if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
            {
                stream_position_t position;

                /* Update the slider if the user isn't dragging it. */
                if( p_intf->p_sys->b_slider_free )
                {
                    /* Update the value */
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_Tell( p_input, &position );
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    if( position.i_size )
                    {
                        p_intf->p_sys->i_slider_pos =
                        ( SLIDER_MAX_POS * position.i_tell ) / position.i_size;

                        p_main_interface->slider->SetValue(
                            p_intf->p_sys->i_slider_pos );

                        DisplayStreamDate( p_main_interface->slider_box,p_intf,
                                           p_intf->p_sys->i_slider_pos );
                    }
                }
            }

            /* Manage Playing status */
            if( i_old_playing_status != p_input->stream.control.i_status )
            {
                if( p_input->stream.control.i_status == PAUSE_S )
                {
                    p_main_interface->TogglePlayButton( PAUSE_S );
                }
                else
                {
                    p_main_interface->TogglePlayButton( PLAYING_S );
                }
                i_old_playing_status = p_input->stream.control.i_status;
            }

            /* Manage Speed status */
            if( i_old_rate != p_input->stream.control.i_rate )
            {
                p_main_interface->statusbar->SetStatusText(
                    wxString::Format(wxT("x%.2f"),
                    1000.0 / p_input->stream.control.i_rate), 1 );
                i_old_rate = p_input->stream.control.i_rate;
            }
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
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
        p_main_interface->Close(TRUE);
        return;
    }

    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * DisplayStreamDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void DisplayStreamDate( wxControl *p_slider_frame, intf_thread_t * p_intf ,
                        int i_pos )
{
    if( p_intf->p_sys->p_input )
    {
#define p_area p_intf->p_sys->p_input->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        p_slider_frame->SetLabel(
            wxU(input_OffsetToTime( p_intf->p_sys->p_input,
                    psz_time, p_area->i_size * i_pos / SLIDER_MAX_POS )) );
#undef p_area
     }
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                 vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    p_intf->p_sys->b_popup_change = VLC_TRUE;

    return VLC_SUCCESS;
}
