/*****************************************************************************
 * vmem.c: memory video driver for vlc
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyrgiht (C) 2010 Rémi Denis-Courmont
 *
 * Authors: Sam Hocevar <sam@zoy.org>
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

    add_integer("vmem-width", 320, T_WIDTH, LT_WIDTH, false)
        change_private()
    add_integer("vmem-height", 200, T_HEIGHT, LT_HEIGHT, false)
        change_private()
    add_integer("vmem-pitch", 640, T_PITCH, LT_PITCH, false)
        change_private()
    add_string("vmem-chroma", "RV16", T_CHROMA, LT_CHROMA, true)
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

/* NOTE: the callback prototypes must match those of LibVLC */
struct vout_display_sys_t {
    picture_pool_t *pool;
    unsigned        count;

    void *opaque;
    void *(*lock)(void *sys, void **plane);
    void (*unlock)(void *sys, void *id, void *const *plane);
    void (*display)(void *sys, void *id);
    void (*cleanup)(void *sys);

    unsigned pitches[PICTURE_PLANE_MAX];
    unsigned lines[PICTURE_PLANE_MAX];
};

typedef unsigned (*vlc_format_cb)(void **, char *, unsigned *, unsigned *,
                                  unsigned *, unsigned *);

static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);

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
    vout_display_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(!sys))
        return VLC_ENOMEM;

    /* Get the callbacks */
    vlc_format_cb setup = var_InheritAddress(vd, "vmem-setup");

    sys->lock = var_InheritAddress(vd, "vmem-lock");
    if (sys->lock == NULL) {
        msg_Err(vd, "missing lock callback");
        free(sys);
        return VLC_EGENERIC;
    }
    sys->unlock = var_InheritAddress(vd, "vmem-unlock");
    sys->display = var_InheritAddress(vd, "vmem-display");
    sys->cleanup = var_InheritAddress(vd, "vmem-cleanup");
    sys->opaque = var_InheritAddress(vd, "vmem-data");
    sys->pool = NULL;

    /* Define the video format */
    video_format_t fmt = vd->fmt;

    if (setup != NULL) {
        char chroma[5];

        memcpy(chroma, &fmt.i_chroma, 4);
        chroma[4] = '\0';
        memset(sys->pitches, 0, sizeof(sys->pitches));
        memset(sys->lines, 0, sizeof(sys->lines));

        sys->count = setup(&sys->opaque, chroma, &fmt.i_width, &fmt.i_height,
                           sys->pitches, sys->lines);
        if (sys->count == 0) {
            msg_Err(vd, "video format setup failure (no pictures)");
            free(sys);
            return VLC_EGENERIC;
        }
        fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);

    } else {
        char *chroma = var_InheritString(vd, "vmem-chroma");
        fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);
        free(chroma);

        fmt.i_width  = var_InheritInteger(vd, "vmem-width");
        fmt.i_height = var_InheritInteger(vd, "vmem-height");
        sys->pitches[0] = var_InheritInteger(vd, "vmem-pitch");
        sys->lines[0] = fmt.i_height;
        for (size_t i = 1; i < PICTURE_PLANE_MAX; i++)
        {
            sys->pitches[i] = sys->pitches[0];
            sys->lines[i] = sys->lines[0];
        }
        sys->count = 1;
        sys->cleanup = NULL;
    }

    if (!fmt.i_chroma) {
        msg_Err(vd, "vmem-chroma should be 4 characters long");
        free(sys);
        return VLC_EGENERIC;
    }

    /* Define the bitmasks */
    switch (fmt.i_chroma)
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
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = true;

    /* */
    vd->sys     = sys;
    vd->fmt     = fmt;
    vd->info    = info;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = NULL;

    /* */
    vout_display_SendEventFullscreen(vd, false);
    vout_display_SendEventDisplaySize(vd, fmt.i_width, fmt.i_height, false);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->cleanup)
        sys->cleanup(sys->opaque);
    picture_pool_Delete(sys->pool);
    free(sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        return sys->pool;

    if (count > sys->count)
        count = sys->count;

    picture_t *pictures[count];

    for (unsigned i = 0; i < count; i++) {
        picture_sys_t *picsys = malloc(sizeof (*picsys));
        if (unlikely(picsys == NULL))
        {
            count = i;
            break;
        }
        picsys->sys = sys;
        picsys->id = NULL;

        picture_resource_t rsc = { .p_sys = picsys };

        for (unsigned i = 0; i < PICTURE_PLANE_MAX; i++) {
            /* vmem-lock is responsible for the allocation */
            rsc.p[i].p_pixels = NULL;
            rsc.p[i].i_lines  = sys->lines[i];
            rsc.p[i].i_pitch  = sys->pitches[i];
        }

        pictures[i] = picture_NewFromResource(&vd->fmt, &rsc);
        if (!pictures[i]) {
            free(rsc.p_sys);
            count = i;
            break;
        }
    }

    /* */
    picture_pool_configuration_t pool;
    memset(&pool, 0, sizeof(pool));
    pool.picture_count = count;
    pool.picture       = pictures;
    pool.lock          = Lock;
    pool.unlock        = Unlock;
    sys->pool = picture_pool_NewExtended(&pool);
    if (!sys->pool) {
        for (unsigned i = 0; i < count; i++)
            picture_Release(pictures[i]);
    }

    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    assert(!picture_IsReferenced(picture));
    if (sys->display != NULL)
        sys->display(sys->opaque, picture->p_sys->id);
    picture_Release(picture);
    VLC_UNUSED(subpicture);
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
