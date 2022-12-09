/*****************************************************************************
 * filters.c: VAAPI filters
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Author: Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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
# include <config.h>
#endif

#include <assert.h>
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>
#include "filters.h"

/********************************
 * Common structures and macros *
 ********************************/

struct  va_filter_desc
{
    vlc_decoder_device *dec_device;
    VADisplay           dpy;
    VAConfigID          conf;
    VAContextID         ctx;
    VABufferID          buf;
    VASurfaceID *       surface_ids;
};

typedef struct
{
    struct va_filter_desc       va;
    picture_pool_t *            dest_pics;
    bool                        b_pipeline_fast;
    void *                      p_data;
} filter_sys_t;

#define DEST_PICS_POOL_SZ       3

struct  range
{
    float       min_value;
    float       max_value;
};

#define GET_DRV_SIGMA(vlc_sigma, vlc_range, drv_range)                  \
    ((vlc_sigma - (vlc_range).min_value) *                              \
     ((drv_range).max_value - (drv_range).min_value) /                  \
     ((vlc_range).max_value - (vlc_range).min_value) + (drv_range).min_value)

/***********************************
 * Adjust structures and constants *
 ***********************************/

enum { ADJUST_CONT, ADJUST_LUM, ADJUST_HUE, ADJUST_SAT, NUM_ADJUST_MODES };

static VAProcColorBalanceType const     va_adjust_modes[NUM_ADJUST_MODES] =
{
    [ADJUST_CONT] = VAProcColorBalanceContrast,
    [ADJUST_LUM] = VAProcColorBalanceBrightness,
    [ADJUST_HUE] = VAProcColorBalanceHue,
    [ADJUST_SAT] = VAProcColorBalanceSaturation
};

static char const       adjust_params_names[NUM_ADJUST_MODES][11] =
{
    [ADJUST_CONT] = "contrast",
    [ADJUST_LUM] = "brightness",
    [ADJUST_HUE] = "hue",
    [ADJUST_SAT] = "saturation"
};

static struct range const       vlc_adjust_sigma_ranges[NUM_ADJUST_MODES] =
{
    [ADJUST_CONT] = { .0f,    2.f    },
    [ADJUST_LUM]  = { .0f,    2.f    },
    [ADJUST_HUE]  = { -180.f, +180.f },
    [ADJUST_SAT]  = { .0f,    3.f    }
};

struct  adjust_params
{
    struct
    {
        _Atomic float           drv_value;
        VAProcFilterValueRange  drv_range;
        bool                    is_available;
    } sigma[NUM_ADJUST_MODES];
};

struct  adjust_data
{
    struct adjust_params  params;
    unsigned int          num_available_modes;
};

/* Adaptation of VAAPI adjust filter so it looks more like the CPU one */
static float
adapt_adjust_sigma(char const * psz_var, float const sigma,
                   struct range const * p_range)
{
    if (!strcmp(psz_var, "contrast"))
    {
        struct range const      adapt_range = { .0f, .35f };
        return GET_DRV_SIGMA(sigma, *p_range, adapt_range);
    }
    else if (!strcmp(psz_var, "saturation"))
    {
        struct range const      adapt_range = { .0f, 1.f };
        return GET_DRV_SIGMA(sigma, *p_range, adapt_range);
    }
    return sigma;
}

/*****************************************
 * Basic filter structures and constants *
 *****************************************/

static struct range const       vlc_denoise_sigma_range = { .0f, 2.f };
static struct range const       vlc_sharpen_sigma_range = { .0f, 2.f };

struct  basic_filter_data
{
    struct
    {
        _Atomic float           drv_value;
        VAProcFilterValueRange  drv_range;
        struct range const *    p_vlc_range;
        const char *            psz_name;
    } sigma;

    VAProcFilterType    filter_type;
};

/****************************************
 * Deinterlace structures and constants *
 ****************************************/

struct  deint_mode
{
    char                        name[5];
    VAProcDeinterlacingType     type;
    bool                        b_double_rate;
};

static struct deint_mode const  deint_modes[] =
{
    { "x",     VAProcDeinterlacingMotionAdaptive,     true },
    { "x",     VAProcDeinterlacingMotionCompensated,  true },
    { "bob",   VAProcDeinterlacingBob,                true },
    { "mean",  VAProcDeinterlacingWeave,              false }
};

#define METADATA_SIZE 3

struct  deint_data
{
    struct
    {
        picture_t **        pp_pics;
        picture_t **        pp_cur_pic;
        unsigned int        num_pics;
        unsigned int        sz;
    } history;

    struct
    {
        VASurfaceID *   surfaces;
        unsigned int    sz;
    } backward_refs, forward_refs;

    struct
    {
        vlc_tick_t date;
        int     i_nb_fields;
    } meta[METADATA_SIZE];

    bool                b_double_rate;
    unsigned int        cur_frame;
};

/********************
 * Common functions *
 ********************/

static picture_t *
Filter(filter_t * filter, picture_t * src,
       void (*pf_update_va_filter_params)(void *, void *),
       void (*pf_prepare_render_surface)(void *),
       void (*pf_update_pipeline_params)(void *,
                                         VAProcPipelineParameterBuffer *))
{
    filter_sys_t *const filter_sys = filter->p_sys;
    VABufferID          pipeline_buf = VA_INVALID_ID;
    picture_t *const    dest = picture_pool_Wait(filter_sys->dest_pics);
    if (!dest)
        return NULL;

    vlc_vaapi_PicAttachContext(dest);
    picture_CopyProperties(dest, src);

    void *      p_va_params;

    if (vlc_vaapi_MapBuffer(VLC_OBJECT(filter), filter_sys->va.dpy,
                            filter_sys->va.buf, &p_va_params))
        goto error;

    if (pf_update_va_filter_params)
        pf_update_va_filter_params(filter_sys->p_data, p_va_params);

    if (vlc_vaapi_UnmapBuffer(VLC_OBJECT(filter),
                              filter_sys->va.dpy, filter_sys->va.buf))
        goto error;

    if (vlc_vaapi_BeginPicture(VLC_OBJECT(filter),
                               filter_sys->va.dpy, filter_sys->va.ctx,
                               vlc_vaapi_PicGetSurface(dest)))
        goto error;

    if (pf_prepare_render_surface)
        pf_prepare_render_surface(filter_sys->p_data);

    VAProcPipelineParameterBuffer *     pipeline_params;

    pipeline_buf =
        vlc_vaapi_CreateBuffer(VLC_OBJECT(filter),
                               filter_sys->va.dpy, filter_sys->va.ctx,
                               VAProcPipelineParameterBufferType,
                               sizeof(*pipeline_params), 1, NULL);
    if (pipeline_buf == VA_INVALID_ID)
        goto error;

    if (vlc_vaapi_MapBuffer(VLC_OBJECT(filter), filter_sys->va.dpy,
                            pipeline_buf, (void **)&pipeline_params))
        goto error;

    *pipeline_params = (typeof(*pipeline_params)){0};
    pipeline_params->surface = vlc_vaapi_PicGetSurface(src);
    pipeline_params->filters = &filter_sys->va.buf;
    pipeline_params->num_filters = 1;
    if (filter_sys->b_pipeline_fast)
        pipeline_params->pipeline_flags = VA_PROC_PIPELINE_FAST;
    if (pf_update_pipeline_params)
        pf_update_pipeline_params(filter_sys->p_data, pipeline_params);

    if (vlc_vaapi_UnmapBuffer(VLC_OBJECT(filter),
                              filter_sys->va.dpy, pipeline_buf))
        goto error;

    if (vlc_vaapi_RenderPicture(VLC_OBJECT(filter),
                                filter_sys->va.dpy, filter_sys->va.ctx,
                                &pipeline_buf, 1))
        goto error;

    if (vlc_vaapi_EndPicture(VLC_OBJECT(filter),
                             filter_sys->va.dpy, filter_sys->va.ctx))
        goto error;

    return dest;

error:
    if (pipeline_buf != VA_INVALID_ID)
        vlc_vaapi_DestroyBuffer(VLC_OBJECT(filter),
                                filter_sys->va.dpy, pipeline_buf);
    if (dest)
        picture_Release(dest);
    return NULL;
}

static int
Open(filter_t * filter,
     VAProcFilterType const filter_type,
     VAProcPipelineCaps * p_pipeline_caps,
     void * p_data,
     int (*pf_init_filter_params)(filter_t *, void *, void **,
                                  unsigned int *, unsigned int *),
     int (*pf_use_pipeline_caps)(void *, VAProcPipelineCaps const *))
{
    filter_sys_t *      filter_sys;

    if (filter->vctx_in == NULL ||
        vlc_video_context_GetType(filter->vctx_in) != VLC_VIDEO_CONTEXT_VAAPI)
    if (!vlc_vaapi_IsChromaOpaque(filter->fmt_out.video.i_chroma) ||
        !video_format_IsSimilar(&filter->fmt_out.video, &filter->fmt_in.video))
        return VLC_EGENERIC;

    filter_sys = calloc(1, sizeof(*filter_sys));
    if (!filter_sys)
        return VLC_ENOMEM;
    filter->p_sys = filter_sys;

    filter_sys->p_data = p_data;

    filter_sys->va.conf = VA_INVALID_ID;
    filter_sys->va.ctx = VA_INVALID_ID;
    filter_sys->va.buf = VA_INVALID_ID;
    filter_sys->va.dec_device = vlc_video_context_HoldDevice(filter->vctx_in);
    filter_sys->va.dpy = filter_sys->va.dec_device->opaque;
    assert(filter_sys->va.dec_device);

    filter_sys->dest_pics =
        vlc_vaapi_PoolNew(VLC_OBJECT(filter), filter->vctx_in,
                          filter_sys->va.dpy, DEST_PICS_POOL_SZ,
                          &filter_sys->va.surface_ids, &filter->fmt_out.video);
    if (!filter_sys->dest_pics)
        goto error;

    filter_sys->va.conf =
        vlc_vaapi_CreateConfigChecked(VLC_OBJECT(filter), filter_sys->va.dpy,
                                      VAProfileNone, VAEntrypointVideoProc,
                                      filter->fmt_out.video.i_chroma);
    if (filter_sys->va.conf == VA_INVALID_ID)
        goto error;

    filter_sys->va.ctx =
        vlc_vaapi_CreateContext(VLC_OBJECT(filter),
                                filter_sys->va.dpy, filter_sys->va.conf,
                                filter->fmt_out.video.i_width,
                                filter->fmt_out.video.i_height,
                                0, filter_sys->va.surface_ids,
                                DEST_PICS_POOL_SZ);
    if (filter_sys->va.ctx == VA_INVALID_ID)
        goto error;

    if (vlc_vaapi_IsVideoProcFilterAvailable(VLC_OBJECT(filter),
                                             filter_sys->va.dpy,
                                             filter_sys->va.ctx,
                                             filter_type))
        goto error;

    void *      p_va_params;
    uint32_t    i_sz_param;
    uint32_t    i_num_params;

    if (pf_init_filter_params(filter, p_data,
                              &p_va_params, &i_sz_param, &i_num_params))
        goto error;

    filter_sys->va.buf =
        vlc_vaapi_CreateBuffer(VLC_OBJECT(filter),
                               filter_sys->va.dpy, filter_sys->va.ctx,
                               VAProcFilterParameterBufferType,
                               i_sz_param, i_num_params, p_va_params);
    free(p_va_params);
    if (filter_sys->va.buf == VA_INVALID_ID)
        goto error;

    if (vlc_vaapi_QueryVideoProcPipelineCaps(VLC_OBJECT(filter),
                                             filter_sys->va.dpy,
                                             filter_sys->va.ctx,
                                             &filter_sys->va.buf,
                                             1, p_pipeline_caps))
        goto error;

    filter_sys->b_pipeline_fast =
        p_pipeline_caps->pipeline_flags & VA_PROC_PIPELINE_FAST;

    if (pf_use_pipeline_caps &&
        pf_use_pipeline_caps(p_data, p_pipeline_caps))
        goto error;

    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);

    return VLC_SUCCESS;

error:
    if (filter_sys->va.buf != VA_INVALID_ID)
        vlc_vaapi_DestroyBuffer(VLC_OBJECT(filter),
                                filter_sys->va.dpy, filter_sys->va.buf);
    if (filter_sys->va.ctx != VA_INVALID_ID)
        vlc_vaapi_DestroyContext(VLC_OBJECT(filter),
                                 filter_sys->va.dpy, filter_sys->va.ctx);
    if (filter_sys->va.conf != VA_INVALID_ID)
        vlc_vaapi_DestroyConfig(VLC_OBJECT(filter),
                                filter_sys->va.dpy, filter_sys->va.conf);
    if (filter_sys->dest_pics)
        picture_pool_Release(filter_sys->dest_pics);
    if (filter_sys->va.dec_device)
        vlc_decoder_device_Release(filter_sys->va.dec_device);
    free(filter_sys);
    return VLC_EGENERIC;
}

static void
Close(filter_t *filter, filter_sys_t * filter_sys)
{
    vlc_object_t * obj = VLC_OBJECT(filter);
    picture_pool_Release(filter_sys->dest_pics);
    vlc_vaapi_DestroyBuffer(obj, filter_sys->va.dpy, filter_sys->va.buf);
    vlc_vaapi_DestroyContext(obj, filter_sys->va.dpy, filter_sys->va.ctx);
    vlc_vaapi_DestroyConfig(obj, filter_sys->va.dpy, filter_sys->va.conf);
    vlc_decoder_device_Release(filter_sys->va.dec_device);
    vlc_video_context_Release(filter->vctx_out);
    free(filter_sys);
}

static int
FilterCallback(vlc_object_t * obj, char const * psz_var,
               vlc_value_t oldval, vlc_value_t newval,
               void * p_data)
{ VLC_UNUSED(obj); VLC_UNUSED(oldval);
    struct range const *                p_vlc_range;
    VAProcFilterValueRange const *      p_drv_range;
    _Atomic float *                     p_drv_value;
    bool                                b_found = false;
    bool                                b_adjust = false;

    for (unsigned int i = 0; i < NUM_ADJUST_MODES; ++i)
        if (!strcmp(psz_var, adjust_params_names[i]))
        {
            struct adjust_data *const   p_adjust_data = p_data;

            if (!p_adjust_data->params.sigma[i].is_available)
                return VLC_EGENERIC;

            p_vlc_range = vlc_adjust_sigma_ranges + i;
            p_drv_range = &p_adjust_data->params.sigma[i].drv_range;
            p_drv_value = &p_adjust_data->params.sigma[i].drv_value;

            b_adjust = true;
            b_found = true;
        }
    if (!b_found)
        if (!strcmp(psz_var, "denoise-sigma") ||
            !strcmp(psz_var, "sharpen-sigma"))
        {
            struct basic_filter_data *const     p_basic_filter_data = p_data;

            p_vlc_range = p_basic_filter_data->sigma.p_vlc_range;
            p_drv_range = &p_basic_filter_data->sigma.drv_range;
            p_drv_value = &p_basic_filter_data->sigma.drv_value;

            b_found = true;
        }

    if (!b_found)
        return VLC_EGENERIC;

    float       vlc_sigma = VLC_CLIP(newval.f_float,
                                     p_vlc_range->min_value,
                                     p_vlc_range->max_value);

    if (b_adjust)
        vlc_sigma = adapt_adjust_sigma(psz_var, vlc_sigma, p_vlc_range);

    float const drv_sigma = GET_DRV_SIGMA(vlc_sigma,
                                          *p_vlc_range, *p_drv_range);

    atomic_store_explicit(p_drv_value, drv_sigma, memory_order_relaxed);

    return VLC_SUCCESS;
}

/********************
 * Adjust functions *
 ********************/

static void
Adjust_UpdateVAFilterParams(void * p_data, void * va_params)
{
    const struct adjust_data *const   p_adjust_data = p_data;
    const struct adjust_params *const p_adjust_params = &p_adjust_data->params;
    VAProcFilterParameterBufferColorBalance *const p_va_params = va_params;

    unsigned int i = 0;
    for (unsigned int j = 0; j < NUM_ADJUST_MODES; ++j)
        if (p_adjust_params->sigma[j].is_available)
            p_va_params[i++].value =
                atomic_load_explicit(&p_adjust_params->sigma[j].drv_value,
                                     memory_order_relaxed);
}

static picture_t *
Adjust(filter_t * filter, picture_t * src)
{
    if (src == NULL) return NULL;
    picture_t *const    dest =
        Filter(filter, src, Adjust_UpdateVAFilterParams, NULL, NULL);
    picture_Release(src);
    return dest;
}

static int
OpenAdjust_InitFilterParams(filter_t * filter, void * p_data,
                            void ** pp_va_params,
                            uint32_t * p_va_param_sz,
                            uint32_t * p_num_va_params)
{
    struct adjust_data *const   p_adjust_data = p_data;
    struct adjust_params *const p_adjust_params = &p_adjust_data->params;
    filter_sys_t *const         filter_sys = filter->p_sys;
    VAProcFilterCapColorBalance caps[VAProcColorBalanceCount];
    unsigned int                num_caps = VAProcColorBalanceCount;

    if (vlc_vaapi_QueryVideoProcFilterCaps(VLC_OBJECT(filter),
                                           filter_sys->va.dpy,
                                           filter_sys->va.ctx,
                                           VAProcFilterColorBalance,
                                           caps, &num_caps))
        return VLC_EGENERIC;

    for (unsigned int i = 0; i < num_caps; ++i)
    {
        unsigned int    j;

        for (j = 0; j < num_caps; ++j)
            if (caps[j].type == va_adjust_modes[i])
            {
                float   vlc_sigma =
                    VLC_CLIP(var_InheritFloat(filter, adjust_params_names[i]),
                             vlc_adjust_sigma_ranges[i].min_value,
                             vlc_adjust_sigma_ranges[i].max_value);
                vlc_sigma =
                    adapt_adjust_sigma(adjust_params_names[i],
                                       vlc_sigma, vlc_adjust_sigma_ranges + i);

                p_adjust_params->sigma[i].drv_range = caps[j].range;
                p_adjust_params->sigma[i].is_available = true;
                ++p_adjust_data->num_available_modes;

                float const     drv_sigma =
                    GET_DRV_SIGMA(vlc_sigma, vlc_adjust_sigma_ranges[i],
                                  p_adjust_params->sigma[i].drv_range);

                atomic_init(&p_adjust_params->sigma[i].drv_value, drv_sigma);
                break;
            }
    }

    VAProcFilterParameterBufferColorBalance *   p_va_params;

    *p_va_param_sz = sizeof(typeof(*p_va_params));
    *p_num_va_params = p_adjust_data->num_available_modes;

    p_va_params = calloc(*p_num_va_params, *p_va_param_sz);
    if (!p_va_params)
        return VLC_ENOMEM;

    unsigned int i = 0;
    for (unsigned int j = 0; j < NUM_ADJUST_MODES; ++j)
        if (p_adjust_params->sigma[j].is_available)
        {
            p_va_params[i].type = VAProcFilterColorBalance;
            p_va_params[i++].attrib = va_adjust_modes[j];
        }

    *pp_va_params = p_va_params;

    return VLC_SUCCESS;
}

static void
CloseAdjust(filter_t *filter)
{
    filter_sys_t *const filter_sys = filter->p_sys;

    for (unsigned int i = 0; i < NUM_ADJUST_MODES; ++i)
    {
        var_DelCallback(filter, adjust_params_names[i],
                        FilterCallback, filter_sys->p_data);
        var_Destroy(filter, adjust_params_names[i]);
    }
    free(filter_sys->p_data);
    Close(filter, filter_sys);
}

static const struct vlc_filter_operations Adjust_ops = {
    .filter_video = Adjust, .close = CloseAdjust,
};

static int
OpenAdjust(filter_t *filter)
{
    VAProcPipelineCaps          pipeline_caps;
    struct adjust_data *const   p_data = calloc(1, sizeof(*p_data));
    if (!p_data)
        return VLC_ENOMEM;

    for (unsigned int i = 0; i < NUM_ADJUST_MODES; ++i)
        var_Create(filter, adjust_params_names[i],
                   VLC_VAR_FLOAT | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND);

    if (Open(filter, VAProcFilterColorBalance, &pipeline_caps, p_data,
             OpenAdjust_InitFilterParams, NULL))
        goto error;

    for (unsigned int i = 0; i < NUM_ADJUST_MODES; ++i)
        var_AddCallback(filter, adjust_params_names[i], FilterCallback, p_data);

    filter->ops = &Adjust_ops;

    return VLC_SUCCESS;

error:
    for (unsigned int i = 0; i < NUM_ADJUST_MODES; ++i)
        var_Destroy(filter, adjust_params_names[i]);
    free(p_data);
    return VLC_EGENERIC;
}

/***************************
 * Basic filters functions *
 ***************************/

static void
BasicFilter_UpdateVAFilterParams(void * p_data, void * va_params)
{
    const struct basic_filter_data *const p_basic_filter_data = p_data;
    const _Atomic float *drv_value = &p_basic_filter_data->sigma.drv_value;
    VAProcFilterParameterBuffer *const  p_va_param = va_params;

    p_va_param->value = atomic_load_explicit(drv_value, memory_order_relaxed);
}

static picture_t *
BasicFilter(filter_t * filter, picture_t * src)
{
    if (src == NULL) return NULL;
    picture_t *const    dest =
        Filter(filter, src, BasicFilter_UpdateVAFilterParams, NULL, NULL);
    picture_Release(src);
    return dest;
}

static int
OpenBasicFilter_InitFilterParams(filter_t * filter, void * p_data,
                                 void ** pp_va_params,
                                 uint32_t * p_va_param_sz,
                                 uint32_t * p_num_va_params)
{
    struct basic_filter_data *const     p_basic_filter_data = p_data;
    filter_sys_t *const                 filter_sys = filter->p_sys;
    VAProcFilterCap                     caps;
    unsigned int                        num_caps = 1;

    if (vlc_vaapi_QueryVideoProcFilterCaps(VLC_OBJECT(filter),
                                           filter_sys->va.dpy,
                                           filter_sys->va.buf,
                                           p_basic_filter_data->filter_type,
                                           &caps, &num_caps)
        || !num_caps)
        return VLC_EGENERIC;

    float const vlc_sigma =
        VLC_CLIP(var_InheritFloat(filter, p_basic_filter_data->sigma.psz_name),
                 p_basic_filter_data->sigma.p_vlc_range->min_value,
                 p_basic_filter_data->sigma.p_vlc_range->max_value);

    p_basic_filter_data->sigma.drv_range = caps.range;

    float const drv_sigma =
        GET_DRV_SIGMA(vlc_sigma, *p_basic_filter_data->sigma.p_vlc_range,
                      p_basic_filter_data->sigma.drv_range);

    atomic_init(&p_basic_filter_data->sigma.drv_value, drv_sigma);

    VAProcFilterParameterBuffer *       p_va_param;

    *p_va_param_sz = sizeof(*p_va_param);
    *p_num_va_params = 1;

    p_va_param = calloc(1, sizeof(*p_va_param));
    if (!p_va_param)
        return VLC_ENOMEM;

    p_va_param->type = p_basic_filter_data->filter_type;
    *pp_va_params = p_va_param;

    return VLC_SUCCESS;
}

static void
CloseBasicFilter(filter_t *filter)
{
    filter_sys_t *const                 filter_sys = filter->p_sys;
    struct basic_filter_data *const     p_data = filter_sys->p_data;

    var_DelCallback(filter, p_data->sigma.psz_name, FilterCallback, p_data);
    var_Destroy(filter, p_data->sigma.psz_name);
    free(p_data);
    Close(filter, filter_sys);
}

static const struct vlc_filter_operations BasicFilter_ops = {
    .filter_video = BasicFilter, .close = CloseBasicFilter
};

static int
OpenBasicFilter(filter_t *filter, VAProcFilterType filter_type,
                const char *psz_sigma_name, struct range const *p_vlc_range)
{
    VAProcPipelineCaps                  pipeline_caps;
    assert(filter->psz_name);
    struct basic_filter_data *const     p_data = calloc(1, sizeof(*p_data));
    if (!p_data)
        return VLC_ENOMEM;

    p_data->filter_type = filter_type;
    p_data->sigma.psz_name = psz_sigma_name;
    p_data->sigma.p_vlc_range = p_vlc_range;

    var_Create(filter, p_data->sigma.psz_name,
               VLC_VAR_FLOAT | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND);

    if (Open(filter, p_data->filter_type, &pipeline_caps, p_data,
             OpenBasicFilter_InitFilterParams, NULL))
        goto error;

    var_AddCallback(filter, p_data->sigma.psz_name, FilterCallback, p_data);

    filter->ops = &BasicFilter_ops;

    return VLC_SUCCESS;

error:
    var_Destroy(filter, p_data->sigma.psz_name);
    free(p_data);
    return VLC_EGENERIC;
}

static int
OpenDenoiseFilter(filter_t *filter)
{
    return OpenBasicFilter(filter, VAProcFilterNoiseReduction, "denoise-sigma",
                           &vlc_denoise_sigma_range);
}

static int
OpenSharpenFilter(filter_t *filter)
{
    return OpenBasicFilter(filter, VAProcFilterSharpening, "sharpen-sigma",
                           &vlc_sharpen_sigma_range);
}

/*************************
 * Deinterlace functions *
 *************************/

static picture_t *
Deinterlace_UpdateHistory(struct deint_data * p_deint_data, picture_t * src)
{
    if (p_deint_data->history.num_pics == p_deint_data->history.sz)
    {
        picture_Release(*p_deint_data->history.pp_pics);
        memmove(p_deint_data->history.pp_pics, p_deint_data->history.pp_pics + 1,
                --p_deint_data->history.num_pics * sizeof(picture_t *));
    }
    p_deint_data->history.pp_pics[p_deint_data->history.num_pics++] = src;

    return *p_deint_data->history.pp_cur_pic;
}

static void
Deinterlace_UpdateFilterParams(void * p_data, void * va_params)
{
    struct deint_data *const    p_deint_data = p_data;
    VAProcFilterParameterBufferDeinterlacing *const      p_va_params = va_params;

    p_va_params->flags =
        p_deint_data->history.pp_cur_pic[0]->b_top_field_first ?
        0 : VA_DEINTERLACING_BOTTOM_FIELD_FIRST;
    if (p_deint_data->cur_frame ==
        (p_deint_data->history.pp_cur_pic[0]->b_top_field_first ? 1 : 0))
        p_va_params->flags |= VA_DEINTERLACING_BOTTOM_FIELD;
}

static void
Deinterlace_UpdateReferenceFrames(void * p_data)
{
    struct deint_data *const    p_deint_data = p_data;

    if (p_deint_data->backward_refs.sz)
        for (unsigned int i = 0; i < p_deint_data->backward_refs.sz; ++i)
        {
            unsigned int const  idx = p_deint_data->forward_refs.sz + 1 + i;

            p_deint_data->backward_refs.surfaces[i] =
                vlc_vaapi_PicGetSurface(p_deint_data->history.pp_pics[idx]);
        }

    if (p_deint_data->forward_refs.sz)
        for (unsigned int i = 0; i < p_deint_data->forward_refs.sz; ++i)
        {
            unsigned int const  idx = p_deint_data->forward_refs.sz - 1 - i;

            p_deint_data->forward_refs.surfaces[i] =
                vlc_vaapi_PicGetSurface(p_deint_data->history.pp_pics[idx]);
        }
}

static void
Deinterlace_UpdatePipelineParams
(void * p_data, VAProcPipelineParameterBuffer * pipeline_param)
{
    struct deint_data *const    p_deint_data = p_data;

    pipeline_param->backward_references = p_deint_data->backward_refs.surfaces;
    pipeline_param->forward_references = p_deint_data->forward_refs.surfaces;
    pipeline_param->num_backward_references = p_deint_data->backward_refs.sz;
    pipeline_param->num_forward_references = p_deint_data->forward_refs.sz;
}

static picture_t *
Deinterlace(filter_t * filter, picture_t * src)
{
    if (src == NULL) return NULL;
    filter_sys_t *const         filter_sys = filter->p_sys;
    struct deint_data *const    p_deint_data = filter_sys->p_data;

    src = Deinterlace_UpdateHistory(p_deint_data, src);
    if (p_deint_data->history.num_pics < p_deint_data->history.sz)
        return NULL;

    picture_t *const    dest =
        Filter(filter, src,
               Deinterlace_UpdateFilterParams,
               Deinterlace_UpdateReferenceFrames,
               Deinterlace_UpdatePipelineParams);

    if (dest)
        dest->b_progressive = true;

    return dest;
}

static picture_t *
DeinterlaceX2(filter_t * filter, picture_t * src)
{
    if (src == NULL) return NULL;
    filter_sys_t *const         filter_sys = filter->p_sys;
    struct deint_data *const    p_deint_data = filter_sys->p_data;
    const video_format_t *      fmt = &filter->fmt_out.video;

    /* TODO: could use the meta array and calculation from deinterlace/common
       but then it would also be appropriate to use the picture history array
       too and the callback system...so a rewrite of this module basically.*/
    for (unsigned int i = 1; i < METADATA_SIZE; ++i)
        p_deint_data->meta[i-1] = p_deint_data->meta[i];
    p_deint_data->meta[METADATA_SIZE-1].date        = src->date;
    p_deint_data->meta[METADATA_SIZE-1].i_nb_fields = src->i_nb_fields;

    picture_t * cur = Deinterlace_UpdateHistory(p_deint_data, src);
    if (p_deint_data->history.num_pics < p_deint_data->history.sz)
        return NULL;

    vlc_tick_t i_field_dur = 0;
    unsigned int i = 0;
    for ( ; i < METADATA_SIZE-1; i ++)
        if (p_deint_data->meta[i].date != VLC_TICK_INVALID)
            break;
    if (i < METADATA_SIZE-1) {
        unsigned int i_fields_total = 0;
        for (unsigned int j = i; j < METADATA_SIZE-1; ++j)
            i_fields_total += p_deint_data->meta[j].i_nb_fields;
        i_field_dur = (src->date - p_deint_data->meta[i].date) / i_fields_total;
    }
    else if (fmt->i_frame_rate_base)
        i_field_dur = vlc_tick_from_samples(fmt->i_frame_rate_base, fmt->i_frame_rate);

    picture_t *dest[2] = {NULL, NULL};
    for (i = 0; i < 2; ++i)
    {
        p_deint_data->cur_frame = i;
        dest[i] = Filter(filter, cur,
                         Deinterlace_UpdateFilterParams,
                         Deinterlace_UpdateReferenceFrames,
                         Deinterlace_UpdatePipelineParams);
        if (!dest[i])
           goto error;

        dest[i]->b_progressive = true;
        dest[i]->i_nb_fields = 1;
    }

    vlc_picture_chain_AppendChain( dest[0], dest[1] );
    dest[0]->date = cur->date;
    if (dest[0]->date != VLC_TICK_INVALID)
        dest[1]->date = dest[0]->date + i_field_dur;
    else
        dest[1]->date = VLC_TICK_INVALID;

    return dest[0];

error:
    for (i = 0; i < 2; ++i)
        if (dest[i])
            picture_Release(dest[i]);

    return NULL;
}

static void
Deinterlace_Flush(filter_t *filter)
{
    filter_sys_t *p_sys = filter->p_sys;
    struct deint_data *const p_deint_data = p_sys->p_data;

    while (p_deint_data->history.num_pics)
    {
        picture_t *     pic =
            p_deint_data->history.pp_pics[--p_deint_data->history.num_pics];
        picture_Release(pic);
    }

    for (unsigned int i = 0; i < METADATA_SIZE; ++i)
    {
        p_deint_data->meta[i].date = VLC_TICK_INVALID;
        p_deint_data->meta[i].i_nb_fields = 2;
    }
}

static inline bool
OpenDeinterlace_IsValidType(filter_t * filter,
                            VAProcFilterCapDeinterlacing const caps[],
                            unsigned int num_caps,
                            struct deint_mode const * deint_mode)
{
    (void) filter;
    for (unsigned int j = 0; j < num_caps; ++j)
        if (caps[j].type == deint_mode->type)
            return true;
    return false;
}

static inline int
OpenDeinterlace_GetMode(filter_t * filter, char const * deint_mode,
                        struct deint_mode * p_deint_mode,
                        VAProcFilterCapDeinterlacing const caps[],
                        unsigned int num_caps)
{
    bool fallback = false;
    if (deint_mode && strcmp(deint_mode, "auto"))
    {
        for (unsigned int i = 0; i < ARRAY_SIZE(deint_modes); ++i)
        {
            if (!strcmp(deint_mode, deint_modes[i].name))
            {
                if (OpenDeinterlace_IsValidType(filter, caps, num_caps,
                                                deint_modes + i))
                {
                    *p_deint_mode = deint_modes[i];
                    msg_Dbg(filter, "using %s deinterlace method",
                            deint_modes[i].name);
                    return VLC_SUCCESS;
                }
            }
        }
        fallback = true;
    }

    for (unsigned int i = 0; i < ARRAY_SIZE(deint_modes); ++i)
        if (OpenDeinterlace_IsValidType(filter, caps, num_caps,
                                        deint_modes + i))
        {
            *p_deint_mode = deint_modes[i];
            if (fallback)
                msg_Info(filter, "%s algorithm not available, falling back to "
                         "%s algorithm", deint_mode, deint_modes[i].name);
            else
                msg_Dbg(filter, "using %s deinterlace method",
                        deint_modes[i].name);
            return VLC_SUCCESS;
        }

    /* We shouldn't be able to reach this, 'cause if there is no deinterlacing
       algorithm available, then the driver would have told us before the
       deinterlace filtering is not supported at all. */

    msg_Err(filter, "no algorithm available");
    return VLC_EGENERIC;
}

static int
OpenDeinterlace_InitFilterParams(filter_t * filter, void * p_data,
                                 void ** pp_va_params,
                                 uint32_t * p_va_param_sz,
                                 uint32_t * p_num_va_params)
{
    struct deint_data *const    p_deint_data = p_data;
    filter_sys_t *const         filter_sys = filter->p_sys;
    VAProcFilterCapDeinterlacing
                                caps[VAProcDeinterlacingCount];
    unsigned int                num_caps = VAProcDeinterlacingCount;

    if (vlc_vaapi_QueryVideoProcFilterCaps(VLC_OBJECT(filter),
                                           filter_sys->va.dpy,
                                           filter_sys->va.buf,
                                           VAProcFilterDeinterlacing,
                                           &caps, &num_caps))
        return VLC_EGENERIC;

    struct deint_mode   deint_mode;
    char *const         psz_deint_mode =
        var_InheritString(filter, "deinterlace-mode");

    int ret = OpenDeinterlace_GetMode(filter, psz_deint_mode,
                                      &deint_mode, caps, num_caps);
    free(psz_deint_mode);
    if (ret)
        return VLC_EGENERIC;

    VAProcFilterParameterBufferDeinterlacing *  p_va_param;

    *p_va_param_sz = sizeof(*p_va_param);
    *p_num_va_params = 1;

    p_va_param = calloc(1, sizeof(*p_va_param));
    if (!p_va_param)
        return VLC_ENOMEM;

    p_va_param->type = VAProcFilterDeinterlacing;
    p_va_param->algorithm = deint_mode.type;
    *pp_va_params = p_va_param;

    p_deint_data->b_double_rate = deint_mode.b_double_rate;

    return VLC_SUCCESS;
}

static int
OpenDeinterlace_InitHistory(void * p_data, VAProcPipelineCaps const * pipeline_caps)
{
    struct deint_data *const    p_deint_data = p_data;
    unsigned int const          sz_backward_refs =
        pipeline_caps->num_backward_references;
    unsigned int const          sz_forward_refs =
        pipeline_caps->num_forward_references;
    unsigned int const          history_sz =
        sz_backward_refs + 1 + sz_forward_refs;

    p_deint_data->history.pp_pics = calloc(history_sz, sizeof(picture_t *));
    if (!p_deint_data->history.pp_pics)
        return VLC_ENOMEM;

    p_deint_data->history.pp_cur_pic =
        p_deint_data->history.pp_pics + sz_forward_refs;
    p_deint_data->history.num_pics = 0;
    p_deint_data->history.sz = history_sz;

    if (history_sz - 1)
    {
        p_deint_data->forward_refs.surfaces =
            vlc_alloc(history_sz - 1, sizeof(VASurfaceID));
        if (!p_deint_data->forward_refs.surfaces)
            return VLC_ENOMEM;
    }

    p_deint_data->backward_refs.surfaces =
        p_deint_data->forward_refs.surfaces + sz_forward_refs;

    p_deint_data->backward_refs.sz = sz_backward_refs;
    p_deint_data->forward_refs.sz = sz_forward_refs;

    return VLC_SUCCESS;
}

static void CloseDeinterlace(filter_t *filter)
{
    filter_sys_t *const         filter_sys = filter->p_sys;
    struct deint_data *const    p_data = filter_sys->p_data;

    Deinterlace_Flush(filter);
    free(p_data->forward_refs.surfaces);
    free(p_data->history.pp_pics);
    free(p_data);
    Close(filter, filter_sys);
}

static const struct vlc_filter_operations DeinterlaceX2_ops = {
    .filter_video = DeinterlaceX2, .flush = Deinterlace_Flush, .close = CloseDeinterlace,
};
static const struct vlc_filter_operations Deinterlace_ops = {
    .filter_video = Deinterlace,   .flush = Deinterlace_Flush, .close = CloseDeinterlace,
};

static int
OpenDeinterlace(filter_t *filter)
{
    VAProcPipelineCaps          pipeline_caps;
    struct deint_data *const    p_data = calloc(1, sizeof(*p_data));
    if (!p_data)
        return VLC_ENOMEM;

    if (Open(filter, VAProcFilterDeinterlacing, &pipeline_caps, p_data,
             OpenDeinterlace_InitFilterParams, OpenDeinterlace_InitHistory))
        goto error;

    if (p_data->b_double_rate)
        filter->ops = &DeinterlaceX2_ops;
    else
        filter->ops = &Deinterlace_ops;

    for (unsigned int i = 0; i < METADATA_SIZE; ++i)
    {
        p_data->meta[i].date = VLC_TICK_INVALID;
        p_data->meta[i].i_nb_fields = 2;
    }

    return VLC_SUCCESS;

error:
    if (p_data->forward_refs.surfaces)
        free(p_data->forward_refs.surfaces);
    if (p_data->history.pp_pics)
        free(p_data->history.pp_pics);
    free(p_data);
    return VLC_EGENERIC;
}

/*********************
 * Module descriptor *
 *********************/

vlc_module_begin()
    set_shortname(N_("VAAPI filters"))
    set_description(N_("Video Accelerated API filters"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_submodule()
    set_callback_video_filter(OpenAdjust)
    add_shortcut("adjust")

    add_submodule()
    set_deinterlace_callback(OpenDeinterlace)

    add_submodule()
    set_callback_video_filter(OpenDenoiseFilter)
    /* Note: Skip label translation - too technical */
    add_float_with_range("denoise-sigma", 1.f, .0f, 2.f,
                         "Denoise strength",
                         "Set the Denoise strength. (Defaults to 1).")
    add_shortcut("denoise")

    add_submodule()
    set_callback_video_filter(OpenSharpenFilter)
    add_shortcut("sharpen")

    add_submodule()
    set_callback_video_converter(vlc_vaapi_OpenChroma, 10)
vlc_module_end()
