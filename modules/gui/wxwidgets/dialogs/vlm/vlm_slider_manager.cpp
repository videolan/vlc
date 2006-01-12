/*****************************************************************************
 * vlm_slider_manager.cpp : Manage an input slider for a VLM stream
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "dialogs/vlm/vlm_slider_manager.hpp"
#include "dialogs/vlm/vlm_stream.hpp"
#include "dialogs/vlm/vlm_streampanel.hpp"

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
VLMSliderManager::VLMSliderManager( intf_thread_t *_p_intf,
                                    VLMBroadcastStreamPanel *_p_sp )
{
    p_intf = _p_intf;
    p_input = NULL;

    p_sp = _p_sp;
    slider = p_sp->p_slider;

    b_slider_free = VLC_TRUE;
    time_string = wxU( "0:00:00 / 0:00:00");
}

VLMSliderManager::~VLMSliderManager()
{
}

/*****************************************************************************
 * Public methods.
 *****************************************************************************/
void VLMSliderManager::Update()
{
    /* Update the input */
    if( p_input == NULL )
    {
        UpdateInput();

        if( p_input )
        {
            slider->SetValue( 0 );
            UpdateButtons( VLC_TRUE );
        }
    }
    else if( p_input->b_dead )
    {
        HideSlider();
        UpdateButtons( VLC_FALSE );

        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_input && !p_input->b_die )
    {
        vlc_value_t pos;

        /* Really manage the slider */
        var_Get( p_input, "position", &pos );

        if( pos.f_float > 0.0 && ! IsShown() ) ShowSlider();
        else if( pos.f_float <= 0.0 ) HideSlider();

        if( IsPlaying() && IsShown() )
        {
            /* Update the slider if the user isn't dragging it. */
            if( IsFree() )
            {
                char psz_time[ MSTRTIME_MAX_SIZE ];
                char psz_total[ MSTRTIME_MAX_SIZE ];
                vlc_value_t time;
                mtime_t i_seconds;

                /* Update the value */
                if( pos.f_float >= 0.0 )
                {
                    i_slider_pos = (int)(SLIDER_MAX_POS * pos.f_float);

                    slider->SetValue( i_slider_pos );

                    var_Get( p_input, "time", &time );
                    i_seconds = time.i_time / 1000000;
                    secstotimestr ( psz_time, i_seconds );

                    var_Get( p_input, "length",  &time );
                    i_seconds = time.i_time / 1000000;
                    secstotimestr ( psz_total, i_seconds );

                    UpdateTime( psz_time, psz_total );
                }
            }
        }
    }
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void VLMSliderManager::UpdateInput()
{
    if( p_sp->GetStream()->p_media->i_instance == 0 )
    {
        p_input = NULL;
        return;
    }
    /** FIXME !! */
    p_input = p_sp->GetStream()->p_media->instance[0]->p_input;
}

void VLMSliderManager::UpdateButtons( vlc_bool_t b_play )
{
    if( b_play )
    {
        p_sp->TogglePlayButton( PLAYING_S );
    }
    else
    {
        p_sp->TogglePlayButton( PAUSE_S );
    }
}

vlc_bool_t VLMSliderManager::IsShown()
{
    return slider->IsEnabled();
}

void VLMSliderManager::ShowSlider()
{
    slider->Enable();
}

void VLMSliderManager::HideSlider()
{
    slider->SetValue( 0 );
    slider->Disable();
    UpdateTime( "0:00:00", "0:00:00" );
}

vlc_bool_t VLMSliderManager::IsFree()
{
    return b_slider_free;
}

vlc_bool_t VLMSliderManager::IsPlaying()
{
    return VLC_TRUE; /* Is it really useful ? */
}

void VLMSliderManager::UpdateTime( char *psz_time, char *psz_total )
{
    time_string = wxU(psz_time) + wxString(wxT(" / ") ) +wxU(psz_total) ;
}


void VLMSliderManager::ProcessUpdate( wxScrollEvent &event )
{

#ifdef WIN32
    if( event.GetEventType() == wxEVT_SCROLL_THUMBRELEASE
        || event.GetEventType() == wxEVT_SCROLL_ENDSCROLL )
    {
#endif
        if( i_slider_pos != event.GetPosition() && p_input )
        {
            vlc_value_t pos;
            pos.f_float = (float)event.GetPosition() / (float)SLIDER_MAX_POS;

            var_Set( p_input, "position", pos );
        }

#ifdef WIN32
        b_slider_free = VLC_TRUE;
    }
    else
    {
        b_slider_free = VLC_FALSE;

        if( p_input )
        {
            /* Update stream date */
            char psz_time[ MSTRTIME_MAX_SIZE ], psz_total[ MSTRTIME_MAX_SIZE ];
            mtime_t i_seconds;

            i_seconds = var_GetTime( p_input, "length" ) / I64C(1000000 );
            secstotimestr( psz_total, i_seconds );

            i_seconds = var_GetTime( p_input, "time" ) /   I64C(1000000 );
            secstotimestr( psz_time, i_seconds );

            time_string = wxU(psz_time) + wxString(wxT(" / ") ) +wxU(psz_total) ;
        }
    }
#endif

#undef WIN32
}
