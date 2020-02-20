/*****************************************************************************
 * hqdn3d.c : high-quality denoise 3D ported from MPlayer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
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
#include <vlc_picture.h>
#include "filter_picture.h"


#include "hqdn3d.h"

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Open         (vlc_object_t *);
static void Close        (vlc_object_t *);
static picture_t *Filter (filter_t *, picture_t *);
static int DenoiseCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FILTER_PREFIX       "hqdn3d-"

#define LUMA_SPAT_TEXT          N_("Spatial luma strength (0-254)")
#define CHROMA_SPAT_TEXT        N_("Spatial chroma strength (0-254)")
#define LUMA_TEMP_TEXT          N_("Temporal luma strength (0-254)")
#define CHROMA_TEMP_TEXT        N_("Temporal chroma strength (0-254)")

vlc_module_begin()
    set_shortname(N_("HQ Denoiser 3D"))
    set_description(N_("High Quality 3D Denoiser filter"))
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_float_with_range(FILTER_PREFIX "luma-spat", 4.0, 0.0, 254.0,
            LUMA_SPAT_TEXT, LUMA_SPAT_TEXT, false)
    add_float_with_range(FILTER_PREFIX "chroma-spat", 3.0, 0.0, 254.0,
            CHROMA_SPAT_TEXT, CHROMA_SPAT_TEXT, false)
    add_float_with_range(FILTER_PREFIX "luma-temp", 6.0, 0.0, 254.0,
            LUMA_TEMP_TEXT, LUMA_TEMP_TEXT, false)
    add_float_with_range(FILTER_PREFIX "chroma-temp", 4.5, 0.0, 254.0,
            CHROMA_TEMP_TEXT, CHROMA_TEMP_TEXT, false)

    add_shortcut("hqdn3d")

    set_callbacks(Open, Close)
vlc_module_end()

static const char *const filter_options[] = {
    "luma-spat", "chroma-spat", "luma-temp", "chroma-temp", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
typedef struct
{
    const vlc_chroma_description_t *chroma;
    int w[3], h[3];

    struct vf_priv_s cfg;
    bool   b_recalc_coefs;
    vlc_mutex_t coefs_mutex;
    float  luma_spat, luma_temp, chroma_spat, chroma_temp;
} filter_sys_t;

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
    cfg->Line = malloc(wmax*sizeof(unsigned int));
    if (!cfg->Line) {
        free(sys);
        return VLC_ENOMEM;
    }

    config_ChainParse(filter, FILTER_PREFIX, filter_options,
                      filter->p_cfg);


    vlc_mutex_init( &sys->coefs_mutex );
    sys->b_recalc_coefs = true;
    sys->luma_spat = var_CreateGetFloatCommand(filter, FILTER_PREFIX "luma-spat");
    sys->chroma_spat = var_CreateGetFloatCommand(filter, FILTER_PREFIX "chroma-spat");
    sys->luma_temp = var_CreateGetFloatCommand(filter, FILTER_PREFIX "luma-temp");
    sys->chroma_temp = var_CreateGetFloatCommand(filter, FILTER_PREFIX "chroma-temp");

    filter->p_sys = sys;
    filter->pf_video_filter = Filter;

    var_AddCallback( filter, FILTER_PREFIX "luma-spat", DenoiseCallback, sys );
    var_AddCallback( filter, FILTER_PREFIX "chroma-spat", DenoiseCallback, sys );
    var_AddCallback( filter, FILTER_PREFIX "luma-temp", DenoiseCallback, sys );
    var_AddCallback( filter, FILTER_PREFIX "chroma-temp", DenoiseCallback, sys );

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

    var_DelCallback( filter, FILTER_PREFIX "luma-spat", DenoiseCallback, sys );
    var_DelCallback( filter, FILTER_PREFIX "chroma-spat", DenoiseCallback, sys );
    var_DelCallback( filter, FILTER_PREFIX "luma-temp", DenoiseCallback, sys );
    var_DelCallback( filter, FILTER_PREFIX "chroma-temp", DenoiseCallback, sys );

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
    bool recalc = false;

    if (!src) return NULL;

    dst = filter_NewPicture(filter);
    if ( unlikely(!dst) ) {
        picture_Release(src);
        return NULL;
    }
    vlc_mutex_lock( &sys->coefs_mutex );
    recalc = sys->b_recalc_coefs;
    sys->b_recalc_coefs = false;

    if( unlikely( recalc ) )
    {
        msg_Dbg( filter, "Changing coefs to %.2f %.2f %.2f %.2f",
                            sys->luma_spat, sys->luma_temp, sys->chroma_spat, sys->chroma_temp );
        PrecalcCoefs(cfg->Coefs[0], sys->luma_spat);
        PrecalcCoefs(cfg->Coefs[1], sys->luma_temp);
        PrecalcCoefs(cfg->Coefs[2], sys->chroma_spat);
        PrecalcCoefs(cfg->Coefs[3], sys->chroma_temp);
    }
    vlc_mutex_unlock( &sys->coefs_mutex );

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

    if(unlikely(!cfg->Frame[0] || !cfg->Frame[1] || !cfg->Frame[2]))
    {
        picture_Release( src );
        picture_Release( dst );
        return NULL;
    }

    return CopyInfoAndRelease(dst, src);
}


static int DenoiseCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    filter_sys_t *sys = (filter_sys_t*)p_data;

    /* Just take values and flag for recalc so we don't block UI thread calling this
     * and don't right thread safety calcing coefs in here without mutex*/
    vlc_mutex_lock( &sys->coefs_mutex );
    if( !strcmp( psz_var, FILTER_PREFIX "luma-spat") )
        sys->luma_spat = newval.f_float;
    else if( !strcmp( psz_var, FILTER_PREFIX "luma-temp") )
        sys->luma_temp = newval.f_float;
    else if( !strcmp( psz_var, FILTER_PREFIX "chroma-temp") )
        sys->chroma_spat = newval.f_float;
    else if( !strcmp( psz_var, FILTER_PREFIX "chroma-spat") )
        sys->chroma_temp = newval.f_float;
    sys->b_recalc_coefs = true;
    vlc_mutex_unlock( &sys->coefs_mutex );

    return VLC_SUCCESS;
}
