/*****************************************************************************
 * preferences.cpp: the "Preferences" dialog box
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#include <vcl.h>
#pragma hdrstop

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                                /* strcmp */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "preferences.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma link "CSPIN"
#pragma resource "*.dfm"

extern intf_thread_t *p_intfGlobal;


/****************************************************************************
 * Functions to help components creation
 ****************************************************************************/
__fastcall TGroupBoxPref::TGroupBoxPref( TComponent* Owner,
            module_config_t *p_config_arg ) : TGroupBox( Owner )
{
    p_config = p_config_arg;
    Caption = p_config->psz_text;
}
//---------------------------------------------------------------------------
TListView * __fastcall TGroupBoxPref::CreateListView( TWinControl *Parent,
            int Left, int Width, int Top, int Height, TViewStyle ViewStyle )
{
    TListView *ListView = new TListView( Parent );
    ListView->Parent = Parent;
    ListView->ViewStyle = ViewStyle;
    ListView->Left = Left;
    ListView->Width = Width;
    ListView->Top = Top;
    ListView->Height = Height;
    return ListView;
}
//---------------------------------------------------------------------------
TButton * __fastcall TGroupBoxPref::CreateButton( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption )
{
    TButton *Button = new TButton( Parent );
    Button->Parent = Parent;
    Button->Left = Left;
    Button->Width = Width;
    Button->Top = Top;
    Button->Height = Height;
    Button->Caption = Caption;
    return Button;
}
//---------------------------------------------------------------------------
TCheckBox * __fastcall TGroupBoxPref::CreateCheckBox( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption )
{
    TCheckBox *CheckBox = new TCheckBox( Parent );
    CheckBox->Parent = Parent;
    CheckBox->Left = Left;
    CheckBox->Width = Width;
    CheckBox->Top = Top;
    CheckBox->Height = Height;
    CheckBox->Caption = Caption;
    return CheckBox;
}
//---------------------------------------------------------------------------
TLabel * __fastcall TGroupBoxPref::CreateLabel( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption,
            bool WordWrap )
{
    TLabel *Label = new TLabel( Parent );
    Label->Parent = Parent;
    Label->Caption = Caption;
    Label->Left = Left;
    Label->Width = Width;
    Label->Top = Top;
    Label->Height = Height;
    Label->WordWrap = WordWrap;
    return Label;
}
//---------------------------------------------------------------------------
TEdit * __fastcall TGroupBoxPref::CreateEdit( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Text )
{
    TEdit *Edit = new TEdit( Parent );
    Edit->Parent = Parent;
    Edit->Left = Left;
    Edit->Width = Width;
    Edit->Top = Top;
    Edit->Height = Height;
    Edit->Text = Text;
    return Edit;
}
//---------------------------------------------------------------------------
TCSpinEdit * __fastcall TGroupBoxPref::CreateSpinEdit( TWinControl *Parent,
            int Left, int Width, int Top, int Height,
            long Min, long Max, long Value )
{
    TCSpinEdit *SpinEdit = new TCSpinEdit( Parent );
    SpinEdit->Parent = Parent;
    SpinEdit->Left = Left;
    SpinEdit->Width = Width;
    SpinEdit->Top = Top;
    SpinEdit->Height = Height;
    SpinEdit->MinValue = Min;
    SpinEdit->MaxValue = Max;
    SpinEdit->Value = Value;
    return SpinEdit;
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPref::UpdateChanges()
{
}


/****************************************************************************
 * GroupBox for module management
 ****************************************************************************/
__fastcall TGroupBoxPlugin::TGroupBoxPlugin( TComponent* Owner,
            module_config_t *p_config ) : TGroupBoxPref( Owner, p_config )
{
    /* init listview */
    ListView = CreateListView( this, 16, 164, 24, 160, vsReport );
    ListView->ReadOnly = true;
    ListView->Columns->Add();
    ListView->Columns->Items[0]->Width = 160;
    ListView->Columns->Items[0]->Caption = "Name";//p_config->psz_text;
    ListView->OnSelectItem = ListViewSelectItem;

    /* init description label */
    LabelDesc = CreateLabel( this, 230, 225, 50, 52,
                             p_config->psz_longtext, true );

    /* init hint label */
    LabelHint = CreateLabel( this, 230, 225, 135, 13, "", false );

    /* init configure button */
    ButtonConfig = CreateButton( this, 16, 70, 192, 25, "Configure" );
    ButtonConfig->Enabled = false;
    ButtonConfig->OnClick = ButtonConfigClick;

    /* init select button */
    ButtonSelect = CreateButton( this, 110, 70, 192, 25, "Select" );
    ButtonSelect->OnClick = ButtonSelectClick;

    /* init 'Selected' label */
    LabelSelected = CreateLabel( this, 230, 45, 198, 13, "Selected", false );

    /* init 'Selected' edit */
    Edit = CreateEdit( this, 280, 164, 194, 21, "" );
    vlc_mutex_lock( p_config->p_lock );
    Edit->Text = p_config->psz_value ? p_config->psz_value : "";
    vlc_mutex_unlock( p_config->p_lock );

    Height = 233;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::ListViewSelectItem( TObject *Sender,
        TListItem *Item, bool Selected )
{
    module_t *p_module;
    AnsiString Name;

    Name = Item->Caption;
    if( Name != "" )
    {
        /* look for module 'Name' */
        for( p_module = p_intfGlobal->p_vlc->module_bank.first ;
             p_module != NULL ;
             p_module = p_module->next )
        {
            if( strcmp( p_module->psz_object_name, Name.c_str() ) == 0 )
            {
                ModuleSelected = p_module;
                LabelHint->Caption = p_module->psz_longname ?
                                     p_module->psz_longname : "";
                ButtonConfig->Enabled = p_module->i_config_items ? true : false;

                break;
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::ButtonSelectClick( TObject *Sender )
{
    if( !ModuleSelected ) return;
    Edit->Text = ModuleSelected->psz_object_name;
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::ButtonConfigClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->
                        CreatePreferences( ModuleSelected->psz_object_name );
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::UpdateChanges()
{
    /* XXX: Necessary, since c_str() returns only a temporary pointer... */
    free( p_config->psz_value );
    p_config->psz_value = (char *)malloc( Edit->Text.Length() + 1 );
    strcpy( p_config->psz_value, Edit->Text.c_str() );
}


/****************************************************************************
 * GroupBox for string management
 ****************************************************************************/
__fastcall TGroupBoxString::TGroupBoxString( TComponent* Owner,
            module_config_t *p_config ) : TGroupBoxPref( Owner, p_config )
{
    /* init description label */
    LabelDesc = CreateLabel( this, 230, 225, 24, 26,
                             p_config->psz_longtext, true );

    /* init edit */
    Edit = CreateEdit( this, 16, 164, 24, 21, "" );
    vlc_mutex_lock( p_config->p_lock );
    Edit->Text = p_config->psz_value ? p_config->psz_value : "";
    vlc_mutex_unlock( p_config->p_lock );

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    Edit->Top = Top + ( Height - Edit->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxString::UpdateChanges()
{
    /* XXX: Necessary, since c_str() returns only a temporary pointer... */
    free( p_config->psz_value );
    p_config->psz_value = (char *)malloc( Edit->Text.Length() + 1 );
    strcpy( p_config->psz_value, Edit->Text.c_str() );
}


/****************************************************************************
 * GroupBox for integer management
 ****************************************************************************/
__fastcall TGroupBoxInteger::TGroupBoxInteger( TComponent* Owner,
            module_config_t *p_config ) : TGroupBoxPref( Owner, p_config )
{
    /* init description label */
    LabelDesc = CreateLabel( this, 230, 225, 19, 26,
                             p_config->psz_longtext, true );

    /* init spinedit */
    SpinEdit = CreateSpinEdit( this, 16, 164, 24, 21,
                               -1, 100000, p_config->i_value );

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    SpinEdit->Top = Top + ( Height - SpinEdit->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxInteger::UpdateChanges()
{
    /* Warning: we're casting from long to int */
    p_config->i_value = (int)SpinEdit->Value;
}


/****************************************************************************
 * GroupBox for boolean management
 ****************************************************************************/
__fastcall TGroupBoxBool::TGroupBoxBool( TComponent* Owner,
            module_config_t *p_config ) : TGroupBoxPref( Owner, p_config )
{
    /* init description label */
    LabelDesc = CreateLabel( this, 230, 225, 19, 26,
                             p_config->psz_longtext, true );

    /* init checkbox */
    CheckBox = CreateCheckBox( this, 16, 184, 28, 17, p_config->psz_text );
    CheckBox->Checked = p_config->i_value;

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    CheckBox->Top = Top + ( Height - CheckBox->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxBool::UpdateChanges()
{
    p_config->i_value = CheckBox->Checked ? 1 : 0;
}


/****************************************************************************
 * Callbacks for the dialog
 ****************************************************************************/
//---------------------------------------------------------------------------
__fastcall TPreferencesDlg::TPreferencesDlg( TComponent* Owner )
        : TForm( Owner )
{
    Icon = p_intfGlobal->p_sys->p_window->Icon;
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::FormClose( TObject *Sender,
      TCloseAction &Action )
{
    Action = caHide;
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::FormShow( TObject *Sender )
{
/*
    p_intfGlobal->p_sys->p_window->MenuPreferences->Checked = true;
    p_intfGlobal->p_sys->p_window->PopupPreferences->Checked = true;
*/
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::FormHide( TObject *Sender )
{
/*
    p_intfGlobal->p_sys->p_window->MenuPreferences->Checked = false;
    p_intfGlobal->p_sys->p_window->PopupPreferences->Checked = false;
*/
}


/****************************************************************************
 * CreateConfigDialog: dynamically creates the configuration dialog
 * box from all the configuration data provided by the selected module.
 ****************************************************************************/
#define ADD_PANEL                               \
{                                               \
            Panel = new TPanel( this );         \
            Panel->Parent = ScrollBox;          \
            Panel->Caption = "";                \
            Panel->BevelOuter = bvNone;         \
            Panel->Height = 12;                 \
}

void __fastcall TPreferencesDlg::CreateConfigDialog( char *psz_module_name )
{
    module_t           *p_module, *p_module_plugins;
    module_config_t    *p_item;
    int                 i_pages, i_ctrl;
    
    TTabSheet          *TabSheet;
    TScrollBox         *ScrollBox;
    TPanel             *Panel;
    TGroupBoxPlugin    *GroupBoxPlugin;
    TGroupBoxString    *GroupBoxString;
    TGroupBoxInteger   *GroupBoxInteger;
    TGroupBoxBool      *GroupBoxBool;
    TListItem          *ListItem;

    /* Look for the selected module */
    for( p_module = p_intfGlobal->p_vlc->module_bank.first ; p_module != NULL ;
         p_module = p_module->next )
    {
        if( psz_module_name
             && !strcmp( psz_module_name, p_module->psz_object_name ) )
        {
            break;
        }
    }
    if( !p_module ) return;

    /*
     * We found it, now we can start building its configuration interface
     */

    /* Enumerate config options and add corresponding config boxes */
    p_item = p_module->p_config;
    do
    {
        switch( p_item->i_type )
        {
        case CONFIG_HINT_CATEGORY:

            /* create a new tabsheet. */
            TabSheet = new TTabSheet( this );
            TabSheet->PageControl = PageControlPref;
            TabSheet->Caption = p_item->psz_text;
            TabSheet->Visible = true;

            /* pack a scrollbox into the tabsheet */
            ScrollBox = new TScrollBox( this );
            ScrollBox->Parent = TabSheet;
            ScrollBox->Align = alClient;
            ScrollBox->BorderStyle = bsNone;
            ScrollBox->HorzScrollBar->Tracking = true;
            ScrollBox->VertScrollBar->Tracking = true;

            break;

        case CONFIG_ITEM_MODULE:

            /* add new groupbox for the config option */
            GroupBoxPlugin = new TGroupBoxPlugin( this, p_item );
            GroupBoxPlugin->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            /* build a list of available plugins */
            for( p_module_plugins = p_intfGlobal->p_vlc->module_bank.first ;
                 p_module_plugins != NULL ;
                 p_module_plugins = p_module_plugins->next )
            {
                if( p_module_plugins->i_capabilities &
                    ( 1 << p_item->i_value ) )
                {
                    ListItem = GroupBoxPlugin->ListView->Items->Add();
                    ListItem->Caption = p_module_plugins->psz_object_name;
                }
            }

            break;

        case CONFIG_ITEM_FILE:

        case CONFIG_ITEM_STRING:

            /* add new groupbox for the config option */
            GroupBoxString = new TGroupBoxString( this, p_item );
            GroupBoxString->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;

        case CONFIG_ITEM_INTEGER:

            /* add new groupbox for the config option */
            GroupBoxInteger = new TGroupBoxInteger( this, p_item );
            GroupBoxInteger->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;

        case CONFIG_ITEM_BOOL:

            /* add new groupbox for the config option */
            GroupBoxBool = new TGroupBoxBool( this, p_item );
            GroupBoxBool->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;
        }
        
        p_item++;
    }
    while( p_item->i_type != CONFIG_HINT_END );

    /* Reorder groupboxes inside the tabsheets */
    for( i_pages = 0; i_pages < PageControlPref->PageCount; i_pages++ )
    {
        /* get scrollbox from the tabsheet */
        ScrollBox = (TScrollBox *)PageControlPref->Pages[i_pages]->Controls[0];

        for( i_ctrl = ScrollBox->ControlCount - 1; i_ctrl >= 0 ; i_ctrl-- )
        {
            ScrollBox->Controls[i_ctrl]->Align = alTop;
        }
    }

    /* set active tabsheet
     * FIXME: i don't know why, but both lines are necessary */
    PageControlPref->ActivePageIndex = 1;
    PageControlPref->ActivePageIndex = 0;
}
#undef ADD_PANEL
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonOkClick( TObject *Sender )
{
    ButtonApplyClick( Sender );
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonApplyClick( TObject *Sender )
{
    TScrollBox *ScrollBox;
    TGroupBoxPref *GroupBox;
    int i, j;

    for( i = 0; i < PageControlPref->PageCount; i++ )
    {
        /* get scrollbox from the tabsheet */
        ScrollBox = (TScrollBox *)PageControlPref->Pages[i]->Controls[0];

        for( j = 0; j < ScrollBox->ControlCount ; j++ )
        {
            /* skip the panels */
            if( ScrollBox->Controls[j]->InheritsFrom( __classid( TGroupBoxPref ) ) )
            {
                GroupBox = (TGroupBoxPref *)ScrollBox->Controls[j];
                GroupBox->UpdateChanges();
                SaveValue( GroupBox->p_config );
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonSaveClick( TObject *Sender )
{
    ButtonApplyClick( Sender );
    config_SaveConfigFile( p_intfGlobal, NULL );
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonCancelClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::SaveValue( module_config_t *p_config )
{
    switch( p_config->i_type )
    {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_MODULE:
            config_PutPsz( p_intfGlobal, p_config->psz_name,
                           *p_config->psz_value ? p_config->psz_value : NULL );
            break;
        case CONFIG_ITEM_INTEGER:
        case CONFIG_ITEM_BOOL:
            config_PutInt( p_intfGlobal, p_config->psz_name,
                           p_config->i_value );
            break;
    }
}
//---------------------------------------------------------------------------

