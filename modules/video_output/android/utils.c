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

/*
 * Android Surface (pre android 2.3)
 */

extern void *jni_AndroidJavaSurfaceToNativeSurface(jobject surf);
#ifndef ANDROID_SYM_S_LOCK
# define ANDROID_SYM_S_LOCK "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb"
#endif
#ifndef ANDROID_SYM_S_LOCK2
# define ANDROID_SYM_S_LOCK2 "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE"
#endif
#ifndef ANDROID_SYM_S_UNLOCK
# define ANDROID_SYM_S_UNLOCK "_ZN7android7Surface13unlockAndPostEv"
#endif
typedef void (*AndroidSurface_lock)(void *, void *, int);
typedef void (*AndroidSurface_lock2)(void *, void *, void *);
typedef void (*AndroidSurface_unlockAndPost)(void *);

typedef struct {
    void *p_dl_handle;
    void *p_surface_handle;
    AndroidSurface_lock pf_lock;
    AndroidSurface_lock2 pf_lock2;
    AndroidSurface_unlockAndPost pf_unlockAndPost;
} NativeSurface;

static inline void *
NativeSurface_Load(const char *psz_lib, NativeSurface *p_ns)
{
    void *p_lib = dlopen(psz_lib, RTLD_NOW);
    if (!p_lib)
        return NULL;

    p_ns->pf_lock = (AndroidSurface_lock)(dlsym(p_lib, ANDROID_SYM_S_LOCK));
    p_ns->pf_lock2 = (AndroidSurface_lock2)(dlsym(p_lib, ANDROID_SYM_S_LOCK2));
    p_ns->pf_unlockAndPost =
        (AndroidSurface_unlockAndPost)(dlsym(p_lib, ANDROID_SYM_S_UNLOCK));

    if ((p_ns->pf_lock || p_ns->pf_lock2) && p_ns->pf_unlockAndPost)
        return p_lib;

    dlclose(p_lib);
    return NULL;
}


static ANativeWindow*
NativeSurface_fromSurface(JNIEnv *env, jobject jsurf)
{
    (void) env;
    void *p_surface_handle;
    NativeSurface *p_ns;

    static const char *libs[] = {
        "libsurfaceflinger_client.so",
        "libgui.so",
        "libui.so"
    };
    p_surface_handle = jni_AndroidJavaSurfaceToNativeSurface(jsurf);
    if (!p_surface_handle)
        return NULL;
    p_ns = malloc(sizeof(NativeSurface));
    if (!p_ns)
        return NULL;
    p_ns->p_surface_handle = p_surface_handle;

    for (size_t i = 0; i < sizeof(libs) / sizeof(*libs); i++)
    {
        void *p_dl_handle = NativeSurface_Load(libs[i], p_ns);
        if (p_dl_handle)
        {
            p_ns->p_dl_handle = p_dl_handle;
            return (ANativeWindow*)p_ns;
        }
    }
    free(p_ns);
    return NULL;
}

static void
NativeSurface_release(ANativeWindow* p_anw)
{
    NativeSurface *p_ns = (NativeSurface *)p_anw;

    dlclose(p_ns->p_dl_handle);
    free(p_ns);
}

static int32_t
NativeSurface_lock(ANativeWindow *p_anw, ANativeWindow_Buffer *p_anb,
                   ARect *p_rect)
{
    (void) p_rect;
    NativeSurface *p_ns = (NativeSurface *)p_anw;
    struct {
        uint32_t    w;
        uint32_t    h;
        uint32_t    s;
        uint32_t    usage;
        uint32_t    format;
        uint32_t*   bits;
        uint32_t    reserved[2];
    } info = { 0 };

    if (p_ns->pf_lock)
        p_ns->pf_lock(p_ns->p_surface_handle, &info, 1);
    else
        p_ns->pf_lock2(p_ns->p_surface_handle, &info, NULL);

    if (!info.w || !info.h) {
        p_ns->pf_unlockAndPost(p_ns->p_surface_handle);
        return -1;
    }

    if (p_anb) {
        p_anb->bits = info.bits;
        p_anb->width = info.w;
        p_anb->height = info.h;
        p_anb->stride = info.s;
        p_anb->format = info.format;
    }
    return 0;
}

static void
NativeSurface_unlockAndPost(ANativeWindow *p_anw)
{
    NativeSurface *p_ns = (NativeSurface *)p_anw;

    p_ns->pf_unlockAndPost(p_ns->p_surface_handle);
}

void LoadNativeSurfaceAPI(native_window_api_t *native)
{
    native->winFromSurface = NativeSurface_fromSurface;
    native->winRelease = NativeSurface_release;
    native->winLock = NativeSurface_lock;
    native->unlockAndPost = NativeSurface_unlockAndPost;
    native->setBuffersGeometry = NULL;
}

/*
 * Android NativeWindow (post android 2.3)
 */

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

/*
 * Android private NativeWindow (post android 2.3)
 */

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
