/*****************************************************************************
 * preferences_widgets.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: preferences_widgets.cpp,v 1.2 2003/10/19 23:38:09 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Sigmund Augdal <sigmunau@idi.ntnu.no>
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

/*****************************************************************************
 * ConfigControl implementation
 *****************************************************************************/
ConfigControl::ConfigControl( wxWindow *parent ): wxPanel( parent )
{
    sizer = new wxBoxSizer( wxHORIZONTAL );
    i_value = 0;
    f_value = 0;
    psz_value = NULL;
}

ConfigControl::~ConfigControl()
{
    if( psz_value ) free( psz_value );
}

wxSizer *ConfigControl::Sizer()
{
    return sizer;
}

int ConfigControl::GetIntValue()
{
    return i_value;
}

float ConfigControl::GetFloatValue()
{
    return f_value;
}

const char *ConfigControl::GetPszValue()
{
    return psz_value;
}

/*****************************************************************************
 * KeyConfigControl implementation
 *****************************************************************************/
KeyConfigControl::KeyConfigControl( module_config_t *p_item, wxWindow *parent )
  : ConfigControl( parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    alt = new wxCheckBox( this, -1, wxU(_("Alt")) );
    alt->SetValue( p_item->i_value & KEY_MODIFIER_ALT );
    ctrl = new wxCheckBox( this, -1, wxU(_("Ctrl")) );
    ctrl->SetValue( p_item->i_value & KEY_MODIFIER_CTRL );
    shift = new wxCheckBox( this, -1, wxU(_("Shift")) );
    shift->SetValue( p_item->i_value & KEY_MODIFIER_SHIFT );
    combo = new wxComboBox( this, -1, wxU("f"), wxDefaultPosition,
                            wxDefaultSize, 0, NULL,
                            wxCB_READONLY | wxCB_SORT );
    for( unsigned int i = 0; i < sizeof(keys)/sizeof(key_descriptor_s); i++ )
    {
        /* HPReg says casting the int to void * is fine */
        combo->Append( wxU(_(keys[i].psz_key_string)),
                       (void*)keys[i].i_key_code );
        if( keys[i].i_key_code == ( p_item->i_value & ~KEY_MODIFIER ) )
        {
            combo->SetSelection( i );
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
    ;
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
ModuleConfigControl::ModuleConfigControl( intf_thread_t *p_intf,
                                          module_config_t *p_item,
                                          wxWindow *parent )
  : ConfigControl( parent )
{
    vlc_list_t *p_list;
    module_t *p_parser;
    
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    combo = new wxComboBox( this, -1, wxU(p_item->psz_value),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY | wxCB_SORT );
    
    /* build a list of available modules */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
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

ModuleConfigControl::~ModuleConfigControl()
{
    ;
}

const char *ModuleConfigControl::GetPszValue()
{
    return combo->GetStringSelection().mb_str();
}

/*****************************************************************************
 * StringConfigControl implementation
 *****************************************************************************/
StringConfigControl::StringConfigControl( module_config_t *p_item,
                                          wxWindow *parent )
  : ConfigControl( parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    textctrl = new wxTextCtrl( this, -1, 
                               wxU(p_item->psz_value),
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

const char *StringConfigControl::GetPszValue()
{
    return textctrl->GetValue().mb_str();
}

/*****************************************************************************
 * StringListConfigControl implementation
 *****************************************************************************/
StringListConfigControl::StringListConfigControl( module_config_t *p_item,
                                                  wxWindow *parent )
  : ConfigControl( parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    combo = new wxComboBox( this, -1, wxU(p_item->psz_value),
                            wxDefaultPosition, wxDefaultSize,
                            0, NULL, wxCB_READONLY|wxCB_SORT );
    
    /* build a list of available options */
    for( int i_index = 0; p_item->ppsz_list[i_index];
         i_index++ )
    {
        combo->Append( wxU(p_item->ppsz_list[i_index]) );
        if( p_item->psz_value && !strcmp( p_item->psz_value,
                                          p_item->ppsz_list[i_index] ) )
            combo->SetSelection( i_index );
    }
    
    if( p_item->psz_value )
        combo->SetValue( wxU(p_item->psz_value) );
    combo->SetToolTip( wxU(p_item->psz_longtext) );
    sizer->Add( combo, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );    
    sizer->Layout();
    this->SetSizerAndFit( sizer );
}

StringListConfigControl::~StringListConfigControl()
{
    ;
}

const char *StringListConfigControl::GetPszValue()
{
    return combo->GetStringSelection().mb_str();
}

/*****************************************************************************
 * FileConfigControl implementation
 *****************************************************************************/
FileConfigControl::FileConfigControl( module_config_t *p_item,
                                      wxWindow *parent )
  : ConfigControl( parent )
{
    directory = p_item->i_type == CONFIG_ITEM_DIRECTORY;
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    sizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    textctrl = new wxTextCtrl( this, -1, 
                               wxU(p_item->psz_value),
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
        wxDirDialog dialog( this, wxU(_("Choose Directory")) );

        if( dialog.ShowModal() == wxID_OK )
        {
            textctrl->SetValue( dialog.GetPath() );      
        }
    }
    else
    {
        wxFileDialog dialog( this, wxU(_("Choose File")),
                             wxT(""), wxT(""), wxT("*.*"),
#if defined( __WXMSW__ )
                             wxOPEN
#else
                             wxOPEN | wxSAVE
#endif
                           );
    }
}

FileConfigControl::~FileConfigControl()
{
    ;
}
    
const char *FileConfigControl::GetPszValue()
{
    return textctrl->GetValue().mb_str();
}

/*****************************************************************************
 * IntegerConfigControl implementation
 *****************************************************************************/
IntegerConfigControl::IntegerConfigControl( module_config_t *p_item,
                                            wxWindow *parent )
  : ConfigControl( parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    spin = new wxSpinCtrl( this, -1,
                           wxString::Format(wxT("%d"),
                                            p_item->i_value),
                           wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS,
                           -16000, 16000, p_item->i_value);
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
 * RangedIntConfigControl implementation
 *****************************************************************************/
RangedIntConfigControl::RangedIntConfigControl( module_config_t *p_item,
                                                wxWindow *parent )
  : ConfigControl( parent )
{
    label = new wxStaticText(this, -1, wxU(p_item->psz_text));
    slider = new wxSlider( this, -1, p_item->i_value, p_item->i_min,
                           p_item->i_max );
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
FloatConfigControl::FloatConfigControl( module_config_t *p_item,
                                        wxWindow *parent )
  : ConfigControl( parent )
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
BoolConfigControl::BoolConfigControl( module_config_t *p_item,
                                      wxWindow *parent )
  : ConfigControl( parent )
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
    if( checkbox->IsChecked() )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
