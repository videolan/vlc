/*****************************************************************************
 * vlm_slider_manager.hpp: Header for slider_manager
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

#ifndef _VLM_SLIDER_MANAGER_H_
#define _VLM_SLIDER_MANAGER_H_

#include "slider_manager.hpp"

namespace wxvlc
{
    class VLMBroadcastStreamPanel;
    /**
     * This class manages a slider corresponding to the main input
     */
    class VLMSliderManager: public SliderManager
    {
    public:
        VLMSliderManager( intf_thread_t *, VLMBroadcastStreamPanel * );
        virtual ~VLMSliderManager();

        wxString time_string;

        void ProcessUpdate( wxScrollEvent & );

    protected:
        vlc_bool_t b_slider_free;
        virtual void UpdateInput();
        virtual void UpdateButtons( vlc_bool_t );
        virtual void UpdateTime( char *, char *);

        virtual vlc_bool_t IsShown();
        virtual vlc_bool_t IsFree();
        virtual vlc_bool_t IsPlaying();

        virtual void HideSlider();
        virtual void ShowSlider();

        VLMBroadcastStreamPanel * p_sp;
    };
};

#endif
