/*****************************************************************************
 * vlm_streampanel.hpp: Panel for a VLM stream
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

#ifndef _VLM_STREAMPANEL_H_
#define _VLM_STREAMPANEL_H_

#include "wxwidgets.hpp"

class VLMStream;
class VLMBroadcastStream;
class VLMVODStream;

namespace wxvlc
{
    class VLMSliderManager;

    /**
     * This class represents the panel for a VLM Stream
     * This class is abstract, it needs to be subclassed
     */
    class VLMStreamPanel : public wxPanel
    {
    public:
        VLMStreamPanel( intf_thread_t *, wxWindow * );
        virtual ~VLMStreamPanel();

        virtual void TogglePlayButton( int ) {};
        wxSlider  *p_slider;

        virtual void Update() = 0;
    protected:
        intf_thread_t *p_intf;
        vlc_bool_t b_free;
        vlc_bool_t b_new;               ///< Is it a new stream ?
        vlc_bool_t b_found;             ///< Have we found the stream here ?
        friend class VLMPanel;


    private:
    };

    /**
     * This class represents the panel for a Broadcast VLM Stream
     */
    class VLMBroadcastStreamPanel : public VLMStreamPanel
    {
    public:
        VLMBroadcastStreamPanel( intf_thread_t *, wxWindow *,
                                 VLMBroadcastStream * );
        virtual ~VLMBroadcastStreamPanel();
        VLMBroadcastStream *GetStream() { return p_stream; }

        vlc_bool_t b_slider_free;

        VLMSliderManager *p_sm;

        virtual void Update();

        virtual void TogglePlayButton( int );
    protected:

    private:
        VLMBroadcastStream *p_stream;
        DECLARE_EVENT_TABLE();

        void OnPlay( wxCommandEvent &);
        void OnStop( wxCommandEvent &);
        void OnEdit( wxCommandEvent &);
        void OnTrash( wxCommandEvent &);
        void OnSliderUpdate( wxScrollEvent &);


        wxBitmapButton *play_button;

        wxStaticText *p_time;
    };

    /**
     * This class represents the panel for a VOD VLM Stream
     */
    class VLMVODStreamPanel : public VLMStreamPanel
    {
    public:
        VLMVODStreamPanel( intf_thread_t *, wxWindow *,
                                 VLMVODStream * );
        virtual ~VLMVODStreamPanel();

        VLMVODStream *GetStream() { return p_stream; }

        virtual void Update() {}
    protected:

    private:
        VLMVODStream *p_stream;
    };
};

#endif
