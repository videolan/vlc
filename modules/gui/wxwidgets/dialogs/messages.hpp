/*****************************************************************************
 * messages.hpp: Messages dialog
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12670 2005-09-25 11:16:31Z zorglub $
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

#ifndef _WXVLC_MESSAGES_H_
#define _WXVLC_MESSAGES_H_

#include "wxwidgets.hpp"

namespace wxvlc
{
    class Messages: public wxFrame
    {
    public:
        /* Constructor */
        Messages( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~Messages();
        bool Show( bool show = TRUE );
        void UpdateLog();

    private:
        /* Event handlers (these functions should _not_ be virtual) */
        void OnButtonClose( wxCommandEvent& event );
        void OnClose( wxCloseEvent& WXUNUSED(event) );
        void OnClear( wxCommandEvent& event );
        void OnSaveLog( wxCommandEvent& event );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        wxTextCtrl *textctrl;
        wxTextAttr *info_attr;
        wxTextAttr *err_attr;
        wxTextAttr *warn_attr;
        wxTextAttr *dbg_attr;

        wxFileDialog *save_log_dialog;

        vlc_bool_t b_verbose;
    };
};

#endif
