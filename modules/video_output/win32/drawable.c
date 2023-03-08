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
static HWND *used = NULL;

static const struct vlc_window_operations ops = {
    .destroy = Close,
};

#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)

static const TCHAR *EMBED_HWND_CLASS = TEXT("VLC embedded HWND");

struct drawable_sys
{
    HWND embed_hwnd;
    RECT rect_parent;

    WNDPROC prev_proc;

    vlc_wasync_resize_compressor_t compressor;
};

static LRESULT CALLBACK WinVoutEventProc(HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
    HANDLE p_user_data = GetProp(hwnd, EMBED_HWND_CLASS);
    struct drawable_sys *sys = (struct drawable_sys *)p_user_data;
    LRESULT res = CallWindowProc(sys->prev_proc, hwnd, message, wParam, lParam);
    switch(message)
    {
        case WM_SIZE:
        {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            if (RECTWidth(sys->rect_parent)  != RECTWidth(clientRect) ||
                RECTHeight(sys->rect_parent) != RECTHeight(clientRect))
            {
                sys->rect_parent = clientRect;

                vlc_wasync_resize_compressor_reportSize(&sys->compressor,
                                                        RECTWidth(sys->rect_parent),
                                                        RECTHeight(sys->rect_parent));
            }
        }
        break;
    }
    return res;
}

static void RemoveDrawable(HWND val)
{
    size_t n = 0;

    /* Remove this drawable from the list of busy ones */
    vlc_mutex_lock (&serializer);
    assert (used != NULL);
    while (used[n] != val)
    {
        assert (used[n]);
        n++;
    }
    do
        used[n] = used[n + 1];
    while (used[++n] != 0);

    if (n == 1)
    {
        free (used);
        used = NULL;
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

    HWND *tab;
    size_t n = 0;

    vlc_mutex_lock (&serializer);
    if (used != NULL)
        for (/*n = 0*/; used[n]; n++)
            if (used[n] == val)
            {
                msg_Warn (wnd, "HWND 0x%p is busy", val);
                vlc_mutex_unlock (&serializer);
                return VLC_EGENERIC;
            }

    tab = realloc (used, sizeof (*used) * (n + 2));
    if (unlikely(tab == NULL)) {
        vlc_mutex_unlock (&serializer);
        return VLC_ENOMEM;
    }
    used = tab;
    used[n] = val;
    used[n + 1] = 0;

    vlc_mutex_unlock (&serializer);

    struct drawable_sys *sys = vlc_obj_calloc(VLC_OBJECT(wnd), 1, sizeof(*sys));
    if (unlikely(sys == NULL)) {
        RemoveDrawable(val);
        return VLC_ENOMEM;
    }

    sys->embed_hwnd = val;

    if (vlc_wasync_resize_compressor_init(&sys->compressor, wnd))
    {
        msg_Err(wnd, "Failed to init async resize compressor");
        RemoveDrawable(val);
        return VLC_EGENERIC;
    }

    GetClientRect(val, &sys->rect_parent);
    vlc_wasync_resize_compressor_reportSize(&sys->compressor,
                                            RECTWidth(sys->rect_parent),
                                            RECTHeight(sys->rect_parent));

    sys->prev_proc = (WNDPROC)(uintptr_t) GetWindowLongPtr(val, GWLP_WNDPROC);
    SetProp(val, EMBED_HWND_CLASS, sys);
    SetWindowLongPtr(val, GWLP_WNDPROC, (LONG_PTR) WinVoutEventProc);

    wnd->type = VLC_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = (void *)val;
    wnd->ops = &ops;
    wnd->sys = sys;
    return VLC_SUCCESS;
}

/**
 * Release the drawable.
 */
static void Close (vlc_window_t *wnd)
{
    struct drawable_sys *sys = wnd->sys;

    // do not use our callback anymore
    SetWindowLongPtr(sys->embed_hwnd, GWLP_WNDPROC, (LONG_PTR) sys->prev_proc);
    RemoveProp(sys->embed_hwnd, EMBED_HWND_CLASS);

    vlc_wasync_resize_compressor_destroy(&sys->compressor);

    RemoveDrawable(sys->embed_hwnd);
}
