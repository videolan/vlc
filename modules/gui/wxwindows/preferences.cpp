/*****************************************************************************
 * preferences.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: preferences.cpp,v 1.10 2003/04/01 16:11:43 gbazin Exp $
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
#include <wx/window.h>
#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/treectrl.h>
#include <wx/clntdata.h>
#include <wx/dynarray.h>

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

    void ApplyChanges();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnSelectTreeItem( wxTreeEvent& event );

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;
    wxBoxSizer *p_sizer;
    wxWindow *p_parent;

    wxTreeItemId root_item;
    wxTreeItemId plugins_item;
};

struct ConfigData
{
    ConfigData( wxPanel *_panel, int _i_conf_type,
                vlc_bool_t _b_advanced, char *psz_name )
    { panel = _panel; b_advanced = _b_advanced; b_config_list = VLC_FALSE;
      i_config_type = _i_conf_type; option_name = psz_name; }

    vlc_bool_t b_advanced;
    int i_config_type;
    vlc_bool_t b_config_list;

    union control
    {
        wxComboBox *combobox;
        wxRadioButton *radio;
        wxSpinCtrl *spinctrl;
        wxCheckBox *checkbox;
        wxTextCtrl *textctrl;

    } control;

    wxPanel *panel;
    wxString option_name;
};

WX_DEFINE_ARRAY(ConfigData *, ArrayOfConfigData);

class PrefsPanel : public wxPanel
{
public:

    PrefsPanel() { }
    PrefsPanel( wxWindow *parent, intf_thread_t *_p_intf,
                PrefsDialog *_p_prefs_dialog,
                module_t *p_module, char * );
    virtual ~PrefsPanel() {}

    void ApplyChanges();

private:
    void OnAdvanced( wxCommandEvent& WXUNUSED(event) );
    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;

    vlc_bool_t b_advanced;

    wxBoxSizer *config_sizer;
    wxScrolledWindow *config_window;

    ArrayOfConfigData config_array;
};

class ConfigTreeData : public wxTreeItemData
{
public:

    ConfigTreeData() { panel == NULL; }
    virtual ~ConfigTreeData() { if( panel ) delete panel; }

    PrefsPanel *panel;
    wxBoxSizer *sizer;
};

class ConfigEvtHandler : public wxEvtHandler
{
public:
    ConfigEvtHandler( intf_thread_t *p_intf, PrefsDialog *p_prefs_dialog );
    virtual ~ConfigEvtHandler();

    void ConfigEvtHandler::OnCommandEvent( wxCommandEvent& event );

private:

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event = wxID_HIGHEST,
    MRL_Event,
    Reset_Event,
    Advanced_Event,
};

BEGIN_EVENT_TABLE(PrefsDialog, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, PrefsDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)
    EVT_BUTTON(wxID_SAVE, PrefsDialog::OnSave)

    /* Don't destroy the window when the user closes it */
    EVT_CLOSE(PrefsDialog::OnCancel)
END_EVENT_TABLE()

// menu and control ids
enum
{
    PrefsTree_Ctrl = wxID_HIGHEST
};

BEGIN_EVENT_TABLE(PrefsTreeCtrl, wxTreeCtrl)
    EVT_TREE_SEL_CHANGED(PrefsTree_Ctrl, PrefsTreeCtrl::OnSelectTreeItem)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(PrefsPanel, wxPanel)
    /* Button events */
    EVT_BUTTON(Advanced_Event, PrefsPanel::OnAdvanced)

END_EVENT_TABLE()

BEGIN_EVENT_TABLE(ConfigEvtHandler, wxEvtHandler)
    EVT_BUTTON(-1, ConfigEvtHandler::OnCommandEvent)
    EVT_TEXT(-1, ConfigEvtHandler::OnCommandEvent)
    EVT_RADIOBOX(-1, ConfigEvtHandler::OnCommandEvent)
    EVT_SPINCTRL(-1, ConfigEvtHandler::OnCommandEvent)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PrefsDialog::PrefsDialog( intf_thread_t *_p_intf, Interface *_p_main_interface)
  :  wxFrame( _p_main_interface, -1, _("Preferences"), wxDefaultPosition,
              wxSize(650,450), wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    /* Create the preferences tree control */
    wxBoxSizer *controls_sizer = new wxBoxSizer( wxHORIZONTAL );
    prefs_tree =
        new PrefsTreeCtrl( panel, p_intf, this, controls_sizer );

    /* Separation */
    wxStaticLine *static_line = new wxStaticLine( panel, wxID_OK );

    /* Create the buttons */
    wxButton *ok_button = new wxButton( panel, wxID_OK, _("OK") );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL, _("Cancel") );
    wxButton *save_button = new wxButton( panel, wxID_SAVE, _("Save") );
    //wxButton *reset_button = new wxButton( panel, Reset_Event, _("Reset") );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Add( save_button, 0, wxALL, 5 );
    //button_sizer->Add( reset_button, 0, wxALL, 5 );
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
    prefs_tree->ApplyChanges();

    this->Hide();
}

void PrefsDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    this->Hide();
}

void PrefsDialog::OnSave( wxCommandEvent& WXUNUSED(event) )
{
    prefs_tree->ApplyChanges();

    config_SaveConfigFile( p_intf, NULL );
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

    root_item = AddRoot( "" );

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
            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
                ConfigTreeData *config_data = new ConfigTreeData;
                config_data->panel =
                    new PrefsPanel( p_parent, p_intf, p_prefs_dialog,
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
    plugins_item = AppendItem( root_item, _("Plugins") );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;

        /* Exclude the main module */
        if( !strcmp( p_module->psz_object_name, "main" ) )
            continue;

        /* Exclude empty plugins */
        p_item = p_module->p_config;
        if( !p_item ) continue;
        do
        {
            if( p_item->i_type & CONFIG_ITEM )
                break;
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );
        if( p_item->i_type == CONFIG_HINT_END ) continue;

        /* Find the capability child item */
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

        if( i_child_index == GetChildrenCount( plugins_item, FALSE ) &&
            p_module->psz_capability && *p_module->psz_capability )
        {
            /* We didn't find it, add it */
            capability_item = AppendItem( plugins_item,
                                          p_module->psz_capability );
        }

        /* Add the plugin to the tree */
        ConfigTreeData *config_data = new ConfigTreeData;
        config_data->panel =
            new PrefsPanel( p_parent, p_intf, p_prefs_dialog, p_module, NULL );
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

    /* Update Tree Ctrl */
#ifndef WIN32 /* Workaround a bug in win32 implementation */
    SelectItem( GetFirstChild( root_item, cookie ) );
#endif
}

PrefsTreeCtrl::~PrefsTreeCtrl()
{
}

void PrefsTreeCtrl::ApplyChanges()
{
    long cookie, cookie2;
    ConfigTreeData *config_data;

    /* Apply changes to the main module */
    wxTreeItemId item = GetFirstChild( root_item, cookie );
    for( size_t i_child_index = 0;
         i_child_index < GetChildrenCount( root_item, FALSE );
         i_child_index++ )
    {
        config_data = (ConfigTreeData *)GetItemData( item );
        if( config_data )
        {
            config_data->panel->ApplyChanges();
        }

        item = GetNextChild( root_item, cookie );
    }

    /* Apply changes to the plugins */
    item = GetFirstChild( plugins_item, cookie );
    for( size_t i_child_index = 0;
         i_child_index < GetChildrenCount( plugins_item, FALSE );
         i_child_index++ )
    {
        wxTreeItemId item2 = GetFirstChild( item, cookie2 );
        for( size_t i_child_index = 0;
             i_child_index < GetChildrenCount( item, FALSE );
             i_child_index++ )
        {
            config_data = (ConfigTreeData *)GetItemData( item2 );
            if( config_data )
            {
                config_data->panel->ApplyChanges();
            }

            item2 = GetNextChild( item, cookie2 );
        }

        item = GetNextChild( plugins_item, cookie );
    }
}

void PrefsTreeCtrl::OnSelectTreeItem( wxTreeEvent& event )
{
    ConfigTreeData *config_data;

    config_data = (ConfigTreeData *)GetItemData( event.GetOldItem() );
    if( config_data && config_data->panel )
    {
        config_data->panel->Hide();
        p_sizer->Remove( config_data->panel );
    }

    config_data = (ConfigTreeData *)GetItemData( event.GetItem() );
    if( config_data && config_data->panel )
    {
        config_data->panel->Show();
        p_sizer->Add( config_data->panel, 2, wxEXPAND | wxALL, 0 );
        p_sizer->Layout();
    }
}

/*****************************************************************************
 * PrefsPanel class definition.
 *****************************************************************************/
PrefsPanel::PrefsPanel( wxWindow* parent, intf_thread_t *_p_intf,
                        PrefsDialog *_p_prefs_dialog,
                        module_t *p_module, char *psz_section )
  :  wxPanel( parent, -1, wxDefaultPosition, wxDefaultSize )
{
    module_config_t *p_item;
    vlc_list_t      *p_list;
    module_t        *p_parser;

    wxStaticText *label;
    wxComboBox *combo;
    wxSpinCtrl *spin;
    wxCheckBox *checkbox;
    wxTextCtrl *textctrl;
    wxButton *button;
    wxArrayString array;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog =_p_prefs_dialog,

    b_advanced = VLC_TRUE;
    SetAutoLayout( TRUE );

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

    /* Now put all the config options into a scrolled window */
    config_sizer = new wxBoxSizer( wxVERTICAL );
    config_window = new wxScrolledWindow( this, -1, wxDefaultPosition,
                                          wxDefaultSize );
    config_window->SetAutoLayout( TRUE );
    config_window->SetScrollRate( 5, 5 );

    if( p_item ) do
    {
        /* If a category has been specified, check we finished the job */
        if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY &&
            strcmp( psz_section, p_item->psz_text ) )
            break;

        /* put each config option in a separate panel so we can hide advanced
         * options easily */
        wxPanel *panel = new wxPanel( config_window, -1, wxDefaultPosition,
                                      wxDefaultSize );
        wxBoxSizer *panel_sizer = new wxBoxSizer( wxHORIZONTAL );
        ConfigData *config_data =
            new ConfigData( panel, p_item->i_type,
                            p_item->b_advanced, p_item->psz_name );

        switch( p_item->i_type )
        {
        case CONFIG_ITEM_MODULE:
            label = new wxStaticText(panel, -1, p_item->psz_text);
            combo = new wxComboBox( panel, -1, p_item->psz_value,
                                    wxDefaultPosition, wxDefaultSize,
                                    0, NULL, wxCB_READONLY | wxCB_SORT );

            /* build a list of available modules */
            p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
            combo->Append( _("Default"), (void *)NULL );
            combo->SetSelection( 0 );
            for( int i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_parser = (module_t *)p_list->p_values[i_index].p_object ;

                if( !strcmp( p_parser->psz_capability,
                             p_item->psz_type ) )
                {
                    combo->Append( p_parser->psz_longname,
                                   p_parser->psz_object_name );
                    if( p_item->psz_value &&
                        !strcmp(p_item->psz_value, p_parser->psz_object_name) )
                        combo->SetValue( p_parser->psz_longname );
                }
            }

            combo->SetToolTip( p_item->psz_longtext );
            config_data->control.combobox = combo;
            panel_sizer->Add( label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
            panel_sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
            break;

        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:
            label = new wxStaticText(panel, -1, p_item->psz_text);
            panel_sizer->Add( label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

            if( !p_item->ppsz_list )
            {
                textctrl = new wxTextCtrl( panel, -1, p_item->psz_value,
                                           wxDefaultPosition, wxDefaultSize,
                                           wxTE_PROCESS_ENTER);
                textctrl->SetToolTip( p_item->psz_longtext );
                config_data->control.textctrl = textctrl;
                panel_sizer->Add( textctrl, 1, wxALL, 5 );
            }
            else
            {
                combo = new wxComboBox( panel, -1, p_item->psz_value,
                                        wxDefaultPosition, wxDefaultSize,
                                        0, NULL, wxCB_READONLY | wxCB_SORT );

                /* build a list of available options */
                for( int i_index = 0; p_item->ppsz_list[i_index]; i_index++ )
                {
                    combo->Append( p_item->ppsz_list[i_index] );
                }

		if( p_item->psz_value ) combo->SetValue( p_item->psz_value );
                combo->SetToolTip( p_item->psz_longtext );
                config_data->control.combobox = combo;
                config_data->b_config_list = VLC_TRUE;
                panel_sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
            }

            if( p_item->i_type == CONFIG_ITEM_FILE )
            {
                button = new wxButton( panel, -1, _("Browse...") );
                panel_sizer->Add( button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
                button->SetClientData((void *)config_data);
            }
            break;

        case CONFIG_ITEM_INTEGER:
            label = new wxStaticText(panel, -1, p_item->psz_text);
            spin = new wxSpinCtrl( panel, -1,
                                   wxString::Format("%d", p_item->i_value),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, p_item->i_value);
            spin->SetToolTip( p_item->psz_longtext );
            config_data->control.spinctrl = spin;
            panel_sizer->Add( label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
            panel_sizer->Add( spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

            spin->SetClientData((void *)config_data);
            break;

        case CONFIG_ITEM_FLOAT:
            label = new wxStaticText(panel, -1, p_item->psz_text);
            spin = new wxSpinCtrl( panel, -1,
                                   wxString::Format("%f", p_item->f_value),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, (int)p_item->f_value);
            spin->SetToolTip( p_item->psz_longtext );
            config_data->control.spinctrl = spin;
            panel_sizer->Add( label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
            panel_sizer->Add( spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
            break;

        case CONFIG_ITEM_BOOL:
            checkbox = new wxCheckBox( panel, -1, p_item->psz_text );
            if( p_item->i_value ) checkbox->SetValue(TRUE);
            checkbox->SetToolTip( p_item->psz_longtext );
            config_data->control.checkbox = checkbox;
            panel_sizer->Add( checkbox, 0, wxALL, 5 );
            break;

        default:
            delete panel; panel = NULL;
            delete panel_sizer;
            delete config_data;
            break;
        }

        /* Don't add items that were not recognized */
        if( panel == NULL ) continue;

        panel_sizer->Layout();
        panel->SetSizerAndFit( panel_sizer );

        /* Add the config data to our array so we can keep a trace of it */
        config_array.Add( config_data );

        config_sizer->Add( panel, 0, wxEXPAND | wxALL, 2 );
    }
    while( p_item->i_type != CONFIG_HINT_END && p_item++ );

    /* Display a nice message if no configuration options are available */
    if( !config_array.GetCount() )
    {
        config_sizer->Add( new wxStaticText( config_window, -1,
                           _("No configuration options available") ), 1,
                           wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER, 2 );
    }

    config_sizer->Layout();
    config_window->SetSizer( config_sizer );
    sizer->Add( config_window, 1, wxEXPAND | wxALL, 5 );

    /* Intercept all menu events in our custom event handler */
    config_window->PushEventHandler(
        new ConfigEvtHandler( p_intf, p_prefs_dialog ) );

    /* Update panel */
    wxCommandEvent dummy_event;
    b_advanced = !config_GetInt( p_intf, "advanced" );
    OnAdvanced( dummy_event );

    /* Create advanced button */
    if( config_array.GetCount() )
    {
        wxButton *advanced_button = new wxButton( this, Advanced_Event,
                                                  _("Advanced...") );
        sizer->Add( advanced_button, 0, wxALL, 5 );
    }

    sizer->Layout();
    SetSizer( sizer );
}

void PrefsPanel::ApplyChanges()
{
    for( size_t i = 0; i < config_array.GetCount(); i++ )
    {
        ConfigData *config_data = config_array.Item(i);

        switch( config_data->i_config_type )
        {
        case CONFIG_ITEM_MODULE:
            config_PutPsz( p_intf, config_data->option_name.c_str(), (char *)
                           config_data->control.combobox->GetClientData(
                           config_data->control.combobox->GetSelection() ) );
            break;
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:
            if( !config_data->b_config_list )
                config_PutPsz( p_intf, config_data->option_name.c_str(),
                               config_data->control.textctrl->GetValue() );
            else
                config_PutPsz( p_intf, config_data->option_name.c_str(),
                               config_data->control.combobox->GetValue() );
            break;
        case CONFIG_ITEM_BOOL:
            config_PutInt( p_intf, config_data->option_name.c_str(),
                           config_data->control.checkbox->IsChecked() );
            break;
        case CONFIG_ITEM_INTEGER:
            config_PutInt( p_intf, config_data->option_name.c_str(),
                           config_data->control.spinctrl->GetValue() );
            break;
        case CONFIG_ITEM_FLOAT:
            config_PutFloat( p_intf, config_data->option_name.c_str(),
                             config_data->control.spinctrl->GetValue() );
            break;
        }
    }
}

void PrefsPanel::OnAdvanced( wxCommandEvent& WXUNUSED(event) )
{
    b_advanced = !b_advanced;

    for( size_t i = 0; i < config_array.GetCount(); i++ )
    {
        ConfigData *config_data = config_array.Item(i);
        if( config_data->b_advanced )
        {
            config_data->panel->Show( b_advanced );
            config_sizer->Show( config_data->panel, b_advanced );
        }
    }

    config_sizer->Layout();
    config_window->FitInside();
}

/*****************************************************************************
 * A small helper class which intercepts all events
 *****************************************************************************/
ConfigEvtHandler::ConfigEvtHandler( intf_thread_t *_p_intf,
                                    PrefsDialog *_p_prefs_dialog )
{
    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;
}

ConfigEvtHandler::~ConfigEvtHandler()
{
}

void ConfigEvtHandler::OnCommandEvent( wxCommandEvent& event )
{
    if( !event.GetEventObject() )
    {
        event.Skip();
        return;
    }

    ConfigData *config_data = (ConfigData *)
        ((wxEvtHandler *)event.GetEventObject())->GetClientData();

    if( !config_data )
    {
        event.Skip();
        return;
    }

    if( config_data->i_config_type == CONFIG_ITEM_FILE )
    {
        wxFileDialog dialog( p_prefs_dialog, _("Open file"), "", "", "*.*",
                             wxOPEN | wxSAVE );

        if( dialog.ShowModal() == wxID_OK )
        {
            config_data->control.textctrl->SetValue( dialog.GetPath() );      
        }
    }

    switch( config_data->i_config_type )
    {
    case CONFIG_ITEM_MODULE:
        break;
    case CONFIG_ITEM_STRING:
        break;
    case CONFIG_ITEM_FILE:
        break;
    case CONFIG_ITEM_INTEGER:
        break;
    case CONFIG_ITEM_FLOAT:
        break;
    case CONFIG_ITEM_BOOL:
        break;
    }

    event.Skip();
}
