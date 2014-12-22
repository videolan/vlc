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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include <dlfcn.h>
#include <jni.h>

#include "utils.h"

#ifndef ANDROID_SYM_S_LOCK
# define ANDROID_SYM_S_LOCK "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb"
#endif
#ifndef ANDROID_SYM_S_LOCK2
# define ANDROID_SYM_S_LOCK2 "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE"
#endif
#ifndef ANDROID_SYM_S_UNLOCK
# define ANDROID_SYM_S_UNLOCK "_ZN7android7Surface13unlockAndPostEv"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CHROMA_TEXT N_("Chroma used")
#define CHROMA_LONGTEXT N_(\
    "Force use of a specific chroma for output. Default is RGB32.")

#define CFG_PREFIX "androidsurface-"

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("AndroidSurface")
    set_description(N_("Android Surface video output"))
    set_capability("vout display", 155)
    add_shortcut("androidsurface", "android")
    add_string(CFG_PREFIX "chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * JNI prototypes
 *****************************************************************************/

#define THREAD_NAME "AndroidSurface"
extern int jni_attach_thread(JNIEnv **env, const char *thread_name);
extern void jni_detach_thread();
extern jobject jni_LockAndGetAndroidJavaSurface();
extern void  jni_UnlockAndroidSurface();
extern void *jni_AndroidJavaSurfaceToNativeSurface(jobject surf);
extern void  jni_SetSurfaceLayout(int width, int height, int visible_width, int visible_height, int sar_num, int sar_den);
extern int jni_ConfigureSurface(jobject jsurf, int width, int height, int hal, bool *configured);

// _ZN7android7Surface4lockEPNS0_11SurfaceInfoEb
typedef void (*Surface_lock)(void *, void *, int);
// _ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE
typedef void (*Surface_lock2)(void *, void *, void *);
// _ZN7android7Surface13unlockAndPostEv
typedef void (*Surface_unlockAndPost)(void *);

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Display(vout_display_t *, picture_t *, subpicture_t *);
static int              Control(vout_display_t *, int, va_list);

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

/* */
struct vout_display_sys_t {
    picture_pool_t *pool;
    void *p_library;
    Surface_lock s_lock;
    Surface_lock2 s_lock2;
    Surface_unlockAndPost s_unlockAndPost;

    jobject jsurf;
    void *native_surface;

    int i_android_hal;
    unsigned int i_alloc_width;
    unsigned int i_alloc_height;

    video_format_t fmt;
};

struct picture_sys_t {
    void *surf;
    SurfaceInfo info;
    vout_display_sys_t *sys;
};

static int  AndroidLockSurface(picture_t *);
static void AndroidUnlockSurface(picture_t *);

static inline void *LoadSurface(const char *psz_lib, vout_display_sys_t *sys)
{
    void *p_library = dlopen(psz_lib, RTLD_NOW);
    if (!p_library)
        return NULL;

    sys->s_lock = (Surface_lock)(dlsym(p_library, ANDROID_SYM_S_LOCK));
    sys->s_lock2 = (Surface_lock2)(dlsym(p_library, ANDROID_SYM_S_LOCK2));
    sys->s_unlockAndPost =
        (Surface_unlockAndPost)(dlsym(p_library, ANDROID_SYM_S_UNLOCK));

    if ((sys->s_lock || sys->s_lock2) && sys->s_unlockAndPost)
        return p_library;

    dlclose(p_library);
    return NULL;
}

static void *InitLibrary(vout_display_sys_t *sys)
{
    static const char *libs[] = {
        "libsurfaceflinger_client.so",
        "libgui.so",
        "libui.so"
    };

    for (size_t i = 0; i < sizeof(libs) / sizeof(*libs); i++) {
        void *lib = LoadSurface(libs[i], sys);
        if (lib)
            return lib;
    }
    return NULL;
}

static void UpdateLayout(vout_display_sys_t *sys)
{
    unsigned int i_sar_num = 1, i_sar_den = 1;
    unsigned int i_width, i_height;

    if (sys->fmt.i_sar_num != 0 && sys->fmt.i_sar_den != 0) {
        i_sar_num = sys->fmt.i_sar_num;
        i_sar_den = sys->fmt.i_sar_den;
    }
    if (sys->i_alloc_width != 0 && sys->i_alloc_height != 0) {
        i_width = sys->i_alloc_width;
        i_height = sys->i_alloc_height;
    } else {
        i_width = sys->fmt.i_width;
        i_height = sys->fmt.i_height;
    }

    jni_SetSurfaceLayout(i_width, i_height,
                         sys->fmt.i_visible_width,
                         sys->fmt.i_visible_height,
                         i_sar_num,
                         i_sar_den);
}

static int Open(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, &vd->fmt);

    if (fmt.i_chroma == VLC_CODEC_ANDROID_OPAQUE)
        return VLC_EGENERIC;
    if (vout_display_IsWindowed(vd))
        return VLC_EGENERIC;

    /* Allocate structure */
    vout_display_sys_t *sys = (struct vout_display_sys_t*) calloc(1, sizeof(*sys));
    if (!sys)
        goto error;

    /* */
    sys->p_library = InitLibrary(sys);
    if (!sys->p_library) {
        msg_Err(vd, "Could not initialize libandroid.so/libui.so/libgui.so/libsurfaceflinger_client.so!");
        goto error;
    }

    /* Setup chroma */
    char *psz_fcc = var_InheritString(vd, CFG_PREFIX "chroma");
    if( psz_fcc ) {
        fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, psz_fcc);
        free(psz_fcc);
    } else
        fmt.i_chroma = VLC_CODEC_RGB32;

    switch(fmt.i_chroma) {
        case VLC_CODEC_RGB16:
            fmt.i_bmask = 0x0000001f;
            fmt.i_gmask = 0x000007e0;
            fmt.i_rmask = 0x0000f800;
            break;

        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
            fmt.i_chroma = VLC_CODEC_RGB32;
        case VLC_CODEC_RGB32:
            fmt.i_rmask  = 0x000000ff;
            fmt.i_gmask  = 0x0000ff00;
            fmt.i_bmask  = 0x00ff0000;
            break;

        default:
            return VLC_EGENERIC;
    }
    video_format_FixRgb(&fmt);

    msg_Dbg(vd, "Pixel format %4.4s", (char*)&fmt.i_chroma);
    sys->i_android_hal = ChromaToAndroidHal(fmt.i_chroma);
    if (sys->i_android_hal == -1)
        goto error;

    sys->fmt = fmt;
    UpdateLayout(sys);

    /* Create the associated picture */
    picture_sys_t *picsys = calloc(1, sizeof(picture_sys_t));
    if (unlikely(picsys == NULL))
        goto error;
    picsys->sys = sys;

    picture_resource_t resource = { .p_sys = picsys };
    picture_t *picture = picture_NewFromResource(&fmt, &resource);
    if (!picture) {
        free(picsys);
        goto error;
    }

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
        goto error;
    }

    /* Setup vout_display */
    vd->sys     = sys;
    vd->fmt     = fmt;
    vd->pool    = Pool;
    vd->display = Display;
    vd->control = Control;
    vd->prepare = NULL;
    vd->manage  = Manage;

    /* Fix initial state */
    vout_display_SendEventFullscreen(vd, false);

    return VLC_SUCCESS;

error:
    Close(p_this);
    return VLC_ENOMEM;
}

static void Close(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    if (sys) {
        if (sys->pool)
            picture_pool_Release(sys->pool);
        if (sys->p_library)
            dlclose(sys->p_library);
        free(sys);
    }
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);

    return vd->sys->pool;
}

static int  AndroidLockSurface(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;
    SurfaceInfo *info = &picsys->info;
    uint32_t sw, sh;

    if (!sys->native_surface) {
        picsys->surf = jni_LockAndGetAndroidJavaSurface();
        if (unlikely(!picsys->surf)) {
            jni_UnlockAndroidSurface();
            return VLC_EGENERIC;
        }
        sys->native_surface = jni_AndroidJavaSurfaceToNativeSurface(picsys->surf);
        jni_UnlockAndroidSurface();

        if (!sys->native_surface)
            return VLC_EGENERIC;
    }

    sw = sys->fmt.i_width;
    sh = sys->fmt.i_height;
    // For RGB (32 or 16) we need to align on 8 or 4 pixels, 16 pixels for YUV
    int align_pixels = (16 / picture->p[0].i_pixel_pitch) - 1;
    uint32_t aligned_width = (sw + align_pixels) & ~align_pixels;

    if (aligned_width != sys->i_alloc_width || sh != sys->i_alloc_height) {
        bool configured;
        if (jni_ConfigureSurface(picsys->surf,
                                 aligned_width,
                                 sh,
                                 sys->i_android_hal,
                                 &configured) == -1 || !configured) {
            return VLC_EGENERIC;
        }
        sys->i_alloc_width = aligned_width;
        sys->i_alloc_height = sh;
        sys->native_surface = jni_AndroidJavaSurfaceToNativeSurface(picsys->surf);
        UpdateLayout(sys);
    }

    if (sys->s_lock)
        sys->s_lock(sys->native_surface, info, 1);
    else
        sys->s_lock2(sys->native_surface, info, NULL);

    if (info->w != sys->i_alloc_width || info->h != sh) {
        sys->s_unlockAndPost(sys->native_surface);
        return VLC_EGENERIC;
    }

    picture->p[0].p_pixels = (uint8_t*)info->bits;
    picture->p[0].i_lines = info->h;
    picture->p[0].i_pitch = picture->p[0].i_pixel_pitch * info->s;

    return VLC_SUCCESS;
}

static void AndroidUnlockSurface(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;

    if (sys->native_surface)
        sys->s_unlockAndPost(sys->native_surface);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(subpicture);

    /* refcount lowers to 0, and pool_cfg.unlock is called */

    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(args);

    switch (query) {
    case VOUT_DISPLAY_HIDE_MOUSE:
        return VLC_SUCCESS;

    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    {
        vout_display_sys_t *sys = vd->sys;

        const video_format_t *source = (const video_format_t *)va_arg(args, const video_format_t *);

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
            video_format_CopyCrop(&sys->fmt, source);
        } else {
            sys->fmt.i_sar_num = source->i_sar_num;
            sys->fmt.i_sar_den = source->i_sar_den;
        }
        UpdateLayout(sys);

        return VLC_SUCCESS;
    }

    default:
        msg_Err(vd, "Unknown request in android vout display");

    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
        return VLC_EGENERIC;
    }
}
