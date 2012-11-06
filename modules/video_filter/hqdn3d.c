/*****************************************************************************
 * hqdn3d.c : high-quality denoise 3D ported from MPlayer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Cheng Sun <chengsun9@gmail.com>
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
#include <vlc_filter.h>
#include "filter_picture.h"


#include "hqdn3d.h"

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Open         (vlc_object_t *);
static void Close        (vlc_object_t *);
static picture_t *Filter (filter_t *, picture_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FILTER_PREFIX       "hqdn3d-"

#define LUMA_SPAT_TEXT          N_("Spatial luma strength (0-254)")
#define LUMA_SPAT_LONGTEXT      N_("Spatial luma strength (default 4)")
#define CHROMA_SPAT_TEXT        N_("Spatial chroma strength (0-254)")
#define CHROMA_SPAT_LONGTEXT    N_("Spatial chroma strength (default 3)")
#define LUMA_TEMP_TEXT          N_("Temporal luma strength (0-254)")
#define LUMA_TEMP_LONGTEXT      N_("Temporal luma strength (default 6)")
#define CHROMA_TEMP_TEXT        N_("Temporal chroma strength (0-254)")
#define CHROMA_TEMP_LONGTEXT    N_("Temporal chroma strength (default 4.5)")

vlc_module_begin()
    set_shortname(N_("HQ Denoiser 3D"))
    set_description(N_("High Quality 3D Denoiser filter"))
    set_capability("video filter2", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_float_with_range(FILTER_PREFIX "luma-spat", 4.0, 0.0, 254.0,
            LUMA_SPAT_TEXT, LUMA_SPAT_LONGTEXT, false)
    add_float_with_range(FILTER_PREFIX "chroma-spat", 3.0, 0.0, 254.0,
            CHROMA_SPAT_TEXT, CHROMA_SPAT_LONGTEXT, false)
    add_float_with_range(FILTER_PREFIX "luma-temp", 6.0, 0.0, 254.0,
            LUMA_TEMP_TEXT, LUMA_TEMP_LONGTEXT, false)
    add_float_with_range(FILTER_PREFIX "chroma-temp", 4.5, 0.0, 254.0,
            CHROMA_TEMP_TEXT, CHROMA_TEMP_LONGTEXT, false)

    add_shortcut("hqdn3d")

    set_callbacks(Open, Close)
vlc_module_end()

static const char *const filter_options[] = {
    "luma-spat", "chroma-spat", "luma-temp", "chroma-temp", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    const vlc_chroma_description_t *chroma;
    int w[3], h[3];

    float luma_spat;
    float chroma_spat;
    float luma_temp;
    float chroma_temp;

    struct vf_priv_s cfg;
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open(vlc_object_t *this)
{
    filter_t *filter = (filter_t *)this;
    filter_sys_t *sys;
    struct vf_priv_s *cfg;
    const video_format_t *fmt_in  = &filter->fmt_in.video;
    const video_format_t *fmt_out = &filter->fmt_out.video;
    const vlc_fourcc_t fourcc_in  = fmt_in->i_chroma;
    const vlc_fourcc_t fourcc_out = fmt_out->i_chroma;
    int wmax = 0;

    const vlc_chroma_description_t *chroma =
            vlc_fourcc_GetChromaDescription(fourcc_in);
    if (!chroma || chroma->plane_count != 3 || chroma->pixel_size != 1) {
        msg_Err(filter, "Unsupported chroma (%4.4s)", (char*)&fourcc_in);
        return VLC_EGENERIC;
    }

    if (fourcc_in != fourcc_out) {
        msg_Err(filter, "Input and output chromas don't match");
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    sys = calloc(1, sizeof(filter_sys_t));
    if (!sys) {
        return VLC_ENOMEM;
    }
    cfg = &sys->cfg;

    sys->chroma = chroma;

    for (int i = 0; i < 3; ++i) {
        sys->w[i] = fmt_in->i_width  * chroma->p[i].w.num / chroma->p[i].w.den;
        if (sys->w[i] > wmax) wmax = sys->w[i];
        sys->h[i] = fmt_out->i_height * chroma->p[i].h.num / chroma->p[i].h.den;
    }
    cfg->Line = malloc(wmax*sizeof(int));
    if (!cfg->Line) {
        free(sys);
        return VLC_ENOMEM;
    }

    filter->p_sys = sys;
    filter->pf_video_filter = Filter;

    config_ChainParse(filter, FILTER_PREFIX, filter_options,
                      filter->p_cfg);

    sys->luma_spat =
            var_CreateGetFloatCommand(filter, FILTER_PREFIX "luma-spat");
    sys->chroma_spat =
            var_CreateGetFloatCommand(filter, FILTER_PREFIX "chroma-spat");
    sys->luma_temp =
            var_CreateGetFloatCommand(filter, FILTER_PREFIX "luma-temp");
    sys->chroma_temp =
            var_CreateGetFloatCommand(filter, FILTER_PREFIX "chroma-temp");

    PrecalcCoefs(cfg->Coefs[0], sys->luma_spat);
    PrecalcCoefs(cfg->Coefs[1], sys->luma_temp);
    PrecalcCoefs(cfg->Coefs[2], sys->chroma_spat);
    PrecalcCoefs(cfg->Coefs[3], sys->chroma_temp);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close(vlc_object_t *this)
{
    filter_t *filter = (filter_t *)this;
    filter_sys_t *sys = filter->p_sys;
    struct vf_priv_s *cfg = &sys->cfg;

    for (int i = 0; i < 3; ++i) {
        free(cfg->Frame[i]);
    }
    free(cfg->Line);
    free(sys);
}

/*****************************************************************************
 * Filter
 *****************************************************************************/
static picture_t *Filter(filter_t *filter, picture_t *src)
{
    picture_t *dst;
    filter_sys_t *sys = filter->p_sys;
    struct vf_priv_s *cfg = &sys->cfg;

    if (!src) return NULL;

    dst = filter_NewPicture(filter);
    if (!dst) {
        picture_Release(src);
        return NULL;
    }

    deNoise(src->p[0].p_pixels, dst->p[0].p_pixels,
            cfg->Line, &cfg->Frame[0], sys->w[0], sys->h[0],
            src->p[0].i_pitch, dst->p[0].i_pitch,
            cfg->Coefs[0],
            cfg->Coefs[0],
            cfg->Coefs[1]);
    deNoise(src->p[1].p_pixels, dst->p[1].p_pixels,
            cfg->Line, &cfg->Frame[1], sys->w[1], sys->h[1],
            src->p[1].i_pitch, dst->p[1].i_pitch,
            cfg->Coefs[2],
            cfg->Coefs[2],
            cfg->Coefs[3]);
    deNoise(src->p[2].p_pixels, dst->p[2].p_pixels,
            cfg->Line, &cfg->Frame[2], sys->w[2], sys->h[2],
            src->p[2].i_pitch, dst->p[2].i_pitch,
            cfg->Coefs[2],
            cfg->Coefs[2],
            cfg->Coefs[3]);

    return CopyInfoAndRelease(dst, src);
}
