/*****************************************************************************
 * adjust.c: VDPAU colour adjust video filter
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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

#include <stdatomic.h>
#include <stdlib.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "vlc_vdpau.h"

typedef struct
{
    _Atomic float brightness;
    _Atomic float contrast;
    _Atomic float saturation;
    _Atomic float hue;
} filter_sys_t;

static float vlc_to_vdp_brightness(float brightness)
{
    brightness -= 1.f;
    if (brightness > +1.f)
        brightness = +1.f;
    if (brightness < -1.f)
        brightness = -1.f;
    return brightness;
}

static int BrightnessCallback(vlc_object_t *obj, const char *varname,
                              vlc_value_t prev, vlc_value_t cur, void *data)
{
    _Atomic float *atom = data;

    atomic_store_explicit(atom, vlc_to_vdp_brightness(cur.f_float),
                          memory_order_relaxed);
    (void) obj; (void) varname; (void) prev;
    return VLC_SUCCESS;
}

static float vlc_to_vdp_contrast(float contrast)
{
    if (contrast > 10.f)
        contrast = 10.f;
    if (contrast < 0.f)
        contrast = 0.f;
    return contrast;
}

static int ContrastCallback(vlc_object_t *obj, const char *varname,
                            vlc_value_t prev, vlc_value_t cur, void *data)
{
    _Atomic float *atom = data;

    atomic_store_explicit(atom, vlc_to_vdp_contrast(cur.f_float),
                          memory_order_relaxed);
    (void) obj; (void) varname; (void) prev;
    return VLC_SUCCESS;
}

#define vlc_to_vdp_saturation vlc_to_vdp_contrast

static int SaturationCallback(vlc_object_t *obj, const char *varname,
                              vlc_value_t prev, vlc_value_t cur, void *data)
{
    _Atomic float *atom = data;

    atomic_store_explicit(atom, vlc_to_vdp_saturation(cur.f_float),
                          memory_order_relaxed);
    (void) obj; (void) varname; (void) prev;
    return VLC_SUCCESS;
}

static float vlc_to_vdp_hue(float hue)
{
    float dummy;

    hue /= 360.f;
    hue = modff(hue, &dummy);
    if (hue > .5f)
        hue -= 1.f;
    return hue * (float)(2. * M_PI);
}

static int HueCallback(vlc_object_t *obj, const char *varname,
                              vlc_value_t prev, vlc_value_t cur, void *data)
{
    _Atomic float *atom = data;

    atomic_store_explicit(atom, vlc_to_vdp_hue(cur.f_float),
                          memory_order_relaxed);
    (void) obj; (void) varname; (void) prev;
    return VLC_SUCCESS;
}

static picture_t *Adjust(filter_t *filter, picture_t *pic)
{
    const filter_sys_t *sys = filter->p_sys;
    vlc_vdp_video_field_t *f = VDPAU_FIELD_FROM_PICCTX(pic->context);

    if (unlikely(f == NULL))
        return pic;

    f->procamp.brightness = atomic_load_explicit(&sys->brightness,
                                                 memory_order_relaxed);
    f->procamp.contrast = atomic_load_explicit(&sys->contrast,
                                               memory_order_relaxed);
    f->procamp.saturation = atomic_load_explicit(&sys->saturation,
                                                 memory_order_relaxed);
    f->procamp.hue = atomic_load_explicit(&sys->hue, memory_order_relaxed);

    return pic;
}

static const char *const options[] = {
    "brightness", "contrast", "saturation", "hue", NULL
};

static int Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if ( filter->vctx_in == NULL ||
         vlc_video_context_GetType(filter->vctx_in) != VLC_VIDEO_CONTEXT_VDPAU )
        return VLC_EGENERIC;
    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_420
     && filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_422
     && filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_444)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    filter->pf_video_filter = Adjust;
    filter->p_sys = sys;

    config_ChainParse(filter, "", options, filter->p_cfg);

    float f;
    int i;

    f = var_CreateGetFloatCommand(filter, "brightness");
    var_AddCallback(filter, "brightness", BrightnessCallback,
                    &sys->brightness);
    atomic_init(&sys->brightness, vlc_to_vdp_brightness(f));

    f = var_CreateGetFloatCommand(filter, "contrast");
    var_AddCallback(filter, "contrast", ContrastCallback, &sys->contrast);
    atomic_init(&sys->contrast, vlc_to_vdp_contrast(f));

    f = var_CreateGetFloatCommand(filter, "saturation");
    var_AddCallback(filter, "saturation", SaturationCallback,
                    &sys->saturation);
    atomic_init(&sys->saturation, vlc_to_vdp_saturation(f));

    i = var_CreateGetFloatCommand(filter, "hue");
    var_AddCallback(filter, "hue", HueCallback, &sys->hue);
    atomic_init(&sys->hue, vlc_to_vdp_hue(i));

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    var_DelCallback(filter, "hue", HueCallback, &sys->hue);
    var_DelCallback(filter, "saturation", SaturationCallback,
                    &sys->saturation);
    var_DelCallback(filter, "contrast", ContrastCallback, &sys->contrast);
    var_DelCallback(filter, "brightness", BrightnessCallback,
                    &sys->brightness);
    free(sys);
}

vlc_module_begin()
    set_description(N_("VDPAU adjust video filter"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video filter", 0)
    add_shortcut("adjust")
    set_callbacks(Open, Close)
vlc_module_end()
