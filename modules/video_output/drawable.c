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
#include <vlc_vout_window.h>

#define HWND_TEXT N_("Window handle (HWND)")
#define HWND_LONGTEXT N_( \
    "Video will be embedded in this pre-existing window. " \
    "If zero, a new window will be created.")

static int  Open (vout_window_t *, const vout_window_cfg_t *);
static void Close(vout_window_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded window video"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window hwnd", 0)
    set_callbacks (Open, Close)
    add_shortcut ("embed-hwnd")

    add_integer ("drawable-hwnd", 0, HWND_TEXT, HWND_LONGTEXT, true)
        change_volatile ()
vlc_module_end ()

static int Control (vout_window_t *, int, va_list);

/* Keep a list of busy drawables, so we don't overlap videos if there are
 * more than one video track in the stream. */
static vlc_mutex_t serializer = VLC_STATIC_MUTEX;
static uintptr_t *used = NULL;

/**
 * Find the drawable set by libvlc application.
 */
static int Open (vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    VLC_UNUSED (cfg);
    uintptr_t val = var_InheritInteger (wnd, "drawable-hwnd");
    if (val == 0)
        return VLC_EGENERIC;

    uintptr_t *tab;
    size_t n = 0;

    vlc_mutex_lock (&serializer);
    if (used != NULL)
        for (/*n = 0*/; used[n]; n++)
            if (used[n] == val)
            {
                msg_Warn (wnd, "HWND 0x%zX is busy", val);
                val = 0;
                goto skip;
            }

    tab = realloc (used, sizeof (*used) * (n + 2));
    if (likely(tab != NULL))
    {
        used = tab;
        used[n] = val;
        used[n + 1] = 0;
    }
    else
        val = 0;
skip:
    vlc_mutex_unlock (&serializer);

    if (val == 0)
        return VLC_EGENERIC;

    wnd->handle.hwnd = (void *)val;
    wnd->control = Control;
    wnd->sys = (void *)val;
    return VLC_SUCCESS;
}

/**
 * Release the drawable.
 */
static void Close (vout_window_t *wnd)
{
    uintptr_t val = (uintptr_t)wnd->sys;
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

    if (n == 0)
    {
         free (used);
         used = NULL;
    }
    vlc_mutex_unlock (&serializer);
}


static int Control (vout_window_t *wnd, int query, va_list ap)
{
    VLC_UNUSED( ap );

    switch (query)
    {
        case VOUT_WINDOW_SET_SIZE:   /* not allowed */
        case VOUT_WINDOW_SET_STATE: /* not allowed either, would be ugly */
            return VLC_EGENERIC;
        default:
            msg_Warn (wnd, "unsupported control query %d", query);
            return VLC_EGENERIC;
    }
}
