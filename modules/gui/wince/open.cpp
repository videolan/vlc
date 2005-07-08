/*****************************************************************************
 * open.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN (Centrale Réseaux) and its contributors
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

#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event = 1000,
    MRL_Event,

    FileBrowse_Event,
    FileName_Event,

    DiscType_Event,
    DiscDevice_Event,
    DiscTitle_Event,
    DiscChapter_Event,

    NetType_Event,
    NetRadio1_Event, NetRadio2_Event, NetRadio3_Event, NetRadio4_Event,
    NetPort1_Event, NetPort2_Event, NetPort3_Event,
    NetAddr1_Event, NetAddr2_Event, NetAddr3_Event, NetAddr4_Event,

    SubsFileEnable_Event,
    SubsFileSettings_Event,
};

/*****************************************************************************
 * AutoBuiltPanel.
 *****************************************************************************/

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
OpenDialog::OpenDialog( intf_thread_t *p_intf, CBaseWindow *p_parent,
                        HINSTANCE h_inst, int _i_access, int _i_arg )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
    i_access = _i_access;
    i_open_arg = _i_arg;

    for( int i = 0; i < 4; i++ )
    {
        net_radios[i] = 0;
        net_label[i] = 0;
        net_port_label[i] = 0;
        net_ports[i] = 0;
        hUpdown[i] = 0;
        i_net_ports[i] = 0;
        net_addrs_label[i] = 0;
        net_addrs[i] = 0;
    }

    CreateWindow( _T("VLC WinCE"), _T("Messages"),
                  WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX,
                  0, 0, /*CW_USEDEFAULT*/300, /*CW_USEDEFAULT*/300,
                  p_parent->GetHandle(), NULL, h_inst, (void *)this );
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.
  
***********************************************************************/
LRESULT OpenDialog::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;
    INITCOMMONCONTROLSEX  iccex;  // INITCOMMONCONTROLSEX structure    
    RECT rcClient;
    TC_ITEM tcItem;

    switch( msg )
    {
    case WM_CREATE:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        // Get the client area rect to put the panels in
        GetClientRect( hwnd, &rcClient );

        /* Create MRL combobox */
        mrl_box = CreateWindow( _T("STATIC"),
                                _FROMMB(_("Media Resource Locator (MRL)")),
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                5, 10, rcClient.right, 15, hwnd, 0, hInst, 0 );

        mrl_label = CreateWindow( _T("STATIC"), _FROMMB(_("Open:")),
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  5, 10 + 15 + 10, 40, 15, hwnd, 0, hInst, 0 );

        mrl_combo = CreateWindow( _T("COMBOBOX"), _T(""),
                                  WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL |
                                  CBS_SORT | WS_VSCROLL, 45, 10 + 15 + 10 - 3,
                                  rcClient.right - 50 - 5, 5*15 + 6, hwnd,
                                  0, hInst, 0 );

        // No tooltips for ComboBox
        label = CreateWindow( _T("STATIC"),
                              _FROMMB(_("Alternatively, you can build an MRL "
                                       "using one of the following predefined "
                                       "targets:" )),
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              5, 10 + 2*(15 + 10), rcClient.right - 2*5, 2*15,
                              hwnd, 0, hInst, 0 );

        /* Create notebook */
        iccex.dwSize = sizeof (INITCOMMONCONTROLSEX);
        iccex.dwSize = ICC_TAB_CLASSES;
        InitCommonControlsEx (&iccex);

        notebook = CreateWindowEx( 0, WC_TABCONTROL, NULL,
            WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
            5, 10 + 4*15 + 2*10, rcClient.right - 2*5,
            rcClient.bottom - MENU_HEIGHT - 15 - 10 - 10 - (10 + 4*15 + 2*10),
            hwnd, NULL, hInst, NULL );

        tcItem.mask = TCIF_TEXT;
        tcItem.pszText = _T("File");
        TabCtrl_InsertItem( notebook, 0, &tcItem );
        tcItem.pszText = _T("Network");
        TabCtrl_InsertItem( notebook, 1, &tcItem );

        switch( i_access )
        {
        case FILE_ACCESS:
            TabCtrl_SetCurSel( notebook, 0 );
            break;
        case NET_ACCESS:
            TabCtrl_SetCurSel( notebook, 1 );
            break;
        }

        FilePanel( hwnd );
        NetPanel( hwnd );

        OnPageChange();
        break;

    case WM_CLOSE:
        Show( FALSE );
        return TRUE;

    case WM_SETFOCUS:
        SHFullScreen( hwnd, SHFS_SHOWSIPBUTTON );
        SHSipPreference( hwnd, SIP_DOWN ); 
        break;

    case WM_COMMAND:
        if( LOWORD(wp) == IDOK )
        {
            OnOk();
            Show( FALSE );
            break;
        }
        if( HIWORD(wp) == BN_CLICKED )
        {
            if( (HWND)lp == net_radios[0] )
            {
                OnNetTypeChange( NetRadio1_Event );
            } else if( (HWND)lp == net_radios[1] )
            {
                OnNetTypeChange( NetRadio2_Event );
            } else if( (HWND)lp == net_radios[2] )
            {
                OnNetTypeChange( NetRadio3_Event );
            } else if( (HWND)lp == net_radios[3] )
            {
                OnNetTypeChange( NetRadio4_Event );
            } else if( (HWND)lp == subsfile_checkbox )
            {
                OnSubsFileEnable();
            } else if( (HWND)lp == subsfile_button )
            {
                OnSubsFileSettings( hwnd );
            } else if( (HWND)lp == browse_button )
            {
                OnFileBrowse();
            } 
            break;
        }
        if( HIWORD(wp) == EN_CHANGE )
        {
            if( (HWND)lp == net_addrs[1] )
            {
                OnNetPanelChange( NetAddr2_Event );
            } else if( (HWND)lp == net_addrs[2] )
            {
                OnNetPanelChange( NetAddr3_Event );
            } else if( (HWND)lp == net_addrs[3] )
            {
                OnNetPanelChange( NetAddr4_Event );
            } else if( (HWND)lp == net_ports[0] )
            {
                OnNetPanelChange( NetPort1_Event );
            } else if( (HWND)lp == net_ports[1] )
            {
                OnNetPanelChange( NetPort2_Event );
            }
        }
        if( HIWORD(wp) == CBN_EDITUPDATE )
        {
            if ((HWND)lp == file_combo)
            {
                OnFilePanelChange();
            }
        }
        break;

    case WM_NOTIFY:
        if( (((NMHDR *)lp)->code) == TCN_SELCHANGE ) OnPageChange();
        break;

    default:
        break;
    }

    return DefWindowProc( hwnd, msg, wp, lp );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void OpenDialog::FilePanel( HWND hwnd )
{
    RECT rc;
    GetWindowRect( notebook, &rc );

    /* Create browse file line */
    file_combo = CreateWindow( _T("COMBOBOX"), _T(""),
        WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | CBS_SORT | WS_VSCROLL,
        rc.left + 10, rc.top + 10 - 3, rc.right - 10 - (rc.left + 10),
        5*15 + 6, hwnd, NULL, hInst, NULL );

    browse_button = CreateWindow( _T("BUTTON"), _T("Browse..."),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        rc.left + 10, rc.top + 10 + 15 + 10 - 3, 80, 15 + 6,
        hwnd, NULL, hInst, NULL );

    /* Create Subtitles File checkox */
    subsfile_checkbox = CreateWindow( _T("BUTTON"), _T("Subtitle options"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        rc.left + 10, rc.top + 10 + 2*(15 + 10), 15, 15,
        hwnd, NULL, hInst, NULL );
    SendMessage( subsfile_checkbox, BM_SETCHECK, BST_UNCHECKED, 0 );

    subsfile_label = CreateWindow( _T("STATIC"), _T("Subtitle options"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 10 + 15 + 10, rc.top + 10 + 2*(15 + 10), 100, 15,
                hwnd, NULL, hInst, NULL);

    subsfile_button = CreateWindow( _T("BUTTON"), _T("Settings..."),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                rc.right - 80 - 10, rc.top + 10 + 2*(15 + 10) - 3, 80, 15 + 6,
                hwnd, NULL, hInst, NULL );

    char *psz_subsfile = config_GetPsz( p_intf, "sub-file" );
    if( psz_subsfile && *psz_subsfile )
    {
        SendMessage( subsfile_checkbox, BM_SETCHECK, BST_CHECKED, 0 );
        EnableWindow( subsfile_button, TRUE );
        string sz_subsfile = "sub-file=";
        sz_subsfile += psz_subsfile;
        subsfile_mrl.push_back( sz_subsfile );
    }
    if( psz_subsfile ) free( psz_subsfile );
}

void OpenDialog::NetPanel( HWND hwnd )
{
    INITCOMMONCONTROLSEX ic;
    TCHAR psz_text[256];

    struct net_type
    {
        TCHAR *psz_text;
        int length;
    };

    static struct net_type net_type_array[] =
    {
        { _T("UDP/RTP"), 82 },
        { _T("UDP/RTP Multicast"), 140 },
        { _T("HTTP/FTP/MMS"), 90 },
        { _T("RTSP"), 30 }
    };

    RECT rc;
    GetWindowRect( notebook, &rc);

    /* UDP/RTP row */
    net_radios[0] = CreateWindow( _T("BUTTON"), net_type_array[0].psz_text,
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                rc.left + 5, rc.top + 10, 15, 15, hwnd, NULL, hInst, NULL );

    net_label[0] = CreateWindow( _T("STATIC"), net_type_array[0].psz_text,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 5 + 15 + 5, rc.top + 10, net_type_array[0].length,
                15, hwnd, NULL, hInst, NULL );

    i_net_ports[0] = config_GetInt( p_intf, "server-port" );

    net_port_label[0] = CreateWindow( _T("STATIC"), _T("Port"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 5 , rc.top + 10 + 2*(15 + 10), 30, 15,
                hwnd, NULL, hInst, NULL );

    _stprintf( psz_text, _T("%d"), i_net_ports[0] );
    net_ports[0] = CreateWindow( _T("EDIT"), psz_text,
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
        rc.left + 5 + 30 + 5, rc.top + 10 + 2*(15 + 10) - 3,
        rc.right - 5 - (rc.left + 5 + 30 + 5), 15 + 6, hwnd, NULL, hInst, NULL );

    ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
    ic.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&ic);

    hUpdown[0] = CreateUpDownControl(
                WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_ALIGNRIGHT |
                UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
                0, 0, 0, 0, hwnd, 0, hInst,
                net_ports[0], 16000, 0, i_net_ports[0]);

    /* UDP/RTP Multicast row */
    net_radios[1] = CreateWindow( _T("BUTTON"), net_type_array[1].psz_text,
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                rc.left + 5, rc.top + 10 + 15 + 10, 15, 15,
                hwnd, NULL, hInst, NULL);

    net_label[1] = CreateWindow( _T("STATIC"), net_type_array[1].psz_text,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 5 + 15 + 5, rc.top + 10 + 15 + 10,
                net_type_array[1].length, 15, hwnd, NULL, hInst, NULL );

    net_addrs_label[1] = CreateWindow( _T("STATIC"), _T("Address"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 5 , rc.top + 10 + 2*(15 + 10), 50, 15,
                hwnd, NULL, hInst, NULL);

    net_addrs[1] = CreateWindow( _T("EDIT"), _T(""),
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
                rc.left + 5 + 50 + 5, rc.top + 10 + 2*(15 + 10) - 3,
                rc.right - 5 - (rc.left + 5 + 50 + 5), 15 + 6,
                hwnd, NULL, hInst, NULL);

    net_port_label[1] = CreateWindow( _T("STATIC"), _T("Port"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                rc.left + 5 , rc.top + 10 + 3*(15 + 10), 30, 15,
                hwnd, NULL, hInst, NULL);

    i_net_ports[1] = i_net_ports[0];

    _stprintf( psz_text, _T("%d"), i_net_ports[1] );
    net_ports[1] = CreateWindow( _T("EDIT"), psz_text,
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
                rc.left + 5 + 30 + 5, rc.top + 10 + 3*(15 + 10) - 3,
                rc.right - 5 -(rc.left + 5 + 30 + 5), 15 + 6,
                hwnd, NULL, hInst, NULL );

    ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
    ic.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&ic);

    hUpdown[1] = CreateUpDownControl( WS_CHILD | WS_VISIBLE | WS_BORDER |
        UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
        0, 0, 0, 0, hwnd, 0, hInst,
        net_ports[1], 16000, 0, i_net_ports[1] );

    /* HTTP and RTSP rows */
    net_radios[2] = CreateWindow( _T("BUTTON"), net_type_array[2].psz_text,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        rc.left + 5 + 15 + 5 + net_type_array[0].length + 5,
        rc.top + 10, 15, 15, hwnd, NULL, hInst, NULL );
        
    net_label[2] = CreateWindow( _T("STATIC"), net_type_array[2].psz_text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rc.left + 5 + 15 + 5 + net_type_array[0].length + 5 + 15 + 5,
        rc.top + 10, net_type_array[2].length, 15,
        hwnd, NULL, hInst, NULL );

    net_addrs_label[2] = CreateWindow( _T("STATIC"), _T("URL"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rc.left + 5 , rc.top + 10 + 2*(15 + 10), 30, 15,
        hwnd, NULL, hInst, NULL );

    net_addrs[2] = CreateWindow( _T("EDIT"), _T(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
        rc.left + 5 + 30 + 5, rc.top + 10 + 2*(15 + 10) - 3,
        rc.right - 5 - (rc.left + 5 + 30 + 5), 15 + 6,
        hwnd, NULL, hInst, NULL);

    net_radios[3] = CreateWindow( _T("BUTTON"), net_type_array[3].psz_text,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        rc.left + 5 + 15 + 5 + net_type_array[1].length + 5,
        rc.top + 10 + 15 + 10, 15, 15, hwnd, NULL, hInst, NULL );

    net_label[3] = CreateWindow( _T("STATIC"), net_type_array[3].psz_text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rc.left + 5 + 15 + 5 + net_type_array[1].length + 5 + 15 + 5,
        rc.top + 10 + 15 + 10, net_type_array[3].length, 15,
        hwnd, NULL, hInst, NULL );

    net_addrs_label[3] = CreateWindow( _T("STATIC"), _T("URL"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rc.left + 5 , rc.top + 10 + 2*(15 + 10), 30, 15,
        hwnd, NULL, hInst, NULL );

    net_addrs[3] = CreateWindow( _T("EDIT"), _T("rtsp://"),
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT | ES_AUTOHSCROLL,
        rc.left + 5 + 30 + 5, rc.top + 10 + 2*(15 + 10) - 3,
        rc.right - 5 - (rc.left + 5 + 30 + 5), 15 + 6,
        hwnd, NULL, hInst, NULL );

    SendMessage( net_radios[0], BM_SETCHECK, BST_CHECKED, 0 );
}

void OpenDialog::UpdateMRL()
{
    UpdateMRL( i_access );
}

void OpenDialog::UpdateMRL( int i_access_method )
{
    string demux, mrltemp;
    TCHAR psz_text[2048];
    char psz_tmp[256];

    i_access = i_access_method;

    switch( i_access_method )
    {
    case FILE_ACCESS:
        GetWindowText( file_combo, psz_text, 2048 );
        mrltemp = _TOMB(psz_text);
        break;
    case NET_ACCESS:
        switch( i_net_type )
        {
        case 0:
            mrltemp = "udp" + demux + "://";
            if( i_net_ports[0] !=
                config_GetInt( p_intf, "server-port" ) )
            {
                sprintf( psz_tmp, "@:%d", i_net_ports[0] );
                mrltemp += psz_tmp;
            }
            break;

        case 1:
            mrltemp = "udp" + demux + "://@";
            Edit_GetText( net_addrs[1], psz_text, 2048 );
            mrltemp += _TOMB(psz_text);
            if( i_net_ports[1] != config_GetInt( p_intf, "server-port" ) )
            {
                sprintf( psz_tmp, ":%d", i_net_ports[1] );
                mrltemp += psz_tmp;
            }
            break;

        case 2:
            /* http access */
            Edit_GetText( net_addrs[2], psz_text, 2048 );
            if( !strstr( _TOMB(psz_text), "http://" ) )
            {
                mrltemp = "http" + demux + "://";
            }
            mrltemp += _TOMB(psz_text);
            break;

        case 3:
            /* RTSP access */
            Edit_GetText( net_addrs[3], psz_text, 2048 );
            if( !strstr( _TOMB(psz_text), "rtsp://" ) )
            {
                mrltemp = "rtsp" + demux + "://";
            }
            mrltemp += _TOMB(psz_text);
            break;
        }
        break;
    default:
        break;
    }

    SetWindowText( mrl_combo, _FROMMB(mrltemp.c_str()) );
}

void OpenDialog::OnPageChange()
{
    if( TabCtrl_GetCurSel( notebook ) == 0 )
    {
        for( int i=0; i<4; i++ )
        {
            SetWindowPos( net_radios[i], HWND_BOTTOM, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE );
            SetWindowPos( net_label[i], HWND_BOTTOM, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE );
        }
        DisableNETCtrl();

        SetWindowPos( file_combo, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( browse_button, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_checkbox, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_label, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_button, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );

        i_access = FILE_ACCESS;
    }
    else if ( TabCtrl_GetCurSel( notebook ) == 1 )
    {
        SetWindowPos( file_combo, HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( browse_button, HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_checkbox, HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_label, HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( subsfile_button, HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );

        for( int i=0; i<4; i++ )
        {
            SetWindowPos( net_radios[i], HWND_TOP, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE );
            SendMessage( net_radios[i], BM_SETCHECK, BST_UNCHECKED, 0 );
            SetWindowPos( net_label[i], HWND_TOP, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE );
        }
        SetWindowPos( net_port_label[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_ports[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( hUpdown[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );

        SendMessage( net_radios[0], BM_SETCHECK, BST_CHECKED, 0 );

        i_access = NET_ACCESS;
    }

    UpdateMRL();
}

void OpenDialog::OnOk()
{
    TCHAR psz_text[2048];

    GetWindowText( mrl_combo, psz_text, 2048 );

    int i_args;
    char **pp_args = vlc_parse_cmdline( _TOMB(psz_text), &i_args );

    ComboBox_AddString( mrl_combo, psz_text );
    if( ComboBox_GetCount( mrl_combo ) > 10 ) 
        ComboBox_DeleteString( mrl_combo, 0 );
    ComboBox_SetCurSel( mrl_combo, ComboBox_GetCount( mrl_combo ) - 1 );

    /* Update the playlist */
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    for( int i = 0; i < i_args; i++ )
    {
        vlc_bool_t b_start = !i && i_open_arg;
        playlist_item_t *p_item =
            playlist_ItemNew( p_intf, pp_args[i], pp_args[i] );

        /* Insert options */
        while( i + 1 < i_args && pp_args[i + 1][0] == ':' )
        {
            playlist_ItemAddOption( p_item, pp_args[i + 1] );
            i++;
        }

        /* Get the options from the subtitles dialog */
        if( (SendMessage( subsfile_checkbox, BM_GETCHECK, 0, 0 ) & BST_CHECKED)
            && subsfile_mrl.size() )
        {
            for( int j = 0; j < (int)subsfile_mrl.size(); j++ )
            {
                playlist_ItemAddOption( p_item, subsfile_mrl[j].c_str() );
            }
        }

        playlist_AddItem( p_playlist, p_item,
                          PLAYLIST_APPEND, PLAYLIST_END );

        if( b_start )
        {
            playlist_Control( p_playlist, PLAYLIST_ITEMPLAY , p_item );
        }
    }

    //TogglePlayButton( PLAYING_S );

    while( i_args-- )
    {
        free( pp_args[i_args] );
        if( !i_args ) free( pp_args );
    }
    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * File panel event methods.
 *****************************************************************************/
void OpenDialog::OnFilePanelChange()
{
    UpdateMRL( FILE_ACCESS );
}

static void OnOpenCB( intf_dialog_args_t *p_arg )
{
    OpenDialog *p_this = (OpenDialog *)p_arg->p_arg;
    char psz_tmp[PATH_MAX+2] = "\0";

    if( p_arg->i_results && p_arg->psz_results[0] )
    {
        if( strchr( p_arg->psz_results[0], ' ' ) )
        {
            strcat( psz_tmp, "\"" );
            strcat( psz_tmp, p_arg->psz_results[0] );
            strcat( psz_tmp, "\"" );
        }
        else strcat( psz_tmp, p_arg->psz_results[0] );

        SetWindowText( p_this->file_combo, _FROMMB(psz_tmp) );
        ComboBox_AddString( p_this->file_combo, _FROMMB(psz_tmp) );
        if( ComboBox_GetCount( p_this->file_combo ) > 10 ) 
            ComboBox_DeleteString( p_this->file_combo, 0 );

        p_this->UpdateMRL( FILE_ACCESS );
    }
}

void OpenDialog::OnFileBrowse()
{
    intf_dialog_args_t *p_arg =
        (intf_dialog_args_t *)malloc( sizeof(intf_dialog_args_t) );
    memset( p_arg, 0, sizeof(intf_dialog_args_t) );

    p_arg->psz_title = strdup( "Open file" );
    p_arg->psz_extensions = strdup( "All (*.*)|*.*" );
    p_arg->p_arg = this;
    p_arg->pf_callback = OnOpenCB;

    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_GENERIC, 0, p_arg);
}

/*****************************************************************************
 * Net panel event methods.
 *****************************************************************************/
void OpenDialog::OnNetPanelChange( int event )
{
    TCHAR psz_text[2048];
    int port;

    if( event >= NetPort1_Event && event <= NetPort2_Event )
    {
        Edit_GetText( net_ports[event - NetPort1_Event], psz_text, 2048 );
        _stscanf( psz_text, _T("%d"), &port );
        i_net_ports[event - NetPort1_Event] = port;
    }

    UpdateMRL( NET_ACCESS );
}

void OpenDialog::OnNetTypeChange( int event )
{
    DisableNETCtrl();

    i_net_type = event - NetRadio1_Event;

    if( event == NetRadio1_Event )
    {
        SetWindowPos( net_port_label[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_ports[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( hUpdown[0], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
    } 
    else if( event == NetRadio2_Event )
    {
        SetWindowPos( net_addrs_label[1], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_addrs[1], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_port_label[1], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_ports[1], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( hUpdown[1], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
    } 
    else if( event == NetRadio3_Event )
    {
        SetWindowPos( net_addrs_label[2], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_addrs[2], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
    } 
    else if( event == NetRadio4_Event )
    {
        SetWindowPos( net_addrs_label[3], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_addrs[3], HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
    }
        
    UpdateMRL( NET_ACCESS );
}

void OpenDialog::DisableNETCtrl()
{
    for( int i=0; i<4; i++ )
    {
        SetWindowPos( net_port_label[i], HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_ports[i], HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( hUpdown[i], HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_addrs_label[i], HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( net_addrs[i], HWND_BOTTOM, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE );
    }

    UpdateMRL( FILE_ACCESS );
}

/*****************************************************************************
 * Subtitles file event methods.
 *****************************************************************************/
void OpenDialog::OnSubsFileEnable()
{
    EnableWindow( subsfile_button, ( SendMessage( subsfile_checkbox,
                  BM_GETCHECK, 0, 0 ) & BST_CHECKED ) ? TRUE : FALSE );
}

void OpenDialog::OnSubsFileSettings( HWND hwnd )
{

    /* Show/hide the open dialog */
    SubsFileDialog *subsfile_dialog = new SubsFileDialog( p_intf, this, hInst);
    CreateDialogBox(  hwnd, subsfile_dialog );

    subsfile_mrl.clear();

    for( int i = 0; i < (int)subsfile_dialog->subsfile_mrl.size(); i++ )
        subsfile_mrl.push_back( subsfile_dialog->subsfile_mrl[i] );

    delete subsfile_dialog;
}
