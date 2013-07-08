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
    picture_t *(*import)(filter_t *, picture_t *);

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
    VdpColorStandard std = (filter->fmt_in.video.i_height > 576)
                         ? VDP_COLOR_STANDARD_ITUR_BT_709
                         : VDP_COLOR_STANDARD_ITUR_BT_601;

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
static VdpVideoMixer MixerCreate(filter_t *filter)
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
    uint32_t height = filter->fmt_in.video.i_height;
    const void *values[3] = { &width, &height, &sys->chroma, };

    err = vdp_video_mixer_create(sys->vdp, sys->device, featc, featv,
                                 3, parms, values, &mixer);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "creation",
                vdp_get_error_string(sys->vdp, err));
        mixer = VDP_INVALID_HANDLE;
    }

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
            sys->history[i].field->destroy(sys->history[i].field);
            sys->history[i].field = NULL;
        }
}

/** Get a VLC picture for a VDPAU output surface */
static picture_t *OutputAllocate(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    picture_t *pic = filter_NewPicture(filter);
    if (pic == NULL)
        return NULL;

    picture_sys_t *psys = pic->p_sys;
    assert(psys->vdp != NULL);
    if (unlikely(sys->vdp != psys->vdp))
    {
        if (sys->mixer != VDP_INVALID_HANDLE)
        {
            Flush(filter);
            vdp_video_mixer_destroy(sys->vdp, sys->mixer);
            sys->mixer = VDP_INVALID_HANDLE;
        }
        sys->vdp = psys->vdp;
        sys->device = psys->device;
    }

    if (unlikely(sys->mixer == VDP_INVALID_HANDLE))
    {
        sys->mixer = MixerCreate(filter);
        if (sys->mixer == VDP_INVALID_HANDLE)
            goto error;
        msg_Dbg(filter, "using video mixer %"PRIu32, sys->mixer);
    }
    return pic;
error:
    picture_Release(pic);
    return NULL;
}

/** Export a VDPAU video surface picture to a normal VLC picture */
static picture_t *VideoExport(filter_t *filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_vdp_video_field_t *field = src->context;
    vlc_vdp_video_frame_t *psys = field->frame;
    VdpStatus err;
    VdpVideoSurface surface = psys->surface;
    void *planes[3];
    uint32_t pitches[3];

    for (int i = 0; i < dst->i_planes; i++)
    {
        planes[i] = dst->p[i].p_pixels;
        pitches[i] = dst->p[i].i_pitch;
    }
    if (dst->format.i_chroma == VLC_CODEC_I420)
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
    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}

/** Import VLC picture into VDPAU video surface */
static picture_t *VideoImport(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    VdpVideoSurface surface;
    VdpStatus err;

    /* Create surface (TODO: reuse?) */
    err = vdp_video_surface_create(sys->vdp, sys->device, sys->chroma,
                                   filter->fmt_in.video.i_width,
                                   filter->fmt_in.video.i_height, &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "creation",
                vdp_get_error_string(sys->vdp, err));
        picture_Release(src);
        return NULL;
    }

    /* Put bits */
    const void *planes[3];
    uint32_t pitches[3];
    for (int i = 0; i < src->i_planes; i++)
    {
        planes[i] = src->p[i].p_pixels;
        pitches[i] = src->p[i].i_pitch;
    }
    if (src->format.i_chroma == VLC_CODEC_I420)
    {
        planes[1] = src->p[2].p_pixels;
        planes[2] = src->p[1].p_pixels;
        pitches[1] = src->p[2].i_pitch;
        pitches[2] = src->p[1].i_pitch;
    }
    err = vdp_video_surface_put_bits_y_cb_cr(sys->vdp, surface, sys->format,
                                             planes, pitches);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "surface", "import",
                vdp_get_error_string(sys->vdp, err));
        vdp_video_surface_destroy(sys->vdp, surface);
        goto error;
    }

    /* Wrap surface into a picture */
    video_format_t fmt = src->format;
    fmt.i_chroma = (sys->chroma == VDP_CHROMA_TYPE_420)
        ? VLC_CODEC_VDPAU_VIDEO_420 : VLC_CODEC_VDPAU_VIDEO_422;

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
    picture_Release(src);
    return NULL;
}

static picture_t *VideoPassthrough(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_vdp_video_field_t *field = src->context;

    if (unlikely(field == NULL))
    {
        msg_Err(filter, "corrupt VDPAU video surface");
        return NULL;
    }

    vlc_vdp_video_frame_t *psys = field->frame;

    /* Corner case: different VDPAU instances decoding and rendering */
    if (psys->vdp != sys->vdp)
    {
        video_format_t fmt = src->format;
        fmt.i_chroma = (sys->chroma == VDP_CHROMA_TYPE_420)
            ? VLC_CODEC_UYVY : VLC_CODEC_NV12;

        picture_t *pic = picture_NewFromFormat(&fmt);
        if (unlikely(pic == NULL))
            return NULL;

        pic = VideoExport(filter, src, pic);
        if (pic == NULL)
            return NULL;

        src = VideoImport(filter, pic);
    }
    return src;
}

static inline VdpVideoSurface picture_GetVideoSurface(const picture_t *pic)
{
    vlc_vdp_video_field_t *field = pic->context;
    return field->frame->surface;
}

static picture_t *MixerRender(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    VdpStatus err;

    picture_t *dst = OutputAllocate(filter);
    if (dst == NULL)
    {
        picture_Release(src);
        Flush(filter);
        return NULL;
    }

    src = sys->import(filter, src);

    /* Update history and take "present" picture field */
    sys->history[MAX_PAST + MAX_FUTURE].field = vlc_vdp_video_detach(src);
    sys->history[MAX_PAST + MAX_FUTURE].date = src->date;
    sys->history[MAX_PAST + MAX_FUTURE].force = src->b_force;
    picture_Release(src);

    vlc_vdp_video_field_t *f = sys->history[MAX_PAST].field;
    if (f == NULL)
        goto skip;
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

    /* Render video into output */
    VdpVideoMixerPictureStructure structure = f->structure;
    VdpVideoSurface past[MAX_PAST];
    VdpVideoSurface surface = f->frame->surface;
    VdpVideoSurface future[MAX_FUTURE];
    VdpRect src_rect = {
        filter->fmt_in.video.i_x_offset, filter->fmt_in.video.i_y_offset,
        filter->fmt_in.video.i_visible_width + filter->fmt_in.video.i_x_offset,
        filter->fmt_in.video.i_visible_height + filter->fmt_in.video.i_y_offset
    };
    VdpOutputSurface output = dst->p_sys->surface;
    VdpRect dst_rect = {
        0, 0,
        filter->fmt_out.video.i_visible_width,
        filter->fmt_out.video.i_visible_height
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
                                 &src_rect, output, &dst_rect, NULL, 0, NULL);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "rendering",
                vdp_get_error_string(sys->vdp, err));
skip:
        picture_Release(dst);
        dst = NULL;
    }

    f = sys->history[0].field;
    if (f != NULL)
        f->destroy(f); /* Release oldest field */
    memmove(sys->history, sys->history + 1, /* Advance history */
            sizeof (sys->history[0]) * (MAX_PAST + MAX_FUTURE));

    return dst;
}

static int OutputOpen(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    if (filter->fmt_out.video.i_chroma != VLC_CODEC_VDPAU_OUTPUT)
        return VLC_EGENERIC;

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->vdp = NULL;
    sys->mixer = VDP_INVALID_HANDLE;

    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VDPAU_VIDEO_422)
    {
        sys->chroma = VDP_CHROMA_TYPE_422;
        sys->format = VDP_YCBCR_FORMAT_UYVY;
        sys->import = VideoPassthrough;
    }
    else
    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VDPAU_VIDEO_420)
    {
        sys->chroma = VDP_CHROMA_TYPE_420;
        sys->format = VDP_YCBCR_FORMAT_NV12;
        sys->import = VideoPassthrough;
    }
    else
    if (vlc_fourcc_to_vdp_ycc(filter->fmt_in.video.i_chroma,
                              &sys->chroma, &sys->format))
        sys->import = VideoImport;
    else
    {
        free(sys);
        return VLC_EGENERIC;
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

    filter->pf_video_filter = MixerRender;
    filter->pf_video_flush = Flush;
    filter->p_sys = sys;
    return VLC_SUCCESS;
}

static void OutputClose(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    Flush(filter);
    if (sys->mixer != VDP_INVALID_HANDLE)
        vdp_video_mixer_destroy(sys->vdp, sys->mixer);

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

static int YCbCrOpen(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_420
     && filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO_422)
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

    if (!vlc_fourcc_to_vdp_ycc(filter->fmt_out.video.i_chroma,
                               &sys->chroma, &sys->format))
    {
        free(sys);
        return VLC_EGENERIC;
    }

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
    set_capability("video filter2", 10)
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
