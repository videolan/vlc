/*****************************************************************************
 * input_manager.hpp: Header for input_manager
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _INPUT_MANAGER_H_
#define _INPUT_MANAGER_H_

#include "wxwidgets.hpp"

namespace wxvlc
{
    class Interface;

    /**
     * This class manages all the controls related to the input
     */
    class InputManager : public wxPanel
    {
    public:
        InputManager( intf_thread_t *, Interface *, wxWindow * );
        virtual ~InputManager();

        void Update();
        vlc_bool_t IsPlaying();

    protected:
        void UpdateInput();
        void UpdateNowPlaying();
        void UpdateButtons( vlc_bool_t );
        void UpdateDiscButtons();
        void UpdateTime();

        void HideSlider();
        void ShowSlider( bool show = true );

        void OnSliderUpdate( wxScrollEvent& event );

        void OnDiscMenu( wxCommandEvent& event );
        void OnDiscPrev( wxCommandEvent& event );
        void OnDiscNext( wxCommandEvent& event );

        void HideDiscFrame();
        void ShowDiscFrame( bool show = true );

        wxPanel         *disc_frame;
        wxBoxSizer      *disc_sizer;
        wxBitmapButton  *disc_menu_button;
        wxBitmapButton  *disc_prev_button;
        wxBitmapButton  *disc_next_button;

        intf_thread_t * p_intf;
        input_thread_t *p_input;
        Interface * p_main_intf;

        wxSlider *slider;            ///< Slider for this input
        int i_slider_pos;            ///< Current slider position
        vlc_bool_t b_slider_free;    ///< Slider status

        wxBoxSizer *sizer;

    private:
        DECLARE_EVENT_TABLE();

        int i_old_playing_status;    ///< Previous playing status
        int i_old_rate;              ///< Previous playing rate

        mtime_t i_input_hide_delay;  ///< Allows delaying slider hidding
    };
};

#endif
