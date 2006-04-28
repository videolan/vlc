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

    /* CtrlList events */
    EVT_LIST_ITEM_ACTIVATED( ChooseItem_Event, UpdateVLC::OnChooseItem )

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

    p_u = update_New( p_intf );
}


UpdateVLC::~UpdateVLC()
{
    update_Delete( p_u );
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
    update_Check( p_u, VLC_FALSE );
    update_iterator_t *p_uit = update_iterator_New( p_u );
    if( p_uit )
    {
        wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );

        p_uit->i_rs = UPDATE_RELEASE_STATUS_NEWER;
        p_uit->i_t = UPDATE_FILE_TYPE_ALL;
        update_iterator_Action( p_uit, UPDATE_MIRROR );

        DestroyChildren();

        wxListCtrl *list =
            new wxListCtrl( this, ChooseItem_Event,
                            wxDefaultPosition, wxSize( 400, 300 ),
                            wxLC_SINGLE_SEL|wxLC_LIST );
        wxImageList *images = new wxImageList( 32, 32, TRUE );
        images->Add( wxIcon( update_ascii_xpm ) );
        images->Add( wxIcon( update_info_xpm ) );
        images->Add( wxIcon( update_source_xpm ) );
        images->Add( wxIcon( update_binary_xpm ) );
        images->Add( wxIcon( update_document_xpm ) );
        list->AssignImageList( images, wxIMAGE_LIST_SMALL );
        while( update_iterator_Action( p_uit, UPDATE_FILE ) != UPDATE_FAIL )
        {
            int i_image;
            switch( p_uit->file.i_type )
            {
                case UPDATE_FILE_TYPE_INFO:
                    i_image = 1;
                    break;
                case UPDATE_FILE_TYPE_SOURCE:
                    i_image = 2;
                    break;
                case UPDATE_FILE_TYPE_BINARY:
                    i_image = 3;
                    break;
                case UPDATE_FILE_TYPE_PLUGIN:
                    i_image = 4;
                    break;
                default:
                    i_image = 0;
            }
            char *psz_tmp = NULL;
            if( p_uit->file.l_size )
            {
                if( p_uit->file.l_size > 1024 * 1024 * 1024 )
                     asprintf( &psz_tmp, "(%ld GB)",
                                p_uit->file.l_size / (1024*1024*1024) );
                if( p_uit->file.l_size > 1024 * 1024 )
                    asprintf( &psz_tmp, "(%ld MB)",
                                p_uit->file.l_size / (1024*1024) );
                else if( p_uit->file.l_size > 1024 )
                    asprintf( &psz_tmp, "(%ld kB)",
                                p_uit->file.l_size / 1024 );
                else
                    asprintf( &psz_tmp, "(%ld B)", p_uit->file.l_size );
            }
            list->InsertItem( list->GetItemCount(),
                              wxU(p_uit->file.psz_description)+wxU("\n")
                              + wxU(p_uit->release.psz_version)+wxU(" ")
                              + wxU(psz_tmp),
                              i_image );
            if( psz_tmp ) free( psz_tmp );
        }

        main_sizer->Add( new wxStaticText( this, -1, wxU( _("\nAvailable " 
                "updates and related downloads.\n"
                "(Double click on a file to download it)\n" ) ) ) );
        main_sizer->Add( list );
        SetSizerAndFit( main_sizer );
        Layout();
        update_iterator_Delete( p_uit );
    }
}

void UpdateVLC::OnChooseItem( wxListEvent& event )
{
    update_iterator_t *p_uit = update_iterator_New( p_u );
    if( p_uit )
    {
        p_uit->i_rs = UPDATE_RELEASE_STATUS_NEWER;
        p_uit->i_t = UPDATE_FILE_TYPE_ALL;
        update_iterator_Action( p_uit, UPDATE_MIRROR );

        int i_count = 0;
        while( update_iterator_Action( p_uit, UPDATE_FILE ) != UPDATE_FAIL )
        {
            if( i_count == event.GetIndex() )
                break;
            i_count++;
        }
        wxString url = wxU( p_uit->file.psz_url );
        wxFileDialog *filedialog =
                    new wxFileDialog( this, wxU(_("Save file...")),
                        wxT(""), url.AfterLast( '/' ), wxT("*.*"),
                        wxSAVE | wxOVERWRITE_PROMPT );
        if( filedialog->ShowModal() == wxID_OK )
        {
            update_download( p_uit, filedialog->GetPath().mb_str() );
        }
        update_iterator_Delete( p_uit );
        delete filedialog;
    }
}
