/*****************************************************************************
 * preferences_widgets.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <string.h>                                            /* strerror() */
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "wince.h"

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <commctrl.h>

#include "preferences_widgets.h"

/*****************************************************************************
 * CreateConfigControl wrapper
 *****************************************************************************/
ConfigControl *CreateConfigControl( vlc_object_t *p_this,
                                    module_config_t *p_item,
                                    HWND parent, HINSTANCE hInst,
                                    int *py_pos )
{
    ConfigControl *p_control = NULL;

    if( p_item->psz_current )
    {
        return NULL;
    }
    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new ModuleConfigControl( p_this, p_item, parent, hInst, py_pos );
        break;

    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
        {
            p_control = new StringConfigControl( p_this, p_item, parent, hInst, py_pos );
        }
        /*else
        {
            p_control = new StringListConfigControl( p_this, p_item, parent, hInst, py_pos );
        }*/
        break;
/*
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
        p_control = new FileConfigControl( p_this, p_item, parent, hInst, py_pos );
        break;

    case CONFIG_ITEM_INTEGER:
        if( p_item->i_list )
        {
            p_control = new IntegerListConfigControl( p_this, p_item, parent, hInst, py_pos );
        }
        else if( p_item->i_min != 0 || p_item->i_max != 0 )
        {
            p_control = new RangedIntConfigControl( p_this, p_item, parent, hInst, py_pos );
        }
        else
        {
            p_control = new IntegerConfigControl( p_this, p_item, parent, hInst, py_pos );
        }
        break;
*/
    case CONFIG_ITEM_KEY:
        p_control = new KeyConfigControl( p_this, p_item, parent, hInst, py_pos  );
        break;

    case CONFIG_ITEM_FLOAT:
        p_control = new FloatConfigControl( p_this, p_item, parent, hInst, py_pos );
        break;

    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent, hInst, py_pos );
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
                              module_config_t *p_item,
                              HWND parent, HINSTANCE hInst )
  : p_this( _p_this ), pf_update_callback( NULL ), p_update_data( NULL ),
    parent( parent ), name( p_item->psz_name ), i_type( p_item->i_type ),
    b_advanced( p_item->b_advanced )

{
    /*sizer = new wxBoxSizer( wxHORIZONTAL );*/
}

ConfigControl::~ConfigControl()
{
}

/*wxSizer *ConfigControl::Sizer()
{
    return sizer;
}*/

char *ConfigControl::GetName()
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

void ConfigControl::OnUpdate( UINT event )
{
    if( pf_update_callback )
    {
        pf_update_callback( p_update_data );
    }
}

/*****************************************************************************
 * KeyConfigControl implementation
 *****************************************************************************/
string *KeyConfigControl::m_keysList = NULL;

KeyConfigControl::KeyConfigControl( vlc_object_t *p_this,
                                    module_config_t *p_item,
                                    HWND parent, HINSTANCE hInst,
                                    int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    // Number of keys descriptions
    unsigned int i_keys = sizeof(vlc_keys)/sizeof(key_descriptor_t);

    // Init the keys decriptions array
    if( m_keysList == NULL )
    {
        m_keysList = new string[i_keys];
        for( unsigned int i = 0; i < i_keys; i++ )
        {
            m_keysList[i] = vlc_keys[i].psz_key_string;
        }
    }

    label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                WS_CHILD | WS_VISIBLE | SS_LEFT, 5, *py_pos, 200, 15,
                parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    alt = CreateWindow( _T("BUTTON"), _T("Alt"),
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                        20, *py_pos, 15, 15, parent, NULL, hInst, NULL );
    Button_SetCheck( alt, p_item->i_value & KEY_MODIFIER_ALT ? BST_CHECKED :
                     BST_UNCHECKED );

    alt_label = CreateWindow( _T("STATIC"), _T("Alt"),
                WS_CHILD | WS_VISIBLE | SS_LEFT, 20 + 15 + 5, *py_pos, 30, 15,
                parent, NULL, hInst, NULL );

    ctrl = CreateWindow( _T("BUTTON"), _T("Ctrl"),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                20 + 15 + 5 + 30 + 5, *py_pos, 15, 15,
                parent, NULL, hInst, NULL );
    Button_SetCheck( ctrl, p_item->i_value & KEY_MODIFIER_CTRL ? BST_CHECKED :
                     BST_UNCHECKED );

    ctrl_label = CreateWindow( _T("STATIC"), _T("Ctrl"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20 + 15 + 5 + 30 + 5 + 15 + 5, *py_pos, 30, 15,
                parent, NULL, hInst, NULL );

    shift = CreateWindow( _T("BUTTON"), _T("Shift"),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                20 + 15 + 5 + 2*(30 + 5) + 15 + 5, *py_pos, 15, 15,
                parent, NULL, hInst, NULL );
    Button_SetCheck( shift, p_item->i_value & KEY_MODIFIER_SHIFT ?
                     BST_CHECKED : BST_UNCHECKED );

    shift_label = CreateWindow( _T("STATIC"), _T("Shift"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20 + 15 + 5 + 2*(30 + 5) + 2*(15 + 5), *py_pos, 30, 15,
                parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    combo = CreateWindow( _T("COMBOBOX"), _T(""),
                WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST |
                CBS_SORT | WS_VSCROLL, 20, *py_pos, 130, 5*15 + 6,
                parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    for( unsigned int i = 0; i < i_keys ; i++ )
    {
        ComboBox_AddString( combo, _FROMMB(m_keysList[i].c_str()) );
        ComboBox_SetItemData( combo, i, (void*)vlc_keys[i].i_key_code );
        if( (unsigned int)vlc_keys[i].i_key_code ==
            ( ((unsigned int)p_item->i_value) & ~KEY_MODIFIER ) )
        {
            ComboBox_SetCurSel( combo, i );
            ComboBox_SetText( combo, _FROMMB(m_keysList[i].c_str()) );
        }
    }
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
    if( Button_GetCheck( alt ) )
    {
        result |= KEY_MODIFIER_ALT;
    }
    if( Button_GetCheck( ctrl ) )
    {
        result |= KEY_MODIFIER_CTRL;
    }
    if( Button_GetCheck( shift ) )
    {
        result |= KEY_MODIFIER_SHIFT;
    }
    int selected = ComboBox_GetCurSel( combo );
    if( selected != -1 )
    {
        result |= (int)ComboBox_GetItemData( combo, selected );
    }
    return result;
}

/*****************************************************************************
 * ModuleConfigControl implementation
 *****************************************************************************/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *p_this,
                                          module_config_t *p_item,
                                          HWND parent, HINSTANCE hInst,
                                          int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          5, *py_pos, 200, 15,
                          parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    combo = CreateWindow( _T("COMBOBOX"), _T(""),
                          WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL |
                          CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL,
                          20, *py_pos, 180, 5*15 + 6,
                          parent, NULL, hInst, NULL);

    *py_pos += 15 + 10;

    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    ComboBox_AddString( combo, _T("Default") );
    ComboBox_SetItemData( combo, 0, (void *)NULL );
    ComboBox_SetCurSel( combo, 0 );
    //ComboBox_SetText( combo, _T("Default") );
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
        {
            ComboBox_AddString( combo, _FROMMB(p_parser->psz_longname) );
            ComboBox_SetItemData( combo, i_index,
                                  (void*)p_parser->psz_object_name );
            if( p_item->psz_value && !strcmp(p_item->psz_value,
                                             p_parser->psz_object_name) )
            {
                ComboBox_SetCurSel( combo, i_index );
                //ComboBox_SetText( combo, _FROMMB(p_parser->psz_longname) );
            }
        }
    }
    vlc_list_release( p_list );
}

ModuleConfigControl::~ModuleConfigControl()
{
    ;
}

char *ModuleConfigControl::GetPszValue()
{
    int selected = ComboBox_GetCurSel( combo );
    if( selected != -1 )
        return (char *)ComboBox_GetItemData( combo, selected );
    else return NULL;
}

/*****************************************************************************
 * StringConfigControl implementation
 *****************************************************************************/
StringConfigControl::StringConfigControl( vlc_object_t *p_this,
                                          module_config_t *p_item,
                                          HWND parent, HINSTANCE hInst,
                                          int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          5, *py_pos, 200, 15,
                          parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    textctrl = CreateWindow( _T("EDIT"), p_item->psz_value ?
                             _FROMMB(p_item->psz_value) : _T(""),
                             WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT |
                             ES_AUTOHSCROLL, 20, *py_pos - 3, 180, 15 + 3,
                             parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;
}

StringConfigControl::~StringConfigControl()
{
    ;
}

char *StringConfigControl::GetPszValue()
{
    int i_size;
    char *psz_result;
    TCHAR *psz_string;

    i_size = Edit_GetTextLength( textctrl );
    psz_string = (TCHAR *)malloc( (i_size + 1) * sizeof(TCHAR) );
    Edit_GetText( textctrl, psz_string, i_size + 1 );
    psz_result = strdup( _TOMB(psz_string) );
    free( psz_string );
    return psz_result;
}

#if 0
/*****************************************************************************
 * StringListConfigControl implementation
 *****************************************************************************/
StringListConfigControl::StringListConfigControl( vlc_object_t *p_this,
                                                  module_config_t *p_item,
                                                  HWND parent, HINSTANCE hInst,
                                                  int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
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
}

void StringListConfigControl::UpdateCombo( module_config_t *p_item )
{
    /* build a list of available options */
    for( int i_index = 0; i_index < p_item->i_list; i_index++ )
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
        }
    }
}

BEGIN_EVENT_TABLE(StringListConfigControl, wxPanel)
    /* Button events */
    EVT_BUTTON(-1, StringListConfigControl::OnAction)

    /* Text events */
    EVT__T(-1, StringListConfigControl::OnUpdate)
END_EVENT_TABLE()

void StringListConfigControl::OnAction( wxCommandEvent& event )
{
    int i_action = event.GetId() - wxID_HIGHEST;

    module_config_t *p_item = config_FindConfig( p_this, GetName().mb_str() );
    if( !p_item ) return;

    if( i_action < 0 || i_action >= p_item->i_action ) return;

    vlc_value_t val;
    wxString value = GetPszValue();
    (const char *)val.psz_string = value.mb_str();
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
                                      HWND parent, HINSTANCE hInst,
                                      int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
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
IntegerConfigControl::IntegerConfigControl( vlc_object_t *p_this,
                                            module_config_t *p_item,
                                            HWND parent, HINSTANCE hInst,
                                            int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    spin = new wxSpinCtrl( this, -1,
                           wxString::Format(wxT("%d"),
                                            p_item->i_value),
                           wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS,
                           -10000000, 10000000, p_item->i_value);
    spin->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Add( spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

IntegerConfigControl::~IntegerConfigControl()
{
    ;
}

int IntegerConfigControl::GetIntValue()
{
    return spin->GetValue();
}

/*****************************************************************************
 * IntegerListConfigControl implementation
 *****************************************************************************/
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *p_this,
                                                    module_config_t *p_item,
                                                    HWND parent,
                                                    HINSTANCE hInst,
                                                    int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
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
RangedIntConfigControl::RangedIntConfigControl( vlc_object_t *p_this,
                                                module_config_t *p_item, 
                                                HWND parent, HINSTANCE hInst,
                                                int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
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

#endif
/*****************************************************************************
 * FloatConfigControl implementation
 *****************************************************************************/
FloatConfigControl::FloatConfigControl( vlc_object_t *p_this,
                                        module_config_t *p_item,
                                        HWND parent, HINSTANCE hInst,
                                        int *py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          5, *py_pos, 200, 15,
                          parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;

    TCHAR psz_string[100];
    _stprintf( psz_string, _T("%f"), p_item->f_value );
    textctrl = CreateWindow( _T("EDIT"), psz_string,
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_RIGHT | ES_AUTOHSCROLL,
        20, *py_pos - 3, 70, 15 + 3, parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;
}

FloatConfigControl::~FloatConfigControl()
{
    ;
}

float FloatConfigControl::GetFloatValue()
{
    float f_value;

    int i_size = Edit_GetTextLength( textctrl );  
    TCHAR *psz_string = (TCHAR *)malloc( (i_size + 1) * sizeof(TCHAR) );
    Edit_GetText( textctrl, psz_string, i_size + 1 );

    if( _tscanf( psz_string, _T("%f"), &f_value ) == 1 )
    {
        free( psz_string );
        return f_value;
    }

    free( psz_string );
    return 0.0;
}

/*****************************************************************************
 * BoolConfigControl implementation
 *****************************************************************************/
BoolConfigControl::BoolConfigControl( vlc_object_t *p_this,
                                      module_config_t *p_item, HWND parent,
                                      HINSTANCE hInst, int * py_pos )
  : ConfigControl( p_this, p_item, parent, hInst )
{
    checkbox = CreateWindow( _T("BUTTON"), _T(""),
                             WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                             5, *py_pos, 15, 15,
                             parent, NULL, hInst, NULL );
    Button_SetCheck( checkbox, p_item->i_value ? BST_CHECKED : BST_UNCHECKED );

    checkbox_label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   5 + 15 + 5, *py_pos, 180, 15,
                                   parent, NULL, hInst, NULL );

    *py_pos += 15 + 10;
}

BoolConfigControl::~BoolConfigControl()
{
    ;
}

int BoolConfigControl::GetIntValue()
{
    if( Button_GetCheck( checkbox ) ) return 1;
    else return 0;
}
