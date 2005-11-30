/*****************************************************************************
 * slider_manager.cpp : Manage an input slider
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#include "input_manager.hpp"
#include "interface.hpp"
#include "video.hpp"

#include <vlc_meta.h>

/* include the toolbar graphics */
#include "bitmaps/prev.xpm"
#include "bitmaps/next.xpm"
#include "bitmaps/playlist.xpm"

/* IDs for the controls */
enum
{
    SliderScroll_Event = wxID_HIGHEST,

    DiscMenu_Event,
    DiscPrev_Event,
    DiscNext_Event
};

BEGIN_EVENT_TABLE(InputManager, wxPanel)
    /* Slider events */
    EVT_COMMAND_SCROLL(SliderScroll_Event, InputManager::OnSliderUpdate)

    /* Disc Buttons events */
    EVT_BUTTON(DiscMenu_Event, InputManager::OnDiscMenu)
    EVT_BUTTON(DiscPrev_Event, InputManager::OnDiscPrev)
    EVT_BUTTON(DiscNext_Event, InputManager::OnDiscNext)

END_EVENT_TABLE()

#define STATUS_STOP 0
#define STATUS_PLAYING 1
#define STATUS_PAUSE 2

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
InputManager::InputManager( intf_thread_t *_p_intf, Interface *_p_main_intf,
                            wxWindow *p_parent )
  : wxPanel( p_parent )
{
    p_intf = _p_intf;
    p_main_intf = _p_main_intf;
    p_input = NULL;
    i_old_playing_status = STATUS_STOP;
    i_old_rate = INPUT_RATE_DEFAULT;
    b_slider_free = VLC_TRUE;

    /* Create slider */
    slider = new wxSlider( this, SliderScroll_Event, 0, 0, SLIDER_MAX_POS );

    /* Create disc buttons */
    disc_frame = new wxPanel( this );

    disc_menu_button = new wxBitmapButton( disc_frame, DiscMenu_Event,
                                           wxBitmap( playlist_xpm ) );
    disc_prev_button = new wxBitmapButton( disc_frame, DiscPrev_Event,
                                           wxBitmap( prev_xpm ) );
    disc_next_button = new wxBitmapButton( disc_frame, DiscNext_Event,
                                           wxBitmap( next_xpm ) );

    disc_sizer = new wxBoxSizer( wxHORIZONTAL );
    disc_sizer->Add( disc_menu_button, 1, wxEXPAND | wxLEFT | wxRIGHT, 1 );
    disc_sizer->Add( disc_prev_button, 1, wxEXPAND | wxLEFT | wxRIGHT, 1 );
    disc_sizer->Add( disc_next_button, 1, wxEXPAND | wxLEFT | wxRIGHT, 1 );
    disc_frame->SetSizer( disc_sizer );
    disc_sizer->Layout();

    /* Add everything to the panel */
    sizer = new wxBoxSizer( wxHORIZONTAL );
    SetSizer( sizer );
    sizer->Add( slider, 1, wxEXPAND | wxALL, 5 );
    sizer->Add( disc_frame, 0, wxALL, 2 );

    /* Hide by default */
    sizer->Hide( disc_frame );
    sizer->Hide( slider );

    sizer->Layout();
    Fit();
}

InputManager::~InputManager()
{
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_intf->p_sys->p_input ) vlc_object_release( p_intf->p_sys->p_input );
    p_intf->p_sys->p_input = NULL;
    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * Public methods.
 *****************************************************************************/
vlc_bool_t InputManager::IsPlaying()
{
    return (p_input && !p_input->b_die);
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void InputManager::UpdateInput()
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

void InputManager::UpdateNowPlaying()
{
    char *psz_now_playing = vlc_input_item_GetInfo( p_input->input.p_item,
                _("Meta-information"), _(VLC_META_NOW_PLAYING) );
    if( psz_now_playing && *psz_now_playing )
    {
        p_main_intf->statusbar->SetStatusText(
                    wxString(wxU(psz_now_playing)) + wxT( " - " ) +
                    wxU(p_input->input.p_item->psz_name), 2 );
    }
    else
    {
        p_main_intf->statusbar->SetStatusText(
                   wxU(p_input->input.p_item->psz_name), 2 );
    }
    free( psz_now_playing );
}

void InputManager::UpdateButtons( vlc_bool_t b_play )
{
    if( !b_play )
    {
        if( i_old_playing_status == STATUS_STOP ) return;

        i_old_playing_status = STATUS_STOP;
        p_main_intf->TogglePlayButton( PAUSE_S );
        p_main_intf->statusbar->SetStatusText( wxT(""), 0 );
        p_main_intf->statusbar->SetStatusText( wxT(""), 2 );

#ifdef wxHAS_TASK_BAR_ICON
        if( p_main_intf->p_systray )
        {
            p_main_intf->p_systray->UpdateTooltip(
                wxString(wxT("VLC media player - ")) + wxU(_("Stopped")) );
        }
#endif

        return;
    }

    /* Manage Playing status */
    vlc_value_t val;
    var_Get( p_input, "state", &val );
    val.i_int = val.i_int == PAUSE_S ? STATUS_PAUSE : STATUS_PLAYING;
    if( i_old_playing_status != val.i_int )
    {
        i_old_playing_status = val.i_int;
        p_main_intf->TogglePlayButton( val.i_int == STATUS_PAUSE ?
                                       PAUSE_S : PLAYING_S );

#ifdef wxHAS_TASK_BAR_ICON
        if( p_main_intf->p_systray )
        {
            p_main_intf->p_systray->UpdateTooltip(
                wxU(p_input->input.p_item->psz_name) + wxString(wxT(" - ")) +
                (val.i_int == PAUSE_S ? wxU(_("Paused")) : wxU(_("Playing"))));
        }
#endif
    }
}

void InputManager::UpdateDiscButtons()
{
    vlc_value_t val;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 && !disc_frame->IsShown() )
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
            disc_menu_button->Show();
            disc_sizer->Show( disc_menu_button );
            disc_sizer->Layout();
            disc_sizer->Fit( disc_frame );
            disc_menu_button->SetToolTip( wxU(_( HELP_MENU ) ) );
            disc_prev_button->SetToolTip( wxU(_( HELP_PCH ) ) );
            disc_next_button->SetToolTip( wxU(_( HELP_NCH ) ) );
        }
        else
        {
            disc_menu_button->Hide();
            disc_sizer->Hide( disc_menu_button );
            disc_prev_button->SetToolTip( wxU(_( HELP_PTR ) ) );
            disc_next_button->SetToolTip( wxU(_( HELP_NTR ) ) );
        }

        ShowDiscFrame();
    }
    else if( val.i_int == 0 && disc_frame->IsShown() )
    {
        HideDiscFrame();
    }
}

void InputManager::HideSlider()
{
    ShowSlider( false );
}

void InputManager::HideDiscFrame()
{
    ShowDiscFrame( false );
}

void InputManager::UpdateTime()
{
    char psz_time[ MSTRTIME_MAX_SIZE ], psz_total[ MSTRTIME_MAX_SIZE ];
    mtime_t i_seconds;

    i_seconds = var_GetTime( p_intf->p_sys->p_input, "length" ) / 1000000;
    secstotimestr( psz_total, i_seconds );

    i_seconds = var_GetTime( p_intf->p_sys->p_input, "time" ) / 1000000;
    secstotimestr( psz_time, i_seconds );

    p_main_intf->statusbar->SetStatusText(
        wxU(psz_time) + wxString(wxT(" / ")) +wxU(psz_total), 0 );
}

void InputManager::Update()
{
    /* Update the input */
    if( p_input == NULL )
    {
        UpdateInput();

        if( p_input )
        {
            slider->SetValue( 0 );
        }
        else
        {
            if( disc_frame->IsShown() ) HideDiscFrame();
            if( slider->IsShown() ) HideSlider();
        }
    }
    else if( p_input->b_dead )
    {
        UpdateButtons( VLC_FALSE );
        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_input && !p_input->b_die )
    {
        vlc_value_t pos, len;

        UpdateTime();
        UpdateButtons( VLC_TRUE );
        UpdateNowPlaying();
        UpdateDiscButtons();

        /* Really manage the slider */
        var_Get( p_input, "position", &pos );
        var_Get( p_input, "length", &len );

        if( len.i_time > 0 && pos.f_float >= 0 &&
            !slider->IsShown() ) ShowSlider();
        else if( len.i_time < 0 && pos.f_float <= 0 &&
                 slider->IsShown() ) HideSlider();

        /* Update the slider if the user isn't dragging it. */
        if( slider->IsShown() && b_slider_free )
        {
            i_slider_pos = (int)(SLIDER_MAX_POS * pos.f_float);
            slider->SetValue( i_slider_pos );
        }

        /* Manage Speed status */
        vlc_value_t val;
        var_Get( p_input, "rate", &val );
        if( i_old_rate != val.i_int )
        {
            p_main_intf->statusbar->SetStatusText(
                wxString::Format(wxT("x%.2f"),
                (float)INPUT_RATE_DEFAULT / val.i_int ), 1 );
            i_old_rate = val.i_int;
        }
    }
}

/*****************************************************************************
 * Event Handlers.
 *****************************************************************************/
void InputManager::OnDiscMenu( wxCommandEvent& WXUNUSED(event) )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( p_input )
    {
        vlc_value_t val; val.i_int = 2;

        var_Set( p_input, "title  0", val);
        vlc_object_release( p_input );
    }
}

void InputManager::OnDiscPrev( wxCommandEvent& WXUNUSED(event) )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( p_input )
    {
        int i_type = var_Type( p_input, "prev-chapter" );
        vlc_value_t val; val.b_bool = VLC_TRUE;

        var_Set( p_input, ( i_type & VLC_VAR_TYPE ) != 0 ?
                 "prev-chapter" : "prev-title", val );

        vlc_object_release( p_input );
    }
}

void InputManager::OnDiscNext( wxCommandEvent& WXUNUSED(event) )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( p_input )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        vlc_value_t val; val.b_bool = VLC_TRUE;

        var_Set( p_input, ( i_type & VLC_VAR_TYPE ) != 0 ?
                 "next-chapter" : "next-title", val );

        vlc_object_release( p_input );
    }
}

void InputManager::OnSliderUpdate( wxScrollEvent& event )
{
    vlc_mutex_lock( &p_intf->change_lock );

#ifdef WIN32
    if( event.GetEventType() == wxEVT_SCROLL_THUMBRELEASE
        || event.GetEventType() == wxEVT_SCROLL_ENDSCROLL )
    {
#endif
        if( i_slider_pos != event.GetPosition() && p_intf->p_sys->p_input )
        {
            vlc_value_t pos;
            pos.f_float = (float)event.GetPosition() / (float)SLIDER_MAX_POS;
            var_Set( p_intf->p_sys->p_input, "position", pos );
        }

#ifdef WIN32
        b_slider_free = VLC_TRUE;
    }
    else
    {
        b_slider_free = VLC_FALSE;
        if( p_intf->p_sys->p_input ) UpdateTime();
    }
#endif

#undef WIN32
    vlc_mutex_unlock( &p_intf->change_lock );
}

void InputManager::ShowSlider( bool show )
{
    if( !!show == !!slider->IsShown() ) return;

    if( p_intf->p_sys->b_video_autosize )
        UpdateVideoWindow( p_intf, p_main_intf->video_window );

    sizer->Show( slider, show );
    sizer->Layout();

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_main_intf->AddPendingEvent( intf_event );
}

void InputManager::ShowDiscFrame( bool show )
{
    if( !!show == !!disc_frame->IsShown() ) return;

    if( p_intf->p_sys->b_video_autosize )
        UpdateVideoWindow( p_intf, p_main_intf->video_window );

    sizer->Show( disc_frame, show );
    sizer->Layout();

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_main_intf->AddPendingEvent( intf_event );
}
