/*****************************************************************************
 * event.h: vout event
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <math.h>

#include "vout_control.h"

/* TODO/FIXME
 *
 * It should be converted to something like input_thread_t:
 * one intf-event can be grabbed by a callback, all others
 * variable only var_Change
 *
 * Maybe a intf-mouse can be used too (don't like it).
 *
 * (Some case may infinite loop otherwise here)
 */

static inline void vout_SendEventClose(vout_thread_t *vout)
{
#warning FIXME: implement video close event
    /* FIXME: this code is disabled as it breaks the non-playlist cases */
    //playlist_Stop(pl_Get(vout));
    (void) vout;
}
static inline void vout_SendEventKey(vout_thread_t *vout, int key)
{
    var_SetInteger(vout->p_libvlc, "key-pressed", key);
}
static inline void vout_SendEventMouseMoved(vout_thread_t *vout, int x, int y)
{
    var_SetCoords(vout, "mouse-moved", x, y);
}
static inline void vout_SendEventMousePressed(vout_thread_t *vout, int button)
{
    int key;
    var_OrInteger(vout, "mouse-button-down", 1 << button);

    switch (button)
    {
    case MOUSE_BUTTON_LEFT:
    {
        /* FIXME? */
        int x, y;
        var_GetCoords(vout, "mouse-moved", &x, &y);
        var_SetCoords(vout, "mouse-clicked", x, y);
        var_SetBool(vout->p_libvlc, "intf-popupmenu", false);
        return;
    }
    case MOUSE_BUTTON_CENTER:
        var_ToggleBool(vout->p_libvlc, "intf-toggle-fscontrol");
        return;
    case MOUSE_BUTTON_RIGHT:
        var_SetBool(vout->p_libvlc, "intf-popupmenu", true);
        return;
    case MOUSE_BUTTON_WHEEL_UP:    key = KEY_MOUSEWHEELUP;    break;
    case MOUSE_BUTTON_WHEEL_DOWN:  key = KEY_MOUSEWHEELDOWN;  break;
    case MOUSE_BUTTON_WHEEL_LEFT:  key = KEY_MOUSEWHEELLEFT;  break;
    case MOUSE_BUTTON_WHEEL_RIGHT: key = KEY_MOUSEWHEELRIGHT; break;
    }
    vout_SendEventKey(vout, key);
}
static inline void vout_SendEventMouseReleased(vout_thread_t *vout, int button)
{
    var_NAndInteger(vout, "mouse-button-down", 1 << button);
}
static inline void vout_SendEventMouseDoubleClick(vout_thread_t *vout)
{
    //vout_ControlSetFullscreen(vout, !var_GetBool(vout, "fullscreen"));
    var_ToggleBool(vout, "fullscreen");
}
static inline void vout_SendEventMouseVisible(vout_thread_t *vout)
{
    /* TODO */
    VLC_UNUSED(vout);
}
static inline void vout_SendEventMouseHidden(vout_thread_t *vout)
{
    /* TODO */
    VLC_UNUSED(vout);
}

static inline void vout_SendEventFullscreen(vout_thread_t *vout, bool is_fullscreen)
{
    var_SetBool(vout, "fullscreen", is_fullscreen);
}

static inline void vout_SendEventDisplayFilled(vout_thread_t *vout, bool is_display_filled)
{
    if (!var_GetBool(vout, "autoscale") != !is_display_filled)
        var_SetBool(vout, "autoscale", is_display_filled);
}

static inline void vout_SendEventZoom(vout_thread_t *vout, int num, int den)
{
    VLC_UNUSED(vout);
    VLC_UNUSED(num);
    VLC_UNUSED(den);
    /* FIXME deadlock problems with current vout */
#if 0
    const float zoom = (float)num / (float)den;

    /* XXX 0.1% is arbitrary */
    if (fabs(zoom - var_GetFloat(vout, "scale")) > 0.001)
        var_SetFloat(vout, "scale", zoom);
#endif
}

static inline void vout_SendEventOnTop(vout_thread_t *vout, bool is_on_top)
{
    VLC_UNUSED(vout);
    VLC_UNUSED(is_on_top);
    /* FIXME deadlock problems with current vout */
#if 0

    if (!var_GetBool(vout, "video-on-top") != !is_on_top)
        var_SetBool(vout, "video-on-top", is_on_top);
#endif
}

/**
 * It must be called on source aspect ratio changes, with the new DAR (Display
 * Aspect Ratio) value.
 */
static inline void vout_SendEventSourceAspect(vout_thread_t *vout,
                                              unsigned num, unsigned den)
{
    VLC_UNUSED(vout);
    VLC_UNUSED(num);
    VLC_UNUSED(den);
    /* FIXME the value stored in "aspect-ratio" are not reduced
     * creating a lot of problems here */
#if 0
    char *ar;
    if (num > 0 && den > 0) {
        if (asprintf(&ar, "%u:%u", num, den) < 0)
            return;
    } else {
        ar = strdup("");
    }

    char *current = var_GetString(vout, "aspect-ratio");
    msg_Err(vout, "vout_SendEventSourceAspect %s -> %s", current, ar);
    if (ar && current && strcmp(ar, current))
        var_SetString(vout, "aspect-ratio", ar);

    free(current);
    free(ar);
#endif
}
static inline void vout_SendEventSourceCrop(vout_thread_t *vout,
                                            unsigned num, unsigned den,
                                            unsigned left, unsigned top,
                                            unsigned right, unsigned bottom)
{
    VLC_UNUSED(num);
    VLC_UNUSED(den);

    vlc_value_t val;

    /* I cannot use var_Set here, infinite loop otherwise */

    /* */
    val.i_int = left;
    var_Change(vout, "crop-left",   VLC_VAR_SETVALUE, &val, NULL);
    val.i_int = top;
    var_Change(vout, "crop-top",    VLC_VAR_SETVALUE, &val, NULL);
    val.i_int = right;
    var_Change(vout, "crop-right",  VLC_VAR_SETVALUE, &val, NULL);
    val.i_int = bottom;
    var_Change(vout, "crop-bottom", VLC_VAR_SETVALUE, &val, NULL);

    /* FIXME the value stored in "crop" are not reduced
     * creating a lot of problems here */
#if 0
    char *crop;
    if (num > 0 && den > 0) {
        if (asprintf(&crop, "%u:%u", num, den) < 0)
            crop = NULL;
    } else if (left > 0 || top > 0 || right > 0 || bottom > 0) {
        if (asprintf(&crop, "%u+%u+%u+%u", left, top, right, bottom) < 0)
            crop = NULL;
    } else {
        crop = strdup("");
    }
    if (crop) {
        val.psz_string = crop;
        var_Change(vout, "crop", VLC_VAR_SETVALUE, &val, NULL);
        free(crop);
    }
#endif
}
#if 0
static inline void vout_SendEventSnapshot(vout_thread_t *vout, const char *filename)
{
    /* signal creation of a new snapshot file */
    var_SetString(vout->p_libvlc, "snapshot-file", filename);
}

#warning "FIXME clean up postproc event"

extern void vout_InstallDeprecatedPostProcessing(vout_thread_t *);
extern void vout_UninstallDeprecatedPostProcessing(vout_thread_t *);

static inline void vout_SendEventPostProcessing(vout_thread_t *vout, bool is_available)
{
    if (is_available)
        vout_InstallDeprecatedPostProcessing(vout);
    else
        vout_UninstallDeprecatedPostProcessing(vout);
}

static inline void vout_SendEventFilters(vout_thread_t *vout)
{
    vout_filter_t **filter;
    int           filter_count;

    vout_ControlGetFilters(vout, &filter, &filter_count);

    char *list = strdup("");
    for (int i = 0; i < filter_count; i++) {
        char *psz;

        if (asprintf(&psz, "%s%s%s",
                     list, i > 0 ? ":" : "", filter[i]->name) < 0) {
            free(list);
            list = NULL;
            break;
        }
        free(list);
        list = psz;
    }

    if (list) {
        vlc_value_t val;
        val.psz_string = list;
        var_Change(vout, "video-filter", VLC_VAR_SETVALUE, &val, NULL);
        free(list);
    }

    for (int i = 0; i < filter_count; i++)
        vout_filter_Delete(filter[i]);
    free(filter);
}
#endif
