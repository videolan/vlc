/*****************************************************************************
 * stream.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id: streamwizard.cpp,v 1.6 2004/01/29 17:51:08 zorglub Exp $
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
#include <vlc/intf.h>

#include "wxwindows.h"

#include <wx/statline.h>


#define STREAM_INTRO N_( "Stream with VLC in three steps." )
#define STREAM_STEP1 N_( "Step 1: Select what to stream." )
#define STREAM_STEP2 N_( "Step 2: Define streaming method." )
#define STREAM_STEP3 N_( "Step 3: Start streaming." )


/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Open_Event,
    Sout_Event,
    Start_Event,
    Close_Event
};

BEGIN_EVENT_TABLE(StreamDialog, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, StreamDialog::OnClose)

    EVT_BUTTON(Open_Event,StreamDialog::OnOpen)
    EVT_BUTTON(Sout_Event,StreamDialog::OnSout)
    EVT_BUTTON(Start_Event,StreamDialog::OnStart)

    /* Hide the window when the user closes the window */
    EVT_CLOSE(StreamDialog::OnClose)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
StreamDialog::StreamDialog( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Stream")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    SetIcon( *p_intf->p_sys->p_icon );
    SetAutoLayout( TRUE );

    p_open_dialog = NULL;
    p_sout_dialog = NULL;

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    wxStaticText *intro_label = new wxStaticText( panel,
                          -1 , wxU(_( STREAM_INTRO )));


    wxStaticText *step1_label = new wxStaticText( panel,
                           -1 , wxU(_( STREAM_STEP1 )));

    step2_label = new wxStaticText( panel,
                 -1 , wxU(_( STREAM_STEP2 )));

    step3_label = new wxStaticText( panel,
                 -1 , wxU(_( STREAM_STEP3 )));

    wxButton *open_button = new wxButton( panel,
                  Open_Event, wxU(_("Open...")));

    sout_button = new wxButton( panel,
                   Sout_Event, wxU(_("Choose...")));

    start_button = new wxButton( panel,
                   Start_Event, wxU(_("Start!")));


    step2_label->Disable();
    step3_label->Disable();

    sout_button->Disable();
    start_button->Disable();

    /* Place everything in sizers */
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer *step1_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxBoxSizer *step2_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxBoxSizer *step3_sizer = new wxBoxSizer( wxHORIZONTAL );

    step1_sizer->Add( step1_label, 1, wxALL | wxEXPAND | wxALIGN_LEFT, 10 );
    step1_sizer->Add( open_button, 1, wxALL | wxEXPAND | wxALIGN_RIGHT, 10 );

    step2_sizer->Add( step2_label, 1, wxALL | wxEXPAND | wxALIGN_LEFT, 10 );
    step2_sizer->Add( sout_button, 1, wxALL | wxEXPAND | wxALIGN_RIGHT, 10 );

    step3_sizer->Add( step3_label,  1, wxALL | wxEXPAND | wxLEFT, 10 );
    step3_sizer->Add( start_button, 1, wxALL | wxEXPAND | wxLEFT, 10 );

    panel_sizer->Add( intro_label, 0, wxEXPAND | wxALL, 10 );

    panel_sizer->Add( new wxStaticLine( panel, 0), 0,
                      wxEXPAND | wxLEFT | wxRIGHT, 2 );
    panel_sizer->Add( step1_sizer, 0, wxEXPAND, 10 );
    panel_sizer->Add( new wxStaticLine( panel, 0), 0,
                      wxEXPAND | wxLEFT | wxRIGHT, 2 );
    panel_sizer->Add( step2_sizer, 0, wxEXPAND, 10 );
    panel_sizer->Add( new wxStaticLine( panel, 0), 0,
                      wxEXPAND | wxLEFT | wxRIGHT, 2 );
    panel_sizer->Add( step3_sizer, 0, wxEXPAND, 10 );

    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxEXPAND, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

}

/*****************************************************************************
 * Destructor.
 *****************************************************************************/
StreamDialog::~StreamDialog()
{
    if( p_open_dialog ) delete p_open_dialog;
    if( p_sout_dialog ) delete p_sout_dialog;
}

void StreamDialog::OnOpen( wxCommandEvent& event )
{
    if( !p_open_dialog )
    {
        p_open_dialog = new OpenDialog(
                    p_intf, this, FILE_ACCESS, 1 , OPEN_STREAM );
    }

    if( p_open_dialog)
    {
       p_open_dialog->Show();
       mrl = p_open_dialog->mrl;
       sout_button->Enable();
       step2_label->Enable();
    }
}

void StreamDialog::OnSout( wxCommandEvent& event )
{
    /* Show/hide the sout dialog */
    if( p_sout_dialog == NULL )
        p_sout_dialog = new SoutDialog( p_intf, this );

    if( p_sout_dialog && p_sout_dialog->ShowModal() == wxID_OK )
    {
        sout_mrl = p_sout_dialog->GetOptions();
        start_button->Enable();
        step3_label->Enable();
    }
}

void StreamDialog::OnStart( wxCommandEvent& event )
{
    /* Update the playlist */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    for( int i = 0; i < (int)p_open_dialog->mrl.GetCount(); i++ )
    {
        playlist_item_t *p_item = playlist_ItemNew( p_intf,
                      (const char *)p_open_dialog->mrl[i].mb_str(),
                      (const char *)p_open_dialog->mrl[i].mb_str() );
        int i_options = 0;

        /* Count the input options */
        while( i + i_options + 1 < (int)p_open_dialog->mrl.GetCount() &&
               ((const char *)p_open_dialog->mrl[i + i_options + 1].
                                             mb_str())[0] == ':' )
        {
            i_options++;
        }

        /* Insert options */
        for( int j = 0; j < i_options; j++ )
        {
            playlist_ItemAddOption( p_item ,
                                p_open_dialog->mrl[i + j  + 1].mb_str() );
        }

        /* Get the options from the stream output dialog */
        if( sout_mrl.GetCount() )
        {
            for( int j = 0; j < (int)sout_mrl.GetCount(); j++ )
            {
                playlist_ItemAddOption( p_item , sout_mrl[j].mb_str() );
            }
        }

         playlist_AddItem( p_playlist, p_item,
                      PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO), PLAYLIST_END );

        msg_Dbg(p_intf,"playings %s",
                 (const char *)p_open_dialog->mrl[i].mb_str());

        i += i_options;
    }
    vlc_object_release( p_playlist );

    Hide();
}


void StreamDialog::OnClose( wxCommandEvent& event )
{
    Hide();
}
