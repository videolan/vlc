/*****************************************************************************
 * ci_filters.m: Video filters for MacOSX OpenGL video output
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
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
# include "config.h"
#endif

#include <assert.h>
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_mouse.h>
#include "filter_picture.h"
#include "vt_utils.h"

#include <CoreImage/CIContext.h>
#include <CoreImage/CIImage.h>
#include <CoreImage/CIFilter.h>
#include <CoreImage/CIVector.h>

enum    filter_type
{
    FILTER_NONE = -1,
    FILTER_ADJUST_HUE,
    FILTER_ADJUST_COLOR_CONTROLS,
    FILTER_ADJUST_GAMMA,
    FILTER_INVERT,
    FILTER_POSTERIZE,
    FILTER_SEPIA,
    FILTER_SHARPEN,
    FILTER_PSYCHEDELIC,
    FILTER_CUSTOM,
    NUM_FILTERS,
    NUM_MAX_EQUIVALENT_VLC_FILTERS = 3
};

#define NUM_FILTER_PARAM_MAX    4

struct  filter_chain
{
    enum filter_type            filter;
    CIFilter *                  ci_filter;
    _Atomic float               ci_params[NUM_FILTER_PARAM_MAX];
    struct filter_chain *       next;
    union {
        struct
        {
#define PSYCHEDELIC_COUNT_DEFAULT 5
#define PSYCHEDELIC_COUNT_MIN 3
#define PSYCHEDELIC_COUNT_MAX 40
            int x, y;
            unsigned count;
        } psychedelic;
    } ctx;
};

struct  ci_filters_ctx
{
    CVPixelBufferPoolRef        cvpx_pool;
    video_format_t              cvpx_pool_fmt;
    CIContext *                 ci_ctx;
    struct filter_chain *       fchain;
    filter_t *                  src_converter;
    filter_t *                  dst_converter;
};

typedef struct filter_sys_t
{
    char const *                psz_filter;
    bool                        mouse_moved;
    vlc_mouse_t                 old_mouse;
    vlc_mouse_t                 mouse;
    struct ci_filters_ctx *     ctx;
} filter_sys_t;

struct  range
{
    float       min;
    float       max;
};

struct  filter_desc
{
    struct
    {
        char const *            vlc;
        NSString *              ci;
    } const             name_desc;

    struct      filter_param_desc
    {
        char const *            vlc;
        NSString *              ci;
        struct
        {
            struct range    vlc;
            struct range    ci;
        }                       ranges[2];
        int                     vlc_type;
    } const             param_descs[NUM_FILTER_PARAM_MAX];
    void (*pf_init)(filter_t *filter, struct filter_chain *fchain);
    void (*pf_control)(filter_t *filter, struct filter_chain *fchain);
};

static void filter_PsychedelicInit(filter_t *filter, struct filter_chain *fchain);
static void filter_PsychedelicControl(filter_t *filter, struct filter_chain *fchain);

static struct filter_desc       filter_desc_table[] =
{
    [FILTER_ADJUST_HUE] =
    {
        { "adjust", @"CIHueAdjust" },
        {
            { "hue", @"inputAngle", {{{-180.f, +180.f}, {+3.f, -3.f}}}, VLC_VAR_FLOAT }
        }
    },
    [FILTER_ADJUST_COLOR_CONTROLS] =
    {
        { "adjust", @"CIColorControls" },
        {
            { "contrast",   @"inputContrast",   {{{.0f, 2.f}, {.0f, 2.f}}},   VLC_VAR_FLOAT },
            { "brightness", @"inputBrightness", {{{.0f, 2.f}, {-1.f, +1.f}}}, VLC_VAR_FLOAT },
            { "saturation", @"inputSaturation", {{{.0f, 3.f}, {.0f, 2.7f}}},  VLC_VAR_FLOAT }
        }
    },
    [FILTER_ADJUST_GAMMA] =
    {
        { "adjust", @"CIGammaAdjust" },
        {
            { "gamma", @"inputPower", {{{.01f, 1.f}, {10.f, 1.f}}, {{1.f, 10.f}, {1.f, .01f}}}, VLC_VAR_FLOAT }
        }
    },
    [FILTER_INVERT] =
    {
        { "invert", @"CIColorInvert" }
    },
    [FILTER_POSTERIZE] =
    {
        { "posterize", @"CIColorPosterize" },
        {
            { "posterize-level", @"inputLevels", {{{2.f, 256.f}, {2.f, 256.f}}}, VLC_VAR_INTEGER }
        }
    },
    [FILTER_SEPIA] =
    {
        { "sepia", @"CISepiaTone" },
        {
            { "sepia-intensity", @"inputIntensity", {{{.0f, 255.f}, {.0f, 1.f}}}, VLC_VAR_INTEGER }
        }
    },
    [FILTER_SHARPEN] =
    {
        { "sharpen", @"CISharpenLuminance" },
        {
            { "sharpen-sigma", @"inputSharpness", {{{.0f, 2.f}, {.0f, 5.f}}}, VLC_VAR_FLOAT }
        }
    },
    [FILTER_PSYCHEDELIC] =
    {
        { "psychedelic", @"CIKaleidoscope" }, { { } },
        filter_PsychedelicInit,
        filter_PsychedelicControl
    },
    [FILTER_CUSTOM] =
    {
        { "custom" }, { { } },
        filter_PsychedelicInit,
        filter_PsychedelicControl
    },
};

#define GET_CI_VALUE(vlc_value, vlc_range, ci_range)               \
    ((vlc_value - vlc_range.min) * (ci_range.max - ci_range.min) / \
     (vlc_range.max - vlc_range.min) + ci_range.min)

static struct filter_chain *
filter_chain_AddFilter(struct filter_chain **fchain, enum filter_type filter)
{
    struct filter_chain *elem = calloc(1, sizeof(*elem));
    if (!elem)
        return NULL;
    elem->filter = filter;

    if (!*fchain)
        *fchain = elem;
    else
    {
        struct filter_chain *it = *fchain;
        while (it->next) it = it->next;
        it->next = elem;
    }

    return elem;
}

static void
filter_chain_RemoveFilter(struct filter_chain **fchain,
                          enum filter_type filter)
{
    struct filter_chain *prev = NULL;
    struct filter_chain *to_del;

    for (to_del = *fchain; to_del && to_del->filter != filter;
         to_del = to_del->next)
        prev = to_del;
    assert(to_del);
    if (!prev)
        *fchain = to_del->next;
    else
        prev->next = to_del->next;

    free(to_del);
}

static void
filter_desc_table_GetFilterTypes
(char const *vlc_filter_name,
 enum filter_type filter_types[NUM_MAX_EQUIVALENT_VLC_FILTERS])
{
    int j = 0;
    for (int i = 0; i < NUM_FILTERS; ++i)
        if (!strcmp(filter_desc_table[i].name_desc.vlc, vlc_filter_name))
        {
            assert(j < NUM_MAX_EQUIVALENT_VLC_FILTERS);
            filter_types[j++] = i;
        }
    assert(j);
    while (j < NUM_MAX_EQUIVALENT_VLC_FILTERS)
        filter_types[j++] = FILTER_NONE;
}

static inline NSString *
filter_desc_table_GetFilterName(enum filter_type type)
{ assert(type < NUM_FILTERS);
    return filter_desc_table[type].name_desc.ci;
}

static float
filter_ConvertParam(float f_vlc_val,
                    struct filter_param_desc const *param_desc)
{
    struct range clip_range = { param_desc->ranges[0].vlc.min,
                                param_desc->ranges[1].vlc.max
                                ? param_desc->ranges[1].vlc.max
                                : param_desc->ranges[0].vlc.max };
    f_vlc_val = VLC_CLIP(f_vlc_val, clip_range.min, clip_range.max);

    unsigned int range_idx;
    for (range_idx = 0; range_idx < 2; ++range_idx)
        if (f_vlc_val >= param_desc->ranges[range_idx].vlc.min &&
            f_vlc_val <= param_desc->ranges[range_idx].vlc.max)
            break;
    assert(range_idx < 2);

    return GET_CI_VALUE(f_vlc_val,
                        param_desc->ranges[range_idx].vlc,
                        param_desc->ranges[range_idx].ci);
}

static int
ParamsCallback(vlc_object_t *obj,
               char const *psz_var,
               vlc_value_t oldval, vlc_value_t newval,
               void *p_data)
{
    VLC_UNUSED(obj); VLC_UNUSED(oldval);
    struct filter_chain *filter = p_data;
    struct filter_param_desc const *filter_param_descs =
        filter_desc_table[filter->filter].param_descs;

    unsigned int i = 0;
    while (i < NUM_FILTER_PARAM_MAX &&
           strcmp(filter_param_descs[i].vlc, psz_var))
        ++i;
    assert(i < NUM_FILTER_PARAM_MAX);

    float new_vlc_val;
    if (filter_param_descs[i].vlc_type == VLC_VAR_FLOAT)
        new_vlc_val = newval.f_float;
    else if (filter_param_descs[i].vlc_type == VLC_VAR_INTEGER)
        new_vlc_val = newval.i_int;
    else
        vlc_assert_unreachable();

    atomic_store(filter->ci_params + i,
                 filter_ConvertParam(new_vlc_val, filter_param_descs + i));

    return VLC_SUCCESS;
}

static void filter_PsychedelicInit(filter_t *filter, struct filter_chain *fchain)
{
    filter_sys_t *sys = filter->p_sys;
    fchain->ctx.psychedelic.x = filter->fmt_in.video.i_width / 2;
    fchain->ctx.psychedelic.y = filter->fmt_in.video.i_height / 2;
    fchain->ctx.psychedelic.count = PSYCHEDELIC_COUNT_DEFAULT;
}

static void filter_PsychedelicControl(filter_t *filter, struct filter_chain *fchain)
{
    filter_sys_t *sys = filter->p_sys;

    if (sys->mouse_moved)
    {
        fchain->ctx.psychedelic.x = sys->mouse.i_x;
        fchain->ctx.psychedelic.y = filter->fmt_in.video.i_height
                                            - sys->mouse.i_y - 1;

        if (sys->mouse.i_pressed)
        {
            fchain->ctx.psychedelic.count++;
            if (fchain->ctx.psychedelic.count > PSYCHEDELIC_COUNT_MAX)
                fchain->ctx.psychedelic.count = PSYCHEDELIC_COUNT_MIN;
        }
    }
    CIVector *ci_vector =
        [CIVector vectorWithX: (float)fchain->ctx.psychedelic.x
                            Y: (float)fchain->ctx.psychedelic.y];
    @try {
        [fchain->ci_filter setValue: ci_vector
                             forKey: @"inputCenter"];
        [fchain->ci_filter setValue: [NSNumber numberWithFloat: fchain->ctx.psychedelic.count]
                             forKey: @"inputCount"];
    }
    @catch (NSException * e) { /* inputCenter key doesn't exist */ }
}

static picture_t *
Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *p_sys = filter->p_sys;
    struct ci_filters_ctx *ctx = p_sys->ctx;
    enum filter_type filter_types[NUM_MAX_EQUIVALENT_VLC_FILTERS];

    filter_desc_table_GetFilterTypes(p_sys->psz_filter, filter_types);
    if (ctx->fchain->filter != filter_types[0])
        return src;

    picture_t *dst = picture_NewFromFormat(&ctx->cvpx_pool_fmt);
    if (!dst)
        goto error;

    CVPixelBufferRef cvpx = cvpxpool_new_cvpx(ctx->cvpx_pool);
    if (!cvpx)
        goto error;

    if (cvpxpic_attach(dst, cvpx, filter->vctx_out, NULL))
    {
        CFRelease(cvpx);
        goto error;
    }
    CFRelease(cvpx);

    if (ctx->src_converter)
    {
        src = ctx->dst_converter->pf_video_filter(ctx->src_converter, src);
        if (!src)
            return NULL;
    }

    @autoreleasepool {
        CIImage *ci_img = [CIImage imageWithCVImageBuffer: cvpxpic_get_ref(src)];
        if (!ci_img)
            goto error;

        for (struct filter_chain *fchain = ctx->fchain;
             fchain; fchain = fchain->next)
        {
            [fchain->ci_filter setValue: ci_img
                                 forKey: kCIInputImageKey];

            for (unsigned int i = 0; i < NUM_FILTER_PARAM_MAX &&
                     filter_desc_table[fchain->filter].param_descs[i].vlc; ++i)
            {
                NSString *ci_param_name =
                    filter_desc_table[fchain->filter].param_descs[i].ci;
                float ci_value = atomic_load(fchain->ci_params + i);

                [fchain->ci_filter setValue: [NSNumber numberWithFloat: ci_value]
                                     forKey: ci_param_name];
            }

            if (filter_desc_table[fchain->filter].pf_control)
                filter_desc_table[fchain->filter].pf_control(filter, fchain);
            ci_img = [fchain->ci_filter valueForKey: kCIOutputImageKey];
        }

        [ctx->ci_ctx render: ci_img
            toCVPixelBuffer: cvpx];
    } /* autoreleasepool */

    CopyInfoAndRelease(dst, src);

    if (ctx->dst_converter)
    {
        dst = ctx->dst_converter->pf_video_filter(ctx->dst_converter, dst);
        if (!dst)
            return NULL;
    }

    p_sys->mouse_moved = false;
    return dst;

error:
    if (dst)
        picture_Release(dst);
    picture_Release(src);
    p_sys->mouse_moved = false;
    return NULL;
}

static int
Mouse(filter_t *filter, struct vlc_mouse_t *mouse,
      const struct vlc_mouse_t *old, const struct vlc_mouse_t *new)
{
    VLC_UNUSED(mouse);
    filter_sys_t *sys = filter->p_sys;
    sys->old_mouse = *old;
    sys->mouse = *new;
    sys->mouse_moved = true;
    *mouse = *new;
    return VLC_SUCCESS;
}

static int
Open_FilterInit(filter_t *filter, struct filter_chain *fchain)
{
    struct filter_param_desc const *filter_param_descs =
        filter_desc_table[fchain->filter].param_descs;
    NSString *ci_filter_name = filter_desc_table_GetFilterName(fchain->filter);
    if (ci_filter_name == nil)
    {
        char *psz_filter_name = var_InheritString(filter, "ci-filter");
        if (psz_filter_name)
            ci_filter_name = [NSString stringWithUTF8String:psz_filter_name];
        free(psz_filter_name);
    }

    fchain->ci_filter = [CIFilter filterWithName: ci_filter_name];
    if (!fchain->ci_filter)
    {
        msg_Warn(filter, "filter '%s' could not be created",
                 [ci_filter_name UTF8String]);
        return VLC_EGENERIC;
    }

    for (int i = 0; i < NUM_FILTER_PARAM_MAX && filter_param_descs[i].vlc; ++i)
    {
        NSString *ci_param_name = filter_param_descs[i].ci;
        char const *vlc_param_name = filter_param_descs[i].vlc;

        float vlc_param_val;
        if (filter_param_descs[i].vlc_type == VLC_VAR_FLOAT)
            vlc_param_val = var_CreateGetFloatCommand(filter, vlc_param_name);
        else if (filter_param_descs[i].vlc_type == VLC_VAR_INTEGER)
            vlc_param_val =
                (float)var_CreateGetIntegerCommand(filter, vlc_param_name);
        else
            vlc_assert_unreachable();

        atomic_init(fchain->ci_params + i,
                    filter_ConvertParam(vlc_param_val,
                                        filter_param_descs + i));

        var_AddCallback(filter, filter_param_descs[i].vlc,
                        ParamsCallback, fchain);
    }
    if (filter_desc_table[fchain->filter].pf_init)
        filter_desc_table[fchain->filter].pf_init(filter, fchain);

    return VLC_SUCCESS;
}

static int
Open_CreateFilters(filter_t *filter, struct filter_chain **p_last_filter,
                   enum filter_type filter_types[NUM_MAX_EQUIVALENT_VLC_FILTERS])
{
    struct filter_chain *new_filter;

    for (unsigned int i = 0;
         i < NUM_MAX_EQUIVALENT_VLC_FILTERS
             && filter_types[i] != FILTER_NONE; ++i)
    {
        new_filter = filter_chain_AddFilter(p_last_filter, filter_types[i]);
        if (!new_filter)
            return VLC_EGENERIC;
        p_last_filter = &new_filter;
        if (Open_FilterInit(filter, new_filter) != VLC_SUCCESS)
        {
            for (unsigned int j = 0; j < i ; ++j)
                filter_chain_RemoveFilter(p_last_filter, filter_types[i]);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void
cvpx_video_context_Destroy(void *priv)
{
    struct ci_filters_ctx *ctx = priv;

    if (ctx->src_converter)
    {
        module_unneed(ctx->src_converter, ctx->src_converter->p_module);
        vlc_object_delete(ctx->src_converter);
    }
    if (ctx->dst_converter)
    {
        module_unneed(ctx->dst_converter, ctx->dst_converter->p_module);
        vlc_object_delete(ctx->dst_converter);
    }

    if (ctx->cvpx_pool)
        CVPixelBufferPoolRelease(ctx->cvpx_pool);
}

static filter_t *
CVPX_to_CVPX_converter_Create(filter_t *filter, bool to_rgba)
{
    filter_t *converter = vlc_object_create(filter, sizeof(filter_t));
    if (!converter)
        return NULL;

    converter->fmt_in = filter->fmt_out;
    converter->fmt_out = filter->fmt_out;

    if (to_rgba)
    {
        converter->fmt_out.video.i_chroma =
        converter->fmt_out.i_codec = VLC_CODEC_CVPX_BGRA;
    }
    else
    {
        converter->fmt_in.video.i_chroma =
        converter->fmt_in.i_codec = VLC_CODEC_CVPX_BGRA;
    }

    converter->p_module = module_need(converter, "video converter", NULL, false);
    if (!converter->p_module)
    {
        vlc_object_delete(converter);
        return NULL;
    }

    return converter;
}

static int
Open(vlc_object_t *obj, char const *psz_filter)
{
    filter_t *filter = (filter_t *)obj;

    switch (filter->fmt_in.video.i_chroma)
    {
        case VLC_CODEC_CVPX_NV12:
        case VLC_CODEC_CVPX_UYVY:
        case VLC_CODEC_CVPX_I420:
        case VLC_CODEC_CVPX_BGRA:
            break;
        default:
            return VLC_EGENERIC;
    }

    if (filter->vctx_in == NULL ||
        vlc_video_context_GetType(filter->vctx_in) != VLC_VIDEO_CONTEXT_CVPX)
        return VLC_EGENERIC;

    filter_sys_t *p_sys = filter->p_sys = calloc(1, sizeof(filter_sys_t));
    if (!filter->p_sys)
        return VLC_ENOMEM;

    enum filter_type filter_types[NUM_MAX_EQUIVALENT_VLC_FILTERS];
    filter_desc_table_GetFilterTypes(psz_filter, filter_types);

    struct ci_filters_ctx *ctx =
        vlc_video_context_GetCVPXPrivate(filter->vctx_in, CVPX_VIDEO_CONTEXT_CIFILTERS);

    if (ctx)
        filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);
    else
    {
        static const struct vlc_video_context_operations ops = {
            cvpx_video_context_Destroy,
        };
        vlc_decoder_device *dec_dev =
            filter_HoldDecoderDeviceType(filter,
                                         VLC_DECODER_DEVICE_VIDEOTOOLBOX);
        if (!dec_dev)
        {
            msg_Err(filter, "Missing decoder device");
            goto error;
        }
        filter->vctx_out =
            vlc_video_context_CreateCVPX(dec_dev, CVPX_VIDEO_CONTEXT_CIFILTERS,
                                         sizeof(struct ci_filters_ctx), &ops);
        vlc_decoder_device_Release(dec_dev);
        if (!filter->vctx_out)
            goto error;

        ctx = vlc_video_context_GetCVPXPrivate(filter->vctx_out,
                                               CVPX_VIDEO_CONTEXT_CIFILTERS);
        assert(ctx);

        ctx->src_converter = ctx->dst_converter = NULL;
        ctx->fchain = NULL;
        ctx->cvpx_pool = nil;
        ctx->cvpx_pool_fmt = filter->fmt_out.video;

        if (filter->fmt_in.video.i_chroma != VLC_CODEC_CVPX_NV12
         && filter->fmt_in.video.i_chroma != VLC_CODEC_CVPX_BGRA)
        {
            ctx->src_converter =
                    CVPX_to_CVPX_converter_Create(filter, true);
            ctx->dst_converter = ctx->src_converter ?
                    CVPX_to_CVPX_converter_Create(filter, false) : NULL;
            if (!ctx->src_converter || !ctx->dst_converter)
                goto error;
            ctx->cvpx_pool_fmt.i_chroma = VLC_CODEC_CVPX_BGRA;
        }

#if !TARGET_OS_IPHONE
        CGLContextObj glctx = var_InheritAddress(filter, "macosx-glcontext");
        if (!glctx)
        {
            msg_Err(filter, "can't find 'macosx-glcontext' var");
            goto error;
        }
        ctx->ci_ctx = [CIContext contextWithCGLContext: glctx
                                           pixelFormat: nil
                                            colorSpace: nil
                                               options: @{
                                                kCIContextWorkingColorSpace : [NSNull null],
                                                kCIContextOutputColorSpace : [NSNull null],
                                               }];
#else
        CVEAGLContext eaglctx = var_InheritAddress(filter, "ios-eaglcontext");
        if (!eaglctx)
        {
            msg_Err(filter, "can't find 'ios-eaglcontext' var");
            goto error;
        }
        ctx->ci_ctx = [CIContext contextWithEAGLContext: eaglctx];
#endif
        if (!ctx->ci_ctx)
            goto error;

        ctx->cvpx_pool = cvpxpool_create(&ctx->cvpx_pool_fmt, 2);
        if (!ctx->cvpx_pool)
            goto error;
    }
    if (Open_CreateFilters(filter, &ctx->fchain, filter_types))
        goto error;

    p_sys->psz_filter = psz_filter;
    p_sys->ctx = ctx;

    filter->pf_video_filter = Filter;
    filter->pf_video_mouse = Mouse;

    return VLC_SUCCESS;

error:
    if (filter->vctx_out)
        vlc_video_context_Release(filter->vctx_out);
    free(p_sys);
    return VLC_EGENERIC;
}

static int
OpenAdjust(vlc_object_t *obj)
{
    return Open(obj, "adjust");
}

static int
OpenInvert(vlc_object_t *obj)
{
    return Open(obj, "invert");
}

static int
OpenPosterize(vlc_object_t *obj)
{
    return Open(obj, "posterize");
}

static int
OpenSepia(vlc_object_t *obj)
{
    return Open(obj, "sepia");
}

static int
OpenSharpen(vlc_object_t *obj)
{
    return Open(obj, "sharpen");
}

static int
OpenPsychedelic(vlc_object_t *obj)
{
    return Open(obj, "psychedelic");
}

static int
OpenCustom(vlc_object_t *obj)
{
    return Open(obj, "custom");
}

static void
Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *p_sys = filter->p_sys;
    struct ci_filters_ctx *ctx = p_sys->ctx;
    enum filter_type filter_types[NUM_MAX_EQUIVALENT_VLC_FILTERS];

    filter_desc_table_GetFilterTypes(p_sys->psz_filter, filter_types);
    for (unsigned int i = 0;
         i < NUM_MAX_EQUIVALENT_VLC_FILTERS && filter_types[i] != FILTER_NONE;
         ++i)
        filter_chain_RemoveFilter(&ctx->fchain, filter_types[i]);

    vlc_video_context_Release(filter->vctx_out);
    free(p_sys);
}

#define CI_CUSTOM_FILTER_TEXT N_("Use a specific Core Image Filter")
#define CI_CUSTOM_FILTER_LONGTEXT N_( \
    "Example: 'CICrystallize', 'CIBumpDistortion', 'CIThermal', 'CIComicEffect'")

vlc_module_begin()
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_description(N_("Mac OS X hardware video filters"))

    add_submodule()
    set_callbacks(OpenAdjust, Close)
    add_shortcut("adjust")

    add_submodule()
    set_callbacks(OpenInvert, Close)
    add_shortcut("invert")

    add_submodule()
    set_callbacks(OpenPosterize, Close)
    add_shortcut("posterize")

    add_submodule()
    set_callbacks(OpenSepia, Close)
    add_shortcut("sepia")

    add_submodule()
    set_callbacks(OpenSharpen, Close)
    add_shortcut("sharpen")

    add_submodule()
    set_callbacks(OpenPsychedelic, Close)
    add_shortcut("psychedelic")

    add_submodule()
    set_callbacks(OpenCustom, Close)
    add_shortcut("ci")
    add_string("ci-filter", "CIComicEffect", CI_CUSTOM_FILTER_TEXT, CI_CUSTOM_FILTER_LONGTEXT, true);
vlc_module_end()
