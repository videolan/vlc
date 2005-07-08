/*****************************************************************************
 * subtitles.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
    FileBrowse_Event = wxID_HIGHEST,
};

BEGIN_EVENT_TABLE(SubsFileDialog, wxDialog)
    /* Button events */
    EVT_BUTTON(wxID_OK, SubsFileDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, SubsFileDialog::OnCancel)
    EVT_BUTTON(FileBrowse_Event, SubsFileDialog::OnFileBrowse)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
SubsFileDialog::SubsFileDialog( intf_thread_t *_p_intf, wxWindow* _p_parent ):
    wxDialog( _p_parent, -1, wxU(_("Subtitle options")),
              wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    /* Create the subtitles file textctrl */
    wxBoxSizer *file_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *file_box = new wxStaticBox( panel, -1,
                                             wxU(_("Subtitles file")) );
    wxStaticBoxSizer *file_sizer = new wxStaticBoxSizer( file_box,
                                                        wxHORIZONTAL );
    char *psz_subsfile = config_GetPsz( p_intf, "sub-file" );
    if( !psz_subsfile ) psz_subsfile = strdup("");
    file_combo = new wxComboBox( panel, -1, wxL2U(psz_subsfile),
                                 wxPoint(20,25), wxSize(300, -1) );
    if( psz_subsfile ) free( psz_subsfile );
    wxButton *browse_button = new wxButton( panel, FileBrowse_Event,
                                            wxU(_("Browse...")) );
    file_sizer->Add( file_combo, 1, wxALL, 5 );
    file_sizer->Add( browse_button, 0, wxALL, 5 );
    file_sizer_sizer->Add( file_sizer, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( file_sizer, 0, wxEXPAND | wxALL, 5 );

    /* Subtitles encoding */
    encoding_combo = NULL;
    module_config_t *p_item =
        config_FindConfig( VLC_OBJECT(p_intf), "subsdec-encoding" );
    if( p_item )
    {
        wxBoxSizer *enc_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
        wxStaticBox *enc_box = new wxStaticBox( panel, -1,
                                                wxU(_("Subtitles encoding")) );
        wxStaticBoxSizer *enc_sizer = new wxStaticBoxSizer( enc_box,
                                                            wxHORIZONTAL );
        wxStaticText *label =
            new wxStaticText(panel, -1, wxU(p_item->psz_text));
        encoding_combo = new wxComboBox( panel, -1, wxU(p_item->psz_value),
                                         wxDefaultPosition, wxDefaultSize,
                                         0, NULL, wxCB_READONLY );

        /* build a list of available options */
        for( int i_index = 0; p_item->ppsz_list && p_item->ppsz_list[i_index];
             i_index++ )
        {
            encoding_combo->Append( wxU(p_item->ppsz_list[i_index]) );
            if( p_item->psz_value && !strcmp( p_item->psz_value,
                                              p_item->ppsz_list[i_index] ) )
                encoding_combo->SetSelection( i_index );
        }

        if( p_item->psz_value )
        encoding_combo->SetValue( wxU(p_item->psz_value) );
        encoding_combo->SetToolTip( wxU(p_item->psz_longtext) );

        enc_sizer->Add( label, 0, wxALL | wxALIGN_CENTER, 5 );
        enc_sizer->Add( encoding_combo, 0, wxEXPAND | wxALL | wxALIGN_CENTER, 5 );
        enc_sizer_sizer->Add( enc_sizer, 1, wxEXPAND | wxALL, 5 );
        panel_sizer->Add( enc_sizer, 0, wxEXPAND | wxALL, 5 );
    }

    /* Misc Subtitles options */
    wxBoxSizer *misc_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *misc_box = new wxStaticBox( panel, -1,
                                             wxU(_("Subtitles options")) );

    wxStaticBoxSizer *misc_sizer = new wxStaticBoxSizer( misc_box,
                                                         wxVERTICAL );

    wxFlexGridSizer *grid_sizer = new wxFlexGridSizer( 2, 1, 20 );

    /* Font size */
    p_item =
        config_FindConfig( VLC_OBJECT(p_intf), "freetype-rel-fontsize" );
    if( p_item )
    {
        wxBoxSizer *size_sizer = new wxBoxSizer( wxHORIZONTAL );
        wxStaticText *label =
            new wxStaticText(panel, -1, wxU(p_item->psz_text));
        size_combo = new wxComboBox( panel, -1, wxT(""),
                                     wxDefaultPosition, wxDefaultSize,
                                     0, NULL, wxCB_READONLY );

        /* build a list of available options */
        for( int i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            size_combo->Append( wxU(p_item->ppsz_list_text[i_index]),
                                (void *)p_item->pi_list[i_index] );
            if( p_item->i_value == p_item->pi_list[i_index] )
            {
                size_combo->SetSelection( i_index );
                size_combo->SetValue(wxU(p_item->ppsz_list_text[i_index]));
            }
        }

        size_combo->SetToolTip( wxU(p_item->psz_longtext) );

        size_sizer->Add( label, 0,  wxRIGHT | wxALIGN_CENTER, 5 );
        size_sizer->Add( size_combo, 0, wxLEFT | wxALIGN_CENTER, 5 );
        grid_sizer->Add( size_sizer, 1, wxEXPAND | wxALL, 5 );
    }

    p_item =
        config_FindConfig( VLC_OBJECT(p_intf), "subsdec-align" );
    if( p_item )
    {
        wxBoxSizer *align_sizer = new wxBoxSizer( wxHORIZONTAL );
        wxStaticText *label =
            new wxStaticText(panel, -1, wxU(p_item->psz_text));
        align_combo = new wxComboBox( panel, -1, wxT(""),
                                     wxDefaultPosition, wxDefaultSize,
                                     0, NULL, wxCB_READONLY );

        /* build a list of available options */
        for( int i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            align_combo->Append( wxU(p_item->ppsz_list_text[i_index]),
                                (void *)p_item->pi_list[i_index] );
            if( p_item->i_value == p_item->pi_list[i_index] )
            {
                align_combo->SetSelection( i_index );
                align_combo->SetValue(wxU(p_item->ppsz_list_text[i_index]));
            }
        }

        align_combo->SetToolTip( wxU(p_item->psz_longtext) );

        align_sizer->Add( label, 0,  wxRIGHT | wxALIGN_CENTER, 5 );
        align_sizer->Add( align_combo, 0, wxLEFT | wxALIGN_CENTER, 5 );
        grid_sizer->Add( align_sizer, 1, wxEXPAND | wxALL, 5 );
    }

    misc_sizer->Add( grid_sizer, 1, wxEXPAND | wxALL , 5 );

    grid_sizer = new wxFlexGridSizer( 4, 1, 20 );

    wxStaticText *label =
        new wxStaticText(panel, -1, wxU(_("Frames per second")));

    float f_fps = config_GetFloat( p_intf, "sub-fps" );
    /* Outside the new wxSpinCtrl to avoid an internal error in gcc2.95 ! */
    wxString format_fps(wxString::Format(wxT("%d"),(int)f_fps));
    fps_spinctrl = new wxSpinCtrl( panel, -1, format_fps,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, (int)f_fps );
    fps_spinctrl->SetToolTip( wxU(_("Override frames per second. "
               "It will only work with MicroDVD and SubRIP subtitles.")) );
    grid_sizer->Add( label, 0, wxALIGN_CENTER, 5 );
    grid_sizer->Add( fps_spinctrl, 0,wxALIGN_CENTER, 5 );


    wxStaticText *label_delay =
        new wxStaticText(panel, -1, wxU(_("Delay")));

    int i_delay = config_GetInt( p_intf, "sub-delay" );
    /* Outside the new wxSpinCtrl to avoid an internal error in gcc2.95 ! */
    wxString format_delay(wxString::Format(wxT("%i"), i_delay ));
    delay_spinctrl = new wxSpinCtrl( panel, -1, format_delay,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS,
                                     -30000, 30000, i_delay );
    delay_spinctrl->SetToolTip( wxU(_("Set subtitle delay (in 1/10s)" ) ) );

    grid_sizer->Add( label_delay , 0, wxALIGN_CENTER, 5 );
    grid_sizer->Add( delay_spinctrl, 0, wxALIGN_CENTER, 5 );

    misc_sizer->Add( grid_sizer, 0, wxALL, 5 );

    misc_sizer_sizer->Add( misc_sizer, 1, wxEXPAND | wxALL, 5 );

    panel_sizer->Add( misc_sizer, 0, wxEXPAND | wxALL, 5 );

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

    panel_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxALIGN_LEFT | wxALIGN_BOTTOM |
                      wxALL, 5 );
    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

SubsFileDialog::~SubsFileDialog()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void SubsFileDialog::OnOk( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_OK );
}

void SubsFileDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_CANCEL );
}

void SubsFileDialog::OnFileBrowse( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog( this, wxU(_("Open file")),
                         wxT(""), wxT(""), wxT("*"), wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
        file_combo->SetValue( dialog.GetPath() );
    }
}
