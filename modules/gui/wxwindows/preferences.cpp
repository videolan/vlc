/*****************************************************************************
 * preferences.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: preferences.cpp,v 1.1 2003/03/26 00:56:22 gbazin Exp $
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
#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/treectrl.h>

#include <vlc/intf.h>

#include "wxwindows.h"

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

/*****************************************************************************
 * Classes declarations.
 *****************************************************************************/
class PrefsTreeCtrl : public wxTreeCtrl
{
public:

    PrefsTreeCtrl() { }
    PrefsTreeCtrl( wxWindow *parent, intf_thread_t *_p_intf,
                   PrefsDialog *p_prefs_dialog, wxBoxSizer *_p_sizer );
    virtual ~PrefsTreeCtrl();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnSelectTreeItem( wxTreeEvent& event );

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;
    wxBoxSizer *p_sizer;
    wxWindow *p_parent;
};

class PrefsPanel : public wxScrolledWindow
{
public:

    PrefsPanel() { }
    PrefsPanel( wxWindow *parent, intf_thread_t *_p_intf,
                module_t *p_module, char * );
    virtual ~PrefsPanel() {}

private:
    void OnFileBrowse( wxCommandEvent& WXUNUSED(event) );
    void OnDirectoryBrowse( wxCommandEvent& WXUNUSED(event) );
    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
};

class ConfigData : public wxTreeItemData
{
public:

    ConfigData() { panel == NULL; }
    virtual ~ConfigData() { if( panel ) delete panel; }

    wxWindow *panel;
    wxBoxSizer *sizer;
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event = wxID_HIGHEST,
    MRL_Event,

};

BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
    /* Button events */
    EVT_BUTTON(wxID_OK, PrefsDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)

END_EVENT_TABLE()

// menu and control ids
enum
{
    PrefsTree_Ctrl = wxID_HIGHEST
};

BEGIN_EVENT_TABLE(PrefsTreeCtrl, wxTreeCtrl)
    EVT_TREE_SEL_CHANGED(PrefsTree_Ctrl, PrefsTreeCtrl::OnSelectTreeItem)
END_EVENT_TABLE()

enum
{
    FileBrowse_Event = wxID_HIGHEST,
    DirectoryBrowse_Event,

};

BEGIN_EVENT_TABLE(PrefsPanel, wxScrolledWindow)
    /* Button events */
    EVT_BUTTON(FileBrowse_Event, PrefsPanel::OnFileBrowse)
    EVT_BUTTON(DirectoryBrowse_Event, PrefsPanel::OnDirectoryBrowse)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PrefsDialog::PrefsDialog( intf_thread_t *_p_intf, Interface *_p_main_interface)
  :  wxDialog( _p_main_interface, -1, _("Preferences"), wxDefaultPosition,
               wxSize(600,400), wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    /* Create the preferences tree control */
    wxBoxSizer *controls_sizer = new wxBoxSizer( wxHORIZONTAL );
    PrefsTreeCtrl *prefs_tree =
        new PrefsTreeCtrl( panel, p_intf, this, controls_sizer );

    /* Separation */
    wxStaticLine *static_line = new wxStaticLine( panel, wxID_OK );

    /* Create the buttons */
    wxButton *ok_button = new wxButton( panel, wxID_OK, _("OK") );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL, _("Cancel") );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Layout();

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( controls_sizer, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxALIGN_LEFT | wxALIGN_BOTTOM |
                      wxALL, 5 );
    panel_sizer->Layout();
    panel->SetSizer( panel_sizer );
    main_sizer->Add( panel, 1, wxEXPAND, 0 );
    main_sizer->Layout();
    SetSizer( main_sizer );
}

PrefsDialog::~PrefsDialog()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/


/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void PrefsDialog::OnOk( wxCommandEvent& WXUNUSED(event) )
{
    this->Hide();
}

void PrefsDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    this->Hide();
}

void PrefsTreeCtrl::OnSelectTreeItem( wxTreeEvent& event )
{
    ConfigData *config_data;

    config_data = (ConfigData *)GetItemData( event.GetOldItem() );
    if( config_data && config_data->panel )
    {
        config_data->panel->Hide();
        p_sizer->Remove( config_data->panel );
    }

    config_data = (ConfigData *)GetItemData( event.GetItem() );
    if( config_data && config_data->panel )
    {
        config_data->panel->Show();
        config_data->panel->FitInside();
        p_sizer->Add( config_data->panel, 2, wxEXPAND | wxALL, 0 );
        p_sizer->Layout();
    }
}

void PrefsPanel::OnFileBrowse( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog( this, _("Open file"), "", "", "*.*",
                         wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
#if 0
        file_combo->SetValue( dialog.GetPath() );      
#endif
    }
}

void PrefsPanel::OnDirectoryBrowse( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog( this, _("Open file"), "", "", "*.*",
                         wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
#if 0
        file_combo->SetValue( dialog.GetPath() );      
#endif
    }
}

/*****************************************************************************
 * PrefsTreeCtrl class definition.
 *****************************************************************************/
PrefsTreeCtrl::PrefsTreeCtrl( wxWindow *_p_parent, intf_thread_t *_p_intf,
                              PrefsDialog *_p_prefs_dialog,
                              wxBoxSizer *_p_sizer )
  : wxTreeCtrl( _p_parent, PrefsTree_Ctrl, wxDefaultPosition, wxDefaultSize,
                wxTR_NO_LINES | wxTR_FULL_ROW_HIGHLIGHT |
                wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT |
                wxTR_HAS_BUTTONS | wxTR_TWIST_BUTTONS | wxSUNKEN_BORDER )
{
    vlc_list_t      *p_list;
    module_t        *p_module;
    module_config_t *p_item;
    int i_index;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;
    p_sizer = _p_sizer;
    p_parent = _p_parent;

    wxTreeItemId root_item = AddRoot( "" );

    /* List the plugins */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list ) return;

    /*
     * Build a tree of the main options
     */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_module->psz_object_name, "main" ) )
            break;
    }
    if( i_index < p_list->i_count )
    {
        /* We found the main module */

        /* Enumerate config options and add corresponding config boxes */
        p_item = p_module->p_config;

        if( p_item ) do
        {
            if( p_item->b_advanced && !config_GetInt( p_intf, "advanced" ))
            {
                continue;
            }
            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
                ConfigData *config_data = new ConfigData;
                config_data->panel =
                    new PrefsPanel( p_parent, p_intf,
                                    p_module, p_item->psz_text );
                config_data->panel->Hide();

                /* Add the category to the tree */
                AppendItem( root_item, p_item->psz_text, -1, -1, config_data );
                break;
            }
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        SortChildren( root_item );
    }

    /*
     * Build a tree of all the plugins
     */

    wxTreeItemId plugins_item = AppendItem( root_item, _("Plugins") );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;

        /* Find the capabiltiy child item */
        long cookie; size_t i_child_index;
        wxTreeItemId capability_item = GetFirstChild( plugins_item, cookie);
        for( i_child_index = 0;
             i_child_index < GetChildrenCount( plugins_item, FALSE );
             i_child_index++ )
        {
            if( !GetItemText(capability_item).Cmp(p_module->psz_capability) )
            {
                break;
            }
            capability_item = GetNextChild( plugins_item, cookie );
        }

        if( i_child_index == GetChildrenCount( plugins_item, FALSE ) )
        {
            /* We didn't find it, add it */
            capability_item = AppendItem( plugins_item,
                                          p_module->psz_capability );
        }

        /* Add the plugin to the tree */
        ConfigData *config_data = new ConfigData;
        config_data->panel =
            new PrefsPanel( p_parent, p_intf, p_module, NULL );
        config_data->panel->Hide();
        AppendItem( capability_item, p_module->psz_object_name, -1, -1,
                    config_data );
    }

    /* Sort all this mess */
    long cookie; size_t i_child_index;
    SortChildren( plugins_item );
    wxTreeItemId capability_item = GetFirstChild( plugins_item, cookie);
    for( i_child_index = 0;
         i_child_index < GetChildrenCount( plugins_item, FALSE );
         i_child_index++ )
    {
        capability_item = GetNextChild( plugins_item, cookie );
        SortChildren( capability_item );
    }

    /* Clean-up everything */
    vlc_list_release( p_list );

    p_sizer->Add( this, 1, wxEXPAND | wxALL, 0 );
    p_sizer->Layout();

}

PrefsTreeCtrl::~PrefsTreeCtrl()
{
}

/*****************************************************************************
 * PrefsPanel class definition.
 *****************************************************************************/
PrefsPanel::PrefsPanel( wxWindow* parent, intf_thread_t *_p_intf,
                        module_t *p_module, char *psz_section )
  :  wxScrolledWindow( parent, -1, wxDefaultPosition, wxDefaultSize )
{
    module_config_t *p_item;
    wxStaticText *label;
    wxComboBox *combo;
    wxRadioButton *radio;
    wxSpinCtrl *spin;
    wxCheckBox *checkbox;
    wxTextCtrl *textctrl;
    wxButton *button;
    wxStaticLine *static_line;
    wxBoxSizer *horizontal_sizer;

    /* Initializations */
    p_intf = _p_intf;
    SetAutoLayout( TRUE );
    SetScrollRate( 5, 5 );

    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );

    /* Enumerate config options and add corresponding config boxes */
    p_item = p_module->p_config;

    /* Find the category if it has been specified */
    if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY )
    {
        while( !p_item->i_type == CONFIG_HINT_CATEGORY ||
               strcmp( psz_section, p_item->psz_text ) )
        {
            if( p_item->i_type == CONFIG_HINT_END )
                break;
            p_item++;
        }
    }

    /* Add a head title to the panel */
    wxStaticBox *static_box = new wxStaticBox( this, -1, "" );
    wxStaticBoxSizer *box_sizer = new wxStaticBoxSizer( static_box,
                                                        wxHORIZONTAL );
    label = new wxStaticText( this, -1,
                              psz_section ? p_item->psz_text :
                              p_module->psz_longname );

    box_sizer->Add( label, 1, wxALL, 5 );
    sizer->Add( box_sizer, 0, wxEXPAND | wxALL, 5 );

    if( p_item ) do
    {
        if( p_item->b_advanced && !config_GetInt( p_intf, "advanced" ) )
        {
            continue;
        }

        /* If a category has been specified, check we finished the job */
        if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY &&
            strcmp( psz_section, p_item->psz_text ) )
            break;

        switch( p_item->i_type )
        {
        case CONFIG_HINT_CATEGORY:
#if 0
            label = new wxStaticText(this, -1, p_item->psz_text);
            sizer->Add( label, 0, wxALL, 5 );
#endif
            break;

        case CONFIG_ITEM_MODULE:
            /* build a list of available modules */

            label = new wxStaticText(this, -1, p_item->psz_text);
            combo = new wxComboBox( this, -1, "", wxPoint(20,25),
                                    wxSize(120, -1), 0, NULL );
            combo->SetToolTip( p_item->psz_longtext );
            horizontal_sizer = new wxBoxSizer( wxHORIZONTAL );
            horizontal_sizer->Add( label, 0, wxALL, 5 );
            horizontal_sizer->Add( combo, 0, wxALL, 5 );
            sizer->Add( horizontal_sizer, 0, wxALL, 5 );
            break;

        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
            label = new wxStaticText(this, -1, p_item->psz_text);
            textctrl = new wxTextCtrl( this, -1, "",
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_PROCESS_ENTER);
#if 0
            combo = new wxComboBox( this, -1, "", wxPoint(20,25),
                                    wxSize(120, -1), 0, NULL );
#endif
            textctrl->SetToolTip( p_item->psz_longtext );
            horizontal_sizer = new wxBoxSizer( wxHORIZONTAL );
            horizontal_sizer->Add( label, 0, wxALL, 5 );
            horizontal_sizer->Add( textctrl, 0, wxALL, 5 );
            if( p_item->i_type == CONFIG_ITEM_FILE )
            {
                button = new wxButton( this, -1, _("Browse...") );
                horizontal_sizer->Add( button, 0, wxALL, 5 );
            }
            sizer->Add( horizontal_sizer, 0, wxALL, 5 );
            break;

        case CONFIG_ITEM_INTEGER:
            label = new wxStaticText(this, -1, p_item->psz_text);
            spin = new wxSpinCtrl( this, -1, p_item->psz_text,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, 8);
            spin->SetToolTip( p_item->psz_longtext );
            horizontal_sizer = new wxBoxSizer( wxHORIZONTAL );
            horizontal_sizer->Add( label, 0, wxALL, 5 );
            horizontal_sizer->Add( spin, 0, wxALL, 5 );
            sizer->Add( horizontal_sizer, 0, wxALL, 5 );
            break;

        case CONFIG_ITEM_FLOAT:
            label = new wxStaticText(this, -1, p_item->psz_text);
            spin = new wxSpinCtrl( this, -1, p_item->psz_text,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, 8);
            spin->SetToolTip( p_item->psz_longtext );
            horizontal_sizer = new wxBoxSizer( wxHORIZONTAL );
            horizontal_sizer->Add( label, 0, wxALL, 5 );
            horizontal_sizer->Add( spin, 0, wxALL, 5 );
            sizer->Add( horizontal_sizer, 0, wxALL, 5 );
            break;

        case CONFIG_ITEM_BOOL:
            checkbox = new wxCheckBox( this, -1, p_item->psz_text );
            checkbox->SetToolTip( p_item->psz_longtext );
            horizontal_sizer = new wxBoxSizer( wxHORIZONTAL );
            horizontal_sizer->Add( checkbox, 0, wxALL, 5 );
            sizer->Add( horizontal_sizer, 0, wxALL, 5 );
            break;
        }
    }
    while( p_item->i_type != CONFIG_HINT_END && p_item++ );


    sizer->Layout();
    SetSizer( sizer );
}
