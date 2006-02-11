/*****************************************************************************
 * iteminfo.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
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

#include "dialogs/iteminfo.hpp"
#include "dialogs/infopanels.hpp"
#include <wx/combobox.h>
#include <wx/statline.h>

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Uri_Event,
    Name_Event,
    Enabled_Event,
};

BEGIN_EVENT_TABLE(ItemInfoDialog, wxDialog)
    /* Button events */
    EVT_BUTTON(wxID_OK, ItemInfoDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, ItemInfoDialog::OnCancel)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
ItemInfoDialog::ItemInfoDialog( intf_thread_t *_p_intf,
                                playlist_item_t *_p_item,
                                wxWindow* _p_parent ):
    wxDialog( _p_parent, -1, wxU(_("Playlist item info")),
             wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    p_item = _p_item;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    /* Create the standard info panel */
    info_panel = new ItemInfoPanel(p_intf, panel, true );
    info_panel->Update( &(p_item->input) );
    /* Separation */
    wxStaticLine *static_line = new wxStaticLine( panel, wxID_OK );

    /* Create the buttons */
    wxButton *ok_button = new wxButton( panel, wxID_OK );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( info_panel, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxALIGN_LEFT | wxALIGN_BOTTOM |
                      wxALL, 5 );
    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

ItemInfoDialog::~ItemInfoDialog()
{
}


/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void ItemInfoDialog::OnOk( wxCommandEvent& WXUNUSED(event) )
{
    vlc_mutex_lock( &p_item->input.lock );
    p_item->input.psz_name = info_panel->GetName();
    p_item->input.psz_uri = info_panel->GetURI();
    vlc_mutex_unlock( &p_item->input.lock );
    EndModal( wxID_OK );
}

void ItemInfoDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_CANCEL );
}
