/*****************************************************************************
 * updatevlc.hpp: VLC Update checker
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12670 2005-09-25 11:16:31Z zorglub $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _WXVLC_UPDATEVLC_H_
#define _WXVLC_UPDATEVLC_H_

#include "wxwidgets.hpp"
#include <wx/treectrl.h>
#include <list>

class wxTreeCtrl;

namespace wxvlc
{
    class UpdateVLC: public wxFrame
    {
    public:
        /** Constructor */
        UpdateVLC( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~UpdateVLC();

    private:
        void OnButtonClose( wxCommandEvent& event );
        void OnClose( wxCloseEvent& WXUNUSED(event) );
        void GetData();
        void OnCheckForUpdate( wxCommandEvent& event );
        void OnMirrorChoice( wxCommandEvent& event );
        void UpdateUpdatesTree();
        void UpdateMirrorsChoice();
        void OnUpdatesTreeActivate( wxTreeEvent& event );
        void DownloadFile( wxString url, wxString dst );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        wxTreeCtrl *updates_tree;

        wxChoice *mirrors_choice;

        wxString release_type; /* could be "stable", "test", "nightly" ... */

        struct update_file_t
        {
            wxString type;
            wxString md5;
            wxString size;
            wxString url;
            wxString description;
        };

        struct update_version_t
        {
            wxString type;
            wxString major;
            wxString minor;
            wxString revision;
            wxString extra;
            std::list<update_file_t> m_files;
        };

        std::list<update_version_t> m_versions;

        struct update_mirror_t
        {
            wxString name;
            wxString location;
            wxString type;
            wxString base_url;
        };

        std::list<update_mirror_t> m_mirrors;
    };
};

#endif
