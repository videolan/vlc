/*****************************************************************************
 * infopanels.hpp: Information panels (statistics, general info, ...)
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
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
class MetaDataPanel: public wxPanel
{
public:
    /* Constructor */
    MetaDataPanel( intf_thread_t *p_intf, wxWindow *p_parent, bool );
    virtual ~MetaDataPanel();

    void Update( input_item_t *);
    void Clear();

    char* GetURI();
    char* GetName();

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

    wxStaticText *artist_text;
    wxStaticText *genre_text;
    wxStaticText *copyright_text;
    wxStaticText *collection_text;
    wxStaticText *seqnum_text;
    wxStaticText *description_text;
    wxStaticText *rating_text;
    wxStaticText *date_text;
    wxStaticText *setting_text;
    wxStaticText *language_text;
    wxStaticText *nowplaying_text;
    wxStaticText *publisher_text;

    bool b_modifiable;
};


class AdvancedInfoPanel: public wxPanel
{
public:
    /* Constructor */
    AdvancedInfoPanel( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~AdvancedInfoPanel();

    void Update( input_item_t *);
    void Clear();


    char* GetURI();
    char* GetName();

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

    wxBoxSizer *panel_sizer;
    wxFlexGridSizer *sizer;

    wxFlexGridSizer *input_sizer;
    wxStaticBoxSizer *input_bsizer;
    wxStaticText *read_bytes_text;
    wxStaticText *input_bitrate_text;
    wxStaticText *demux_bytes_text;
    wxStaticText *demux_bitrate_text;

    wxFlexGridSizer *video_sizer;
    wxStaticBoxSizer *video_bsizer;
    wxStaticText *video_decoded_text;
    wxStaticText *displayed_text;
    wxStaticText *lost_frames_text;

    wxFlexGridSizer *sout_sizer;
    wxStaticBoxSizer *sout_bsizer;
    wxStaticText *sout_sent_packets_text;
    wxStaticText *sout_sent_bytes_text;
    wxStaticText *sout_send_bitrate_text;

    wxFlexGridSizer *audio_sizer;
    wxStaticBoxSizer *audio_bsizer;
    wxStaticText *audio_decoded_text;
    wxStaticText *played_abuffers_text;
    wxStaticText *lost_abuffers_text;
};
};
#endif
