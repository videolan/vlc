/*****************************************************************************
 * open.hpp: Headers for the Open dialog
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifndef _WXVLC_OPEN_H_
#define _WXVLC_OPEN_H_

#include "wxwidgets.hpp"

#include <wx/spinctrl.h>
#include <wx/notebook.h>

class AutoBuiltPanel;
namespace wxvlc
{
    class SubsFileDialog;
    class SoutDialog;
    WX_DEFINE_ARRAY(AutoBuiltPanel *, ArrayOfAutoBuiltPanel);

    class OpenDialog: public wxDialog
    {

public:
    /* Constructor */
    OpenDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                int i_access_method, int i_arg = 0  );

    /* Extended Contructor */
    OpenDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                int i_access_method, int i_arg = 0 , int _i_method = 0 );
    virtual ~OpenDialog();

    int Show();
    int Show( int i_access_method, int i_arg = 0 );

    void UpdateMRL();
    void UpdateMRL( int i_access_method );

    wxArrayString mrl;

private:
    wxPanel *FilePanel( wxWindow* parent );
    wxPanel *DiscPanel( wxWindow* parent );
    wxPanel *NetPanel( wxWindow* parent );

    ArrayOfAutoBuiltPanel input_tab_array;

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnClose( wxCloseEvent& event );

    void OnPageChange( wxNotebookEvent& event );
    void OnMRLChange( wxCommandEvent& event );

    /* Event handlers for the file page */
    void OnFilePanelChange( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );
    void OnSubFileBrowse( wxCommandEvent& event );
    void OnSubFileChange( wxCommandEvent& event );

    /* Event handlers for the disc page */
    void OnDiscPanelChangeSpin( wxSpinEvent& event );
    void OnDiscPanelChange( wxCommandEvent& event );
    void OnDiscTypeChange( wxCommandEvent& event );
#ifdef HAVE_LIBCDIO
    void OnDiscProbe( wxCommandEvent& event );
#endif
    void OnDiscDeviceChange( wxCommandEvent& event );

    /* Event handlers for the net page */
    void OnNetPanelChangeSpin( wxSpinEvent& event );
    void OnNetPanelChange( wxCommandEvent& event );
    void OnNetTypeChange( wxCommandEvent& event );

    /* Event handlers for the stream output */
    void OnSubsFileEnable( wxCommandEvent& event );
    void OnSubsFileSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the stream output */
    void OnSoutEnable( wxCommandEvent& event );
    void OnSoutSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the caching option */
    void OnCachingEnable( wxCommandEvent& event );
    void OnCachingChange( wxCommandEvent& event );
    void OnCachingChangeSpin( wxSpinEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
    int i_current_access_method;
    int i_disc_type_selection;

    int i_method; /* Normal or for the stream dialog ? */
    int i_open_arg;

    wxComboBox *mrl_combo;
    wxNotebook *notebook;

    /* Controls for the file panel */
    wxComboBox *file_combo;
    wxComboBox *subfile_combo;
    wxFileDialog *file_dialog;

    /* Controls for the disc panel */
    wxRadioBox *disc_type;
    wxCheckBox *disc_probe;
    wxTextCtrl *disc_device;
    wxSpinCtrl *disc_title; int i_disc_title;
    wxSpinCtrl *disc_chapter; int i_disc_chapter;
    wxSpinCtrl *disc_sub; int i_disc_sub;
    wxSpinCtrl *disc_audio; int i_disc_audio;

    /* The media equivalent name for a DVD names. For example,
     * "Title", is "Track" for a CD-DA */
    wxStaticText *disc_title_label;
    wxStaticText *disc_chapter_label;
    wxStaticText *disc_sub_label;
    wxStaticText *disc_audio_label;

    /* Indicates if the disc device control was modified */
    bool b_disc_device_changed;

    /* Controls for the net panel */
    wxRadioBox *net_type;
    int i_net_type;
    wxPanel *net_subpanels[4];
    wxRadioButton *net_radios[4];
    wxSpinCtrl *net_ports[4];
    int        i_net_ports[4];
    wxTextCtrl *net_addrs[4];
    wxCheckBox *net_timeshift;
    wxCheckBox *net_ipv6;

    /* Controls for the subtitles file */
    wxButton *subsfile_button;
    wxButton *subbrowse_button;
    wxCheckBox *subsfile_checkbox;
    SubsFileDialog *subsfile_dialog;
    wxArrayString subsfile_mrl;

    /* Controls for the stream output */
    wxButton *sout_button;
    wxCheckBox *sout_checkbox;
    SoutDialog *sout_dialog;
    wxArrayString sout_mrl;

    /* Controls for the caching options */
    wxCheckBox *caching_checkbox;
    wxSpinCtrl *caching_value;
    int i_caching;
};

};

enum
{
    FILE_ACCESS = 0,
    DISC_ACCESS,
    NET_ACCESS,

    /* Auto-built panels */
    CAPTURE_ACCESS
};
#define MAX_ACCESS CAPTURE_ACCESS


#endif
