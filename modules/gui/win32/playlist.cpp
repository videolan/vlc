/*****************************************************************************
 * playlist.cpp: Interface for the playlist dialog
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "playlist.h"
#include "misc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

//---------------------------------------------------------------------------
__fastcall TPlaylistDlg::TPlaylistDlg(
    TComponent* Owner, intf_thread_t *_p_intf ) : TForm( Owner )
{
    p_intf = _p_intf;
    Icon = p_intf->p_sys->p_window->Icon;
    Translate( this );
}
//---------------------------------------------------------------------------
char * __fastcall TPlaylistDlg::rindex( char *s, char c )
{
    char *ref = s;

    s = s + strlen( s ) - 2;
    while( ( s > ref ) && ( *s != c ) )
    {
        s--;
    }
    if( *s == c )
    {
        return( s );
    }
    else
    {
        return( NULL );
    }
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Event handlers
 ****************************************************************************/
void __fastcall TPlaylistDlg::FormShow( TObject *Sender )
{
    p_intf->p_sys->p_window->PlaylistAction->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::FormHide( TObject *Sender )
{
    p_intf->p_sys->p_window->PlaylistAction->Checked = false;
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::BitBtnOkClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::PlayStreamActionExecute( TObject *Sender )
{
    TListItem *Item;
    TListItem *ItemStart;
    TItemStates Focused;

    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* search the selected item */
    if( ListViewPlaylist->SelCount > 0 )
    {
        if( ListViewPlaylist->SelCount == 1 )
        {
            Item = ListViewPlaylist->Selected;
        }
        else
        {
            Focused << isFocused;
            ItemStart = ListViewPlaylist->Items->Item[0];

            Item = ListViewPlaylist->GetNextItem( ItemStart, sdAll, Focused );
        }

        playlist_Goto( p_playlist, Item->Index - 1 );
    }

    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::ListViewPlaylistKeyDown( TObject *Sender,
        WORD &Key, TShiftState Shift )
{
    /* 'suppr' or 'backspace' */
    if( ( Key == VK_DELETE ) || ( Key == VK_BACK ) )
    {
        DeleteSelectionActionExecute( Sender );
    }

    /* 'enter' */
    if( Key == VK_RETURN )
    {
        PlayStreamActionExecute( Sender );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::ListViewPlaylistCustomDrawItem(
        TCustomListView *Sender, TListItem *Item, TCustomDrawState State,
        bool &DefaultDraw)
{
    TRect Rect = Item->DisplayRect( drBounds );

    /* set the background color */
    if( Item->Index == p_intf->p_sys->i_playing )
    {
        Sender->Canvas->Brush->Color = clRed;
    }
    else
    {
        Sender->Canvas->Brush->Color = clWhite;
    }

    Sender->Canvas->FillRect( Rect );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Menu and popup callbacks
 ****************************************************************************/
void __fastcall TPlaylistDlg::MenuAddFileClick( TObject *Sender )
{
    p_intf->p_sys->p_window->OpenFileActionExecute( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddDiscClick( TObject *Sender )
{
    p_intf->p_sys->p_window->OpenDiscActionExecute( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddNetClick( TObject *Sender )
{
    p_intf->p_sys->p_window->NetworkStreamActionExecute( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddUrlClick( TObject *Sender )
{
    /* TODO */
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteSelectionActionExecute( TObject *Sender )
{
#if 0 /* PLAYLIST TARASS */
    /* user wants to delete a file in the queue */
    int         i_pos;
    playlist_t *p_playlist = p_intf->p_vlc->p_playlist;

    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        if( ListViewPlaylist->Items->Item[i_pos]->Selected )
        {
            DeleteItem( i_pos );
        }
    }

    /* Rebuild the ListView */
    UpdateGrid( p_playlist );

    vlc_mutex_unlock( &p_intf->change_lock );
#endif
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteAllActionExecute( TObject *Sender )
{
#if 0 /* PLAYLIST TARASS */
    int         i_pos;
    playlist_t *p_playlist = p_intf->p_vlc->p_playlist;

    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        DeleteItem( i_pos );
    }

    /* Rebuild the ListView */
    UpdateGrid( p_playlist );

    vlc_mutex_unlock( &p_intf->change_lock );
#endif
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::InvertSelectionActionExecute( TObject *Sender )
{
#if 0 /* PLAYLIST TARASS */
#define NOT( var ) ( (var) ? false : true )
    int         i_pos;
    playlist_t *p_playlist = p_intf->p_vlc->p_playlist;
    TListItems *Items = ListViewPlaylist->Items;

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        Items->Item[i_pos]->Selected = NOT( Items->Item[i_pos]->Selected );
    }
#undef NOT
#endif
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::CropSelectionActionExecute( TObject *Sender )
{
    InvertSelectionActionExecute( Sender );
    DeleteSelectionActionExecute( Sender );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Useful functions, needed by the event handlers
 ****************************************************************************/
void __fastcall TPlaylistDlg::UpdateGrid( playlist_t * p_playlist )
{
    int i_dummy;
    char *FileName;
    TListItem *Item;

    ListViewPlaylist->Items->BeginUpdate();

    /* Clear the list... */
    ListViewPlaylist->Items->Clear();

    /* ...and rebuild it */
    for( i_dummy = 0; i_dummy < p_playlist->i_size; i_dummy++ )
    {
#ifdef WIN32
        /* Position of the last '\' in the string */
        FileName = rindex( p_playlist->pp_items[i_dummy]->psz_name, '\\' );
#else
        /* Position of the last '/' in the string */
        FileName = rindex( p_playlist->pp_items[i_dummy]->psz_name, '/' );
#endif
        if( ( FileName == NULL ) || ( *(FileName + 1) == '\0' ) )
        {
            FileName = p_playlist->pp_items[i_dummy]->psz_name;
        }
        else
        {
            /* Skip leading '\' or '/' */
            FileName++;
        }

        Item = ListViewPlaylist->Items->Add();
        Item->Caption = FileName;
        Item->SubItems->Add( "no info" );
    }
    /* TODO: Set background color ? */

    ListViewPlaylist->Items->EndUpdate();
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Manage()
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_intf->p_sys->i_playing != p_playlist->i_index )
    {
        p_intf->p_sys->i_playing = p_playlist->i_index;

        /* update the background color */
        UpdateGrid( p_playlist );
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteItem( int i_pos )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Delete( p_playlist, i_pos );
    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Previous()
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Next()
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------

