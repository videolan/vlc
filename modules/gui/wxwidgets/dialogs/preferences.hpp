/*****************************************************************************
 * preferences.hpp : Headers for the preference dialog
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _WXVLC_PREFERENCES_H_
#define _WXVLC_PREFERENCES_H_

#include "wxwidgets.hpp"

class PrefsTreeCtrl;
class PrefsPanel;

namespace wxvlc
{
    /** This class is the preferences window
     *  It includes a preferences panel (right part)
     *  and the PrefsTreeCtrl (left part)
     */
    class PrefsDialog: public wxFrame
    {
    public:
        /** Constructor */
        PrefsDialog( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~PrefsDialog();

    private:
        wxPanel *PrefsPanel( wxWindow* parent );

        /* Event handlers (these functions should _not_ be virtual) */
        void OnOk( wxCommandEvent& event );
        void OnCancel( wxCommandEvent& event );
        void OnSave( wxCommandEvent& event );
        void OnResetAll( wxCommandEvent& event );
        void OnAdvanced( wxCommandEvent& event );
        void OnClose( wxCloseEvent& event );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;

        PrefsTreeCtrl *prefs_tree;
    };

};

#endif

