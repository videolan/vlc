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

#define FREE( i ) { if( i ) free( i ); i = NULL; }

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
     No_Event,
     NoShow_Event
};

BEGIN_EVENT_TABLE( InteractionDialog, wxFrame )
    EVT_CLOSE( InteractionDialog::OnClose )
    EVT_BUTTON( wxID_OK, InteractionDialog::OnOkYes )
    EVT_BUTTON( wxID_YES, InteractionDialog::OnOkYes )
    EVT_BUTTON( wxID_CANCEL, InteractionDialog::OnCancel)
    EVT_BUTTON( No_Event, InteractionDialog::OnNo )
    EVT_BUTTON( wxID_CLEAR, InteractionDialog::OnClear )
    EVT_CHECKBOX( NoShow_Event, InteractionDialog::OnNoShow )
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
    main_sizer->Add( widgets_panel, 1, wxEXPAND | wxALL, 5 );
    main_sizer->Add( new wxStaticLine( this, -1 ), 0, wxEXPAND  );
    main_sizer->Add( buttons_panel, 0, wxEXPAND | wxALL, 5 );
    SetSizer( main_sizer );

    b_noshow = false;
    Render();
}

InteractionDialog::~InteractionDialog()
{
}

void InteractionDialog::Update( )
{
    widgets_panel->DestroyChildren();
    /* FIXME: Needed for the spacer */
    buttons_sizer->Remove( 1 );buttons_sizer->Remove( 2 );buttons_sizer->Remove( 3 );
    buttons_panel->DestroyChildren();
    input_widgets.clear();
    Render();
    if( b_noshow == false )
    {
        Show();
    }
}

/// \todo Dirty - Clean that up
void InteractionDialog::Render()
{
    wxStaticText *label;
    wxTextCtrl   *input;
    wxGauge      *gauge;


    if( p_dialog->i_id == DIALOG_ERRORS )
    {
        wxTextCtrl *errors ; // Special case
        label = new wxStaticText( widgets_panel, -1,
          wxU( _("The following errors happened. More details might be "
                 "available in the Messages window.") ) );
        errors = new wxTextCtrl( widgets_panel, -1, wxT(""),
                         wxDefaultPosition, wxDefaultSize,
                         wxTE_MULTILINE | wxTE_READONLY );
        for( int i = 0 ; i< p_dialog->i_widgets; i++ )
        {
            (*errors) << wxL2U( p_dialog->pp_widgets[i]->psz_text ) <<
                           wxU( "\n" ) ;
        }
        widgets_sizer->Add( label );
        widgets_sizer->Add( errors, 0, wxEXPAND|wxALL, 3 );
    }
    else
    {
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
                widgets_sizer->Add( label , 0, 0, 0);
                widgets_sizer->Add( input, 0, wxEXPAND , 0 );

                InputWidget widget;
                widget.control = input;
                widget.val = &p_widget->val;
                widget.i_type = WIDGET_INPUT_TEXT;
                input_widgets.push_back( widget );
            case WIDGET_PROGRESS:
                label = new wxStaticText(widgets_panel, -1,
                                    wxU( p_widget->psz_text ) );
                gauge = new wxGauge( widgets_panel, -1, 100,
                                     wxDefaultPosition, wxDefaultSize );
                widgets_sizer->Add( label , 0, 0, 0);
                widgets_sizer->Add( gauge, 0, wxEXPAND , 0 );
                gauge->SetValue( (int)(p_widget->val.f_float ) );
            }
        }
    }

    //-------------- Buttons ------------------
    if( p_dialog->i_flags & DIALOG_OK_CANCEL )
    {
        wxButton *ok = new wxButton( buttons_panel,
                                     wxID_OK, wxU( _("OK") ) );
        wxButton *cancel = new wxButton( buttons_panel,
                                         wxID_CANCEL, wxU( _("Cancel") ) );
        buttons_sizer->Add( ok, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_CENTER, 5 );
        buttons_sizer->Add( cancel, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_CENTER, 5 );
    }
    else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
    {
        wxButton *yes = new wxButton( buttons_panel,
                                      wxID_YES, wxU( _("Yes") ) );
        wxButton *no = new wxButton( buttons_panel,
                                     wxID_NO, wxU( _("No") ) );
        wxButton *cancel = new wxButton( buttons_panel,
                                         wxID_CANCEL, wxU( _("Cancel") ) );
        buttons_sizer->Add( yes, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_CENTER, 5 );
        buttons_sizer->Add( no, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_CENTER, 5 );
        buttons_sizer->Add( cancel, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_CENTER, 5 );
    }
    else if( p_dialog->i_flags & DIALOG_CLEAR_NOSHOW )
    {
        wxCheckBox *noshow = new wxCheckBox( buttons_panel,
                                         NoShow_Event, wxU( _("Don't show") ) );
        noshow->SetValue( b_noshow );
        wxButton *clear = new wxButton( buttons_panel,
                                        wxID_CLEAR, wxU( _("Clear") ) );
        buttons_sizer->Add( noshow, 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_LEFT, 5 );
        buttons_sizer->Add( 0, 0, 1 );
        buttons_sizer->Add( clear , 0, wxEXPAND | wxRIGHT| wxLEFT | wxALIGN_RIGHT, 5 );

    }
    widgets_sizer->Layout();
    widgets_panel->SetSizerAndFit( widgets_sizer );
    buttons_sizer->Layout();
    buttons_panel->SetSizerAndFit( buttons_sizer );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
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

void InteractionDialog::OnClear( wxCommandEvent& event )
{
    int i;
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    for( i = p_dialog->i_widgets - 1 ; i >= 0 ; i-- )
    {
        user_widget_t *p_widget = p_dialog->pp_widgets[i];
        FREE( p_widget->psz_text );
        FREE( p_widget->val.psz_string );
        REMOVE_ELEM( p_dialog->pp_widgets, p_dialog->i_widgets, i );
        free( p_widget );
    }
    widgets_panel->DestroyChildren();
    /* FIXME: Needed for the spacer */
    buttons_sizer->Remove( 1 );buttons_sizer->Remove( 2 );buttons_sizer->Remove( 3 );
    buttons_panel->DestroyChildren();
    input_widgets.clear();
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
    Render();
}

void InteractionDialog::OnNoShow( wxCommandEvent& event )
{
     b_noshow = event.IsChecked();
}

void InteractionDialog::Finish( int i_ret )
{
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    vector<InputWidget>::iterator it = input_widgets.begin();
    while ( it < input_widgets.end() )
    {
        if( (*it).i_type == WIDGET_INPUT_TEXT )
            (*it).val->psz_string = strdup( (*it).control->GetValue().mb_str() );
        it++;
    }
    Hide();
    p_dialog->i_status = ANSWERED_DIALOG;
    p_dialog->i_return = i_ret;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
}

#undef FREE
