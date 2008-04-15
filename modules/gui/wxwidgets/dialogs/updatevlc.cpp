/*****************************************************************************
 * updatevlc.cpp : VLC Update checker
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "updatevlc.hpp"

#ifdef UPDATE_CHECK
#include <wx/imaglist.h>

#include "bitmaps/update_ascii.xpm"
#include "bitmaps/update_binary.xpm"
#include "bitmaps/update_document.xpm"
#include "bitmaps/update_info.xpm"
#include "bitmaps/update_source.xpm"

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Close_Event,
    CheckForUpdate_Event,
    ChooseItem_Event
};

BEGIN_EVENT_TABLE(UpdateVLC, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, UpdateVLC::OnButtonClose)
    EVT_BUTTON(CheckForUpdate_Event, UpdateVLC::OnCheckForUpdate)

    /* Hide the window when the user closes the window */
    EVT_CLOSE(UpdateVLC::OnClose)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
UpdateVLC::UpdateVLC( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Updates")),
             wxDefaultPosition, wxDefaultSize,
             wxSYSTEM_MENU|wxCLOSE_BOX|wxFRAME_FLOAT_ON_PARENT
             |wxFRAME_TOOL_WINDOW|wxCAPTION )
{
    /* Initializations */
    p_intf = _p_intf;
    SetIcon( *p_intf->p_sys->p_icon );
    SetAutoLayout( TRUE );

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxButton *update_button =
        new wxButton( this, CheckForUpdate_Event,
                      wxU(_("Check for updates")) );
    main_sizer->Add( update_button );
    SetSizerAndFit( main_sizer );

    p_update = update_New( p_intf );
}


UpdateVLC::~UpdateVLC()
{
    update_Delete( p_update );
}

void UpdateVLC::OnButtonClose( wxCommandEvent& event )
{
    wxCloseEvent cevent;
    OnClose(cevent);
}

void UpdateVLC::OnClose( wxCloseEvent& WXUNUSED(event) )
{
    Hide();
}

void UpdateVLC::OnCheckForUpdate( wxCommandEvent& event )
{
    update_Check( p_update, NULL, this );
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );

    DestroyChildren();

    /*list->InsertItem( list->GetItemCount(),
                      wxU(p_uit->file.psz_description)+wxU("\n")
                      + wxU(p_uit->release.psz_version)+wxU(" ")
                      + wxU(psz_tmp),
                      i_image );*/

    if( update_NeedUpgrade( p_update ) )
        main_sizer->Add( new wxStaticText( this, -1, wxU( p_update->release.psz_desc )
                         + wxU( "\nYou can download the latest version of VLC at the adress :\n" )
                         + wxU( p_update->release.psz_url ) ) );
    else
        main_sizer->Add( new wxStaticText( this, -1,
                         wxU( _( "\nYou have the latest version of VLC\n" ) ) ) );

    SetSizerAndFit( main_sizer );
    Layout();
}
#endif
