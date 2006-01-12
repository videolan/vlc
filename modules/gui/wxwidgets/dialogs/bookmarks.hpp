/*****************************************************************************
 * bookmarks.hpp: Headers for the bookmarks window
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

#ifndef _WXVLC_BOOKMARKS_H_
#define _WXVLC_BOOKMARKS_H_

#include "wxwidgets.hpp"
#include <wx/listctrl.h>

namespace wxvlc
{
    class BookmarksDialog: public wxFrame
    {
    public:
        /* Constructor */
        BookmarksDialog( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~BookmarksDialog();

        bool Show( bool );

    private:
        void Update();

        /* Event handlers (these functions should _not_ be virtual) */
        void OnClose( wxCloseEvent& event );
        void OnAdd( wxCommandEvent& event );
        void OnDel( wxCommandEvent& event );
        void OnClear( wxCommandEvent& event );
        void OnActivateItem( wxListEvent& event );
        void OnUpdate( wxCommandEvent &event );
        void OnEdit( wxCommandEvent& event );
        void OnExtract( wxCommandEvent& event );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        wxWindow *p_parent;

        wxListView *list_ctrl;
    };

    class BookmarkEditDialog : public wxDialog
    {
    public:
        /* Constructor */
        BookmarkEditDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                            seekpoint_t *p_seekpoint );
        virtual ~BookmarkEditDialog();
        seekpoint_t *p_seekpoint;
    private:
        wxTextCtrl *name_text, *time_text, *bytes_text;

        void OnOK( wxCommandEvent& event);
        void OnCancel( wxCommandEvent& event);

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
    };
};

#endif
