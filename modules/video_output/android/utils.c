/*****************************************************************************
 * utils.c: shared code between Android vout modules.
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
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
#include "utils.h"
#include <dlfcn.h>

void *LoadNativeWindowAPI(native_window_api_t *native)
{
    void *p_library = dlopen("libandroid.so", RTLD_NOW);
    if (!p_library)
        return NULL;

    native->winFromSurface =
        (ptr_ANativeWindow_fromSurface)(dlsym(p_library, "ANativeWindow_fromSurface"));
    native->winRelease =
        (ptr_ANativeWindow_release)(dlsym(p_library, "ANativeWindow_release"));
    native->winLock =
        (ptr_ANativeWindow_lock)(dlsym(p_library, "ANativeWindow_lock"));
    native->unlockAndPost =
        (ptr_ANativeWindow_unlockAndPost)(dlsym(p_library, "ANativeWindow_unlockAndPost"));
    native->setBuffersGeometry =
        (ptr_ANativeWindow_setBuffersGeometry)(dlsym(p_library, "ANativeWindow_setBuffersGeometry"));

    if (native->winFromSurface && native->winRelease && native->winLock
        && native->unlockAndPost && native->setBuffersGeometry)
        return p_library;

    native->winFromSurface = NULL;
    native->winRelease = NULL;
    native->winLock = NULL;
    native->unlockAndPost = NULL;
    native->setBuffersGeometry = NULL;

    dlclose(p_library);
    return NULL;
}

int LoadNativeWindowPrivAPI(native_window_priv_api_t *native)
{
    native->connect = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_connect");
    native->disconnect = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_disconnect");
    native->setUsage = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_setUsage");
    native->setBuffersGeometry = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_setBuffersGeometry");
    native->getMinUndequeued = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_getMinUndequeued");
    native->getMaxBufferCount = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_getMaxBufferCount");
    native->setBufferCount = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_setBufferCount");
    native->setCrop = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_setCrop");
    native->dequeue = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_dequeue");
    native->lock = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_lock");
    native->lockData = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_lockData");
    native->unlockData = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_unlockData");
    native->queue = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_queue");
    native->cancel = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_cancel");
    native->setOrientation = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_setOrientation");

    return native->connect && native->disconnect && native->setUsage &&
        native->setBuffersGeometry && native->getMinUndequeued &&
        native->getMaxBufferCount && native->setBufferCount && native->setCrop &&
        native->dequeue && native->lock && native->lockData && native->unlockData &&
        native->queue && native->cancel && native->setOrientation ? 0 : -1;
}

extern void jni_getMouseCoordinates(int *, int *, int *, int *);

void Manage(vout_display_t *vd)
{
    int x, y, button, action;
    jni_getMouseCoordinates(&action, &button, &x, &y);
    if (x >= 0 && y >= 0)
    {
        switch( action )
        {
            case AMOTION_EVENT_ACTION_DOWN:
                vout_display_SendEventMouseMoved(vd, x, y);
                vout_display_SendEventMousePressed(vd, button); break;
            case AMOTION_EVENT_ACTION_UP:
                vout_display_SendEventMouseMoved(vd, x, y);
                vout_display_SendEventMouseReleased(vd, button); break;
            case AMOTION_EVENT_ACTION_MOVE:
                vout_display_SendEventMouseMoved(vd, x, y); break;
        }
    }
}

