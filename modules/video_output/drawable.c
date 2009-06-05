/**
 * @file drawable.c
 * @brief Legacy monolithic LibVLC video window provider
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
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
#include <vlc_vout.h>
#include <vlc_window.h>

static int  OpenXID (vlc_object_t *);
static int  OpenHWND (vlc_object_t *);
static void Close (vlc_object_t *);

#define XID_TEXT N_("ID of the video output X window")
#define XID_LONGTEXT N_( \
    "VLC can embed its video output in an existing X11 window. " \
    "This is the X identifier of that window (0 means none).")

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded X window video"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("xwindow", 70)
    set_callbacks (OpenXID, Close)
    add_integer ("drawable-xid", 0, NULL, XID_TEXT, XID_LONGTEXT, true)
        change_unsaveable ()
        /*change_integer_range (0, 0xffffffff)*/

    add_submodule ()
        set_description (N_("Embedded Windows video"))
        set_capability ("hwnd", 70)
        set_callbacks (OpenHWND, Close)

vlc_module_end ()

static int Control (vout_window_t *, int, va_list);

/* TODO: move to vlc_variables.h */
static inline void *var_GetAddress (vlc_object_t *o, const char *name)
{
    vlc_value_t val;
    return var_Get (o, name, &val) ? NULL : val.p_address;
}

static vlc_mutex_t serializer = VLC_STATIC_MUTEX;

/**
 * Find the drawable set by libvlc application.
 */
static int Open (vlc_object_t *obj, const char *varname, bool ptr)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    void **used, *val;
    size_t n = 0;

    if (var_Create (obj->p_libvlc, "drawables-in-use", VLC_VAR_ADDRESS)
     || var_Create (obj, varname, VLC_VAR_DOINHERIT
                                  | (ptr ? VLC_VAR_ADDRESS : VLC_VAR_INTEGER)))
        return VLC_ENOMEM;

    if (ptr)
        val = var_GetAddress (obj, varname);
    else
        val = (void *)(uintptr_t)var_GetInteger (obj, varname);
    var_Destroy (obj, varname);

    /* Keep a list of busy drawables, so we don't overlap videos if there are
     * more than one video track in the stream. */
    vlc_mutex_lock (&serializer);
    /* TODO: per-type list of busy drawables */
    used = var_GetAddress (VLC_OBJECT (obj->p_libvlc), "drawables-in-use");
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
        var_SetAddress (obj->p_libvlc, "drawables-in-use", used);
    }
    else
    {
skip:
        msg_Warn (wnd, "drawable %p is busy", val);
        val = NULL;
    }
    vlc_mutex_unlock (&serializer);

    if (val == NULL)
        return VLC_EGENERIC;

    if (ptr)
        wnd->handle.hwnd = val;
    else
        wnd->handle.xid = (uintptr_t)val;

    /* FIXME: check that X server matches --x11-display (if specified) */
    /* FIXME: get window size (in platform-dependent ways) */

    wnd->control = Control;
    wnd->p_sys = val;
    return VLC_SUCCESS;
}

static int  OpenXID (vlc_object_t *obj)
{
    return Open (obj, "drawable-xid", false);
}

static int  OpenHWND (vlc_object_t *obj)
{
    return Open (obj, "drawable-hwnd", true);
}


/**
 * Release the drawable.
 */
static void Close (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    void **used, *val = wnd->p_sys;
    size_t n = 0;

    /* Remove this drawable from the list of busy ones */
    vlc_mutex_lock (&serializer);
    used = var_GetAddress (VLC_OBJECT (obj->p_libvlc), "drawables-in-use");
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
      /* should not be needed (var_Destroy...) but better safe than sorry: */
         var_SetAddress (obj->p_libvlc, "drawables-in-use", NULL);
    vlc_mutex_unlock (&serializer);

    if (n == 0)
        free (used);
    /* Variables are reference-counted... */
    var_Destroy (obj->p_libvlc, "drawables-in-use");
}


static int Control (vout_window_t *wnd, int query, va_list ap)
{
    switch (query)
    {
        case VOUT_SET_SIZE: /* not allowed */
        case VOUT_SET_STAY_ON_TOP: /* not allowed either, would be ugly */
            return VLC_EGENERIC;
    }

    msg_Warn (wnd, "unsupported control query %d", query);
    return VLC_EGENERIC;
}

