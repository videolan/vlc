/*****************************************************************************
 * opaque.c: Android video output module using direct rendering with
 * opaque buffers
 *****************************************************************************
 * Copyright (C) 2013 Felix Abecassis
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_filter.h>
#include <vlc_md5.h>

#include <dlfcn.h>

#include "../codec/omxil/android_opaque.h"
#include "utils.h"

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("vout_mediacodec")
    set_description(N_("Android MediaCodec direct rendering video output"))
    set_capability("vout display", 200)
    add_shortcut("androidsurface", "android")
    set_callbacks(Open, Close)
vlc_module_end()

#define THREAD_NAME "vout_mediacodec"
extern int jni_attach_thread(JNIEnv **env, const char *thread_name);
extern void jni_detach_thread();
extern jobject jni_LockAndGetSubtitlesSurface();
extern void  jni_UnlockAndroidSurface();

static const vlc_fourcc_t subpicture_chromas[] =
{
    VLC_CODEC_RGBA,
    0
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Display(vout_display_t *, picture_t *, subpicture_t *);
static int              Control(vout_display_t *, int, va_list);

struct vout_display_sys_t
{
    picture_pool_t *pool;

    void *p_library;
    native_window_api_t native_window;

    jobject jsurf;
    ANativeWindow *window;

    video_format_t fmt;

    filter_t *p_spu_blend;
    picture_t *subtitles_picture;

    bool b_has_subpictures;

    uint8_t hash[16];
};

static void DisplaySubpicture(vout_display_t *vd, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    struct md5_s hash;
    InitMD5(&hash);
    if (subpicture) {
        for (subpicture_region_t *r = subpicture->p_region; r != NULL; r = r->p_next) {
            AddMD5(&hash, &r->i_x, sizeof(r->i_x));
            AddMD5(&hash, &r->i_y, sizeof(r->i_y));
            AddMD5(&hash, &r->fmt.i_visible_width, sizeof(r->fmt.i_visible_width));
            AddMD5(&hash, &r->fmt.i_visible_height, sizeof(r->fmt.i_visible_height));
            AddMD5(&hash, &r->fmt.i_x_offset, sizeof(r->fmt.i_x_offset));
            AddMD5(&hash, &r->fmt.i_y_offset, sizeof(r->fmt.i_y_offset));
            const int pixels_offset = r->fmt.i_y_offset * r->p_picture->p->i_pitch +
                                      r->fmt.i_x_offset * r->p_picture->p->i_pixel_pitch;

            for (int y = 0; y < r->fmt.i_visible_height; y++)
                AddMD5(&hash, &r->p_picture->p->p_pixels[pixels_offset + y*r->p_picture->p->i_pitch], r->fmt.i_visible_width);
        }
    }
    EndMD5(&hash);
    if (!memcmp(hash.buf, sys->hash, 16))
        return;
    memcpy(sys->hash, hash.buf, 16);

    jobject jsurf = jni_LockAndGetSubtitlesSurface();
    if (sys->window && jsurf != sys->jsurf)
    {
        sys->native_window.winRelease(sys->window);
        sys->window = NULL;
    }
    sys->jsurf = jsurf;
    if (!sys->window)
    {
        JNIEnv *p_env;
        jni_attach_thread(&p_env, THREAD_NAME);
        sys->window = sys->native_window.winFromSurface(p_env, jsurf);
        jni_detach_thread();
    }

    ANativeWindow_Buffer buf = { 0 };
    int32_t err = sys->native_window.winLock(sys->window, &buf, NULL);
    if (err) {
        jni_UnlockAndroidSurface();
        return;
    }

    if (buf.width >= sys->fmt.i_width && buf.height >= sys->fmt.i_height)
    {
        /* Wrap the NativeWindow corresponding to the subtitles surface in a picture_t */
        picture_t *picture = sys->subtitles_picture;
        picture->p[0].p_pixels = (uint8_t*)buf.bits;
        picture->p[0].i_lines = buf.height;
        picture->p[0].i_pitch = picture->p[0].i_pixel_pitch * buf.stride;
        /* Clear the subtitles surface. */
        memset(picture->p[0].p_pixels, 0, picture->p[0].i_pitch * picture->p[0].i_lines);
        if (subpicture)
        {
            /* Allocate a blending filter if needed. */
            if (unlikely(!sys->p_spu_blend))
                sys->p_spu_blend = filter_NewBlend(VLC_OBJECT(vd), &picture->format);
            picture_BlendSubpicture(picture, sys->p_spu_blend, subpicture);
        }
    }

    sys->native_window.unlockAndPost(sys->window);
    jni_UnlockAndroidSurface();
}

static int  LockSurface(picture_t *);
static void UnlockSurface(picture_t *);

/* We need to allocate a picture pool of more than 30 buffers in order
 * to be connected directly to the decoder without any intermediate
 * buffer pool. */
#define POOL_SIZE 31

static int Open(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t*)p_this;

    video_format_t fmt = vd->fmt;

    if (fmt.i_chroma != VLC_CODEC_ANDROID_OPAQUE)
        return VLC_EGENERIC;

    /* Allocate structure */
    vout_display_sys_t *sys = (struct vout_display_sys_t*)calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->p_library = LoadNativeWindowAPI(&sys->native_window);
    if (!sys->p_library)
    {
        free(sys);
        msg_Err(vd, "Could not initialize NativeWindow API.");
        return VLC_EGENERIC;
    }
    sys->fmt = fmt;
    video_format_t subpicture_format = sys->fmt;
    subpicture_format.i_chroma = VLC_CODEC_RGBA;
    /* Create a RGBA picture for rendering subtitles. */
    sys->subtitles_picture = picture_NewFromFormat(&subpicture_format);

    /* Export the subpicture capability of this vout. */
    vd->info.subpicture_chromas = subpicture_chromas;

    int i_pictures = POOL_SIZE;
    picture_t** pictures = calloc(sizeof(*pictures), i_pictures);
    if (!pictures)
        goto error;
    for (int i = 0; i < i_pictures; i++)
    {
        picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));
        if (unlikely(p_picsys == NULL))
            goto error;

        picture_resource_t resource = { .p_sys = p_picsys };
        picture_t *picture = picture_NewFromResource(&fmt, &resource);
        if (!picture)
        {
            free(p_picsys);
            goto error;
        }
        pictures[i] = picture;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = i_pictures;
    pool_cfg.picture       = pictures;
    pool_cfg.lock          = LockSurface;
    pool_cfg.unlock        = UnlockSurface;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool)
    {
        for (int i = 0; i < i_pictures; i++)
            picture_Release(pictures[i]);
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
    free(pictures);
    Close(p_this);
    return VLC_ENOMEM;
}

static void Close(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    picture_pool_Delete(sys->pool);
    if (sys->window)
        sys->native_window.winRelease(sys->window);
    dlclose(sys->p_library);
    picture_Release(sys->subtitles_picture);
    if (sys->p_spu_blend)
        filter_DeleteBlend(sys->p_spu_blend);
    free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);

    return vd->sys->pool;
}

static int LockSurface(picture_t *picture)
{
    VLC_UNUSED(picture);

    return VLC_SUCCESS;
}

static void UnlockSurface(picture_t *picture)
{
    picture_sys_t *p_picsys = picture->p_sys;
    void (*unlock_callback)(picture_sys_t*) = p_picsys->pf_unlock_callback;
    if (unlock_callback)
        unlock_callback(p_picsys);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(subpicture);

    picture_sys_t *p_picsys = picture->p_sys;
    vout_display_sys_t *sys = vd->sys;
    void (*display_callback)(picture_sys_t*) = p_picsys->pf_display_callback;
    if (display_callback)
        display_callback(p_picsys);

    if (subpicture)
	sys->b_has_subpictures = true;
    /* As long as no subpicture was received, do not call
       DisplaySubpicture since JNI calls and clearing the subtitles
       surface are expensive operations. */
    if (sys->b_has_subpictures)
    {
	DisplaySubpicture(vd, subpicture);
        if (!subpicture)
        {
            /* The surface has been cleared and there is no new
               subpicture to upload, do not clear again until a new
               subpicture is received. */
            sys->b_has_subpictures = false;
        }
    }

    /* refcount lowers to 0, and pool_cfg.unlock is called */
    picture_Release(picture);
    if (subpicture)
        subpicture_Delete(subpicture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(args);

    switch (query) {
    case VOUT_DISPLAY_HIDE_MOUSE:
        return VLC_SUCCESS;

    default:
        msg_Err(vd, "Unknown request in vout mediacodec display");

    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_GET_OPENGL:
        return VLC_EGENERIC;
    }
}
