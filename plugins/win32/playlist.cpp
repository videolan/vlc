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
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern intf_thread_t *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TPlaylistDlg::TPlaylistDlg( TComponent* Owner )
        : TForm( Owner )
{
    Icon = p_intfGlobal->p_sys->p_window->Icon;
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
    p_intfGlobal->p_sys->p_window->MenuPlaylist->Checked = true;
    p_intfGlobal->p_sys->p_window->PopupPlaylist->Checked = true;
    p_intfGlobal->p_sys->p_window->ToolButtonPlaylist->Down = true;
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuPlaylist->Checked = false;
    p_intfGlobal->p_sys->p_window->PopupPlaylist->Checked = false;
    p_intfGlobal->p_sys->p_window->ToolButtonPlaylist->Down = false;
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::BitBtnOkClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::ListViewPlaylistDblClick( TObject *Sender )
{
    TListItem *Item;
    TListItem *ItemStart;
    TItemStates Focused;

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

        /* stop current item, select the good one */
        if( ( p_intfGlobal->p_vlc->p_input_bank->pp_input[0] != NULL ) &&
            ( Item->Index != p_intfGlobal->p_sys->i_playing ) )
        {
            /* FIXME: temporary hack */
            p_intfGlobal->p_vlc->p_input_bank->pp_input[0]->b_eof = 1;
        }
        intf_PlaylistJumpto( p_intfGlobal->p_vlc->p_playlist, Item->Index - 1 );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::ListViewPlaylistKeyDown( TObject *Sender,
        WORD &Key, TShiftState Shift )
{
    /* 'suppr' or 'backspace' */
    if( ( Key == VK_DELETE ) || ( Key == VK_BACK ) )
    {
        MenuDeleteSelectedClick( Sender );
    }

    /* 'enter' */
    if( Key == VK_RETURN )
    {
        PopupPlayClick( Sender );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::ListViewPlaylistCustomDrawItem(
        TCustomListView *Sender, TListItem *Item, TCustomDrawState State,
        bool &DefaultDraw)
{
    TRect Rect = Item->DisplayRect( drBounds );

    /* set the background color */
    if( Item->Index == p_intfGlobal->p_sys->i_playing )
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
 * Menu callbacks
 ****************************************************************************/
void __fastcall TPlaylistDlg::MenuAddFileClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuOpenFileClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddDiscClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuOpenDiscClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddNetClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuNetworkStreamClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuAddUrlClick( TObject *Sender )
{
    /* TODO */
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuDeleteSelectedClick( TObject *Sender )
{
    /* user wants to delete a file in the queue */
    int         i_pos;
    playlist_t *p_playlist = p_intfGlobal->p_vlc->p_playlist;

    /* lock the struct */
    vlc_mutex_lock( &p_intfGlobal->change_lock );

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

    vlc_mutex_unlock( &p_intfGlobal->change_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuDeleteAllClick( TObject *Sender )
{
    int         i_pos;
    playlist_t *p_playlist = p_intfGlobal->p_vlc->p_playlist;

    /* lock the struct */
    vlc_mutex_lock( &p_intfGlobal->change_lock );

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        DeleteItem( i_pos );
    }

    /* Rebuild the ListView */
    UpdateGrid( p_playlist );

    vlc_mutex_unlock( &p_intfGlobal->change_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuSelectionInvertClick( TObject *Sender )
{
#define NOT( var ) ( (var) ? false : true )
    int         i_pos;
    playlist_t *p_playlist = p_intfGlobal->p_vlc->p_playlist;
    TListItems *Items = ListViewPlaylist->Items;

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        Items->Item[i_pos]->Selected = NOT( Items->Item[i_pos]->Selected );
    }
#undef NOT
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::MenuSelectionCropClick( TObject *Sender )
{
    MenuSelectionInvertClick( Sender );
    MenuDeleteSelectedClick( Sender );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Popup callbacks
 ****************************************************************************/
void __fastcall TPlaylistDlg::PopupPlayClick( TObject *Sender )
{
    ListViewPlaylistDblClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::PopupInvertSelectionClick( TObject *Sender )
{
    MenuSelectionInvertClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::PopupCropSelectionClick( TObject *Sender )
{
    MenuSelectionCropClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::PopupDeleteSelectedClick( TObject *Sender )
{
    MenuDeleteSelectedClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::PopupDeleteAllClick( TObject *Sender )
{
    MenuDeleteAllClick( Sender );
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
        FileName = rindex( p_playlist->p_item[i_dummy].psz_name, '\\' );
#else
        /* Position of the last '/' in the string */
        FileName = rindex( p_playlist->p_item[i_dummy].psz_name, '/' );
#endif
        if( ( FileName == NULL ) || ( *(FileName + 1) == '\0' ) )
        {
            FileName = p_playlist->p_item[i_dummy].psz_name;
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
void __fastcall TPlaylistDlg::Manage( intf_thread_t * p_intf )
{
    playlist_t *p_playlist = p_intfGlobal->p_vlc->p_playlist ;

    vlc_mutex_lock( &p_playlist->change_lock );

    if( p_intf->p_sys->i_playing != p_playlist->i_index )
    {
        p_intf->p_sys->i_playing = p_playlist->i_index;

        /* update the background color */
        UpdateGrid( p_playlist );
    }

    vlc_mutex_unlock( &p_playlist->change_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteItem( int i_pos )
{
    intf_PlaylistDelete( p_intfGlobal->p_vlc->p_playlist, i_pos );

    /* are we deleting the current played stream */
    if( p_intfGlobal->p_sys->i_playing == i_pos )
    {
        /* next ! */
        p_intfGlobal->p_vlc->p_input_bank->pp_input[0]->b_eof = 1;
        /* this has to set the slider to 0 */
        
        /* step minus one */
        p_intfGlobal->p_sys->i_playing-- ;

        vlc_mutex_lock( &p_intfGlobal->p_vlc->p_playlist->change_lock );
        p_intfGlobal->p_vlc->p_playlist->i_index-- ;
        vlc_mutex_unlock( &p_intfGlobal->p_vlc->p_playlist->change_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Previous()
{
    if( p_intfGlobal->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_intfGlobal->p_vlc->p_playlist );
        intf_PlaylistPrev( p_intfGlobal->p_vlc->p_playlist );
        p_intfGlobal->p_vlc->p_input_bank->pp_input[0]->b_eof = 1;
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Next()
{
    if( p_intfGlobal->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        /* FIXME: temporary hack */
        p_intfGlobal->p_vlc->p_input_bank->pp_input[0]->b_eof = 1;
    }
}
//---------------------------------------------------------------------------

