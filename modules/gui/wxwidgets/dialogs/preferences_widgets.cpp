/*****************************************************************************
 * preferences_widgets.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include "wxwidgets.hpp"
#include "preferences_widgets.h"
#include <vlc_keys.h>
#include <vlc_config_cat.h>

#include <wx/statline.h>
#include <wx/spinctrl.h>

/*****************************************************************************
 * CreateConfigControl wrapper
 *****************************************************************************/
ConfigControl *CreateConfigControl( vlc_object_t *p_this,
                                    module_config_t *p_item, wxWindow *parent )
{
    ConfigControl *p_control = NULL;

    /*Skip deprecated options */
    if( p_item->psz_current )
    {
        return NULL;
    }

    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new ModuleConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_MODULE_CAT:
        p_control = new ModuleCatConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = new ModuleListCatConfigControl( p_this, p_item, parent );
        break;

    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
        {
            p_control = new StringConfigControl( p_this, p_item, parent );
        }
        else
        {
            p_control = new StringListConfigControl( p_this, p_item, parent );
        }
        break;

    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
        p_control = new FileConfigControl( p_this, p_item, parent );
        break;

    case CONFIG_ITEM_INTEGER:
        if( p_item->i_list )
        {
            p_control = new IntegerListConfigControl( p_this, p_item, parent );
        }
        else if( p_item->i_min != 0 || p_item->i_max != 0 )
        {
            p_control = new RangedIntConfigControl( p_this, p_item, parent );
        }
        else
        {
            p_control = new IntegerConfigControl( p_this, p_item, parent );
        }
        break;

    case CONFIG_ITEM_KEY:
        p_control = new KeyConfigControl( p_this, p_item, parent );
        break;

    case CONFIG_ITEM_FLOAT:
        p_control = new FloatConfigControl( p_this, p_item, parent );
        break;

    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent );
        break;

    case CONFIG_SECTION:
        p_control = new SectionConfigControl( p_this, p_item, parent );
        break;

    default:
        break;
    }

    return p_control;
}

/*****************************************************************************
 * ConfigControl implementation
 *****************************************************************************/
ConfigControl::ConfigControl( vlc_object_t *_p_this,
                              module_config_t *p_item, wxWindow *parent )
  : wxPanel( parent ), p_this( _p_this ),
    pf_update_callback( NULL ), p_update_data( NULL ),
    name( wxU(p_item->psz_name) ), i_type( p_item->i_type ),
    b_advanced( p_item->b_advanced )

{
    sizer = new wxBoxSizer( wxHORIZONTAL );
}

ConfigControl::~ConfigControl()
{
}

wxSizer *ConfigControl::Sizer()
{
    return sizer;
}

wxString ConfigControl::GetName()
{
    return name;
}

int ConfigControl::GetType()
{
    return i_type;
}

vlc_bool_t ConfigControl::IsAdvanced()
{
    return b_advanced;
}

void ConfigControl::SetUpdateCallback( void (*p_callback)( void * ),
                                             void *p_data )
{
    pf_update_callback = p_callback;
    p_update_data = p_data;
}

void ConfigControl::OnUpdate( wxCommandEvent& WXUNUSED(event) )
{
    if( pf_update_callback )
    {
        pf_update_callback( p_update_data );
    }
}

void ConfigControl::OnUpdateScroll( wxScrollEvent& WXUNUSED(event) )
{
    wxCommandEvent cevent;
    OnUpdate(cevent);
}


/*****************************************************************************
 * KeyConfigControl implementation
 *****************************************************************************/
wxString *KeyConfigControl::m_keysList = NULL;

KeyConfigControl::KeyConfigControl( vlc_object_t *p_this,
                                    module_config_t *p_item, wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    // Number of keys descriptions
    unsigned int i_keys = sizeof(vlc_keys)/sizeof(key_descriptor_t);

    // Init the keys decriptions array
    if( m_keysList == NULL )
    {
        m_keysList = new wxString[i_keys];
        for( unsigned int i = 0; i < i_keys; i++ )
        {
            m_keysList[i] = wxU(vlc_keys[i].psz_key_string);
        }
    }

    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    alt = new wxCheckBox( this, -1, wxU(_("Alt")) );
    alt->SetValue( p_item->i_value & KEY_MODIFIER_ALT );
    ctrl = new wxCheckBox( this, -1, wxU(_("Ctrl")) );
    ctrl->SetValue( p_item->i_value & KEY_MODIFIER_CTRL );
    shift = new wxCheckBox( this, -1, wxU(_("Shift")) );
    shift->SetValue( p_item->i_value & KEY_MODIFIER_SHIFT );
    combo = new wxComboBox( this, -1, wxT(""), wxDefaultPosition,
                            wxDefaultSize, i_keys, m_keysList,
                            wxCB_READONLY );
    for( unsigned int i = 0; i < i_keys; i++ )
    {
        combo->SetClientData( i, (void*)vlc_keys[i].i_key_code );
        if( (unsigned int)vlc_keys[i].i_key_code ==
            ( ((unsigned int)p_item->i_value) & ~KEY_MODIFIER ) )
        {
            combo->SetSelection( i );
            combo->SetValue( wxU(_(vlc_keys[i].psz_key_string)) );
        }
    }

    sizer->Add( label, 2, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5 );
    sizer->Add( alt,   1, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5 );
    sizer->Add( ctrl,  1, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5 );
    sizer->Add( shift, 1, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5 );
    sizer->Add( combo, 2, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

KeyConfigControl::~KeyConfigControl()
{
    if( m_keysList )
    {
        delete[] m_keysList;
        m_keysList = NULL;
    }
}

int KeyConfigControl::GetIntValue()
{
    int result = 0;
    if( alt->IsChecked() )
    {
        result |= KEY_MODIFIER_ALT;
    }
    if( ctrl->IsChecked() )
    {
        result |= KEY_MODIFIER_CTRL;
    }
    if( shift->IsChecked() )
    {
        result |= KEY_MODIFIER_SHIFT;
    }
    int selected = combo->GetSelection();
    if( selected != -1 )
    {
        result |= (int)combo->GetClientData( selected );
    }
    return result;
}

/*****************************************************************************
 * ModuleConfigControl implementation
 *****************************************************************************/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *p_this,
                                          module_config_t *p_item,
                                          wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    combo = new wxComboBox( this, -1, wxL2U(p_item->psz_value),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY | wxCB_SORT );

    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    combo->Append( wxU(_("Default")), (void *)NULL );
    combo->SetSelection( 0 );
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
        {
            combo->Append( wxU(p_parser->psz_longname),
                           p_parser->psz_object_name );
            if( p_item->psz_value && !strcmp(p_item->psz_value,
                                             p_parser->psz_object_name) )
                combo->SetValue( wxU(p_parser->psz_longname) );
        }
    }
    vlc_list_release( p_list );

    combo->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

ModuleCatConfigControl::~ModuleCatConfigControl()
{
    ;
}

wxString ModuleCatConfigControl::GetPszValue()
{
    return wxU( (char *)combo->GetClientData( combo->GetSelection() ));
}

/*****************************************************************************
 * ModuleCatConfigControl implementation
 *****************************************************************************/
ModuleCatConfigControl::ModuleCatConfigControl( vlc_object_t *p_this,
                                                module_config_t *p_item,
                                                wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    combo = new wxComboBox( this, -1, wxL2U(p_item->psz_value),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY | wxCB_SORT );

    combo->Append( wxU(_("Default")), (void *)NULL );
    combo->SetSelection( 0 );

    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for(  int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, "main" ) )
              continue;

        module_config_t *p_config = p_parser->p_config;
        if( p_config ) do
        {
            /* Hack: required subcategory is stored in i_min */
            if( p_config->i_type == CONFIG_SUBCATEGORY &&
                p_config->i_value == p_item->i_min )
            {
                combo->Append( wxU(p_parser->psz_longname),
                                   p_parser->psz_object_name );
                if( p_item->psz_value && !strcmp(p_item->psz_value,
                                        p_parser->psz_object_name) )
                combo->SetValue( wxU(p_parser->psz_longname) );
            }
        } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
    }
    vlc_list_release( p_list );

    combo->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

ModuleConfigControl::~ModuleConfigControl()
{
    ;
}

wxString ModuleConfigControl::GetPszValue()
{
    return wxU( (char *)combo->GetClientData( combo->GetSelection() ));
}


/*****************************************************************************
 * ModuleListCatonfigControl implementation
 *****************************************************************************/
BEGIN_EVENT_TABLE(ModuleListCatConfigControl, wxPanel)
    EVT_CHECKBOX( wxID_HIGHEST , ModuleListCatConfigControl::OnUpdate )
END_EVENT_TABLE()


ModuleListCatConfigControl::ModuleListCatConfigControl( vlc_object_t *p_this,
                                                     module_config_t *p_item,
                                                     wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    delete sizer;
    sizer = new wxBoxSizer( wxVERTICAL );
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label );

    text = new wxTextCtrl( this, -1, wxU(p_item->psz_value),
                           wxDefaultPosition,wxSize( 300, 20 ) );


    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for(  int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, "main" ) )
              continue;

        module_config_t *p_config = p_parser->p_config;
        if( p_config ) do
        {
            /* Hack: required subcategory is stored in i_min */
            if( p_config->i_type == CONFIG_SUBCATEGORY &&
                p_config->i_value == p_item->i_min )
            {
                moduleCheckBox *mc = new moduleCheckBox;
                mc->checkbox = new wxCheckBox( this, wxID_HIGHEST,
                                               wxU(p_parser->psz_longname));
                mc->psz_module = strdup( p_parser->psz_object_name );
                pp_checkboxes.push_back( mc );

                if( p_item->psz_value &&
                    strstr( p_item->psz_value, p_parser->psz_object_name ) )
                {
                    mc->checkbox->SetValue( true );
                }
                sizer->Add( mc->checkbox );
            }
        } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
    }
    vlc_list_release( p_list );

    text->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add(text, 0, wxEXPAND|wxALL, 5 );

    sizer->Add (new wxStaticText( this, -1, wxU( vlc_wraptext( _("Select modules that you want. To get more advanced control, you can also modify the resulting chain by yourself") , 72, 1 ) ) ) );

    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

ModuleListCatConfigControl::~ModuleListCatConfigControl()
{
    ;
}

wxString ModuleListCatConfigControl::GetPszValue()
{
    return text->GetValue() ;
}

void  ModuleListCatConfigControl::OnUpdate( wxCommandEvent &event )
{
    bool b_waschecked = false;
    wxString newtext =  text->GetValue();

    for( unsigned int i = 0 ; i< pp_checkboxes.size() ; i++ )
    {
        b_waschecked = newtext.Find( wxU(pp_checkboxes[i]->psz_module)) != -1 ;
        /* For some reasons, ^^ doesn't compile :( */
        if( (pp_checkboxes[i]->checkbox->IsChecked() && ! b_waschecked )||
            (! pp_checkboxes[i]->checkbox->IsChecked() && b_waschecked) )
        {
            if( b_waschecked )
            {
                /* Maybe not the clest solution */
                if( ! newtext.Replace(wxString(wxT(":"))
                                      +wxU(pp_checkboxes[i]->psz_module),
                                      wxT("")))
                {
                    if( ! newtext.Replace(wxString(wxU(pp_checkboxes[i]->psz_module))
                                           + wxT(":"),wxT("")))
            {
                        newtext.Replace(wxU(pp_checkboxes[i]->psz_module),wxU(""));
                    }
                }
            }
            else
            {
                if( newtext.Len() == 0 )
                {
                    newtext = wxU(pp_checkboxes[i]->psz_module);
                }
                else
                {
                    newtext += wxU( ":" );
                    newtext += wxU(pp_checkboxes[i]->psz_module);
                }
            }
        }
    }
    text->SetValue( newtext );
}

/*****************************************************************************
 * StringConfigControl implementation
 *****************************************************************************/
StringConfigControl::StringConfigControl( vlc_object_t *p_this,
                                          module_config_t *p_item,
                                          wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    textctrl = new wxTextCtrl( this, -1,
                               wxL2U(p_item->psz_value),
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxTE_PROCESS_ENTER);
    textctrl->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( textctrl, 1, wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

StringConfigControl::~StringConfigControl()
{
    ;
}

wxString StringConfigControl::GetPszValue()
{
    return textctrl->GetValue();
}

BEGIN_EVENT_TABLE(StringConfigControl, wxPanel)
    /* Text events */
    EVT_TEXT(-1, StringConfigControl::OnUpdate)
END_EVENT_TABLE()

/*****************************************************************************
 * StringListConfigControl implementation
 *****************************************************************************/
StringListConfigControl::StringListConfigControl( vlc_object_t *p_this,
                                                  module_config_t *p_item,
                                                  wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    psz_default_value = p_item->psz_value;
    if( psz_default_value ) psz_default_value = strdup( psz_default_value );

    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    combo = new wxComboBox( this, -1, wxT(""),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY );
    UpdateCombo( p_item );

    combo->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    for( int i = 0; i < p_item->i_action; i++ )
    {
        wxButton *button =
            new wxButton( this, wxID_HIGHEST+i,
                          wxU(p_item->ppsz_action_text[i]) );
        sizer->Add( button, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    }

    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

StringListConfigControl::~StringListConfigControl()
{
    if( psz_default_value ) free( psz_default_value );
}

void StringListConfigControl::UpdateCombo( module_config_t *p_item )
{
    vlc_bool_t b_found = VLC_FALSE;
    int i_index;

    /* build a list of available options */
    for( i_index = 0; i_index < p_item->i_list; i_index++ )
    {
        combo->Append( ( p_item->ppsz_list_text &&
                         p_item->ppsz_list_text[i_index] ) ?
                       wxU(p_item->ppsz_list_text[i_index]) :
                       wxL2U(p_item->ppsz_list[i_index]) );
        combo->SetClientData( i_index, (void *)p_item->ppsz_list[i_index] );
        if( ( p_item->psz_value &&
              !strcmp( p_item->psz_value, p_item->ppsz_list[i_index] ) ) ||
             ( !p_item->psz_value && !*p_item->ppsz_list[i_index] ) )
        {
            combo->SetSelection( i_index );
            combo->SetValue( ( p_item->ppsz_list_text &&
                               p_item->ppsz_list_text[i_index] ) ?
                             wxU(p_item->ppsz_list_text[i_index]) :
                             wxL2U(p_item->ppsz_list[i_index]) );
            b_found = VLC_TRUE;
        }
    }

    if( p_item->psz_value && !b_found )
    {
        /* Add custom entry to list */
        combo->Append( wxL2U(p_item->psz_value) );
        combo->SetClientData( i_index, (void *)psz_default_value );
        combo->SetSelection( i_index );
        combo->SetValue( wxL2U(p_item->psz_value) );
    }
}

BEGIN_EVENT_TABLE(StringListConfigControl, wxPanel)
    /* Button events */
    EVT_BUTTON(-1, StringListConfigControl::OnAction)

    /* Text events */
    EVT_TEXT(-1, StringListConfigControl::OnUpdate)
END_EVENT_TABLE()

void StringListConfigControl::OnAction( wxCommandEvent& event )
{
    int i_action = event.GetId() - wxID_HIGHEST;

    module_config_t *p_item = config_FindConfig( p_this, GetName().mb_str() );
    if( !p_item ) return;

    if( i_action < 0 || i_action >= p_item->i_action ) return;

    vlc_value_t val;
    wxString value = GetPszValue();
    *((const char **)&val.psz_string) = value.mb_str();
    p_item->ppf_action[i_action]( p_this, GetName().mb_str(), val, val, 0 );

    if( p_item->b_dirty )
    {
        combo->Clear();
        UpdateCombo( p_item );
        p_item->b_dirty = VLC_FALSE;
    }
}

wxString StringListConfigControl::GetPszValue()
{
    int selected = combo->GetSelection();
    if( selected != -1 )
    {
        return wxL2U((char *)combo->GetClientData( selected ));
    }
    return wxString();
}

/*****************************************************************************
 * FileConfigControl implementation
 *****************************************************************************/
FileConfigControl::FileConfigControl( vlc_object_t *p_this,
                                      module_config_t *p_item,
                                      wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    directory = p_item->i_type == CONFIG_ITEM_DIRECTORY;
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    textctrl = new wxTextCtrl( this, -1,
                               wxL2U(p_item->psz_value),
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxTE_PROCESS_ENTER);
    textctrl->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( textctrl, 1, wxALL, 5 );
    browse = new wxButton( this, wxID_HIGHEST, wxU(_("Browse...")) );
    sizer->Add( browse, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

BEGIN_EVENT_TABLE(FileConfigControl, wxPanel)
    /* Button events */
    EVT_BUTTON(wxID_HIGHEST, FileConfigControl::OnBrowse)
END_EVENT_TABLE()

void FileConfigControl::OnBrowse( wxCommandEvent& event )
{
    if( directory )
    {
        wxDirDialog dialog( this, wxU(_("Choose directory")) );

        if( dialog.ShowModal() == wxID_OK )
        {
            textctrl->SetValue( dialog.GetPath() );
        }
    }
    else
    {
        wxFileDialog dialog( this, wxU(_("Choose file")),
                             wxT(""), wxT(""), wxT("*.*"),
#if defined( __WXMSW__ )
                             wxOPEN
#else
                             wxOPEN | wxSAVE
#endif
                           );
        if( dialog.ShowModal() == wxID_OK )
        {
            textctrl->SetValue( dialog.GetPath() );
        }
    }
}

FileConfigControl::~FileConfigControl()
{
    ;
}

wxString FileConfigControl::GetPszValue()
{
    return textctrl->GetValue();
}

/*****************************************************************************
 * IntegerConfigControl implementation
 *****************************************************************************/
BEGIN_EVENT_TABLE(IntegerConfigControl, wxPanel)
    EVT_TEXT(-1, IntegerConfigControl::OnUpdate)
    EVT_COMMAND_SCROLL(-1, IntegerConfigControl::OnUpdateScroll)
END_EVENT_TABLE()

IntegerConfigControl::IntegerConfigControl( vlc_object_t *p_this,
                                            module_config_t *p_item,
                                            wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    spin = new wxSpinCtrl( this, -1,
                           wxString::Format(wxT("%d"),
                                            p_item->i_value),
                           wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS,
                           -100000000, 100000000, p_item->i_value);
    spin->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
    i_value = p_item->i_value;
}

IntegerConfigControl::~IntegerConfigControl()
{
    ;
}

int IntegerConfigControl::GetIntValue()
{
    /* We avoid using GetValue because of a recursion bug with wxSpinCtrl with
     * wxGTK. */
    return i_value; //spin->GetValue();
}

void IntegerConfigControl::OnUpdate( wxCommandEvent &event )
{
    i_value = event.GetInt();
    ConfigControl::OnUpdate( event );
}
void IntegerConfigControl::OnUpdateScroll( wxScrollEvent &event )
{
    wxCommandEvent cevent;
    cevent.SetInt(event.GetPosition());
    OnUpdate(cevent);
}

/*****************************************************************************
 * IntegerListConfigControl implementation
 *****************************************************************************/
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *p_this,
                                                    module_config_t *p_item,
                                                    wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    combo = new wxComboBox( this, -1, wxT(""),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY );

    UpdateCombo( p_item );

    combo->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

IntegerListConfigControl::~IntegerListConfigControl()
{
}

void IntegerListConfigControl::UpdateCombo( module_config_t *p_item )
{
    /* build a list of available options */
    for( int i_index = 0; i_index < p_item->i_list; i_index++ )
    {
        if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
        {
            combo->Append( wxU(p_item->ppsz_list_text[i_index]) );
        }
        else
        {
            combo->Append( wxString::Format(wxT("%i"),
                                            p_item->pi_list[i_index]) );
        }
        combo->SetClientData( i_index, (void *)p_item->pi_list[i_index] );
        if( p_item->i_value == p_item->pi_list[i_index] )
        {
            combo->SetSelection( i_index );
            if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
            {
                combo->SetValue( wxU(p_item->ppsz_list_text[i_index]) );
            }
            else
            {
                combo->SetValue( wxString::Format(wxT("%i"),
                                                  p_item->pi_list[i_index]) );
            }
        }
    }
}

BEGIN_EVENT_TABLE(IntegerListConfigControl, wxPanel)
    /* Button events */
    EVT_BUTTON(-1, IntegerListConfigControl::OnAction)

    /* Update events */
    EVT_TEXT(-1, IntegerListConfigControl::OnUpdate)
END_EVENT_TABLE()

void IntegerListConfigControl::OnAction( wxCommandEvent& event )
{
    int i_action = event.GetId() - wxID_HIGHEST;

    module_config_t *p_item;
    p_item = config_FindConfig( p_this, GetName().mb_str() );
    if( !p_item ) return;

    if( i_action < 0 || i_action >= p_item->i_action ) return;

    vlc_value_t val;
    val.i_int = GetIntValue();
    p_item->ppf_action[i_action]( p_this, GetName().mb_str(), val, val, 0 );

    if( p_item->b_dirty )
    {
        combo->Clear();
        UpdateCombo( p_item );
        p_item->b_dirty = VLC_FALSE;
    }
}

int IntegerListConfigControl::GetIntValue()
{
    int selected = combo->GetSelection();
    if( selected != -1 )
    {
        return (int)combo->GetClientData( selected );
    }
    return -1;
}

/*****************************************************************************
 * RangedIntConfigControl implementation
 *****************************************************************************/
BEGIN_EVENT_TABLE(RangedIntConfigControl, wxPanel)
    EVT_COMMAND_SCROLL(-1, RangedIntConfigControl::OnUpdateScroll)
END_EVENT_TABLE()

RangedIntConfigControl::RangedIntConfigControl( vlc_object_t *p_this,
                                                module_config_t *p_item,
                                                wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    slider = new wxSlider( this, -1, p_item->i_value, p_item->i_min,
                           p_item->i_max, wxDefaultPosition, wxDefaultSize,
                           wxSL_LABELS | wxSL_HORIZONTAL );
    slider->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( slider, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

RangedIntConfigControl::~RangedIntConfigControl()
{
    ;
}

int RangedIntConfigControl::GetIntValue()
{
    return slider->GetValue();
}


/*****************************************************************************
 * FloatConfigControl implementation
 *****************************************************************************/
BEGIN_EVENT_TABLE(FloatConfigControl, wxPanel)
    EVT_TEXT(-1, FloatConfigControl::OnUpdate)
END_EVENT_TABLE()

FloatConfigControl::FloatConfigControl( vlc_object_t *p_this,
                                        module_config_t *p_item,
                                        wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    textctrl = new wxTextCtrl( this, -1,
                               wxString::Format(wxT("%f"),
                                                p_item->f_value),
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_PROCESS_ENTER );
    textctrl->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( textctrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

FloatConfigControl::~FloatConfigControl()
{
    ;
}

float FloatConfigControl::GetFloatValue()
{
    float f_value;
    if( (wxSscanf(textctrl->GetValue(), wxT("%f"), &f_value) == 1) )
        return f_value;
    else return 0.0;
}

/*****************************************************************************
 * BoolConfigControl implementation
 *****************************************************************************/
BEGIN_EVENT_TABLE(BoolConfigControl, wxPanel)
    EVT_CHECKBOX(-1, BoolConfigControl::OnUpdate)
END_EVENT_TABLE()

BoolConfigControl::BoolConfigControl( vlc_object_t *p_this,
                                      module_config_t *p_item,
                                      wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    checkbox = new wxCheckBox( this, -1, wxU(p_item->psz_text) );
    if( p_item->i_value ) checkbox->SetValue(TRUE);
    checkbox->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( checkbox, 0, wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

BoolConfigControl::~BoolConfigControl()
{
    ;
}

int BoolConfigControl::GetIntValue()
{
    if( checkbox->IsChecked() ) return 1;
    else return 0;
}

/*****************************************************************************
 * SectionConfigControl implementation
 *****************************************************************************/
SectionConfigControl::SectionConfigControl( vlc_object_t *p_this,
                                            module_config_t *p_item,
                                            wxWindow *parent )
  : ConfigControl( p_this, p_item, parent )
{
    delete sizer;
    sizer = new wxBoxSizer( wxVERTICAL );
    sizer->Add( new wxStaticText( this, -1, wxU( p_item->psz_text ) ) );
    sizer->Add( new wxStaticLine( this, -1 ), 0, wxEXPAND, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

SectionConfigControl::~SectionConfigControl()
{
    ;
}
