/*****************************************************************************
 * fileinfo.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: fileinfo.cpp,v 1.7 2003/04/06 13:18:26 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/treectrl.h>

#include <vlc/intf.h>

#include "wxwindows.h"

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Close_Event
};

BEGIN_EVENT_TABLE(FileInfo, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, FileInfo::OnClose)

    /* Destroy the window when the user closes the window */
    EVT_CLOSE(FileInfo::OnClose)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
FileInfo::FileInfo( intf_thread_t *_p_intf, Interface *_p_main_interface ):
    wxFrame( _p_main_interface, -1, "FileInfo", wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    intf_thread_t *p_intf = _p_intf;
    input_thread_t *p_input = p_intf->p_sys->p_input;
    SetIcon( *p_intf->p_sys->p_icon );
    SetAutoLayout(TRUE);

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    wxTreeCtrl *tree =
        new wxTreeCtrl( panel, -1, wxDefaultPosition, wxSize(350,350),
                        wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxSUNKEN_BORDER );

    /* Create the OK button */
    wxButton *ok_button = new wxButton( panel, wxID_OK, _("OK") );
    ok_button->SetDefault();

    /* Place everything in sizers */
    wxBoxSizer *ok_button_sizer = new wxBoxSizer( wxHORIZONTAL );
    ok_button_sizer->Add( ok_button, 0, wxALL, 5 );
    ok_button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( tree, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( ok_button_sizer, 0, wxALIGN_CENTRE );
    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxEXPAND, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

    if ( !p_intf->p_sys->p_input )
    {
        /* Nothing to show, but hey... */
        Show( true );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    wxTreeItemId root = tree->AddRoot( p_input->psz_name );
    input_info_category_t *p_cat = p_input->stream.p_info;
    
    while ( p_cat ) {
        wxTreeItemId cat = tree->AppendItem( root, p_cat->psz_name );
        input_info_t *p_info = p_cat->p_info;
        while ( p_info ) {
            tree->AppendItem( cat, wxString(p_info->psz_name) + ": "
                              + p_info->psz_value );
            p_info = p_info->p_next;
        }
        p_cat = p_cat->p_next;
        tree->Expand( cat );
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    Show( true );
}

FileInfo::~FileInfo()
{
}

void FileInfo::OnClose( wxCommandEvent& event )
{
    Destroy();
}
