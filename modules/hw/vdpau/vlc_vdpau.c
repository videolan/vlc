/*****************************************************************************
 * vlc_vdpau.c: VDPAU helper for VLC
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
# include <config.h>
#endif

#include <stdlib.h>
#include <vlc_common.h>
#include "vlc_vdpau.h"

#pragma GCC visibility push(default)

typedef struct
{
    VdpGetErrorString *get_error_string;
    VdpGetProcAddress *get_proc_address;
    VdpGetApiVersion *get_api_version;
    void *dummy3;
    VdpGetInformationString *get_information_string;
    VdpDeviceDestroy *device_destroy;
    VdpGenerateCSCMatrix *generate_csc_matrix;
    VdpVideoSurfaceQueryCapabilities *video_surface_query_capabilities;
    VdpVideoSurfaceQueryGetPutBitsYCbCrCapabilities *video_surface_query_get_put_bits_y_cb_cr_capabilities;
    VdpVideoSurfaceCreate *video_surface_create;
    VdpVideoSurfaceDestroy *video_surface_destroy;
    VdpVideoSurfaceGetParameters *video_surface_get_parameters;
    VdpVideoSurfaceGetBitsYCbCr *video_surface_get_bits_y_cb_cr;
    VdpVideoSurfacePutBitsYCbCr *video_surface_put_bits_y_cb_cr;
    VdpOutputSurfaceQueryCapabilities *output_surface_query_capabilities;
    VdpOutputSurfaceQueryGetPutBitsNativeCapabilities *output_surface_query_get_put_bits_native_capabilities;
    VdpOutputSurfaceQueryPutBitsIndexedCapabilities *output_surface_query_put_bits_indexed_capabilities;
    VdpOutputSurfaceQueryPutBitsYCbCrCapabilities *output_surface_query_put_bits_y_cb_cr_capabilities;
    VdpOutputSurfaceCreate *output_surface_create;
    VdpOutputSurfaceDestroy *output_surface_destroy;
    VdpOutputSurfaceGetParameters *output_surface_get_parameters;
    VdpOutputSurfaceGetBitsNative *output_surface_get_bits_native;
    VdpOutputSurfacePutBitsNative *output_surface_put_bits_native;
    VdpOutputSurfacePutBitsIndexed *output_surface_put_bits_indexed;
    VdpOutputSurfacePutBitsYCbCr *output_surface_put_bits_y_cb_cr;
    VdpBitmapSurfaceQueryCapabilities *bitmap_surface_query_capabilities;
    VdpBitmapSurfaceCreate *bitmap_surface_create;
    VdpBitmapSurfaceDestroy *bitmap_surface_destroy;
    VdpBitmapSurfaceGetParameters *bitmap_surface_get_parameters;
    VdpBitmapSurfacePutBitsNative *bitmap_surface_put_bits_native;
    void *dummy30;
    void *dummy31;
    void *dummy32;
    VdpOutputSurfaceRenderOutputSurface *output_surface_render_output_surface;
    VdpOutputSurfaceRenderBitmapSurface *output_surface_render_bitmap_surface;
    void *output_surface_render_video_surface_luma;
    VdpDecoderQueryCapabilities *decoder_query_capabilities;
    VdpDecoderCreate *decoder_create;
    VdpDecoderDestroy *decoder_destroy;
    VdpDecoderGetParameters *decoder_get_parameters;
    VdpDecoderRender *decoder_render;
    VdpVideoMixerQueryFeatureSupport *video_mixer_query_feature_support;
    VdpVideoMixerQueryParameterSupport *video_mixer_query_parameter_support;
    VdpVideoMixerQueryAttributeSupport *video_mixer_query_attribute_support;
    VdpVideoMixerQueryParameterValueRange *video_mixer_query_parameter_value_range;
    VdpVideoMixerQueryAttributeValueRange *video_mixer_query_attribute_value_range;
    VdpVideoMixerCreate *video_mixer_create;
    VdpVideoMixerSetFeatureEnables *video_mixer_set_feature_enables;
    VdpVideoMixerSetAttributeValues *video_mixer_set_attribute_values;
    VdpVideoMixerGetFeatureSupport *video_mixer_get_feature_support;
    VdpVideoMixerGetFeatureEnables *video_mixer_get_feature_enables;
    VdpVideoMixerGetParameterValues *video_mixer_get_parameter_values;
    VdpVideoMixerGetAttributeValues *video_mixer_get_attribute_values;
    VdpVideoMixerDestroy *video_mixer_destroy;
    VdpVideoMixerRender *video_mixer_render;
    VdpPresentationQueueTargetDestroy *presentation_queue_target_destroy;
    VdpPresentationQueueCreate *presentation_queue_create;
    VdpPresentationQueueDestroy *presentation_queue_destroy;
    VdpPresentationQueueSetBackgroundColor *presentation_queue_set_background_color;
    VdpPresentationQueueGetBackgroundColor *presentation_queue_get_background_color;
    void *dummy60;
    void *dummy61;
    VdpPresentationQueueGetTime *presentation_queue_get_time;
    VdpPresentationQueueDisplay *presentation_queue_display;
    VdpPresentationQueueBlockUntilSurfaceIdle *presentation_queue_block_until_surface_idle;
    VdpPresentationQueueQuerySurfaceStatus *presentation_queue_query_surface_status;
    VdpPreemptionCallbackRegister *preemption_callback_register;
} vdp_vtable_t;

struct vdp_s
{
    union
    {
        vdp_vtable_t vt;
        void *funcs[1];
    }; /**< VDPAU function pointers table */
    void *handle; /**< Shared library handle */
};

const char *vdp_get_error_string(const vdp_t *vdp, VdpStatus status)
{
    return vdp->vt.get_error_string(status);
}

static const char *vdp_get_error_string_fallback(const vdp_t *vdp,
                                                 VdpStatus status)
{
    (void) vdp;
    return (status != VDP_STATUS_OK) ? "Unknown error" : "No error";
}

VdpStatus vdp_get_proc_address(const vdp_t *vdp, VdpDevice device,
    VdpFuncId func_id, void **func_ptr)
{
    return vdp->vt.get_proc_address(device, func_id, func_ptr);
}

VdpStatus vdp_get_api_version(const vdp_t *vdp, uint32_t *ver)
{
    return vdp->vt.get_api_version(ver);
}

VdpStatus vdp_get_information_string(const vdp_t *vdp, const char **str)
{
    return vdp->vt.get_information_string(str);
}

/*** Device ***/
VdpStatus vdp_device_destroy(const vdp_t *vdp, VdpDevice device)
{
    return vdp->vt.device_destroy(device);
}

VdpStatus vdp_generate_csc_matrix(const vdp_t *vdp, const VdpProcamp *procamp,
    VdpColorStandard standard, VdpCSCMatrix *csc_matrix)
{
    VdpProcamp buf, *copy = NULL;

    if (procamp != NULL)
    {
        buf = *procamp;
        copy = &buf;
    }
    return vdp->vt.generate_csc_matrix(copy, standard, csc_matrix);
}

/*** Video surface ***/
VdpStatus vdp_video_surface_query_capabilities(const vdp_t *vdp, VdpDevice dev,
    VdpChromaType type, VdpBool *ok, uint32_t *mw, uint32_t *mh)
{
    return vdp->vt.video_surface_query_capabilities(dev, type, ok, mw, mh);
}

VdpStatus vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
    const vdp_t *vdp, VdpDevice device, VdpChromaType type, VdpYCbCrFormat fmt,
    VdpBool *ok)
{
    return vdp->vt.video_surface_query_get_put_bits_y_cb_cr_capabilities(
        device, type, fmt, ok);
}

VdpStatus vdp_video_surface_create(const vdp_t *vdp, VdpDevice device,
    VdpChromaType chroma, uint32_t w, uint32_t h, VdpVideoSurface *surface)
{
    return vdp->vt.video_surface_create(device, chroma, w, h, surface);
}

VdpStatus vdp_video_surface_destroy(const vdp_t *vdp, VdpVideoSurface surface)
{
    return vdp->vt.video_surface_destroy(surface);
}

VdpStatus vdp_video_surface_get_parameters(const vdp_t *vdp,
    VdpVideoSurface surface, VdpChromaType *type, uint32_t *w, uint32_t *h)
{
    return vdp->vt.video_surface_get_parameters(surface, type, w, h);
}

VdpStatus vdp_video_surface_get_bits_y_cb_cr(const vdp_t *vdp,
    VdpVideoSurface surface, VdpYCbCrFormat fmt,
    void *const *data, uint32_t const *pitches)
{
    return vdp->vt.video_surface_get_bits_y_cb_cr(surface, fmt, data, pitches);
}

VdpStatus vdp_video_surface_put_bits_y_cb_cr(const vdp_t *vdp,
    VdpVideoSurface surface, VdpYCbCrFormat fmt,
    const void *const *data, uint32_t const *pitches)
{
    return vdp->vt.video_surface_put_bits_y_cb_cr(surface, fmt, data, pitches);
}

/*** Output surface ***/
VdpStatus vdp_output_surface_query_capabilities(const vdp_t *vdp,
    VdpDevice device, VdpRGBAFormat fmt, VdpBool *ok,
    uint32_t *max_width, uint32_t *max_height)
{
    return vdp->vt.output_surface_query_capabilities(device, fmt, ok,
                                                     max_width, max_height);
}

VdpStatus vdp_output_surface_query_get_put_bits_native_capabilities(
    const vdp_t *vdp, VdpDevice device, VdpRGBAFormat fmt, VdpBool *ok)
{
    return vdp->vt.output_surface_query_get_put_bits_native_capabilities(
                                                              device, fmt, ok);
}

VdpStatus vdp_output_surface_query_put_bits_indexed_capabilities(
    const vdp_t *vdp, VdpDevice device, VdpRGBAFormat fmt,
    VdpIndexedFormat idxfmt, VdpColorTableFormat colfmt, VdpBool *ok)
{
    return vdp->vt.output_surface_query_put_bits_indexed_capabilities(device,
                                                      fmt, idxfmt, colfmt, ok);
}

VdpStatus vdp_output_surface_query_put_bits_y_cb_cr_capabilities(
    const vdp_t *vdp, VdpDevice device,
    VdpRGBAFormat fmt, VdpYCbCrFormat yccfmt, VdpBool *ok)
{
    return vdp->vt.output_surface_query_put_bits_y_cb_cr_capabilities(device,
                                                              fmt, yccfmt, ok);
}

VdpStatus vdp_output_surface_create(const vdp_t *vdp, VdpDevice device,
    VdpRGBAFormat fmt, uint32_t w, uint32_t h, VdpOutputSurface *surface)
{
    return vdp->vt.output_surface_create(device, fmt, w, h, surface);
}

VdpStatus vdp_output_surface_destroy(const vdp_t *vdp,
                                     VdpOutputSurface surface)
{
    return vdp->vt.output_surface_destroy(surface);
}

VdpStatus vdp_output_surface_get_parameters(const vdp_t *vdp,
    VdpOutputSurface surface, VdpRGBAFormat *fmt, uint32_t *w, uint32_t *h)
{
    return vdp->vt.output_surface_get_parameters(surface, fmt, w, h);
}

VdpStatus vdp_output_surface_get_bits_native(const vdp_t *vdp,
    VdpOutputSurface surface, const VdpRect *src,
    void *const *data, uint32_t const *pitches)
{
    return vdp->vt.output_surface_get_bits_native(surface, src, data, pitches);
}

VdpStatus vdp_output_surface_put_bits_native(const vdp_t *vdp,
    VdpOutputSurface surface, const void *const *data, uint32_t const *pitches,
    const VdpRect *dst)
{
    return vdp->vt.output_surface_put_bits_native(surface, data, pitches, dst);
}

VdpStatus vdp_output_surface_put_bits_indexed(const vdp_t *vdp,
    VdpOutputSurface surface, VdpIndexedFormat fmt, const void *const *data,
    const uint32_t *pitch, const VdpRect *dst,
    VdpColorTableFormat tabfmt, const void *tab)
{
    return vdp->vt.output_surface_put_bits_indexed(surface, fmt, data, pitch,
                                                   dst, tabfmt, tab);
}

VdpStatus vdp_output_surface_put_bits_y_cb_cr(const vdp_t *vdp,
    VdpOutputSurface surface, VdpYCbCrFormat fmt, const void *const *data,
    const uint32_t *pitches, const VdpRect *dst, const VdpCSCMatrix *mtx)
{
    return vdp->vt.output_surface_put_bits_y_cb_cr(surface, fmt, data,
                                                   pitches, dst, mtx);
}

/*** Bitmap surface ***/
VdpStatus vdp_bitmap_surface_query_capabilities(const vdp_t *vdp,
    VdpDevice device, VdpRGBAFormat fmt, VdpBool *ok, uint32_t *w, uint32_t *h)
{
    return vdp->vt.bitmap_surface_query_capabilities(device, fmt, ok, w, h);
}

VdpStatus vdp_bitmap_surface_create(const vdp_t *vdp, VdpDevice device,
    VdpRGBAFormat fmt, uint32_t w, uint32_t h, VdpBool fq,
    VdpBitmapSurface *surface)
{
    return vdp->vt.bitmap_surface_create(device, fmt, w, h, fq, surface);
}

VdpStatus vdp_bitmap_surface_destroy(const vdp_t *vdp,
                                     VdpBitmapSurface surface)
{
    return vdp->vt.bitmap_surface_destroy(surface);
}

VdpStatus vdp_bitmap_surface_get_parameters(const vdp_t *vdp,
    VdpBitmapSurface surface, VdpRGBAFormat *fmt, uint32_t *w, uint32_t *h,
    VdpBool *fq)
{
    return vdp->vt.bitmap_surface_get_parameters(surface, fmt, w, h, fq);
}

VdpStatus vdp_bitmap_surface_put_bits_native(const vdp_t *vdp,
    VdpBitmapSurface surface, const void *const *data, const uint32_t *pitch,
    const VdpRect *rect)
{
    return vdp->vt.bitmap_surface_put_bits_native(surface, data, pitch, rect);
}

VdpStatus vdp_output_surface_render_output_surface(const vdp_t *vdp,
    VdpOutputSurface dst_surface, const VdpRect *dst_rect,
    VdpOutputSurface src_surface, const VdpRect *src_rect,
    const VdpColor *colors,
    const VdpOutputSurfaceRenderBlendState *const state, uint32_t flags)
{
    return vdp->vt.output_surface_render_output_surface(dst_surface, dst_rect,
        src_surface, src_rect, colors, state, flags);
}

VdpStatus vdp_output_surface_render_bitmap_surface(const vdp_t *vdp,
    VdpOutputSurface dst_surface, const VdpRect *dst_rect,
    VdpBitmapSurface src_surface, const VdpRect *src_rect,
    const VdpColor *colors,
    const VdpOutputSurfaceRenderBlendState *state, uint32_t flags)
{
    return vdp->vt.output_surface_render_bitmap_surface(dst_surface, dst_rect,
        src_surface, src_rect, colors, state, flags);
}

/*** Decoder ***/
VdpStatus vdp_decoder_query_capabilities(const vdp_t *vdp, VdpDevice device,
    VdpDecoderProfile profile, VdpBool *ok, uint32_t *l, uint32_t *m,
    uint32_t *w, uint32_t *h)
{
    return vdp->vt.decoder_query_capabilities(device, profile, ok, l, m, w, h);
}

VdpStatus vdp_decoder_create(const vdp_t *vdp, VdpDevice device,
    VdpDecoderProfile profile, uint32_t w, uint32_t h, uint32_t refs,
    VdpDecoder *decoder)
{
    return vdp->vt.decoder_create(device, profile, w, h, refs, decoder);
}

VdpStatus vdp_decoder_destroy(const vdp_t *vdp, VdpDecoder decoder)
{
    return vdp->vt.decoder_destroy(decoder);
}

VdpStatus vdp_decoder_get_parameters(const vdp_t *vdp, VdpDecoder decoder,
    VdpDecoderProfile *profile, uint32_t *w, uint32_t *h)
{
    return vdp->vt.decoder_get_parameters(decoder, profile, w, h);
}

VdpStatus vdp_decoder_render(const vdp_t *vdp, VdpDecoder decoder,
    VdpVideoSurface target, const VdpPictureInfo *info,
    uint32_t bufv, const VdpBitstreamBuffer *bufc)

{
    return vdp->vt.decoder_render(decoder, target, info, bufv, bufc);
}

/*** Video mixer ***/
VdpStatus vdp_video_mixer_query_feature_support(const vdp_t *vdp,
    VdpDevice device, VdpVideoMixerFeature feature, VdpBool *ok)
{
    return vdp->vt.video_mixer_query_feature_support(device, feature, ok);
}

VdpStatus vdp_video_mixer_query_parameter_support(const vdp_t *vdp,
    VdpDevice device, VdpVideoMixerParameter parameter, VdpBool *ok)
{
    return vdp->vt.video_mixer_query_parameter_support(device, parameter, ok);
}

VdpStatus vdp_video_mixer_query_attribute_support(const vdp_t *vdp,
    VdpDevice device, VdpVideoMixerAttribute attribute, VdpBool *ok)
{
    return vdp->vt.video_mixer_query_attribute_support(device, attribute, ok);
}

VdpStatus vdp_video_mixer_query_parameter_value_range(const vdp_t *vdp,
    VdpDevice device, VdpVideoMixerParameter parameter, void *min, void *max)
{
    return vdp->vt.video_mixer_query_parameter_value_range(device, parameter,
        min, max);
}

VdpStatus vdp_video_mixer_query_attribute_value_range(const vdp_t *vdp,
    VdpDevice device, VdpVideoMixerAttribute attribute, void *min, void *max)
{
    return vdp->vt.video_mixer_query_attribute_value_range(device, attribute,
        min, max);
}

VdpStatus vdp_video_mixer_create(const vdp_t *vdp, VdpDevice device,
    uint32_t featc, const VdpVideoMixerFeature *featv,
    uint32_t parmc, const VdpVideoMixerParameter *parmv,
    const void *const *parmvalv, VdpVideoMixer *mixer)
{
    return vdp->vt.video_mixer_create(device, featc, featv, parmc, parmv,
                                      parmvalv, mixer);
}

VdpStatus vdp_video_mixer_set_feature_enables(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count, const VdpVideoMixerFeature *ids,
    const VdpBool *values)
{
    return vdp->vt.video_mixer_set_feature_enables(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_set_attribute_values(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count,
    const VdpVideoMixerAttribute *const ids, const void *const *values)
{
    return vdp->vt.video_mixer_set_attribute_values(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_get_feature_support(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count, const VdpVideoMixerFeature *ids,
    VdpBool *values)
{
    return vdp->vt.video_mixer_get_feature_support(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_get_feature_enables(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count, const VdpVideoMixerFeature *ids,
    VdpBool *values)
{
    return vdp->vt.video_mixer_get_feature_enables(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_get_parameter_values(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count, const VdpVideoMixerParameter *ids,
    void *const *values)
{
    return vdp->vt.video_mixer_get_parameter_values(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_get_attribute_values(const vdp_t *vdp,
    VdpVideoMixer mixer, uint32_t count, const VdpVideoMixerAttribute *ids,
    void *const *values)
{
    return vdp->vt.video_mixer_get_attribute_values(mixer, count, ids, values);
}

VdpStatus vdp_video_mixer_destroy(const vdp_t *vdp, VdpVideoMixer mixer)
{
    return vdp->vt.video_mixer_destroy(mixer);
}

VdpStatus vdp_video_mixer_render(const vdp_t *vdp, VdpVideoMixer mixer,
    VdpOutputSurface bgsurface, const VdpRect *bgrect,
    VdpVideoMixerPictureStructure pic_struct, uint32_t prev_count,
    const VdpVideoSurface *prev, VdpVideoSurface cur, uint32_t next_count,
    const VdpVideoSurface *next, const VdpRect *src_rect,
    VdpOutputSurface dst, const VdpRect *dst_rect, const VdpRect *dst_v_rect,
    uint32_t layerc, const VdpLayer *layerv)
{
    return vdp->vt.video_mixer_render(mixer, bgsurface, bgrect, pic_struct,
        prev_count, prev, cur, next_count, next, src_rect, dst, dst_rect,
        dst_v_rect, layerc, layerv);
}

/*** Presentation queue ***/
VdpStatus vdp_presentation_queue_target_destroy(const vdp_t *vdp,
    VdpPresentationQueueTarget target)
{
    return vdp->vt.presentation_queue_target_destroy(target);
}

VdpStatus vdp_presentation_queue_create(const vdp_t *vdp, VdpDevice device,
    VdpPresentationQueueTarget target, VdpPresentationQueue *queue)
{
    return vdp->vt.presentation_queue_create(device, target, queue);
}

VdpStatus vdp_presentation_queue_destroy(const vdp_t *vdp,
    VdpPresentationQueue queue)
{
    return vdp->vt.presentation_queue_destroy(queue);
}

VdpStatus vdp_presentation_queue_set_background_color(const vdp_t *vdp,
    VdpPresentationQueue queue, const VdpColor *color)
{
    VdpColor bak = *color;
    return vdp->vt.presentation_queue_set_background_color(queue, &bak);
}

VdpStatus vdp_presentation_queue_get_background_color(const vdp_t *vdp,
    VdpPresentationQueue queue, VdpColor *color)
{
    return vdp->vt.presentation_queue_get_background_color(queue, color);
}

VdpStatus vdp_presentation_queue_get_time(const vdp_t *vdp,
    VdpPresentationQueue queue, VdpTime *current_time)
{
    return vdp->vt.presentation_queue_get_time(queue, current_time);
}

VdpStatus vdp_presentation_queue_display(const vdp_t *vdp,
    VdpPresentationQueue queue, VdpOutputSurface surface, uint32_t clip_width,
    uint32_t clip_height, VdpTime pts)
{
    return vdp->vt.presentation_queue_display(queue, surface, clip_width,
                                              clip_height, pts);
}

VdpStatus vdp_presentation_queue_block_until_surface_idle(const vdp_t *vdp,
    VdpPresentationQueue queue, VdpOutputSurface surface, VdpTime *pts)
{
    return vdp->vt.presentation_queue_block_until_surface_idle(queue, surface,
                                                               pts);
}

VdpStatus vdp_presentation_queue_query_surface_status(const vdp_t *vdp,
    VdpPresentationQueue queue, VdpOutputSurface surface,
    VdpPresentationQueueStatus *status, VdpTime *pts)
{
    return vdp->vt.presentation_queue_query_surface_status(queue, surface,
                                                           status, pts);
}

/*** Preemption ***/
VdpStatus vdp_preemption_callback_register(const vdp_t *vdp, VdpDevice device,
    VdpPreemptionCallback cb, void *ctx)
{
    return vdp->vt.preemption_callback_register(device, cb, ctx);
}

static VdpStatus vdp_fallback(/* UNDEFINED - LEAVE EMPTY */)
{
    return VDP_STATUS_NO_IMPLEMENTATION;
}

/*** X11 & VLC ***/
#include <dlfcn.h>
#include <vdpau/vdpau_x11.h>

VdpStatus vdp_presentation_queue_target_create_x11(const vdp_t *vdp,
    VdpDevice device, uint32_t drawable, VdpPresentationQueueTarget *target)
{
    void *ptr;
    VdpStatus err = vdp_get_proc_address(vdp, device,
                       VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11, &ptr);
    if (err != VDP_STATUS_OK)
        return err;

    VdpPresentationQueueTargetCreateX11 *f = ptr;
    return f(device, drawable, target);
}

VdpStatus vdp_create_x11(void *dpy, int snum,
                         vdp_t **restrict vdpp, VdpDevice *restrict devp)
{
    vdp_t *vdp = malloc(sizeof (*vdp));
    if (unlikely(vdp == NULL))
        return VDP_STATUS_RESOURCES;
    *vdpp = vdp;

    VdpStatus err = VDP_STATUS_NO_IMPLEMENTATION;

    vdp->handle = dlopen("libvdpau.so.1", RTLD_LAZY|RTLD_LOCAL);
    if (vdp->handle == NULL)
    {
        free(vdp);
        return err;
    }

    VdpDeviceCreateX11 *create = dlsym(vdp->handle, "vdp_device_create_x11");
    if (create == NULL)
        goto error;

    VdpGetProcAddress *gpa;
    err = create(dpy, snum, devp, &gpa);
    if (err != VDP_STATUS_OK)
        goto error;

    for (VdpFuncId i = 0; i < sizeof (vdp->vt) / sizeof (void *); i++)
        if (gpa(*devp, i, vdp->funcs + i) != VDP_STATUS_OK)
        {
            void *fallback = vdp_fallback;
            if (unlikely(i == VDP_FUNC_ID_GET_ERROR_STRING))
                fallback = vdp_get_error_string_fallback;
            vdp->funcs[i] = fallback;
        }

    return VDP_STATUS_OK;
error:
    vdp_destroy_x11(vdp);
    return err;
}

void vdp_destroy_x11(vdp_t *vdp)
{
    dlclose(vdp->handle);
    free(vdp);
}
