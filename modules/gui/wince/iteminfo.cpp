/*****************************************************************************
 * iteminfo.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

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

        uri_text = CreateWindow( _T("EDIT"), _FROMMB(p_item->input.psz_uri),
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
            70, 10 - 3, rcClient.right - 70 - 10, 15 + 6, hwnd, 0, hInst, 0 );

        /* Name Textbox */
        name_label = CreateWindow( _T("STATIC"), _T("Name:"),
                                   WS_CHILD | WS_VISIBLE | SS_RIGHT ,
                                   0, 10 + 15 + 10, 60, 15,
                                   hwnd, NULL, hInst, NULL);

        name_text = CreateWindow( _T("EDIT"),
            _FROMMB(p_item->input.psz_name),
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
                     p_item->b_enabled ? BST_CHECKED : BST_UNCHECKED, 0 );

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
    tvi.pszText = _FROMMB(p_item->input.psz_name);
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
    vlc_mutex_lock( &p_item->input.lock );
    for( int i = 0; i < p_item->input.i_categories; i++ )
    {
        info_category_t *p_cat = p_item->input.pp_categories[i];

        // Set the text of the item. 
        tvi.pszText = _FROMMB( p_item->input.psz_name );
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
    vlc_mutex_unlock( &p_item->input.lock );

    TreeView_Expand( info_tree, hPrevRootItem, TVE_EXPANDPARTIAL |TVE_EXPAND );
}

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void ItemInfoDialog::OnOk()
{
    int b_state = VLC_FALSE;

    vlc_mutex_lock( &p_item->input.lock );

    TCHAR psz_name[MAX_PATH];
    Edit_GetText( name_text, psz_name, MAX_PATH );
    if( p_item->input.psz_name ) free( p_item->input.psz_name );
    p_item->input.psz_name = strdup( _TOMB(psz_name) );

    TCHAR psz_uri[MAX_PATH];
    Edit_GetText( uri_text, psz_uri, MAX_PATH );
    if( p_item->input.psz_uri ) free( p_item->input.psz_uri );
    p_item->input.psz_uri = strdup( _TOMB(psz_uri) );

    vlc_bool_t b_old_enabled = p_item->b_enabled;

    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        b_state = SendMessage( enabled_checkbox, BM_GETCHECK, 0, 0 );
        if( b_old_enabled == VLC_FALSE && (b_state & BST_CHECKED) )
            p_playlist->i_enabled ++;
        else if( b_old_enabled == VLC_TRUE && (b_state & BST_UNCHECKED) )
            p_playlist->i_enabled --;

        vlc_object_release( p_playlist );
    }

    p_item->b_enabled = (b_state & BST_CHECKED) ? VLC_TRUE : VLC_FALSE ;

    vlc_mutex_unlock( &p_item->input.lock );
}
