/*****************************************************************************
 * skin_common.h: Private Skin interface description
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: skin_common.h,v 1.11 2003/06/01 16:39:49 asmax Exp $
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


#ifndef SKIN_COMMON_H
#define SKIN_COMMON_H

#define SLIDER_RANGE        1048576  // 1024*1024
#define DEFAULT_SKIN_FILE   "skins/default.vlt"


class Theme;
class OpenDialog;
class Messages;
class SoutDialog;
class PrefsDialog;
class FileInfo;
#ifndef BASIC_SKINS
class wxIcon;
#endif
#ifdef WIN32
class ExitTimer;
#endif

#ifdef X11_SKINS
#include <X11/Xlib.h>
#endif

//---------------------------------------------------------------------------
// intf_sys_t: description and status of skin interface
//---------------------------------------------------------------------------
struct intf_sys_t
{
    // Pointer to the theme main class
    Theme *p_theme;
    char *p_new_theme_file;

    // The input thread
    input_thread_t *p_input;

    // The playlist thread
    playlist_t *p_playlist;

    // Check if thread is closing
    int  i_close_status;
    bool b_all_win_closed;

    // Message bank subscription
    msg_subscription_t *p_sub;

    // Interface status
    int i_index;        // Set which file is being played
    int i_size;         // Size of playlist;

#ifndef BASIC_SKINS
    wxIcon *p_icon;

    // Dialogs
    OpenDialog  *OpenDlg;
    Messages    *MessagesDlg;
    SoutDialog  *SoutDlg;
    PrefsDialog *PrefsDlg;
    FileInfo    *InfoDlg;
#endif

#ifdef X11_SKINS
    Display *display;
    Window mainWin;    // Window which receives "broadcast" events
#endif

#ifdef WIN32
#ifndef BASIC_SKINS
    bool b_wx_die;
    ExitTimer *p_kludgy_timer;
#endif

    // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
    HINSTANCE h_msimg32_dll;
    BOOL (WINAPI *TransparentBlt)( HDC,int,int,int,int,HDC,int,
                                   int,int,int,UINT );
    // Idem for user32.dll and SetLayeredWindowAttributes()
    HINSTANCE h_user32_dll;
    BOOL (WINAPI *SetLayeredWindowAttributes)( HWND,COLORREF,BYTE,DWORD );
#endif

};

#endif


