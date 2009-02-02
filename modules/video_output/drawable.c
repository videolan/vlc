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

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Drawable"))
    set_description (N_("External embedded video window"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
#ifdef WIN32
    set_capability ("hwnd", 70)
#else
    set_capability ("xwindow", 70)
#endif
    set_callbacks (Open, Close)

vlc_module_end ()

static int Control (vout_window_t *, int, va_list);

/**
 * Find the drawable set by libvlc application.
 */
static int Open (vlc_object_t *obj)
{
    static vlc_mutex_t serializer = VLC_STATIC_MUTEX;
    vout_window_t *wnd = (vout_window_t *)obj;
    int drawable = 0;

    if (var_Create (obj->p_libvlc, "drawable-busy", VLC_VAR_BOOL))
        return VLC_ENOMEM;

    vlc_mutex_lock (&serializer);
    /* Note: We cannot simply clear the drawable variable.
     * It would break libvlc_video_get_parent(). */
    if (!var_GetBool (obj->p_libvlc, "drawable-busy"))
    {
        /* TODO: implement separate variables for XIDs and HWNDs */
        drawable = var_GetInteger (obj->p_libvlc, "drawable");
        if (drawable != 0)
            var_SetBool (obj->p_libvlc, "drawable-busy", true);
    }
    vlc_mutex_unlock (&serializer);

    if (drawable == 0)
    {
        var_Destroy (obj->p_libvlc, "drawable-busy");
        return VLC_EGENERIC;
    }

#ifdef WIN32
    /* FIXME: don't loose critical bits on Win64 */
    wnd->handle.hwnd = (void *)drawable;
#else
    /* FIXME: check that X server matches --x11-display (if specified) */
    /* FIXME: get X drawable dimensions */
    wnd->handle.xid = drawable;
#endif
    /* FIXME: get window size (in platform-dependent ways) */

    wnd->control = Control;
    return VLC_SUCCESS;
}


/**
 * Release the drawable.
 */
static void Close (vlc_object_t *obj)
{
    /* This is atomic with regards to var_GetBool() in Open(): */
    var_SetBool (obj->p_libvlc, "drawable-busy", false);

    /* Variables are reference-counted... */
    var_Destroy (obj->p_libvlc, "drawable-busy");
}


static int Control (vout_window_t *wnd, int query, va_list ap)
{
    switch (query)
    {
        case VOUT_GET_SIZE:
        {
            unsigned int *pi_width = va_arg (ap, unsigned int *);
            unsigned int *pi_height = va_arg (ap, unsigned int *);
            *pi_width = wnd->width;
            *pi_height = wnd->height;
            return VLC_SUCCESS;
        }

        case VOUT_SET_SIZE: /* not allowed */
        case VOUT_SET_STAY_ON_TOP: /* not allowed either, would be ugly */
            return VLC_EGENERIC;
    }

    msg_Warn (wnd, "unsupported control query %d", query);
    return VLC_EGENERIC;
}

