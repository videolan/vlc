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
    vlc_thread_t thread;
    vlc_sem_t hwnd_set;

    vlc_window_t *wnd;
    HWND hWnd;
    HWND embed_hwnd;
    RECT rect_parent;

    vlc_wasync_resize_compressor_t compressor;
};

static LRESULT CALLBACK WinVoutEventProc(HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
    if( message == WM_CREATE /*WM_NCCREATE*/ )
    {
        /* Store our internal structure for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
    if( unlikely(p_user_data == 0) )
        return DefWindowProc(hwnd, message, wParam, lParam);
    struct drawable_sys *sys = (struct drawable_sys *)p_user_data;

    vlc_window_t *wnd = sys->wnd;

    RECT clientRect;
    GetClientRect(sys->embed_hwnd, &clientRect);
    if (RECTWidth(sys->rect_parent)  != RECTWidth(clientRect) ||
        RECTHeight(sys->rect_parent) != RECTHeight(clientRect)) {
        sys->rect_parent = clientRect;

        SetWindowPos(hwnd, 0, 0, 0,
                     RECTWidth(sys->rect_parent),
                     RECTHeight(sys->rect_parent),
                     SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
    }

    switch( message )
    {
    case WM_ERASEBKGND:
        /* nothing to erase */
        return 1;

    case WM_PAINT:
        /* nothing to repaint */
        ValidateRect(hwnd, NULL);
        break;

    case WM_CLOSE:
        /* wait for all pending timers */
        vlc_wasync_resize_compressor_dropOrWait(&sys->compressor);
        vlc_window_ReportClose(wnd);
        return 0;

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_SIZE:
        vlc_wasync_resize_compressor_reportSize(&sys->compressor, LOWORD(lParam), HIWORD(lParam));
        return 0;
    default:
        break;
    }

    /* Let windows handle the message */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void *WindowLoopThread(void *lpParameter)
{
    struct drawable_sys *sys = lpParameter;

    /* Get this module's instance */
    HMODULE hInstance = GetModuleHandle(NULL);

    sys->hWnd =
        CreateWindowEx( 0,
                    EMBED_HWND_CLASS,              /* name of window class */
                    TEXT("Embedded HWND"),                 /* window title */
                    WS_CHILD|WS_VISIBLE|WS_DISABLED,       /* window style */
                    0,                             /* default X coordinate */
                    0,                             /* default Y coordinate */
                    RECTWidth(sys->rect_parent),           /* window width */
                    RECTHeight(sys->rect_parent),         /* window height */
                    sys->embed_hwnd,                      /* parent window */
                    NULL,                        /* no menu in this window */
                    hInstance,          /* handle of this program instance */
                    sys );                            /* send to WM_CREATE */

    vlc_sem_post(&sys->hwnd_set);

    if (sys->hWnd == NULL)
        return NULL;

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    MSG msg;
    while( GetMessage( &msg, 0, 0, 0 ) )
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return NULL;
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

        HINSTANCE hInstance = GetModuleHandle(NULL);
        UnregisterClass( EMBED_HWND_CLASS, hInstance );
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
    bool first_hwnd = used == NULL;
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

    sys->embed_hwnd = (HWND)val;
    sys->wnd = wnd;
    GetClientRect(sys->embed_hwnd, &sys->rect_parent);
    vlc_sem_init(&sys->hwnd_set, 0);

    if (first_hwnd)
    {
        /* Get this module's instance */
        HMODULE hInstance = GetModuleHandle(NULL);

        WNDCLASS wc = { 0 };                      /* window class components */
        wc.lpfnWndProc   = WinVoutEventProc;                /* event handler */
        wc.hInstance     = hInstance;                            /* instance */
        wc.lpszClassName = EMBED_HWND_CLASS;
        if( !RegisterClass(&wc) )
        {
            msg_Err( sys->wnd, "RegisterClass failed (err=%lu)", GetLastError() );
            goto error;
        }
    }

    if (vlc_wasync_resize_compressor_init(&sys->compressor, wnd))
    {
        msg_Err(wnd, "Failed to init async resize compressor");
        goto error;
    }

    // Create a Thread for the window event loop
    if (vlc_clone(&sys->thread, WindowLoopThread, sys))
    {
        msg_Err( sys->wnd, "Failed to start WindowLoopThread");
        goto error;
    }

    vlc_sem_wait(&sys->hwnd_set);

    if (sys->hWnd == NULL)
    {
        msg_Err( sys->wnd, "Failed to create a window (err=%lu)", GetLastError() );
        goto error;
    }

    wnd->type = VLC_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = (void *)sys->hWnd;
    wnd->ops = &ops;
    wnd->sys = (void *)sys;
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
    struct drawable_sys *sys = wnd->sys;

    if (sys->hWnd)
        PostMessage( sys->hWnd, WM_CLOSE, 0, 0 );

    if (sys->thread != NULL)
        vlc_join(sys->thread, NULL);

    vlc_wasync_resize_compressor_destroy(&sys->compressor);

    RemoveDrawable(sys->embed_hwnd);
}
