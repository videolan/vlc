/*****************************************************************************
 * interaction.cpp: wxWidgets handling of interaction dialogs
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id: bookmarks.cpp 13106 2005-11-02 19:20:34Z zorglub $
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
#include "dialogs/interaction.hpp"

#include <wx/statline.h>

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
     OkYes_Event,
     No_Event,
     Cancel_Event
};

BEGIN_EVENT_TABLE( InteractionDialog, wxFrame)
    EVT_CLOSE( InteractionDialog::OnClose )
    EVT_BUTTON( OkYes_Event, InteractionDialog::OnOkYes )
    EVT_BUTTON( Cancel_Event, InteractionDialog::OnCancel)
    EVT_BUTTON( No_Event, InteractionDialog::OnNo )
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
InteractionDialog::InteractionDialog( intf_thread_t *_p_intf,
                                      wxWindow *p_parent,
                                      interaction_dialog_t *_p_dialog )
  : wxFrame( p_parent, -1, wxU( _p_dialog->psz_title ) )
{
    /* Initializations */
    p_intf = _p_intf;
    p_dialog = _p_dialog;
    SetIcon( *p_intf->p_sys->p_icon );

    widgets_panel = new wxPanel( this, -1 );
    widgets_sizer = new wxBoxSizer( wxVERTICAL );
    widgets_panel->SetSizer( widgets_sizer );

    buttons_panel = new wxPanel( this, -1 );
    buttons_sizer = new wxBoxSizer( wxHORIZONTAL );
    buttons_panel->SetSizer( buttons_sizer );

    main_sizer = new wxBoxSizer( wxVERTICAL );
    main_sizer->Add( widgets_panel, 0, wxEXPAND | wxALL, 5 );
    main_sizer->Add( new wxStaticLine( this, -1 ), 0, wxEXPAND  );
    main_sizer->Add( buttons_panel, 0, wxEXPAND | wxALL, 5 );
    SetSizer( main_sizer );

    Render();
}

InteractionDialog::~InteractionDialog()
{
}

void InteractionDialog::Update( )
{
    widgets_panel->DestroyChildren();
    buttons_panel->DestroyChildren();
    input_widgets.clear();
    Render();
    Show();
}

/// \todo Dirty - Clean that up
void InteractionDialog::Render()
{
    wxStaticText *label;
    wxTextCtrl *input;

    //-------------- Widgets ------------------
    for( int i = 0 ; i< p_dialog->i_widgets; i++ )
    {
        user_widget_t* p_widget = p_dialog->pp_widgets[i];
        /// \todo Merge cleanly with preferences widgets
        switch( p_widget->i_type )
        {
        case WIDGET_TEXT:
            label = new wxStaticText( widgets_panel, -1,
                                      wxU( p_widget->psz_text ) );
            widgets_sizer->Add( label );
            break;
        case WIDGET_INPUT_TEXT:
            label = new wxStaticText( widgets_panel, -1,
                                      wxU( p_widget->psz_text ) );
            input = new wxTextCtrl( widgets_panel, -1 );
            widgets_sizer->Add( label );
            widgets_sizer->Add( input );

            InputWidget widget;
            widget.control = input;
            widget.val = &p_widget->val;
            widget.i_type = WIDGET_INPUT_TEXT;
            input_widgets.push_back( widget );
        }
    }
    widgets_sizer->Layout();
    widgets_panel->SetSizerAndFit( widgets_sizer );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

    //-------------- Buttons ------------------
    if( p_dialog->i_flags & DIALOG_OK_CANCEL )
    {
        wxButton *ok = new wxButton( buttons_panel,
                                     OkYes_Event, wxU( _("OK") ) );
        wxButton *cancel = new wxButton( buttons_panel,
                                         Cancel_Event, wxU( _("Cancel") ) );
        buttons_sizer->Add( ok, 0, wxEXPAND | wxRIGHT| wxLEFT, 5 );
        buttons_sizer->Add( cancel, 0, wxEXPAND | wxRIGHT| wxLEFT, 5 );
    }

}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void InteractionDialog::OnClose( wxCloseEvent& event )
{
    Finish( DIALOG_CANCELLED );
}

void InteractionDialog::OnCancel( wxCommandEvent& event )
{
    Finish( DIALOG_CANCELLED );
}

void InteractionDialog::OnNo( wxCommandEvent& event )
{
    Finish( DIALOG_NO );
}

void InteractionDialog::OnOkYes( wxCommandEvent& event )
{
    Finish( DIALOG_OK_YES );
}

void InteractionDialog::Finish( int i_ret )
{
    vector<InputWidget>::iterator it = input_widgets.begin();
    while ( it < input_widgets.end() )
    {
        if( (*it).i_type == WIDGET_INPUT_TEXT )
            (*it).val->psz_string = strdup( (*it).control->GetValue().mb_str() );
        it++;
    }
    Hide();
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    p_dialog->i_status = ANSWERED_DIALOG;
    p_dialog->i_return = i_ret;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
}
