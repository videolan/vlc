/*****************************************************************************
 * interaction.hpp : Headers for an interaction dialog
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: bookmarks.hpp 13444 2005-11-28 23:33:48Z dionoea $
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

#ifndef _WXVLC_INTERACTION_H_
#define _WXVLC_INTERACTION_H_

#include "wxwidgets.hpp"
#include <vlc_interaction.h>

#include <vector>
using namespace std;

namespace wxvlc
{

    struct InputWidget
    {
        /// \todo Clean up
        wxTextCtrl *control;
        vlc_value_t *val;
        int i_type;
    };

    class InteractionDialog: public wxDialog
    {
    public:
        /* Constructor */
        InteractionDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                           interaction_dialog_t * );
        virtual ~InteractionDialog();

        void Update();

    private:
        /* Event handlers (these functions should _not_ be virtual) */
        void OnClose ( wxCloseEvent& event );
        void OnOkYes ( wxCommandEvent& event );
        void OnCancel( wxCommandEvent& event );
        void OnNo    ( wxCommandEvent& event );
        void OnClear ( wxCommandEvent& event );
        void OnNoShow( wxCommandEvent& event );

        void Render();
        void Finish( int );

        wxBoxSizer *widgets_sizer;
        wxPanel    *widgets_panel;
        wxBoxSizer *buttons_sizer;
        wxPanel    *buttons_panel;

        wxBoxSizer *main_sizer;

        DECLARE_EVENT_TABLE();

        vector<InputWidget> input_widgets;

        intf_thread_t *p_intf;
        interaction_dialog_t *p_dialog;
        wxWindow *p_parent;

        bool b_noshow;
    };
};

#endif
