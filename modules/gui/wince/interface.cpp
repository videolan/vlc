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

#include <string>
#include <stdio.h>
using namespace std; 

#include <winuser.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <aygshell.h>

#include <commdlg.h> // common dialogs -> fileopen.lib ?

#include "wince.h"

#define NUMIMAGES     9   // Number of buttons in the toolbar           
#define IMAGEWIDTH    17   // Width of the buttons in the toolbar  
#define IMAGEHEIGHT   16   // Height of the buttons in the toolbar  
#define BUTTONWIDTH   0    // Width of the button images in the toolbar
#define BUTTONHEIGHT  0    // Height of the button images in the toolbar
#define ID_TOOLBAR    2000 // Identifier of the main tool bar
#define dwTBFontStyle TBSTYLE_BUTTON | TBSTYLE_CHECK | TBSTYLE_GROUP // style for toolbar buttons

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
#define EXTRA_PREFS _T("Shows the extended GUI")

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
  {0, ID_FILE_QUICKOPEN,        TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {1, ID_FILE_OPENNET,       TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP,     0, -1},
  {2, StopStream_Event,       TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {3, PlayStream_Event,        TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP,     0, -1},
  {4, ID_VIEW_PLAYLIST,       TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP,     0, -1},
  {5, PrevStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {6, NextStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP,     0, -1},
  {7, SlowStream_Event,      TBSTATE_ENABLED, TBSTYLE_BUTTON,  0, -1},
  {8, FastStream_Event,       TBSTATE_ENABLED, TBSTYLE_BUTTON,   0, -1},
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
    pIntf = _p_intf;
    hwndMain = hwndCB = hwndTB = hwndSlider = hwndLabel = hwndVol = hwndSB = 0;
    i_old_playing_status = PAUSE_S;

    hInst = hInstance; // Store instance handle in our global variable

    // Check if the application is running.
    // If it's running then focus its window.
    hwndMain = FindWindow( _T("VLC WinCE"), _T("VLC media player") );  
    if( hwndMain ) 
    {
        SetForegroundWindow( hwndMain );
        return TRUE;
    }

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

    // Create main window
    hwndMain =
        CreateWindow( _T("VLC WinCE"), _T("VLC media player"), WS_VISIBLE,
                      0, MENU_HEIGHT, CW_USEDEFAULT, CW_USEDEFAULT,
                      NULL, NULL, hInstance, (void *)this );

    if( !hwndMain ) return FALSE;

    ShowWindow( hwndMain, TRUE );
    UpdateWindow( hwndMain );

    return TRUE;
}

/***********************************************************************

FUNCTION: 
  CreateToolbar

PURPOSE: 
  Registers the TOOLBAR control class and creates a toolbar.

***********************************************************************/
HWND WINAPI Interface::CreateToolbar( HWND hwnd )
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

    hwndTB = CreateToolbarEx( hwnd, dwStyle, NULL, NUMIMAGES,
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
  CreateSliderbar

PURPOSE: 
  Registers the TRACKBAR_CLASS control class and creates a trackbar.

***********************************************************************/
HWND WINAPI Interface::CreateSliderbar( HWND hwnd )
{
    HWND hwndSlider;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers TRACKBAR_CLASS control classes from the common control dll
    InitCommonControlsEx( &iccex );

    hwndSlider = CreateWindowEx( NULL, TRACKBAR_CLASS, NULL,
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

HWND WINAPI Interface::CreateStaticText( HWND hwnd )
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
  CreateVolTrackbar

PURPOSE: 
  Registers the TRACKBAR_CLASS control class and creates a trackbar.

***********************************************************************/
HWND WINAPI Interface::CreateVolTrackbar( HWND hwnd )
{
    HWND hwndVol;
    RECT rect;

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_BAR_CLASSES;

    // Registers TRACKBAR_CLASS control classes from the common control dll
    InitCommonControlsEx( &iccex );

    hwndVol = CreateWindowEx( NULL, TRACKBAR_CLASS, NULL,
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
  CreateStatusbar

PURPOSE: 
  Registers the StatusBar control class and creates a Statusbar.

***********************************************************************/
HWND WINAPI Interface::CreateStatusbar( HWND hwnd )
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

    hwndSB = CreateWindowEx( NULL, STATUSCLASSNAME, NULL,
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
  BaseWndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT CALLBACK CBaseWindow::BaseWndProc( HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam )
{
    // check to see if a copy of the 'this' pointer needs to be saved
    if( msg == WM_CREATE )
    {
        CBaseWindow *pObj = reinterpret_cast<CBaseWindow *>
            ((long)((LPCREATESTRUCT)lParam)->lpCreateParams);
        ::SetWindowLong( hwnd, GWL_USERDATA,
                         (LONG)((LPCREATESTRUCT)lParam)->lpCreateParams );

        pObj->DlgFlag = FALSE;
        pObj->hWnd = hwnd; // videowindow
    }

    if( msg == WM_INITDIALOG )
    {
        CBaseWindow *pObj = reinterpret_cast<CBaseWindow *>(lParam);
        ::SetWindowLong( hwnd, GWL_USERDATA, lParam );
        pObj->DlgFlag = TRUE;
        pObj->hWnd = hwnd; //streamout
    }

    BOOL bProcessed = FALSE;
    LRESULT lResult;

    // Retrieve the pointer
    CBaseWindow *pObj =
        reinterpret_cast<CBaseWindow *>(::GetWindowLong( hwnd, GWL_USERDATA ));

    // Filter message through child classes
    if( pObj )
        lResult = pObj->WndProc( hwnd, msg, wParam, lParam, &bProcessed );
    else
        return ( pObj->DlgFlag ? FALSE : TRUE ); // message not processed

    if( pObj->DlgFlag )
        return bProcessed; // processing a dialog message return TRUE if processed
    else if( !bProcessed )
        // If message was unprocessed and not a dialog, send it back to Windows
        lResult = DefWindowProc( hwnd, msg, wParam, lParam );

    return lResult; // processing a window message return FALSE if processed
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT CALLBACK Interface::WndProc( HWND hwnd, UINT msg, WPARAM wp,
                                     LPARAM lp, PBOOL pbProcessed )
{
    SHMENUBARINFO mbi;

    // call the base class first
    LRESULT lResult = CBaseWindow::WndProc( hwnd, msg, wp, lp, pbProcessed );
    BOOL bWasProcessed = *pbProcessed;
    *pbProcessed = TRUE;

    switch( msg )
    {
    case WM_CREATE:
        //Create the menubar
        memset( &mbi, 0, sizeof(SHMENUBARINFO) );
        mbi.cbSize     = sizeof(SHMENUBARINFO);
        mbi.hwndParent = hwnd;
        mbi.nToolBarId = IDR_MENUBAR;
        mbi.hInstRes   = hInst;
        mbi.nBmpId     = 0;
        mbi.cBmpImages = 0;

        if( !SHCreateMenuBar(&mbi) )
        {
            MessageBox(hwnd, L"SHCreateMenuBar Failed", L"Error", MB_OK);
            //return -1;
        }
    
        hwndCB = mbi.hwndMB;

        // Creates the toolbar
        hwndTB = CreateToolbar( hwnd );

        // Creates the sliderbar
        hwndSlider = CreateSliderbar( hwnd );

        // Creates the time label
        hwndLabel = CreateStaticText( hwnd );

        // Creates the volume trackbar
        hwndVol = CreateVolTrackbar( hwnd );

        // Creates the statusbar
        hwndSB = CreateStatusbar( hwnd );

        /* Video window */
        video = CreateVideoWindow( pIntf, hInst, hwnd );

        ti = new Timer(pIntf, hwnd, this);

        // Hide the SIP button (WINCE only)
        SetForegroundWindow( hwnd );
        SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );

        return lResult;

    case WM_COMMAND:
        switch( GET_WM_COMMAND_ID(wp,lp) )
        {
        case ID_FILE_QUICKOPEN: 
            OnOpenFileSimple();
            return lResult;

        case ID_FILE_OPENFILE: 
            open = new OpenDialog( pIntf, hInst, FILE_ACCESS,
                                   ID_FILE_OPENFILE, OPEN_NORMAL );
            DialogBoxParam( hInst, (LPCTSTR)IDD_DUMMY, hwnd,
                            (DLGPROC)open->BaseWndProc, (long)open );
            delete open;
            return lResult;

        case ID_FILE_OPENNET:
            open = new OpenDialog( pIntf, hInst, NET_ACCESS, ID_FILE_OPENNET,
                                   OPEN_NORMAL );
            DialogBoxParam( hInst, (LPCTSTR)IDD_DUMMY, hwnd,
                            (DLGPROC)open->BaseWndProc, (long)open );
            delete open;
            return lResult;

        case PlayStream_Event: 
            OnPlayStream();
            return lResult;

        case StopStream_Event: 
            OnStopStream();
            return lResult;

        case PrevStream_Event: 
            OnPrevStream();
            return lResult;

        case NextStream_Event: 
            OnNextStream();
            return lResult;

        case SlowStream_Event: 
            OnSlowStream();
            return lResult;

        case FastStream_Event: 
            OnFastStream();
            return lResult;

        case ID_FILE_ABOUT: 
        {
            string about = (string)"VLC media player " PACKAGE_VERSION +
                _("\n(WinCE interface)\n\n") +
                _("(c) 1996-2005 - the VideoLAN Team\n\n") +
                _("The VideoLAN team <videolan@videolan.org>\n"
                  "http://www.videolan.org/\n\n");

            MessageBox( hwnd, _FROMMB(about.c_str()),
                        _T("About VLC media player"), MB_OK );
            return lResult;
        }

        case ID_FILE_EXIT:
            SendMessage( hwnd, WM_CLOSE, 0, 0 );
            return lResult;

        case ID_VIEW_STREAMINFO:
            fi = new FileInfo( pIntf, hInst );
            DialogBoxParam( hInst, (LPCTSTR)IDD_DUMMY, hwnd,
                            (DLGPROC)fi->BaseWndProc, (long)fi );
            delete fi;
            return lResult;

        case ID_VIEW_MESSAGES:
            hmsg = new Messages( pIntf, hInst );
            DialogBoxParam( hInst, (LPCTSTR)IDD_MESSAGES, hwnd,
                            (DLGPROC)hmsg->BaseWndProc, (long)hmsg );
            delete hmsg;
            return lResult;

        case ID_VIEW_PLAYLIST:
            pl = new Playlist( pIntf, hInst );
            DialogBoxParam( hInst, (LPCTSTR)IDD_DUMMY, hwnd,
                            (DLGPROC)pl->BaseWndProc, (long)pl );
            delete pl;
            return lResult;

        case ID_SETTINGS_PREF:
            pref = new PrefsDialog( pIntf, hInst );
            DialogBoxParam( hInst, (LPCTSTR)IDD_DUMMY, hwnd,
                            (DLGPROC)pref->BaseWndProc, (long)pref );
            delete pref;
            return lResult;
                  
        default:
            OnMenuEvent( pIntf, GET_WM_COMMAND_ID(wp,lp) );
            // we should test if it is a menu command
        }
        break;
  
    case WM_TIMER:
        ti->Notify();
        return lResult;

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
        if( (HWND)lp == hwndSlider )
        {
            OnSliderUpdate( wp );
            return lResult;
        }
        break;

    case WM_VSCROLL:
        if( (HWND)lp == hwndVol )
        {
            OnChange( wp );
            return lResult;
        }
        break;

    case WM_INITMENUPOPUP:
      msg_Err( pIntf, "WM_INITMENUPOPUP" );
        RefreshSettingsMenu( pIntf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_SETTINGS ) );
        RefreshAudioMenu( pIntf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_AUDIO ) );
        RefreshVideoMenu( pIntf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_VIDEO ) );
        RefreshNavigMenu( pIntf,
            (HMENU)SendMessage( hwndCB, SHCMBM_GETSUBMENU, (WPARAM)0,
                                (LPARAM)IDM_NAVIGATION ) );

      msg_Err( pIntf, "WM_MEND" );
#if 0
        // Undo the video display because menu is opened
        // due to GAPI, menu top display is not assumed
        // FIXME verify if p_child_window exits
        SendMessage( pIntf->p_sys->p_video_window->p_child_window,
                     WM_INITMENUPOPUP, wp, lp );
#endif

        //refresh screen
        /* InvalidateRect(hwnd, NULL, TRUE);
           /UpdateWindow(hwndCB); //  NULL*/
        break;

#if 0
    case WM_NOTIFY:
        // Redo the video display because menu can be closed
        // FIXME verify if p_child_window exits
        if( (((NMHDR *)lp)->code) == NM_CUSTOMDRAW )
            SendMessage( pIntf->p_sys->p_video_window->p_child_window,
                         WM_NOTIFY, wp, lp );
        return lResult;
#endif

    case WM_HELP:
        MessageBox (hwnd, _T("Help"), _T("Help"), MB_OK);
        return lResult;

    case WM_CLOSE:
        DestroyWindow( hwndCB );
        DestroyWindow( hwnd );
        return lResult;

    case WM_DESTROY:
      PostQuitMessage( 0 );
      return lResult;
    }

    return DefWindowProc( hwnd, msg, wp, lp );
}

void Interface::OnOpenFileSimple( void )
{
    OPENFILENAME ofn;
    TCHAR DateiName[80+1] = _T("\0");
    static TCHAR szFilter[] = _T("All (*.*)\0*.*\0");

    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( pIntf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
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
    ofn.Flags = NULL; 
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );

    if( GetOpenFileName( (LPOPENFILENAME)&ofn ) )
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
        vlc_object_find( pIntf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    if( p_playlist->i_size && p_playlist->i_enabled )
    {
        vlc_value_t state;

        input_thread_t *p_input = (input_thread_t *)
            vlc_object_find( pIntf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

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
        vlc_object_find( pIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );

    if( p_vout == NULL ) return;

    if( var_Get( (vlc_object_t *)p_vout, "video-on-top", &val ) < 0 )
        return;

    val.b_bool = !val.b_bool;
    var_Set( (vlc_object_t *)p_vout, "video-on-top", val );

    vlc_object_release( (vlc_object_t *)p_vout );
}

void Interface::OnSliderUpdate( int wp )
{
    vlc_mutex_lock( &pIntf->change_lock );
    input_thread_t *p_input = pIntf->p_sys->p_input;

    DWORD dwPos = SendMessage( hwndSlider, TBM_GETPOS, 0, 0 ); 

    if( (int)LOWORD(wp) == SB_THUMBPOSITION ||
        (int)LOWORD(wp) == SB_ENDSCROLL )
    {
        if( pIntf->p_sys->i_slider_pos != dwPos && p_input )
        {
            vlc_value_t pos;
            pos.f_float = (float)dwPos / (float)SLIDER_MAX_POS;
            var_Set( p_input, "position", pos );
        }

        pIntf->p_sys->b_slider_free = VLC_TRUE;
    }
    else
    {
        pIntf->p_sys->b_slider_free = VLC_FALSE;

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

    vlc_mutex_unlock( &pIntf->change_lock );
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
    aout_VolumeSet( pIntf, i_volume * AOUT_VOLUME_MAX / 200 / 2 );
#if 0
    SetToolTip( wxString::Format((wxString)wxU(_("Volume")) + wxT(" %d"),
                i_volume ) );
#endif
}

void Interface::OnStopStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( pIntf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Stop( p_playlist );
    TogglePlayButton( PAUSE_S );
    vlc_object_release( p_playlist );
}

void Interface::OnPrevStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( pIntf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnNextStream( void )
{
    playlist_t * p_playlist = (playlist_t *)
        vlc_object_find( pIntf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnSlowStream( void )
{
    input_thread_t *p_input = (input_thread_t *)
        vlc_object_find( pIntf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input == NULL ) return;

    vlc_value_t val; val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-slower", val );
    vlc_object_release( p_input );
}

void Interface::OnFastStream( void )
{
    input_thread_t *p_input = (input_thread_t *)
        vlc_object_find( pIntf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input == NULL ) return;

    vlc_value_t val; val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-faster", val );
    vlc_object_release( p_input );
}
