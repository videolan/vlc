/*****************************************************************************
 * preferences.cpp: the "Preferences" dialog box
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
 *          Boris Dores <babal@via.ecp.fr>
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

/****************************************************************************
 * A THintWindow with a limited width
 ****************************************************************************/
void __fastcall TNarrowHintWindow::ActivateHint( const Windows::TRect &Rect,
    const System::AnsiString AHint )
{
    TRect NarrowRect = CalcHintRect( 300, AHint, NULL );
    NarrowRect.Left = Rect.Left;
    NarrowRect.Top = Rect.Top;
    NarrowRect.Right += Rect.Left;
    NarrowRect.Bottom += Rect.Top;
    THintWindow::ActivateHint( NarrowRect, AHint );
}


/****************************************************************************
 * Just a wrapper to embed an AnsiString into a TObject
 ****************************************************************************/
__fastcall TObjectString::TObjectString( char * String )
{
    FString = AnsiString( String );
}
//---------------------------------------------------------------------------
AnsiString __fastcall TObjectString::String()
{
    return FString;
}


/****************************************************************************
 * A TCheckListBox that automatically disposes any TObject
 * associated with the string items
 ****************************************************************************/
__fastcall TCleanCheckListBox::~TCleanCheckListBox()
{
    for( int i = 0 ; i < Items->Count ; i++ )
    {
        if( Items->Objects[i] != NULL )
            delete Items->Objects[i];
    }
}


/****************************************************************************
 * Functions to help components creation
 ****************************************************************************/
__fastcall TPanelPref::TPanelPref( TComponent* Owner,
    module_config_t *_p_config, intf_thread_t *_p_intf ) : TPanel( Owner )
{
    p_intf = _p_intf;
    p_config = _p_config;
    BevelInner = bvNone;
    BevelOuter = bvNone;
    BorderStyle = bsNone;
}
//---------------------------------------------------------------------------
TCleanCheckListBox * __fastcall TPanelPref::CreateCleanCheckListBox(
    TWinControl *Parent, int Left, int Width, int Top, int Height )
{
    TCleanCheckListBox *CleanCheckListBox = new TCleanCheckListBox( Parent );
    CleanCheckListBox->Parent = Parent;
    CleanCheckListBox->Left = Left;
    CleanCheckListBox->Width = Width;
    CleanCheckListBox->Top = Top;
    CleanCheckListBox->Height = Height;
    return CleanCheckListBox;
}
//---------------------------------------------------------------------------
TButton * __fastcall TPanelPref::CreateButton( TWinControl *Parent,
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
TCheckBox * __fastcall TPanelPref::CreateCheckBox( TWinControl *Parent,
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
TLabel * __fastcall TPanelPref::CreateLabel( TWinControl *Parent,
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
TEdit * __fastcall TPanelPref::CreateEdit( TWinControl *Parent,
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
TCSpinEdit * __fastcall TPanelPref::CreateSpinEdit( TWinControl *Parent,
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

#define LIBWIN32_PREFSIZE_VPAD                4
#define LIBWIN32_PREFSIZE_HPAD                4
#define LIBWIN32_PREFSIZE_LEFT                16
#define LIBWIN32_PREFSIZE_EDIT_LEFT           (LIBWIN32_PREFSIZE_LEFT+32)
#define LIBWIN32_PREFSIZE_WIDTH               375
#define LIBWIN32_PREFSIZE_EDIT_WIDTH          (LIBWIN32_PREFSIZE_WIDTH-32)
#define LIBWIN32_PREFSIZE_BUTTON_WIDTH        150
#define LIBWIN32_PREFSIZE_SPINEDIT_WIDTH      100
#define LIBWIN32_PREFSIZE_RIGHT               (LIBWIN32_PREFSIZE_LEFT+LIBWIN32_PREFSIZE_WIDTH)
#define LIBWIN32_PREFSIZE_BUTTON_HEIGHT       25
#define LIBWIN32_PREFSIZE_LABEL_HEIGHT        26
#define LIBWIN32_PREFSIZE_CHECKLISTBOX_HEIGHT 120
#define LIBWIN32_PREFSIZE_EDIT_HEIGHT         21
#define LIBWIN32_PREFSIZE_CHECKBOX_HEIGHT     17
#define LIBWIN32_PREFSIZE_SPINEDIT_HEIGHT     21

/****************************************************************************
 * Panel for module management
 ****************************************************************************/
__fastcall TPanelPlugin::TPanelPlugin( TComponent* Owner,
        module_config_t *p_config, intf_thread_t *_p_intf,
        bool b_multi_plugins ) : TPanelPref( Owner, p_config, _p_intf )
{
    this->b_multi_plugins = b_multi_plugins;

    /* init configure button */
    ButtonConfig = CreateButton( this,
            LIBWIN32_PREFSIZE_RIGHT - LIBWIN32_PREFSIZE_BUTTON_WIDTH,
            LIBWIN32_PREFSIZE_BUTTON_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_BUTTON_HEIGHT,
            "Configure..." );
    ButtonConfig->Enabled = false;
    ButtonConfig->OnClick = ButtonConfigClick;

    /* init label */
    AnsiString Text = AnsiString( p_config->psz_text ) + ":";
    Label = CreateLabel( this,
            LIBWIN32_PREFSIZE_LEFT,
            LIBWIN32_PREFSIZE_RIGHT - LIBWIN32_PREFSIZE_BUTTON_WIDTH
             - LIBWIN32_PREFSIZE_HPAD,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_LABEL_HEIGHT,
            Text.c_str(), true );

    /* vertical alignement */
    if ( ButtonConfig->Height > Label->Height )
        Label->Top += ( ButtonConfig->Height - Label->Height ) / 2;
    else
        ButtonConfig->Top += ( Label->Height - ButtonConfig->Height ) / 2;

    /* init checklistbox */
    CleanCheckListBox = CreateCleanCheckListBox( this,
            LIBWIN32_PREFSIZE_EDIT_LEFT,
            LIBWIN32_PREFSIZE_EDIT_WIDTH,
            max( Label->Top + Label->Height , ButtonConfig->Top
                 + ButtonConfig->Height ) + LIBWIN32_PREFSIZE_VPAD,
                 LIBWIN32_PREFSIZE_CHECKLISTBOX_HEIGHT );
    CleanCheckListBox->OnClick = CheckListBoxClick;
    CleanCheckListBox->OnClickCheck = CheckListBoxClickCheck;
    CleanCheckListBox->Hint = p_config->psz_longtext;
    CleanCheckListBox->ShowHint = true;

    /* init up and down buttons */
    if ( b_multi_plugins )
    {
        ButtonUp = CreateButton ( this, LIBWIN32_PREFSIZE_LEFT,
                CleanCheckListBox->Left - LIBWIN32_PREFSIZE_HPAD
                    - LIBWIN32_PREFSIZE_LEFT,
                CleanCheckListBox->Top + ( CleanCheckListBox->Height
                    - 2*LIBWIN32_PREFSIZE_BUTTON_HEIGHT ) / 3,
                LIBWIN32_PREFSIZE_BUTTON_HEIGHT,
                "+" );
        ButtonUp->Enabled = false;
        ButtonUp->OnClick = ButtonUpClick;
        ButtonUp->Hint = "Raise the plugin priority";
        ButtonUp->ShowHint = true;

        ButtonDown = CreateButton ( this, LIBWIN32_PREFSIZE_LEFT,
                CleanCheckListBox->Left - LIBWIN32_PREFSIZE_HPAD
                    - LIBWIN32_PREFSIZE_LEFT,
                CleanCheckListBox->Top + ( CleanCheckListBox->Height
                    - 2*LIBWIN32_PREFSIZE_BUTTON_HEIGHT ) * 2 / 3
                    + LIBWIN32_PREFSIZE_BUTTON_HEIGHT,
                LIBWIN32_PREFSIZE_BUTTON_HEIGHT,
                "-" );
        ButtonDown->Enabled = false;
        ButtonDown->OnClick = ButtonDownClick;
        ButtonDown->Hint = "Decrease the plugin priority";
        ButtonDown->ShowHint = true;
    }
    else
    {
        ButtonUp = NULL;
        ButtonDown = NULL;
    }

    /* panel height */
    Height = CleanCheckListBox->Top + CleanCheckListBox->Height
            + LIBWIN32_PREFSIZE_VPAD;
};
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::CheckListBoxClick( TObject *Sender )
{
    module_t *p_parser;
    vlc_list_t list;
    int i_index;

    /* check that the click is valid (we are on an item, and the click
     * started on an item */
    if( CleanCheckListBox->ItemIndex == -1 )
    {
        if ( ButtonUp != NULL ) ButtonUp->Enabled = false;
        if ( ButtonDown != NULL ) ButtonDown->Enabled = false;
        return;
    }

    AnsiString Name = ((TObjectString*)CleanCheckListBox->Items->
        Objects[CleanCheckListBox->ItemIndex])->String().c_str();
    if( Name == "" )
        return;

    /* enable up and down buttons */
    if ( b_multi_plugins && ButtonUp != NULL && ButtonDown != NULL )
    {
        if ( CleanCheckListBox->ItemIndex == 0 )
            ButtonUp->Enabled = false; else ButtonUp->Enabled = true;
        if ( CleanCheckListBox->ItemIndex
            == CleanCheckListBox->Items->Count - 1 )
            ButtonDown->Enabled = false; else ButtonDown->Enabled = true;
    }

    /* look for module 'Name' */
    list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( strcmp( p_parser->psz_object_name, Name.c_str() ) == 0 )
        {
            ModuleSelected = p_parser;
            ButtonConfig->Enabled =
                p_parser->i_config_items ? true : false;

            break;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::CheckListBoxClickCheck( TObject *Sender )
{
    if ( ! b_multi_plugins )
    {
        /* one item maximum must be checked */
        if( CleanCheckListBox->Checked[CleanCheckListBox->ItemIndex] )
        {
            for( int item = 0; item < CleanCheckListBox->Items->Count; item++ )
            {
                if( item != CleanCheckListBox->ItemIndex )
                {
                    CleanCheckListBox->Checked[item] = false;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::ButtonConfigClick( TObject *Sender )
{
    p_intf->p_sys->p_window->
                        CreatePreferences( ModuleSelected->psz_object_name );
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::ButtonUpClick( TObject *Sender )
{
    if( CleanCheckListBox->ItemIndex != -1 && CleanCheckListBox->ItemIndex > 0 )
    {
        int Pos = CleanCheckListBox->ItemIndex;
        CleanCheckListBox->Items->Move ( Pos , Pos - 1 );
        CleanCheckListBox->ItemIndex = Pos - 1;
        CheckListBoxClick ( Sender );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::ButtonDownClick( TObject *Sender )
{
    if( CleanCheckListBox->ItemIndex != -1
        && CleanCheckListBox->ItemIndex < CleanCheckListBox->Items->Count - 1 )
    {
        int Pos = CleanCheckListBox->ItemIndex;
        CleanCheckListBox->Items->Move ( Pos , Pos + 1 );
        CleanCheckListBox->ItemIndex = Pos + 1;
        CheckListBoxClick ( Sender );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::SetValue ( AnsiString Values )
{
    int TopChecked = 0;
    while ( Values.Length() != 0 )
    {
        AnsiString Value;

        int NextValue = Values.Pos ( "," );
        if ( NextValue == 0 )
        {
            Value = Values.Trim();
            Values = "";
        }
        else
        {
            Value = Values.SubString(1,NextValue-1).Trim();
            Values = Values.SubString ( NextValue + 1
                    , Values.Length() - NextValue );
        }

        if ( Value.Length() > 0 )
        {
            for ( int i = TopChecked; i < CleanCheckListBox->Items->Count; i++ )
            {
                if ( ((TObjectString*)CleanCheckListBox->Items->Objects[i])
                        ->String() == Value )
                {
                    CleanCheckListBox->Checked[i] = true;
                    CleanCheckListBox->Items->Move ( i , TopChecked );
                    TopChecked++;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPanelPlugin::UpdateChanges()
{
    AnsiString Name = "";

    /* find the selected plugin (if any) */
    for( int item = 0; item < CleanCheckListBox->Items->Count; item++ )
    {
        if( CleanCheckListBox->Checked[item] )
        {
            if ( Name.Length() == 0 )
            {
                Name = ((TObjectString*)CleanCheckListBox->Items->Objects[item])
                       ->String();
            }
            else
            {
                Name = Name + ","
                     + ((TObjectString*)CleanCheckListBox->Items->Objects[item])
                       ->String();
            }
        }
    }

    config_PutPsz( p_intf, p_config->psz_name,
                   Name.Length() ? Name.c_str() : NULL );
}


/****************************************************************************
 * Panel for string management
 ****************************************************************************/
__fastcall TPanelString::TPanelString( TComponent* Owner,
        module_config_t *p_config, intf_thread_t *_p_intf )
        : TPanelPref( Owner, p_config, _p_intf )
{
    /* init description label */
    AnsiString Text = AnsiString ( p_config->psz_text ) + ":";
    Label = CreateLabel( this,
            LIBWIN32_PREFSIZE_LEFT,
            LIBWIN32_PREFSIZE_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_LABEL_HEIGHT,
            Text.c_str(), true );

    /* init edit */
    Edit = CreateEdit( this,
            LIBWIN32_PREFSIZE_EDIT_LEFT,
            LIBWIN32_PREFSIZE_EDIT_WIDTH,
            LIBWIN32_PREFSIZE_VPAD + Label->Height + LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_EDIT_HEIGHT, "" );
    vlc_mutex_lock( p_config->p_lock );
    Edit->Text = p_config->psz_value ? p_config->psz_value : "";
    vlc_mutex_unlock( p_config->p_lock );
    Edit->Hint = p_config->psz_longtext;
    Edit->ShowHint = true;

    /* panel height */
    Height = LIBWIN32_PREFSIZE_VPAD + Label->Height + LIBWIN32_PREFSIZE_VPAD
            + Edit->Height + LIBWIN32_PREFSIZE_VPAD;
};
//---------------------------------------------------------------------------
void __fastcall TPanelString::UpdateChanges()
{
    config_PutPsz( p_intf, p_config->psz_name,
                   Edit->Text.Length() ? Edit->Text.c_str() : NULL );
}


/****************************************************************************
 * Panel for integer management
 ****************************************************************************/
__fastcall TPanelInteger::TPanelInteger( TComponent* Owner,
        module_config_t *p_config, intf_thread_t *_p_intf )
        : TPanelPref( Owner, p_config, _p_intf )
{
    /* init description label */
    AnsiString Text = AnsiString ( p_config->psz_text ) + ":";
    Label = CreateLabel( this,
            LIBWIN32_PREFSIZE_LEFT,
            LIBWIN32_PREFSIZE_WIDTH - LIBWIN32_PREFSIZE_SPINEDIT_WIDTH
             - LIBWIN32_PREFSIZE_HPAD,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_LABEL_HEIGHT, Text.c_str(), true );

    /* init spinedit */
    SpinEdit = CreateSpinEdit( this,
            LIBWIN32_PREFSIZE_RIGHT - LIBWIN32_PREFSIZE_SPINEDIT_WIDTH,
            LIBWIN32_PREFSIZE_SPINEDIT_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_SPINEDIT_HEIGHT,
            -1, 100000, p_config->i_value );
    SpinEdit->Hint = p_config->psz_longtext;
    SpinEdit->ShowHint = true;

    /* vertical alignement and panel height */
    if ( SpinEdit->Height > Label->Height )
    {
        Label->Top += ( SpinEdit->Height - Label->Height ) / 2;
        Height = SpinEdit->Top + SpinEdit->Height + LIBWIN32_PREFSIZE_VPAD;
    }
    else
    {
        SpinEdit->Top += ( Label->Height - SpinEdit->Height ) / 2;
        Height = Label->Top + Label->Height + LIBWIN32_PREFSIZE_VPAD;
    }
};
//---------------------------------------------------------------------------
void __fastcall TPanelInteger::UpdateChanges()
{
    /* Warning: we're casting from long to int */
    config_PutInt( p_intf, p_config->psz_name, (int)SpinEdit->Value );
}


/****************************************************************************
 * Panel for float management
 ****************************************************************************/
__fastcall TPanelFloat::TPanelFloat( TComponent* Owner,
        module_config_t *p_config, intf_thread_t *_p_intf )
        : TPanelPref( Owner, p_config, _p_intf )
{
#define MAX_FLOAT_CHARS 20
    /* init description label */
    AnsiString Text = AnsiString( p_config->psz_text ) + ":";
    Label = CreateLabel( this,
            LIBWIN32_PREFSIZE_LEFT,
            LIBWIN32_PREFSIZE_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_LABEL_HEIGHT,
            Text.c_str(), true );

    /* init edit */
    char *psz_value = (char *)malloc( MAX_FLOAT_CHARS );
    snprintf( psz_value, MAX_FLOAT_CHARS, "%f", p_config->f_value );
    /* we use the spinedit size, to be similar with the integers */
    Edit = CreateEdit( this,
            LIBWIN32_PREFSIZE_RIGHT - LIBWIN32_PREFSIZE_SPINEDIT_WIDTH,
            LIBWIN32_PREFSIZE_SPINEDIT_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_SPINEDIT_HEIGHT,
            psz_value );
    free( psz_value );
    Edit->Hint = p_config->psz_longtext;
    Edit->ShowHint = true;

    /* vertical alignement and panel height */
    if ( Edit->Height > Label->Height )
    {
        Label->Top += ( Edit->Height - Label->Height ) / 2;
        Height = Edit->Top + Edit->Height + LIBWIN32_PREFSIZE_VPAD;
    }
    else
    {
        Edit->Top += ( Label->Height - Edit->Height ) / 2;
        Height = Label->Top + Label->Height + LIBWIN32_PREFSIZE_VPAD;
    }

#undef MAX_FLOAT_CHARS
};
//---------------------------------------------------------------------------
void __fastcall TPanelFloat::UpdateChanges()
{
    /* Warning: we're casting from double to float */
    config_PutFloat( p_intf, p_config->psz_name, atof( Edit->Text.c_str() ) );
}


/****************************************************************************
 * Panel for boolean management
 ****************************************************************************/
__fastcall TPanelBool::TPanelBool( TComponent* Owner,
        module_config_t *p_config, intf_thread_t *_p_intf )
        : TPanelPref( Owner, p_config, _p_intf )
{
    /* init checkbox */
    CheckBox = CreateCheckBox( this,
            LIBWIN32_PREFSIZE_LEFT,
            LIBWIN32_PREFSIZE_WIDTH,
            LIBWIN32_PREFSIZE_VPAD,
            LIBWIN32_PREFSIZE_CHECKBOX_HEIGHT, p_config->psz_text );
    CheckBox->Checked = p_config->i_value;
    CheckBox->Hint = p_config->psz_longtext;
    CheckBox->ShowHint = true;

    /* panel height */
    Height = LIBWIN32_PREFSIZE_VPAD + CheckBox->Height + LIBWIN32_PREFSIZE_VPAD;
};
//---------------------------------------------------------------------------
void __fastcall TPanelBool::UpdateChanges()
{
    config_PutInt( p_intf, p_config->psz_name, CheckBox->Checked ? 1 : 0 );
}


/****************************************************************************
 * Callbacks for the dialog
 ****************************************************************************/
__fastcall TPreferencesDlg::TPreferencesDlg( TComponent* Owner,
    intf_thread_t *_p_intf ) : TForm( Owner )
{
    p_intf = _p_intf;
    Icon = p_intf->p_sys->p_window->Icon;
    Application->HintHidePause = 0x1000000;
    HintWindowClass = __classid ( TNarrowHintWindow );
    /* prevent the form from being resized horizontally */
    Constraints->MinWidth = Width;
    Constraints->MaxWidth = Width;
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::FormClose( TObject *Sender,
      TCloseAction &Action )
{
    Action = caHide;
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
    module_t           *p_parser;
    vlc_list_t          list;
    int                 i_index;

    module_config_t    *p_item;
    int                 i_pages, i_ctrl;

    TTabSheet          *TabSheet;
    TScrollBox         *ScrollBox = NULL;
    TPanel             *Panel;
    TPanelPlugin       *PanelPlugin;
    TPanelString       *PanelString;
    TPanelInteger      *PanelInteger;
    TPanelFloat        *PanelFloat;
    TPanelBool         *PanelBool;

    /* Look for the selected module */
    list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( psz_module_name
             && !strcmp( psz_module_name, p_parser->psz_object_name ) )
        {
            break;
        }
    }
    if( !p_parser || i_index == list.i_count )
    {
        vlc_list_release( &list );
        return;
    }

    /*
     * We found it, now we can start building its configuration interface
     */

    /* Enumerate config options and add corresponding config boxes */
    p_item = p_parser->p_config;
    if( p_item ) do
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

            /* add a panel as top margin */
            ADD_PANEL;

            break;

        case CONFIG_ITEM_MODULE:

            /* add new panel for the config option */
            PanelPlugin = new TPanelPlugin( this, p_item, p_intf, true );
            PanelPlugin->Parent = ScrollBox;

            /* Look for valid modules */
            for( i_index = 0; i_index < list.i_count; i_index++ )
            {
                p_parser = (module_t *)list.p_values[i_index].p_object ;

                if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
                {
                    AnsiString ModuleDesc;
                    if ( p_parser->psz_longname != NULL ) {
                        ModuleDesc = AnsiString( p_parser->psz_longname ) +
                            " (" + AnsiString( p_parser->psz_object_name ) +
                            ")";
                    }
                    else
                        ModuleDesc = AnsiString( p_parser->psz_object_name );

                    PanelPlugin->CleanCheckListBox->Items->AddObject(
                        ModuleDesc.c_str(),
                        new TObjectString( p_parser->psz_object_name ) );
                }
            }

            /* check relevant boxes */
            PanelPlugin->SetValue ( AnsiString ( p_item->psz_value ) );

            break;

        case CONFIG_ITEM_FILE:

        case CONFIG_ITEM_STRING:

            /* add new panel for the config option */
            PanelString = new TPanelString( this, p_item, p_intf );
            PanelString->Parent = ScrollBox;

            break;

        case CONFIG_ITEM_INTEGER:

            /* add new panel for the config option */
            PanelInteger = new TPanelInteger( this, p_item, p_intf );
            PanelInteger->Parent = ScrollBox;

            break;

        case CONFIG_ITEM_FLOAT:

            /* add new panel for the config option */
            PanelFloat = new TPanelFloat( this, p_item, p_intf );
            PanelFloat->Parent = ScrollBox;

            break;

        case CONFIG_ITEM_BOOL:

            /* add new panel for the config option */
            PanelBool = new TPanelBool( this, p_item, p_intf );
            PanelBool->Parent = ScrollBox;

            break;
        default:
            msg_Warn( p_intf, "unknown config type: %i", p_item->i_type );
            break;
        }

        p_item++;
    }
    while( p_item->i_type != CONFIG_HINT_END );

    /* Reorder panels inside the tabsheets */
    for( i_pages = 0; i_pages < PageControlPref->PageCount; i_pages++ )
    {
        /* get scrollbox from the tabsheet */
        ScrollBox = (TScrollBox *)PageControlPref->Pages[i_pages]->Controls[0];

        /* add a panel as bottom margin */
        ADD_PANEL;

        for( i_ctrl = ScrollBox->ControlCount - 1; i_ctrl >= 0 ; i_ctrl-- )
        {
            ScrollBox->Controls[i_ctrl]->Align = alTop;
        }
    }

    vlc_list_release( &list );

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
    TPanelPref *Panel;
    int i, j;

    for( i = 0; i < PageControlPref->PageCount; i++ )
    {
        /* get scrollbox from the tabsheet */
        ScrollBox = (TScrollBox *)PageControlPref->Pages[i]->Controls[0];

        for( j = 0; j < ScrollBox->ControlCount ; j++ )
        {
            /* skip the panels */
            if( ScrollBox->Controls[j]->InheritsFrom( __classid( TPanelPref ) ) )
            {
                Panel = (TPanelPref *)ScrollBox->Controls[j];
                Panel->UpdateChanges();
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonSaveClick( TObject *Sender )
{
    ButtonApplyClick( Sender );
    config_SaveConfigFile( p_intf, NULL );
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDlg::ButtonCancelClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
