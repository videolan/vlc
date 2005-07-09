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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>

#include <wx/combobox.h>
#include <wx/statline.h>

#include <vlc/intf.h>

#include "wxwindows.h"

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
    wxPanel *info_panel = InfoPanel( panel );

    /* Separation */
    wxStaticLine *static_line = new wxStaticLine( panel, wxID_OK );

    /* Create the buttons */
    wxButton *ok_button = new wxButton( panel, wxID_OK, wxU(_("OK")) );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL,
                                            wxU(_("Cancel")) );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( info_panel, 0, wxEXPAND | wxALL, 5 );
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
 * Private methods.
 *****************************************************************************/
wxPanel *ItemInfoDialog::InfoPanel( wxWindow* parent )
{
    wxPanel *info_panel = new wxPanel( parent, -1, wxDefaultPosition,
                                  wxDefaultSize );
    info_panel->SetAutoLayout( TRUE );
    wxBoxSizer *info_sizer = new wxBoxSizer( wxHORIZONTAL );

    /* Create a box to surround the controls */
    wxStaticBox *panel_box = new wxStaticBox( info_panel, -1,
                                   wxU(_("Item Info")) );

    wxStaticBoxSizer *box_sizer = new wxStaticBoxSizer( panel_box,
                                                          wxVERTICAL );

    wxFlexGridSizer *sizer = new wxFlexGridSizer(2,3,20);
    /* URI Textbox */
    wxStaticText *uri_label =
           new wxStaticText( info_panel, -1, wxU(_("URI")) );

    uri_text = new wxTextCtrl( info_panel, Uri_Event,
        wxU(p_item->input.psz_uri), wxDefaultPosition, wxSize( 300, -1 ),
        wxTE_PROCESS_ENTER );

    sizer->Add( uri_label, 0 , wxALIGN_LEFT |wxALL , 5 );
    sizer->Add( uri_text, 1 , wxALIGN_RIGHT | wxALL , 5 );

    /* Name Textbox */
    wxStaticText *name_label =
           new wxStaticText(  info_panel, -1, wxU(_("Name")) );

    name_text = new wxTextCtrl( info_panel, Uri_Event,
        wxU(p_item->input.psz_name), wxDefaultPosition, wxSize( 300, -1 ),
        wxTE_PROCESS_ENTER );

    sizer->Add( name_label, 0 , wxALIGN_LEFT |wxALL , 5  );
    sizer->Add( name_text, 1 , wxALIGN_RIGHT | wxALL , 5 );

    /* Treeview */
    info_tree = new wxTreeCtrl( info_panel, -1, wxDefaultPosition,
                                wxSize(220,200),
                                wxSUNKEN_BORDER |wxTR_HAS_BUTTONS |
                                wxTR_HIDE_ROOT );

    sizer->Layout();
    box_sizer->Add( sizer, 0, wxEXPAND, 5 );
    box_sizer->Add( info_tree, 0, wxEXPAND, 5 );
    info_sizer->Add( box_sizer, 1, wxBOTTOM, 5 );

    info_panel->SetSizer( info_sizer );
    info_sizer->Layout();
    info_sizer->SetSizeHints( info_panel );

    UpdateInfo();

    return info_panel;
}

void ItemInfoDialog::UpdateInfo()
{
    if( !info_root )
    {
       info_root = info_tree->AddRoot( wxU( p_item->input.psz_name) );
    }

    /* Rebuild the tree */
    for( int i = 0; i< p_item->input.i_categories ; i++)
    {
        wxTreeItemId cat = info_tree->AppendItem( info_root,
                            wxU( p_item->input.pp_categories[i]->psz_name) );

        for( int j = 0 ; j < p_item->input.pp_categories[i]->i_infos ; j++ )
        {
           info_tree->AppendItem( cat , (wxString)
               wxU(p_item->input.pp_categories[i]->pp_infos[j]->psz_name) +
               wxT(": ") +
               wxU(p_item->input.pp_categories[i]->pp_infos[j]->psz_value) );
        }

        info_tree->Expand( cat );
    }
}

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void ItemInfoDialog::OnOk( wxCommandEvent& WXUNUSED(event) )
{
    vlc_mutex_lock( &p_item->input.lock );
    p_item->input.psz_name = strdup( name_text->GetLineText(0).mb_str() );
    p_item->input.psz_uri = strdup( uri_text->GetLineText(0).mb_str() );
    vlc_mutex_unlock( &p_item->input.lock );
    EndModal( wxID_OK );
}

void ItemInfoDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_CANCEL );
}
