/**
 * @file drawable.c
 * @brief Legacy monolithic LibVLC video window provider
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdarg.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded window video"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window hwnd", 70)
    set_callbacks (Open, Close)
vlc_module_end ()

static int Control (vout_window_t *, int, va_list);

static vlc_mutex_t serializer = VLC_STATIC_MUTEX;

/**
 * Find the drawable set by libvlc application.
 */
static int Open (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    void **used, *val;
    size_t n = 0;

    if (var_Create (obj->p_libvlc, "hwnd-in-use", VLC_VAR_ADDRESS)
     || var_Create (obj, "drawable-hwnd", VLC_VAR_DOINHERIT | VLC_VAR_ADDRESS))
        return VLC_ENOMEM;

    val = var_GetAddress (obj, "drawable-hwnd");
    var_Destroy (obj, "drawable-hwnd");

    /* Keep a list of busy drawables, so we don't overlap videos if there are
     * more than one video track in the stream. */
    vlc_mutex_lock (&serializer);
    used = var_GetAddress (obj->p_libvlc, "hwnd-in-use");
    if (used != NULL)
    {
        while (used[n] != NULL)
        {
            if (used[n] == val)
                goto skip;
            n++;
        }
    }

    used = realloc (used, sizeof (*used) * (n + 2));
    if (used != NULL)
    {
        used[n] = val;
        used[n + 1] = NULL;
        var_SetAddress (obj->p_libvlc, "hwnd-in-use", used);
    }
    else
    {
skip:
        msg_Warn (wnd, "HWND %p is busy", val);
        val = NULL;
    }
    vlc_mutex_unlock (&serializer);

    if (val == NULL)
        return VLC_EGENERIC;

    wnd->handle.hwnd = val;
    wnd->control = Control;
    wnd->sys = val;
    return VLC_SUCCESS;
}

/**
 * Release the drawable.
 */
static void Close (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    void **used, *val = wnd->sys;
    size_t n = 0;

    /* Remove this drawable from the list of busy ones */
    vlc_mutex_lock (&serializer);
    used = var_GetAddress (obj->p_libvlc, "hwnd-in-use");
    assert (used);
    while (used[n] != val)
    {
        assert (used[n]);
        n++;
    }
    do
        used[n] = used[n + 1];
    while (used[++n] != NULL);

    if (n == 0)
         var_SetAddress (obj->p_libvlc, "hwnd-in-use", NULL);
    vlc_mutex_unlock (&serializer);

    if (n == 0)
        free (used);
    /* Variables are reference-counted... */
    var_Destroy (obj->p_libvlc, "hwnd-in-use");
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

