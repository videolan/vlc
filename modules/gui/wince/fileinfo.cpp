/*****************************************************************************
 * fileinfo.cpp : WinCE gui plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
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
 * Constructor.
 *****************************************************************************/
FileInfo::FileInfo( intf_thread_t *p_intf, CBaseWindow *p_parent,
                    HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    hwnd_fileinfo = hwndTV = NULL;
}

/***********************************************************************

FUNCTION: 
  CreateTreeView

PURPOSE: 
  Registers the TreeView control class and creates a TreeView.

***********************************************************************/
BOOL FileInfo::CreateTreeView(HWND hwnd)
{
    DWORD dwStyle;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_TREEVIEW_CLASSES;

    // Registers Statusbar control classes from the common control dll
    InitCommonControlsEx( &iccex );

    // Get the coordinates of the parent window's client area
    GetClientRect( hwnd, &rect );

    // Assign the window styles for the tree view.
    dwStyle = WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | 
                          TVS_HASBUTTONS;

    // Create the tree-view control.
    hwndTV = CreateWindowEx( 0, WC_TREEVIEW, NULL, dwStyle, 0, MENU_HEIGHT,
                             rect.right-rect.left,
                             rect.bottom-rect.top-MENU_HEIGHT,
                             hwnd, NULL, hInst, NULL );

    // Be sure that the tree view actually was created.
    if( !hwndTV ) return FALSE;

    UpdateFileInfo();

    return TRUE;
}

/***********************************************************************

FUNCTION: 
  UpdateFileInfo

PURPOSE: 
  Update the TreeView with file information.

***********************************************************************/
void FileInfo::UpdateFileInfo()
{
    TVITEM tvi = {0}; 
    TVINSERTSTRUCT tvins = {0}; 
    HTREEITEM hPrev = (HTREEITEM)TVI_FIRST; 
    HTREEITEM hPrevRootItem = NULL; 
    HTREEITEM hPrevLev2Item = NULL; 

    p_intf->p_sys->p_input = (input_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    input_thread_t *p_input = p_intf->p_sys->p_input;

    if( !p_input ) return;

    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM; 

    // Set the text of the item.
    tvi.pszText = _FROMMB( p_input->input.p_item->psz_name );
    tvi.cchTextMax = _tcslen( tvi.pszText );

    // Save the heading level in the item's application-defined data area
    tvi.lParam = (LPARAM)1;
    tvins.item = tvi; 
    //tvins.hInsertAfter = TVI_LAST;
    tvins.hInsertAfter = hPrev; 
    tvins.hParent = TVI_ROOT; 

    // Add the item to the tree-view control. 
    hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );

    hPrevRootItem = hPrev; 

    vlc_mutex_lock( &p_input->input.p_item->lock );
    for( int i = 0; i < p_input->input.p_item->i_categories; i++ )
    {
        info_category_t *p_cat = p_input->input.p_item->pp_categories[i];

        // Set the text of the item. 
        tvi.pszText = _FROMMB( p_input->input.p_item->psz_name );
        tvi.cchTextMax = _tcslen( tvi.pszText );
        
        // Save the heading level in the item's application-defined data area
        tvi.lParam = (LPARAM)2; // level 2
        tvins.item = tvi; 
        tvins.hInsertAfter = hPrev; 
        tvins.hParent = hPrevRootItem;

        // Add the item to the tree-view control. 
        hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );

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
            hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );
        }

        TreeView_Expand( hwndTV, hPrevLev2Item, TVE_EXPANDPARTIAL|TVE_EXPAND );
    }
    vlc_mutex_unlock( &p_input->input.p_item->lock );

    TreeView_Expand( hwndTV, hPrevRootItem, TVE_EXPANDPARTIAL|TVE_EXPAND );

    return;
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT FileInfo::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;

    switch( msg )
    {
    case WM_INITDIALOG: 
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );
        CreateTreeView( hwnd );
        UpdateWindow( hwnd );
        SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
        break;

    case WM_CLOSE:
        EndDialog( hwnd, LOWORD( wp ) );
        break;

    case WM_SETFOCUS:
        SHSipPreference( hwnd, SIP_DOWN ); 
        SHFullScreen( hwnd, SHFS_HIDESIPBUTTON );
        break;

    case WM_COMMAND:
        if ( LOWORD(wp) == IDOK )
        {
            EndDialog( hwnd, LOWORD( wp ) );
        }
        break;

    default:
        break;
    }

    return FALSE;
}
