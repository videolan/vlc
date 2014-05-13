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

    if (native->winFromSurface && native->winRelease && native->winLock && native->unlockAndPost)
        return p_library;

    native->winFromSurface = NULL;
    native->winRelease = NULL;
    native->winLock = NULL;
    native->unlockAndPost = NULL;

    dlclose(p_library);
    return NULL;
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

