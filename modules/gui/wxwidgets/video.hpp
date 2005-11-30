/*****************************************************************************
 * video.hpp: Embedded video management
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifndef _WXVLC_VIDEO_H_
#define _WXVLC_VIDEO_H_

#include "wxwidgets.hpp"

namespace wxvlc
{
    class VideoWindow: public wxWindow
    {
    public:
        /* Constructor */
        VideoWindow( intf_thread_t *_p_intf, wxWindow *p_parent );
        virtual ~VideoWindow();

        void *GetWindow( vout_thread_t *p_vout, int *, int *,
                         unsigned int *, unsigned int * );
        void ReleaseWindow( void * );
        int  ControlWindow( void *, int, va_list );

        mtime_t i_creation_date;

    private:
        intf_thread_t *p_intf;
        vout_thread_t *p_vout;
        wxWindow *p_parent;
        vlc_mutex_t lock;
        vlc_bool_t b_shown;
        vlc_bool_t b_auto_size;

        wxWindow *p_child_window;

        wxTimer m_hide_timer;

        void UpdateSize( wxEvent& event );
        void UpdateHide( wxEvent& event );
        void OnControlEvent( wxCommandEvent& event );
        void OnHideTimer( wxTimerEvent& WXUNUSED(event));

        DECLARE_EVENT_TABLE();
    };
};

/* Delegates - Todo: fix this (remove 1st, make 2nd method) */
wxWindow *CreateVideoWindow( intf_thread_t *p_intf, wxWindow *p_parent );
void UpdateVideoWindow( intf_thread_t *p_intf, wxWindow *p_window );

#endif
