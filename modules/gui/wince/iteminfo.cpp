/*****************************************************************************
 * iteminfo.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include "wince.h"

#include <winuser.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
ItemInfoDialog::ItemInfoDialog( intf_thread_t *p_intf, CBaseWindow *p_parent,
                                HINSTANCE h_inst,
                                playlist_item_t *_p_item )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    p_item = _p_item;
}

/***********************************************************************

FUNCTION:
  WndProc

PURPOSE:
  Processes messages sent to the main window.
 
***********************************************************************/
LRESULT ItemInfoDialog::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;
    SHMENUBARINFO mbi;
    INITCOMMONCONTROLSEX iccex;
    RECT rcClient;
    char *psz_uri;

    switch( msg )
    {
    case WM_INITDIALOG:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        //Create the menubar.
        memset( &mbi, 0, sizeof (SHMENUBARINFO) );
        mbi.cbSize     = sizeof (SHMENUBARINFO);
        mbi.hwndParent = hwnd;
        mbi.dwFlags    = SHCMBF_EMPTYBAR;
        mbi.hInstRes   = hInst;

        if( !SHCreateMenuBar(&mbi) )
        {
            MessageBox( hwnd, _T("SHCreateMenuBar Failed"), _T("Error"), MB_OK );
            //return -1;
        }

        hwndCB = mbi.hwndMB;

        // Get the client area rect to put the panels in
        GetClientRect( hwnd, &rcClient );

        /* URI Textbox */
        uri_label = CreateWindow( _T("STATIC"), _T("URI:"),
                        WS_CHILD | WS_VISIBLE | SS_RIGHT,
                        0, 10, 60, 15, hwnd, NULL, hInst, NULL);

        psz_uri = input_item_GetURI( p_item->p_input );
        uri_text = CreateWindow( _T("EDIT"), _FROMMB(psz_uri),
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
            70, 10 - 3, rcClient.right - 70 - 10, 15 + 6, hwnd, 0, hInst, 0 );
        free( psz_uri );

        /* Name Textbox */
        name_label = CreateWindow( _T("STATIC"), _T("Name:"),
                                   WS_CHILD | WS_VISIBLE | SS_RIGHT ,
                                   0, 10 + 15 + 10, 60, 15,
                                   hwnd, NULL, hInst, NULL);

        name_text = CreateWindow( _T("EDIT"),
            _FROMMB(p_item->p_input->psz_name),
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
            70, 10 + 15 + 10 - 3, rcClient.right - 70 - 10, 15 + 6,
            hwnd, NULL, hInst, NULL);

        /* CheckBox */
        checkbox_label = CreateWindow( _T("STATIC"), _T("Item Enabled:"),
            WS_CHILD | WS_VISIBLE | SS_RIGHT ,
            rcClient.right - 15 - 10 - 90 - 10, 10 + 4*( 15 + 10 ) + 5, 90, 15,
            hwnd, NULL, hInst, NULL );

        enabled_checkbox = CreateWindow( _T("BUTTON"), _T("Item Enabled"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            rcClient.right - 15 - 10, 10 + 4*( 15 + 10 ) + 5, 15, 15,
            hwnd, NULL, hInst, NULL );

        SendMessage( enabled_checkbox, BM_SETCHECK,
                     (p_item->i_flags & PLAYLIST_DBL_FLAG) ?
                     BST_UNCHECKED : BST_CHECKED, 0 );

        /* Treeview */
        iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
        iccex.dwICC = ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx( &iccex );

        // Create the tree-view control.
        info_tree = CreateWindowEx( 0, WC_TREEVIEW, NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES |
            TVS_LINESATROOT | TVS_HASBUTTONS,
            0, rcClient.bottom/2, rcClient.right,
            rcClient.bottom - rcClient.bottom/2 - MENU_HEIGHT + 2, // +2 to fix
            hwnd, NULL, hInst, NULL );

        UpdateInfo();
        break;

    case WM_CLOSE:
        EndDialog( hwnd, LOWORD( wp ) );
        break;

    case WM_SETFOCUS:
        SHSipPreference( hwnd, SIP_DOWN );
        SHFullScreen( hwnd, SHFS_HIDESIPBUTTON );
        break;

    case WM_COMMAND:
        if( LOWORD(wp) == IDOK )
        {
            OnOk();
            EndDialog( hwnd, LOWORD( wp ) );
        }
        break;

    default:
        break;
    }

    return FALSE;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
 void ItemInfoDialog::UpdateInfo()
{
    TVITEM tvi = {0};
    TVINSERTSTRUCT tvins = {0};
    HTREEITEM hPrev = (HTREEITEM)TVI_FIRST;
    HTREEITEM hPrevRootItem = NULL;
    HTREEITEM hPrevLev2Item = NULL;

    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;

    // Set the text of the item.
    tvi.pszText = _FROMMB(p_item->p_input->psz_name);
    tvi.cchTextMax = _tcslen(tvi.pszText);

    // Save the heading level in the item's application-defined data area
    tvi.lParam = (LPARAM)1; // root level
    tvins.item = tvi;
    tvins.hInsertAfter = hPrev;
    tvins.hParent = TVI_ROOT;

    // Add the item to the tree-view control.
    hPrev = (HTREEITEM)TreeView_InsertItem( info_tree, &tvins );
    hPrevRootItem = hPrev;

    /* Rebuild the tree */
    vlc_mutex_lock( &p_item->p_input->lock );
    for( int i = 0; i < p_item->p_input->i_categories; i++ )
    {
        info_category_t *p_cat = p_item->p_input->pp_categories[i];

        // Set the text of the item.
        tvi.pszText = _FROMMB( p_item->p_input->psz_name );
        tvi.cchTextMax = _tcslen( tvi.pszText );
 
        // Save the heading level in the item's application-defined data area
        tvi.lParam = (LPARAM)2; // level 2
        tvins.item = tvi;
        tvins.hInsertAfter = hPrev;
        tvins.hParent = hPrevRootItem;

        // Add the item to the tree-view control.
        hPrev = (HTREEITEM)TreeView_InsertItem( info_tree, &tvins );

        hPrevLev2Item = hPrev;

        for( int j = 0; j < p_cat->i_infos; j++ )
        {
            info_t *p_info = p_cat->pp_infos[j];

            // Set the text of the item.
            string szAnsi = (string)p_info->psz_name;
            szAnsi += ": ";
            szAnsi += p_info->psz_value;
            tvi.pszText = (TCHAR *)_FROMMB( szAnsi.c_str() );
            tvi.cchTextMax = _tcslen( tvi.pszText );
            tvi.lParam = (LPARAM)3; // level 3
            tvins.item = tvi;
            tvins.hInsertAfter = hPrev;
            tvins.hParent = hPrevLev2Item;
 
            // Add the item to the tree-view control.
            hPrev = (HTREEITEM)TreeView_InsertItem( info_tree, &tvins );
        }

        TreeView_Expand( info_tree, hPrevLev2Item,
                         TVE_EXPANDPARTIAL |TVE_EXPAND );
    }
    vlc_mutex_unlock( &p_item->p_input->lock );

    TreeView_Expand( info_tree, hPrevRootItem, TVE_EXPANDPARTIAL |TVE_EXPAND );
}

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void ItemInfoDialog::OnOk()
{
    int b_state = false;

    TCHAR psz_name[MAX_PATH];
    Edit_GetText( name_text, psz_name, MAX_PATH );
    input_item_SetName( p_item->p_input, _TOMB( psz_name ) );

    TCHAR psz_uri[MAX_PATH];
    Edit_GetText( uri_text, psz_uri, MAX_PATH );
    input_item_SetURI( p_item->p_input, _TOMB(psz_uri) );

    vlc_mutex_lock( &p_item->p_input->lock );
    bool b_old_enabled = !(p_item->i_flags & PLAYLIST_DBL_FLAG);

    playlist_t * p_playlist = pl_Hold( p_intf );
    if( p_playlist != NULL )
    {
        b_state = SendMessage( enabled_checkbox, BM_GETCHECK, 0, 0 );
        pl_Release( p_intf );
    }

    p_item->i_flags |= (b_state & BST_CHECKED) ? false : PLAYLIST_DBL_FLAG ;

    vlc_mutex_unlock( &p_item->p_input->lock );
}
