/*****************************************************************************
 * preferences.h: the "Preferences" dialog box
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

#include <videolan/vlc.h>

#include "interface.h"

#include "preferences.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern struct intf_thread_s *p_intfGlobal;

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
TUpDown * __fastcall TGroupBoxPref::CreateUpDown( TWinControl *Parent,
            int Min, int Max, int Position, bool Thousands )
{
    TUpDown *UpDown = new TUpDown( Parent );
    UpDown->Parent = Parent;
    UpDown->Min = Min;
    UpDown->Max = Max;
    UpDown->Position = Position;
    UpDown->Thousands = Thousands;
    return UpDown;
}


/****************************************************************************
 * GroupBox for plugin management
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
    Edit->OnChange = EditChange;

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
        /* look for plugin 'Name' */
        for( p_module = p_module_bank->first ;
             p_module != NULL ;
             p_module = p_module->next )
        {
            if( !strcmp( p_module->psz_name, Name.c_str() ) )
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
    Edit->Text = ModuleSelected->psz_name;
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::ButtonConfigClick( TObject *Sender )
{
    /* FIWME: TODO */
}
//---------------------------------------------------------------------------
void __fastcall TGroupBoxPlugin::EditChange( TObject *Sender )
{
    TEdit *Edit = (TEdit *)Sender;
    p_config->psz_value = Edit->Text.c_str();
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
    Edit->OnChange = EditChange;

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    Edit->Top = Top + ( Height - Edit->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxString::EditChange( TObject *Sender )
{
    TEdit *Edit = (TEdit *)Sender;
    p_config->psz_value = Edit->Text.c_str();
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

    /* init edit */
    Edit = CreateEdit( this, 16, 148, 24, 21, "" );
    Edit->OnChange = EditChange;

    /* init updown */
    UpDown = CreateUpDown( this, -1, 32767, p_config->i_value, false );
    UpDown->Associate = Edit;

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    Edit->Top = Top + ( Height - Edit->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxInteger::EditChange( TObject *Sender )
{
    TEdit *Edit = (TEdit *)Sender;
    p_config->i_value = StrToInt( Edit->Text );
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
    CheckBox->OnClick = CheckBoxClick;

    /* vertical alignment */
    Height = LabelDesc->Height + 24;
    LabelDesc->Top = Top + ( Height - LabelDesc->Height ) / 2 + 4;
    CheckBox->Top = Top + ( Height - CheckBox->Height ) / 2 + 4;
};
//---------------------------------------------------------------------------
void __fastcall TGroupBoxBool::CheckBoxClick( TObject *Sender )
{
    TCheckBox *CheckBox = (TCheckBox *)Sender;
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
    p_intfGlobal->p_sys->p_window->MenuPreferences->Checked = true;
    p_intfGlobal->p_sys->p_window->PopupPreferences->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuPreferences->Checked = false;
    p_intfGlobal->p_sys->p_window->PopupPreferences->Checked = false;
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
    bool config_dialog;
    module_t *p_module;
    module_t *p_module_plugins;
    int i, j;

    TTabSheet *TabSheet;
    TScrollBox *ScrollBox;
    TPanel *Panel;
    TGroupBoxPlugin *GroupBoxPlugin;
    TGroupBoxString *GroupBoxString;
    TGroupBoxInteger *GroupBoxInteger;
    TGroupBoxBool *GroupBoxBool;
    TListItem *ListItem;

    /* Check if the dialog box is already opened, if so this will save us
     * quite a bit of work. (the interface will be destroyed when you actually
     * close the main window, but remember that it is only hidden if you
     * clicked on the action buttons). This trick also allows us not to
     * duplicate identical dialog windows. */

    /* FIXME: we must find a way of really checking whether the dialog
     * box is already opened */
    config_dialog = false;

    if( config_dialog )
    {
        /* Yeah it was open */
        Show();
        return;
    }

    /* Look for the selected module */
    for( p_module = p_module_bank->first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && !strcmp( psz_module_name, p_module->psz_name ) )
            break;
    }
    if( !p_module ) return;

    /*
     * We found it, now we can start building its configuration interface
     */

    /* Enumerate config options and add corresponding config boxes */
    for( i = 0; i < p_module->i_config_lines; i++ )
    {
        switch( p_module->p_config[i].i_type )
        {
        case MODULE_CONFIG_HINT_CATEGORY:

            /* create a new tabsheet. */
            TabSheet = new TTabSheet( this );
            TabSheet->PageControl = PageControlPref;
            TabSheet->Caption = p_module->p_config[i].psz_text;
            TabSheet->Visible = true;

            /* pack a scrollbox into the tabsheet */
            ScrollBox = new TScrollBox( this );
            ScrollBox->Parent = TabSheet;
            ScrollBox->Align = alClient;
            ScrollBox->BorderStyle = bsNone;
            ScrollBox->HorzScrollBar->Tracking = true;
            ScrollBox->VertScrollBar->Tracking = true;

            break;

        case MODULE_CONFIG_ITEM_PLUGIN:

            /* add new groupbox for the config option */
            GroupBoxPlugin = new TGroupBoxPlugin( this, &p_module->p_config[i] );
            GroupBoxPlugin->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            /* build a list of available plugins */
            for( p_module_plugins = p_module_bank->first ;
                 p_module_plugins != NULL ;
                 p_module_plugins = p_module_plugins->next )
            {
                if( p_module_plugins->i_capabilities &
                    ( 1 << p_module->p_config[i].i_value ) )
                {
                    ListItem = GroupBoxPlugin->ListView->Items->Add();
                    ListItem->Caption = p_module_plugins->psz_name;
                }
            }

            break;

        case MODULE_CONFIG_ITEM_FILE:

        case MODULE_CONFIG_ITEM_STRING:

            /* add new groupbox for the config option */
            GroupBoxString = new TGroupBoxString( this, &p_module->p_config[i] );
            GroupBoxString->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;

        case MODULE_CONFIG_ITEM_INTEGER:

            /* add new groupbox for the config option */
            GroupBoxInteger = new TGroupBoxInteger( this, &p_module->p_config[i] );
            GroupBoxInteger->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;

        case MODULE_CONFIG_ITEM_BOOL:

            /* add new groupbox for the config option */
            GroupBoxBool = new TGroupBoxBool( this, &p_module->p_config[i] );
            GroupBoxBool->Parent = ScrollBox;

            /* add panel as separator */
            ADD_PANEL;

            break;
        }
    }

    /* Reorder groupboxes inside the tabsheets */
    for( i = 0; i < PageControlPref->PageCount; i++ )
    {
        /* get scrollbox from the tabsheet */
        ScrollBox = (TScrollBox *)PageControlPref->Pages[i]->Controls[0];

        for( j = ScrollBox->ControlCount - 1; j >= 0 ; j-- )
        {
            ScrollBox->Controls[j]->Align = alTop;
        }
    }

    /* set active tabsheet
     * FIXME: i don't know why, but both lines are necessary */
    PageControlPref->ActivePageIndex = 1;
    PageControlPref->ActivePageIndex = 0;

    /* Ok, job done successfully. Let's keep a reference to the dialog box*/
    /* FIXME: TODO */

    /* we want this ref to be destroyed if the object is destroyed */
    /* FIXME: TODO */
    
    Show();
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
                SaveValue( GroupBox->p_config );
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonSaveClick( TObject *Sender )
{
    ButtonApplyClick( Sender );
    config_SaveConfigFile( NULL );
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
        case MODULE_CONFIG_ITEM_STRING:
        case MODULE_CONFIG_ITEM_FILE:
        case MODULE_CONFIG_ITEM_PLUGIN:
            config_PutPszVariable( p_config->psz_name, p_config->psz_value );
            break;
        case MODULE_CONFIG_ITEM_INTEGER:
        case MODULE_CONFIG_ITEM_BOOL:
            config_PutIntVariable( p_config->psz_name, p_config->i_value );
            break;
    }
}
//---------------------------------------------------------------------------

