/*****************************************************************************
 * iteminfo.hpp: private wxWindows interface description
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

#ifndef _WXVLC_ITEMINFO_H_
#define _WXVLC_ITEMINFO_H_

#include "wxwidgets.hpp"
#include "dialogs/infopanels.hpp"
#include <wx/treectrl.h>

namespace wxvlc
{
class ItemInfoDialog: public wxDialog
{
public:
    /* Constructor */
    ItemInfoDialog( intf_thread_t *p_intf, playlist_item_t *_p_item,
                    wxWindow *p_parent );
    virtual ~ItemInfoDialog();

    wxArrayString GetOptions();

private:
    wxPanel *GroupPanel( wxWindow* parent );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );

    void UpdateInfo();

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    playlist_item_t *p_item;
    wxWindow *p_parent;

    /* Controls for the iteminfo dialog box */
    wxPanel *info_subpanel;
    MetaDataPanel *info_panel;

    wxPanel *group_subpanel;
    wxPanel *group_panel;

    wxTextCtrl *uri_text;
    wxTextCtrl *name_text;

    wxTreeCtrl *info_tree;
    wxTreeItemId info_root;

};

};
#endif
