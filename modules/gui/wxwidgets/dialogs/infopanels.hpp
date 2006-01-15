/*****************************************************************************
 * infopanels.hpp: Information panels (statistics, general info, ...)
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: iteminfo.hpp 13905 2006-01-12 23:10:04Z dionoea $
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

#ifndef _WXVLC_INFOPANELS_H_
#define _WXVLC_INFOPANELS_H_

#include "wxwidgets.hpp"

#include <wx/treectrl.h>

namespace wxvlc
{
class ItemInfoPanel: public wxPanel
{
public:
    /* Constructor */
    ItemInfoPanel( intf_thread_t *p_intf, wxWindow *p_parent, bool );
    virtual ~ItemInfoPanel();

    void Update( input_item_t *);
    void Clear();

    void OnOk();
    void OnCancel();

private:
    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    input_item_t *p_item;
    wxWindow *p_parent;

    wxTextCtrl *uri_text;
    wxTextCtrl *name_text;
    wxStaticText *uri_label;
    wxStaticText *name_label;

    wxTreeCtrl *info_tree;
    wxTreeItemId info_root;

    bool b_modifiable;
};

class InputStatsInfoPanel: public wxPanel
{
public:
    /* Constructor */
    InputStatsInfoPanel( intf_thread_t *p_intf,wxWindow *p_parent );
    virtual ~InputStatsInfoPanel();

    void Update( input_item_t *);
    void Clear();

    void OnOk();
    void OnCancel();

private:
    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    input_item_t *p_item;
    wxWindow *p_parent;

    wxStaticText *read_bytes_text;
    wxStaticText *input_bitrate_text;
    wxStaticText *demux_bytes_text;
    wxStaticText *demux_bitrate_text;

    wxStaticText *video_decoded_text;
    wxStaticText *displayed_text;
    wxStaticText *lost_frames_text;
};
};
#endif
