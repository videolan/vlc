/*****************************************************************************
 * vout_wrapper.h: definitions for vout wrappers (temporary)
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

#ifndef LIBVLC_VOUT_WRAPPER_H
#define LIBVLC_VOUT_WRAPPER_H 1

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
                                        subpicture_t *subpicture,
                                        vlc_tick_t date)
{
    if (vd->prepare)
        vd->prepare(vd, picture, subpicture, date);
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
#if defined(_WIN32) || defined(__OS2__)
    unsigned wm_state;
#endif
    vlc_rational_t sar;
} vout_display_state_t;

/**
 * It creates a vout managed display.
 */
vout_display_t *vout_NewDisplay( vout_thread_t *, const video_format_t *,
    const vout_display_state_t *, const char *module);
/**
 * It destroy a vout managed display.
 */
void vout_DeleteDisplay(vout_display_t *, vout_display_state_t *);
bool vout_IsDisplayFiltered(vout_display_t *);
picture_t * vout_FilterDisplay(vout_display_t *, picture_t *);
void vout_FilterFlush(vout_display_t *);
bool vout_AreDisplayPicturesInvalid(vout_display_t *);

bool vout_ManageDisplay(vout_display_t *, bool allow_reset_pictures);

void vout_SetDisplaySize(vout_display_t *, unsigned width, unsigned height);
void vout_SetDisplayFilled(vout_display_t *, bool is_filled);
void vout_SetDisplayZoom(vout_display_t *, unsigned num, unsigned den);
void vout_SetDisplayAspect(vout_display_t *, unsigned num, unsigned den);
void vout_SetDisplayCrop(vout_display_t *, unsigned num, unsigned den,
                         unsigned left, unsigned top, int right, int bottom);
void vout_SetDisplayViewpoint(vout_display_t *, const vlc_viewpoint_t *);

#endif /* LIBVLC_VOUT_WRAPPER_H */

