/*****************************************************************************
 * skin_common.h: Private Skin interface description
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: skin_common.h,v 1.14 2003/06/04 16:03:33 gbazin Exp $
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
class Dialogs;
class wxIcon;

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

    // Interface dialogs
    Dialogs *p_dialogs;

#ifndef BASIC_SKINS
    wxIcon      *p_icon;
#endif

#ifdef X11_SKINS
    Display *display;
    Window mainWin;    // Window which receives "broadcast" events
    vlc_mutex_t xlock;
#endif

#ifdef WIN32
    // Interface thread id used to post broadcast messages
    DWORD dwThreadId;

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
