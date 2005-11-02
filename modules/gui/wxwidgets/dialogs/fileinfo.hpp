/*****************************************************************************
 * fileinfo.hpp: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12502 2005-09-09 19:38:01Z gbazin $
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

#ifndef _WXVLC_FILEINFO_H_
#define _WXVLC_FILEINFO_H_

#include "wxwidgets.hpp"

#include <wx/treectrl.h>

namespace wxvlc
{
    class FileInfo: public wxFrame
    {
    public:
        /* Constructor */
        FileInfo( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~FileInfo();
        void UpdateFileInfo();

        vlc_bool_t b_need_update;

    private:
        void OnButtonClose( wxCommandEvent& event );
        void OnClose( wxCloseEvent& WXUNUSED(event) );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        wxTreeCtrl *fileinfo_tree;
        wxTreeItemId fileinfo_root;
        wxString fileinfo_root_label;
    };
};

#endif
