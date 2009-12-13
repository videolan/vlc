/*****************************************************************************
 * vlc_vout_wrapper.h: definitions for vout wrappers (temporary)
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_VOUT_WRAPPER_H
#define VLC_VOUT_WRAPPER_H 1

#include <vlc_vout_display.h>
#include <vlc_vout_opengl.h>

/* XXX DO NOT use it outside the vout module wrapper XXX */

/**
 * It retreives a picture pool from the display
 */
static inline picture_pool_t *vout_display_Pool(vout_display_t *vd, unsigned count)
{
    return vd->pool(vd, count);
}

/**
 * It preparse a picture for display.
 */
static inline void vout_display_Prepare(vout_display_t *vd, picture_t *picture)
{
    if (vd->prepare )
        vd->prepare(vd, picture);
}

/**
 * It display a picture.
 */
static inline void vout_display_Display(vout_display_t *vd, picture_t *picture)
{
    vd->display(vd, picture);
}

/**
 * It holds a state for a vout display.
 */
typedef struct {
    vout_display_cfg_t cfg;

    bool is_on_top;
    struct {
        int num;
        int den;
    } sar;
} vout_display_state_t;

/**
 * It creates a vout managed display.
 */
VLC_EXPORT(vout_display_t *, vout_NewDisplay, ( vout_thread_t *, const video_format_t *, const vout_display_state_t *, const char *psz_module, mtime_t i_double_click_timeout, mtime_t i_hide_timeout ));
/**
 * It destroy a vout managed display.
 */
VLC_EXPORT(void, vout_DeleteDisplay, (vout_display_t *, vout_display_state_t *));

VLC_EXPORT(bool, vout_IsDisplayFiltered, (vout_display_t *));
VLC_EXPORT(picture_t *, vout_FilterDisplay, (vout_display_t *, picture_t *));
VLC_EXPORT(bool, vout_AreDisplayPicturesInvalid, (vout_display_t *));

VLC_EXPORT(void, vout_ManageDisplay, (vout_display_t *, bool allow_reset_pictures));

VLC_EXPORT(void, vout_SetDisplayFullscreen, (vout_display_t *, bool is_fullscreen));
VLC_EXPORT(void, vout_SetDisplayFilled, (vout_display_t *, bool is_filled));
VLC_EXPORT(void, vout_SetDisplayZoom, (vout_display_t *, int num, int den));
VLC_EXPORT(void, vout_SetDisplayOnTop, (vout_display_t *, bool is_on_top));
VLC_EXPORT(void, vout_SetDisplayAspect, (vout_display_t *, unsigned sar_num, unsigned sar_den));
VLC_EXPORT(void, vout_SetDisplayCrop, (vout_display_t *, unsigned crop_num, unsigned crop_den, unsigned x, unsigned y, unsigned width, unsigned height));
VLC_EXPORT(vout_opengl_t *, vout_GetDisplayOpengl, (vout_display_t *));

#endif /* VLC_VOUT_WRAPPER_H */

