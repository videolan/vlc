/*****************************************************************************
 * preferences.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
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

#include <vlc_help.h>

#include "wxwindows.h"
#include "preferences_widgets.h"

#include <wx/combobox.h>
#include <wx/statline.h>
#include <wx/clntdata.h>
#include <wx/dynarray.h>

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

#define GENERAL_ID 1242
#define PLUGIN_ID 1243
#define CAPABILITY_ID 1244

/*****************************************************************************
 * Classes declarations.
 *****************************************************************************/
class ConfigTreeData;
class PrefsTreeCtrl : public wxTreeCtrl
{
public:

    PrefsTreeCtrl() { }
    PrefsTreeCtrl( wxWindow *parent, intf_thread_t *_p_intf,
                   PrefsDialog *p_prefs_dialog, wxBoxSizer *_p_sizer );
    virtual ~PrefsTreeCtrl();

    void ApplyChanges();
    void CleanChanges();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnSelectTreeItem( wxTreeEvent& event );
    void OnAdvanced( wxCommandEvent& event );

    ConfigTreeData *FindModuleConfig( ConfigTreeData *config_data );

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;
    wxBoxSizer *p_sizer;
    wxWindow *p_parent;
    vlc_bool_t b_advanced;

    wxTreeItemId root_item;
    wxTreeItemId general_item;
    wxTreeItemId plugins_item;
};

WX_DEFINE_ARRAY(ConfigControl *, ArrayOfConfigControls);

class PrefsPanel : public wxPanel
{
public:

    PrefsPanel() { }
    PrefsPanel( wxWindow *parent, intf_thread_t *_p_intf,
                PrefsDialog *, int i_object_id, char *, char * );
    virtual ~PrefsPanel() {}

    void ApplyChanges();
    void SwitchAdvanced( vlc_bool_t );

private:
    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;

    vlc_bool_t b_advanced;

    wxBoxSizer *config_sizer;
    wxScrolledWindow *config_window;

    ArrayOfConfigControls config_array;
};

class ConfigTreeData : public wxTreeItemData
{
public:

    ConfigTreeData() { b_submodule = 0; panel = NULL; psz_section = NULL;
                       psz_help = NULL; }
    virtual ~ConfigTreeData() { if( panel ) delete panel;
                                if( psz_section) free(psz_section);
                                if( psz_help) free(psz_help); }

    vlc_bool_t b_submodule;

    PrefsPanel *panel;
    wxBoxSizer *sizer;
    int i_object_id;
    char *psz_section;
    char *psz_help;
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event = wxID_HIGHEST,
    MRL_Event,
    ResetAll_Event,
    Advanced_Event,
};

BEGIN_EVENT_TABLE(PrefsDialog, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, PrefsDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)
    EVT_BUTTON(wxID_SAVE, PrefsDialog::OnSave)
    EVT_BUTTON(ResetAll_Event, PrefsDialog::OnResetAll)
    EVT_CHECKBOX(Advanced_Event, PrefsDialog::OnAdvanced)

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
    EVT_COMMAND(Advanced_Event, wxEVT_USER_FIRST, PrefsTreeCtrl::OnAdvanced)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PrefsDialog::PrefsDialog( intf_thread_t *_p_intf, wxWindow *p_parent)
  :  wxFrame( p_parent, -1, wxU(_("Preferences")), wxDefaultPosition,
              wxSize(700,450), wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
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
    wxButton *ok_button = new wxButton( panel, wxID_OK, wxU(_("OK")) );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL,
                                            wxU(_("Cancel")) );
    wxButton *save_button = new wxButton( panel, wxID_SAVE, wxU(_("Save")) );
    wxButton *reset_button = new wxButton( panel, ResetAll_Event,
                                           wxU(_("Reset All")) );

    wxPanel *dummy_panel = new wxPanel( this, -1 );
    wxCheckBox *advanced_checkbox =
        new wxCheckBox( panel, Advanced_Event, wxU(_("Advanced options")) );

    if( config_GetInt( p_intf, "advanced" ) )
    {
        advanced_checkbox->SetValue(TRUE);
        wxCommandEvent dummy_event;
        dummy_event.SetInt(TRUE);
        OnAdvanced( dummy_event );
    }

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Add( save_button, 0, wxALL, 5 );
    button_sizer->Add( reset_button, 0, wxALL, 5 );
    button_sizer->Add( dummy_panel, 1, wxALL, 5 );
    button_sizer->Add( advanced_checkbox, 0, wxALL | wxALIGN_RIGHT |
                       wxALIGN_CENTER_VERTICAL, 0 );
    button_sizer->Layout();

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( controls_sizer, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxALIGN_LEFT | wxALIGN_BOTTOM |
                      wxALL | wxEXPAND, 5 );
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
    prefs_tree->CleanChanges();
}

void PrefsDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    this->Hide();
    prefs_tree->CleanChanges();
}

void PrefsDialog::OnSave( wxCommandEvent& WXUNUSED(event) )
{
    prefs_tree->ApplyChanges();
    config_SaveConfigFile( p_intf, NULL );
    this->Hide();
}

void PrefsDialog::OnResetAll( wxCommandEvent& WXUNUSED(event) )
{
    wxMessageDialog dlg( this,
        wxU(_("Beware this will reset your VLC media player preferences.\n"
              "Are you sure you want to continue?")),
        wxU(_("Reset Preferences")), wxYES_NO|wxNO_DEFAULT|wxCENTRE );

    if ( dlg.ShowModal() == wxID_YES )
    {
        /* TODO: need to reset all the controls */
        config_ResetAll( p_intf );
        prefs_tree->CleanChanges();
        config_SaveConfigFile( p_intf, NULL );
    }
}

void PrefsDialog::OnAdvanced( wxCommandEvent& event )
{
    wxCommandEvent newevent( wxEVT_USER_FIRST, Advanced_Event );
    newevent.SetInt( event.GetInt() );

    prefs_tree->AddPendingEvent( newevent );
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
    b_advanced = VLC_FALSE;

    root_item = AddRoot( wxT("") );

    /* List the plugins */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list ) return;

    /*
     * Build a tree of the main options
     */
    ConfigTreeData *config_data = new ConfigTreeData;
    config_data->i_object_id = GENERAL_ID;
    config_data->psz_help = wraptext( GENERAL_HELP, 72 , ISUTF8 );
    config_data->psz_section = strdup( GENERAL_TITLE );
    general_item = AppendItem( root_item, wxU(_("General settings")),
                                -1, -1, config_data );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_module->psz_object_name, "main" ) )
            break;
    }
    if( i_index < p_list->i_count )
    {
        /* We found the main module */

        /* Enumerate config categories and store a reference so we can
         * generate their config panel them when it is asked by the user. */
        p_item = p_module->p_config;

        if( p_item ) do
        {
            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
                ConfigTreeData *config_data = new ConfigTreeData;
                config_data->psz_section = strdup(p_item->psz_text);
                if( p_item->psz_longtext )
                {
                    config_data->psz_help =
                        wraptext( p_item->psz_longtext, 72 , ISUTF8 );
                }
                else
                {
                    config_data->psz_help = NULL;
                }
                config_data->i_object_id = p_module->i_object_id;

                /* Add the category to the tree */
                AppendItem( general_item, wxU(p_item->psz_text),
                            -1, -1, config_data );
                break;
            }
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        SortChildren( general_item );
    }


    /*
     * Build a tree of all the plugins
     */
    config_data = new ConfigTreeData;
    config_data->i_object_id = PLUGIN_ID;
    config_data->psz_help = wraptext( PLUGIN_HELP, 72, ISUTF8 );
    config_data->psz_section = strdup( PLUGIN_TITLE );
    plugins_item = AppendItem( root_item, wxU(_("Modules")),
                        -1, -1,config_data );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;

        /* Exclude the main module */
        if( !strcmp( p_module->psz_object_name, "main" ) )
            continue;

        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */
        if( p_module->b_submodule )
            p_item = ((module_t *)p_module->p_parent)->p_config;
        else
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
            if( !GetItemText(capability_item).Cmp(
                    wxU(p_module->psz_capability ) ) )
            {
                break;
            }
            capability_item = GetNextChild( plugins_item, cookie );
        }

        if( i_child_index == GetChildrenCount( plugins_item, FALSE ) &&
            p_module->psz_capability && *p_module->psz_capability )
        {
            /* We didn't find it, add it */
            ConfigTreeData *config_data = new ConfigTreeData;
            config_data->psz_section =
                wraptext( GetCapabilityHelp( p_module->psz_capability , 1 ),
                          72, ISUTF8 );
            config_data->psz_help =
                wraptext( GetCapabilityHelp( p_module->psz_capability , 2 ),
                          72, ISUTF8 );
            config_data->i_object_id = CAPABILITY_ID;
            capability_item = AppendItem( plugins_item,
                                          wxU(p_module->psz_capability),
                                          -1,-1,config_data );
        }

        /* Add the plugin to the tree */
        ConfigTreeData *config_data = new ConfigTreeData;
        config_data->b_submodule = p_module->b_submodule;
        config_data->i_object_id = p_module->b_submodule ?
            ((module_t *)p_module->p_parent)->i_object_id :
            p_module->i_object_id;
        config_data->psz_help = NULL;
        AppendItem( capability_item, wxU(p_module->psz_object_name), -1, -1,
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
        SortChildren( capability_item );
        capability_item = GetNextChild( plugins_item, cookie );
    }

    /* Clean-up everything */
    vlc_list_release( p_list );

    p_sizer->Add( this, 1, wxEXPAND | wxALL, 0 );
    p_sizer->Layout();

    /* Update Tree Ctrl */
#ifndef WIN32 /* Workaround a bug in win32 implementation */
    SelectItem( GetFirstChild( root_item, cookie ) );
#endif

    Expand( general_item );
}

PrefsTreeCtrl::~PrefsTreeCtrl()
{
}

void PrefsTreeCtrl::ApplyChanges()
{
    long cookie, cookie2;
    ConfigTreeData *config_data;

    /* Apply changes to the main module */
    wxTreeItemId item = GetFirstChild( general_item, cookie );
    for( size_t i_child_index = 0;
         i_child_index < GetChildrenCount( general_item, FALSE );
         i_child_index++ )
    {
        config_data = (ConfigTreeData *)GetItemData( item );
        if( config_data && config_data->panel )
        {
            config_data->panel->ApplyChanges();
        }

        item = GetNextChild( general_item, cookie );
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
            if( config_data && config_data->panel )
            {
                config_data->panel->ApplyChanges();
            }

            item2 = GetNextChild( item, cookie2 );
        }

        item = GetNextChild( plugins_item, cookie );
    }
}

void PrefsTreeCtrl::CleanChanges()
{
    long cookie, cookie2;
    ConfigTreeData *config_data;

    config_data = !GetSelection() ? NULL :
        FindModuleConfig( (ConfigTreeData *)GetItemData( GetSelection() ) );
    if( config_data  )
    {
        config_data->panel->Hide();
        p_sizer->Remove( config_data->panel );
    }

    /* Clean changes for the main module */
    wxTreeItemId item = GetFirstChild( general_item, cookie );
    for( size_t i_child_index = 0;
         i_child_index < GetChildrenCount( general_item, FALSE );
         i_child_index++ )
    {
        config_data = (ConfigTreeData *)GetItemData( item );
        if( config_data && config_data->panel )
        {
            delete config_data->panel;
            config_data->panel = NULL;
        }

        item = GetNextChild( general_item, cookie );
    }

    /* Clean changes for the plugins */
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

            if( config_data && config_data->panel )
            {
                delete config_data->panel;
                config_data->panel = NULL;
            }

            item2 = GetNextChild( item, cookie2 );
        }

        item = GetNextChild( plugins_item, cookie );
    }

    if( GetSelection() )
    {
        wxTreeEvent event;
        OnSelectTreeItem( event );
    }
}

ConfigTreeData *PrefsTreeCtrl::FindModuleConfig( ConfigTreeData *config_data )
{
    /* We need this complexity because submodules don't have their own config
     * options. They use the parent module ones. */

    if( !config_data || !config_data->b_submodule )
    {
        return config_data;
    }

    long cookie, cookie2;
    ConfigTreeData *config_new;
    wxTreeItemId item = GetFirstChild( plugins_item, cookie );
    for( size_t i_child_index = 0;
         i_child_index < GetChildrenCount( plugins_item, FALSE );
         i_child_index++ )
    {
        wxTreeItemId item2 = GetFirstChild( item, cookie2 );
        for( size_t i_child_index = 0;
             i_child_index < GetChildrenCount( item, FALSE );
             i_child_index++ )
        {
            config_new = (ConfigTreeData *)GetItemData( item2 );
            if( config_new && !config_new->b_submodule &&
                config_new->i_object_id == config_data->i_object_id )
            {
                return config_new;
            }

            item2 = GetNextChild( item, cookie2 );
        }

        item = GetNextChild( plugins_item, cookie );
    }

    /* Found nothing */
    return NULL;
}

void PrefsTreeCtrl::OnSelectTreeItem( wxTreeEvent& event )
{
    ConfigTreeData *config_data = NULL;

    if( event.GetOldItem() )
        config_data = FindModuleConfig( (ConfigTreeData *)GetItemData(
                                        event.GetOldItem() ) );
    if( config_data && config_data->panel )
    {
        config_data->panel->Hide();
        p_sizer->Remove( config_data->panel );
    }

    /* Don't use event.GetItem() because we also send fake events */
    config_data = FindModuleConfig( (ConfigTreeData *)GetItemData(
                                    GetSelection() ) );
    if( config_data )
    {
        if( !config_data->panel )
        {
            /* The panel hasn't been created yet. Let's do it. */
            config_data->panel =
                new PrefsPanel( p_parent, p_intf, p_prefs_dialog,
                                config_data->i_object_id,
                                config_data->psz_section,
                                config_data->psz_help );
            config_data->panel->SwitchAdvanced( b_advanced );
        }
        else
        {
            config_data->panel->SwitchAdvanced( b_advanced );
            config_data->panel->Show();
        }

        p_sizer->Add( config_data->panel, 3, wxEXPAND | wxALL, 0 );
        p_sizer->Layout();
    }
}

void PrefsTreeCtrl::OnAdvanced( wxCommandEvent& event )
{
    b_advanced = event.GetInt();

    ConfigTreeData *config_data = !GetSelection() ? NULL :
        FindModuleConfig( (ConfigTreeData *)GetItemData( GetSelection() ) );
    if( config_data  )
    {
        config_data->panel->Hide();
        p_sizer->Remove( config_data->panel );
    }

    if( GetSelection() )
    {
        wxTreeEvent event;
        OnSelectTreeItem( event );
    }
}

/*****************************************************************************
 * PrefsPanel class definition.
 *****************************************************************************/
PrefsPanel::PrefsPanel( wxWindow* parent, intf_thread_t *_p_intf,
                        PrefsDialog *_p_prefs_dialog,
                        int i_object_id, char *psz_section, char *psz_help )
  :  wxPanel( parent, -1, wxDefaultPosition, wxDefaultSize )
{
    module_config_t *p_item;

    wxStaticText *label;
    wxStaticText *help;
    wxArrayString array;

    module_t *p_module = NULL;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog =_p_prefs_dialog,

    b_advanced = VLC_TRUE;
    SetAutoLayout( TRUE );
    Hide();

    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );


    if( i_object_id == PLUGIN_ID || i_object_id == GENERAL_ID ||
        i_object_id == CAPABILITY_ID )
    {
        label = new wxStaticText( this, -1,wxU(_( psz_section )));
        wxFont heading_font = label->GetFont();
        heading_font.SetPointSize( heading_font.GetPointSize() + 5 );
        label->SetFont( heading_font );
        sizer->Add( label, 0, wxEXPAND | wxLEFT, 10 );
        sizer->Add( new wxStaticLine( this, 0 ), 0,
                    wxEXPAND | wxLEFT | wxRIGHT, 2 );

        help = new wxStaticText( this, -1, wxU(_( psz_help ) ) );
        sizer->Add( help ,0 ,wxEXPAND | wxALL, 5 );

        config_sizer = NULL; config_window = NULL;
    }
    else
    {
        /* Get a pointer to the module */
        p_module = (module_t *)vlc_object_get( p_intf,  i_object_id );
        if( p_module->i_object_type != VLC_OBJECT_MODULE )
        {
            /* 0OOoo something went really bad */
            return;
        }

        /* Enumerate config options and add corresponding config boxes
         * (submodules don't have config options, they are stored in the
         *  parent module) */
        if( p_module->b_submodule )
            p_item = ((module_t *)p_module->p_parent)->p_config;
        else
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
        label = new wxStaticText( this, -1,
                                  wxU(_(psz_section ? p_item->psz_text :
                                  p_module->psz_longname )));
        wxFont heading_font = label->GetFont();
        heading_font.SetPointSize( heading_font.GetPointSize() + 5 );
        label->SetFont( heading_font );
        sizer->Add( label, 0, wxEXPAND | wxLEFT, 10 );
        sizer->Add( new wxStaticLine( this, 0 ), 0,
                    wxEXPAND | wxLEFT | wxRIGHT, 2 );

        /* Now put all the config options into a scrolled window */
        config_sizer = new wxBoxSizer( wxVERTICAL );
        config_window = new wxScrolledWindow( this, -1, wxDefaultPosition,
            wxDefaultSize, wxBORDER_NONE | wxHSCROLL | wxVSCROLL );
        config_window->SetAutoLayout( TRUE );
        config_window->SetScrollRate( 5, 5 );

        if( p_item ) do
        {
            /* If a category has been specified, check we finished the job */
            if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY &&
                strcmp( psz_section, p_item->psz_text ) )
                break;

            ConfigControl *control =
                CreateConfigControl( VLC_OBJECT(p_intf),
                                     p_item, config_window );

            /* Don't add items that were not recognized */
            if( control == NULL ) continue;

            /* Add the config data to our array so we can keep a trace of it */
            config_array.Add( control );

            config_sizer->Add( control, 0, wxEXPAND | wxALL, 2 );
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        config_sizer->Layout();
        config_window->SetSizer( config_sizer );
        sizer->Add( config_window, 1, wxEXPAND | wxALL, 5 );

        /* And at last put a useful help string if available */
        if( psz_help && *psz_help )
        {
            sizer->Add( new wxStaticLine( this, 0 ), 0,
                        wxEXPAND | wxLEFT | wxRIGHT, 2 );
            help = new wxStaticText( this, -1, wxU(_(psz_help)),
                                     wxDefaultPosition, wxDefaultSize,
                                     wxALIGN_LEFT,
                                     wxT("") );
            sizer->Add( help ,0 ,wxEXPAND | wxALL, 5 );
        }

        vlc_object_release( p_module );
    }
    sizer->Layout();
    SetSizer( sizer );
    Show();
}

void PrefsPanel::ApplyChanges()
{
    vlc_value_t val;

    for( size_t i = 0; i < config_array.GetCount(); i++ )
    {
        ConfigControl *control = config_array.Item(i);

        switch( control->GetType() )
        {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:
        case CONFIG_ITEM_MODULE:
            config_PutPsz( p_intf, control->GetName().mb_str(),
                           control->GetPszValue().mb_str() );
            break;
        case CONFIG_ITEM_KEY:
            /* So you don't need to restart to have the changes take effect */
            val.i_int = control->GetIntValue();
            var_Set( p_intf->p_vlc, control->GetName().mb_str(), val );
        case CONFIG_ITEM_INTEGER:
        case CONFIG_ITEM_BOOL:
            config_PutInt( p_intf, control->GetName().mb_str(),
                           control->GetIntValue() );
            break;
        case CONFIG_ITEM_FLOAT:
            config_PutFloat( p_intf, control->GetName().mb_str(),
                             control->GetFloatValue() );
            break;
        }
    }
}

void PrefsPanel::SwitchAdvanced( vlc_bool_t b_new_advanced )
{
    if( b_advanced == b_new_advanced ) return;

    if( config_sizer && config_window )
    {
        b_advanced = b_new_advanced;

        for( size_t i = 0; i < config_array.GetCount(); i++ )
        {
            ConfigControl *control = config_array.Item(i);
            if( control->IsAdvanced() )
            {
                control->Show( b_advanced );
                config_sizer->Show( control, b_advanced );
            }
        }

        config_sizer->Layout();
        config_window->FitInside();
        config_window->Refresh();
    }
    return;
}
