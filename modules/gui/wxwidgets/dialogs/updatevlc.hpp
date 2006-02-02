/*****************************************************************************
 * updatevlc.hpp: VLC Update checker
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#ifndef _WXVLC_UPDATEVLC_H_
#define _WXVLC_UPDATEVLC_H_

#include <vlc_update.h>

#include "wxwidgets.hpp"
#include <wx/treectrl.h>

class wxTreeCtrl;

namespace wxvlc
{
    class UpdateVLC: public wxFrame
    {
    public:
        UpdateVLC( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~UpdateVLC();

    private:
        void OnButtonClose( wxCommandEvent& event );
        void OnClose( wxCloseEvent& WXUNUSED(event) );
        void OnCheckForUpdate( wxCommandEvent& event );
        void OnChooseItem( wxListEvent& event );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        update_t *p_u;
    };
};

#endif
