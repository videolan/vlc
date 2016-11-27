/*****************************************************************************
 * sharpen.c: VDPAU sharpen video filter
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

#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_atomic.h>
#include "vlc_vdpau.h"

struct filter_sys_t
{
    atomic_uint_fast32_t sigma;
};

static float vlc_to_vdp_sigma(float sigma)
{
    sigma /= 2.f;
    if (sigma > +1.f)
        sigma = +1.f;
    if (sigma < -1.f)
        sigma = -1.f;
    return sigma;
}

static int SharpenCallback(vlc_object_t *obj, const char *varname,
                           vlc_value_t prev, vlc_value_t cur, void *data)
{
    filter_sys_t *sys = data;
    union { uint32_t u; float f; } u;

    u.f = vlc_to_vdp_sigma(cur.f_float);
    atomic_store(&sys->sigma, u.u);
    (void) obj; (void) varname; (void) prev;
    return VLC_SUCCESS;
}

static picture_t *Sharpen(filter_t *filter, picture_t *pic)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_vdp_video_field_t *f = (vlc_vdp_video_field_t *)pic->context;
    union { uint32_t u; float f; } u;

    if (unlikely(f == NULL))
        return pic;

    u.u = atomic_load(&sys->sigma);
    f->sharpen += u.f;
    if (f->sharpen > +1.f)
        f->sharpen = +1.f;
    if (f->sharpen < -1.f)
        f->sharpen = -1.f;

    return pic;
}

static const char *const options[] = { "sigma", NULL };

static int Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_420
     && filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_422
     && filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_444)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    /* Check for sharpen support */
    vdp_t *vdp;
    VdpDevice device;
    VdpStatus err;
    VdpBool ok;

    err = vdp_get_x11(NULL, -1, &vdp, &device);
    if (err != VDP_STATUS_OK)
        return VLC_EGENERIC; /* Weird. The decoder should be active... */

    err = vdp_video_mixer_query_feature_support(vdp, device,
                                       VDP_VIDEO_MIXER_FEATURE_SHARPNESS, &ok);
    if (err != VDP_STATUS_OK)
        ok = VDP_FALSE;
    vdp_release_x11(vdp);

    if (ok != VDP_TRUE)
    {
        msg_Err(filter, "sharpening/blurring not supported by VDPAU device");
        return VLC_EGENERIC;
    }

    /* Initialization */
    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    filter->pf_video_filter = Sharpen;
    filter->p_sys = sys;

    config_ChainParse(filter, "sharpen-", options, filter->p_cfg);
    var_AddCallback(filter, "sharpen-sigma", SharpenCallback, sys);

    union { uint32_t u; float f; } u;
    u.f = vlc_to_vdp_sigma(var_CreateGetFloatCommand(filter, "sharpen-sigma"));
    atomic_init(&sys->sigma, u.u);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    var_DelCallback(filter, "sharpen-sigma", SharpenCallback, sys);
    free(sys);
}

vlc_module_begin()
    set_description(N_("VDPAU sharpen video filter"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video filter", 0)
    add_shortcut("sharpen")
    set_callbacks(Open, Close)
vlc_module_end()
