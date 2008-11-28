/*****************************************************************************
 * messages.cpp : WinCE gui plugin for VLC
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

Messages::Messages( intf_thread_t *p_intf, CBaseWindow *p_parent,
                    HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    hListView = NULL;

    hWnd = CreateWindow( _T("VLC WinCE"), _T("Messages"),
                         WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX,
                         0, 0, /*CW_USEDEFAULT*/300, /*CW_USEDEFAULT*/300,
                         p_parent->GetHandle(), NULL, h_inst, (void *)this );
        // Suscribe to messages bank
    cb_data = new msg_cb_data_t;
    cb_data->self = this;
    sub = msg_Subscribe( p_intf->p_libvlc, sinkMessage, cb_data );
}

Messages::~Messages()
{
    msg_Unsubscribe(sub);
    delete cb_data;
}

/***********************************************************************
FUNCTION:
  WndProc

PURPOSE:
  Processes messages sent to the main window.
***********************************************************************/
LRESULT Messages::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;

    TCHAR psz_text[TEXTMAXBUF];
    OPENFILENAME ofn;
    int i_dummy;
    HANDLE fichier;

    switch( msg )
    {
    case WM_CREATE:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        hListView = CreateWindow( WC_LISTVIEW, NULL,
                                  WS_VISIBLE | WS_CHILD | LVS_REPORT |
                                  LVS_SHOWSELALWAYS | WS_VSCROLL | WS_HSCROLL |
                                  WS_BORDER | LVS_NOCOLUMNHEADER, 0, 0, 0, 0,
                                  hwnd, NULL, hInst, NULL );
        ListView_SetExtendedListViewStyle( hListView, LVS_EX_FULLROWSELECT );

        LVCOLUMN lv;
        lv.mask = LVCF_FMT;
        lv.fmt = LVCFMT_LEFT ;
        ListView_InsertColumn( hListView, 0, &lv );

        SetTimer( hwnd, 1, 500 /*milliseconds*/, NULL );
        break;

    case WM_WINDOWPOSCHANGED:
        {
            RECT rect;
            if( !GetClientRect( hwnd, &rect ) ) break;
            SetWindowPos( hListView, 0, 0, 0,
                          rect.right - rect.left, rect.bottom - rect.top, 0 );

            LVCOLUMN lv;
            lv.cx = rect.right - rect.left;
            lv.mask = LVCF_WIDTH;
            ListView_SetColumn( hListView, 0, &lv );
        }
        break;

    case WM_SETFOCUS:
        SHSipPreference( hwnd, SIP_DOWN );
        SHFullScreen( hwnd, SHFS_HIDESIPBUTTON );
        break;

    case WM_CLOSE:
        Show( FALSE );
        return TRUE;

    case WM_COMMAND:
        switch( LOWORD(wp) )
        {
        case IDOK:
            Show( FALSE );
            break;

        case IDCLEAR:
            ListView_DeleteAllItems( hListView );
            break;

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
            break;

        default:
            break;
        }

    default:
        break;
    }

    return DefWindowProc( hwnd, msg, wp, lp );
}

void Messages::sinkMessage (msg_cb_data_t *data, msg_item_t *item,
                                  unsigned overruns)
{
    Messages *self = data->self;

    self->sinkMessage (item, overruns);
}

void Messages::sinkMessage (msg_item_t *item, unsigned overruns)
{
    vlc_value_t val;
    var_Get( p_intf->p_libvlc, "verbose", &val );


    /* Append all messages to log window */
    string debug = item->psz_module;

    switch( item->i_type )
    {
        case VLC_MSG_INFO: debug += ": "; break;
        case VLC_MSG_ERR: debug += " error: "; break;
        case VLC_MSG_WARN: debug += " warning: "; break;
        default: debug += " debug: "; break;
    }

    /* Add message */
    debug += item->psz_msg;

    LVITEM lv;
    lv.mask = LVIF_TEXT;
    lv.pszText = TEXT("");
    lv.cchTextMax = 1;
    lv.iSubItem = 0;
    lv.iItem = ListView_GetItemCount( hListView );
    ListView_InsertItem( hListView, &lv );
    ListView_SetItemText( hListView, lv.iItem, 0,
                          (TCHAR *)_FROMMB(debug.c_str()) );
}

