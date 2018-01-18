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
#include "vlc_vdpau.h"

/* Picture history as recommended by VDPAU documentation */
#define MAX_PAST   2
#define MAX_FUTURE 1

struct filter_sys_t
{
    vdp_t *vdp;
    VdpDevice device;
    VdpVideoMixer mixer;
    VdpChromaType chroma;
    VdpYCbCrFormat format;

    struct
    {
        vlc_vdp_video_field_t *field;
        mtime_t date;
        bool force;
    } history[MAX_PAST + 1 + MAX_FUTURE];

    struct
    {
        float brightness;
        float contrast;
        float saturation;
        float hue;
    } procamp;
};

/** Initialize the colour space conversion matrix */
static VdpStatus MixerSetupColors(filter_t *filter, const VdpProcamp *procamp,
                                  VdpCSCMatrix *restrict csc)
{
    filter_sys_t *sys = filter->p_sys;
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

    err = vdp_generate_csc_matrix(sys->vdp, procamp, std, csc);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s failure: %s", "color space matrix",
                vdp_get_error_string(sys->vdp, err));
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
static VdpVideoMixer MixerCreate(filter_t *filter, bool import)
{
    filter_sys_t *sys = filter->p_sys;
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
        err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
                                                    algo, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
            msg_Dbg(filter, "using video mixer %s feature",
                    "temporal-spatial deinterlace");
        else
            algo = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL; /* fallback */
    }
    if (algo == VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL)
    {
        err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
                                                    algo, &ok);
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
            err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
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
        err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
                                 VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION, &ok);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
        {
            msg_Dbg(filter, "using video mixer %s feature", "noise reduction");
            featv[featc++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
        }
    }

    err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
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

        err = vdp_video_mixer_query_feature_support(sys->vdp, sys->device,
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
    uint32_t height = import ? filter->fmt_in.video.i_visible_height
                             : filter->fmt_in.video.i_height;
    const void *values[3] = { &width, &height, &sys->chroma, };

    err = vdp_video_mixer_create(sys->vdp, sys->device, featc, featv,
                                 3, parms, values, &mixer);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "creation",
                vdp_get_error_string(sys->vdp, err));
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

        err = vdp_video_mixer_set_feature_enables(sys->vdp, mixer,
                                                  featc, featv, enablev);
        if (err != VDP_STATUS_OK)
            msg_Err(filter, "video %s %s failure: %s", "mixer", "features",
                    vdp_get_error_string(sys->vdp, err));
    }

    if (attrc > 0)
    {
        err = vdp_video_mixer_set_attribute_values(sys->vdp, mixer,
                                                   attrc, attrv, valv);
        if (err != VDP_STATUS_OK)
            msg_Err(filter, "video %s %s failure: %s", "mixer", "attributes",
                    vdp_get_error_string(sys->vdp, err));
    }

    return mixer;
}

static void Flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    for (unsigned i = 0; i < MAX_PAST + MAX_FUTURE; i++)
        if (sys->history[i].field != NULL)
        {
            vlc_vdp_video_destroy(sys->history[i].field);
            sys->history[i].field = NULL;
        }
}

/** Export a VDPAU video surface picture to a normal VLC picture */
static picture_t *VideoExport(filter_t *filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_vdp_video_field_t *field = (vlc_vdp_video_field_t *)src->context;
    vlc_vdp_video_frame_t *psys = field->frame;
    VdpStatus err;
    VdpVideoSurface surface = psys->surface;
    void *planes[3];
    uint32_t pitches[3];

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
    err = vdp_video_surface_get_bits_y_cb_cr(psys->vdp, surface, sys->format,
                                             planes, pitches);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "export",
                vdp_get_error_string(psys->vdp, err));
        picture_Release(dst);
        dst = NULL;
    }
    picture_Release(src);
    return dst;
}

/** Import VLC picture into VDPAU video surface */
static picture_t *VideoImport(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    VdpVideoSurface surface;
    VdpStatus err;

    if (sys->vdp == NULL)
        goto drop;

    /* Create surface (TODO: reuse?) */
    err = vdp_video_surface_create(sys->vdp, sys->device, sys->chroma,
                                   filter->fmt_in.video.i_width,
                                   filter->fmt_in.video.i_visible_height,
                                   &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "creation",
                vdp_get_error_string(sys->vdp, err));
        goto drop;
    }

    /* Put bits */
    const void *planes[3];
    uint32_t pitches[3];
    for (int i = 0; i < src->i_planes; i++)
    {
        planes[i] = src->p[i].p_pixels
                  + filter->fmt_in.video.i_y_offset * src->p[i].i_pitch;
        pitches[i] = src->p[i].i_pitch;
    }
    if (src->format.i_chroma == VLC_CODEC_I420
     || src->format.i_chroma == VLC_CODEC_I422
     || src->format.i_chroma == VLC_CODEC_I444)
    {
        planes[1] = src->p[2].p_pixels;
        planes[2] = src->p[1].p_pixels;
        pitches[1] = src->p[2].i_pitch;
        pitches[2] = src->p[1].i_pitch;
    }
    if (src->format.i_chroma == VLC_CODEC_I420
     || src->format.i_chroma == VLC_CODEC_YV12
     || src->format.i_chroma == VLC_CODEC_NV12)
    {
        for (int i = 1; i < src->i_planes; i++)
            planes[i] = ((const uint8_t *)planes[i])
                + (filter->fmt_in.video.i_y_offset / 2) * src->p[i].i_pitch;
    }

    err = vdp_video_surface_put_bits_y_cb_cr(sys->vdp, surface, sys->format,
                                             planes, pitches);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "import",
                vdp_get_error_string(sys->vdp, err));
        goto error;
    }

    /* Wrap surface into a picture */
    video_format_t fmt = src->format;

    switch (sys->chroma)
    {
        case VDP_CHROMA_TYPE_420:
            fmt.i_chroma = VLC_CODEC_VDPAU_VIDEO_420;
            break;
        case VDP_CHROMA_TYPE_422:
            fmt.i_chroma = VLC_CODEC_VDPAU_VIDEO_422;
            break;
        case VDP_CHROMA_TYPE_444:
            fmt.i_chroma = VLC_CODEC_VDPAU_VIDEO_444;
            break;
        default:
            vlc_assert_unreachable();
    }


    picture_t *dst = picture_NewFromFormat(&fmt);
    if (unlikely(dst == NULL))
        goto error;
    picture_CopyProperties(dst, src);
    picture_Release(src);

    err = vlc_vdp_video_attach(sys->vdp, surface, dst);
    if (unlikely(err != VDP_STATUS_OK))
    {
        picture_Release(dst);
        dst = NULL;
    }
    return dst;
error:
    vdp_video_surface_destroy(sys->vdp, surface);
drop:
    picture_Release(src);
    return NULL;
}

static picture_t *Render(filter_t *filter, picture_t *src, bool import)
{
    filter_sys_t *sys = filter->p_sys;
    picture_t *dst = NULL;
    VdpStatus err;

    if (unlikely(src->context == NULL))
    {
        msg_Err(filter, "corrupt VDPAU video surface %p", (void *)src);
        picture_Release(src);
        return NULL;
    }

    /* Corner case: different VDPAU instances decoding and rendering */
    vlc_vdp_video_field_t *field = (vlc_vdp_video_field_t *)src->context;
    if (field->frame->vdp != sys->vdp)
    {
        video_format_t fmt = src->format;
        switch (sys->chroma)
        {
             case VDP_CHROMA_TYPE_420: fmt.i_chroma = VLC_CODEC_NV12; break;
             case VDP_CHROMA_TYPE_422: fmt.i_chroma = VLC_CODEC_UYVY; break;
             case VDP_CHROMA_TYPE_444: fmt.i_chroma = VLC_CODEC_NV24; break;
             default: vlc_assert_unreachable();
        }

        picture_t *pic = picture_NewFromFormat(&fmt);
        if (likely(pic != NULL))
        {
            pic = VideoExport(filter, src, pic);
            if (pic != NULL)
                src = VideoImport(filter, pic);
            else
                src = NULL;
        }
        else
        {
            picture_Release(src);
            src = NULL;
        }
    }

    /* Update history and take "present" picture field */
    if (likely(src != NULL))
    {
        sys->history[MAX_PAST + MAX_FUTURE].field =
            vlc_vdp_video_copy((vlc_vdp_video_field_t *)src->context);
        sys->history[MAX_PAST + MAX_FUTURE].date = src->date;
        sys->history[MAX_PAST + MAX_FUTURE].force = src->b_force;
        picture_Release(src);
    }
    else
    {
        sys->history[MAX_PAST + MAX_FUTURE].field = NULL;
        sys->history[MAX_PAST + MAX_FUTURE].force = false;
    }

    vlc_vdp_video_field_t *f = sys->history[MAX_PAST].field;
    if (f == NULL)
    {   /* There is no present field, probably just starting playback. */
        if (!sys->history[MAX_PAST + MAX_FUTURE].force)
            goto skip;

        /* If the picture is forced, ignore deinterlacing and fast forward. */
        /* FIXME: Remove the forced hack pictures in video output core and
         * allow the last field of a video to be rendered properly. */
        while (sys->history[MAX_PAST].field == NULL)
        {
            f = sys->history[0].field;
            if (f != NULL)
                vlc_vdp_video_destroy(f);

            memmove(sys->history, sys->history + 1,
                    sizeof (sys->history[0]) * (MAX_PAST + MAX_FUTURE));
            sys->history[MAX_PAST + MAX_FUTURE].field = NULL;
        }
        f = sys->history[MAX_PAST].field;
    }

    /* Get a VLC picture for a VDPAU output surface */
    dst = filter_NewPicture(filter);
    if (dst == NULL)
        goto skip;

    assert(dst->p_sys != NULL && dst->p_sys->vdp ==sys->vdp);
    dst->date = sys->history[MAX_PAST].date;
    dst->b_force = sys->history[MAX_PAST].force;

    /* Enable/Disable features */
    const VdpVideoMixerFeature features[] = {
        VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
    };
    const VdpBool enables[] = {
        f->sharpen != 0.f,
    };

    err = vdp_video_mixer_set_feature_enables(sys->vdp, sys->mixer,
                  sizeof (features) / sizeof (features[0]), features, enables);
    if (err != VDP_STATUS_OK)
        msg_Err(filter, "video %s %s failure: %s", "mixer", "features",
                vdp_get_error_string(sys->vdp, err));

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

    err = vdp_video_mixer_set_attribute_values(sys->vdp, sys->mixer,
                                               count, attrs, values);
    if (err != VDP_STATUS_OK)
        msg_Err(filter, "video %s %s failure: %s", "mixer", "attributes",
                vdp_get_error_string(sys->vdp, err));

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

    VdpOutputSurface output = dst->p_sys->surface;

    if (swap)
    {
        VdpRGBAFormat fmt;
        uint32_t width, height;

        err = vdp_output_surface_get_parameters(sys->vdp, output,
                                                &fmt, &width, &height);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "query",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }

        err = vdp_output_surface_create(sys->vdp, sys->device,
                                        fmt, height, width, &output);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "creation",
                    vdp_get_error_string(sys->vdp, err));
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

    if (import)
        src_rect.y0 = src_rect.y1 = 0;
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
        f = sys->history[(MAX_PAST - 1) - i].field;
        past[i] = (f != NULL) ? f->frame->surface : VDP_INVALID_HANDLE;
    }
    for (unsigned i = 0; i < MAX_FUTURE; i++)
    {
        f = sys->history[(MAX_PAST + 1) + i].field;
        future[i] = (f != NULL) ? f->frame->surface : VDP_INVALID_HANDLE;
    }

    err = vdp_video_mixer_render(sys->vdp, sys->mixer, VDP_INVALID_HANDLE,
                                 NULL, structure,
                                 MAX_PAST, past, surface, MAX_FUTURE, future,
                                 &src_rect, output, &dst_rect, &dst_rect, 0,
                                 NULL);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "rendering",
                vdp_get_error_string(sys->vdp, err));
        goto error;
    }

    if (swap)
    {
        err = vdp_output_surface_render_output_surface(sys->vdp,
            dst->p_sys->surface, NULL, output, NULL, NULL, NULL,
            VDP_OUTPUT_SURFACE_RENDER_ROTATE_90);
        vdp_output_surface_destroy(sys->vdp, output);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(filter, "output %s %s failure: %s", "surface", "render",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }
    }

skip:
    f = sys->history[0].field;
    if (f != NULL)
        vlc_vdp_video_destroy(f); /* Release oldest field */
    memmove(sys->history, sys->history + 1, /* Advance history */
            sizeof (sys->history[0]) * (MAX_PAST + MAX_FUTURE));

    return dst;
error:
    picture_Release(dst);
    dst = NULL;
    goto skip;
}

static picture_t *VideoRender(filter_t *filter, picture_t *src)
{
    return Render(filter, src, false);
}


static picture_t *YCbCrRender(filter_t *filter, picture_t *src)
{
    src = VideoImport(filter, src);
    return (src != NULL) ? Render(filter, src, true) : NULL;
}

static int OutputOpen(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (filter->fmt_out.video.i_chroma != VLC_CODEC_VDPAU_OUTPUT)
        return VLC_EGENERIC;

    assert(filter->fmt_out.video.orientation == ORIENT_TOP_LEFT
        || filter->fmt_in.video.orientation == filter->fmt_out.video.orientation);

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    filter->p_sys = sys;

    picture_t *(*video_filter)(filter_t *, picture_t *) = VideoRender;

    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VDPAU_VIDEO_444)
    {
        sys->chroma = VDP_CHROMA_TYPE_444;
        sys->format = VDP_YCBCR_FORMAT_NV12;
    }
    else
    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VDPAU_VIDEO_422)
    {
        sys->chroma = VDP_CHROMA_TYPE_422;
        /* TODO: check if the drivery supports NV12 or UYVY */
        sys->format = VDP_YCBCR_FORMAT_UYVY;
    }
    else
    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VDPAU_VIDEO_420)
    {
        sys->chroma = VDP_CHROMA_TYPE_420;
        sys->format = VDP_YCBCR_FORMAT_NV12;
    }
    else
    if (vlc_fourcc_to_vdp_ycc(filter->fmt_in.video.i_chroma,
                              &sys->chroma, &sys->format))
        video_filter = YCbCrRender;
    else
        goto error;

    /* Get the context and allocate the mixer (through *ahem* picture) */
    picture_t *pic = filter_NewPicture(filter);
    if (pic == NULL)
        goto error;

    picture_sys_t *picsys = pic->p_sys;
    assert(picsys != NULL && picsys->vdp != NULL);

    sys->vdp = vdp_hold_x11(picsys->vdp, NULL);
    sys->device = picsys->device;
    picture_Release(pic);

    sys->mixer = MixerCreate(filter, video_filter == YCbCrRender);
    if (sys->mixer == VDP_INVALID_HANDLE)
    {
        vdp_release_x11(sys->vdp);
        goto error;
    }

    /* NOTE: The video mixer capabilities should be checked here, and the
     * then video mixer set up. But:
     * 1) The VDPAU back-end is accessible only once the first picture
     *    gets filtered. Thus the video mixer is created later.
     * 2) Bailing out due to insufficient capabilities would break the
     *    video pipeline. Thus capabilities should be checked earlier. */

    for (unsigned i = 0; i < MAX_PAST + MAX_FUTURE; i++)
        sys->history[i].field = NULL;

    sys->procamp.brightness = 0.f;
    sys->procamp.contrast = 1.f;
    sys->procamp.saturation = 1.f;
    sys->procamp.hue = 0.f;

    filter->pf_video_filter = video_filter;
    filter->pf_flush = Flush;
    return VLC_SUCCESS;
error:
    free(sys);
    return VLC_EGENERIC;
}

static void OutputClose(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    Flush(filter);
    vdp_video_mixer_destroy(sys->vdp, sys->mixer);
    vdp_release_x11(sys->vdp);
    free(sys);
}

static picture_t *VideoExport_Filter(filter_t *filter, picture_t *src)
{
    if (unlikely(src->context == NULL))
    {
        msg_Err(filter, "corrupt VDPAU video surface %p", src);
        picture_Release(src);
        return NULL;
    }

    picture_t *dst = filter_NewPicture(filter);
    if (dst == NULL)
        return NULL;

    return VideoExport(filter, src, dst);
}

static bool ChromaMatches(VdpChromaType vdp_type, vlc_fourcc_t vlc_chroma)
{
    switch (vlc_chroma)
    {
        case VLC_CODEC_VDPAU_VIDEO_420:
            return vdp_type == VDP_CHROMA_TYPE_420;
        case VLC_CODEC_VDPAU_VIDEO_422:
            return vdp_type == VDP_CHROMA_TYPE_422;
        case VLC_CODEC_VDPAU_VIDEO_444:
            return vdp_type == VDP_CHROMA_TYPE_444;
        default:
            return false;
    }
}

static int YCbCrOpen(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    VdpChromaType type;
    VdpYCbCrFormat format;

    if (!vlc_fourcc_to_vdp_ycc(filter->fmt_out.video.i_chroma, &type, &format)
      || !ChromaMatches(type, filter->fmt_in.video.i_chroma))
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

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->chroma = type;
    sys->format = format;

    filter->pf_video_filter = VideoExport_Filter;
    filter->p_sys = sys;
    return VLC_SUCCESS;
}

static void YCbCrClose(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    free(sys);
}

static const int algo_values[] = {
    -1,
    VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
    VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
};

static const char *const algo_names[] = {
    N_("Bob"), N_("Temporal"), N_("Temporal-spatial"),
};

vlc_module_begin()
    set_shortname(N_("VDPAU"))
    set_description(N_("VDPAU surface conversions"))
    set_capability("video converter", 10)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(OutputOpen, OutputClose)

    add_integer("vdpau-deinterlace",
                VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
                N_("Deinterlace"), N_("Deinterlacing algorithm"), true)
        change_integer_list(algo_values, algo_names)
    add_bool("vdpau-ivtc", false,
             N_("Inverse telecine"), N_("Inverse telecine"), true)
    add_bool("vdpau-chroma-skip", false,
             N_("Deinterlace chroma skip"),
             N_("Whether temporal deinterlacing applies to luma only"), true)
    add_float_with_range("vdpau-noise-reduction", 0., 0., 1.,
        N_("Noise reduction level"), N_("Noise reduction level"), true)
    add_integer_with_range("vdpau-scaling", 0, 0, 9,
       N_("Scaling quality"), N_("High quality scaling level"), true)

    add_submodule()
    set_callbacks(YCbCrOpen, YCbCrClose)
vlc_module_end()
