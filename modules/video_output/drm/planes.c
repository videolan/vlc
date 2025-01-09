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
#include <stdlib.h>
#include <string.h>
#ifndef HAVE_LIBDRM
# include <drm/drm_mode.h>
#else
# include <drm_mode.h>
#endif
#include <vlc_common.h>
#include <vlc_fourcc.h>
#include "vlc_drm.h"

enum { /* DO NOT CHANGE. MUST MATCH KERNEL ABI. */
    VLC_DRM_PLANE_TYPE_OVERLAY=0,
    VLC_DRM_PLANE_TYPE_PRIMARY=1,
    VLC_DRM_PLANE_TYPE_CURSOR=2,
};

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

static bool vlc_drm_prop_match(int fd, uint_fast32_t pid, const char *name)
{
    struct drm_mode_get_property prop = {
        .prop_id = pid,
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop) < 0)
        return false;
    return strcmp(name, prop.name) == 0;
}

static int vlc_drm_get_prop(int fd, uint_fast32_t oid, uint_fast32_t tid,
                            const char *name, uint64_t *restrict valp)
{
    struct drm_mode_obj_get_properties counter = {
        .obj_id = oid,
        .obj_type = tid,
    };
    int ret = -1;

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &counter) < 0)
        return -1;

    size_t count = counter.count_props;
    uint32_t *ids = vlc_alloc(count, sizeof (*ids));
    uint64_t *values = vlc_alloc(count, sizeof (*values));

    if (unlikely(ids == NULL || values == NULL))
        goto out;

    struct drm_mode_obj_get_properties props = {
        .props_ptr = (uintptr_t)(void *)ids,
        .prop_values_ptr = (uintptr_t)(void *)values,
        .count_props = count,
        .obj_id = oid,
        .obj_type = tid,
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &props) < 0)
        goto out;
    if (unlikely(count < props.count_props)) {
        /*
         * Properties should not be created asynchronously. It could
         * theoretically occur if the underlying object was hot-unplugged, then
         * different object with the same type was hot-plugged and got the same
         * object identifier. But if so, everything we thought we knew till now
         * has potentially become invalid, so we might as well fail safely.
         */
        errno = ENOBUFS;
        goto out;
    }

    /* NOTE: if more than one property is needed, rethink this function */
    for (size_t i = 0; i < props.count_props; i++) {
        if (vlc_drm_prop_match(fd, ids[i], name)) {
            *valp = values[i];
            ret = 0;
            goto out;
        }
    }
    errno = ENXIO;
out:
    free(values);
    free(ids);
    return ret;
}

static int vlc_drm_get_plane_prop(int fd, uint_fast32_t plane,
                                  const char *name, uint64_t *restrict valp)
{
    return vlc_drm_get_prop(fd, plane, DRM_MODE_OBJECT_PLANE, name, valp);
}

static ssize_t vlc_drm_get_planes(int fd, uint32_t **restrict listp)
{
    size_t count = 32;

    for (;;) {
        uint32_t *planes = vlc_alloc(count, sizeof (*planes));
        if (unlikely(planes == NULL))
            return -1;

        struct drm_mode_get_plane_res res = {
            .plane_id_ptr = (uintptr_t)(void *)planes,
            .count_planes = count,
        };

        if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &res) < 0) {
            free(planes);
            return -1;
        }

        if (likely(count >= res.count_planes)) {
            *listp = planes;
            return res.count_planes;
        }
        free(planes);
    }
}

uint_fast32_t vlc_drm_get_crtc_primary_plane(int fd, unsigned int idx,
                                             size_t *restrict nfmt)
{
    assert(idx < 32); /* Don't mix up object IDs and indices! */

    uint32_t *planes;
    ssize_t count = vlc_drm_get_planes(fd, &planes);
    if (count < 0)
        return -1;

    uint_fast32_t ret = 0;

    for (ssize_t i = 0; i < count; i++) {
        struct drm_mode_get_plane plane = {
            .plane_id = planes[i],
        };
        uint64_t planetype;

        if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_GETPLANE, &plane) >= 0
         && ((plane.possible_crtcs >> idx) & 1)
         && vlc_drm_get_plane_prop(fd, planes[i], "type", &planetype) == 0
         && planetype == VLC_DRM_PLANE_TYPE_PRIMARY) {
            ret = planes[i];
            *nfmt = plane.count_format_types;
            goto out;
        }
    }
    errno = ENXIO;
out:
    free(planes);
    return ret;
}

static uint_fast32_t vlc_drm_find_format(vlc_fourcc_t vlc_fourcc, size_t n,
                                         const uint32_t *restrict drm_fourccs)
{
    assert(vlc_fourcc != 0);

    const uint_fast32_t drm_fourcc = vlc_drm_fourcc(vlc_fourcc);

    if (drm_fourcc != 0) {
        /* Linear search for YUV(A) and RGBA formats */
        for (size_t i = 0; i < n; i++)
            if (drm_fourccs[i] == drm_fourcc)
                return drm_fourcc;
    }

    /* Quadratic search for RGB formats */
    for (size_t i = 0; i < n; i++)
        if (vlc_fourcc_drm(drm_fourccs[i]) == vlc_fourcc)
            return drm_fourccs[i];

     return 0;
}

uint_fast32_t vlc_drm_find_best_format(int fd, uint_fast32_t plane_id,
                                       size_t nfmt, vlc_fourcc_t chroma)
{
    uint32_t *fmts = vlc_alloc(nfmt, sizeof (*fmts));
    if (unlikely(fmts == NULL))
        return 0;

    struct drm_mode_get_plane plane = {
        .plane_id = plane_id,
        .count_format_types = nfmt,
        .format_type_ptr = (uintptr_t)(void *)fmts,
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_GETPLANE, &plane) < 0) {
        free(fmts);
        return 0;
    }

    if (nfmt > plane.count_format_types)
        nfmt = plane.count_format_types;

    vlc_fourcc_t *list = NULL;
    /* Look for an exact match first */
    uint_fast32_t drm_fourcc = vlc_drm_find_format(chroma, nfmt, fmts);
    if (drm_fourcc != 0)
        goto out;

    /* Fallback to decreasingly optimal formats */
    list = vlc_fourcc_GetFallback(chroma);
    if (list == NULL)
        goto out;

    for (size_t i = 0; list[i] != 0; i++) {
        drm_fourcc = vlc_drm_find_format(list[i], nfmt, fmts);
        if (drm_fourcc != 0)
            goto out;
    }
    errno = ENOTSUP;
out:
    free(list);
    free(fmts);
    return drm_fourcc;
}
