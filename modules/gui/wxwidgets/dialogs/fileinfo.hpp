/*****************************************************************************
 * fileinfo.hpp: private wxWindows interface description
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

#ifndef _WXVLC_FILEINFO_H_
#define _WXVLC_FILEINFO_H_

#include "wxwidgets.hpp"

#include <wx/treectrl.h>

namespace wxvlc
{
    class MetaDataPanel;
    class AdvancedInfoPanel;
    class InputStatsInfoPanel;
    class FileInfo: public wxFrame
    {
    public:
        /* Constructor */
        FileInfo( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~FileInfo();
        void Update();

        vlc_bool_t b_need_update;
        bool b_stats;

    private:
        void OnButtonClose( wxCommandEvent& event );
        void OnClose( wxCloseEvent& WXUNUSED(event) );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;

        mtime_t last_update;

        MetaDataPanel *item_info;
        AdvancedInfoPanel *advanced_info;
        InputStatsInfoPanel *stats_info;

        wxBoxSizer *panel_sizer;
    };
};

#endif
