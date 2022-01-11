/*****************************************************************************
 * instance.c: VDPAU instance management for VLC
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <vlc_common.h>
#include "vlc_vdpau.h"

#pragma GCC visibility push(default)

typedef struct vdp_instance
{
    Display *display;
    vdp_t *vdp;
    VdpDevice device;

    uintptr_t refs; /**< Reference count */
    struct vdp_instance *next;
} vdp_instance_t;

static vdp_instance_t *vdp_instance_create(const char *name, int num)
{
    vdp_instance_t *vi = malloc(sizeof (*vi));

    if (unlikely(vi == NULL))
        return NULL;

    vi->display = XOpenDisplay(name);
    if (vi->display == NULL)
    {
        free(vi);
        return NULL;
    }

    if (num < 0)
        num = XDefaultScreen(vi->display);

    VdpStatus err = vdp_create_x11(vi->display, num, &vi->vdp, &vi->device);
    if (err != VDP_STATUS_OK)
    {
        XCloseDisplay(vi->display);
        free(vi);
        return NULL;
    }

    vi->next = NULL;
    vi->refs = 1;

    return vi;
}

static void vdp_instance_destroy(vdp_instance_t *vi)
{
    vdp_device_destroy(vi->vdp, vi->device);
    vdp_destroy_x11(vi->vdp);
    XCloseDisplay(vi->display);
    free(vi);
}

static vdp_instance_t *list = NULL;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** Finds an existing VDPAU instance for the given X11 display and screen.
 * If not found, (try to) create a new one.
 * @param display_name X11 display string, NULL for default
 * @param snum X11 screen number, strictly negative for default
 **/
VdpStatus vdp_get_x11(const char *display_name, int snum,
                      vdp_t **restrict vdpp, VdpDevice *restrict devicep)
{
    vdp_instance_t *vi = vdp_instance_create(display_name, snum);
    if (vi == NULL)
        return VDP_STATUS_ERROR;

    pthread_mutex_lock(&lock);
    vi->next = list;
    list = vi;
    pthread_mutex_unlock(&lock);
    *vdpp = vi->vdp;
    *devicep = vi->device;
    return VDP_STATUS_OK;
}

void vdp_release_x11(vdp_t *vdp)
{
    vdp_instance_t *vi, **pp = &list;

    pthread_mutex_lock(&lock);
    for (;;)
    {
        vi = *pp;
        assert(vi != NULL);
        if (vi->vdp == vdp)
            break;
        pp = &vi->next;
    }

    assert(vi->refs > 0);
    vi->refs--;
    if (vi->refs > 0)
        vi = NULL; /* Keep the instance for now */
    else
        *pp = vi->next; /* Unlink the instance */
    pthread_mutex_unlock(&lock);

    if (vi != NULL)
        vdp_instance_destroy(vi);
}
