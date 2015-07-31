/*****************************************************************************
 * utils.c: shared code between Android vout modules.
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
 *          Thomas Guillem <thomas@gllm.fr>
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
#include <jni.h>
#include <pthread.h>
#include <assert.h>

typedef ANativeWindow* (*ptr_ANativeWindow_fromSurface)(JNIEnv*, jobject);
typedef void (*ptr_ANativeWindow_release)(ANativeWindow*);

struct AWindowHandler
{
    JavaVM *p_jvm;
    jobject jobj;

    struct {
        jobject jsurface;
        ANativeWindow *p_anw;
    } views[AWindow_Max];

    void *p_anw_dl;
    ptr_ANativeWindow_fromSurface pf_winFromSurface;
    ptr_ANativeWindow_release pf_winRelease;
    native_window_api_t anw_api;
    native_window_priv_api_t anwpriv_api;

    vlc_mutex_t lock;
    struct {
        bool b_registered;
        struct {
            int i_action, i_button, i_x, i_y;
        } mouse;
        struct {
            int i_width, i_height;
        } window;
    } event;
};

static struct
{
    struct {
        jmethodID getVideoSurface;
        jmethodID getSubtitlesSurface;
        jmethodID setCallback;
        jmethodID setBuffersGeometry;
        jmethodID setWindowLayout;
    } AndroidNativeWindow;
} jfields;

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

static void *
NativeSurface_getHandle(JNIEnv *p_env, jobject jsurf)
{
    jclass clz;
    jfieldID fid;
    intptr_t p_surface_handle = NULL;

    clz = (*p_env)->GetObjectClass(p_env, jsurf);
    if ((*p_env)->ExceptionOccurred(p_env))
    {
        (*p_env)->ExceptionClear(p_env);
        return NULL;
    }
    fid = (*p_env)->GetFieldID(p_env, clz, "mSurface", "I");
    if (fid == NULL)
    {
        jthrowable exp = (*p_env)->ExceptionOccurred(p_env);
        if (exp)
        {
            (*p_env)->DeleteLocalRef(p_env, exp);
            (*p_env)->ExceptionClear(p_env);
        }
        fid = (*p_env)->GetFieldID(p_env, clz, "mNativeSurface", "I");
        if (fid == NULL)
        {
            jthrowable exp = (*p_env)->ExceptionOccurred(p_env);
            if (exp)
            {
                (*p_env)->DeleteLocalRef(p_env, exp);
                (*p_env)->ExceptionClear(p_env);
            }
        }
    }
    if (fid != NULL)
        p_surface_handle = (intptr_t)(*p_env)->GetIntField(p_env, jsurf, fid);
    (*p_env)->DeleteLocalRef(p_env, clz);

    return (void *)p_surface_handle;
}


static ANativeWindow*
NativeSurface_fromSurface(JNIEnv *p_env, jobject jsurf)
{
    void *p_surface_handle;
    NativeSurface *p_ns;

    static const char *libs[] = {
        "libsurfaceflinger_client.so",
        "libgui.so",
        "libui.so"
    };
    p_surface_handle = NativeSurface_getHandle(p_env, jsurf);
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

static void
LoadNativeSurfaceAPI(AWindowHandler *p_awh)
{
    p_awh->pf_winFromSurface = NativeSurface_fromSurface;
    p_awh->pf_winRelease = NativeSurface_release;
    p_awh->anw_api.winLock = NativeSurface_lock;
    p_awh->anw_api.unlockAndPost = NativeSurface_unlockAndPost;
    p_awh->anw_api.setBuffersGeometry = NULL;
}

/*
 * Android NativeWindow (post android 2.3)
 */

static void
LoadNativeWindowAPI(AWindowHandler *p_awh)
{
    void *p_library = dlopen("libandroid.so", RTLD_NOW);
    if (!p_library)
    {
        LoadNativeSurfaceAPI(p_awh);
        return;
    }

    p_awh->pf_winFromSurface =
        (ptr_ANativeWindow_fromSurface)(dlsym(p_library, "ANativeWindow_fromSurface"));
    p_awh->pf_winRelease =
        (ptr_ANativeWindow_release)(dlsym(p_library, "ANativeWindow_release"));
    p_awh->anw_api.winLock =
        (ptr_ANativeWindow_lock)(dlsym(p_library, "ANativeWindow_lock"));
    p_awh->anw_api.unlockAndPost =
        (ptr_ANativeWindow_unlockAndPost)(dlsym(p_library, "ANativeWindow_unlockAndPost"));
    p_awh->anw_api.setBuffersGeometry =
        (ptr_ANativeWindow_setBuffersGeometry)(dlsym(p_library, "ANativeWindow_setBuffersGeometry"));

    if (p_awh->pf_winFromSurface && p_awh->pf_winRelease
     && p_awh->anw_api.winLock && p_awh->anw_api.unlockAndPost
     && p_awh->anw_api.setBuffersGeometry)
    {
        p_awh->p_anw_dl = p_library;
    }
    else
    {
        dlclose(p_library);
        LoadNativeSurfaceAPI(p_awh);
    }
}

/*
 * Android private NativeWindow (post android 2.3)
 */

static int
LoadNativeWindowPrivAPI(native_window_priv_api_t *native)
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

/*
 * Andoid JNIEnv helper
 */

static pthread_key_t jni_env_key;
static pthread_once_t jni_env_key_once = PTHREAD_ONCE_INIT;

/* This function is called when a thread attached to the Java VM is canceled or
 * exited */
static void
jni_detach_thread(void *data)
{
    JNIEnv *env = data;
    JavaVM *jvm;

    (*env)->GetJavaVM(env, &jvm);
    assert(jvm);
    (*jvm)->DetachCurrentThread(jvm);
}

static void jni_env_key_create()
{
    /* Create a TSD area and setup a destroy callback when a thread that
     * previously set the jni_env_key is canceled or exited */
    pthread_key_create(&jni_env_key, jni_detach_thread);
}

static JNIEnv *
android_getEnvCommon(vlc_object_t *p_obj, JavaVM *jvm, const char *psz_name)
{
    assert((p_obj && !jvm) || (!p_obj && jvm));

    JNIEnv *env;

    pthread_once(&jni_env_key_once, jni_env_key_create);
    env = pthread_getspecific(jni_env_key);
    if (env == NULL)
    {
        if (!jvm)
            jvm = var_InheritAddress(p_obj, "android-jvm");

        if (!jvm)
            return NULL;

        /* if GetEnv returns JNI_OK, the thread is already attached to the
         * JavaVM, so we are already in a java thread, and we don't have to
         * setup any destroy callbacks */
        if ((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_2) != JNI_OK)
        {
            /* attach the thread to the Java VM */
            JavaVMAttachArgs args;

            args.version = JNI_VERSION_1_2;
            args.name = psz_name;
            args.group = NULL;

            if ((*jvm)->AttachCurrentThread(jvm, &env, &args) != JNI_OK)
                return NULL;

            /* Set the attached env to the thread-specific data area (TSD) */
            if (pthread_setspecific(jni_env_key, env) != 0)
            {
                (*jvm)->DetachCurrentThread(jvm);
                return NULL;
            }
        }
    }

    return env;
}

JNIEnv *
android_getEnv(vlc_object_t *p_obj, const char *psz_name)
{
    return android_getEnvCommon(p_obj, NULL, psz_name);
}

static void
AndroidNativeWindow_onMouseEvent(JNIEnv*, jobject, jlong, jint, jint, jint, jint);
static void
AndroidNativeWindow_onWindowSize(JNIEnv*, jobject, jlong, jint, jint );

const JNINativeMethod jni_callbacks[] = {
    { "nativeOnMouseEvent", "(JIIII)V",
        (void *)AndroidNativeWindow_onMouseEvent },
    { "nativeOnWindowSize", "(JII)V",
        (void *)AndroidNativeWindow_onWindowSize },
};

static int
InitJNIFields(JNIEnv *env, vlc_object_t *p_obj, AWindowHandler *p_awh)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    int ret;
    jclass clazz;

    vlc_mutex_lock(&lock);

    if (i_init_state != -1)
        goto end;

#define CHECK_EXCEPTION(what) do { \
    if( (*env)->ExceptionOccurred(env) ) \
    { \
        msg_Err(p_obj, "%s failed", what); \
        (*env)->ExceptionClear(env); \
        i_init_state = 0; \
        goto end; \
    } \
} while( 0 )
#define GET_METHOD(id, str, args) do { \
    jfields.AndroidNativeWindow.id = (*env)->GetMethodID(env, clazz, (str), (args)); \
    CHECK_EXCEPTION("GetMethodID("str")"); \
} while( 0 )

    clazz = (*env)->GetObjectClass(env, p_awh);
    CHECK_EXCEPTION("AndroidNativeWindow clazz");
    GET_METHOD(getVideoSurface, "getVideoSurface", "()Landroid/view/Surface;");
    GET_METHOD(getSubtitlesSurface, "getSubtitlesSurface", "()Landroid/view/Surface;");
    GET_METHOD(setCallback, "setCallback", "(J)Z");
    GET_METHOD(setBuffersGeometry, "setBuffersGeometry", "(Landroid/view/Surface;III)Z");
    GET_METHOD(setWindowLayout, "setWindowLayout", "(IIIIII)V");
#undef CHECK_EXCEPTION
#undef GET_METHOD

    if ((*env)->RegisterNatives(env, clazz, jni_callbacks, 2) < 0)
    {
        msg_Err(p_obj, "RegisterNatives failed");
        i_init_state = 0;
        goto end;
    }
    (*env)->DeleteLocalRef(env, clazz);

    i_init_state = 1;
    msg_Dbg(p_obj, "InitJNIFields success");
end:
    ret = i_init_state == 1 ? VLC_SUCCESS : VLC_EGENERIC;
    if (ret)
        msg_Err(p_obj, "AndroidNativeWindow jni init failed" );
    vlc_mutex_unlock(&lock);
    return ret;
}

#define JNI_CALL(what, method, ...) \
    (*p_env)->what(p_env, p_awh->jobj, jfields.AndroidNativeWindow.method, ##__VA_ARGS__)

static JNIEnv*
AWindowHandler_getEnv(AWindowHandler *p_awh)
{
    return android_getEnvCommon(NULL, p_awh->p_jvm, "AWindowHandler");
}

AWindowHandler *
AWindowHandler_new(vlc_object_t *p_obj)
{
    AWindowHandler *p_awh;
    JNIEnv *p_env;
    JavaVM *p_jvm = var_InheritAddress(p_obj, "android-jvm");
    jobject jobj = var_InheritAddress(p_obj, "drawable-androidwindow");

    if (!p_jvm || !jobj)
    {
        msg_Err(p_obj, "libvlc_media_player options not set");
        return NULL;
    }

    p_env = android_getEnvCommon(NULL, p_jvm, "AWindowHandler");
    if (!p_env)
    {
        msg_Err(p_obj, "can't get JNIEnv");
        return NULL;
    }

    if (InitJNIFields(p_env, p_obj, jobj) != VLC_SUCCESS)
    {
        msg_Err(p_obj, "InitJNIFields failed");
        return NULL;
    }
    p_awh = calloc(1, sizeof(AWindowHandler));
    if (!p_awh)
        return NULL;
    p_awh->p_jvm = p_jvm;
    p_awh->jobj = (*p_env)->NewGlobalRef(p_env, jobj);
    LoadNativeWindowAPI(p_awh);
    vlc_mutex_init(&p_awh->lock);
    p_awh->event.mouse.i_action = p_awh->event.mouse.i_button =
    p_awh->event.mouse.i_x = p_awh->event.mouse.i_y = -1;
    p_awh->event.window.i_width = p_awh->event.window.i_height = -1;

    return p_awh;
}

static void
AWindowHandler_releaseSurfaceEnv(AWindowHandler *p_awh, JNIEnv *p_env,
                                 enum AWindow_ID id)
{
    AWindowHandler_releaseANativeWindow(p_awh, id);
    if (p_awh->views[id].jsurface)
    {
        (*p_env)->DeleteGlobalRef(p_env, p_awh->views[id].jsurface);
        p_awh->views[id].jsurface = NULL;
    }
}

void
AWindowHandler_destroy(AWindowHandler *p_awh)
{
    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);

    if (p_env)
    {
        if (p_awh->event.b_registered)
            JNI_CALL(CallBooleanMethod, setCallback, (jlong)0LL);
        AWindowHandler_releaseSurfaceEnv(p_awh, p_env, AWindow_Video);
        AWindowHandler_releaseSurfaceEnv(p_awh, p_env, AWindow_Subtitles);
        (*p_env)->DeleteGlobalRef(p_env, p_awh->jobj);
    }

    vlc_mutex_destroy(&p_awh->lock);

    if (p_awh->p_anw_dl)
        dlclose(p_awh->p_anw_dl);

    free(p_awh);
}

native_window_api_t *
AWindowHandler_getANativeWindowAPI(AWindowHandler *p_awh)
{
    return &p_awh->anw_api;
}

native_window_priv_api_t *
AWindowHandler_getANativeWindowPrivAPI(AWindowHandler *p_awh)
{
    if (LoadNativeWindowPrivAPI(&p_awh->anwpriv_api) != 0)
        return NULL;
    else
        return &p_awh->anwpriv_api;
}

jobject
AWindowHandler_getSurface(AWindowHandler *p_awh, enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    jobject jsurface;
    JNIEnv *p_env;

    if (p_awh->views[id].jsurface)
        return p_awh->views[id].jsurface;

    p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return NULL;

    if (id == AWindow_Video)
        jsurface = JNI_CALL(CallObjectMethod, getVideoSurface);
    else
        jsurface = JNI_CALL(CallObjectMethod, getSubtitlesSurface);
    if (!jsurface)
        return NULL;

    p_awh->views[id].jsurface = (*p_env)->NewGlobalRef(p_env, jsurface);
    (*p_env)->DeleteLocalRef(p_env, jsurface);
    return p_awh->views[id].jsurface;
}

void
AWindowHandler_releaseSurface(AWindowHandler *p_awh, enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
    if (p_env)
        AWindowHandler_releaseSurfaceEnv(p_awh, p_env, id);
}

ANativeWindow *
AWindowHandler_getANativeWindow(AWindowHandler *p_awh, enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    jobject jsurf;
    JNIEnv *p_env;

    if (p_awh->views[id].p_anw)
        return p_awh->views[id].p_anw;

    p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return NULL;

    jsurf = AWindowHandler_getSurface(p_awh, id);
    if (!jsurf)
        return NULL;

    p_awh->views[id].p_anw = p_awh->pf_winFromSurface(p_env, jsurf);
    return p_awh->views[id].p_anw;
}

void AWindowHandler_releaseANativeWindow(AWindowHandler *p_awh,
                                         enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    if (p_awh->views[id].p_anw)
    {
        p_awh->pf_winRelease(p_awh->views[id].p_anw);
        p_awh->views[id].p_anw = NULL;
    }
}

static inline AWindowHandler *jlong_AWindowHandler(jlong handle)
{
    return (AWindowHandler *)(intptr_t) handle;
}

static void
AndroidNativeWindow_onMouseEvent(JNIEnv* env, jobject clazz, jlong handle,
                                 jint action, jint button, jint x, jint y)
{
    (void) env; (void) clazz;
    AWindowHandler *p_awh = jlong_AWindowHandler(handle);

    vlc_mutex_lock(&p_awh->lock);
    p_awh->event.mouse.i_action = action;
    p_awh->event.mouse.i_button = button;
    p_awh->event.mouse.i_x = x;
    p_awh->event.mouse.i_y = y;
    vlc_mutex_unlock(&p_awh->lock);
}

static void
AndroidNativeWindow_onWindowSize(JNIEnv* env, jobject clazz, jlong handle,
                                 jint width, jint height)
{
    (void) env; (void) clazz;
    AWindowHandler *p_awh = jlong_AWindowHandler(handle);

    vlc_mutex_lock(&p_awh->lock);
    p_awh->event.window.i_width = width;
    p_awh->event.window.i_height = height;
    vlc_mutex_unlock(&p_awh->lock);
}

static bool
AWindowHandler_registerCallback(AWindowHandler *p_awh)
{
    if (!p_awh->event.b_registered)
    {
        JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
        if (!p_env)
            return false;
        p_awh->event.b_registered = JNI_CALL(CallBooleanMethod,
                                             setCallback,
                                             (jlong)(intptr_t)p_awh);
    }
    return p_awh->event.b_registered;
}

bool
AWindowHandler_getMouseCoordinates(AWindowHandler *p_awh,
                                   int *p_action, int *p_button,
                                   int *p_x, int *p_y)
{
    if (!AWindowHandler_registerCallback(p_awh))
        return false;

    vlc_mutex_lock(&p_awh->lock);
    if (p_awh->event.mouse.i_action == -1
     || p_awh->event.mouse.i_button == -1
     || p_awh->event.mouse.i_x <= 0 || p_awh->event.mouse.i_y <= 0)
    {
        vlc_mutex_unlock(&p_awh->lock);
        return false;
    }
    *p_action = p_awh->event.mouse.i_action;
    *p_button = p_awh->event.mouse.i_button;
    *p_x = p_awh->event.mouse.i_x;
    *p_y = p_awh->event.mouse.i_y;
    vlc_mutex_unlock(&p_awh->lock);
    return true;
}

bool
AWindowHandler_getWindowSize(AWindowHandler *p_awh,
                             int *p_width, int *p_height)
{
    if (!AWindowHandler_registerCallback(p_awh))
        return false;

    vlc_mutex_lock(&p_awh->lock);
    if (p_awh->event.window.i_width <= 0
     || p_awh->event.window.i_height <= 0)
    {
        vlc_mutex_unlock(&p_awh->lock);
        return false;
    }
    *p_width = p_awh->event.window.i_width;
    *p_height = p_awh->event.window.i_height;
    vlc_mutex_unlock(&p_awh->lock);
    return true;
}

int
AWindowHandler_setBuffersGeometry(AWindowHandler *p_awh, enum AWindow_ID id,
                                  int i_width, int i_height, int i_format)
{
    jobject jsurf;
    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return VLC_EGENERIC;

    jsurf = AWindowHandler_getSurface(p_awh, id);
    if (!jsurf)
        return VLC_EGENERIC;

    return JNI_CALL(CallBooleanMethod, setBuffersGeometry,
                    jsurf, i_width, i_height, i_format) ? VLC_SUCCESS
                                                        : VLC_EGENERIC;
}

int
AWindowHandler_setWindowLayout(AWindowHandler *p_awh,
                               int i_width, int i_height,
                               int i_visible_width, int i_visible_height,
                               int i_sar_num, int i_sar_den)
{
    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return VLC_EGENERIC;

    JNI_CALL(CallVoidMethod, setWindowLayout, i_width, i_height,
             i_visible_width,i_visible_height, i_sar_num, i_sar_den);
    return VLC_SUCCESS;
}
