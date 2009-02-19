/*****************************************************************************
 * playlist.cpp : WinCE gui plugin for VLC
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

#include <commctrl.h>
#include <commdlg.h>

#ifndef TEXTMAXBUF
#define TEXTMAXBUF 512 // at least 500
#endif

#define LONG2POINT(l, pt)  ((pt).x = (SHORT)LOWORD(l), (pt).y = (SHORT)HIWORD(l))

#define NUMIMAGES     11   // Number of buttons in the toolbar
#define IMAGEWIDTH    16   // Width of the buttons in the toolbar
#define IMAGEHEIGHT   16   // Height of the buttons in the toolbar
#define BUTTONWIDTH   0    // Width of the button images in the toolbar
#define BUTTONHEIGHT  0    // Height of the button images in the toolbar
#define ID_TOOLBAR    2000 // Identifier of the main tool bar

enum
{
  Infos_Event = 1000,
  Up_Event,
  Down_Event,
  Random_Event,
  Loop_Event,
  Repeat_Event,
  PopupPlay_Event,
  PopupDel_Event,
  PopupEna_Event,
  PopupInfo_Event
};

// Help strings
#define HELP_OPENPL _T("Open playlist")
#define HELP_SAVEPL _T("Save playlist")
#define HELP_ADDFILE _T("Add File")
#define HELP_ADDMRL _T("Add MRL")
#define HELP_DELETE _T("Delete selection")
#define HELP_INFOS _T("Item info")
#define HELP_UP _T("Up")
#define HELP_DOWN _T("Down")
#define HELP_RANDOM _T("Random")
#define HELP_LOOP _T("Repeat all")
#define HELP_REPEAT _T("Repeat one")

// The TBBUTTON structure contains information the toolbar buttons.
static TBBUTTON tbButton2[] =
{
  {0,  ID_MANAGE_OPENPL,        TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {1,  ID_MANAGE_SAVEPL,        TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {0,  0,                       TBSTATE_ENABLED, TBSTYLE_SEP    },
  {2,  ID_MANAGE_ADDFILE,       TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {3,  ID_MANAGE_ADDMRL,        TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {4,  ID_SEL_DELETE,           TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {0,  0,                       TBSTATE_ENABLED, TBSTYLE_SEP    },
  {5,  Infos_Event,             TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {0,  0,                       TBSTATE_ENABLED, TBSTYLE_SEP    },
  {6,  Up_Event,                TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {7,  Down_Event,              TBSTATE_ENABLED, TBSTYLE_BUTTON },
  {0,  0,                       TBSTATE_ENABLED, TBSTYLE_SEP    },
  {8,  Random_Event,            TBSTATE_ENABLED, TBSTYLE_CHECK  },
  {9,  Loop_Event,              TBSTATE_ENABLED, TBSTYLE_CHECK  },
  {10, Repeat_Event,            TBSTATE_ENABLED, TBSTYLE_CHECK  }
};

// Toolbar ToolTips
TCHAR * szToolTips2[] =
{
    HELP_OPENPL,
    HELP_SAVEPL,
    HELP_ADDFILE,
    HELP_ADDMRL,
    HELP_DELETE,
    HELP_INFOS,
    HELP_UP,
    HELP_DOWN,
    HELP_RANDOM,
    HELP_LOOP,
    HELP_REPEAT
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *p_intf, CBaseWindow *p_parent,
                    HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    hListView = NULL;
    i_title_sorted = 1;
    i_author_sorted = 1;

    b_need_update = true;
}

/***********************************************************************
FUNCTION:
  CreateMenuBar

PURPOSE:
  Creates a menu bar.
***********************************************************************/
static HWND CreateMenuBar( HWND hwnd, HINSTANCE hInst )
{
#ifdef UNDER_CE
    SHMENUBARINFO mbi;
    memset( &mbi, 0, sizeof(SHMENUBARINFO) );
    mbi.cbSize     = sizeof(SHMENUBARINFO);
    mbi.hwndParent = hwnd;
    mbi.hInstRes   = hInst;
    mbi.nToolBarId = IDR_MENUBAR2;

    if( !SHCreateMenuBar( &mbi ) )
    {
        MessageBox(hwnd, _T("SHCreateMenuBar Failed"), _T("Error"), MB_OK);
        return 0;
    }

    TBBUTTONINFO tbbi;
    tbbi.cbSize = sizeof(tbbi);
    tbbi.dwMask = TBIF_LPARAM;

    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_MANAGE, (LPARAM)&tbbi );
    HMENU hmenu_file = (HMENU)tbbi.lParam;
    RemoveMenu( hmenu_file, 0, MF_BYPOSITION );
    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_SORT, (LPARAM)&tbbi );
    HMENU hmenu_sort = (HMENU)tbbi.lParam;
    RemoveMenu( hmenu_sort, 0, MF_BYPOSITION );
    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_SEL, (LPARAM)&tbbi );
    HMENU hmenu_sel = (HMENU)tbbi.lParam;
    RemoveMenu( hmenu_sel, 0, MF_BYPOSITION );

#else
    HMENU hmenu_file = CreatePopupMenu();
    HMENU hmenu_sort = CreatePopupMenu();
    HMENU hmenu_sel = CreatePopupMenu();
#endif

    AppendMenu( hmenu_file, MF_STRING, ID_MANAGE_ADDFILE,
                _T("&Add File...") );
    AppendMenu( hmenu_file, MF_STRING, ID_MANAGE_ADDDIRECTORY,
                _T("Add Directory...") );
    AppendMenu( hmenu_file, MF_STRING, ID_MANAGE_ADDMRL,
                _T("Add MRL...") );
    AppendMenu( hmenu_file, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_file, MF_STRING, ID_MANAGE_OPENPL,
                _T("Open &Playlist") );
    AppendMenu( hmenu_file, MF_STRING, ID_MANAGE_SAVEPL,
                _T("Save Playlist") );

    AppendMenu( hmenu_sort, MF_STRING, ID_SORT_TITLE,
                _T("Sort by &title") );
    AppendMenu( hmenu_sort, MF_STRING, ID_SORT_RTITLE,
                _T("&Reverse sort by title") );
    AppendMenu( hmenu_sort, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_sort, MF_STRING, ID_SORT_AUTHOR,
                _T("Sort by &author") );
    AppendMenu( hmenu_sort, MF_STRING, ID_SORT_RAUTHOR,
                _T("Reverse sort by &author") );
    AppendMenu( hmenu_sort, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_sort, MF_STRING, ID_SORT_SHUFFLE,
                _T("&Shuffle Playlist") );

    AppendMenu( hmenu_sel, MF_STRING, ID_SEL_ENABLE,
                _T("&Enable") );
    AppendMenu( hmenu_sel, MF_STRING, ID_SEL_DISABLE,
                _T("&Disable") );
    AppendMenu( hmenu_sel, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_sel, MF_STRING, ID_SEL_INVERT,
                _T("&Invert") );
    AppendMenu( hmenu_sel, MF_STRING, ID_SEL_DELETE,
                _T("D&elete") );
    AppendMenu( hmenu_sel, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_sel, MF_STRING, ID_SEL_SELECTALL,
                _T("&Select All") );


#ifdef UNDER_CE
    return mbi.hwndMB;

#else
    HMENU hmenu = CreateMenu();

    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_file, _T("Manage") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_sort, _T("Sort") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_sel, _T("Selection") );

    SetMenu( hwnd, hmenu );
    return hwnd;

#endif
}

/***********************************************************************
FUNCTION:
  WndProc

PURPOSE:
  Processes messages sent to the main window.
***********************************************************************/
LRESULT Playlist::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;
    SHMENUBARINFO mbi;
    INITCOMMONCONTROLSEX iccex;
    RECT rect, rectTB;
    DWORD dwStyle;

    int bState;
    playlist_t *p_playlist;

    switch( msg )
    {
    case WM_INITDIALOG:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        hwndCB = CreateMenuBar( hwnd, hInst );

        iccex.dwSize = sizeof (INITCOMMONCONTROLSEX);
        iccex.dwICC = ICC_BAR_CLASSES;

        // Registers TOOLBAR control classes from the common control dll
        InitCommonControlsEx (&iccex);

        //  Create the toolbar control.
        dwStyle = WS_VISIBLE | WS_CHILD | TBSTYLE_TOOLTIPS |
            WS_EX_OVERLAPPEDWINDOW | CCS_NOPARENTALIGN;

        hwndTB = CreateToolbarEx( hwnd, dwStyle, 0, NUMIMAGES,
                                  hInst, IDB_BITMAP3, tbButton2,
                                  sizeof (tbButton2) / sizeof (TBBUTTON),
                                  BUTTONWIDTH, BUTTONHEIGHT,
                                  IMAGEWIDTH, IMAGEHEIGHT, sizeof(TBBUTTON) );
        if( !hwndTB ) break;
 
        // Add ToolTips to the toolbar.
        SendMessage( hwndTB, TB_SETTOOLTIPS, (WPARAM) NUMIMAGES,
                     (LPARAM)szToolTips2 );

        // Reposition the toolbar.
        GetClientRect( hwnd, &rect );
        GetWindowRect( hwndTB, &rectTB );
        MoveWindow( hwndTB, rect.left, rect.top - 2, rect.right - rect.left,
                    MENU_HEIGHT /*rectTB.bottom - rectTB.top */, TRUE);

        // random, loop, repeat buttons states
        vlc_value_t val;
        p_playlist = pl_Hold( p_intf );
        if( !p_playlist ) break;

        var_Get( p_playlist , "random", &val );
        bState = val.b_bool ? TBSTATE_CHECKED : 0;
        SendMessage( hwndTB, TB_SETSTATE, Random_Event,
                     MAKELONG(bState | TBSTATE_ENABLED, 0) );
        var_Get( p_playlist , "loop", &val );
        bState = val.b_bool ? TBSTATE_CHECKED : 0;
        SendMessage( hwndTB, TB_SETSTATE, Loop_Event,
                     MAKELONG(bState | TBSTATE_ENABLED, 0) );
        var_Get( p_playlist , "repeat", &val );
        bState = val.b_bool ? TBSTATE_CHECKED : 0;
        SendMessage( hwndTB, TB_SETSTATE, Repeat_Event,
                     MAKELONG(bState | TBSTATE_ENABLED, 0) );
        pl_Release( p_intf );

        GetClientRect( hwnd, &rect );
        hListView = CreateWindow( WC_LISTVIEW, NULL, WS_VISIBLE | WS_CHILD |
            LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_HSCROLL,
            rect.left, rect.top + 2*(MENU_HEIGHT+1), rect.right - rect.left,
            rect.bottom - ( rect.top + 2*MENU_HEIGHT) - MENU_HEIGHT,
            hwnd, NULL, hInst, NULL );
        ListView_SetExtendedListViewStyle( hListView, LVS_EX_FULLROWSELECT );

        LVCOLUMN lv;
        lv.mask = LVCF_WIDTH | LVCF_FMT | LVCF_TEXT;
        lv.fmt = LVCFMT_LEFT ;
        GetClientRect( hwnd, &rect );
        lv.cx = 120;
        lv.pszText = _T("Name");
        lv.cchTextMax = 9;
        ListView_InsertColumn( hListView, 0, &lv);
        lv.cx = 55;
        lv.pszText = _T("Author");
        lv.cchTextMax = 9;
        ListView_InsertColumn( hListView, 1, &lv);
        lv.cx = rect.right - rect.left - 180;
        lv.pszText = _T("Duration");
        lv.cchTextMax = 9;
        ListView_InsertColumn( hListView, 2, &lv);

        SetTimer( hwnd, 1, 500 /*milliseconds*/, NULL );
        break;

    case WM_TIMER:
        UpdatePlaylist();
        break;

    case WM_CLOSE:
        EndDialog( hwnd, LOWORD( wp ) );
        break;

    case WM_SETFOCUS:
        SHSipPreference( hwnd, SIP_DOWN );
        SHFullScreen( hwnd, SHFS_HIDESIPBUTTON );
        break;

    case WM_COMMAND:
        switch( LOWORD(wp) )
        {
        case IDOK:
            EndDialog( hwnd, LOWORD( wp ) );
            break;

        case ID_MANAGE_OPENPL:
            OnOpen();
            b_need_update = true;
            break;

        case ID_MANAGE_SAVEPL:
            OnSave();
            break;

        case ID_MANAGE_ADDFILE:
            p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_SIMPLE,
                                           0, 0 );
            b_need_update = true;
            break;

        case ID_MANAGE_ADDDIRECTORY:
            p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_DIRECTORY,
                                           0, 0 );
            b_need_update = true;
            break;

        case ID_MANAGE_ADDMRL:
            p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE, 0, 0 );
            b_need_update = true;
            break;

        case ID_SEL_DELETE:
            OnDeleteSelection();
            b_need_update = true;
            break;

        case Infos_Event:
            OnPopupInfo( hwnd );
            b_need_update = true;
            break;

        case Up_Event:
            OnUp();
            b_need_update = true;
            break;

        case Down_Event:
            OnDown();
            b_need_update = true;
            break;

        case Random_Event:
            OnRandom();
            break;

        case Loop_Event:
            OnLoop();
            break;

        case Repeat_Event:
            OnRepeat();
            break;

        case ID_SORT_TITLE:
            OnSort( ID_SORT_TITLE );
            break;

        case ID_SORT_RTITLE:
            OnSort( ID_SORT_RTITLE );
            break;

        case ID_SORT_AUTHOR:
            OnSort( ID_SORT_AUTHOR );
            break;

        case ID_SORT_RAUTHOR:
            OnSort( ID_SORT_RAUTHOR );
            break;

        case ID_SORT_SHUFFLE:
            OnSort( ID_SORT_SHUFFLE );
            break;

        case ID_SEL_ENABLE:
            OnEnableSelection();
            break;

        case ID_SEL_DISABLE:
            OnDisableSelection();
            break;

        case ID_SEL_INVERT:
            OnInvertSelection();
            break;

        case ID_SEL_SELECTALL:
            OnSelectAll();
            break;

        case PopupPlay_Event:
            OnPopupPlay();
            b_need_update = true;
            break;

        case PopupDel_Event:
            OnPopupDel();
            b_need_update = true;
            break;

        case PopupEna_Event:
            OnPopupEna();
            b_need_update = true;
            break;

        case PopupInfo_Event:
            OnPopupInfo( hwnd );
            b_need_update = true;
            break;

        default:
            break;
        }
        break;

    case WM_NOTIFY:
        if( ( ((LPNMHDR)lp)->hwndFrom == hListView ) &&
            ( ((LPNMHDR)lp)->code == NM_CUSTOMDRAW ) )
        {
            SetWindowLong( hwnd, DWL_MSGRESULT,
                           (LONG)ProcessCustomDraw(lp) );
        }
        else if( ( ((LPNMHDR)lp)->hwndFrom == hListView ) &&
                 ( ((LPNMHDR)lp)->code == GN_CONTEXTMENU  ) )
        {
            HandlePopupMenu( hwnd, ((PNMRGINFO)lp)->ptAction );
        }
        else if( ( ((LPNMHDR)lp)->hwndFrom == hListView ) &&
                 ( ((LPNMHDR)lp)->code == LVN_COLUMNCLICK  ) )
        {
            OnColSelect( ((LPNMLISTVIEW)lp)->iSubItem );
        }
        else if( ( ((LPNMHDR)lp)->hwndFrom == hListView ) &&
                 ( ((LPNMHDR)lp)->code == LVN_ITEMACTIVATE  ) )
        {
            OnActivateItem( ((LPNMLISTVIEW)lp)->iSubItem );
        }
        break;

    default:
         // the message was not processed
         // indicate if the base class handled it
        break;
    }

    return FALSE;
}

LRESULT Playlist::ProcessCustomDraw( LPARAM lParam )
{
    LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

    switch( lplvcd->nmcd.dwDrawStage )
    {
    case CDDS_PREPAINT : //Before the paint cycle begins
        //request notifications for individual listview items
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: //Before an item is drawn
        playlist_t *p_playlist = pl_Hold( p_intf );
        if( p_playlist == NULL ) return CDRF_DODEFAULT;
        if( (int)lplvcd->nmcd.dwItemSpec == p_playlist->i_current_index )
        {
            lplvcd->clrText = RGB(255,0,0);
            pl_Release( p_intf );
            return CDRF_NEWFONT;
        }

        PL_LOCK;
        playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                        (int)lplvcd->nmcd.dwItemSpec );
        if( !p_item )
        {
            PL_UNLOCK;
            pl_Release( p_intf );
            return CDRF_DODEFAULT;
        }
        if( p_item->i_flags & PLAYLIST_DBL_FLAG )
        {
            lplvcd->clrText = RGB(192,192,192);
            PL_UNLOCK;
            pl_Release( p_intf );
            return CDRF_NEWFONT;
        }
        PL_UNLOCK;
        pl_Release( p_intf );
    }

    return CDRF_DODEFAULT;
}

/**********************************************************************
 * Handles the display of the "floating" popup
 **********************************************************************/
void Playlist::HandlePopupMenu( HWND hwnd, POINT point )
{
    HMENU hMenuTrackPopup;

    // Create the popup menu.
    hMenuTrackPopup = CreatePopupMenu();

    // Append some items.
    AppendMenu( hMenuTrackPopup, MF_STRING, PopupPlay_Event, _T("Play") );
    AppendMenu( hMenuTrackPopup, MF_STRING, PopupDel_Event, _T("Delete") );
    AppendMenu( hMenuTrackPopup, MF_STRING, PopupEna_Event,
                _T("Toggle enabled") );
    AppendMenu( hMenuTrackPopup, MF_STRING, PopupInfo_Event, _T("Info") );

    /* Draw and track the "floating" popup */
    TrackPopupMenu( hMenuTrackPopup, 0, point.x, point.y, 0, hwnd, NULL );

    /* Destroy the menu since were are done with it. */
    DestroyMenu( hMenuTrackPopup );
}

/**********************************************************************
 * Show the playlist
 **********************************************************************/
void Playlist::ShowPlaylist( bool b_show )
{
    if( b_show ) Rebuild();
    Show( b_show );
}

/**********************************************************************
 * Update the playlist
 **********************************************************************/
void Playlist::UpdatePlaylist()
{
    if( b_need_update )
    {
        Rebuild();
        b_need_update = false;
    }
 
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;
 
    /* Update the colour of items */

    PL_LOCK;
    if( p_intf->p_sys->i_playing != playlist_CurrentSize( p_playlist ) )
    {
        // p_playlist->i_index in RED
        Rebuild();

        // if exists, p_intf->p_sys->i_playing in BLACK
        p_intf->p_sys->i_playing = p_playlist->i_current_index;
    }
    PL_UNLOCK;

    pl_Release( p_intf );
}

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void Playlist::Rebuild()
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    int i_focused =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    /* Clear the list... */
    ListView_DeleteAllItems( hListView );

    /* ...and rebuild it */
    PL_LOCK;
    playlist_item_t * p_root = p_playlist->p_local_onelevel;
    playlist_item_t * p_child = NULL;

    int iItem = 0;

    while( ( p_child = playlist_GetNextLeaf( p_playlist, p_root, p_child, FALSE, FALSE ) ) )
    {
        LVITEM lv;
        lv.mask = LVIF_TEXT;
        lv.pszText = _T("");
        lv.cchTextMax = 1;
        lv.iSubItem = 0;
        lv.iItem = iItem;
        ListView_InsertItem( hListView, &lv );
        ListView_SetItemText( hListView, lv.iItem, 0,
            _FROMMB(p_child->p_input->psz_name) );

        UpdateItem( p_child->i_id );
        iItem++;
    }
    PL_UNLOCK;

    if ( i_focused )
        ListView_SetItemState( hListView, i_focused, LVIS_FOCUSED |
                               LVIS_SELECTED, LVIS_STATEIMAGEMASK )
    else
        ListView_SetItemState( hListView, i_focused, LVIS_FOCUSED,
                               LVIS_STATEIMAGEMASK );

    pl_Release( p_intf );
}

/**********************************************************************
 * Update one playlist item
 **********************************************************************/
void Playlist::UpdateItem( int i )
{
    playlist_t *p_playlist = pl_Hold( p_intf );

    if( p_playlist == NULL ) return;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i );

    if( !p_item )
    {
        PL_UNLOCK;
        pl_Release( p_intf );
        return;
    }

    ListView_SetItemText( hListView, i, 0, _FROMMB(p_item->p_input->psz_name) );

    ListView_SetItemText( hListView, i, 1,
                          _FROMMB( input_item_GetInfo( p_item->p_input,
                                   _("General") , _("Author") ) ) );

    char psz_duration[MSTRTIME_MAX_SIZE];
    mtime_t dur = input_item_GetDuration( p_item->p_input );
    PL_UNLOCK;

    if( dur != -1 ) secstotimestr( psz_duration, dur/1000000 );
    else memcpy( psz_duration , "-:--:--", sizeof("-:--:--") );

    ListView_SetItemText( hListView, i, 2, _FROMMB(psz_duration) );

    pl_Release( p_intf );
}

/**********************************************************************
 * Private functions
 **********************************************************************/
void Playlist::DeleteItem( int item )
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    playlist_DeleteFromInput( p_playlist, item, FALSE );
    ListView_DeleteItem( hListView, item );

    pl_Release( p_intf );
}

/**********************************************************************
 * I/O functions
 **********************************************************************/
static void OnOpenCB( intf_dialog_args_t *p_arg )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_arg->p_arg;

    if( p_arg->i_results && p_arg->psz_results[0] )
    {
        playlist_t * p_playlist = pl_Hold( p_intf );

        if( p_playlist )
        {
            playlist_Import( p_playlist, p_arg->psz_results[0] );
            pl_Release( p_intf );
        }
    }
}

void Playlist::OnOpen()
{
    char *psz_filters ="All playlists|*.pls;*.m3u;*.asx;*.b4s|M3U files|*.m3u";

    intf_dialog_args_t *p_arg =
        (intf_dialog_args_t *)malloc( sizeof(intf_dialog_args_t) );
    memset( p_arg, 0, sizeof(intf_dialog_args_t) );

    p_arg->psz_title = strdup( "Open playlist" );
    p_arg->psz_extensions = strdup( psz_filters );
    p_arg->p_arg = p_intf;
    p_arg->pf_callback = OnOpenCB;

    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_GENERIC, 0, p_arg);
}

static void OnSaveCB( intf_dialog_args_t *p_arg )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_arg->p_arg;

    if( p_arg->i_results && p_arg->psz_results[0] )
    {
        playlist_t * p_playlist = pl_Hold( p_intf );

        if( p_playlist )
        {
            char *psz_export;
            char *psz_ext = strrchr( p_arg->psz_results[0], '.' );

            if( psz_ext && !strcmp( psz_ext, ".pls") )
                psz_export = "export-pls";
            else psz_export = "export-m3u";

            playlist_Export( p_playlist, p_arg->psz_results[0], p_playlist->p_local_onelevel, psz_export );
            pl_Release( p_intf );
        }
    }
}

void Playlist::OnSave()
{
    char *psz_filters ="M3U file|*.m3u|PLS file|*.pls";

    intf_dialog_args_t *p_arg =
        (intf_dialog_args_t *)malloc( sizeof(intf_dialog_args_t) );
    memset( p_arg, 0, sizeof(intf_dialog_args_t) );

    p_arg->psz_title = strdup( "Save playlist" );
    p_arg->psz_extensions = strdup( psz_filters );
    p_arg->b_save = true;
    p_arg->p_arg = p_intf;
    p_arg->pf_callback = OnSaveCB;

    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_GENERIC,
                                   0, p_arg );
}

/**********************************************************************
 * Selection functions
 **********************************************************************/
void Playlist::OnDeleteSelection()
{
    /* Delete from the end to the beginning, to avoid a shift of indices */
    for( long item = ((int) ListView_GetItemCount( hListView ) - 1);
         item >= 0; item-- )
    {
        if( ListView_GetItemState( hListView, item, LVIS_SELECTED ) )
        {
            DeleteItem( item );
        }
    }
}

void Playlist::OnInvertSelection()
{
    UINT iState;

    for( long item = 0; item < ListView_GetItemCount( hListView ); item++ )
    {
        iState = ListView_GetItemState( hListView, item, LVIS_STATEIMAGEMASK );
        ListView_SetItemState( hListView, item, iState ^ LVIS_SELECTED,
                               LVIS_STATEIMAGEMASK );
    }
}

void Playlist::OnEnableSelection()
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    for( long item = ListView_GetItemCount( hListView ) - 1;
         item >= 0; item-- )
    {
        if( ListView_GetItemState( hListView, item, LVIS_SELECTED ) )
        {
            PL_LOCK;
            playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item );
            p_item->i_flags ^= PLAYLIST_DBL_FLAG;
            PL_UNLOCK;
            UpdateItem( item );
        }
    }
    pl_Release( p_intf );
}

void Playlist::OnDisableSelection()
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    for( long item = ListView_GetItemCount( hListView ) - 1;
         item >= 0; item-- )
    {
        if( ListView_GetItemState( hListView, item, LVIS_SELECTED ) )
        {
            PL_LOCK;
            playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item );
            p_item->i_flags |= PLAYLIST_DBL_FLAG;
            PL_UNLOCK;
            UpdateItem( item );
        }
    }
    pl_Release( p_intf );
}

void Playlist::OnSelectAll()
{
    for( long item = 0; item < ListView_GetItemCount( hListView ); item++ )
    {
        ListView_SetItemState( hListView, item, LVIS_FOCUSED | LVIS_SELECTED,
                               LVIS_STATEIMAGEMASK );
    }
}

void Playlist::OnActivateItem( int i_item )
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    playlist_Skip( p_playlist, i_item - p_playlist->i_current_index );

    pl_Release( p_intf );
}

void Playlist::ShowInfos( HWND hwnd, int i_item )
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_item );
    if( p_item )
    {
        ItemInfoDialog *iteminfo_dialog =
            new ItemInfoDialog( p_intf, this, hInst, p_item );
        PL_UNLOCK;
        CreateDialogBox( hwnd, iteminfo_dialog );
        UpdateItem( i_item );
        delete iteminfo_dialog;
    }

    pl_Release( p_intf );
}

/********************************************************************
 * Move functions
 ********************************************************************/
void Playlist::OnUp()
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    /* We use the first selected item, so find it */
    long i_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    if( i_item > 0 && i_item < playlist_CurrentSize( p_playlist ) )
    {
        playlist_Prev( p_playlist );
        if( i_item > 1 )
        {
            ListView_SetItemState( hListView, i_item - 1, LVIS_FOCUSED,
                                   LVIS_STATEIMAGEMASK );
        }
        else
        {
            ListView_SetItemState( hListView, 0, LVIS_FOCUSED,
                                   LVIS_STATEIMAGEMASK );
        }
    }
    pl_Release( p_intf );

    return;
}

void Playlist::OnDown()
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    /* We use the first selected item, so find it */
    long i_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    if( i_item >= 0 && i_item < playlist_CurrentSize( p_playlist ) - 1 )
    {
        playlist_Next( p_playlist );
        ListView_SetItemState( hListView, i_item + 1, LVIS_FOCUSED,
                               LVIS_STATEIMAGEMASK );
    }
    pl_Release( p_intf );

    return;
}

/**********************************************************************
 * Playlist mode functions
 **********************************************************************/
void Playlist::OnRandom()
{
    vlc_value_t val;
    int bState = SendMessage( hwndTB, TB_GETSTATE, Random_Event, 0 );
    val.b_bool = (bState & TBSTATE_CHECKED) ? true : false;

    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    var_Set( p_playlist , "random", val );
    pl_Release( p_intf );
}

void Playlist::OnLoop ()
{
    vlc_value_t val;
    int bState = SendMessage( hwndTB, TB_GETSTATE, Loop_Event, 0 );
    val.b_bool = (bState & TBSTATE_CHECKED) ? true : false;

    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    var_Set( p_playlist , "loop", val );
    pl_Release( p_intf );
}

void Playlist::OnRepeat ()
{
    vlc_value_t val;
    int bState = SendMessage( hwndTB, TB_GETSTATE, Repeat_Event, 0 );
    val.b_bool = (bState & TBSTATE_CHECKED) ? true : false;

    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    var_Set( p_playlist , "repeat", val );
    pl_Release( p_intf );
}

/********************************************************************
 * Sorting functions
 ********************************************************************/
void Playlist::OnSort( UINT event )
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    switch( event )
    {
    case ID_SORT_TITLE:
        playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_TITLE, ORDER_NORMAL);
        break;
    case ID_SORT_RTITLE:
        playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_TITLE, ORDER_REVERSE );
        break;
    case ID_SORT_AUTHOR:
        playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_ARTIST, ORDER_NORMAL);
        break;
    case ID_SORT_RAUTHOR:
        playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_ARTIST, ORDER_REVERSE);
        break;
    case ID_SORT_SHUFFLE:
        playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_RANDOM, ORDER_NORMAL);
        break;
    }

    pl_Release( p_intf );

    b_need_update = true;

    return;
}

void Playlist::OnColSelect( int iSubItem )
{
    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    switch( iSubItem )
    {
    case 0:
        if( i_title_sorted != 1 )
        {
            playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_TITLE, ORDER_NORMAL);
            i_title_sorted = 1;
        }
        else
        {
            playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_TITLE, ORDER_REVERSE );
            i_title_sorted = -1;
        }
        break;
    case 1:
        if( i_author_sorted != 1 )
        {
            playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_ARTIST, ORDER_NORMAL);
            i_author_sorted = 1;
        }
        else
        {
            playlist_RecursiveNodeSort(p_playlist , p_playlist->p_root_onelevel,
                                    SORT_ARTIST, ORDER_REVERSE);
            i_author_sorted = -1;
        }
        break;
    default:
        break;
    }

    pl_Release( p_intf );

    b_need_update = true;

    return;
}

/*****************************************************************************
 * Popup management functions
 *****************************************************************************/
void Playlist::OnPopupPlay()
{
    int i_popup_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    if( i_popup_item != -1 )
    {
        playlist_Skip( p_playlist, i_popup_item - p_playlist->i_current_index );
    }

    pl_Release( p_intf );
}

void Playlist::OnPopupDel()
{
    int i_popup_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    DeleteItem( i_popup_item );
}

void Playlist::OnPopupEna()
{
    int i_popup_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    playlist_t *p_playlist = pl_Hold( p_intf );
    if( p_playlist == NULL ) return;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_popup_item );

    if( !(p_playlist->items.p_elems[i_popup_item]->i_flags & PLAYLIST_DBL_FLAG) )
        //playlist_IsEnabled( p_playlist, i_popup_item ) )
    {
        p_item->i_flags |= PLAYLIST_DBL_FLAG;
    }
    else
    {
        p_item->i_flags ^= PLAYLIST_DBL_FLAG;
    }

    PL_UNLOCK;
    pl_Release( p_intf );
    UpdateItem( i_popup_item );
}

void Playlist::OnPopupInfo( HWND hwnd )
{
    int i_popup_item =
        ListView_GetNextItem( hListView, -1, LVIS_SELECTED | LVNI_ALL );

    ShowInfos( hwnd, i_popup_item );
}
