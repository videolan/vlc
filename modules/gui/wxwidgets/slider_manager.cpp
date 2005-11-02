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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

#include "vlc_meta.h"

#include "wxwidgets.hpp"

#include "slider_manager.hpp"

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
SliderManager::SliderManager( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    p_input = NULL;
    i_old_playing_status = PAUSE_S;
}

SliderManager::~SliderManager()
{
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_intf->p_sys->p_input ) vlc_object_release( p_intf->p_sys->p_input );
    p_intf->p_sys->p_input = NULL;
    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void SliderManager::Update()
{
    /* Update the input */
    if( p_input == NULL )
    {
        UpdateInput();
        if( p_input )
        {
            _slider->SetValue( 0 );
            UpdateNowPlaying();

            UpdateButtons( VLC_TRUE );

            i_old_playing_status = PLAYING_S;
        }
    }
    else if( p_input->b_dead )
    {
        HideControls();
        HideSlider();

        UpdateButtons( VLC_FALSE );

        i_old_playing_status = PAUSE_S;

        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_input )
    {
        if( !p_input->b_die )
        {
            vlc_value_t pos;

            DontHide();

            UpdateNowPlaying();

            /* Really manage the slider */
            var_Get( p_input, "position", &pos );

            UpdateDiscButtons();

            if( pos.f_float > 0.0 && ! IsShown() )
            {
                 ShowSlider();
            }
            else if( pos.f_float <= 0.0 )
            {
                HideSlider();
            }

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

                        _slider->SetValue( i_slider_pos );

                        var_Get( p_input, "time", &time );
                        i_seconds = time.i_time / 1000000;
                        secstotimestr ( psz_time, i_seconds );

                        var_Get( p_input, "length",  &time );
                        i_seconds = time.i_time / 1000000;
                        secstotimestr ( psz_total, i_seconds );

                        UpdateTime( psz_time, psz_total );
//                        p_main_interface->statusbar->SetStatusText(
//                            wxU(psz_time) + wxString(wxT(" / ")) +
//                            wxU(psz_total), 0 );
                    }
                }
            }
        }
    }
}
