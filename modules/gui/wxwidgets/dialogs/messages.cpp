/*****************************************************************************
 * messages.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "dialogs/messages.hpp"

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Close_Event,
    Clear_Event,
    Save_Log_Event
};

BEGIN_EVENT_TABLE(Messages, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_CLOSE, Messages::OnButtonClose)
    EVT_BUTTON(wxID_CLEAR, Messages::OnClear)
    EVT_BUTTON(wxID_SAVEAS, Messages::OnSaveLog)

    /* Special events : we don't want to destroy the window when the user
     * clicks on (X) */
    EVT_CLOSE(Messages::OnClose)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Messages::Messages( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Messages")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    b_verbose = VLC_FALSE;
    SetIcon( *p_intf->p_sys->p_icon );
    save_log_dialog = NULL;
    b_verbose = VLC_FALSE;

    /* Create a panel to put everything in */
    wxPanel *messages_panel = new wxPanel( this, -1 );
    messages_panel->SetAutoLayout( TRUE );

    /* Create the textctrl and some text attributes */
    textctrl = new wxTextCtrl( messages_panel, -1, wxT(""), wxDefaultPosition,
        wxSize::wxSize( 400, 500 ), wxTE_MULTILINE | wxTE_READONLY |
                                    wxTE_RICH | wxTE_NOHIDESEL );
    info_attr = new wxTextAttr( wxColour::wxColour( 0, 128, 0 ) );
    err_attr = new wxTextAttr( *wxRED );
    warn_attr = new wxTextAttr( *wxBLUE );
    dbg_attr = new wxTextAttr( *wxBLACK );



    /* Create the Save Log button */
    wxButton *save_log_button = new wxButton( messages_panel, wxID_SAVEAS );
    save_log_button->SetDefault();

    /* Create the Clear button */
    wxButton *clear_button = new wxButton( messages_panel, wxID_CLEAR );
    clear_button->SetDefault();

    /* Create the OK button */
    wxButton *ok_button = new wxButton( messages_panel, wxID_CLOSE);
    ok_button->SetDefault();

    /* Place everything in sizers */
    wxBoxSizer *buttons_sizer = new wxBoxSizer( wxHORIZONTAL );
    buttons_sizer->Add( save_log_button, 0, wxEXPAND |wxALIGN_LEFT| wxALL, 5 );
    buttons_sizer->Add( clear_button, 0, wxEXPAND |wxALIGN_RIGHT| wxALL, 5 );
    buttons_sizer->Add( ok_button, 0, wxEXPAND |wxALIGN_RIGHT| wxALL, 5 );
    buttons_sizer->Add( new wxPanel( this, -1 ), 1, wxALL, 5 );
    buttons_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( textctrl, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( buttons_sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();
    messages_panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( messages_panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

Messages::~Messages()
{
    /* Clean up */
    if( save_log_dialog ) delete save_log_dialog;

    delete info_attr;
    delete err_attr;
    delete warn_attr;
    delete dbg_attr;
}

bool Messages::Show( bool show )
{
    b_verbose = show;
    return wxFrame::Show( show );
}

void Messages::UpdateLog()
{
    msg_subscription_t *p_sub = p_intf->p_sys->p_sub;
    int i_start;

    vlc_mutex_lock( p_sub->p_lock );
    int i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );


    if( p_sub->i_start != i_stop )
    {
        textctrl->SetInsertionPointEnd();

        for( i_start = p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {

            if( !b_verbose &&
                VLC_MSG_ERR != p_sub->p_msg[i_start].i_type )
                continue;

            /* Append all messages to log window */
            textctrl->SetDefaultStyle( *dbg_attr );
            (*textctrl) << wxL2U(p_sub->p_msg[i_start].psz_module);

            switch( p_sub->p_msg[i_start].i_type )
            {
            case VLC_MSG_INFO:
                (*textctrl) << wxT(": ");
                textctrl->SetDefaultStyle( *info_attr );
                break;
            case VLC_MSG_ERR:
                (*textctrl) << wxT(" error: ");
                textctrl->SetDefaultStyle( *err_attr );
                break;
            case VLC_MSG_WARN:
                (*textctrl) << wxT(" warning: ");
                textctrl->SetDefaultStyle( *warn_attr );
                break;
            case VLC_MSG_DBG:
            default:
                (*textctrl) << wxT(" debug: ");
                break;
            }

            /* Add message */
            (*textctrl) << wxL2U(p_sub->p_msg[i_start].psz_msg) << wxT("\n");
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Messages::OnButtonClose( wxCommandEvent& WXUNUSED(event) )
{
    wxCloseEvent cevent;
    OnClose(cevent);
}

void Messages::OnClose( wxCloseEvent& WXUNUSED(event) )
{
    Hide();
}

void Messages::OnClear( wxCommandEvent& WXUNUSED(event) )
{
    textctrl->Clear();
}

void Messages::OnSaveLog( wxCommandEvent& WXUNUSED(event) )
{
    if( save_log_dialog == NULL )
        save_log_dialog = new wxFileDialog( this,
            wxU(_("Save Messages As...")),
            wxT(""), wxT("messages"), wxT("*"), wxSAVE | wxOVERWRITE_PROMPT );

    if( save_log_dialog && save_log_dialog->ShowModal() == wxID_OK )
    {
        if( !textctrl->SaveFile( save_log_dialog->GetPath() ) )
        {
            // [FIX ME] should print an error message
        }
    }
}
