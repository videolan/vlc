/*****************************************************************************
 * playlist.h: Interface for the playlist dialog
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

#ifndef playlistH
#define playlistH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Menus.hpp>
#include <Grids.hpp>
#include <ComCtrls.hpp>
#include <ActnList.hpp>

//---------------------------------------------------------------------------
class TPlaylistDlg : public TForm
{
__published:	// IDE-managed Components
    TBitBtn *BitBtnOk;
    TMainMenu *MainMenuPlaylist;
    TMenuItem *MenuAdd;
    TMenuItem *MenuAddFile;
    TMenuItem *MenuAddDisc;
    TMenuItem *MenuAddNet;
    TMenuItem *MenuAddUrl;
    TMenuItem *MenuDelete;
    TMenuItem *MenuDeleteAll;
    TMenuItem *MenuDeleteSelected;
    TMenuItem *MenuSelection;
    TMenuItem *MenuSelectionCrop;
    TMenuItem *MenuSelectionInvert;
    TListView *ListViewPlaylist;
    TPopupMenu *PopupMenuPlaylist;
    TMenuItem *PopupPlay;
    TMenuItem *N1;
    TMenuItem *PopupDeleteAll;
    TMenuItem *PopupDeleteSelected;
    TMenuItem *N2;
    TMenuItem *PopupInvertSelection;
    TMenuItem *PopupCropSelection;
    TActionList *ActionList1;
    TAction *InvertSelectionAction;
    TAction *CropSelectionAction;
    TAction *DeleteSelectionAction;
    TAction *DeleteAllAction;
    TAction *PlayStreamAction;
    void __fastcall FormShow( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall BitBtnOkClick( TObject *Sender );
    void __fastcall ListViewPlaylistKeyDown( TObject *Sender, WORD &Key,
            TShiftState Shift );
    void __fastcall ListViewPlaylistCustomDrawItem( TCustomListView *Sender,
            TListItem *Item, TCustomDrawState State, bool &DefaultDraw );
    void __fastcall MenuAddFileClick( TObject *Sender );
    void __fastcall MenuAddDiscClick( TObject *Sender );
    void __fastcall MenuAddNetClick( TObject *Sender );
    void __fastcall MenuAddUrlClick( TObject *Sender );
    void __fastcall InvertSelectionActionExecute( TObject *Sender );
    void __fastcall CropSelectionActionExecute( TObject *Sender );
    void __fastcall DeleteSelectionActionExecute( TObject *Sender );
    void __fastcall DeleteAllActionExecute( TObject *Sender );
    void __fastcall PlayStreamActionExecute( TObject *Sender );
private:	// User declarations
    char * __fastcall rindex( char *s, char c );
public:		// User declarations
    __fastcall TPlaylistDlg( TComponent* Owner );
    void __fastcall UpdateGrid( playlist_t * p_playlist );
    void __fastcall Manage( intf_thread_t * p_intf );
    void __fastcall DeleteItem( int i_pos );
    void __fastcall Previous();
    void __fastcall Next();
};
//---------------------------------------------------------------------------
#endif
