/*****************************************************************************
 * messages.cpp : WinCE gui plugin for VLC
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

#ifndef NMAXFILE
#define NMAXFILE 512 // at least 256
#endif

#ifndef TEXTMAXBUF
#define TEXTMAXBUF 512 // at least 500
#endif

/*****************************************************************************
 * Constructor.
 *****************************************************************************/

Messages::Messages( intf_thread_t *_p_intf, HINSTANCE _hInst )
{
    /* Initializations */
    p_intf = _p_intf;
    hInst = _hInst;
    hListView = NULL;
    b_verbose = VLC_FALSE;
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.

***********************************************************************/
LRESULT Messages::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                           PBOOL pbProcessed  )
{
    SHINITDLGINFO shidi;

    TCHAR psz_text[TEXTMAXBUF];
    OPENFILENAME ofn;
    int i_dummy;
    HANDLE fichier;
    int nList=0;


    LRESULT lResult = CBaseWindow::WndProc( hwnd, msg, wp, lp, pbProcessed );
    BOOL bWasProcessed = *pbProcessed;
    *pbProcessed = TRUE;

    switch (msg)
    {
    case WM_INITDIALOG: 
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        RECT rect; 
        GetClientRect( hwnd, &rect );
        hListView = CreateWindow( WC_LISTVIEW, NULL,
                                  WS_VISIBLE | WS_CHILD | LVS_REPORT |
                                  LVS_SHOWSELALWAYS | WS_VSCROLL | WS_HSCROLL |
                                  WS_BORDER /*| LVS_NOCOLUMNHEADER */,
                                  rect.left + 20, rect.top + 50, 
                                  rect.right - rect.left - ( 2 * 20 ), 
                                  rect.bottom - rect.top - 50 - 20, 
                                  hwnd, NULL, hInst, NULL );            
        ListView_SetExtendedListViewStyle( hListView, LVS_EX_FULLROWSELECT );

        LVCOLUMN lv;
        lv.mask = LVCF_WIDTH | LVCF_FMT | LVCF_TEXT;
        lv.fmt = LVCFMT_LEFT ;
        GetClientRect( hwnd, &rect );
        lv.cx = rect.right - rect.left;
        lv.pszText = _T("Messages");
        lv.cchTextMax = 9;
        ListView_InsertColumn( hListView, 0, &lv);

        SetTimer( hwnd, 1, 500 /*milliseconds*/, NULL );

        SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
        return lResult;

    case WM_TIMER:
        UpdateLog();
        return lResult;

    case WM_COMMAND:
        switch( LOWORD(wp) )
        {
        case IDOK:
            EndDialog( hwnd, LOWORD( wp ) );
            return TRUE;

        case IDCLEAR:
            ListView_DeleteAllItems( hListView );
            return TRUE;

        case IDSAVEAS:  
            memset( &(ofn), 0, sizeof(ofn) );
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = _T("");
            ofn.nMaxFile = NMAXFILE;    
            ofn.lpstrFilter = _T("Text (*.txt)\0*.txt\0");
            ofn.lpstrTitle = _T("Save File As");
            ofn.Flags = OFN_HIDEREADONLY; 
            ofn.lpstrDefExt = _T("txt");

            if( GetSaveFileName( (LPOPENFILENAME)&ofn ) )
            {
                fichier = CreateFile( ofn.lpstrFile, GENERIC_WRITE,
                                      FILE_SHARE_READ|FILE_SHARE_WRITE,
                                      NULL, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, NULL );

                if( fichier != INVALID_HANDLE_VALUE )
                {
                    int n;

                    //SetFilePointer( fichier, 0, NULL, FILE_END );
                    for( n = 0; n < ListView_GetItemCount( hListView ); n++ )
                    {
                        ListView_GetItemText( hListView, n, 0, psz_text,
                                              TEXTMAXBUF );
                        string text_out = (string)_TOMB(psz_text) + "\n";
                        WriteFile( fichier, text_out.c_str(), text_out.size(),
                                   (LPDWORD)&i_dummy, NULL );
                    }
                    FlushFileBuffers( fichier );
                    CloseHandle(fichier);
                }
            }

            SHFullScreen( GetForegroundWindow(), SHFS_HIDESIPBUTTON );
            return TRUE;

        default:
            *pbProcessed = bWasProcessed;
            lResult = FALSE;
            return lResult;
        }

    default:
         // the message was not processed
         // indicate if the base class handled it
         *pbProcessed = bWasProcessed;
         lResult = FALSE;
         return lResult;
    }

    return lResult;
}

void Messages::UpdateLog()
{
    msg_subscription_t *p_sub = p_intf->p_sys->p_sub;
    string debug;
    int i_start, i_stop;

    vlc_mutex_lock( p_sub->p_lock );
    i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        for( i_start = p_sub->i_start; i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            if( !b_verbose && VLC_MSG_ERR != p_sub->p_msg[i_start].i_type )
                continue;

            /* Append all messages to log window */
            debug = p_sub->p_msg[i_start].psz_module;
        
            switch( p_sub->p_msg[i_start].i_type )
            {
            case VLC_MSG_INFO:
                debug += ": ";
                break;
            case VLC_MSG_ERR:
                debug += " error: ";
                break;
            case VLC_MSG_WARN:
                debug += " warning: ";
                break;
            case VLC_MSG_DBG:
            default:
                debug += " debug: ";
                break;
            }

            /* Add message */
            debug += p_sub->p_msg[i_start].psz_msg;

            LVITEM lv;
            lv.mask = LVIF_TEXT;
            lv.pszText = TEXT("");
            lv.cchTextMax = 1;
            lv.iSubItem = 0;
            lv.iItem = ListView_GetItemCount( hListView );
            ListView_InsertItem( hListView, &lv );
            ListView_SetItemText( hListView, lv.iItem, 0,
                                  _FROMMB(debug.c_str()) );
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}
