/*****************************************************************************
 * gradfun.c: wrapper for the gradfun filter from libav
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * Based on the work of: Nolan Lum and Loren Merritt
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_cpu.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CFG_PREFIX "gradfun-"

#define RADIUS_MIN (4)
#define RADIUS_MAX (32)
#define RADIUS_TEXT N_("Radius")
#define RADIUS_LONGTEXT N_("Radius in pixels")

#define STRENGTH_MIN (0.51)
#define STRENGTH_MAX (255)
#define STRENGTH_TEXT N_("Strength")
#define STRENGTH_LONGTEXT N_("Strength used to modify the value of a pixel")

vlc_module_begin()
    set_description(N_("Gradfun video filter"))
    set_shortname(N_("Gradfun"))
    set_help(N_("Debanding algorithm"))
    set_capability("video filter2", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    add_integer_with_range(CFG_PREFIX "radius", 16, RADIUS_MIN, RADIUS_MAX,
                           RADIUS_TEXT, RADIUS_LONGTEXT, false)
    add_float_with_range(CFG_PREFIX "strength", 1.2, STRENGTH_MIN, STRENGTH_MAX,
                         STRENGTH_TEXT, STRENGTH_LONGTEXT, false)

    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define FFMAX(a,b) __MAX(a,b)
#ifdef CAN_COMPILE_MMXEXT
#   define HAVE_MMX2 1
#else
#   define HAVE_MMX2 0
#endif
#ifdef CAN_COMPILE_SSE2
#   define HAVE_SSE2 1
#else
#   define HAVE_SSE2 0
#endif
#ifdef CAN_COMPILE_SSSE3
#   define HAVE_SSSE3 1
#else
#   define HAVE_SSSE3 0
#endif
// FIXME too restrictive
#ifdef __x86_64__
#   define HAVE_6REGS 1
#else
#   define HAVE_6REGS 0
#endif
#define av_clip_uint8 clip_uint8_vlc
#include "gradfun.h"

static picture_t *Filter(filter_t *, picture_t *);
static int Callback(vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void *);

struct filter_sys_t {
    vlc_mutex_t      lock;
    float            strength;
    int              radius;
    const vlc_chroma_description_t *chroma;
    struct vf_priv_s cfg;
};

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;
    const vlc_fourcc_t fourcc = filter->fmt_in.video.i_chroma;

    const vlc_chroma_description_t *chroma = vlc_fourcc_GetChromaDescription(fourcc);
    if (!chroma || chroma->plane_count < 3 || chroma->pixel_size != 1) {
        msg_Err(filter, "Unsupported chroma (%4.4s)", (char*)&fourcc);
        return VLC_EGENERIC;
    }

    filter_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->lock);
    sys->chroma   = chroma;
    sys->strength = var_CreateGetFloatCommand(filter,   CFG_PREFIX "strength");
    sys->radius   = var_CreateGetIntegerCommand(filter, CFG_PREFIX "radius");
    var_AddCallback(filter, CFG_PREFIX "strength", Callback, NULL);
    var_AddCallback(filter, CFG_PREFIX "radius",   Callback, NULL);
    sys->cfg.buf = NULL;

    struct vf_priv_s *cfg = &sys->cfg;
    cfg->thresh      = 0.0;
    cfg->radius      = 0;
    cfg->buf         = NULL;

#if HAVE_SSE2 && HAVE_6REGS
    if (vlc_CPU_SSE2())
        cfg->blur_line = blur_line_sse2;
    else
#endif
        cfg->blur_line   = blur_line_c;
#if HAVE_SSSE3
    if (vlc_CPU_SSSE3())
        cfg->filter_line = filter_line_ssse3;
    else
#endif
#if HAVE_MMX2
    if (vlc_CPU_MMXEXT())
        cfg->filter_line = filter_line_mmx2;
    else
#endif
        cfg->filter_line = filter_line_c;

    filter->p_sys           = sys;
    filter->pf_video_filter = Filter;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    filter_t     *filter = (filter_t *)object;
    filter_sys_t *sys = filter->p_sys;

    var_DelCallback(filter, CFG_PREFIX "radius",   Callback, NULL);
    var_DelCallback(filter, CFG_PREFIX "strength", Callback, NULL);
    vlc_free(sys->cfg.buf);
    vlc_mutex_destroy(&sys->lock);
    free(sys);
}

static picture_t *Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;

    picture_t *dst = filter_NewPicture(filter);
    if (!dst) {
        picture_Release(src);
        return NULL;
    }

    vlc_mutex_lock(&sys->lock);
    float strength = VLC_CLIP(sys->strength, STRENGTH_MIN, STRENGTH_MAX);
    int   radius   = VLC_CLIP((sys->radius + 1) & ~1, RADIUS_MIN, RADIUS_MAX);
    vlc_mutex_unlock(&sys->lock);

    const video_format_t *fmt = &filter->fmt_in.video;
    struct vf_priv_s *cfg = &sys->cfg;

    cfg->thresh = (1 << 15) / strength;
    if (cfg->radius != radius) {
        cfg->radius = radius;
        cfg->buf    = vlc_memalign(16,
                                   (((fmt->i_width + 15) & ~15) * (cfg->radius + 1) / 2 + 32) * sizeof(*cfg->buf));
    }

    for (int i = 0; i < dst->i_planes; i++) {
        const plane_t *srcp = &src->p[i];
        plane_t       *dstp = &dst->p[i];

        const vlc_chroma_description_t *chroma = sys->chroma;
        int w = fmt->i_width  * chroma->p[i].w.num / chroma->p[i].w.den;
        int h = fmt->i_height * chroma->p[i].h.num / chroma->p[i].h.den;
        int r = (cfg->radius  * chroma->p[i].w.num / chroma->p[i].w.den +
                 cfg->radius  * chroma->p[i].h.num / chroma->p[i].h.den) / 2;
        r = VLC_CLIP((r + 1) & ~1, RADIUS_MIN, RADIUS_MAX);
        if (__MIN(w, h) > 2 * r && cfg->buf) {
            filter_plane(cfg, dstp->p_pixels, srcp->p_pixels,
                         w, h, dstp->i_pitch, srcp->i_pitch, r);
        } else {
            plane_CopyPixels(dstp, srcp);
        }
    }

    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}

static int Callback(vlc_object_t *object, char const *cmd,
                    vlc_value_t oldval, vlc_value_t newval, void *data)
{
    filter_t     *filter = (filter_t *)object;
    filter_sys_t *sys = filter->p_sys;
    VLC_UNUSED(oldval); VLC_UNUSED(data);

    vlc_mutex_lock(&sys->lock);
    if (!strcmp(cmd, CFG_PREFIX "strength"))
        sys->strength = newval.f_float;
    else
        sys->radius    = newval.i_int;
    vlc_mutex_unlock(&sys->lock);

    return VLC_SUCCESS;
}

