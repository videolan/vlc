/**
 * @file planes.c
 * @brief DRM planes
 */
/*****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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

#include <assert.h>
#include <errno.h>
#include <drm_mode.h>
#include <vlc_common.h>
#include "vlc_drm.h"

int vlc_drm_get_crtc_index(int fd, uint_fast32_t crtc_id)
{
    uint32_t crtcs[32];
    struct drm_mode_card_res res = {
        .crtc_id_ptr = (uintptr_t)(void *)crtcs,
        .count_crtcs = ARRAY_SIZE(crtcs),
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0)
        return -1;
    if (unlikely(res.count_crtcs > ARRAY_SIZE(crtcs))) {
        /* The API cannot deal with more than 32 CRTCs. Buggy driver? */
        errno = ENOBUFS;
        return -1;
    }

    for (size_t i = 0; i < res.count_crtcs; i++)
        if (crtcs[i] == crtc_id)
            return i;

    errno = ENXIO;
    return -1;
}
