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
typedef ANativeWindow* (*ptr_ANativeWindow_fromSurfaceTexture)(JNIEnv*, jobject);
typedef void (*ptr_ANativeWindow_release)(ANativeWindow*);

struct AWindowHandler
{
    JavaVM *p_jvm;
    jobject jobj;
    vout_window_t *wnd;

    struct {
        jobject jsurface;
        ANativeWindow *p_anw;
    } views[AWindow_Max];

    void *p_anw_dl;
    ptr_ANativeWindow_fromSurface pf_winFromSurface;
    ptr_ANativeWindow_release pf_winRelease;
    native_window_api_t anw_api;
    native_window_priv_api_t anwpriv_api;

    struct {
        awh_events_t cb;
    } event;
    bool b_has_video_layout_listener;

    struct {
        jfloatArray jtransform_mtx_array;
        jfloat *jtransform_mtx;
    } stex;
};

struct SurfaceTexture
{
    JavaVM *p_jvm;

    void *p_anw_dl;
    ptr_ANativeWindow_fromSurface pf_winFromSurface;
    ptr_ANativeWindow_release pf_winRelease;

    jobject thiz;
    jobject jsurface;
    ANativeWindow *p_anw;

};

static struct
{
    struct {
        jclass clazz;
        jmethodID getVideoSurface;
        jmethodID getSubtitlesSurface;
        jmethodID registerNative;
        jmethodID unregisterNative;
        jmethodID setBuffersGeometry;
        jmethodID setVideoLayout;
    } AndroidNativeWindow;
    struct {
        jmethodID attachToGLContext;
        jmethodID detachFromGLContext;
        jmethodID waitAndUpdateTexImage;
        jmethodID getSurface;
    } SurfaceTexture;
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
    intptr_t p_surface_handle = 0;

    clz = (*p_env)->GetObjectClass(p_env, jsurf);
    if ((*p_env)->ExceptionCheck(p_env))
    {
        (*p_env)->ExceptionClear(p_env);
        return NULL;
    }
    fid = (*p_env)->GetFieldID(p_env, clz, "mSurface", "I");
    if (fid == NULL)
    {
        if ((*p_env)->ExceptionCheck(p_env))
            (*p_env)->ExceptionClear(p_env);
        fid = (*p_env)->GetFieldID(p_env, clz, "mNativeSurface", "I");
        if (fid == NULL)
        {
            if ((*p_env)->ExceptionCheck(p_env))
                (*p_env)->ExceptionClear(p_env);
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

    p_awh->pf_winFromSurface = dlsym(p_library, "ANativeWindow_fromSurface");
    p_awh->pf_winRelease = dlsym(p_library, "ANativeWindow_release");
    p_awh->anw_api.winLock = dlsym(p_library, "ANativeWindow_lock");
    p_awh->anw_api.unlockAndPost = dlsym(p_library, "ANativeWindow_unlockAndPost");
    p_awh->anw_api.setBuffersGeometry = dlsym(p_library, "ANativeWindow_setBuffersGeometry");

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

int
android_loadNativeWindowPrivApi(native_window_priv_api_t *native)
{
#define LOAD(symbol) do { \
if ((native->symbol = dlsym(RTLD_DEFAULT, "ANativeWindowPriv_" #symbol)) == NULL) \
    return -1; \
} while(0)
    LOAD(connect);
    LOAD(disconnect);
    LOAD(setUsage);
    LOAD(setBuffersGeometry);
    LOAD(getMinUndequeued);
    LOAD(getMaxBufferCount);
    LOAD(setBufferCount);
    LOAD(setCrop);
    LOAD(dequeue);
    LOAD(lock);
    LOAD(lockData);
    LOAD(unlockData);
    LOAD(queue);
    LOAD(cancel);
    LOAD(setOrientation);
    return 0;
#undef LOAD
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
InitJNIFields(JNIEnv *env, vlc_object_t *p_obj, jobject *jobj)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    int ret;
    jclass clazz;

    vlc_mutex_lock(&lock);

    if (i_init_state != -1)
        goto end;

#define CHECK_EXCEPTION(what, critical) do { \
    if( (*env)->ExceptionCheck(env) ) \
    { \
        msg_Err(p_obj, "%s failed", what); \
        (*env)->ExceptionClear(env); \
        if (critical) { \
            i_init_state = 0; \
            goto end; \
        } \
    } \
} while( 0 )
#define GET_METHOD(id, str, args, critical) do { \
    jfields.id = (*env)->GetMethodID(env, clazz, (str), (args)); \
    CHECK_EXCEPTION("GetMethodID("str")", critical); \
} while( 0 )

    clazz = (*env)->GetObjectClass(env, jobj);
    CHECK_EXCEPTION("AndroidNativeWindow clazz", true);
    GET_METHOD(AndroidNativeWindow.getVideoSurface,
               "getVideoSurface", "()Landroid/view/Surface;", true);
    GET_METHOD(AndroidNativeWindow.getSubtitlesSurface,
               "getSubtitlesSurface", "()Landroid/view/Surface;", true);
    GET_METHOD(AndroidNativeWindow.registerNative,
               "registerNative", "(J)I", true);
    GET_METHOD(AndroidNativeWindow.unregisterNative,
               "unregisterNative", "()V", true);
    GET_METHOD(AndroidNativeWindow.setBuffersGeometry,
               "setBuffersGeometry", "(Landroid/view/Surface;III)Z", true);
    GET_METHOD(AndroidNativeWindow.setVideoLayout,
               "setVideoLayout", "(IIIIII)V", true);

    GET_METHOD(SurfaceTexture.attachToGLContext,
               "SurfaceTexture_attachToGLContext", "(I)Z", true);
    GET_METHOD(SurfaceTexture.detachFromGLContext,
               "SurfaceTexture_detachFromGLContext", "()V", true);
    GET_METHOD(SurfaceTexture.waitAndUpdateTexImage,
               "SurfaceTexture_waitAndUpdateTexImage", "([F)Z",
               true);
    GET_METHOD(SurfaceTexture.getSurface,
               "SurfaceTexture_getSurface", "()Landroid/view/Surface;", true);

    if ((*env)->RegisterNatives(env, clazz, jni_callbacks, 2) < 0)
    {
        msg_Err(p_obj, "RegisterNatives failed");
        i_init_state = 0;
        goto end;
    }
    jfields.AndroidNativeWindow.clazz = (*env)->NewGlobalRef(env, clazz);
    (*env)->DeleteLocalRef(env, clazz);

#undef GET_METHOD
#undef CHECK_EXCEPTION

    i_init_state = 1;
end:
    ret = i_init_state == 1 ? VLC_SUCCESS : VLC_EGENERIC;
    if (ret)
        msg_Err(p_obj, "AndroidNativeWindow jni init failed" );
    vlc_mutex_unlock(&lock);
    return ret;
}

#define JNI_CALL(what, obj, method, ...) \
    (*p_env)->what(p_env, obj, jfields.method, ##__VA_ARGS__)
#define JNI_ANWCALL(what, method, ...) \
    (*p_env)->what(p_env, p_awh->jobj, jfields.AndroidNativeWindow.method, ##__VA_ARGS__)
#define JNI_STEXCALL(what, method, ...) \
    (*p_env)->what(p_env, p_awh->jobj, jfields.SurfaceTexture.method, ##__VA_ARGS__)

static JNIEnv*
AWindowHandler_getEnv(AWindowHandler *p_awh)
{
    return android_getEnvCommon(NULL, p_awh->p_jvm, "AWindowHandler");
}

AWindowHandler *
AWindowHandler_new(vout_window_t *wnd, awh_events_t *p_events)
{
#define AWINDOW_REGISTER_FLAGS_SUCCESS 0x1
#define AWINDOW_REGISTER_FLAGS_HAS_VIDEO_LAYOUT_LISTENER 0x2

    AWindowHandler *p_awh;
    JNIEnv *p_env;
    JavaVM *p_jvm = var_InheritAddress(wnd, "android-jvm");
    jobject jobj = var_InheritAddress(wnd, "drawable-androidwindow");

    if (!p_jvm || !jobj)
    {
        msg_Err(wnd, "libvlc_media_player options not set");
        return NULL;
    }

    p_env = android_getEnvCommon(NULL, p_jvm, "AWindowHandler");
    if (!p_env)
    {
        msg_Err(wnd, "can't get JNIEnv");
        return NULL;
    }

    if (InitJNIFields(p_env, VLC_OBJECT(wnd), jobj) != VLC_SUCCESS)
    {
        msg_Err(wnd, "InitJNIFields failed");
        return NULL;
    }
    msg_Dbg(wnd, "InitJNIFields success");

    p_awh = calloc(1, sizeof(AWindowHandler));
    if (!p_awh)
        return NULL;

    p_awh->p_jvm = p_jvm;
    p_awh->jobj = (*p_env)->NewGlobalRef(p_env, jobj);

    p_awh->wnd = wnd;
    p_awh->event.cb = *p_events;

    jfloatArray jarray = (*p_env)->NewFloatArray(p_env, 16);
    if ((*p_env)->ExceptionCheck(p_env))
    {
        (*p_env)->ExceptionClear(p_env);
        free(p_awh);
        return NULL;
    }
    p_awh->stex.jtransform_mtx_array = (*p_env)->NewGlobalRef(p_env, jarray);
    (*p_env)->DeleteLocalRef(p_env, jarray);
    p_awh->stex.jtransform_mtx = NULL;

    const jint flags = JNI_ANWCALL(CallIntMethod, registerNative,
                                   (jlong)(intptr_t)p_awh);
    if ((flags & AWINDOW_REGISTER_FLAGS_SUCCESS) == 0)
    {
        msg_Err(wnd, "AWindow already registered");
        (*p_env)->DeleteGlobalRef(p_env, p_awh->jobj);
        (*p_env)->DeleteGlobalRef(p_env, p_awh->stex.jtransform_mtx_array);
        free(p_awh);
        return NULL;
    }
    LoadNativeWindowAPI(p_awh);

    p_awh->b_has_video_layout_listener =
        flags & AWINDOW_REGISTER_FLAGS_HAS_VIDEO_LAYOUT_LISTENER;

    return p_awh;
}

static void
AWindowHandler_releaseANativeWindowEnv(AWindowHandler *p_awh, JNIEnv *p_env,
                                       enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    if (p_awh->views[id].p_anw)
    {
        p_awh->pf_winRelease(p_awh->views[id].p_anw);
        p_awh->views[id].p_anw = NULL;
    }

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
        JNI_ANWCALL(CallVoidMethod, unregisterNative);
        AWindowHandler_releaseANativeWindowEnv(p_awh, p_env, AWindow_Video);
        AWindowHandler_releaseANativeWindowEnv(p_awh, p_env, AWindow_Subtitles);
        (*p_env)->DeleteGlobalRef(p_env, p_awh->jobj);
    }

    if (p_awh->p_anw_dl)
        dlclose(p_awh->p_anw_dl);

    (*p_env)->DeleteGlobalRef(p_env, p_awh->stex.jtransform_mtx_array);
    free(p_awh);
}

native_window_api_t *
AWindowHandler_getANativeWindowAPI(AWindowHandler *p_awh)
{
    return &p_awh->anw_api;
}

static int
WindowHandler_NewSurfaceEnv(AWindowHandler *p_awh, JNIEnv *p_env,
                            enum AWindow_ID id)
{
    jobject jsurface;

    switch (id)
    {
        case AWindow_Video:
            jsurface = JNI_ANWCALL(CallObjectMethod, getVideoSurface);
            break;
        case AWindow_Subtitles:
            jsurface = JNI_ANWCALL(CallObjectMethod, getSubtitlesSurface);
            break;
        case AWindow_SurfaceTexture:
            jsurface = JNI_STEXCALL(CallObjectMethod, getSurface);
            break;
    }
    if (!jsurface)
        return VLC_EGENERIC;

    p_awh->views[id].jsurface = (*p_env)->NewGlobalRef(p_env, jsurface);
    (*p_env)->DeleteLocalRef(p_env, jsurface);
    return VLC_SUCCESS;
}

ANativeWindow *
AWindowHandler_getANativeWindow(AWindowHandler *p_awh, enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    JNIEnv *p_env;

    if (p_awh->views[id].p_anw)
        return p_awh->views[id].p_anw;

    p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return NULL;

    if (WindowHandler_NewSurfaceEnv(p_awh, p_env, id) != VLC_SUCCESS)
        return NULL;
    assert(p_awh->views[id].jsurface != NULL);

    p_awh->views[id].p_anw = p_awh->pf_winFromSurface(p_env,
                                                      p_awh->views[id].jsurface);
    return p_awh->views[id].p_anw;
}

jobject
AWindowHandler_getSurface(AWindowHandler *p_awh, enum AWindow_ID id)
{
    assert(id < AWindow_Max);

    if (p_awh->views[id].jsurface)
        return p_awh->views[id].jsurface;

    AWindowHandler_getANativeWindow(p_awh, id);
    return p_awh->views[id].jsurface;
}


void AWindowHandler_releaseANativeWindow(AWindowHandler *p_awh,
                                         enum AWindow_ID id)
{
    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
    if (p_env)
        AWindowHandler_releaseANativeWindowEnv(p_awh, p_env, id);
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

    p_awh->event.cb.on_new_mouse_coords(p_awh->wnd,
        & (struct awh_mouse_coords) { action, button, x, y });
}

static void
AndroidNativeWindow_onWindowSize(JNIEnv* env, jobject clazz, jlong handle,
                                 jint width, jint height)
{
    (void) env; (void) clazz;
    AWindowHandler *p_awh = jlong_AWindowHandler(handle);

    if (width >= 0 && height >= 0)
        p_awh->event.cb.on_new_window_size(p_awh->wnd, width, height);
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

    return JNI_ANWCALL(CallBooleanMethod, setBuffersGeometry,
                       jsurf, i_width, i_height, i_format) ? VLC_SUCCESS
                                                           : VLC_EGENERIC;
}

bool
AWindowHandler_canSetVideoLayout(AWindowHandler *p_awh)
{
    return p_awh->b_has_video_layout_listener;
}

int
AWindowHandler_setVideoLayout(AWindowHandler *p_awh,
                              int i_width, int i_height,
                              int i_visible_width, int i_visible_height,
                              int i_sar_num, int i_sar_den)
{
    assert(p_awh->b_has_video_layout_listener);
    JNIEnv *p_env = AWindowHandler_getEnv(p_awh);
    if (!p_env)
        return VLC_EGENERIC;

    JNI_ANWCALL(CallVoidMethod, setVideoLayout, i_width, i_height,
                i_visible_width,i_visible_height, i_sar_num, i_sar_den);
    return VLC_SUCCESS;
}

int
SurfaceTexture_attachToGLContext(AWindowHandler *p_awh, int tex_name)
{
    JNIEnv *p_env = android_getEnvCommon(NULL, p_awh->p_jvm, "SurfaceTexture");
    if (!p_env)
        return VLC_EGENERIC;

    return JNI_STEXCALL(CallBooleanMethod, attachToGLContext, tex_name) ?
           VLC_SUCCESS : VLC_EGENERIC;
}

void
SurfaceTexture_detachFromGLContext(AWindowHandler *p_awh)
{
    JNIEnv *p_env = android_getEnvCommon(NULL, p_awh->p_jvm, "SurfaceTexture");
    if (!p_env)
        return;

    JNI_STEXCALL(CallVoidMethod, detachFromGLContext);

    AWindowHandler_releaseANativeWindowEnv(p_awh, p_env, AWindow_SurfaceTexture);

    if (p_awh->stex.jtransform_mtx != NULL)
    {
        (*p_env)->ReleaseFloatArrayElements(p_env, p_awh->stex.jtransform_mtx_array,
                                            p_awh->stex.jtransform_mtx,
                                            JNI_ABORT);
        p_awh->stex.jtransform_mtx = NULL;
    }
}

int
SurfaceTexture_waitAndUpdateTexImage(AWindowHandler *p_awh,
                                     const float **pp_transform_mtx)
{
    JNIEnv *p_env = android_getEnvCommon(NULL, p_awh->p_jvm, "SurfaceTexture");
    if (!p_env)
        return VLC_EGENERIC;

    if (p_awh->stex.jtransform_mtx != NULL)
        (*p_env)->ReleaseFloatArrayElements(p_env, p_awh->stex.jtransform_mtx_array,
                                            p_awh->stex.jtransform_mtx,
                                            JNI_ABORT);

    bool ret = JNI_STEXCALL(CallBooleanMethod, waitAndUpdateTexImage,
                            p_awh->stex.jtransform_mtx_array);
    if (ret)
    {
        p_awh->stex.jtransform_mtx = (*p_env)->GetFloatArrayElements(p_env,
                                            p_awh->stex.jtransform_mtx_array, NULL);
        *pp_transform_mtx = p_awh->stex.jtransform_mtx;
        return VLC_SUCCESS;
    }
    else
    {
        p_awh->stex.jtransform_mtx = NULL;
        return VLC_EGENERIC;
    }
}
