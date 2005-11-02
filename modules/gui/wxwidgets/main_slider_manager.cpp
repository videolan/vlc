/*****************************************************************************
 * slider_manager.cpp : Manage an input slider
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: timer.cpp 11981 2005-08-03 15:03:23Z xtophe $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "main_slider_manager.hpp"
#include "interface.hpp"

#include <vlc_meta.h>

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
MainSliderManager::MainSliderManager( intf_thread_t *_p_intf,
                                      Interface *_p_main_intf ) :
        SliderManager( _p_intf )
{
    p_main_intf = _p_main_intf;
    _slider = p_main_intf->slider;
}

MainSliderManager::~MainSliderManager()
{
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_intf->p_sys->p_input ) vlc_object_release( p_intf->p_sys->p_input );
    p_intf->p_sys->p_input = NULL;
    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

void MainSliderManager::UpdateInput()
{
    playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                   FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        LockPlaylist( p_intf->p_sys, p_playlist );
        p_input = p_intf->p_sys->p_input = p_playlist->p_input;
        if( p_intf->p_sys->p_input )
             vlc_object_yield( p_intf->p_sys->p_input );
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        vlc_object_release( p_playlist );
    }
}

void MainSliderManager::UpdateNowPlaying()
{
    char *psz_now_playing = vlc_input_item_GetInfo(
    p_intf->p_sys->p_input->input.p_item,
                _("Meta-information"), _(VLC_META_NOW_PLAYING) );
    if( psz_now_playing && *psz_now_playing )
    {
        p_main_intf->statusbar->SetStatusText(
                    wxString(wxU(psz_now_playing)) + wxT( " - " ) +
                    wxU(p_intf->p_sys->p_input->input.p_item->psz_name), 2 );
    }
    else
    {
        p_main_intf->statusbar->SetStatusText(
                   wxU(p_intf->p_sys->p_input->input.p_item->psz_name), 2 );
    }
    free( psz_now_playing );
}

void MainSliderManager::UpdateButtons( vlc_bool_t b_play )
{
    if( b_play )
    {
        p_main_intf->TogglePlayButton( PLAYING_S );
#ifdef wxHAS_TASK_BAR_ICON
        if( p_main_intf->p_systray )
        {
            p_main_intf->p_systray->UpdateTooltip(
                  wxU( p_intf->p_sys->p_input->input.p_item->psz_name ) +
                  wxString(wxT(" - ")) + wxU(_("Playing")));
        }
#endif
    }
    else
    {
        p_main_intf->TogglePlayButton( PAUSE_S );
        p_main_intf->statusbar->SetStatusText( wxT(""), 0 );
        p_main_intf->statusbar->SetStatusText( wxT(""), 2 );
#ifdef wxHAS_TASK_BAR_ICON
        if( p_main_intf->p_systray )
        {
            p_main_intf->p_systray->UpdateTooltip( wxString(wxT("VLC media player - ")) + wxU(_("Stopped")) );
        }
#endif
    }
}

void MainSliderManager::HideControls()
{
    p_main_intf->m_controls_timer.Start(200, wxTIMER_ONE_SHOT);
}

void MainSliderManager::DontHide()
{
    p_main_intf->m_controls_timer.Stop();

    /* New input or stream map change */
    p_intf->p_sys->b_playing = VLC_TRUE;
}

void MainSliderManager::UpdateDiscButtons()
{
    vlc_value_t val;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 && !p_main_intf->disc_frame->IsShown() )
    {
        vlc_value_t val;

        #define HELP_MENU N_("Menu")
        #define HELP_PCH N_("Previous chapter")
        #define HELP_NCH N_("Next chapter")
        #define HELP_PTR N_("Previous track")
        #define HELP_NTR N_("Next track")

        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );

        if( val.i_int > 0 )
        {
            p_main_intf->disc_menu_button->Show();
            p_main_intf->disc_sizer->Show(
                        p_main_intf->disc_menu_button );
            p_main_intf->disc_sizer->Layout();
            p_main_intf->disc_sizer->Fit(
            p_main_intf->disc_frame );
            p_main_intf->disc_menu_button->SetToolTip(
                        wxU(_( HELP_MENU ) ) );
            p_main_intf->disc_prev_button->SetToolTip(
                        wxU(_( HELP_PCH ) ) );
            p_main_intf->disc_next_button->SetToolTip(
                        wxU(_( HELP_NCH ) ) );
        }
        else
        {
            p_main_intf->disc_menu_button->Hide();
            p_main_intf->disc_sizer->Hide(
                        p_main_intf->disc_menu_button );
            p_main_intf->disc_prev_button->SetToolTip(
                        wxU(_( HELP_PTR ) ) );
            p_main_intf->disc_next_button->SetToolTip(
                        wxU(_( HELP_NTR ) ) );
        }

        p_main_intf->ShowDiscFrame();
    }
    else if( val.i_int == 0 && p_main_intf->disc_frame->IsShown() )
    {
        p_main_intf->HideDiscFrame();
    }
}

vlc_bool_t MainSliderManager::IsShown()
{
    return p_main_intf->slider_frame->IsShown();
}

void MainSliderManager::ShowSlider()
{
    p_main_intf->ShowSlider();
}

void MainSliderManager::HideSlider()
{
    p_main_intf->m_slider_timer.Start( 200, wxTIMER_ONE_SHOT );
}

vlc_bool_t MainSliderManager::IsFree()
{
    return p_intf->p_sys->b_slider_free;
}

vlc_bool_t MainSliderManager::IsPlaying()
{
    return p_intf->p_sys->b_playing;
}

void MainSliderManager::UpdateTime( char *psz_time, char *psz_total )
{
    p_main_intf->statusbar->SetStatusText(
                 wxU(psz_time) + wxString(wxT(" / ")) +wxU(psz_total), 0 );
}
