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
#include "dragdrop.h"
#include "misc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
#pragma resource "*.dfm"

//---------------------------------------------------------------------------
_fastcall TPlaylistDlg::TPlaylistDlg(
    TComponent* Owner, intf_thread_t *_p_intf ) : TForm( Owner )
{
    p_intf = _p_intf;
    Icon = p_intf->p_sys->p_window->Icon;
    Translate( this );

    /* store a pointer to the core playlist */
    p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        msg_Err( p_intf, "cannot find a playlist object" );
    }

    /* drag and drop stuff */

    /* initialize the OLE library */
    OleInitialize( NULL );
    /* TDropTarget will send the WM_OLEDROP message to the form */
    lpDropTarget = (LPDROPTARGET)new TDropTarget( this->Handle );
    CoLockObjectExternal( lpDropTarget, true, true );
    /* register the listview as a drop target */
    RegisterDragDrop( ListViewPlaylist->Handle, lpDropTarget );
}
//---------------------------------------------------------------------------
__fastcall TPlaylistDlg::~TPlaylistDlg()
{
    /* release the core playlist */
    vlc_object_release( p_playlist );

    /* remove the listview from the list of drop targets */
    RevokeDragDrop( ListViewPlaylist->Handle );
    lpDropTarget->Release();
    CoLockObjectExternal( lpDropTarget, false, true );
    /* uninitialize the OLE library */
    OleUninitialize();
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
 * External drop handling
 *****************************************************************************/
void __fastcall TPlaylistDlg::OnDrop( TMessage &Msg )
{
    p_intf->p_sys->p_window->OnDrop( Msg );
}
//--------------------------------------------------------------------------


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

    if( p_playlist == NULL )
        return;

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

        playlist_Goto( p_playlist, Item->Index );
    }
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
        Sender->Canvas->Brush->Color = clRed;
    else
        Sender->Canvas->Brush->Color = clWhite;

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
    /* user wants to delete a file in the queue */
    int i_pos;

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

    /* rebuild the ListView */
    UpdateGrid();

    vlc_mutex_unlock( &p_intf->change_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteAllActionExecute( TObject *Sender )
{
    int i_pos;

    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        DeleteItem( i_pos );
    }

    /* Rebuild the ListView */
    UpdateGrid();

    vlc_mutex_unlock( &p_intf->change_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::InvertSelectionActionExecute( TObject *Sender )
{
#define NOT( var ) ( (var) ? false : true )
    int         i_pos;
    TListItems *Items = ListViewPlaylist->Items;

    /* delete the items from the last to the first */
    for( i_pos = p_playlist->i_size - 1; i_pos >= 0; i_pos-- )
    {
        Items->Item[i_pos]->Selected = NOT( Items->Item[i_pos]->Selected );
    }
#undef NOT
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::CropSelectionActionExecute( TObject *Sender )
{
    InvertSelectionActionExecute( Sender );
    DeleteSelectionActionExecute( Sender );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Useful functions, needed by the event handlers or by other windows
 ****************************************************************************/
void __fastcall TPlaylistDlg::Add( AnsiString FileName, int i_mode, int i_pos )
{
    if( p_playlist == NULL )
        return;

    playlist_Add( p_playlist, FileName.c_str(), i_mode, i_pos );
    
    /* refresh the display */
    UpdateGrid();
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Stop()
{
    if( p_playlist == NULL )
        return;

    playlist_Stop( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Play()
{
    if( p_playlist == NULL )
        return;

    if ( p_playlist->i_size )
        playlist_Play( p_playlist );
    else
        Show();
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Pause()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Slow()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Fast()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::UpdateGrid()
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
    if( p_playlist == NULL )
        return;

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_intf->p_sys->i_playing != p_playlist->i_index )
    {
        p_intf->p_sys->i_playing = p_playlist->i_index;

        /* update the background color */
        UpdateGrid();
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::DeleteItem( int i_pos )
{
    if( p_playlist == NULL )
        return;

    playlist_Delete( p_playlist, i_pos );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Previous()
{
    if( p_playlist == NULL )
        return;

    playlist_Prev( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TPlaylistDlg::Next()
{
    if( p_playlist == NULL )
        return;

    playlist_Next( p_playlist );
}
//---------------------------------------------------------------------------


void __fastcall TPlaylistDlg::MenuFileCloseClick(TObject *Sender)
{
    Hide();
}
//---------------------------------------------------------------------------

void __fastcall TPlaylistDlg::MenuFileOpenClick(TObject *Sender)
{
    if ( PlaylistOpenDlg->Execute() )
    {
        playlist_LoadFile ( p_playlist , PlaylistOpenDlg->FileName.c_str() );
        UpdateGrid();
    }
}
//---------------------------------------------------------------------------

void __fastcall TPlaylistDlg::MenuFileSaveClick(TObject *Sender)
{
    if ( PlaylistSaveDlg->Execute() )
    {
        playlist_SaveFile ( p_playlist , PlaylistSaveDlg->FileName.c_str() );
    }
}
//---------------------------------------------------------------------------

