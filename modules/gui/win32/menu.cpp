/*****************************************************************************
 * menu.cpp: functions to handle menu items
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: menu.cpp,v 1.14 2003/02/12 02:11:58 ipkiss Exp $
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "menu.h"
#include "win32_common.h"

/*****************************************************************************
 * TMenusGen::*Click: callbacks for the menuitems
 ****************************************************************************/

/*
 * Variables
 */

/* variables of the audio output */
void __fastcall TMenusGen::AoutVarClick( TObject *Sender )
{
    TMenuItem * Item = (TMenuItem *)Sender;

    vlc_object_t * p_aout;
    p_aout = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                              FIND_ANYWHERE );
    if( p_aout == NULL )
    {
        msg_Warn( p_intf, "cannot set variable (%s)", Item->Caption.c_str() );
        return;
    }

    if( Item->Parent == MenuADevice || Item->Parent == PopupADevice )
    {
        VarChange( p_aout, "audio-device", MenuADevice, PopupADevice, Item );
    }
    else if( Item->Parent == MenuChannel || Item->Parent == PopupChannel )
    {
        VarChange( p_aout, "audio-channels", MenuChannel, PopupChannel, Item );
    }

    vlc_object_release( p_aout );
}

/* variables of the video output */
void __fastcall TMenusGen::VoutVarClick( TObject *Sender )
{
    TMenuItem * Item = (TMenuItem *)Sender;

    vlc_object_t * p_vout;
    p_vout = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout == NULL )
    {
        msg_Warn( p_intf, "cannot set variable (%s)", Item->Caption.c_str() );
        return;
    }

    if( Item->Parent == MenuVDevice || Item->Parent == PopupVDevice )
    {
        VarChange( p_vout, "video-device", MenuVDevice, PopupVDevice, Item );
    }

    vlc_object_release( p_vout );
}

/*
 * Modules
 */

/* Interface modules: we spawn a new interface */
void __fastcall TMenusGen::InterfaceModuleClick( TObject *Sender )
{
    TMenuItem * Item = (TMenuItem *)Sender;

    AnsiString IntfName = CleanCaption( Item->Caption );

    intf_thread_t *p_newintf;

    p_newintf = intf_Create( p_intf->p_vlc, IntfName.c_str() );

    if( p_newintf )
    {
        p_newintf->b_block = VLC_FALSE;
        if( intf_RunThread( p_newintf ) )
        {
            vlc_object_detach( p_newintf );
            intf_Destroy( p_newintf );
        }
    }
}

/*
 * Audio
 */

void __fastcall TMenusGen::MenuLanguageClick( TObject *Sender )
{
    LangChange( MenuLanguage, (TMenuItem *)Sender, PopupLanguage, AUDIO_ES );
}

void __fastcall TMenusGen::PopupLanguageClick( TObject *Sender )
{
    LangChange( PopupLanguage, (TMenuItem *)Sender, MenuLanguage, AUDIO_ES );
}

/*
 * Subtitles
 */

void __fastcall TMenusGen::MenuSubtitleClick( TObject *Sender )
{
    LangChange( MenuSubtitles, (TMenuItem *)Sender, PopupSubtitles, SPU_ES );
}

void __fastcall TMenusGen::PopupSubtitleClick( TObject *Sender )
{
    LangChange( PopupSubtitles, (TMenuItem *)Sender, MenuSubtitles, SPU_ES );
}

/*
 * Program
 */

void __fastcall TMenusGen::MenuProgramClick( TObject *Sender )
{
   ProgramChange( (TMenuItem *)Sender, PopupProgram );
}

void __fastcall TMenusGen::PopupProgramClick( TObject *Sender )
{
    ProgramChange( (TMenuItem *)Sender, MenuProgram );
}

/*
 * Title
 */

void __fastcall TMenusGen::MenuTitleClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    input_area_t  * p_area;
    unsigned int    i_title = Item->Tag;

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    i_title = __MIN( i_title,
                     p_intf->p_sys->p_input->stream.i_area_nb - 1 );
    i_title = __MAX( i_title, 1 );
    p_area = p_intf->p_sys->p_input->stream.pp_areas[i_title];
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    input_ChangeArea( p_intf->p_sys->p_input, p_area );

    Item->Checked = true;
    ItemTitle = Index2Item( PopupNavigation, i_title - 1, false );
    Index2Item( ItemTitle, 0, false )->Checked = true;

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
}

/*
 * Chapter
 */

void __fastcall TMenusGen::MenuChapterClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    input_area_t  * p_area;
    unsigned int    i_title;
    unsigned int    i_chapter = Item->Tag;

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    p_area = p_intf->p_sys->p_input->stream.p_selected_area;
    i_chapter = __MIN( i_chapter, p_area->i_part_nb - 1 );
    i_chapter = __MAX( i_chapter, 1 );
    p_area->i_part = i_chapter;
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    input_ChangeArea( p_intf->p_sys->p_input, p_area );

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    i_title = p_intf->p_sys->p_input->stream.p_selected_area->i_id;
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    ItemTitle = Index2Item( PopupNavigation, i_title, false );
    Index2Item( ItemTitle, i_chapter, false )->Checked = true;

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
}

/*
 * Navigation
 */

void __fastcall TMenusGen::PopupNavigationClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    input_area_t  * p_area;
    unsigned int    i_title   = Data2Title( Item->Tag );
    unsigned int    i_chapter = Data2Chapter( Item->Tag );

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    i_title = __MIN( i_title,
                     p_intf->p_sys->p_input->stream.i_area_nb - 1 );
    i_title = __MAX( i_title, 1 );
    p_area = p_intf->p_sys->p_input->stream.pp_areas[i_title];
    i_chapter = __MIN( i_chapter, p_area->i_part_nb - 1 );
    i_chapter = __MAX( i_chapter, 1 );
    p_area->i_part = i_chapter;
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    input_ChangeArea( p_intf->p_sys->p_input, p_area );

    Item->Checked = true;
    ItemTitle = Index2Item( MenuTitle, i_title, false );
    if( ItemTitle->Checked )
    {
        /* same title, new chapter */
        Index2Item( MenuChapter, i_chapter, false )->Checked = true;
    }
    else
    {
        /* new title => we must rebuild the chapter menu */
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        RadioMenu(
            MenuChapter, "Chapter",
            p_intf->p_sys->p_input->stream.p_selected_area->i_part_nb,
            i_chapter, MenuChapterClick );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
}


__fastcall TMenusGen::TMenusGen( intf_thread_t *_p_intf ) : TObject()
{
    p_intf = _p_intf;

    /* Initialize local pointers to menu items of the main window */
    TMainFrameDlg * p_window = p_intf->p_sys->p_window;
    if( p_window == NULL )
    {
        msg_Warn( p_intf, "Main window wasn't created, expect problems..." );
        return;
    }

    MenuChannel = p_window->MenuChannel;
    PopupChannel = p_window->PopupChannel;
    MenuADevice = p_window->MenuADevice;
    PopupADevice = p_window->PopupADevice;
    MenuVDevice = p_window->MenuVDevice;
    PopupVDevice = p_window->PopupVDevice;
    MenuLanguage = p_window->MenuLanguage;
    PopupLanguage = p_window->PopupLanguage;
    MenuSubtitles = p_window->MenuSubtitles;
    PopupSubtitles = p_window->PopupSubtitles;
    MenuProgram = p_window->MenuProgram;
    PopupProgram = p_window->PopupProgram;
    MenuTitle = p_window->MenuTitle;
    MenuChapter = p_window->MenuChapter;
    PopupNavigation = p_window->PopupNavigation;
    MenuAddInterface = p_window->MenuAddInterface;

    /* Create the "Add interface" menu */
    SetupModuleMenu( "interface", MenuAddInterface, InterfaceModuleClick );
}


/*****************************************************************************
 * SetupMenus: This function dynamically generates some menus
 *****************************************************************************
 * The lock on p_input->stream must be taken before you call this function
 *****************************************************************************/
void __fastcall TMenusGen::SetupMenus()
{
    TMainFrameDlg  * p_window = p_intf->p_sys->p_window;
    input_thread_t * p_input  = p_intf->p_sys->p_input;
    es_descriptor_t   * p_audio_es;
    es_descriptor_t   * p_spu_es;

    p_intf->p_sys->b_chapter_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_audio_update |= p_intf->p_sys->b_program_update |
                                     p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_spu_update |= p_intf->p_sys->b_program_update |
                                   p_intf->p_sys->b_title_update;

    if( p_intf->p_sys->b_program_update )
    {
        pgrm_descriptor_t * p_pgrm;

        if( p_input->stream.p_new_program )
        {
            p_pgrm = p_input->stream.p_new_program;
        }
        else
        {
            p_pgrm = p_input->stream.p_selected_program;
        }

        ProgramMenu( MenuProgram, p_pgrm, MenuProgramClick );
        ProgramMenu( PopupProgram, p_pgrm, PopupProgramClick );

        p_intf->p_sys->b_program_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_title_update )
    {
// why "-1" ?
// because if the titles go from 1 to X-1, there are X-1 titles
        RadioMenu( MenuTitle, "Title",
                   p_input->stream.i_area_nb - 1,
                   p_input->stream.p_selected_area->i_id,
                   MenuTitleClick );

        AnsiString CurrentTitle;
        CurrentTitle.sprintf( "%d", p_input->stream.p_selected_area->i_id );
        p_window->LabelTitleCurrent->Caption = CurrentTitle;

        p_intf->p_sys->b_title_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_chapter_update )
    {
        RadioMenu( MenuChapter, "Chapter",
                   p_input->stream.p_selected_area->i_part_nb - 1,
                   p_input->stream.p_selected_area->i_part,
                   MenuChapterClick );

        NavigationMenu( PopupNavigation, PopupNavigationClick );

        AnsiString CurrentChapter;
        CurrentChapter.sprintf( "%d", p_input->stream.p_selected_area->i_part );
        p_window->LabelChapterCurrent->Caption = CurrentChapter;

        p_intf->p_sys->i_part = p_input->stream.p_selected_area->i_part;

        p_intf->p_sys->b_chapter_update = VLC_FALSE;
    }

    /* look for selected ES */
    p_audio_es = NULL;
    p_spu_es = NULL;

    for( unsigned int i = 0; i < p_input->stream.i_selected_es_number; i++ )
    {
        if( p_input->stream.pp_selected_es[i]->i_cat == AUDIO_ES )
        {
            p_audio_es = p_input->stream.pp_selected_es[i];
        }

        if( p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_spu_es = p_input->stream.pp_selected_es[i];
        }
    }
    this->p_audio_es_old = p_audio_es;
    this->p_spu_es_old = p_spu_es;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* audio menus */
    if( p_intf->p_sys->b_audio_update )
    {
        LanguageMenu( MenuLanguage, p_audio_es, AUDIO_ES, MenuLanguageClick );
        LanguageMenu( PopupLanguage, p_audio_es, AUDIO_ES, PopupLanguageClick );

        p_intf->p_sys->b_audio_update = VLC_FALSE;
    }

    /* sub picture menus */
    if( p_intf->p_sys->b_spu_update )
    {
        LanguageMenu( PopupSubtitles, p_spu_es, SPU_ES, PopupSubtitleClick );
        LanguageMenu( MenuSubtitles, p_spu_es, SPU_ES, MenuSubtitleClick );

        p_intf->p_sys->b_spu_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_aout_update )
    {
        aout_instance_t * p_aout;
        p_aout = (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                     FIND_ANYWHERE );

        if( p_aout != NULL )
        {
            vlc_value_t val;
            val.b_bool = VLC_FALSE;

            var_Set( (vlc_object_t *)p_aout, "intf-change", val );

            SetupVarMenu( (vlc_object_t *)p_aout, "audio-channels",
                          MenuChannel, AoutVarClick );
            SetupVarMenu( (vlc_object_t *)p_aout, "audio-channels",
                          PopupChannel, AoutVarClick );

            SetupVarMenu( (vlc_object_t *)p_aout, "audio-device",
                          MenuADevice, AoutVarClick );
            SetupVarMenu( (vlc_object_t *)p_aout, "audio-device",
                          PopupADevice, AoutVarClick );

            vlc_object_release( (vlc_object_t *)p_aout );
        }

        p_intf->p_sys->b_aout_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_vout_update )
    {
        vout_thread_t * p_vout;
        p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                   FIND_ANYWHERE );

        if( p_vout != NULL )
        {
            vlc_value_t val;
            val.b_bool = VLC_FALSE;

            var_Set( (vlc_object_t *)p_vout, "intf-change", val );

            SetupVarMenu( (vlc_object_t *)p_vout, "video-device",
                          MenuVDevice, VoutVarClick );
            SetupVarMenu( (vlc_object_t *)p_vout, "video-device",
                          PopupVDevice, VoutVarClick );

            vlc_object_release( (vlc_object_t *)p_vout );
        }

        p_intf->p_sys->b_vout_update = VLC_FALSE;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
}


/*****************************************************************************
 * Private functions
 *****************************************************************************/
TMenuItem * TMenusGen::Index2Item( TMenuItem *Root, int i_index,
                                   bool SingleColumn )
{
    if( SingleColumn || ( i_index < 20 ) )
        return Root->Items[i_index];
    else
        return Root->Items[i_index / 10]->Items[i_index % 10];
}

int TMenusGen::Item2Index( TMenuItem *Root, TMenuItem *Item )
{
    if( Item->Parent == Root )
        return Item->MenuIndex;
    else
        return( 10 * Item->Parent->MenuIndex + Item->MenuIndex );
}

int __fastcall TMenusGen::Data2Title( int data )
{
    return (int) (data >> 16 );
}

int __fastcall TMenusGen::Data2Chapter( int data )
{
    return (int) (data & 0xffff);
}

int __fastcall TMenusGen::Pos2Data( int title, int chapter )
{
    return (int) (( title << 16 ) | ( chapter & 0xffff ));
}

/* This function deletes all the '&' characters in the caption string,
 * because Borland automatically adds one when (and only when!) you click on
 * the menuitem. Grrrrr... */
AnsiString __fastcall TMenusGen::CleanCaption( AnsiString Caption )
{
    while( Caption.LastDelimiter( "&" ) != 0 )
    {
        Caption.Delete( Caption.LastDelimiter( "&" ), 1 );
    }

    return Caption;
}

/****************************************************************************
 * VarChange: change a variable in a vlc_object_t
 ****************************************************************************
 * Change the variable and update the menuitems.
 ****************************************************************************/
void __fastcall TMenusGen::VarChange( vlc_object_t *p_object,
        const char *psz_variable, TMenuItem *RootMenu, TMenuItem *RootPopup,
        TMenuItem *Item )
{
    vlc_value_t val;
    int i_index;

    AnsiString Caption = CleanCaption( Item->Caption );
    val.psz_string = Caption.c_str();

    /* set the new value */
    if( var_Set( p_object, psz_variable, val ) < 0 )
    {
        msg_Warn( p_object, "cannot set variable (%s)", val.psz_string );
    }

    i_index = Item->MenuIndex;
    RootMenu->Items[i_index]->Checked = true;
    RootPopup->Items[i_index]->Checked = true;
}

/****************************************************************************
 * LangChange: change audio or subtitles languages
 ****************************************************************************
 * Toggle the language, and update the selected menuitems.
 ****************************************************************************/
void __fastcall TMenusGen::LangChange( TMenuItem *RootCurrent, TMenuItem *Item,
    TMenuItem *RootOther, int i_cat )
{
    es_descriptor_t * p_es;
    es_descriptor_t * p_es_old;
    int i_index;
    int i_es;

    /* find the selected ES */
    i_es = Item->Tag;

    /* find selected menu item */
    i_index = Item2Index( RootCurrent, Item ) - 1;
    if( i_index < 0 )
    {
        /* 'None' was selected */
        p_es = NULL;
    }
    else
    {
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_es = p_intf->p_sys->p_input->stream.pp_es[i_es];
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }

    /* find the current ES */
    if( i_cat == AUDIO_ES )
    {
        p_es_old = this->p_audio_es_old;
        this->p_audio_es_old = p_es;
    }
    else
    {
        p_es_old = this->p_spu_es_old;
        this->p_spu_es_old = p_es;
    }

    /* exchange them */
    input_ToggleES( p_intf->p_sys->p_input, p_es_old, false );
    input_ToggleES( p_intf->p_sys->p_input, p_es, true );

    Item->Checked = true;
    Index2Item( RootOther, i_index + 1, true )->Checked = true;
}

/****************************************************************************
 * ProgramChange: change the program
 ****************************************************************************
 * Toggle the program, and update the selected menuitems.
 ****************************************************************************/
void __fastcall TMenusGen::ProgramChange( TMenuItem *Item,
                                          TMenuItem *RootOther )
{
    int i_program = Item->Tag;

    /* toggle the program */
    input_ChangeProgram( p_intf->p_sys->p_input, (uint16_t)i_program );

    /* check selected menu items */
    Item->Checked = true;
    Index2Item( RootOther, i_program - 1, true )->Checked = true;

    /* update audio/subtitles menus */
    p_intf->p_sys->b_audio_update = VLC_TRUE;
    p_intf->p_sys->b_spu_update = VLC_TRUE;
    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    SetupMenus();
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    p_intf->p_sys->b_audio_update = VLC_FALSE;
    p_intf->p_sys->b_spu_update = VLC_FALSE;

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
}

/*****************************************************************************
 * SetupVarMenu: build a menu allowing to change a variable
 *****************************************************************************/
void __fastcall TMenusGen::SetupVarMenu( vlc_object_t *p_object,
        const char *psz_variable, TMenuItem *Root, TNotifyEvent MenuItemClick )
{
    TMenuItem * Item;
    vlc_value_t val;
    char * psz_value = NULL;
    int i;

    /* remove previous menu */
    Root->Clear();

    /* get the current value */
    if( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return;
    }
    psz_value = val.psz_string;

    if( var_Change( p_object, psz_variable, VLC_VAR_GETLIST, &val ) < 0 )
    {
        free( psz_value );
        return;
    }

    /* append a menuitem for each option */
    for( i = 0; i < val.p_list->i_count; i++ )
    {
        Item = new TMenuItem( Root );
        Item->Caption = val.p_list->p_values[i].psz_string;
        Item->Hint = val.p_list->p_values[i].psz_string;
        Item->RadioItem = true;
        Item->OnClick = MenuItemClick;
        if( !strcmp( psz_value, val.p_list->p_values[i].psz_string ) )
            Item->Checked = true;

        /* Add the item to the submenu */
        Root->Add( Item );
    }

    /* enable the menu if there is at least 1 item */
    Root->Enabled = ( val.p_list->i_count > 0 );

    /* clean up everything */
    var_Change( p_object, psz_variable, VLC_VAR_FREELIST, &val );
//    free( psz_value );
}

/*****************************************************************************
 * SetupModuleMenu: build a menu listing all the modules of a given
                    capability
 *****************************************************************************/
void __fastcall TMenusGen::SetupModuleMenu( const char *psz_capability,
        TMenuItem *Root, TNotifyEvent MenuItemClick )
{
    module_t * p_parser;
    vlc_list_t *p_list;
    int i_index;

    /* remove previous menu */
    Root->Clear();
    Root->Enabled = false;

    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, psz_capability ) )
        {
            TMenuItem *Item = new TMenuItem( Root );
            Item->Caption = p_parser->psz_object_name;
            Item->Hint = Item->Caption;
            Item->OnClick = MenuItemClick;
            Root->Add( Item );
        }
    }

    vlc_list_release( p_list );

    /* be sure that menu is enabled, if there is at least one item */
    if( i_index > 0 )
        Root->Enabled = true;
}

/*****************************************************************************
 * ProgramMenu: update the programs menu of the interface
 *****************************************************************************
 * Builds the program menu according to what have been found in the PAT
 * by the input. Useful for multi-programs streams such as DVB ones.
 *****************************************************************************/
void __fastcall TMenusGen::ProgramMenu( TMenuItem *Root,
    pgrm_descriptor_t *p_pgrm, TNotifyEvent MenuItemClick )
{
    TMenuItem * Item;

    /* remove previous menu */
    Root->Clear();
    Root->Enabled = false;

    /* create a set of program buttons and append them to the container */
    for( unsigned int i = 0; i < p_intf->p_sys->p_input->stream.i_pgrm_number;
         i++ )
    {
        AnsiString Name;
        Name.sprintf( "id %d",
            p_intf->p_sys->p_input->stream.pp_programs[i]->i_number );

        Item = new TMenuItem( Root );
        Item->Caption = Name;
        Item->Hint = Name;
        Item->RadioItem = true;
        Item->OnClick = MenuItemClick;

        /* FIXME: temporary hack to save the program id with the Item
         * It will be used in the callback. */
        Item->Tag = i + 1;

        /* check the currently selected program */
        if( p_pgrm == p_intf->p_sys->p_input->stream.pp_programs[i] )
            Item->Checked = true;

        /* add the item to the submenu */
        Root->Add( Item );
    }

    /* be sure that menu is enabled if more than 1 program */
    if( p_intf->p_sys->p_input->stream.i_pgrm_number > 1 )
        Root->Enabled = true;
}

/*****************************************************************************
 * RadioMenu: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
void __fastcall TMenusGen::RadioMenu( TMenuItem *Root, AnsiString ItemName,
     int i_nb, int i_selected, TNotifyEvent MenuItemClick )
{
    TMenuItem  * ItemGroup;
    TMenuItem  * Item;
    AnsiString   Name;

    /* remove previous menu */
    Root->Enabled = false;
    Root->Clear();

    for( int i_item = 1; i_item <= i_nb; i_item++ )
    {
        /* we group titles/chapters in packets of ten for small screens */
        if( ( i_item % 10 == 1 ) && ( i_nb > 20 ) )
        {
            if( i_item != 1 )
                Root->Add( ItemGroup );

            Name.sprintf( "%ss %d to %d", ItemName, i_item, i_item + 9 );
            ItemGroup = new TMenuItem( Root );
            ItemGroup->Hint = Name;
            ItemGroup->RadioItem = true;

            /* set the accelerator character */
            Name.Insert( "&", Name.Length() - 1 );
            ItemGroup->Caption = Name;
        }

        Name.sprintf( "%s %d", ItemName, i_item );
        Item = new TMenuItem( Root );
        Item->RadioItem = true;
        Item->Hint = Name;

        /* set the accelerator character */
        Name.Insert( "&", Name.Length() );
        Item->Caption = Name;

        /* FIXME: temporary hack to save i_item with the Item
         * It will be used in the callback. */
        Item->Tag = i_item;

        /* check the currently selected chapter */
        if( i_selected == i_item )
            Item->Checked = true;

        /* setup signal handling */
        Item->OnClick = MenuItemClick;

        if( i_nb > 20 )
            ItemGroup->Add( Item );
        else
            Root->Add( Item );
    }

//  if( ( i_nb > 20 ) && ( i_item % 10 ) )  ?
    if( i_nb > 20 )
        Root->Add( ItemGroup );

    /* be sure that menu is enabled, if there are several items */
    if( i_nb > 1 )
        Root->Enabled = true;
}

/*****************************************************************************
 * LanguageMenus: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input:
 *  - languages
 *  - sub-pictures
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
void __fastcall TMenusGen::LanguageMenu( TMenuItem *Root, es_descriptor_t *p_es,
    int i_cat, TNotifyEvent MenuItemClick )
{
    TMenuItem     * Separator;
    TMenuItem     * Item;
    AnsiString      Name;

    /* remove previous menu */
    Root->Clear();
    Root->Enabled = false;

    /* special case for "off" item */
    Name = "None";
    Item = new TMenuItem( Root );
    Item->RadioItem = true;
    Item->Hint = Name;
    Item->Caption = Name;
    Item->OnClick = MenuItemClick;
    Item->Tag = -1;
    Root->Add( Item );

    /* separator item */
    Separator = new TMenuItem( Root );
    Separator->Caption = "-";
    Root->Add( Separator );

    int i_item = 0;

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

#define ES p_intf->p_sys->p_input->stream.pp_es[i]
    /* create a set of language buttons and append them to the Root */
    for( unsigned int i = 0; i < p_intf->p_sys->p_input->stream.i_es_number;
         i++ )
    {
        if( ( ES->i_cat == i_cat ) &&
            ( !ES->p_pgrm ||
              ES->p_pgrm ==
                p_intf->p_sys->p_input->stream.p_selected_program ) )
        {
            i_item++;
            Name = p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc;
            if( Name.IsEmpty() )
                Name.sprintf( "Language %d", i_item );

            Item = new TMenuItem( Root );
            Item->RadioItem = true;
            Item->Hint = Name;
            Item->Caption = Name;
            Item->Tag = i;

            /* check the currently selected item */
            if( p_es == p_intf->p_sys->p_input->stream.pp_es[i] )
                Item->Checked = true;

            /* setup signal hanling */
            Item->OnClick = MenuItemClick;
            Root->Add( Item );
        }
    }
#undef ES

    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    /* be sure that menu is enabled if non empty */
    if( i_item > 0 )
        Root->Enabled = true;
}

/*****************************************************************************
 * NavigationMenu: sets menus for titles and chapters selection
 *****************************************************************************
 * Generates two types of menus:
 *  -simple list of titles
 *  -cascaded lists of chapters for each title
 *****************************************************************************/
void __fastcall TMenusGen::NavigationMenu( TMenuItem *Root,
    TNotifyEvent MenuItemClick )
{
    TMenuItem     * TitleGroup;
    TMenuItem     * TitleItem;
    TMenuItem     * ChapterGroup;
    TMenuItem     * ChapterItem;
    AnsiString      Name;
    unsigned int    i_title_nb;
    unsigned int    i_chapter_nb;


    /* remove previous menu */
    Root->Enabled = false;
    Root->Clear();

    i_title_nb = p_intf->p_sys->p_input->stream.i_area_nb - 1;

    /* loop on titles */
    for( unsigned int i_title = 1; i_title <= i_title_nb; i_title++ )
    {
        /* we group titles in packets of ten for small screens */
        if( ( i_title % 10 == 1 ) && ( i_title_nb > 20 ) )
        {
            if( i_title != 1 )
                Root->Add( TitleGroup );

            Name.sprintf( "%d - %d", i_title, i_title + 9 );
            TitleGroup = new TMenuItem( Root );
            TitleGroup->RadioItem = true;
            TitleGroup->Hint = Name;
            TitleGroup->Caption = Name;
        }

        Name.sprintf( "Title %d (%d)", i_title,
            p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb - 1 );
        {
            TitleItem = new TMenuItem( Root );
            TitleItem->RadioItem = true;
            TitleItem->Hint = Name;
            TitleItem->Caption = Name;

            i_chapter_nb =
                p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb - 1;

            /* loop on chapters */
            for( unsigned int i_chapter = 1; i_chapter <= i_chapter_nb;
                 i_chapter++ )
            {
                /* we group chapters in packets of ten for small screens */
                if( ( i_chapter % 10 == 1 ) && ( i_chapter_nb > 20 ) )
                {
                    if( i_chapter != 1 )
                        TitleItem->Add( ChapterGroup );

                    Name.sprintf( "%d - %d", i_chapter, i_chapter + 9 );
                    ChapterGroup = new TMenuItem( TitleItem );
                    ChapterGroup->RadioItem = true;
                    ChapterGroup->Hint = Name;
                    ChapterGroup->Caption = Name;
                }

                Name.sprintf( "Chapter %d", i_chapter );

                ChapterItem = new TMenuItem( TitleItem );
                ChapterItem->RadioItem = true;
                ChapterItem->Hint = Name;
                ChapterItem->Caption = Name;

                /* FIXME: temporary hack to save i_title and i_chapter with
                 * ChapterItem, since we will need them in the callback */
                ChapterItem->Tag = Pos2Data( i_title, i_chapter );

#define p_area p_intf->p_sys->p_input->stream.pp_areas[i_title]
                /* check the currently selected chapter */
                if( ( p_area ==
                        p_intf->p_sys->p_input->stream.p_selected_area ) &&
                    ( p_area->i_part == i_chapter ) )
                {
                    ChapterItem->Checked = true;
                }
#undef p_area

                /* setup signal handling */
                ChapterItem->OnClick = MenuItemClick;

                if( i_chapter_nb > 20 )
                    ChapterGroup->Add( ChapterItem );
                else
                    TitleItem->Add( ChapterItem );
            }

            if( i_chapter_nb > 20 )
            {
                TitleItem->Add( ChapterGroup );
            }

            if( p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb
                > 1 )
            {
                /* be sure that menu is sensitive */
                Root->Enabled = true;
            }
        }

        if( i_title_nb > 20 )
            TitleGroup->Add( TitleItem );
        else
            Root->Add( TitleItem );
    }

    if( i_title_nb > 20 )
        Root->Add( TitleGroup );

    /* be sure that menu is sensitive */
    Root->Enabled = true;
}

