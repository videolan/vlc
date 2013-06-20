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

    int num; /**< X11 screen number */
    char *name; /**< X11 display name */

    uintptr_t refs; /**< Reference count */
    struct vdp_instance *next;
} vdp_instance_t;

static VdpStatus vdp_instance_create(const char *name, int num,
                                     vdp_instance_t **pp)
{
    size_t namelen = (name != NULL) ? (strlen(name) + 1) : 0;
    vdp_instance_t *vi = malloc(sizeof (*vi) + namelen);

    if (unlikely(vi == NULL))
        return VDP_STATUS_RESOURCES;

    vi->display = XOpenDisplay(name);
    if (vi->display == NULL)
    {
        free(vi);
        return VDP_STATUS_ERROR;
    }

    vi->next = NULL;
    if (name != NULL)
    {
        vi->name = (void *)(vi + 1);
        memcpy(vi->name, name, namelen);
    }
    else
        vi->name = NULL;
    if (num >= 0)
        vi->num = num;
    else
        vi->num = XDefaultScreen(vi->display);
    vi->refs = 1;

    VdpStatus err = vdp_create_x11(vi->display, vi->num,
                                   &vi->vdp, &vi->device);
    if (err != VDP_STATUS_OK)
    {
        XCloseDisplay(vi->display);
        free(vi);
        return err;
    }
    *pp = vi;
    return VDP_STATUS_OK;
}

static void vdp_instance_destroy(vdp_instance_t *vi)
{
    vdp_device_destroy(vi->vdp, vi->device);
    vdp_destroy_x11(vi->vdp);
    XCloseDisplay(vi->display);
    free(vi);
}

/** Compares two string pointers that might be NULL */
static int strnullcmp(const char *a, const char *b)
{
    if (b == NULL)
        return a != NULL;
    if (a == NULL)
        return -1;
    return strcmp(a, b);
}

static int vicmp(const char *name, int num, const vdp_instance_t *vi)
{
    int val = strnullcmp(name, vi->name);
    if (val)
        return val;

    if (num < 0)
        num = XDefaultScreen(vi->display);
    return num - vi->num;
}

static vdp_instance_t *list = NULL;

static vdp_instance_t *vdp_instance_lookup(const char *name, int num)
{
    vdp_instance_t *vi = NULL;

    for (vi = list; vi != NULL; vi = vi->next)
    {
        int val = vicmp(name, num, vi);
        if (val == 0)
        {
            assert(vi->refs < UINTPTR_MAX);
            vi->refs++;
            break;
        }
    }
    return vi;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** Finds an existing VDPAU instance for the given X11 display and screen.
 * If not found, (try to) create a new one.
 * @param display_name X11 display string, NULL for default
 * @param snum X11 screen number, strictly negative for default
 **/
VdpStatus vdp_get_x11(const char *display_name, int snum,
                      vdp_t **restrict vdpp, VdpDevice *restrict devicep)
{
    vdp_instance_t *vi, *vi2;
    VdpStatus err = VDP_STATUS_RESOURCES;

    if (display_name == NULL)
    {
        display_name = getenv("DISPLAY");
        if (display_name == NULL)
            return VDP_STATUS_ERROR;
    }

    pthread_mutex_lock(&lock);
    vi = vdp_instance_lookup(display_name, snum);
    pthread_mutex_unlock(&lock);
    if (vi != NULL)
        goto found;

    err = vdp_instance_create(display_name, snum, &vi);
    if (err != VDP_STATUS_OK)
        return err;

    pthread_mutex_lock(&lock);
    vi2 = vdp_instance_lookup(display_name, snum);
    if (unlikely(vi2 != NULL))
    {   /* Another thread created the instance (race condition corner case) */
        pthread_mutex_unlock(&lock);
        vdp_instance_destroy(vi);
        vi = vi2;
    }
    else
    {
        vi->next = list;
        list = vi;
        pthread_mutex_unlock(&lock);
    }
found:
    *vdpp = vi->vdp;
    *devicep = vi->device;
    return VDP_STATUS_OK;
}

vdp_t *vdp_hold_x11(vdp_t *vdp, VdpDevice *restrict devp)
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

    assert(vi->refs < UINTPTR_MAX);
    vi->refs++;
    pthread_mutex_unlock(&lock);

    if (devp != NULL)
        *devp = vi->device;
   return vdp;
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
