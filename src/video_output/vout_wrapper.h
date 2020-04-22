/*****************************************************************************
 * vout_wrapper.h: definitions for vout wrappers (temporary)
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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
struct vout_crop;

picture_pool_t *vout_GetPool(vout_display_t *vd, unsigned count);

bool vout_IsDisplayFiltered(vout_display_t *);
picture_t * vout_ConvertForDisplay(vout_display_t *, picture_t *);
void vout_FilterFlush(vout_display_t *);

void vout_SetDisplayFitting(vout_display_t *, enum vlc_video_fitting);
void vout_SetDisplayZoom(vout_display_t *, unsigned num, unsigned den);
void vout_SetDisplayAspect(vout_display_t *, unsigned num, unsigned den);
void vout_SetDisplayCrop(vout_display_t *, const struct vout_crop *);
void vout_SetDisplayViewpoint(vout_display_t *, const vlc_viewpoint_t *);
int vout_SetDisplayFormat(vout_display_t *, const video_format_t *fmt,
                          vlc_video_context *vctx);

/* The owner/caller is responsible for managing the lifetime of this ICC
 * profile and always updating the display state to a consistent value */
void vout_SetDisplayIccProfile(vout_display_t *, const vlc_icc_profile_t *);

#endif /* LIBVLC_VOUT_WRAPPER_H */

