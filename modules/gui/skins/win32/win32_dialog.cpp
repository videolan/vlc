/*****************************************************************************
 * win32_dialog.cpp: Win32 implementation of some dialog boxes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_dialog.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- WIN32 -----------------------------------------------------------------
#define _WIN32_IE 0x0400    // Yes, i think it's a fucking kludge !
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <richedit.h>

//--- SKIN ------------------------------------------------------------------
#include "banks.h"
#include "dialog.h"
#include "os_dialog.h"
#include "skin_common.h"
#include "theme.h"
#include "os_theme.h"
#include "event.h"
#include "os_api.h"


//---------------------------------------------------------------------------
// Open file dialog box
//---------------------------------------------------------------------------
Win32OpenFileDialog::Win32OpenFileDialog( string title, bool multiselect ) :
    OpenFileDialog( title, multiselect )
{
}
//---------------------------------------------------------------------------
Win32OpenFileDialog::~Win32OpenFileDialog()
{
}
//---------------------------------------------------------------------------
void Win32OpenFileDialog::AddFilter( string name, string type )
{
    unsigned int i;

    for( i = 0; i < name.length(); i++ )
        Filter[FilterLength++] = name[i];

    Filter[FilterLength++] = ' ';
    Filter[FilterLength++] = '(';

    for( i = 0; i < type.length(); i++ )
        Filter[FilterLength++] = type[i];

    Filter[FilterLength++] = ')';
    Filter[FilterLength++] = '\0';

    for( i = 0; i < type.length(); i++ )
        Filter[FilterLength++] = type[i];

    Filter[FilterLength++] = '\0';

    // Ending null character if this filter is the last
    Filter[FilterLength] = '\0';
}
//---------------------------------------------------------------------------
bool Win32OpenFileDialog::Open()
{
    // Initailize dialog box
    OPENFILENAME OpenFile;
    memset( &OpenFile, 0, sizeof( OpenFile ) );
    OpenFile.lStructSize  = sizeof( OPENFILENAME );
    OpenFile.hwndOwner = NULL;
    OpenFile.lpstrFile = new char[MAX_PATH];
    OpenFile.lpstrFile[0] = '\0';
    OpenFile.nMaxFile = MAX_PATH;
    if( MultiSelect )
    {
        OpenFile.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }
    else
    {
        OpenFile.Flags = OFN_EXPLORER;
    }
    OpenFile.lpstrTitle  = Title.c_str();
    OpenFile.lpstrFilter = Filter;

    // Show dialog box
    if( !GetOpenFileName( &OpenFile ) )
        return false;

    // Find files in string result
    char * File = OpenFile.lpstrFile;
    int i       = OpenFile.nFileOffset;
    int last    = OpenFile.nFileOffset;
    string path;
    string tmpFile;


    // If only one file has been selected
    if( File[OpenFile.nFileOffset - 1] != '\0' )
    {
        FileList.push_back( (string)File );
    }
    // If multiple files have been selected
    else
    {
        // Add \ if not present at end of path
        if( File[OpenFile.nFileOffset - 2] != '\\' )
        {
            path = (string)File + '\\';
        }
        else
        {
            path = (string)File;
        }

        // Search filenames
        while( true )
        {
            if( File[i] == '\0' )
            {
                if( i == last )
                    break;
                else
                {
                    // Add file
                    FileList.push_back( path + (string)&File[last] );
                    last = i + 1;
                }
            }
            i++;
        }
    }

    // Free memory
    delete[] OpenFile.lpstrFile;

    return true;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// CALLBACKs
//---------------------------------------------------------------------------
LRESULT CALLBACK LogWindowProc( HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam )
{
    intf_thread_t *p_intf = (intf_thread_t *)GetWindowLongPtr( hwnd,
        GWLP_USERDATA );

    //msg_Err( p_intf, "Message to hwnd %i (%i)", (int)hwnd, (int)uMsg );
    switch( uMsg )
    {
        case WM_PAINT:
            PAINTSTRUCT Infos;
            BeginPaint( hwnd , &Infos );
            EndPaint( hwnd , &Infos );
            return 0;

        case WM_SIZE:
            if( ( (Win32Theme *)p_intf->p_sys->p_theme )
                ->GetLogHandle() == hwnd )
            {
                SetWindowPos( ( (Win32LogWindow *)
                    p_intf->p_sys->p_theme->GetLogWindow() )->GetRichCtrl(),
                    0, 0, 0, LOWORD( lParam ), HIWORD( lParam ),
                    SWP_NOREDRAW|SWP_NOZORDER|SWP_NOMOVE );
            }
            return 0;

        case WM_CLOSE:
            OSAPI_PostMessage( NULL, VLC_LOG_SHOW, 0, (int)false );
            p_intf->p_sys->p_theme->EvtBank->Get( "hide_log" )
                ->PostSynchroMessage();
            return 0;

    }
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}
//---------------------------------------------------------------------------
DWORD CALLBACK LogWindowStream( DWORD_PTR dwCookie, LPBYTE pbBuff,
                                LONG cb, LONG *pcb )
{
    int i;
    char *text = (char *)( (string *)dwCookie )->c_str();

    if( strlen( text ) < (unsigned int)cb )
    {
        *pcb = strlen( text );
        for( i = 0; i < *pcb; i++ )
            pbBuff[i] = text[i];
    }
    else
    {
        *pcb = cb;
        for( i = 0; i < *pcb; i++ )
            pbBuff[i] = text[i];
    }
    delete (string *)dwCookie;
    return 0;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Win32 Log Window class
//---------------------------------------------------------------------------
Win32LogWindow::Win32LogWindow( intf_thread_t *_p_intf ) : LogWindow( _p_intf )
{
    hWindow   = NULL;
    hRichCtrl = NULL;

    // Define window class
    WNDCLASS WindowClass;
    WindowClass.style = CS_VREDRAW|CS_HREDRAW;
    WindowClass.lpfnWndProc = (WNDPROC)LogWindowProc;
    WindowClass.lpszClassName = "LogWindow";
    WindowClass.lpszMenuName = NULL;
    WindowClass.cbClsExtra = 0;
    WindowClass.cbWndExtra = 0;
    WindowClass.hbrBackground = HBRUSH (COLOR_WINDOW);
    WindowClass.hCursor = LoadCursor( NULL , IDC_ARROW );
    WindowClass.hIcon = LoadIcon( GetModuleHandle( NULL ), "VLC_ICON" );
    WindowClass.hInstance = GetModuleHandle( NULL );

    // Register window class
    RegisterClass( &WindowClass );

    // Load library
    LoadLibrary("riched20.dll");

    // Init common controls
    InitCommonControlsEx( NULL );

    // Create log window
    hWindow = CreateWindowEx( 0, "LogWindow", "Log Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, 0, 0,
        GetModuleHandle( NULL ), NULL );
    // Store with it a pointer to the interface thread
    SetWindowLongPtr( hWindow, GWLP_USERDATA, (LONG_PTR)p_intf );

    // Create rich text control
    hRichCtrl = CreateWindowEx( 0, RICHEDIT_CLASS, NULL,
        WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWindow, 0, GetModuleHandle( NULL ), 0);

    // Change text format
    SendMessage( hRichCtrl, EM_SETBKGNDCOLOR, 0, RGB(0,0,0) );
    ChangeColor( RGB( 128, 128, 128 ) );
    RtfHeader = "{\\rtf1 ";

    Clear();
}
//---------------------------------------------------------------------------
Win32LogWindow::~Win32LogWindow()
{
    DestroyWindow( hRichCtrl );
    DestroyWindow( hWindow );
}
//---------------------------------------------------------------------------
void Win32LogWindow::Clear()
{
    EDITSTREAM *Stream;
    Stream = new EDITSTREAM;
    string *buffer = new string( RtfHeader );
    Stream->dwCookie = (DWORD)buffer;
    Stream->dwError  = 0;
    Stream->pfnCallback = (EDITSTREAMCALLBACK)LogWindowStream;
    SendMessage( hRichCtrl, EM_STREAMIN, SF_RTF, (LPARAM)Stream );
}
//---------------------------------------------------------------------------
void Win32LogWindow::AddLine( string line )
{
    // Initialize stream
        EDITSTREAM *Stream;
        string *buffer      = new string( RtfHeader + line + "\\par }" );
        Stream              = new EDITSTREAM;
        Stream->dwCookie    = (DWORD)buffer;
        Stream->dwError     = 0;
        Stream->pfnCallback = (EDITSTREAMCALLBACK)LogWindowStream;

    SendMessage( hRichCtrl, EM_STREAMIN, SF_RTF|SFF_SELECTION, (LPARAM)Stream );

    SendMessage( hRichCtrl, WM_VSCROLL, SB_BOTTOM, 0 );
}
//---------------------------------------------------------------------------
void Win32LogWindow::ChangeColor( int color, bool bold )
{
    CHARFORMAT format;
    memset(&format, 0, sizeof(CHARFORMAT));
    format.cbSize      = sizeof(CHARFORMAT);
    format.dwMask      = bold ? CFM_COLOR|CFM_BOLD : CFM_COLOR;
    format.dwEffects   = bold ? CFE_BOLD           : 0;
    format.crTextColor = color;
    SendMessage( hRichCtrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format );
}
//---------------------------------------------------------------------------
void Win32LogWindow::Show()
{
    ShowWindow( hWindow, SW_SHOW );
    Visible = true;
}
//---------------------------------------------------------------------------
void Win32LogWindow::Hide()
{
    ShowWindow( hWindow, SW_HIDE );
    Visible = false;
}
//---------------------------------------------------------------------------

