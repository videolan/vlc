/*****************************************************************************
 * subtitles.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: subtitles.cpp,v 1.1 2003/05/13 22:59:16 gbazin Exp $
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

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>

#include <vlc/intf.h>

#if defined MODULE_NAME_IS_skins
#   include "../skins/src/skin_common.h"
#endif

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
    wxDialog( _p_parent, -1, wxU(_("Open Subtitles File")),
              wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    /* Create the subtitles file textctrl */
    wxBoxSizer *file_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *file_box = new wxStaticBox( panel, -1,
                                             wxU(_("Subtitles file")) );
    wxStaticBoxSizer *file_sizer = new wxStaticBoxSizer( file_box,
                                                        wxHORIZONTAL );
    char *psz_subsfile = config_GetPsz( p_intf, "sub-file" );
    file_combo = new wxComboBox( panel, -1,
                                 psz_subsfile ? wxU(psz_subsfile) : wxT(""),
                                 wxPoint(20,25), wxSize(300, -1), 0, NULL );
    if( psz_subsfile ) free( psz_subsfile );
    wxButton *browse_button = new wxButton( panel, FileBrowse_Event,
                                            wxU(_("Browse...")) );
    file_sizer->Add( file_combo, 1, wxALL, 5 );
    file_sizer->Add( browse_button, 0, wxALL, 5 );
    file_sizer_sizer->Add( file_sizer, 1, wxEXPAND | wxALL, 5 );

    /* Misc Subtitles options */
    wxBoxSizer *misc_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *misc_box = new wxStaticBox( panel, -1,
                                             wxU(_("Subtitles options")) );
    wxStaticBoxSizer *misc_sizer = new wxStaticBoxSizer( misc_box,
                                                         wxHORIZONTAL );
    wxStaticText *label =
        new wxStaticText(panel, -1, wxU(_("Delay subtitles (in 1/10s)")));
    int i_delay = config_GetInt( p_intf, "sub-delay" );
    delay_spinctrl = new wxSpinCtrl( panel, -1,
                                     wxString::Format(wxT("%d"), i_delay),
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS,
                                     -650000, 650000, i_delay );

    misc_sizer->Add( label, 0, wxALL, 5 );
    misc_sizer->Add( delay_spinctrl, 0, wxALL, 5 );

    label = new wxStaticText(panel, -1, wxU(_("Frames per second")));

    float f_fps = config_GetFloat( p_intf, "sub-fps" );
    fps_spinctrl = new wxSpinCtrl( panel, -1,
                                   wxString::Format(wxT("%d"),(int)f_fps),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, (int)f_fps );
    fps_spinctrl->SetToolTip( wxU(_("Override frames per second. "
                              "It will only work with MicroDVD subtitles.")) );
    misc_sizer->Add( label, 0, wxALL, 5 );
    misc_sizer->Add( fps_spinctrl, 0, wxALL, 5 );

    misc_sizer_sizer->Add( misc_sizer, 1, wxEXPAND | wxALL, 5 );

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
    panel_sizer->Add( file_sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( misc_sizer, 0, wxEXPAND | wxALL, 5 );
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
                         wxT(""), wxT(""), wxT("*.*"), wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
        file_combo->SetValue( dialog.GetPath() );
    }
}
