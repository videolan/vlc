/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <windows.h>
#include <winuser.h>
#include <assert.h>
#include "csdmenu_win32.h"

typedef struct
{
    bool isRtl;
    notifyMenuVisibleCallback notifyMenuVisible;
    void* userData;
} qt_csd_menu_priv_t;

static bool Popup(struct qt_csd_menu_t* menu, qt_csd_menu_event* event)
{
    qt_csd_menu_priv_t* sys = (qt_csd_menu_priv_t*)menu->p_sys;

    assert(event->platform == QT_CSD_PLATFORM_WINDOWS);

    HWND hwnd = event->data.win32.hwnd;
    HMENU hmenu = GetSystemMenu(hwnd, FALSE);
    if (!hmenu)
        return false;

    // Tweak the menu items according to the current window status.
    const bool maxOrFull = event->windowState & (QT_CSD_WINDOW_FULLSCREEN |  QT_CSD_WINDOW_MAXIMIZED);
    const bool fixedSize = event->windowState & QT_CSD_WINDOW_FIXED_SIZE;

    EnableMenuItem(hmenu, SC_MOVE, (MF_BYCOMMAND | (!maxOrFull ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hmenu, SC_SIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));

    EnableMenuItem(hmenu, SC_RESTORE, (MF_BYCOMMAND | ((maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hmenu, SC_MINIMIZE, (MF_BYCOMMAND | MFS_ENABLED));
    EnableMenuItem(hmenu, SC_MAXIMIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize) ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hmenu, SC_CLOSE, (MF_BYCOMMAND | MFS_ENABLED));


    const unsigned alignment = sys->isRtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN;

    // show menu

    if (sys->notifyMenuVisible)
        sys->notifyMenuVisible(sys->userData, true);

    const int action = TrackPopupMenu(hmenu, (TPM_RETURNCMD | alignment)
                                      , event->x, event->y
                                      , 0, hwnd, NULL);

    // unlike native system menu which sends WM_SYSCOMMAND, TrackPopupMenu sends WM_COMMAND
    // imitate native system menu by sending the action manually as WM_SYSCOMMAND
    PostMessageW(hwnd, WM_SYSCOMMAND, action, 0);

    if (sys->notifyMenuVisible)
        sys->notifyMenuVisible(sys->userData, false);

    return true;
}

int QtWin32CSDMenuOpen(qt_csd_menu_t* p_this, qt_csd_menu_info* info)
{
    if (info->platform != QT_CSD_PLATFORM_WINDOWS)
        return VLC_EGENERIC;

    qt_csd_menu_priv_t* sys = vlc_obj_malloc(p_this, sizeof(qt_csd_menu_priv_t));
    if (!sys)
        return VLC_ENOMEM;

    sys->isRtl = info->isRtl;

    sys->notifyMenuVisible = info->notifyMenuVisible;
    sys->userData = info->userData;

    p_this->p_sys = sys;
    p_this->popup = Popup;
    return VLC_SUCCESS;
}
