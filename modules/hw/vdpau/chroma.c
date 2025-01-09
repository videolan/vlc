/*****************************************************************************
 * chroma.c: VLC picture import into VDPAU
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
#include <inttypes.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>
#include <vlc_chroma_probe.h>
#include "vlc_vdpau.h"

/* Picture history as recommended by VDPAU documentation */
#define MAX_PAST   2
#define MAX_FUTURE 1

typedef struct
{
    struct vlc_vdp_device *device;
    VdpVideoMixer mixer;
    picture_pool_t *pool;

    picture_t *history[MAX_PAST + 1 + MAX_FUTURE];

    struct
    {
        float brightness;
        float contrast;
        float saturation;
        float hue;
    } procamp;
} vlc_vdp_mixer_t;

/** Initialize the colour space conversion matrix */
static VdpStatus MixerSetupColors(filter_t *filter, const VdpProcamp *procamp,
                                  VdpCSCMatrix *restrict csc)
{
    vlc_vdp_mixer_t *sys = filter->p_sys;
    vdp_t *vdp = sys->device->vdp;
    VdpStatus err;
    /* XXX: add some margin for padding... */
    VdpColorStandard std;

    switch (filter->fmt_in.video.space)
    {
        case COLOR_SPACE_BT601:
            std = VDP_COLOR_STANDARD_ITUR_BT_601;
            break;
        case COLOR_SPACE_BT709:
            std = VDP_COLOR_STANDARD_ITUR_BT_709;
            break;
        default:
            if (filter->fmt_in.video.i_height >= 720)
                std = VDP_COLOR_STANDARD_ITUR_BT_709;
            else
                std = VDP_COLOR_STANDARD_ITUR_BT_601;
    }

    err = vdp_generate_csc_matrix(vdp, procamp, std, csc);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s failure: %s", "color space matrix",
                vdp_get_error_string(vdp, err));
        return err;
    }

    if (procamp != NULL)
    {
        sys->procamp.brightness = procamp->brightness;
        sys->procamp.contrast = procamp->contrast;
        sys->procamp.saturation = procamp->saturation;
        sys->procamp.hue = procamp->hue;
    }
    else
    {
        sys->procamp.brightness = 0.f;
        sys->procamp.contrast = 1.f;
        sys->procamp.saturation = 1.f;
        sys->procamp.hue = 0.f;
    }
    return VDP_STATUS_OK;
}

/** Create VDPAU video mixer */
static VdpVideoMixer MixerCreate(filter_t *filter, VdpChromaType chroma)
{
    vlc_vdp_mixer_t *sys = filter->p_sys;
    vdp_t *vdp = sys->device->vdp;
    VdpDevice device = sys->device->device;
    VdpVideoMixer mixer;
    VdpStatus err;
    VdpBool ok;

    /* Check for potentially useful features */
    VdpVideoMixerFeature featv[5];
    unsigned featc = 0;

    int algo = var_InheritInteger(filter, "vdpau-deinterlace");
    bool ivtc = false;
    if (algo == VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL)
    {
        err = vdp_video_mixer_query_feature_support(vdp, device, algo, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
            msg_Dbg(filter, "using video mixer %s feature",
                    "temporal-spatial deinterlace");
        else
            algo = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL; /* fallback */
    }
    if (algo == VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL)
    {
        err = vdp_video_mixer_query_feature_support(vdp, device, algo, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
            msg_Dbg(filter, "using video mixer %s feature",
                    "temporal deinterlace");
        else
            algo = -1;
    }
    if (algo >= 0)
    {
        featv[featc++] = algo;
        ivtc = var_InheritBool(filter, "vdpau-ivtc");
        if (ivtc)
        {
            err = vdp_video_mixer_query_feature_support(vdp, device,
                                VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE, &ok);
            if (err == VDP_STATUS_OK && ok == VDP_TRUE)
                msg_Dbg(filter, "using video mixer %s feature",
                        "inverse telecine");
            featv[featc++] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;
        }
    }

    const float noise = var_InheritFloat(filter, "vdpau-noise-reduction");
    if (noise > 0.f)
    {
        err = vdp_video_mixer_query_feature_support(vdp, device,
                                 VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
        {
            msg_Dbg(filter, "using video mixer %s feature", "noise reduction");
            featv[featc++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
        }
    }

    err = vdp_video_mixer_query_feature_support(vdp, device,
                                       VDP_VIDEO_MIXER_FEATURE_SHARPNESS, &ok);
    if (err == VDP_STATUS_OK && ok == VDP_TRUE)
    {
        msg_Dbg(filter, "using video mixer %s feature", "sharpness");
        featv[featc++] = VDP_VIDEO_MIXER_FEATURE_SHARPNESS;
    }

    const int offset = VDP_VIDEO_MIXER_FEATURE_HIGH_QUALITY_SCALING_L1 - 1;
    unsigned level = var_InheritInteger(filter, "vdpau-scaling");
    while (level > 0)
    {

        err = vdp_video_mixer_query_feature_support(vdp, device,
                                                    offset + level, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
        {
            msg_Dbg(filter, "using video mixer high quality scaling L%u",
                    level);
            featv[featc++] = offset + level;
            break;
        }
        level--; /* fallback to lower quality */
    }

    /* Create the mixer */
    VdpVideoMixerParameter parms[3] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
    };
    uint32_t width = filter->fmt_in.video.i_width;
    uint32_t height = filter->fmt_in.video.i_height;
    const void *values[3] = { &width, &height, &chroma, };

    err = vdp_video_mixer_create(vdp, device, featc, featv,
                                 3, parms, values, &mixer);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "creation",
                vdp_get_error_string(vdp, err));
        return VDP_INVALID_HANDLE;
    }

    msg_Dbg(filter, "using video mixer %"PRIu32, mixer);

    /* Set initial features and attributes */
    VdpVideoMixerAttribute attrv[3];
    const void *valv[3];
    unsigned attrc = 0;
    VdpCSCMatrix csc;
    uint8_t chroma_skip;

    featc = 0;

    if (MixerSetupColors(filter, NULL, &csc) == VDP_STATUS_OK)
    {
        attrv[attrc] = VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX;
        valv[attrc] = &csc;
        attrc++;
    }

    if (algo >= 0)
    {
        featv[featc++] = algo;
        if (ivtc)
            featv[featc++] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;

        chroma_skip = var_InheritBool(filter, "vdpau-chroma-skip");
        attrv[attrc] = VDP_VIDEO_MIXER_ATTRIBUTE_SKIP_CHROMA_DEINTERLACE;
        valv[attrc] = &chroma_skip;
        attrc++;
    }

    if (noise > 0.f)
    {
        featv[featc++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;

        attrv[attrc] = VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL;
        valv[attrc] = &noise;
        attrc++;
    }

    if (level > 0)
        featv[featc++] = offset + level;

    if (featc > 0)
    {
        VdpBool enablev[featc];

        for (unsigned i = 0; i < featc; i++)
            enablev[i] = VDP_TRUE;

        err = vdp_video_mixer_set_feature_enables(vdp, mixer,
                                                  featc, featv, enablev);
        if (err != VDP_STATUS_OK)
            msg_Err(filter, "video %s %s failure: %s", "mixer", "features",
                    vdp_get_error_string(vdp, err));
    }

    if (attrc > 0)
    {
        err = vdp_video_mixer_set_attribute_values(vdp, mixer,
                                                   attrc, attrv, valv);
        if (err != VDP_STATUS_OK)
            msg_Err(filter, "video %s %s failure: %s", "mixer", "attributes",
                    vdp_get_error_string(vdp, err));
    }

    return mixer;
}

static void Flush(filter_t *filter)
{
    vlc_vdp_mixer_t *sys = filter->p_sys;

    for (unsigned i = 0; i < MAX_PAST + MAX_FUTURE; i++)
        if (sys->history[i] != NULL)
        {
            picture_Release(sys->history[i]);
            sys->history[i] = NULL;
        }
}

/** Export a VDPAU video surface picture to a normal VLC picture */
static picture_t *VideoExport(filter_t *filter, picture_t *src, picture_t *dst,
                              VdpYCbCrFormat format)
{
    vlc_vdp_video_field_t *field = VDPAU_FIELD_FROM_PICCTX(src->context);
    VdpStatus err;
    VdpVideoSurface surface = field->frame->surface;
    void *planes[3];
    uint32_t pitches[3];

    vdpau_decoder_device_t *vdpau_decoder =
        GetVDPAUOpaqueContext(picture_GetVideoContext(src));
    picture_CopyProperties(dst, src);

    for (int i = 0; i < dst->i_planes; i++)
    {
        planes[i] = dst->p[i].p_pixels;
        pitches[i] = dst->p[i].i_pitch;
    }
    if (dst->format.i_chroma == VLC_CODEC_I420
     || dst->format.i_chroma == VLC_CODEC_I422
     || dst->format.i_chroma == VLC_CODEC_I444)
    {
        planes[1] = dst->p[2].p_pixels;
        planes[2] = dst->p[1].p_pixels;
        pitches[1] = dst->p[2].i_pitch;
        pitches[2] = dst->p[1].i_pitch;
    }
    err = vdp_video_surface_get_bits_y_cb_cr(vdpau_decoder->vdp, surface, format,
                                             planes, pitches);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "export",
                vdp_get_error_string(vdpau_decoder->vdp, err));
        picture_Release(dst);
        dst = NULL;
    }
    picture_Release(src);
    return dst;
}

static void OutputSurfaceDestroy(struct picture_context_t *ctx)
{
    free(ctx);
}

static picture_context_t *OutputSurfaceClone(picture_context_t *ctx)
{
    picture_context_t *dts_ctx = malloc(sizeof(*dts_ctx));
    if (unlikely(dts_ctx == NULL))
        return NULL;
    *dts_ctx = *ctx;
    vlc_video_context_Hold(dts_ctx->vctx);
    return dts_ctx;
}

static picture_t *Render(filter_t *filter, picture_t *src)
{
    vlc_vdp_mixer_t *sys = filter->p_sys;
    vdp_t *vdp = sys->device->vdp;
    picture_t *dst = NULL;
    VdpStatus err;

    if (unlikely(src->context == NULL))
    {
        msg_Err(filter, "corrupt VDPAU video surface %p", (void *)src);
        picture_Release(src);
        return NULL;
    }

    /* Update history and put "present" picture field as future */
    sys->history[MAX_PAST + MAX_FUTURE] = src;

    /* future position pic slides as current at end of the call (skip:) */
    picture_t *pic_f = sys->history[MAX_PAST];
    if (pic_f == NULL) /* If we just started, there is no future pic */
    {
        /* If the picture is not forced (first pic is), we ignore deinterlacing
         * and fast forward, as there's nothing to output for this call. */
        if(!src->b_force)
            goto skip;
        /* FIXME: Remove the forced hack pictures in video output core and
         * allow the last field of a video to be rendered properly.
         * As we're working with a MAX_FUTURE frame delay, we'll need a signal
         * to solve those single frame and last frame issues. */
        while (sys->history[MAX_PAST] == NULL)
        {
            pic_f = sys->history[0];
            if (pic_f != NULL)
                picture_Release(pic_f);

            memmove(sys->history, sys->history + 1,
                    sizeof (sys->history[0]) * (MAX_PAST + MAX_FUTURE));
            sys->history[MAX_PAST + MAX_FUTURE] = NULL;
        }
        pic_f = sys->history[MAX_PAST];
    }

    /* Get a VLC picture for a VDPAU output surface */
    dst = picture_pool_Get(sys->pool);
    if (dst == NULL)
        goto skip;
    dst->context = malloc(sizeof(*dst->context));
    if (unlikely(dst->context == NULL))
        goto error;

    *dst->context = (picture_context_t) {
        OutputSurfaceDestroy, OutputSurfaceClone,
        vlc_video_context_Hold(filter->vctx_out)
    };

    vlc_vdp_output_surface_t *p_sys = dst->p_sys;
    assert(p_sys != NULL && p_sys->vctx == filter->vctx_out);
    dst->date = pic_f->date;
    dst->b_force = pic_f->b_force;
    dst->b_still = pic_f->b_still;

    /* Enable/Disable features */
    vlc_vdp_video_field_t *f = VDPAU_FIELD_FROM_PICCTX(pic_f->context);
    const VdpVideoMixerFeature features[] = {
        VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
    };
    const VdpBool enables[] = {
        f->sharpen != 0.f,
    };

    err = vdp_video_mixer_set_feature_enables(vdp, sys->mixer,
                  sizeof (features) / sizeof (features[0]), features, enables);
    if (err != VDP_STATUS_OK)
        msg_Err(filter, "video %s %s failure: %s", "mixer", "features",
                vdp_get_error_string(vdp, err));

    /* Configure mixer depending on upstream video filters */
    VdpVideoMixerAttribute attrs[2] = {
        VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL,
    };
    const void *values[2] = {
        &f->sharpen,
    };
    unsigned count = 1;
    VdpCSCMatrix csc;

    if ((sys->procamp.brightness != f->procamp.brightness
      || sys->procamp.contrast != f->procamp.contrast
      || sys->procamp.saturation != f->procamp.saturation
      || sys->procamp.hue != f->procamp.hue)
     && (MixerSetupColors(filter, &f->procamp, &csc) == VDP_STATUS_OK))
    {
        attrs[count] = VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX;
        values[count] = &csc;
        count++;
    }

    err = vdp_video_mixer_set_attribute_values(vdp, sys->mixer,
                                               count, attrs, values);
    if (err != VDP_STATUS_OK)
        msg_Err(filter, "video %s %s failure: %s", "mixer", "attributes",
                vdp_get_error_string(vdp, err));

    /* Check video orientation, allocate intermediate surface if needed */
    bool swap = false;
    bool hflip = false, vflip = false;

    if (filter->fmt_in.video.orientation != filter->fmt_out.video.orientation)
    {
        assert(filter->fmt_out.video.orientation == ORIENT_TOP_LEFT);
        swap = ORIENT_IS_SWAP(filter->fmt_in.video.orientation);
        switch (filter->fmt_in.video.orientation)
        {
            case ORIENT_TOP_LEFT:
            case ORIENT_RIGHT_TOP:
                break;
            case ORIENT_TOP_RIGHT:
            case ORIENT_RIGHT_BOTTOM:
                hflip = true;
                break;
            case ORIENT_BOTTOM_LEFT:
            case ORIENT_LEFT_TOP:
                vflip = true;
                break;
            case ORIENT_BOTTOM_RIGHT:
            case ORIENT_LEFT_BOTTOM:
                vflip = hflip = true;
                break;
        }
    }

    VdpOutputSurface output = p_sys->surface;

    if (swap)
    {
        VdpRGBAFormat fmt;
        uint32_t width, height;

        err = vdp_output_surface_get_parameters(vdp, output,
                                                &fmt, &width, &height);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "query",
                    vdp_get_error_string(vdp, err));
            goto error;
        }

        err = vdp_output_surface_create(vdp, sys->device->device,
                                        fmt, height, width, &output);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "creation",
                    vdp_get_error_string(vdp, err));
            goto error;
        }
    }

    /* Render video into output */
    VdpVideoMixerPictureStructure structure = f->structure;
    VdpVideoSurface past[MAX_PAST];
    VdpVideoSurface surface = f->frame->surface;
    VdpVideoSurface future[MAX_FUTURE];
    VdpRect src_rect = {
        filter->fmt_in.video.i_x_offset, filter->fmt_in.video.i_y_offset,
        filter->fmt_in.video.i_x_offset, filter->fmt_in.video.i_y_offset,
    };

    if (hflip)
        src_rect.x0 += filter->fmt_in.video.i_visible_width;
    else
        src_rect.x1 += filter->fmt_in.video.i_visible_width;
    if (vflip)
        src_rect.y0 += filter->fmt_in.video.i_visible_height;
    else
        src_rect.y1 += filter->fmt_in.video.i_visible_height;

    VdpRect dst_rect = {
        0, 0,
        swap ? filter->fmt_out.video.i_visible_height
             : filter->fmt_out.video.i_visible_width,
        swap ? filter->fmt_out.video.i_visible_width
             : filter->fmt_out.video.i_visible_height,
    };

    for (unsigned i = 0; i < MAX_PAST; i++)
    {
        pic_f = sys->history[(MAX_PAST - 1) - i];
        past[i] = (pic_f != NULL) ? VDPAU_FIELD_FROM_PICCTX(pic_f->context)->frame->surface : VDP_INVALID_HANDLE;
    }
    for (unsigned i = 0; i < MAX_FUTURE; i++)
    {
        pic_f = sys->history[(MAX_PAST + 1) + i];
        future[i] = (pic_f != NULL) ? VDPAU_FIELD_FROM_PICCTX(pic_f->context)->frame->surface : VDP_INVALID_HANDLE;
    }

    err = vdp_video_mixer_render(vdp, sys->mixer, VDP_INVALID_HANDLE,
                                 NULL, structure,
                                 MAX_PAST, past, surface, MAX_FUTURE, future,
                                 &src_rect, output, &dst_rect, &dst_rect, 0,
                                 NULL);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "rendering",
                vdp_get_error_string(vdp, err));
        goto error;
    }

    if (swap)
    {
        err = vdp_output_surface_render_output_surface(vdp,
            p_sys->surface, NULL, output, NULL, NULL, NULL,
            VDP_OUTPUT_SURFACE_RENDER_ROTATE_90);
        vdp_output_surface_destroy(vdp, output);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "render",
                    vdp_get_error_string(vdp, err));
            goto error;
        }
    }

skip:
    pic_f = sys->history[0];
    if (pic_f != NULL)
        picture_Release(pic_f); /* Release oldest field */
    memmove(sys->history, sys->history + 1, /* Advance history */
            sizeof (sys->history[0]) * (MAX_PAST + MAX_FUTURE));

    return dst;
error:
    picture_Release(dst);
    dst = NULL;
    goto skip;
}

static int OutputCheckFormat(vlc_object_t *obj, struct vlc_video_context *vctx,
                             const video_format_t *fmt,
                             VdpRGBAFormat *restrict rgb_fmt)
{
    static const VdpRGBAFormat rgb_fmts[] = {
        VDP_RGBA_FORMAT_R10G10B10A2, VDP_RGBA_FORMAT_B10G10R10A2,
        VDP_RGBA_FORMAT_B8G8R8A8, VDP_RGBA_FORMAT_R8G8B8A8,
    };
    struct vlc_vdp_device *device = GetVDPAUOpaqueContext(vctx);

    for (unsigned i = 0; i < ARRAY_SIZE(rgb_fmts); i++)
    {
        uint32_t w, h;
        VdpBool ok;

        VdpStatus err = vdp_output_surface_query_capabilities(device->vdp,
                                                              device->device,
                                                     rgb_fmts[i], &ok, &w, &h);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(obj, "%s capabilities query failure: %s", "output surface",
                    vdp_get_error_string(device->vdp, err));
            continue;
        }

        if (!ok || w < fmt->i_width || h < fmt->i_height)
            continue;

        *rgb_fmt = rgb_fmts[i];
        msg_Dbg(obj, "using RGBA format %u", *rgb_fmt);
        return 0;
    }

    msg_Err(obj, "no supported output surface format");
    return VLC_EGENERIC;
}

static picture_pool_t *OutputPoolAlloc(vlc_object_t *obj,
    struct vlc_video_context *vctx, const video_format_t *restrict fmt)
{
    /* Check output surface format */
    VdpRGBAFormat rgb_fmt;

    if (OutputCheckFormat(obj, vctx, fmt, &rgb_fmt))
        return NULL;

    /* Allocate the pool */
    return vlc_vdp_output_pool_create(vctx, rgb_fmt, fmt, 3);
}

const struct vlc_video_context_operations vdpau_vctx_ops = {
    NULL,
};

static void OutputClose(filter_t *filter)
{
    vlc_vdp_mixer_t *sys = filter->p_sys;

    Flush(filter);
    vdp_video_mixer_destroy(sys->device->vdp, sys->mixer);
    picture_pool_Release(sys->pool);
    vlc_video_context_Release(filter->vctx_out);
}

static const struct vlc_filter_operations filter_output_opaque_ops = {
    .filter_video = Render, .flush = Flush, .close = OutputClose,
};

static int OutputOpen(filter_t *filter)
{
    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO
     || filter->fmt_out.video.i_chroma != VLC_CODEC_VDPAU_OUTPUT)
        return VLC_ENOTSUP;

    assert(filter->fmt_out.video.orientation == ORIENT_TOP_LEFT
        || filter->fmt_in.video.orientation == filter->fmt_out.video.orientation);

    vlc_decoder_device *dec_device = filter_HoldDecoderDeviceType(filter, VLC_DECODER_DEVICE_VDPAU);
    if (dec_device == NULL)
        return VLC_EGENERIC;

    vlc_vdp_mixer_t *sys = vlc_obj_malloc(VLC_OBJECT(filter), sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_ENOMEM;
    }

    filter->p_sys = sys;

    const VdpChromaType *chroma;
    struct vlc_video_context *vctx_out;

    sys->device = GetVDPAUOpaqueDevice(dec_device);
    vctx_out = vlc_video_context_Create(dec_device, VLC_VIDEO_CONTEXT_VDPAU,
                                        0, &vdpau_vctx_ops);
    vlc_decoder_device_Release(dec_device);
    if (unlikely(vctx_out == NULL))
        return VLC_EGENERIC;

    /* Allocate the output surface picture pool */
    sys->pool = OutputPoolAlloc(VLC_OBJECT(filter), vctx_out,
                                &filter->fmt_out.video);
    if (sys->pool == NULL)
    {
        vlc_video_context_Release(vctx_out);
        return VLC_EGENERIC;
    }

    /* Create the video-to-output mixer */
    chroma = vlc_video_context_GetPrivate(filter->vctx_in,
                                          VLC_VIDEO_CONTEXT_VDPAU);
    sys->mixer = MixerCreate(filter, *chroma);
    if (sys->mixer == VDP_INVALID_HANDLE)
    {
        picture_pool_Release(sys->pool);
        vlc_video_context_Release(vctx_out);
        return VLC_EGENERIC;
    }

    for (unsigned i = 0; i < MAX_PAST + MAX_FUTURE; i++)
        sys->history[i] = NULL;

    sys->procamp.brightness = 0.f;
    sys->procamp.contrast = 1.f;
    sys->procamp.saturation = 1.f;
    sys->procamp.hue = 0.f;

    filter->ops = &filter_output_opaque_ops;
    filter->vctx_out = vctx_out;
    return VLC_SUCCESS;
}

typedef struct
{
    VdpYCbCrFormat format;
} vlc_vdp_yuv_getter_t;

static picture_t *VideoExport_Filter(filter_t *filter, picture_t *src)
{
    vlc_vdp_yuv_getter_t *sys = filter->p_sys;

    if (unlikely(src->context == NULL))
    {
        msg_Err(filter, "corrupt VDPAU video surface %p", (void*)src);
        picture_Release(src);
        return NULL;
    }

    picture_t *dst = filter_NewPicture(filter);
    if (dst == NULL)
        return NULL;

    return VideoExport(filter, src, dst, sys->format);
}

static const struct vlc_filter_operations filter_ycbcr_ops = {
    .filter_video = VideoExport_Filter,
};

static int YCbCrOpen(filter_t *filter)
{
    VdpChromaType type;
    VdpYCbCrFormat format;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO)
        return VLC_ENOTSUP;
    if (filter->vctx_in == NULL)
        return VLC_EINVAL;

    const VdpChromaType *chroma = vlc_video_context_GetPrivate(filter->vctx_in,
                                                      VLC_VIDEO_CONTEXT_VDPAU);

    if (!vlc_fourcc_to_vdp_ycc(filter->fmt_out.video.i_chroma, &type, &format)
     || type != *chroma)
        return VLC_EGENERIC;

    if (filter->fmt_in.video.i_visible_width
                                       != filter->fmt_out.video.i_visible_width
     || filter->fmt_in.video.i_visible_height
                                      != filter->fmt_out.video.i_visible_height
     || filter->fmt_in.video.i_x_offset != filter->fmt_out.video.i_x_offset
     || filter->fmt_in.video.i_y_offset != filter->fmt_out.video.i_y_offset
     || (filter->fmt_in.video.i_sar_num * filter->fmt_out.video.i_sar_den
          != filter->fmt_in.video.i_sar_den * filter->fmt_out.video.i_sar_num))
        return VLC_EGENERIC;

    vlc_vdp_yuv_getter_t *sys = vlc_obj_malloc(VLC_OBJECT(filter), sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->format = format;

    filter->ops = &filter_ycbcr_ops;
    filter->p_sys = sys;
    return VLC_SUCCESS;
}

static const int algo_values[] = {
    -1,
    VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
    VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
};

static const char *const algo_names[] = {
    N_("Bob"), N_("Temporal"), N_("Temporal-spatial"),
};

static void ProbeChroma(vlc_chroma_conv_vec *vec)
{
    vlc_chroma_conv_add(vec, 0.25, VLC_CODEC_VDPAU_VIDEO,
        VLC_CODEC_VDPAU_OUTPUT, false);

    vlc_chroma_conv_add_in_outlist(vec, 1.1, VLC_CODEC_VDPAU_VIDEO, VLC_CODEC_I420,
        VLC_CODEC_YV12, VLC_CODEC_NV12, VLC_CODEC_I422, VLC_CODEC_NV16,
        VLC_CODEC_YUYV, VLC_CODEC_UYVY, VLC_CODEC_I444, VLC_CODEC_NV24);
}

vlc_module_begin()
    set_shortname(N_("VDPAU"))
    set_description(N_("VDPAU surface conversions"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_converter(OutputOpen, 10)

    add_integer("vdpau-deinterlace",
                VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
                N_("Deinterlace"), N_("Deinterlacing algorithm"))
        change_integer_list(algo_values, algo_names)
    add_bool("vdpau-ivtc", false,
             N_("Inverse telecine"), NULL)
    add_bool("vdpau-chroma-skip", false,
             N_("Deinterlace chroma skip"),
             N_("Whether temporal deinterlacing applies to luma only"))
    add_float_with_range("vdpau-noise-reduction", 0., 0., 1.,
        N_("Noise reduction level"), NULL)
    add_integer_with_range("vdpau-scaling", 0, 0, 9,
       N_("Scaling quality"), N_("High quality scaling level"))

    add_submodule()
    set_callback_video_converter(YCbCrOpen, 10)
    add_submodule()
        set_callback_chroma_conv_probe(ProbeChroma)
vlc_module_end()
