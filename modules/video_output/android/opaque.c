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
#include "../codec/omxil/android_opaque.h"

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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Display(vout_display_t *, picture_t *, subpicture_t *);
static int              Control(vout_display_t *, int, va_list);

struct vout_display_sys_t
{
    picture_pool_t *pool;
};

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
    vd->manage  = NULL;

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
    void (*display_callback)(picture_sys_t*) = p_picsys->pf_display_callback;
    if (display_callback)
        display_callback(p_picsys);

    /* refcount lowers to 0, and pool_cfg.unlock is called */
    picture_Release(picture);
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
