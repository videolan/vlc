/*****************************************************************************
 * interface.cpp: WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

#include "wince.h"

#include <winuser.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#define NUMIMAGES     9   // Number of buttons in the toolbar           
#define IMAGEWIDTH    17   // Width of the buttons in the toolbar  
#define IMAGEHEIGHT   16   // Height of the buttons in the toolbar  
#define BUTTONWIDTH   0    // Width of the button images in the toolbar
#define BUTTONHEIGHT  0    // Height of the button images in the toolbar
#define ID_TOOLBAR    2000 // Identifier of the main tool bar

// Help strings
#define HELP_SIMPLE _T("Quick file open")
#define HELP_ADV    _T("Advanced open")
#define HELP_FILE   _T("Open a file")
#define HELP_DISC   _T("Open Disc Media")
#define HELP_NET    _T("Open a network stream")
#define HELP_SAT    _T("Open a satellite stream")
#define HELP_EJECT  _T("Eject the DVD/CD")
#define HELP_EXIT   _T("Exit this program")

#define HELP_OTHER _T("Open other types of inputs")

#define HELP_PLAYLIST   _T("Open the playlist")
#define HELP_LOGS       _T("Show the program logs")
#define HELP_FILEINFO   _T("Show information about the file being played")

#define HELP_PREFS _T("Go to the preferences menu")

#define HELP_ABOUT _T("About this program")

#define HELP_STOP _T("Stop")

#define HELP_PLAY _T("Play")
#define HELP_PAUSE _T("Pause")
#define HELP_PLO _T("Playlist")
#define HELP_PLP _T("Previous playlist item")
#define HELP_PLN _T("Next playlist item")
#define HELP_SLOW _T("Play slower")
#define HELP_FAST _T("Play faster")

// The TBBUTTON structure contains information the toolbar buttons.
static TBBUTTON tbButton[] =      
{
  {0, ID_FILE_QUICKOPEN,        TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {1, ID_FILE_OPENNET,       TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP},
  {2, StopStream_Event,       TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {3, PlayStream_Event,        TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP},
  {4, ID_VIEW_PLAYLIST,       TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP},
  {5, PrevStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {6, NextStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP},
  {7, SlowStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON},
  {8, FastStream_Event,       TBSTATE_ENABLED, TBSTYLE_BUTTON},
};

// Toolbar ToolTips
TCHAR * szToolTips[] = 
{
    HELP_SIMPLE, HELP_NET, HELP_STOP, HELP_PLAY, HELP_PLO, HELP_PLP,
    HELP_PLN, HELP_SLOW, HELP_FAST
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
BOOL Interface::InitInstance( HINSTANCE hInstance, intf_thread_t *_p_intf )
{
    /* Initializations */
    p_intf = _p_intf;
    hwndMain = hwndCB = hwndTB = hwndSlider = hwndLabel = hwndVol = hwndSB = 0;
    i_old_playing_status = PAUSE_S;

    hInst = hInstance; // Store instance handle in our global variable

    // Register window class
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW ;
    wc.lpfnWndProc = (WNDPROC)BaseWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hIcon = NULL;
    wc.hInstance = hInstance;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("VLC WinCE");
    if( !RegisterClass( &wc ) ) return FALSE;

    int i_style = WS_VISIBLE;

#ifndef UNDER_CE
    i_style |= WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
#endif

    // Create main window
    hwndMain =
        CreateWindow( _T("VLC WinCE"), _T("VLC media player"), i_style,
                      0, MENU_HEIGHT, CW_USEDEFAULT, CW_USEDEFAULT,
                      NULL, NULL, hInstance, (void *)this );

    if( !hwndMain ) return FALSE;

    ShowWindow( hwndMain, TRUE );
    UpdateWindow( hwndMain );

    return TRUE;
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
    mbi.nToolBarId = IDR_MENUBAR;

    if( !SHCreateMenuBar( &mbi ) )
    {
        MessageBox(hwnd, _T("SHCreateMenuBar Failed"), _T("Error"), MB_OK);
        return 0;
    }

    TBBUTTONINFO tbbi;
    tbbi.cbSize = sizeof(tbbi);
    tbbi.dwMask = TBIF_LPARAM;

    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_FILE, (LPARAM)&tbbi );
    HMENU hmenu_file = (HMENU)tbbi.lParam;
    RemoveMenu( hmenu_file, 0, MF_BYPOSITION );
    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_VIEW, (LPARAM)&tbbi );
    HMENU hmenu_view = (HMENU)tbbi.lParam;
    RemoveMenu( hmenu_view, 0, MF_BYPOSITION );
    SendMessage( mbi.hwndMB, TB_GETBUTTONINFO, IDM_SETTINGS, (LPARAM)&tbbi );
    HMENU hmenu_settings = (HMENU)tbbi.lParam;

#else
    HMENU hmenu_file = CreatePopupMenu();
    HMENU hmenu_view = CreatePopupMenu();
    HMENU hmenu_settings = CreatePopupMenu();
    HMENU hmenu_audio = CreatePopupMenu();
    HMENU hmenu_video = CreatePopupMenu();
    HMENU hmenu_navigation = CreatePopupMenu();
#endif

    AppendMenu( hmenu_file, MF_STRING, ID_FILE_QUICKOPEN,
                _T("Quick &Open File...") );
    AppendMenu( hmenu_file, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_file, MF_STRING, ID_FILE_OPENFILE,
                _T("Open &File...") );
    AppendMenu( hmenu_file, MF_STRING, ID_FILE_OPENNET,
                _T("Open &Network Stream...") );
    AppendMenu( hmenu_file, MF_SEPARATOR, 0, 0 );
    AppendMenu( hmenu_file, MF_STRING, ID_FILE_ABOUT,
                _T("About VLC") );
    AppendMenu( hmenu_file, MF_STRING, ID_FILE_EXIT,
                _T("E&xit") );

    AppendMenu( hmenu_view, MF_STRING, ID_VIEW_PLAYLIST,
                _T("&Playlist...") );
    AppendMenu( hmenu_view, MF_STRING, ID_VIEW_MESSAGES,
                _T("&Messages...") );
    AppendMenu( hmenu_view, MF_STRING, ID_VIEW_STREAMINFO,
                _T("Stream and Media &info...") );

    AppendMenu( hmenu_settings, MF_STRING, ID_PREFERENCES,
                _T("&Preferences...") );


#ifdef UNDER_CE
    return mbi.hwndMB;

#else
    HMENU hmenu = CreateMenu();

    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_file, _T("File") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_view, _T("View") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_settings,
                _T("Settings") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_audio, _T("Audio") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_video, _T("Video") );
    AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT)hmenu_navigation,
                _T("Nav.") );

    SetMenu( hwnd, hmenu );
    return hwnd;

#endif
}

/***********************************************************************
FUNCTION: 
  CreateToolBar

PURPOSE: 
  Registers the TOOLBAR control class and creates a toolbar.
***********************************************************************/
HWND CreateToolBar( HWND hwnd, HINSTANCE hInst )
{
    DWORD dwStyle;
    HWND hwndTB;
    RECT rect, rectTB;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers TOOLBAR control classes from the common control dll
    InitCommonControlsEx (&iccex);

    //  Create the toolbar control
    dwStyle = WS_VISIBLE | WS_CHILD | TBSTYLE_TOOLTIPS |
        WS_EX_OVERLAPPEDWINDOW | CCS_NOPARENTALIGN;

    hwndTB = CreateToolbarEx( hwnd, dwStyle, 0, NUMIMAGES,
        hInst, IDB_BITMAP1, tbButton, sizeof(tbButton) / sizeof(TBBUTTON),
        BUTTONWIDTH, BUTTONHEIGHT, IMAGEWIDTH, IMAGEHEIGHT, sizeof(TBBUTTON) );

    if( !hwndTB ) return NULL;
  
    // Add ToolTips to the toolbar.
    SendMessage( hwndTB, TB_SETTOOLTIPS, (WPARAM)NUMIMAGES, 
                 (LPARAM)szToolTips );

    // Reposition the toolbar.
    GetClientRect( hwnd, &rect );
    GetWindowRect( hwndTB, &rectTB );
    MoveWindow( hwndTB, rect.left, rect.bottom - rect.top - 2*MENU_HEIGHT, 
                rect.right - rect.left, MENU_HEIGHT, TRUE );

    return hwndTB;
}

/***********************************************************************

FUNCTION: 
  CreateSliderBar

PURPOSE: 
  Registers the TRACKBAR_CLASS control class and creates a trackbar.

***********************************************************************/
HWND CreateSliderBar( HWND hwnd, HINSTANCE hInst )
{
    HWND hwndSlider;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers TRACKBAR_CLASS control classes from the common control dll
    InitCommonControlsEx( &iccex );

    hwndSlider = CreateWindowEx( 0, TRACKBAR_CLASS, NULL,
                WS_CHILD | WS_VISIBLE | TBS_HORZ | WS_EX_OVERLAPPEDWINDOW |
                TBS_BOTTOM,  //|WS_CLIPSIBLINGS,
                0, 0, 0, 0, hwnd, NULL, hInst, NULL );

    if( !hwndSlider ) return NULL;

    SendMessage( hwndSlider, TBM_SETRANGEMIN, 1, 0 );
    SendMessage( hwndSlider, TBM_SETRANGEMAX, 1, SLIDER_MAX_POS );
    SendMessage( hwndSlider, TBM_SETPOS, 1, 0 );

    // Reposition the trackbar
    GetClientRect( hwnd, &rect );
    MoveWindow( hwndSlider, rect.left, 
                rect.bottom - rect.top - 2*(MENU_HEIGHT-1) - SLIDER_HEIGHT, 
                rect.right - rect.left - 40, 30, TRUE );

    ShowWindow( hwndSlider, SW_HIDE );

    return hwndSlider;
}

HWND CreateStaticText( HWND hwnd, HINSTANCE hInst )
{
    HWND hwndLabel;
    RECT rect;

    hwndLabel = CreateWindowEx( 0, _T("STATIC"), _T("label"),
                                WS_CHILD | WS_VISIBLE | SS_CENTER ,
                                0, 0, 0, 0, hwnd, (HMENU)1980, hInst, NULL );

    // Reposition the trackbar
    GetClientRect( hwnd, &rect );

    MoveWindow( hwndLabel, rect.left,
                rect.bottom - rect.top - 2*(MENU_HEIGHT-1) - SLIDER_HEIGHT +30,
                rect.right - rect.left - 40,
                SLIDER_HEIGHT - 30, TRUE );

    ShowWindow( hwndLabel, SW_HIDE );

    return hwndLabel;
}

/***********************************************************************

FUNCTION: 
  CreateVolTrackBar

PURPOSE: 
  Registers the TRACKBAR_CLASS control class and creates a trackbar.

***********************************************************************/
HWND CreateVolTrackBar( HWND hwnd, HINSTANCE hInst )
{
    HWND hwndVol;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers TRACKBAR_CLASS control classes from the common control dll
    InitCommonControlsEx( &iccex );

    hwndVol = CreateWindowEx( 0, TRACKBAR_CLASS, NULL,
                WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_RIGHT | TBS_AUTOTICKS |
                WS_EX_OVERLAPPEDWINDOW, //|WS_CLIPSIBLINGS,
                0, 0, 0, 0, hwnd, NULL, hInst, NULL );

    if( !hwndVol ) return NULL;

    SendMessage( hwndVol, TBM_SETRANGEMIN, 1, 0 );
    SendMessage( hwndVol, TBM_SETRANGEMAX, 1, 200 );
    SendMessage( hwndVol, TBM_SETPOS, 1, 100 );
    SendMessage( hwndVol, TBM_SETTICFREQ, 50, 0 );  

    // Reposition the trackbar
    GetClientRect( hwnd, &rect );
    MoveWindow( hwndVol, rect.right - rect.left - 40, 
                rect.bottom - rect.top - 2*(MENU_HEIGHT-1) - SLIDER_HEIGHT, 
                40, SLIDER_HEIGHT, TRUE );

    ShowWindow( hwndVol, SW_HIDE );

    return hwndVol;
}

/***********************************************************************

FUNCTION: 
  CreateStatusBar

PURPOSE: 
  Registers the StatusBar control class and creates a Statusbar.

***********************************************************************/
HWND CreateStatusBar( HWND hwnd, HINSTANCE hInst )
{
    DWORD dwStyle;
    HWND hwndSB;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof (INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers Statusbar control classes from the common control dll
    InitCommonControlsEx( &iccex );

    // Create the statusbar control
    dwStyle = WS_VISIBLE | WS_CHILD | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN;

    hwndSB = CreateWindowEx( 0, STATUSCLASSNAME, NULL,
                             WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_BOTTOM |
                             TBS_RIGHT  |WS_CLIPSIBLINGS,
                             0, 0, CW_USEDEFAULT, 50, hwnd, NULL, hInst, 0 );

    if (!hwndSB ) return NULL;

    // Get the coordinates of the parent window's client area. 
    GetClientRect( hwnd, &rect );

    // allocate memory for the panes of status bar
    int nopanes = 2;
    int *indicators = new int[nopanes];

    // set width for the panes
    indicators[0] = 3 * ( rect.right - rect.left ) / 4;
    indicators[1] = rect.right - rect.left;

    // call functions to set style
    SendMessage( hwndSB, SB_SETPARTS, (WPARAM)nopanes, (LPARAM)indicators );

    return hwndSB;
}

/***********************************************************************
FUNCTION:
  CreateDialogBox

PURPOSE:
  Creates a Dialog Box.
***********************************************************************/
int CBaseWindow::CreateDialogBox( HWND hwnd, CBaseWindow *p_obj )
{
    uint8_t p_buffer[sizeof(DLGTEMPLATE) + sizeof(WORD) * 4];
    DLGTEMPLATE *p_dlg_template = (DLGTEMPLATE *)p_buffer;
    memset( p_dlg_template, 0, sizeof(DLGTEMPLATE) + sizeof(WORD) * 4 );

    // these values are arbitrary, they won't be used normally anyhow
    p_dlg_template->x  = 0; p_dlg_template->y  = 0;
    p_dlg_template->cx = 300; p_dlg_template->cy = 300;
    p_dlg_template->style =
        DS_MODALFRAME|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX;

    return DialogBoxIndirectParam( GetModuleHandle(0), p_dlg_template, hwnd,
                                   (DLGPROC)p_obj->BaseWndProc, (LPARAM)p_obj );
}

/***********************************************************************
FUNCTION: 
  BaseWndProc

PURPOSE: 
  Processes messages sent to the main window.
***********************************************************************/
LRESULT CALLBACK CBaseWindow::BaseWndProc( HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam )
{
    CBaseWindow *p_obj;

    // check to see if a copy of the 'this' pointer needs to be saved
    if( msg == WM_CREATE )
    {
        p_obj = (CBaseWindow *)(((LPCREATESTRUCT)lParam)->lpCreateParams);
        SetWindowLong( hwnd, GWL_USERDATA,
                       (LONG)((LPCREATESTRUCT)lParam)->lpCreateParams );

        p_obj->hWnd = hwnd;
    }

    if( msg == WM_INITDIALOG )
    {
        p_obj = (CBaseWindow *)lParam;
        SetWindowLong( hwnd, GWL_USERDATA, lParam );
        p_obj->hWnd = hwnd;
    }

    // Retrieve the pointer
    p_obj = (CBaseWindow *)GetWindowLong( hwnd, GWL_USERDATA );

    if( !p_obj ) return DefWindowProc( hwnd, msg, wParam, lParam );

    // Filter message through child classes
    return p_obj->WndProc( hwnd, msg, wParam, lParam );
}

/***********************************************************************
FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
***********************************************************************/
LRESULT CALLBACK Interface::WndProc( HWND hwnd, UINT msg, WPARAM wp,
                                     LPARAM lp )
{
    switch( msg )
    {
    case WM_CREATE:
        hwndCB = CreateMenuBar( hwnd, hInst );
        hwndTB = CreateToolBar( hwnd, hInst );
        hwndSlider = CreateSliderBar( hwnd, hInst );
        hwndLabel = CreateStaticText( hwnd, hInst );
        hwndVol = CreateVolTrackBar( hwnd, hInst );
#ifdef UNDER_CE
        hwndSB = CreateStatusBar( hwnd, hInst );
#endif

        /* Video window */
        if( config_GetInt( p_intf, "wince-embed" ) )
            video = CreateVideoWindow( p_intf, hwnd );

        ti = new Timer(p_intf, hwnd, this);

        // Hide the SIP button (WINCE only)
        SetForegroundWindow( hwnd );
        SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
        break;

    case WM_COMMAND:
        switch( GET_WM_COMMAND_ID(wp,lp) )
        {
        case ID_FILE_QUICKOPEN: 
            OnOpenFileSimple();
            break;

        case ID_FILE_OPENFILE: 
            open = new OpenDialog( p_intf, hInst, FILE_ACCESS,
                                   ID_FILE_OPENFILE, OPEN_NORMAL );
            CreateDialogBox( hwnd, open );
            delete open;
            break;

        case ID_FILE_OPENNET:
            open = new OpenDialog( p_intf, hInst, NET_ACCESS, ID_FILE_OPENNET,
                                   OPEN_NORMAL );
            CreateDialogBox( hwnd, open );
            delete open;
            break;

        case PlayStream_Event: 
            OnPlayStream();
            break;

        case StopStream_Event: 
            OnStopStream();
            break;

        case PrevStream_Event: 
            OnPrevStream();
            break;

        case NextStream_Event: 
            OnNextStream();
            break;

        case SlowStream_Event: 
            OnSlowStream();
            break;

        case FastStream_Event: 
            OnFastStream();
            break;

        case ID_FILE_ABOUT: 
        {
            string about = (string)"VLC media player " PACKAGE_VERSION +
                _("\n(WinCE interface)\n\n") +
                _("(c) 1996-2005 - the VideoLAN Team\n\n") +
                _("The VideoLAN team <videolan@videolan.org>\n"
                  "http://www.videolan.org/\n\n");

            MessageBox( hwnd, _FROMMB(about.c_str()),
                        _T("About VLC media player"), MB_OK );
            break;
        }

        case ID_FILE_EXIT:
            SendMessage( hwnd, WM_CLOSE, 0, 0 );
            break;

        case ID_VIEW_STREAMINFO:
            fi = new FileInfo( p_intf, hInst );
            CreateDialogBox( hwnd, fi );
            delete fi;
            break;

        case ID_VIEW_MESSAGES:
            hmsg = new Messages( p_intf, hInst );
            CreateDialogBox( hwnd, hmsg );
            delete hmsg;
            break;

        case ID_VIEW_PLAYLIST:
            pl = new Playlist( p_intf, hInst );
            CreateDialogBox( hwnd, pl );
            delete pl;
            break;

        case ID_PREFERENCES:
            pref = new PrefsDialog( p_intf, hInst );
            CreateDialogBox( hwnd, pref );
            delete pref;
            break;
                  
        default:
            OnMenuEvent( p_intf, GET_WM_COMMAND_ID(wp,lp) );
            // we should test if it is a menu command
        }
        break;
  
    case WM_TIMER:
        ti->Notify();
        break;

    case WM_CTLCOLORSTATIC: 
        if( ( (HWND)lp == hwndSlider ) || ( (HWND)lp == hwndVol ) )
        { 
            return( (LRESULT)::GetSysColorBrush(COLOR_3DFACE) ); 
        }
        if( (HWND)lp == hwndLabel )
        {
            SetBkColor( (HDC)wp, RGB (192, 192, 192) ); 
            return( (LRESULT)::GetSysColorBrush(COLOR_3DFACE) ); 
        }
        break;

    case WM_HSCROLL:
        if( (HWND)lp == hwndSlider ) OnSliderUpdate( wp );
        break;

    case WM_VSCROLL:
        if( (HWND)lp == hwndVol ) OnChange( wp );
        break;

    case WM_INITMENUPOPUP:
        RefreshSettingsMenu( p_intf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_SETTINGS ) );
        RefreshAudioMenu( p_intf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_AUDIO ) );
        RefreshVideoMenu( p_intf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_VIDEO ) );
        RefreshNavigMenu( p_intf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_NAVIGATION ) );
        break;

    case WM_LBUTTONDOWN:
        {
            SHRGINFO shrg;
            shrg.cbSize = sizeof( shrg );
            shrg.hwndClient = hwnd;
            shrg.ptDown.x = LOWORD(lp);
            shrg.ptDown.y = HIWORD(lp);
            shrg.dwFlags = SHRG_RETURNCMD ;

            if( SHRecognizeGesture( &shrg ) == GN_CONTEXTMENU )
                PopupMenu( p_intf, hwnd, shrg.ptDown );
        }
        break;

    case WM_HELP:
        MessageBox (hwnd, _T("Help"), _T("Help"), MB_OK);
        break;

    case WM_CLOSE:
        DestroyWindow( hwndCB );
        DestroyWindow( hwnd );
        break;

    case WM_ENTERMENULOOP:
    case WM_KILLFOCUS:
        if( video && video->hWnd )
            SendMessage( video->hWnd, WM_KILLFOCUS, 0, 0 );
        break;

    case WM_EXITMENULOOP:
    case WM_SETFOCUS:
        if( video && video->hWnd )
            SendMessage( video->hWnd, WM_SETFOCUS, 0, 0 );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    }

    return DefWindowProc( hwnd, msg, wp, lp );
}

void Interface::OnOpenFileSimple( void )
{
    OPENFILENAME ofn;
    TCHAR DateiName[80+1] = _T("\0");
    static TCHAR szFilter[] = _T("All (*.*)\0*.*\0");

    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    memset( &ofn, 0, sizeof(OPENFILENAME) );
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.hInstance = hInst;
    ofn.lpstrFilter = szFilter;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;     
    ofn.lpstrFile = (LPTSTR)DateiName; 
    ofn.nMaxFile = 80;
    ofn.lpstrFileTitle = NULL; 
    ofn.nMaxFileTitle = 40;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = _T("Quick Open File");
    ofn.Flags = 0; 
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );

    if( GetOpenFile( &ofn ) )
    {
        char *psz_filename = _TOMB(ofn.lpstrFile);
        playlist_Add( p_playlist, psz_filename, psz_filename,
                      PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
    }

    vlc_object_release( p_playlist );
}

void Interface::OnPlayStream( void )
{
    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    if( p_playlist->i_size && p_playlist->i_enabled )
    {
        vlc_value_t state;

        input_thread_t *p_input = (input_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

        if( p_input == NULL )
        {
            /* No stream was playing, start one */
            playlist_Play( p_playlist );
            TogglePlayButton( PLAYING_S );
            vlc_object_release( p_playlist );
            return;
        }

        var_Get( p_input, "state", &state );

        if( state.i_int != PAUSE_S )
        {
            /* A stream is being played, pause it */
            state.i_int = PAUSE_S;
        }
        else
        {
            /* Stream is paused, resume it */
            state.i_int = PLAYING_S;
        }
        var_Set( p_input, "state", state );

        TogglePlayButton( state.i_int );
        vlc_object_release( p_input );
        vlc_object_release( p_playlist );
    }
    else
    {
        /* If the playlist is empty, open a file requester instead */
        vlc_object_release( p_playlist );
        OnOpenFileSimple();
    }
}

void Interface::TogglePlayButton( int i_playing_status )
{
    TBREPLACEBITMAP tbrb;
    tbrb.hInstOld = tbrb.hInstNew = (HINSTANCE) hInst;
    tbrb.nButtons = NUMIMAGES;

    if( i_playing_status == i_old_playing_status ) return;

    if( i_playing_status == PLAYING_S )
    {
        tbrb.nIDOld = IDB_BITMAP2;
        tbrb.nIDNew = IDB_BITMAP1;

        SendMessage( hwndTB, TB_REPLACEBITMAP, (WPARAM)0,
                     (LPARAM)(LPTBREPLACEBITMAP)&tbrb );
    }
    else
    {
        tbrb.nIDOld = IDB_BITMAP1;
        tbrb.nIDNew = IDB_BITMAP2;

        SendMessage( hwndTB, TB_REPLACEBITMAP, (WPARAM)0,
                     (LPARAM)(LPTBREPLACEBITMAP)&tbrb );
    }

    UpdateWindow( hwndTB );

    i_old_playing_status = i_playing_status;
}

void Interface::OnVideoOnTop( void )
{
    vlc_value_t val;

    vout_thread_t *p_vout = (vout_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );

    if( p_vout == NULL ) return;

    if( var_Get( (vlc_object_t *)p_vout, "video-on-top", &val ) < 0 )
        return;

    val.b_bool = !val.b_bool;
    var_Set( (vlc_object_t *)p_vout, "video-on-top", val );

    vlc_object_release( (vlc_object_t *)p_vout );
}

void Interface::OnSliderUpdate( int wp )
{
    vlc_mutex_lock( &p_intf->change_lock );
    input_thread_t *p_input = p_intf->p_sys->p_input;

    int dwPos = SendMessage( hwndSlider, TBM_GETPOS, 0, 0 ); 

    if( (int)LOWORD(wp) == SB_THUMBPOSITION ||
        (int)LOWORD(wp) == SB_ENDSCROLL )
    {
        if( p_intf->p_sys->i_slider_pos != dwPos && p_input )
        {
            vlc_value_t pos;
            pos.f_float = (float)dwPos / (float)SLIDER_MAX_POS;
            var_Set( p_input, "position", pos );
        }

        p_intf->p_sys->b_slider_free = VLC_TRUE;
    }
    else
    {
        p_intf->p_sys->b_slider_free = VLC_FALSE;

        if( p_input )
        {
            /* Update stream date */
            char psz_time[ MSTRTIME_MAX_SIZE ], psz_total[ MSTRTIME_MAX_SIZE ];
            mtime_t i_seconds;

            i_seconds = var_GetTime( p_input, "length" ) / I64C(1000000 );
            secstotimestr( psz_total, i_seconds );

            i_seconds = var_GetTime( p_input, "time" ) / I64C(1000000 );
            secstotimestr( psz_time, i_seconds );

            SendMessage( hwndLabel, WM_SETTEXT, (WPARAM)1,
                         (LPARAM)_FROMMB(psz_time) );
        }
    }

    vlc_mutex_unlock( &p_intf->change_lock );
}

void Interface::OnChange( int wp )
{
    DWORD dwPos = SendMessage( hwndVol, TBM_GETPOS, 0, 0 );

    if( LOWORD(wp) == SB_THUMBPOSITION || LOWORD(wp) == SB_ENDSCROLL )
    {
        Change( 200 - (int)dwPos );
    }
}

void Interface::Change( int i_volume )
{
    aout_VolumeSet( p_intf, i_volume * AOUT_VOLUME_MAX / 200 / 2 );
#if 0
    SetToolTip( wxString::Format((wxString)wxU(_("Volume")) + wxT(" %d"),
                i_volume ) );
#endif
}

void Interface::OnStopStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Stop( p_playlist );
    TogglePlayButton( PAUSE_S );
    vlc_object_release( p_playlist );
}

void Interface::OnPrevStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnNextStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnSlowStream( void )
{
    input_thread_t *p_input = (input_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input == NULL ) return;

    vlc_value_t val; val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-slower", val );
    vlc_object_release( p_input );
}

void Interface::OnFastStream( void )
{
    input_thread_t *p_input = (input_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input == NULL ) return;

    vlc_value_t val; val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-faster", val );
    vlc_object_release( p_input );
}
