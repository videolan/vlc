/**
 * @file drawable.c
 * @brief Legacy monolithic LibVLC video window provider
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdarg.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_window.h>
#include "../wasync_resize_compressor.h"

#define HWND_TEXT N_("Window handle (HWND)")
#define HWND_LONGTEXT N_( \
    "Video will be embedded in this pre-existing window. " \
    "If zero, a new window will be created.")

static int Open(vlc_window_t *);
static void Close(vlc_window_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded window video"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window", 70)
    set_callback(Open)
    add_shortcut ("embed-hwnd")

    add_integer ("drawable-hwnd", 0, HWND_TEXT, HWND_LONGTEXT)
        change_volatile ()
vlc_module_end ()

/* Keep a list of busy drawables, so we don't overlap videos if there are
 * more than one video track in the stream. */
static vlc_mutex_t serializer = VLC_STATIC_MUTEX;
static HHOOK hook = NULL;
static struct drawable_sys *used = NULL;

static const struct vlc_window_operations ops = {
    .destroy = Close,
};

#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)

struct drawable_sys
{
    HWND embed_hwnd;
    RECT rect_parent;

    vlc_wasync_resize_compressor_t compressor;
};

static LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    MSG *pMsg = (void*)(intptr_t)lParam;
    vlc_mutex_lock (&serializer);
    for (struct drawable_sys *sys = used; sys->embed_hwnd != 0; sys++)
    {
        if (pMsg->hwnd == sys->embed_hwnd)
        {
            RECT clientRect;
            GetClientRect(pMsg->hwnd, &clientRect);
            if (RECTWidth(sys->rect_parent)  != RECTWidth(clientRect) ||
                RECTHeight(sys->rect_parent) != RECTHeight(clientRect))
            {
                sys->rect_parent = clientRect;
                vlc_wasync_resize_compressor_reportSize(&sys->compressor, RECTWidth(sys->rect_parent),
                                                                          RECTHeight(sys->rect_parent));
            }
            break;
        }
    }
    vlc_mutex_unlock (&serializer);
    return 0;
}

static void RemoveDrawable(HWND val)
{
    size_t n = 0;

    /* Remove this drawable from the list of busy ones */
    vlc_mutex_lock (&serializer);
    assert (used != NULL);
    while (used[n].embed_hwnd != val)
    {
        assert (used[n].embed_hwnd);
        n++;
    }

    vlc_wasync_resize_compressor_destroy(&used[n].compressor);

    do
        used[n] = used[n + 1];
    while (used[++n].embed_hwnd != 0);

    if (n == 1)
    {
        free (used);
        used = NULL;

        UnhookWindowsHookEx(hook);
        hook = NULL;
    }
    vlc_mutex_unlock (&serializer);
}

/**
 * Find the drawable set by libvlc application.
 */
static int Open(vlc_window_t *wnd)
{
    uintptr_t drawable = var_InheritInteger (wnd, "drawable-hwnd");
    if (drawable == 0)
        return VLC_EGENERIC;
    HWND val = (HWND)drawable;

    size_t n = 0;

    vlc_mutex_lock (&serializer);
    bool first_hwnd = used == NULL;
    if (used != NULL)
        for (/*n = 0*/; used[n].embed_hwnd; n++)
            if (used[n].embed_hwnd == val)
            {
                msg_Warn (wnd, "HWND 0x%p is busy", val);
                vlc_mutex_unlock (&serializer);
                return VLC_EGENERIC;
            }

    if (hook == NULL)
    {
        assert(first_hwnd);
        /* Get this DLL instance, it can't be a global module */
        HMODULE hInstance = GetModuleHandle(TEXT("lib" MODULE_STRING "_plugin.dll"));
        hook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, hInstance, 0);
        if (hook == NULL)
        {
            vlc_mutex_unlock (&serializer);

            char msg[256];
            int i_error = GetLastError();
            FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, i_error, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                            msg, ARRAY_SIZE(msg), NULL );
            msg_Err(wnd, "Failed to hook to the parent window: %s", msg);
            return VLC_EGENERIC;
        }
    }

    struct drawable_sys *tab = realloc (used, sizeof (*used) * (n + 2));
    if (unlikely(tab == NULL)) {
        vlc_mutex_unlock (&serializer);
        if (first_hwnd)
        {
            UnhookWindowsHookEx(hook);
            hook = NULL;
        }
        return VLC_ENOMEM;
    }
    used = tab;
    used[n].embed_hwnd = val;
    used[n + 1].embed_hwnd = 0;

    vlc_mutex_unlock (&serializer);


    struct drawable_sys *sys = &used[n];
    GetClientRect(val, &sys->rect_parent);

    if (vlc_wasync_resize_compressor_init(&sys->compressor, wnd))
    {
        msg_Err(wnd, "Failed to init async resize compressor");
        goto error;
    }

    vlc_wasync_resize_compressor_reportSize(&sys->compressor, RECTWidth(sys->rect_parent),
                                                                 RECTHeight(sys->rect_parent));

    wnd->type = VLC_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = (void *)val;
    wnd->ops = &ops;
    wnd->sys = val;
    return VLC_SUCCESS;

error:
    Close(wnd);
    return VLC_EGENERIC;
}

/**
 * Release the drawable.
 */
static void Close (vlc_window_t *wnd)
{
    RemoveDrawable(wnd->sys);
}
