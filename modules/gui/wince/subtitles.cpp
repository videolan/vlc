/*****************************************************************************
 * subtitles.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
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
SubsFileDialog::SubsFileDialog( intf_thread_t *_p_intf, HINSTANCE _hInst )
{
    /* Initializations */
    p_intf = _p_intf;
    hInst = _hInst;
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT SubsFileDialog::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                 PBOOL pbProcessed  )
{
    SHINITDLGINFO shidi;
    SHMENUBARINFO mbi;
    INITCOMMONCONTROLSEX ic;
    RECT rcClient;

    int size;
    LPWSTR wUnicode;

    char *psz_subsfile;

    float f_fps;
    int i_delay;
    module_config_t *p_item;

    LRESULT lResult = CBaseWindow::WndProc( hwnd, msg, wp, lp, pbProcessed );
    BOOL bWasProcessed = *pbProcessed;
    *pbProcessed = TRUE;

    switch( msg )
    {
    case WM_INITDIALOG:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        //Create the menubar.
        memset (&mbi, 0, sizeof (SHMENUBARINFO));
        mbi.cbSize     = sizeof (SHMENUBARINFO);
        mbi.hwndParent = hwnd;
        mbi.nToolBarId = IDR_DUMMYMENU;
        mbi.hInstRes   = hInst;
        mbi.nBmpId     = 0;
        mbi.cBmpImages = 0;

        if (!SHCreateMenuBar(&mbi))
        {
            MessageBox(hwnd, L"SHCreateMenuBar Failed", L"Error", MB_OK);
            //return -1;
        }

        hwndCB = mbi.hwndMB;

        // Get the client area rect to put the panels in
        GetClientRect(hwnd, &rcClient);

        /* Create the subtitles file textctrl */
        file_box = CreateWindow( _T("STATIC"), _T("Subtitles file"),
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 5, 10, rcClient.right - 2*5, 15,
                                 hwnd, NULL, hInst, NULL );

        psz_subsfile = config_GetPsz( p_intf, "sub-file" );
        if( !psz_subsfile ) psz_subsfile = strdup("");
        size = MultiByteToWideChar( CP_ACP, 0, psz_subsfile, -1, NULL, 0 );
        wUnicode = new WCHAR[size];
        MultiByteToWideChar( CP_ACP, 0, psz_subsfile, -1, wUnicode, size );

        file_combo = CreateWindow( _T("COMBOBOX"), wUnicode,
            WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_SORT | WS_VSCROLL,
            10, 10 + 15 + 10 - 3, rcClient.right - 2*10, 5*15 + 6,
            hwnd, NULL, hInst, NULL );

        free( wUnicode );
        if( psz_subsfile ) free( psz_subsfile );

        browse_button = CreateWindow( _T("BUTTON"), _T("Browse..."),
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        10, 10 + 2*(15 + 10) - 3, 80, 15 + 6,
                        hwnd, NULL, hInst, NULL);

        /* Subtitles encoding */
        encoding_combo = NULL;
        p_item =
            config_FindConfig( VLC_OBJECT(p_intf), "subsdec-encoding" );
        if( p_item )
         {
             enc_box = CreateWindow( _T("STATIC"), _T("Subtitles encoding"),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        5, 10 + 3*(15 + 10), rcClient.right - 2*5, 15,
                        hwnd, NULL, hInst, NULL );

             size = MultiByteToWideChar( CP_ACP, 0, p_item->psz_text, -1, NULL, 0 );
             wUnicode = new WCHAR[size];
             MultiByteToWideChar( CP_ACP, 0, p_item->psz_text, -1, wUnicode, size );

             enc_label = CreateWindow( _T("STATIC"), wUnicode,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 10 + 4*(15 + 10), rcClient.right - 2*10, 15,
                hwnd, NULL, hInst, NULL );

             free( wUnicode );

             size = MultiByteToWideChar( CP_ACP, 0, p_item->psz_value, -1, NULL, 0 );
             wUnicode = new WCHAR[size];
             MultiByteToWideChar( CP_ACP, 0, p_item->psz_value, -1, wUnicode, size );

             encoding_combo = CreateWindow( _T("COMBOBOX"), wUnicode,
                WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | LBS_SORT  | WS_VSCROLL,
                rcClient.right - 150 - 10, 10 + 5*(15 + 10) - 3, 150, 5*15 + 6,
                hwnd, NULL, hInst, NULL );

             free( wUnicode );

             /* build a list of available options */
             for( int i_index = 0; p_item->ppsz_list &&
                    p_item->ppsz_list[i_index]; i_index++ )
             {
                 size = MultiByteToWideChar( CP_ACP, 0, p_item->ppsz_list[i_index], -1, NULL, 0 );
                 wUnicode = new WCHAR[size];
                 MultiByteToWideChar( CP_ACP, 0, p_item->ppsz_list[i_index], -1, wUnicode, size );

                 ComboBox_AddString( encoding_combo, wUnicode );

                 free( wUnicode );

                 if( p_item->psz_value && !strcmp( p_item->psz_value,
                                                   p_item->ppsz_list[i_index] ) )
                   ComboBox_SetCurSel( encoding_combo, i_index );
             }

             if( p_item->psz_value )
             {
                 size = MultiByteToWideChar( CP_ACP, 0, p_item->psz_value, -1, NULL, 0 );
                 wUnicode = new WCHAR[size];
                 MultiByteToWideChar( CP_ACP, 0, p_item->psz_value, -1, wUnicode, size );

                 ComboBox_SelectString( encoding_combo, 0, wUnicode );

                 free( wUnicode );
             }
         }

        /* Misc Subtitles options */
        misc_box = CreateWindow( _T("STATIC"), _T("Subtitles options"),
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 5, 10 + 6*(15 + 10), rcClient.right - 2*5, 15,
                                 hwnd, NULL, hInst, NULL );

        delay_label = CreateWindow( _T("STATIC"), _T("Delay subtitles (in 1/10s)"),
                                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    10, 10 + 7*(15 + 10), rcClient.right - 70 - 2*10, 15,
                                    hwnd, NULL, hInst, NULL );

        i_delay = config_GetInt( p_intf, "sub-delay" );
        wUnicode = new WCHAR[80];
        swprintf( wUnicode, _T("%d"), i_delay );

        delay_edit = CreateWindow( _T("EDIT"), wUnicode,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
                        rcClient.right - 70 - 10, 10 + 7*(15 + 10) - 3, 70, 15 + 6,
                        hwnd, NULL, hInst, NULL);

        free( wUnicode );

        ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
        ic.dwICC = ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&ic);

        delay_spinctrl = CreateUpDownControl(
                        WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
                        0, 0, 0, 0,     hwnd, NULL,     hInst,
                        delay_edit, 650000, -650000, i_delay );

        fps_label = CreateWindow( _T("STATIC"), _T("Frames per second"),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        10, 10 + 8*(15 + 10), rcClient.right - 70 - 2*10, 15,
                        hwnd, NULL, hInst, NULL );

        f_fps = config_GetFloat( p_intf, "sub-fps" );
        wUnicode = new WCHAR[80];
        swprintf( wUnicode, _T("%d"), (int)f_fps );

        fps_edit = CreateWindow( _T("EDIT"), wUnicode,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
                        rcClient.right - 70 - 10, 10 + 8*(15 + 10) - 3, 70, 15 + 6,
                        hwnd, NULL, hInst, NULL);

        free( wUnicode );

        ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
        ic.dwICC = ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&ic);

        fps_spinctrl = CreateUpDownControl(
                        WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
                        0, 0, 0, 0,     hwnd, NULL,     hInst,
                        fps_edit, 16000, 0, (int)f_fps );

        return lResult;

    case WM_COMMAND:
        if ( LOWORD(wp) == IDOK )
        {
            int size;
            BOOL bTemp;
            LPSTR szAnsi;
            LPWSTR wUnicode;

            subsfile_mrl.clear();

            string szFileCombo = "sub-file=";
            size = GetWindowTextLength( file_combo ) + 1;
            wUnicode = new WCHAR[ size ];
            GetWindowText( file_combo, wUnicode, size );
            size = WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, NULL, 0, NULL, &bTemp );
            szAnsi = new char[size];
            WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, szAnsi, size, NULL, &bTemp );
            szFileCombo += szAnsi;
            free( wUnicode ); free( szAnsi );
            subsfile_mrl.push_back( szFileCombo );

            if( GetWindowTextLength( encoding_combo ) != 0 )
            {
                string szEncoding = "subsdec-encoding=";
                size = GetWindowTextLength( encoding_combo ) + 1;
                wUnicode = new WCHAR[ size ];
                GetWindowText( encoding_combo, wUnicode, size );
                size = WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, NULL, 0, NULL, &bTemp );
                szAnsi = new char[size];
                WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, szAnsi, size, NULL, &bTemp );
                szEncoding += szAnsi;
                free( wUnicode ); free( szAnsi );
                subsfile_mrl.push_back( szEncoding );
            }

            string szDelay = "sub-delay=";
            size = Edit_GetTextLength( delay_edit );
            wUnicode = new WCHAR[size + 1]; //Add 1 for the NULL
            Edit_GetText( delay_edit, wUnicode, size + 1);
            size = WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, NULL, 0, NULL, &bTemp );
            szAnsi = new char[size];
            WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, szAnsi, size, NULL, &bTemp );
            szDelay += szAnsi;
            free( wUnicode ); free( szAnsi );
            subsfile_mrl.push_back( szDelay );

            string szFps = "sub-fps=";
            size = Edit_GetTextLength( fps_edit );
            wUnicode = new WCHAR[size + 1]; //Add 1 for the NULL
            Edit_GetText( fps_edit, wUnicode, size + 1);
            size = WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, NULL, 0, NULL, &bTemp );
            szAnsi = new char[size];
            WideCharToMultiByte( CP_ACP, 0, wUnicode, -1, szAnsi, size, NULL, &bTemp );
            szFps += szAnsi;
            free( wUnicode ); free( szAnsi );
            subsfile_mrl.push_back( szFps );

            EndDialog( hwnd, LOWORD( wp ) );
            return TRUE;
        }
        if( HIWORD(wp) == BN_CLICKED )
        {
            if ((HWND)lp == browse_button)
            {
                SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
                OnFileBrowse();
                return TRUE;
            } 
        }

        *pbProcessed = bWasProcessed;
        lResult = FALSE;
        return lResult;

    default:
        // the message was not processed
        // indicate if the base class handled it
        *pbProcessed = bWasProcessed;
        lResult = FALSE;
        return lResult;
    }

    return lResult;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void SubsFileDialog::OnFileBrowse()
{
    OPENFILENAME ofn;
    TCHAR DateiName[80+1] = _T("\0");
    static TCHAR szFilter[] = _T("All (*.*)\0*.*\0");

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof (OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.hInstance = hInst;
    ofn.lpstrFilter = szFilter;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = (LPTSTR) DateiName;
    ofn.nMaxFile = 80;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 40;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = _T("Open File");
    ofn.Flags = NULL;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;
    if( GetOpenFileName((LPOPENFILENAME) &ofn) )
    {
        SetWindowText( file_combo, ofn.lpstrFile );
        ComboBox_AddString( file_combo, ofn.lpstrFile );
        if( ComboBox_GetCount( file_combo ) > 10 )
            ComboBox_DeleteString( file_combo, 0 );
    }
}
