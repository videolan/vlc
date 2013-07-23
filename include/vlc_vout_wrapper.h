/*****************************************************************************
 * vlc_vout_wrapper.h: definitions for vout wrappers (temporary)
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

#ifndef VLC_VOUT_WRAPPER_H
#define VLC_VOUT_WRAPPER_H 1

#include <vlc_vout_display.h>

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
static inline void vout_display_Prepare(vout_display_t *vd,
                                        picture_t *picture,
                                        subpicture_t *subpicture)
{
    if (vd->prepare )
        vd->prepare(vd, picture, subpicture);
}

/**
 * It display a picture.
 */
static inline void vout_display_Display(vout_display_t *vd,
                                        picture_t *picture,
                                        subpicture_t *subpicture)
{
    vd->display(vd, picture, subpicture);
}

/**
 * It holds a state for a vout display.
 */
typedef struct {
    vout_display_cfg_t cfg;
    unsigned wm_state;
    struct {
        int num;
        int den;
    } sar;
} vout_display_state_t;

/**
 * It creates a vout managed display.
 */
VLC_API vout_display_t * vout_NewDisplay( vout_thread_t *, const video_format_t *, const vout_display_state_t *, const char *psz_module, mtime_t i_double_click_timeout, mtime_t i_hide_timeout );
/**
 * It destroy a vout managed display.
 */
VLC_API void vout_DeleteDisplay(vout_display_t *, vout_display_state_t *);

VLC_API bool vout_IsDisplayFiltered(vout_display_t *);
VLC_API picture_t * vout_FilterDisplay(vout_display_t *, picture_t *);
VLC_API bool vout_AreDisplayPicturesInvalid(vout_display_t *);

VLC_API bool vout_ManageDisplay(vout_display_t *, bool allow_reset_pictures);

VLC_API void vout_SetDisplayFullscreen(vout_display_t *, bool is_fullscreen);
VLC_API void vout_SetDisplayFilled(vout_display_t *, bool is_filled);
VLC_API void vout_SetDisplayZoom(vout_display_t *, int num, int den);
VLC_API void vout_SetWindowState(vout_display_t *, unsigned state);
VLC_API void vout_SetDisplayAspect(vout_display_t *, unsigned dar_num, unsigned dar_den);
VLC_API void vout_SetDisplayCrop(vout_display_t *, unsigned crop_num, unsigned crop_den, unsigned left, unsigned top, int right, int bottom);

struct vlc_gl_t;
VLC_API struct vlc_gl_t * vout_GetDisplayOpengl(vout_display_t *);

#endif /* VLC_VOUT_WRAPPER_H */

