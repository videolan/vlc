/*****************************************************************************
 * vlc.c: the vlc player, WinCE version
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlc.c,v 1.4 2002/11/20 16:43:32 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include "config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "../share/resource.h"

#include <vlc/vlc.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static LRESULT CALLBACK About ( HWND hDlg, UINT message,
                                WPARAM wParam, LPARAM lParam );
static long FAR PASCAL WndProc ( HWND hWnd, UINT message,
                                 WPARAM wParam, LPARAM lParam );

/*****************************************************************************
 * Global variables.
 *****************************************************************************/
HINSTANCE hInst;
HWND      hwndCB;

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPTSTR lpCmdLine, int nCmdShow )
{
    int    i_ret;
    int    i_argc = 5;
    char * ppsz_argv[] = { lpCmdLine, "-vv", "--intf", "dummy", "shovel.mpeg", /*"washington.mpeg",*/ NULL };
    HWND   window;
    MSG    message;

    HACCEL   hAccelTable;
    WNDCLASS wc;

    char     psz_title[100];
    wchar_t  pwz_title[100];

    /* Store our instance for future reference */
    hInst = hInstance;

    /* Register window class */
    wc.style          = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc    = (WNDPROC) WndProc;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = 0;
    wc.hInstance      = hInst;
    wc.hIcon          = 0;
    wc.hCursor        = 0;
    wc.hbrBackground  = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName   = 0;
    wc.lpszClassName  = L"VLC";

    RegisterClass(&wc);

    /* Print the version information */
    sprintf( psz_title, "VideoLAN Client %s", VLC_Version() );
    MultiByteToWideChar( CP_ACP, 0, psz_title, -1, pwz_title, 100 );

    /* Create our nice window */
    window = CreateWindow( L"VLC", pwz_title,
                           WS_VISIBLE | WS_SIZEBOX | WS_CAPTION,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           //CW_USEDEFAULT, CW_USEDEFAULT, 
                           200,100,
                           NULL, NULL, hInst, NULL );

    ShowWindow( window, nCmdShow );
    UpdateWindow( window );

    hAccelTable = LoadAccelerators( hInst, (LPCTSTR)IDC_NIOUP );

    /* Create a libvlc structure */
    i_ret = VLC_Create();
    if( i_ret < 0 )
    {
        DestroyWindow( window );
        return i_ret;
    }

    /* Initialize libvlc */
    i_ret = VLC_Init( 0, i_argc, ppsz_argv );
    if( i_ret < 0 )
    {
        VLC_Destroy( 0 );
        DestroyWindow( window );
        return i_ret;
    }

    /* Run libvlc, in non-blocking mode */
    i_ret = VLC_Play( 0 );

    /* Add a non-blocking interface and keep the return value */
    i_ret = VLC_AddIntf( 0, NULL, VLC_FALSE );

    while( GetMessage( &message, NULL, 0, 0 ) )
    {
        if( !TranslateAccelerator(message.hwnd, hAccelTable, &message) )
        {
            TranslateMessage( &message );
            DispatchMessage( &message );
        }
    }

    /* Kill the threads */
    VLC_Die( 0 );

    /* Finish the threads */
    VLC_Stop( 0 );

    /* Destroy the libvlc structure */
    VLC_Destroy( 0 );

    DestroyWindow( window );

    return i_ret;
}

/*****************************************************************************
 * Message handler for the About box.
 *****************************************************************************/
static LRESULT CALLBACK About ( HWND hDlg, UINT message,
                                WPARAM wParam, LPARAM lParam)
{
    RECT rt, rt1;
    int DlgWidth, DlgHeight;    // dialog width and height in pixel units
    int NewPosX, NewPosY;

    switch( message )
    {
        case WM_INITDIALOG:
            /* trying to center the About dialog */
            if( GetWindowRect( hDlg, &rt1 ) )
            {
                GetClientRect( GetParent(hDlg), &rt );
                DlgWidth    = rt1.right - rt1.left;
                DlgHeight   = rt1.bottom - rt1.top ;
                NewPosX     = ( rt.right - rt.left - DlgWidth ) / 2;
                NewPosY     = ( rt.bottom - rt.top - DlgHeight ) / 2;

                /* if the About box is larger than the physical screen */
                if( NewPosX < 0 ) NewPosX = 0;
                if( NewPosY < 0 ) NewPosY = 0;
                SetWindowPos( hDlg, 0, NewPosX, NewPosY,
                              0, 0, SWP_NOZORDER | SWP_NOSIZE );
            }
            return TRUE;

        case WM_COMMAND:
            if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
            {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/*****************************************************************************
 * Message handler for the main window
 *****************************************************************************/
static long FAR PASCAL WndProc ( HWND hWnd, UINT message,
                                 WPARAM wParam, LPARAM lParam )
{
    HDC hdc;
    int wmId, wmEvent;
    PAINTSTRUCT ps;

    switch( message )
    {
        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            // Parse the menu selections:
            switch( wmId )
            {
                case IDM_HELP_ABOUT:
                   DialogBox( hInst, (LPCTSTR)IDD_ABOUTBOX,
                              hWnd, (DLGPROC)About );
                   break;
                case IDM_PLOP:
                   /* Do random stuff */
                   break;
                case IDM_FILE_EXIT:
                   DestroyWindow(hWnd);
                   break;
                default:
                   return DefWindowProc( hWnd, message, wParam, lParam );
            }
            break;
        case WM_CREATE:
            hwndCB = CommandBar_Create(hInst, hWnd, 1);
            CommandBar_InsertMenubar(hwndCB, hInst, IDM_MENU, 0);
            //CommandBar_AddAdornments(hwndCB, 0, 0);
            break;
        case WM_PAINT:
        {
            RECT rt;
            hdc = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rt);
            DrawText( hdc, L"VLC roulaize!", _tcslen(L"VLC roulaize!"), &rt,
                      DT_SINGLELINE | DT_VCENTER | DT_CENTER );
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_DESTROY:
            CommandBar_Destroy(hwndCB);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

