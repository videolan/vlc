/*****************************************************************************
 * slider_manager.hpp: Header for slider_manager
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12502 2005-09-09 19:38:01Z gbazin $
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

#ifndef _SLIDER_MANAGER_H_
#define _SLIDER_MANAGER_H_

#include "wxwidgets.hpp"

namespace wxvlc
{
    /**
     * This class manages a slider corresponding to an input
     * This class is abstract, it needs to be subclassed
     */
    class SliderManager
    {
    public:
        SliderManager( intf_thread_t *p_intf );
        virtual ~SliderManager();

        void Update();

    protected:
        virtual void UpdateInput() = 0;
        virtual void UpdateNowPlaying() {};
        virtual void UpdateButtons( vlc_bool_t ) {};
        virtual void UpdateDiscButtons() {}
        virtual void UpdateTime( char *, char *)  = 0;

        virtual vlc_bool_t IsShown() = 0;
        virtual vlc_bool_t IsFree() = 0;
        virtual vlc_bool_t IsPlaying() = 0;

        virtual void HideSlider() {};
        virtual void ShowSlider() {};

        virtual void HideControls() {};
        virtual void DontHide() {};

        intf_thread_t * p_intf;
        input_thread_t *p_input;
        wxSlider *_slider;           ///< Slider for this input
        int i_slider_pos;            ///< Current slider position

    private:
        int i_old_playing_status;    ///< Previous playing status


    };
};

#endif
