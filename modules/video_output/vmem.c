/*****************************************************************************
 * vmem.c: memory video driver for vlc
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyright (C) 2010 Rémi Denis-Courmont
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

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin()
    set_description(N_("Video memory output"))
    set_shortname(N_("Video memory"))

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

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

    set_callback_display(Open, 0)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    void *id;
} picture_sys_t;

/* NOTE: the callback prototypes must match those of LibVLC */
struct vout_display_sys_t {
    void *opaque;
    void *pic_opaque;
    void *(*lock)(void *sys, void **plane);
    void (*unlock)(void *sys, void *id, void *const *plane);
    void (*display)(void *sys, void *id);
    void (*cleanup)(void *sys);

    unsigned pitches[PICTURE_PLANE_MAX];
    unsigned lines[PICTURE_PLANE_MAX];
};

typedef unsigned (*vlc_format_cb)(void **, char *, unsigned *, unsigned *,
                                  unsigned *, unsigned *);

static void           Prepare(vout_display_t *, picture_t *, subpicture_t *, vlc_tick_t);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * Open: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
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

    /* Define the video format */
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, fmtp);

    if (setup != NULL) {
        char chroma[5];

        memcpy(chroma, &fmt.i_chroma, 4);
        chroma[4] = '\0';
        memset(sys->pitches, 0, sizeof(sys->pitches));
        memset(sys->lines, 0, sizeof(sys->lines));

        unsigned widths[2], heights[2];
        widths[0] = fmt.i_width;
        widths[1] = fmt.i_visible_width;

        heights[0] = fmt.i_height;
        heights[1] = fmt.i_visible_height;

        if (setup(&sys->opaque, chroma, widths, heights,
                           sys->pitches, sys->lines) == 0) {
            msg_Err(vd, "video format setup failure (no pictures)");
            free(sys);
            return VLC_EGENERIC;
        }
        fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);
        fmt.i_width = widths[0];
        fmt.i_height = heights[0];

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
        sys->cleanup = NULL;
    }
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_visible_width = fmt.i_width;
    fmt.i_visible_height = fmt.i_height;

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
    *fmtp = fmt;

    vd->sys     = sys;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->close   = Close;

    (void) cfg; (void) context;
    return VLC_SUCCESS;
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->cleanup)
        sys->cleanup(sys->opaque);
    free(sys);
}

static void Prepare(vout_display_t *vd, picture_t *pic, subpicture_t *subpic,
                    vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;
    picture_resource_t rsc = { .p_sys = NULL };
    void *planes[PICTURE_PLANE_MAX];

    sys->pic_opaque = sys->lock(sys->opaque, planes);

    for (unsigned i = 0; i < PICTURE_PLANE_MAX; i++) {
        rsc.p[i].p_pixels = planes[i];
        rsc.p[i].i_lines  = sys->lines[i];
        rsc.p[i].i_pitch  = sys->pitches[i];
    }

    picture_t *locked = picture_NewFromResource(&vd->fmt, &rsc);
    if (likely(locked != NULL)) {
        picture_CopyPixels(locked, pic);
        picture_Release(locked);
    }

    if (sys->unlock != NULL)
        sys->unlock(sys->opaque, sys->pic_opaque, planes);

    (void) subpic;
}

static void Display(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(pic);

    if (sys->display != NULL)
        sys->display(sys->opaque, sys->pic_opaque);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}
