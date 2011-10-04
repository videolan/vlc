/*****************************************************************************
 * androidsurface.c: android video output using Surface Flinger
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Ming Hu <tewilove@gmail.com>
 *          Ludovic Fauvet <etix@l0cal.com>
 *          Sébastien Toque <xilasz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include <dlfcn.h>

#ifndef ANDROID_SYM_S_LOCK
# define ANDROID_SYM_S_LOCK "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb"
#endif
#ifndef ANDROID_SYM_S_UNLOCK
# define ANDROID_SYM_S_UNLOCK "_ZN7android7Surface13unlockAndPostEv"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("AndroidSurface")
    set_description(N_("Android Surface video output"))
    set_capability("vout display", 155)
    add_shortcut("androidsurface", "android")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * JNI prototypes
 *****************************************************************************/

extern void *jni_LockAndGetAndroidSurface();
extern void  jni_UnlockAndroidSurface();
extern void  jni_SetAndroidSurfaceSize(int width, int height);

// _ZN7android7Surface4lockEPNS0_11SurfaceInfoEb
typedef void (*Surface_lock)(void *, void *, int);
// _ZN7android7Surface13unlockAndPostEv
typedef void (*Surface_unlockAndPost)(void *);

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Display(vout_display_t *, picture_t *, subpicture_t *);
static int              Control(vout_display_t *, int, va_list);

/* */
struct vout_display_sys_t {
    picture_pool_t *pool;
    void *p_library;
    Surface_lock s_lock;
    Surface_unlockAndPost s_unlockAndPost;

    picture_resource_t resource;
};

/* */
typedef struct _SurfaceInfo {
    uint32_t    w;
    uint32_t    h;
    uint32_t    s;
    uint32_t    usage;
    uint32_t    format;
    uint32_t*   bits;
    uint32_t    reserved[2];
} SurfaceInfo;

struct picture_sys_t
{
    void *surf;
    SurfaceInfo info;
    vout_display_sys_t *sys;
};

static int  AndroidLockSurface(picture_t *);
static void AndroidUnlockSurface(picture_t *);

static vlc_mutex_t single_instance = VLC_STATIC_MUTEX;

static inline void *LoadSurface(const char *psz_lib, vout_display_sys_t *sys) {
    void *p_library = dlopen(psz_lib, RTLD_NOW);
    if (p_library) {
        sys->s_lock = (Surface_lock)(dlsym(p_library, ANDROID_SYM_S_LOCK));
        sys->s_unlockAndPost =
            (Surface_unlockAndPost)(dlsym(p_library, ANDROID_SYM_S_UNLOCK));
        if (sys->s_lock && sys->s_unlockAndPost) {
            return p_library;
        }
        dlclose(p_library);
    }
    return NULL;
}

static void *InitLibrary(vout_display_sys_t *sys) {
    void *p_library;
    if ((p_library = LoadSurface("libsurfaceflinger_client.so", sys)))
        return p_library;
    return LoadSurface("libui.so", sys);
}

static int Open(vlc_object_t *p_this) {
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys;
    void *p_library;

    /* */
    if (vlc_mutex_trylock(&single_instance) != 0) {
        msg_Err(vd, "Can't start more than one instance at a time");
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    sys = (struct vout_display_sys_t*) calloc(1, sizeof(*sys));
    if (!sys) {
        vlc_mutex_unlock(&single_instance);
        return VLC_ENOMEM;
    }

    /* */
    sys->p_library = p_library = InitLibrary(sys);
    if (!p_library) {
        free(sys);
        msg_Err(vd, "Could not initialize libui.so/libsurfaceflinger_client.so!");
        vlc_mutex_unlock(&single_instance);
        return VLC_EGENERIC;
    }

    /* Setup chroma */
    video_format_t fmt = vd->fmt;
    fmt.i_chroma = VLC_CODEC_RGB32;
    fmt.i_rmask  = 0x000000ff;
    fmt.i_gmask  = 0x0000ff00;
    fmt.i_bmask  = 0x00ff0000;
    video_format_FixRgb(&fmt);

    /* Create the associated picture */
    picture_resource_t *rsc = &sys->resource;
    rsc->p_sys = malloc(sizeof(*rsc->p_sys));
    if (!rsc->p_sys)
        goto enomem;
    rsc->p_sys->sys = sys;

    for (int i = 0; i < PICTURE_PLANE_MAX; i++) {
        rsc->p[i].p_pixels = NULL;
        rsc->p[i].i_pitch = 0;
        rsc->p[i].i_lines = 0;
    }
    picture_t *picture = picture_NewFromResource(&fmt, rsc);
    if (!picture)
        goto enomem;

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = AndroidLockSurface;
    pool_cfg.unlock        = AndroidUnlockSurface;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool) {
        picture_Release(picture);
        goto enomem;
    }

    /* Setup vout_display */
    vd->sys     = sys;
    vd->fmt     = fmt;
    vd->pool    = Pool;
    vd->display = Display;
    vd->control = Control;
    vd->prepare = NULL;
    vd->manage  = NULL;

    /* Fix initial state */
    vout_display_SendEventFullscreen(vd, false);

    return VLC_SUCCESS;

enomem:
    free(rsc->p_sys);
    free(sys);
    dlclose(p_library);
    vlc_mutex_unlock(&single_instance);
    return VLC_ENOMEM;
}

static void Close(vlc_object_t *p_this) {
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    picture_pool_Delete(sys->pool);
    dlclose(sys->p_library);
    free(sys);
    vlc_mutex_unlock(&single_instance);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count) {
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(count);
    return sys->pool;
}

static int  AndroidLockSurface(picture_t *picture) {
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;
    SurfaceInfo *info;
    uint32_t sw, sh;
    void *surf;

    sw = picture->p[0].i_visible_pitch / picture->p[0].i_pixel_pitch;
    sh = picture->p[0].i_visible_lines;

    picsys->surf = surf = jni_LockAndGetAndroidSurface();
    info = &(picsys->info);

    if (unlikely(!surf)) {
        jni_UnlockAndroidSurface();
        return VLC_EGENERIC;
    }

    sys->s_lock(surf, info, 1);

    // input size doesn't match the surface size,
    // request a resize
    if (info->w != sw || info->h != sh) {
        jni_SetAndroidSurfaceSize(sw, sh);
        sys->s_unlockAndPost(surf);
        jni_UnlockAndroidSurface();
        return VLC_EGENERIC;
    }

    picture->p->p_pixels = (uint8_t*)info->bits;
    picture->p->i_pitch = 4 * info->s;
    picture->p->i_lines = info->h;

    return VLC_SUCCESS;
}

static void AndroidUnlockSurface(picture_t *picture) {
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;

    if (likely(picsys->surf))
        sys->s_unlockAndPost(picsys->surf);
    jni_UnlockAndroidSurface();
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture) {
    VLC_UNUSED(vd);
    VLC_UNUSED(subpicture);
    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args) {
    VLC_UNUSED(args);

    switch (query) {
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_GET_OPENGL:
            return VLC_EGENERIC;
        case VOUT_DISPLAY_HIDE_MOUSE:
            return VLC_SUCCESS;
        default:
            msg_Err(vd, "Unknown request in android vout display");
            return VLC_EGENERIC;
    }
}
