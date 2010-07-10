/*****************************************************************************
 * vmem.c: memory video driver for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define T_WIDTH N_("Width")
#define LT_WIDTH N_("Video memory buffer width.")

#define T_HEIGHT N_("Height")
#define LT_HEIGHT N_("Video memory buffer height.")

#define T_PITCH N_("Pitch")
#define LT_PITCH N_("Video memory buffer pitch in bytes.")

#define T_CHROMA N_("Chroma")
#define LT_CHROMA N_("Output chroma for the memory image as a 4-character " \
                      "string, eg. \"RV32\".")

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Video memory output"))
    set_shortname(N_("Video memory"))

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)

    add_integer("vmem-width", 320, NULL, T_WIDTH, LT_WIDTH, false)
        change_private()
    add_integer("vmem-height", 200, NULL, T_HEIGHT, LT_HEIGHT, false)
        change_private()
    add_integer("vmem-pitch", 640, NULL, T_PITCH, LT_PITCH, false)
        change_private()
    add_string("vmem-chroma", "RV16", NULL, T_CHROMA, LT_CHROMA, true)
        change_private()
    add_obsolete_string("vmem-lock") /* obsoleted since 1.1.1 */
    add_obsolete_string("vmem-unlock") /* obsoleted since 1.1.1 */
    add_obsolete_string("vmem-data") /* obsoleted since 1.1.1 */

    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct picture_sys_t {
    vout_display_sys_t *sys;
    void *id;
};

struct vout_display_sys_t {
    picture_pool_t *pool;
    void *(*lock)(void *sys, void **plane);
    void (*unlock)(void *sys, void *id, void *const *plane);
    void (*display)(void *sys, void *id);
    void *opaque;
};

static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

static int            Lock(picture_t *);
static void           Unlock(picture_t *);

/*****************************************************************************
 * Open: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

    /* */
    char *chroma_format = var_InheritString(vd, "vmem-chroma");
    const vlc_fourcc_t chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma_format);
    free(chroma_format);
    if (!chroma) {
        msg_Err(vd, "vmem-chroma should be 4 characters long");
        return VLC_EGENERIC;
    }

    /* */
    video_format_t fmt = vd->fmt;

    fmt.i_chroma = chroma;
    fmt.i_width  = var_InheritInteger(vd, "vmem-width");
    fmt.i_height = var_InheritInteger(vd, "vmem-height");

    /* Define the bitmasks */
    switch (chroma)
    {
    case VLC_CODEC_RGB15:
        fmt.i_rmask = 0x001f;
        fmt.i_gmask = 0x03e0;
        fmt.i_bmask = 0x7c00;
        break;
    case VLC_CODEC_RGB16:
        fmt.i_rmask = 0x001f;
        fmt.i_gmask = 0x07e0;
        fmt.i_bmask = 0xf800;
        break;
    case VLC_CODEC_RGB24:
        fmt.i_rmask = 0xff0000;
        fmt.i_gmask = 0x00ff00;
        fmt.i_bmask = 0x0000ff;
        break;
    case VLC_CODEC_RGB32:
        fmt.i_rmask = 0xff0000;
        fmt.i_gmask = 0x00ff00;
        fmt.i_bmask = 0x0000ff;
        break;
    default:
        fmt.i_rmask = 0;
        fmt.i_gmask = 0;
        fmt.i_bmask = 0;
        break;
    }

    /* */
    vout_display_sys_t *sys;
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (unlikely(!sys))
        return VLC_ENOMEM;

    sys->lock = var_InheritAddress(vd, "vmem-lock");
    if (sys->lock == NULL) {
        msg_Err(vd, "Invalid lock callback");
        free(sys);
        return VLC_EGENERIC;
    }
    sys->unlock = var_InheritAddress(vd, "vmem-unlock");
    sys->display = var_InheritAddress(vd, "vmem-display");
    sys->opaque = var_InheritAddress(vd, "vmem-data");

    /* */
    const int pitch = var_InheritInteger(vd, "vmem-pitch");
    picture_resource_t rsc;
    rsc.p_sys = malloc(sizeof(*rsc.p_sys));
    if(unlikely(!rsc.p_sys)) {
        free(sys);
        return VLC_ENOMEM;
    }
    rsc.p_sys->sys = sys;
    rsc.p_sys->id = NULL;
    for (int i = 0; i < PICTURE_PLANE_MAX; i++) {
        /* vmem-lock is responsible for the allocation */
        rsc.p[i].p_pixels = NULL;
        rsc.p[i].i_lines  = fmt.i_height;
        rsc.p[i].i_pitch  = pitch;
    }
    picture_t *picture = picture_NewFromResource(&fmt, &rsc);
    if (!picture) {
        free(rsc.p_sys);
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    picture_pool_configuration_t pool;
    memset(&pool, 0, sizeof(pool));
    pool.picture_count = 1;
    pool.picture       = &picture;
    pool.lock          = Lock;
    pool.unlock        = Unlock;
    sys->pool = picture_pool_NewExtended(&pool);
    if (!sys->pool) {
        picture_Release(picture);
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = true;

    /* */
    vd->fmt     = fmt;
    vd->info    = info;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* */
    vout_display_SendEventFullscreen(vd, false);
    vout_display_SendEventDisplaySize(vd, fmt.i_width, fmt.i_height, false);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    picture_pool_Delete(sys->pool);
    free(sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    assert(!picture_IsReferenced(picture));
    if (sys->display != NULL)
        sys->display(sys->opaque, picture->p_sys->id);
    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
        if (cfg->display.width  != vd->fmt.i_width ||
            cfg->display.height != vd->fmt.i_height)
            return VLC_EGENERIC;
        if (cfg->is_fullscreen)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}
static void Manage(vout_display_t *vd)
{
    VLC_UNUSED(vd);
}

/* */
static int Lock(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;
    void *planes[PICTURE_PLANE_MAX];

    picsys->id = sys->lock(sys->opaque, planes);

    for (int i = 0; i < picture->i_planes; i++)
        picture->p[i].p_pixels = planes[i];

    return VLC_SUCCESS;
}

static void Unlock(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *sys = picsys->sys;

    void *planes[PICTURE_PLANE_MAX];

    for (int i = 0; i < picture->i_planes; i++)
        planes[i] = picture->p[i].p_pixels;

    if (sys->unlock != NULL)
        sys->unlock(sys->opaque, picsys->id, planes);
}
