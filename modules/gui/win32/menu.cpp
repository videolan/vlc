/*****************************************************************************
 * menu.cpp: functions to handle menu items
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
//#pragma hdrstop

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "menu.h"
#include "win32_common.h"


/****************************************************************************
 * Local Prototypes
 ****************************************************************************/
extern  intf_thread_t *p_intfGlobal;

static TMenuItem *Index2Item( TMenuItem *, int, bool );
static int Item2Index( TMenuItem *, TMenuItem * );
static void __fastcall LangChange( TMenuItem *, TMenuItem *, TMenuItem *, int );
static void __fastcall ProgramChange( TMenuItem *, TMenuItem * );

static void __fastcall RadioMenu( TMenuItem *, AnsiString,
                                  int, int, TNotifyEvent );
static void __fastcall ProgramMenu( TMenuItem *, pgrm_descriptor_t *,
                                    TNotifyEvent );
static void __fastcall LanguageMenu( TMenuItem *t, es_descriptor_t *,
                                     int, TNotifyEvent );
static void __fastcall NavigationMenu( TMenuItem *, TNotifyEvent );


static TMenuItem *Index2Item( TMenuItem *Root, int i_index, bool SingleColumn )
{
    if( SingleColumn || ( i_index < 20 ) )
    {
        return Root->Items[i_index];
    }
    else
    {
        return Root->Items[i_index / 10]->Items[i_index % 10];
    }
}

static int Item2Index( TMenuItem *Root, TMenuItem *Item )
{
    if( Item->Parent == Root )
    {
        return Item->MenuIndex;
    }
    else
    {
        return( 10 * Item->Parent->MenuIndex + Item->MenuIndex );
    }
}


/****************************************************************************
 * LangChange: change audio or subtitles languages
 ****************************************************************************
 * Toggle the language, and update the selected menuitems.
 ****************************************************************************/
static void __fastcall LangChange( TMenuItem *RootCurrent, TMenuItem *Item,
                                   TMenuItem *RootOther, int i_cat )
{
    intf_thread_t         * p_intf = p_intfGlobal;
    es_descriptor_t       * p_es;
    es_descriptor_t       * p_es_old;
    int                     i_index;
    int                     i_es;

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
        vlc_mutex_lock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
        p_es = p_intfGlobal->p_sys->p_input->stream.pp_es[i_es];
        vlc_mutex_unlock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
    }

    /* find the current ES */
    if( i_cat == AUDIO_ES )
    {
        p_es_old = p_intf->p_sys->p_audio_es_old;
        p_intf->p_sys->p_audio_es_old = p_es;
    }
    else
    {
        p_es_old = p_intf->p_sys->p_spu_es_old;
        p_intf->p_sys->p_spu_es_old = p_es;
    }

    /* exchange them */
    input_ToggleES( p_intfGlobal->p_sys->p_input, p_es_old, false );
    input_ToggleES( p_intfGlobal->p_sys->p_input, p_es, true );

    Item->Checked = true;
    Index2Item( RootOther, i_index + 1, true )->Checked = true;
}


/****************************************************************************
 * ProgramChange: change the program
 ****************************************************************************
 * Toggle the program, and update the selected menuitems.
 ****************************************************************************/
static void __fastcall ProgramChange( TMenuItem *Item, TMenuItem *RootOther )
{
    intf_thread_t * p_intf = p_intfGlobal;
    int             i_program = Item->Tag;

    /* toggle the program */
    input_ChangeProgram( p_intfGlobal->p_sys->p_input, (u16)i_program );

    /* check selected menu items */
    Item->Checked = true;
    Index2Item( RootOther, i_program - 1, true )->Checked = true;

    /* update audio/subtitles menus */
    p_intf->p_sys->b_audio_update = 1;
    p_intf->p_sys->b_spu_update = 1;
    vlc_mutex_lock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
    SetupMenus( p_intf );
    vlc_mutex_unlock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
    p_intf->p_sys->b_audio_update = 0;
    p_intf->p_sys->b_spu_update = 0;

    input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_PLAY );
}


/****************************************************************************
 * TMainFrameDlg::*Click: callbacks for the menuitems
 ****************************************************************************
 * These functions need to be in a class, or we won't be able to pass them
 * as arguments (since TNotifyEvent uses __closure)
 ****************************************************************************/

 /*
 * Audio
 */

void __fastcall TMainFrameDlg::MenuAudioClick( TObject *Sender )
{
    LangChange( MenuAudio, (TMenuItem *)Sender, PopupAudio, AUDIO_ES );
}

void __fastcall TMainFrameDlg::PopupAudioClick( TObject *Sender )
{
    LangChange( PopupAudio, (TMenuItem *)Sender, MenuAudio, AUDIO_ES );
}

/*
 * Subtitles
 */

void __fastcall TMainFrameDlg::MenuSubtitleClick( TObject *Sender )
{
    LangChange( MenuSubtitles, (TMenuItem *)Sender, PopupSubtitles, SPU_ES );
}

void __fastcall TMainFrameDlg::PopupSubtitleClick( TObject *Sender )
{
    LangChange( PopupSubtitles, (TMenuItem *)Sender, MenuSubtitles, SPU_ES );
}

/*
 * Program
 */

void __fastcall TMainFrameDlg::MenuProgramClick( TObject *Sender )
{
    ProgramChange( (TMenuItem *)Sender, PopupProgram );
}

void __fastcall TMainFrameDlg::PopupProgramClick( TObject *Sender )
{
    ProgramChange( (TMenuItem *)Sender, MenuProgram );
}

/*
 * Navigation
 */

void __fastcall TMainFrameDlg::PopupNavigationClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    input_area_t  * p_area;
    int             i_title   = DATA2TITLE( Item->Tag );
    int             i_chapter = DATA2CHAPTER( Item->Tag );

    p_area = p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title];
    p_area->i_part = i_chapter;

    input_ChangeArea( p_intfGlobal->p_sys->p_input, (input_area_t*)p_area );

    Item->Checked = true;
    ItemTitle = Index2Item( MenuTitle, i_title - 1, false );
    if( ItemTitle->Checked )
    {
        /* same title, new chapter */
        Index2Item( MenuChapter, i_chapter - 1, false )->Checked = true;
    }
    else
    {
        /* new title => we must rebuild the chapter menu */
        vlc_mutex_lock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
        RadioMenu( MenuChapter, "Chapter",
                   p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_part_nb,
                   i_chapter, MenuChapterClick );
        vlc_mutex_unlock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );
    }

    input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_PLAY );
}

/*
 * Title
 */

void __fastcall TMainFrameDlg::MenuTitleClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    int             i_title = Item->Tag;

    input_ChangeArea( p_intfGlobal->p_sys->p_input,
                      p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title] );
    Item->Checked = true;
    ItemTitle = Index2Item( PopupNavigation, i_title - 1, false );
    Index2Item( ItemTitle, 0, false )->Checked = true;

    input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_PLAY );
}

/*
 * Chapter
 */

void __fastcall TMainFrameDlg::MenuChapterClick( TObject *Sender )
{
    TMenuItem     * Item = (TMenuItem *)Sender;
    TMenuItem     * ItemTitle;
    input_area_t  * p_area;
    int             i_title;
    int             i_chapter = Item->Tag;

    p_area = p_intfGlobal->p_sys->p_input->stream.p_selected_area;
    p_area->i_part = i_chapter;

    input_ChangeArea( p_intfGlobal->p_sys->p_input, (input_area_t*)p_area );

    i_title = p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_id;
    ItemTitle = Index2Item( PopupNavigation, i_title - 1, false );
    Index2Item( ItemTitle, i_chapter - 1, false )->Checked = true;

    input_SetStatus( p_intfGlobal->p_sys->p_input, INPUT_STATUS_PLAY );
}


/****************************************************************************
 * Functions to generate menus
 ****************************************************************************/

/*****************************************************************************
 * RadioMenu: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
static void __fastcall RadioMenu( TMenuItem * Root, AnsiString ItemName,
                                  int i_nb, int i_selected,
                                  TNotifyEvent MenuItemClick )
{
    TMenuItem     * ItemGroup;
    TMenuItem     * Item;
    TMenuItem     * ItemActive;
    AnsiString      Name;
    int             i_item;

    /* remove previous menu */
    Root->Enabled = false;
    Root->Clear();

    ItemActive = NULL;

    for( i_item = 0; i_item < i_nb; i_item++ )
    {
        /* we group titles/chapters in packets of ten for small screens */
        if( ( i_item % 10 == 0 ) && ( i_nb > 20 ) )
        {
            if( i_item != 0 )
            {
                Root->Add( ItemGroup );
            }

            Name.sprintf( "%ss %d to %d", ItemName, i_item + 1, i_item + 10 );
            ItemGroup = new TMenuItem( Root );
            ItemGroup->Hint = Name;

            /* set the accelerator character */
            Name.Insert( "&", Name.Length() - 1 );
            ItemGroup->Caption = Name;
        }

        Name.sprintf( "%s %d", ItemName, i_item + 1 );
        Item = new TMenuItem( Root );
        Item->RadioItem = true;
        Item->Hint = Name;

        /* set the accelerator character */
        Name.Insert( "&", Name.Length() );
        Item->Caption = Name;

        /* FIXME: temporary hack to save i_item with the Item
         * It will be used in the callback. */
        Item->Tag = i_item + 1;

        if( i_selected == i_item + 1 )
        {
            ItemActive = Item;
        }
        
        /* setup signal handling */
        Item->OnClick = MenuItemClick;

        if( i_nb > 20 )
        {
            ItemGroup->Add( Item );
        }
        else
        {
            Root->Add( Item );
        }
    }

//  if( ( i_nb > 20 ) && ( i_item % 10 ) )  ?
    if( i_nb > 20 )
    {
        Root->Add( ItemGroup );
    }

    /* check currently selected chapter */
    if( ItemActive != NULL )
    {
        ItemActive->Checked = true;
    }

    /* be sure that menu is enabled, if there are several items */
    if( i_nb > 1 )
    {
        Root->Enabled = true;
    }
}


/*****************************************************************************
 * ProgramMenu: update the programs menu of the interface
 *****************************************************************************
 * Builds the program menu according to what have been found in the PAT 
 * by the input. Useful for multi-programs streams such as DVB ones.
 *****************************************************************************/
static void __fastcall ProgramMenu( TMenuItem * Root,
                                    pgrm_descriptor_t * p_pgrm,
                                    TNotifyEvent MenuItemClick )
{
    TMenuItem     * Item;
    TMenuItem     * ItemActive;
    AnsiString      Name;
    int             i;

    /* remove previous menu */
    Root->Clear();
    Root->Enabled = false;

    ItemActive = NULL;

    /* create a set of program buttons and append them to the container */
    for( i = 0; i < p_intfGlobal->p_sys->p_input->stream.i_pgrm_number; i++ )
    {
        Name.sprintf( "id %d",
            p_intfGlobal->p_sys->p_input->stream.pp_programs[i]->i_number );

        Item = new TMenuItem( Root );
        Item->Caption = Name;
        Item->Hint = Name;
        Item->RadioItem = true;
        Item->OnClick = MenuItemClick;

        /* FIXME: temporary hack to save the program id with the Item
         * It will be used in the callback. */
        Item->Tag = i + 1;

        if( p_pgrm == p_intfGlobal->p_sys->p_input->stream.pp_programs[i] )
        {
            /* don't lose Item when we append into menu */
            ItemActive = Item;
        }

        /* Add the item to the submenu */
        Root->Add( Item );
    }

    /* check currently selected program */
    if( ItemActive != NULL )
    {
        ItemActive->Checked = true;
    }

    /* be sure that menu is enabled if more than 1 program */
    if( p_intfGlobal->p_sys->p_input->stream.i_pgrm_number > 1 )
    {
        Root->Enabled = true;
    }
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
static void __fastcall LanguageMenu( TMenuItem * Root, es_descriptor_t * p_es,
                                      int i_cat, TNotifyEvent MenuItemClick )
{
    TMenuItem     * Separator;
    TMenuItem     * Item;
    TMenuItem     * ItemActive;
    AnsiString      Name;
    int             i_item;
    int             i;

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

    ItemActive = NULL;
    i_item = 0;

    vlc_mutex_lock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );

#define ES p_intfGlobal->p_sys->p_input->stream.pp_es[i]
    /* create a set of language buttons and append them to the Root */
    for( i = 0; i < p_intfGlobal->p_sys->p_input->stream.i_es_number; i++ )
    {
        if( ( ES->i_cat == i_cat ) &&
            ( !ES->p_pgrm ||
              ES->p_pgrm ==
                 p_intfGlobal->p_sys->p_input->stream.p_selected_program ) )
        {
            i_item++;
            Name = p_intfGlobal->p_sys->p_input->stream.pp_es[i]->psz_desc;
            if( Name.IsEmpty() )
            {
                Name.sprintf( "Language %d", i_item );
            }

            Item = new TMenuItem( Root );
            Item->RadioItem = true;
            Item->Hint = Name;
            Item->Caption = Name;
            Item->Tag = i;

            if( p_es == p_intfGlobal->p_sys->p_input->stream.pp_es[i] )
            {
                /* don't lose Item when we append into menu */
                ItemActive = Item;
            }

            /* setup signal hanling */
            Item->OnClick = MenuItemClick;
            Root->Add( Item );
        }
    }
#undef ES

    vlc_mutex_unlock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );

    /* check currently selected item */
    if( ItemActive != NULL )
    {
        ItemActive->Checked = true;
    }

    /* be sure that menu is enabled if non empty */
    if( i_item > 0 )
    {
        Root->Enabled = true;
    }
}


/*****************************************************************************
 * NavigationMenu: sets menus for titles and chapters selection
 *****************************************************************************
 * Generates two types of menus:
 *  -simple list of titles
 *  -cascaded lists of chapters for each title
 *****************************************************************************/
static void __fastcall NavigationMenu( TMenuItem * Root,
                                       TNotifyEvent MenuItemClick )
{
    TMenuItem     * TitleGroup;
    TMenuItem     * TitleItem;
    TMenuItem     * ItemActive;
    TMenuItem     * ChapterGroup;
    TMenuItem     * ChapterItem;
    AnsiString      Name;
    int             i_title;
    int             i_chapter;
    int             i_title_nb;
    int             i_chapter_nb;


    /* remove previous menu */
    Root->Enabled = false;
    Root->Clear();

    ItemActive = NULL;
    i_title_nb = p_intfGlobal->p_sys->p_input->stream.i_area_nb;
    
    /* loop on titles */
    for( i_title = 1; i_title < i_title_nb; i_title++ )
    {
        /* we group titles in packets of ten for small screens */
        if( ( i_title % 10 == 1 ) && ( i_title_nb > 20 ) )
        {
            if( i_title != 1 )
            {
                Root->Add( TitleGroup );
            }

            Name.sprintf( "%d - %d", i_title, i_title + 9 );
            TitleGroup = new TMenuItem( Root );
            TitleGroup->RadioItem = true;
            TitleGroup->Hint = Name;
            TitleGroup->Caption = Name;
        }

        Name.sprintf( "Title %d (%d)", i_title,
            p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb );

        {
            TitleItem = new TMenuItem( Root );
            TitleItem->RadioItem = true;
            TitleItem->Hint = Name;
            TitleItem->Caption = Name;

            i_chapter_nb =
                p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb;

            /* loop on chapters */
            for( i_chapter = 0; i_chapter < i_chapter_nb; i_chapter++ )
            {
                /* we group chapters in packets of ten for small screens */
                if( ( i_chapter % 10 == 0 ) && ( i_chapter_nb > 20 ) )
                {
                    if( i_chapter != 0 )
                    {
                        TitleItem->Add( ChapterGroup );
                    }

                    Name.sprintf( "%d - %d", i_chapter + 1, i_chapter + 10 );
                    ChapterGroup = new TMenuItem( TitleItem );
                    ChapterGroup->RadioItem = true;
                    ChapterGroup->Hint = Name;
                    ChapterGroup->Caption = Name;
                }

                Name.sprintf( "Chapter %d", i_chapter + 1 );

                ChapterItem = new TMenuItem( TitleItem );
                ChapterItem->RadioItem = true;
                ChapterItem->Hint = Name;
                ChapterItem->Caption = Name;

                /* FIXME: temporary hack to save i_title and i_chapter with
                 * ChapterItem, since we will need them in the callback */
                 ChapterItem->Tag = (int)POS2DATA( i_title, i_chapter + 1 );

#define p_area p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title]
                if( ( p_area ==
                        p_intfGlobal->p_sys->p_input->stream.p_selected_area ) &&
                    ( p_area->i_part == i_chapter + 1 ) )
                {
                    ItemActive = ChapterItem;
                }
#undef p_area

                /* setup signal hanling */
                ChapterItem->OnClick = MenuItemClick;

                if( i_chapter_nb > 20 )
                {
                    ChapterGroup->Add( ChapterItem );
                }
                else
                {
                    TitleItem->Add( ChapterItem );
                }
            }

            if( i_chapter_nb > 20 )
            {
                TitleItem->Add( ChapterGroup );
            }

            if( p_intfGlobal->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb
                > 1 )
            {
                /* be sure that menu is sensitive */
                Root->Enabled = true;
            }
        }

        if( i_title_nb > 20 )
        {
            TitleGroup->Add( TitleItem );
        }
        else
        {
            Root->Add( TitleItem );
        }
    }

    if( i_title_nb > 20 )
    {
        Root->Add( TitleGroup );
    }

    /* Default selected chapter */
    if( ItemActive != NULL )
    {
        ItemActive->Checked = true;
    }

    /* be sure that menu is sensitive */
    Root->Enabled = true;
}


/*****************************************************************************
 * SetupMenus: function that generates title/chapter/audio/subpic
 * menus with help from preceding functions
 *****************************************************************************/
int __fastcall SetupMenus( intf_thread_t * p_intf )
{
    TMainFrameDlg     * p_window = p_intf->p_sys->p_window;
    es_descriptor_t   * p_audio_es;
    es_descriptor_t   * p_spu_es;
    int                 i;

    p_intf->p_sys->b_chapter_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_audio_update |= p_intf->p_sys->b_title_update |
                                     p_intf->p_sys->b_program_update;
    p_intf->p_sys->b_spu_update |= p_intf->p_sys->b_title_update |
                                   p_intf->p_sys->b_program_update;

    if( p_intf->p_sys->b_program_update )
    { 
        pgrm_descriptor_t * p_pgrm;

        if( p_intfGlobal->p_sys->p_input->stream.p_new_program )
        {
            p_pgrm = p_intfGlobal->p_sys->p_input->stream.p_new_program;
        }
        else
        {
            p_pgrm = p_intfGlobal->p_sys->p_input->stream.p_selected_program;
        }

        ProgramMenu( p_window->MenuProgram, p_pgrm,
                     p_window->MenuProgramClick );
        ProgramMenu( p_window->PopupProgram, p_pgrm,
                     p_window->PopupProgramClick );

        p_intf->p_sys->b_program_update = 0;
    }

    if( p_intf->p_sys->b_title_update )
    {
        RadioMenu( p_window->MenuTitle, "Title",
//why "-1" ?
                   p_intfGlobal->p_sys->p_input->stream.i_area_nb - 1,
                   p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_id,
                   p_window->MenuTitleClick );

        AnsiString CurrentTitle;
        CurrentTitle.sprintf( "%d",
                    p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_id );
        p_window->LabelTitleCurrent->Caption = CurrentTitle;

        p_intf->p_sys->b_title_update = 0;
    }

    if( p_intf->p_sys->b_chapter_update )
    {
        RadioMenu( p_window->MenuChapter, "Chapter",
                   p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_part_nb,
                   p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_part,
                   p_window->MenuChapterClick );

        NavigationMenu( p_window->PopupNavigation,
                        p_window->PopupNavigationClick );

        AnsiString CurrentChapter;
        CurrentChapter.sprintf( "%d",
                    p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_part );
        p_window->LabelChapterCurrent->Caption = CurrentChapter;

        p_intf->p_sys->i_part =
                    p_intfGlobal->p_sys->p_input->stream.p_selected_area->i_part;

        p_intf->p_sys->b_chapter_update = 0;
    }

    /* look for selected ES */
    p_audio_es = NULL;
    p_spu_es = NULL;

     for( i = 0; i < p_intfGlobal->p_sys->p_input->stream.i_selected_es_number; i++ )
    {
        if( p_intfGlobal->p_sys->p_input->stream.pp_selected_es[i]->i_cat
            == AUDIO_ES )
        {
            p_audio_es = p_intfGlobal->p_sys->p_input->stream.pp_selected_es[i];
            p_intfGlobal->p_sys->p_audio_es_old = p_audio_es;
        }

        if( p_intfGlobal->p_sys->p_input->stream.pp_selected_es[i]->i_cat
            == SPU_ES )
        {
            p_spu_es = p_intfGlobal->p_sys->p_input->stream.pp_selected_es[i];
            p_intfGlobal->p_sys->p_spu_es_old = p_spu_es;
        }
    }

    vlc_mutex_unlock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );

    /* audio menus */
    if( p_intf->p_sys->b_audio_update )
    {
        LanguageMenu( p_window->MenuAudio, p_audio_es, AUDIO_ES,
                      p_window->MenuAudioClick );
        LanguageMenu( p_window->PopupAudio, p_audio_es, AUDIO_ES,
                      p_window->PopupAudioClick );

        p_intf->p_sys->b_audio_update = 0;
    }

    /* sub picture menus */
    if( p_intf->p_sys->b_spu_update )
    {
        LanguageMenu( p_window->PopupSubtitles, p_spu_es, SPU_ES,
                      p_window->PopupSubtitleClick );
        LanguageMenu( p_window->MenuSubtitles, p_spu_es, SPU_ES,
                      p_window->MenuSubtitleClick );

        p_intf->p_sys->b_spu_update = 0;
    }

    vlc_mutex_lock( &p_intfGlobal->p_sys->p_input->stream.stream_lock );

    return true;
}



