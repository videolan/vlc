/*****************************************************************************
 * vlm_slider_manager.cpp : Manage an input slider for a VLM stream
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
                                    VLMBroadcastStreamPanel *_p_sp ) :
        SliderManager( _p_intf )
{
    p_sp = _p_sp;
    _slider = p_sp->p_slider;

    b_slider_free = VLC_TRUE;
    time_string = wxU( "0:00:00 / 0:00:00");
}

VLMSliderManager::~VLMSliderManager()
{
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
    return _slider->IsEnabled();
}

void VLMSliderManager::ShowSlider()
{
    _slider->Enable();
}

void VLMSliderManager::HideSlider()
{
    _slider->SetValue( 0 );
    _slider->Disable();
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
