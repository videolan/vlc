/*****************************************************************************
 * subtitles.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2001 the VideoLAN team
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
SubsFileDialog::SubsFileDialog( intf_thread_t *p_intf, CBaseWindow *p_parent,
                                HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT SubsFileDialog::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;
    SHMENUBARINFO mbi;
    INITCOMMONCONTROLSEX ic;
    RECT rcClient;

    char *psz_subsfile;
    module_config_t *p_item;
    float f_fps;
    int i_delay;

    TCHAR psz_text[256];

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
        mbi.dwFlags    = SHCMBF_EMPTYBAR;
        mbi.hInstRes   = hInst;

        if (!SHCreateMenuBar(&mbi))
        {
            MessageBox(hwnd, _T("SHCreateMenuBar Failed"), _T("Error"), MB_OK);
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

        file_combo = CreateWindow( _T("COMBOBOX"), _FROMMB(psz_subsfile),
            WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_SORT | WS_VSCROLL,
            10, 10 + 15 + 10 - 3, rcClient.right - 2*10, 5*15 + 6,
            hwnd, NULL, hInst, NULL );

        if( psz_subsfile ) free( psz_subsfile );

        browse_button = CreateWindow( _T("BUTTON"), _T("Browse..."),
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        10, 10 + 2*(15 + 10) - 3, 80, 15 + 6,
                        hwnd, NULL, hInst, NULL);

        /* Subtitles encoding */
        encoding_combo = NULL;
        p_item = config_FindConfig( VLC_OBJECT(p_intf), "subsdec-encoding" );
        if( p_item )
        {
            enc_box = CreateWindow( _T("STATIC"), _T("Subtitles encoding"),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        5, 10 + 3*(15 + 10), rcClient.right - 2*5, 15,
                        hwnd, NULL, hInst, NULL );

            enc_label = CreateWindow( _T("STATIC"), _FROMMB(p_item->psz_text),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 10 + 4*(15 + 10), rcClient.right - 2*10, 15,
                hwnd, NULL, hInst, NULL );

            encoding_combo = CreateWindow( _T("COMBOBOX"),
                _FROMMB(p_item->psz_value),
                WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST |
                LBS_SORT  | WS_VSCROLL,
                rcClient.right - 150 - 10, 10 + 5*(15 + 10) - 3, 150, 5*15 + 6,
                hwnd, NULL, hInst, NULL );

            /* build a list of available options */
            for( int i_index = 0; p_item->ppsz_list &&
                   p_item->ppsz_list[i_index]; i_index++ )
            {
                ComboBox_AddString( encoding_combo,
                                    _FROMMB(p_item->ppsz_list[i_index]) );

                if( p_item->psz_value &&
                    !strcmp( p_item->psz_value, p_item->ppsz_list[i_index] ) )
                    ComboBox_SetCurSel( encoding_combo, i_index );
            }

            if( p_item->psz_value )
            {
                ComboBox_SelectString( encoding_combo, 0,
                                       _FROMMB(p_item->psz_value) );

            }
        }

        /* Misc Subtitles options */
        misc_box = CreateWindow( _T("STATIC"), _T("Subtitles options"),
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 5, 10 + 6*(15 + 10), rcClient.right - 2*5, 15,
                                 hwnd, NULL, hInst, NULL );

        delay_label = CreateWindow( _T("STATIC"),
                                    _T("Delay subtitles (in 1/10s)"),
                                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    10, 10 + 7*(15 + 10), rcClient.right - 70 - 2*10, 15,
                                    hwnd, NULL, hInst, NULL );

        i_delay = config_GetInt( p_intf, "sub-delay" );
        _stprintf( psz_text, _T("%d"), i_delay );

        delay_edit = CreateWindow( _T("EDIT"), psz_text,
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
            rcClient.right - 70 - 10, 10 + 7*(15 + 10) - 3, 70, 15 + 6,
            hwnd, NULL, hInst, NULL );

        ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
        ic.dwICC = ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&ic);

        delay_spinctrl =
            CreateUpDownControl( WS_CHILD | WS_VISIBLE | WS_BORDER |
                UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
                0, 0, 0, 0, hwnd, 0, hInst,
                delay_edit, 650000, -650000, i_delay );

        fps_label = CreateWindow( _T("STATIC"), _T("Frames per second"),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        10, 10 + 8*(15 + 10), rcClient.right - 70 - 2*10, 15,
                        hwnd, NULL, hInst, NULL );

        f_fps = config_GetFloat( p_intf, "sub-fps" );
        _stprintf( psz_text, _T("%f"), f_fps );

        fps_edit = CreateWindow( _T("EDIT"), psz_text,
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
            rcClient.right - 70 - 10, 10 + 8*(15 + 10) - 3, 70, 15 + 6,
            hwnd, NULL, hInst, NULL);

        ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
        ic.dwICC = ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&ic);

        fps_spinctrl = CreateUpDownControl(
            WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_ALIGNRIGHT |
            UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
            0, 0, 0, 0, hwnd, 0, hInst, fps_edit, 16000, 0, (int)f_fps );

        break;

    case WM_CLOSE:
        EndDialog( hwnd, LOWORD( wp ) );
        break;

    case WM_SETFOCUS:
        SHFullScreen( hwnd, SHFS_SHOWSIPBUTTON );
        break;

    case WM_COMMAND:
        if ( LOWORD(wp) == IDOK )
        {
            subsfile_mrl.clear();

            string szFileCombo = "sub-file=";
            GetWindowText( file_combo, psz_text, 256 );
            szFileCombo += _TOMB(psz_text);
            subsfile_mrl.push_back( szFileCombo );

            if( GetWindowTextLength( encoding_combo ) != 0 )
            {
                string szEncoding = "subsdec-encoding=";
                GetWindowText( encoding_combo, psz_text, 256 );
                szEncoding += _TOMB(psz_text);
                subsfile_mrl.push_back( szEncoding );
            }

            string szDelay = "sub-delay=";
            Edit_GetText( delay_edit, psz_text, 256 );
            szDelay += _TOMB(psz_text);
            subsfile_mrl.push_back( szDelay );

            string szFps = "sub-fps=";
            Edit_GetText( fps_edit, psz_text, 256 );
            szFps += _TOMB(psz_text);
            subsfile_mrl.push_back( szFps );

            EndDialog( hwnd, LOWORD( wp ) );
            break;
        }
        if( HIWORD(wp) == BN_CLICKED )
        {
            if ((HWND)lp == browse_button)
            {
                SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
                OnFileBrowse();
            } 
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

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
static void OnOpenCB( intf_dialog_args_t *p_arg )
{
    SubsFileDialog *p_this = (SubsFileDialog *)p_arg->p_arg;

    if( p_arg->i_results && p_arg->psz_results[0] )
    {
        SetWindowText( p_this->file_combo, _FROMMB(p_arg->psz_results[0]) );
        ComboBox_AddString( p_this->file_combo,
                            _FROMMB(p_arg->psz_results[0]) );
        if( ComboBox_GetCount( p_this->file_combo ) > 10 )
            ComboBox_DeleteString( p_this->file_combo, 0 );
    }
}

void SubsFileDialog::OnFileBrowse()
{
    intf_dialog_args_t *p_arg =
        (intf_dialog_args_t *)malloc( sizeof(intf_dialog_args_t) );
    memset( p_arg, 0, sizeof(intf_dialog_args_t) );

    p_arg->psz_title = strdup( "Open file" );
    p_arg->psz_extensions = strdup( "All|*.*" );
    p_arg->p_arg = this;
    p_arg->pf_callback = OnOpenCB;

    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_GENERIC, 0, p_arg);
}
