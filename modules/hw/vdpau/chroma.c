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
    picture_t *history[MAX_PAST + 1 + MAX_FUTURE];
};

/** Create VDPAU video mixer */
static VdpVideoMixer MixerCreate(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    VdpVideoMixer mixer;
    VdpStatus err;

    VdpVideoMixerParameter parms[3] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
    };
    uint32_t width = filter->fmt_in.video.i_width;
    uint32_t height = filter->fmt_in.video.i_height;
    const void *values[3] = { &width, &height, &sys->chroma, };

    err = vdp_video_mixer_create(sys->vdp, sys->device, 0, NULL,
                                 3, parms, values, &mixer);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(filter, "video %s %s failure: %s", "mixer", "creation",
                vdp_get_error_string(sys->vdp, err));
        mixer = VDP_INVALID_HANDLE;
    }
    return mixer;
}

static void Flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    for (unsigned i = 0; i < MAX_PAST + MAX_FUTURE; i++)
        if (sys->history[i] != NULL)
        {
            picture_Release(sys->history[i]);
            sys->history[i] = NULL;
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

    picture_t *dst = OutputAllocate(filter);
    if (dst == NULL)
    {
        picture_Release(src);
        Flush(filter);
        return NULL;
    }

    src = sys->import(filter, src);

    /* Update history and take "present" picture */
    sys->history[MAX_PAST + MAX_FUTURE] = src;
    src = sys->history[MAX_PAST];

    if (src == NULL)
        goto skip;
    picture_CopyProperties(dst, src);

    /* Render video into output */
    VdpVideoMixerPictureStructure structure =
        ((vlc_vdp_video_field_t *)(src->context))->structure;
    VdpVideoSurface past[MAX_PAST];
    VdpVideoSurface surface = picture_GetVideoSurface(src);
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
    VdpStatus err;

    for (unsigned i = 0; i < MAX_PAST; i++)
    {
        picture_t *pic = sys->history[(MAX_PAST - 1) - i];
        if (pic != NULL)
            past[i] = picture_GetVideoSurface(pic);
        else
            past[i] = VDP_INVALID_HANDLE;
    }
    for (unsigned i = 0; i < MAX_FUTURE; i++)
    {
        picture_t *pic = sys->history[(MAX_PAST + 1) + i];
        if (pic != NULL)
            future[i] = picture_GetVideoSurface(pic);
        else
            future[i] = VDP_INVALID_HANDLE;
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

    /* Forget old history */
    if (sys->history[0] != NULL)
        picture_Release(sys->history[0]);
    memmove(sys->history, sys->history + 1,
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
        sys->history[i] = NULL;

    filter->pf_video_filter = MixerRender;
    filter->pf_video_flush = Flush;
    filter->p_sys = sys;
    return VLC_SUCCESS;
}

static void OutputClose(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    /*Flush(filter); -- core frees pictures for us! */
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

vlc_module_begin()
    set_shortname(N_("VDPAU"))
    set_description(N_("VDPAU surface conversions"))
    set_capability("video filter2", 10)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(OutputOpen, OutputClose)
    add_submodule()
    set_callbacks(YCbCrOpen, YCbCrClose)
vlc_module_end()
