/*****************************************************************************
 * instance.c: VAAPI instance management for VLC
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Author: Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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
# include "config.h"
#endif

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <vlc_common.h>
#include "vlc_vaapi.h"

#pragma GCC visibility push(default)

static struct
{
    pthread_mutex_t     lock;
    VADisplay           dpy;
    unsigned            refcount;
} va_instance = { PTHREAD_MUTEX_INITIALIZER, NULL, 0 };

/* Set the VA instance and sets the reference counter to 1. */
int
vlc_vaapi_SetInstance(VADisplay dpy)
{
    pthread_mutex_lock(&va_instance.lock);
    if (va_instance.refcount != 0)
    {
        vaTerminate(dpy);
        pthread_mutex_unlock(&va_instance.lock);
        return VLC_EGENERIC;
    }
    va_instance.refcount = 1;
    va_instance.dpy = dpy;
    pthread_mutex_unlock(&va_instance.lock);
    return VLC_SUCCESS;
}

/* Retrieve the VA instance and increases the reference counter by 1. */
VADisplay
vlc_vaapi_GetInstance(void)
{
    VADisplay dpy;
    pthread_mutex_lock(&va_instance.lock);
    if (!va_instance.dpy)
        dpy = NULL;
    else
    {
        dpy = va_instance.dpy;
        ++va_instance.refcount;
    }
    pthread_mutex_unlock(&va_instance.lock);
    return dpy;
}

/* Decreases the reference counter by 1 and frees the instance if that counter
   reaches 0. */
void
vlc_vaapi_ReleaseInstance(VADisplay *dpy)
{
    assert(va_instance.dpy == dpy && va_instance.refcount > 0);
    (void) dpy;

    pthread_mutex_lock(&va_instance.lock);
    if (--va_instance.refcount == 0)
    {
        vaTerminate(va_instance.dpy);
        va_instance.dpy = NULL;
    }
    pthread_mutex_unlock(&va_instance.lock);
}

#pragma GCC visibility pop
