/*****************************************************************************
 * vlc_vdpau.h: VDPAU helper for VLC
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

#ifndef VLC_VDPAU_H
#define VLC_VDPAU_H

#include <stdint.h>
#include <vdpau/vdpau.h>
#include <vlc_codec.h>

typedef struct vdp_s vdp_t;

const char *vdp_get_error_string(const vdp_t *, VdpStatus);
VdpStatus vdp_get_proc_address(const vdp_t *, VdpDevice, VdpFuncId, void **);
VdpStatus vdp_get_api_version(const vdp_t *, uint32_t *);
VdpStatus vdp_get_information_string(const vdp_t *, const char **);
VdpStatus vdp_device_destroy(const vdp_t *, VdpDevice);
VdpStatus vdp_generate_csc_matrix(const vdp_t *, const VdpProcamp *,
                                  VdpColorStandard, VdpCSCMatrix *);
VdpStatus vdp_video_surface_query_capabilities(const vdp_t *, VdpDevice,
                             VdpChromaType, VdpBool *, uint32_t *, uint32_t *);
VdpStatus vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
    const vdp_t *, VdpDevice, VdpChromaType, VdpYCbCrFormat, VdpBool *);
VdpStatus vdp_video_surface_create(const vdp_t *, VdpDevice, VdpChromaType,
                                   uint32_t, uint32_t, VdpVideoSurface *);
VdpStatus vdp_video_surface_destroy(const vdp_t *, VdpVideoSurface);
VdpStatus vdp_video_surface_get_parameters(const vdp_t *, VdpVideoSurface,
                                      VdpChromaType *, uint32_t *, uint32_t *);
VdpStatus vdp_video_surface_get_bits_y_cb_cr(const vdp_t *, VdpVideoSurface,
                              VdpYCbCrFormat, void *const *, uint32_t const *);
VdpStatus vdp_video_surface_put_bits_y_cb_cr(const vdp_t *, VdpVideoSurface,
                        VdpYCbCrFormat, const void *const *, uint32_t const *);
VdpStatus vdp_output_surface_query_capabilities(const vdp_t *, VdpDevice,
    VdpRGBAFormat, VdpBool *, uint32_t *, uint32_t *);
VdpStatus vdp_output_surface_query_get_put_bits_native_capabilities(
    const vdp_t *, VdpDevice, VdpRGBAFormat, VdpBool *);
VdpStatus vdp_output_surface_query_put_bits_indexed_capabilities(const vdp_t *,
    VdpDevice, VdpRGBAFormat, VdpIndexedFormat, VdpColorTableFormat,
    VdpBool *);
VdpStatus vdp_output_surface_query_put_bits_y_cb_cr_capabilities(const vdp_t *,
    VdpDevice, VdpRGBAFormat, VdpYCbCrFormat, VdpBool *);
VdpStatus vdp_output_surface_create(const vdp_t *, VdpDevice, VdpRGBAFormat,
    uint32_t, uint32_t, VdpOutputSurface *);
VdpStatus vdp_output_surface_destroy(const vdp_t *, VdpOutputSurface);
VdpStatus vdp_output_surface_get_parameters(const vdp_t *, VdpOutputSurface,
    VdpRGBAFormat *, uint32_t *, uint32_t *);
VdpStatus vdp_output_surface_get_bits_native(const vdp_t *, VdpOutputSurface,
    const VdpRect *, void *const *, uint32_t const *);
VdpStatus vdp_output_surface_put_bits_native(const vdp_t *, VdpOutputSurface,
    const void *const *, uint32_t const *, const VdpRect *);
VdpStatus vdp_output_surface_put_bits_indexed(const vdp_t *, VdpOutputSurface,
    VdpIndexedFormat, const void *const *, const uint32_t *, const VdpRect *,
    VdpColorTableFormat, const void *);
VdpStatus vdp_output_surface_put_bits_y_cb_cr(const vdp_t *, VdpOutputSurface,
    VdpYCbCrFormat, const void *const *, const uint32_t *, const VdpRect *,
    const VdpCSCMatrix *);
VdpStatus vdp_bitmap_surface_query_capabilities(const vdp_t *, VdpDevice,
    VdpRGBAFormat, VdpBool *, uint32_t *, uint32_t *);
VdpStatus vdp_bitmap_surface_create(const vdp_t *, VdpDevice, VdpRGBAFormat,
    uint32_t, uint32_t, VdpBool, VdpBitmapSurface *);
VdpStatus vdp_bitmap_surface_destroy(const vdp_t *, VdpBitmapSurface);
VdpStatus vdp_bitmap_surface_get_parameters(const vdp_t *, VdpBitmapSurface,
    VdpRGBAFormat *, uint32_t *, uint32_t *, VdpBool *);
VdpStatus vdp_bitmap_surface_put_bits_native(const vdp_t *, VdpBitmapSurface,
    const void *const *, const uint32_t *, const VdpRect *);
VdpStatus vdp_output_surface_render_output_surface(const vdp_t *,
    VdpOutputSurface, const VdpRect *, VdpOutputSurface, const VdpRect *,
    const VdpColor *, const VdpOutputSurfaceRenderBlendState *const,
    uint32_t);
VdpStatus vdp_output_surface_render_bitmap_surface(const vdp_t *,
    VdpOutputSurface, const VdpRect *, VdpBitmapSurface, const VdpRect *,
    const VdpColor *, const VdpOutputSurfaceRenderBlendState *, uint32_t);
VdpStatus vdp_decoder_query_capabilities(const vdp_t *, VdpDevice,
    VdpDecoderProfile, VdpBool *,
    uint32_t *, uint32_t *, uint32_t *, uint32_t *);
VdpStatus vdp_decoder_create(const vdp_t *, VdpDevice, VdpDecoderProfile,
    uint32_t, uint32_t, uint32_t, VdpDecoder *);
VdpStatus vdp_decoder_destroy(const vdp_t *, VdpDecoder);
VdpStatus vdp_decoder_get_parameters(const vdp_t *, VdpDecoder,
    VdpDecoderProfile *, uint32_t *, uint32_t *);
VdpStatus vdp_decoder_render(const vdp_t *, VdpDecoder, VdpVideoSurface,
    const VdpPictureInfo *, uint32_t, const VdpBitstreamBuffer *);
VdpStatus vdp_video_mixer_query_feature_support(const vdp_t *, VdpDevice,
    VdpVideoMixerFeature, VdpBool *);
VdpStatus vdp_video_mixer_query_parameter_support(const vdp_t *, VdpDevice,
    VdpVideoMixerParameter, VdpBool *);
VdpStatus vdp_video_mixer_query_attribute_support(const vdp_t *, VdpDevice,
    VdpVideoMixerAttribute, VdpBool *);
VdpStatus vdp_video_mixer_query_parameter_value_range(const vdp_t *, VdpDevice,
    VdpVideoMixerParameter, void *, void *);
VdpStatus vdp_video_mixer_query_attribute_value_range(const vdp_t *, VdpDevice,
    VdpVideoMixerAttribute, void *, void *);
VdpStatus vdp_video_mixer_create(const vdp_t *, VdpDevice, uint32_t,
    const VdpVideoMixerFeature *, uint32_t, const VdpVideoMixerParameter *,
    const void *const *, VdpVideoMixer *);
VdpStatus vdp_video_mixer_set_feature_enables(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerFeature *, const VdpBool *);
VdpStatus vdp_video_mixer_set_attribute_values(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerAttribute *const, const void *const *);
VdpStatus vdp_video_mixer_get_feature_support(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerFeature *, VdpBool *);
VdpStatus vdp_video_mixer_get_feature_enables(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerFeature *, VdpBool *);
VdpStatus vdp_video_mixer_get_parameter_values(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerParameter *, void *const *);
VdpStatus vdp_video_mixer_get_attribute_values(const vdp_t *, VdpVideoMixer,
    uint32_t, const VdpVideoMixerAttribute *, void *const *);
VdpStatus vdp_video_mixer_destroy(const vdp_t *, VdpVideoMixer);
VdpStatus vdp_video_mixer_render(const vdp_t *, VdpVideoMixer,
    VdpOutputSurface, const VdpRect *, VdpVideoMixerPictureStructure, uint32_t,
    const VdpVideoSurface *, VdpVideoSurface, uint32_t,
    const VdpVideoSurface *, const VdpRect *, VdpOutputSurface,
    const VdpRect *, const VdpRect *, uint32_t, const VdpLayer *);
VdpStatus vdp_presentation_queue_target_destroy(const vdp_t *,
    VdpPresentationQueueTarget);
VdpStatus vdp_presentation_queue_create(const vdp_t *, VdpDevice,
    VdpPresentationQueueTarget, VdpPresentationQueue *);
VdpStatus vdp_presentation_queue_destroy(const vdp_t *, VdpPresentationQueue);
VdpStatus vdp_presentation_queue_set_background_color(const vdp_t *,
    VdpPresentationQueue, const VdpColor *);
VdpStatus vdp_presentation_queue_get_background_color(const vdp_t *,
    VdpPresentationQueue, VdpColor *);
VdpStatus vdp_presentation_queue_get_time(const vdp_t *,
    VdpPresentationQueue, VdpTime *);
VdpStatus vdp_presentation_queue_display(const vdp_t *, VdpPresentationQueue,
    VdpOutputSurface, uint32_t, uint32_t, VdpTime);
VdpStatus vdp_presentation_queue_block_until_surface_idle(const vdp_t *,
    VdpPresentationQueue, VdpOutputSurface, VdpTime *);
VdpStatus vdp_presentation_queue_query_surface_status(const vdp_t *,
    VdpPresentationQueue, VdpOutputSurface, VdpPresentationQueueStatus *,
    VdpTime *);
VdpStatus vdp_preemption_callback_register(const vdp_t *vdp, VdpDevice,
                                           VdpPreemptionCallback, void *);

VdpStatus vdp_presentation_queue_target_create_x11(const vdp_t *,
                            VdpDevice, uint32_t, VdpPresentationQueueTarget *);

/**
 * Loads a VDPAU instance and creates a VDPAU device using X11.
 * @param dpy X11 display handle (Xlib Display pointer) [IN]
 * @param snum X11 screen number
 * @param vdpp memory location to hold the VDPAU instance pointer [OUT]
 * @param devp memory location to hold the VDPAU device handle [OUT]
 * @note The X11 display connection must remain valid until after the VDPAU
 * device has been destroyed with vdp_device_destroy().
 * @return VDP_STATUS_OK on success, otherwise a VDPAU error code.
 */
VdpStatus vdp_create_x11(void *dpy, int snum, vdp_t **vdpp, VdpDevice *devp);


/**
 * Destroys a VDPAU instance created with vdp_create_x11().
 * @note The corresponding VDPAU device shall have been destroyed with
 * vdp_device_destroy() first.
 */
void vdp_destroy_x11(vdp_t *);

/* Instance reuse */

/**
 * Finds an existing pair of VDPAU instance and VDPAU device matching the
 * specified X11 display and screen number from within the process-wide list.
 * If no existing instance corresponds, connect to the X11 server,
 * create a new pair of instance and device, and set the reference count to 1.
 * @param name X11 display name
 * @param snum X11 screen number
 * @param vdp memory location to hold the VDPAU instance pointer [OUT]
 * @param dev memory location to hold the VDPAU device handle [OUT]
 * @return VDP_STATUS_OK on success, otherwise a VDPAU error code.
 *
 * @note Use vdp_release_x11() to release the instance. <b>Do not use</b>
 * vdp_device_destroy() and/or vdp_destroy_x11() with vdp_get_x11().
 */
VdpStatus vdp_get_x11(const char *name, int num, vdp_t **vdp, VdpDevice *dev);

/**
 * Increases the reference count of a VDPAU instance created by vdp_get_x11().
 * @param vdp VDPAU instance (as returned by vdp_get_x11())
 * @param device location to store the VDPAU device corresponding to the
 *               VDPAU instance (or NULL) [OUT]
 * @return the first pameter, always succeeds.
 */
vdp_t *vdp_hold_x11(vdp_t *vdp, VdpDevice *device);

/**
 * Decreases the reference count of a VDPAU instance created by vdp_get_x11().
 * If it reaches zero, destroy the corresponding VDPAU device, then the VDPAU
 * instance and remove the pair from the process-wide list.
 */
void vdp_release_x11(vdp_t *);

/* VLC specifics */
# include <stdatomic.h>
# include <stdbool.h>
# include <stdint.h>
# include <vlc_common.h>
# include <vlc_fourcc.h>
# include <vlc_picture.h>

/** Converts VLC YUV format to VDPAU chroma type and YCbCr format */
static inline
bool vlc_fourcc_to_vdp_ycc(vlc_fourcc_t fourcc,
                 VdpChromaType *restrict type, VdpYCbCrFormat *restrict format)
{
    switch (fourcc)
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_YV12:
            *type = VDP_CHROMA_TYPE_420;
            *format = VDP_YCBCR_FORMAT_YV12;
            break;
        case VLC_CODEC_NV12:
            *type = VDP_CHROMA_TYPE_420;
            *format = VDP_YCBCR_FORMAT_NV12;
            break;
        case VLC_CODEC_I422:
            *type = VDP_CHROMA_TYPE_422;
            *format = VDP_YCBCR_FORMAT_YV12;
            break;
        case VLC_CODEC_NV16:
            *type = VDP_CHROMA_TYPE_422;
            *format = VDP_YCBCR_FORMAT_NV12;
            break;
        case VLC_CODEC_YUYV:
            *type = VDP_CHROMA_TYPE_422;
            *format = VDP_YCBCR_FORMAT_YUYV;
            break;
        case VLC_CODEC_UYVY:
            *type = VDP_CHROMA_TYPE_422;
            *format = VDP_YCBCR_FORMAT_UYVY;
            break;
        case VLC_CODEC_I444:
            *type = VDP_CHROMA_TYPE_444;
            *format = VDP_YCBCR_FORMAT_YV12;
            break;
        case VLC_CODEC_NV24:
            *type = VDP_CHROMA_TYPE_444;
            *format = VDP_YCBCR_FORMAT_NV12;
            break;
        default:
            return false;
    }
    return true;
}

typedef struct vlc_vdp_video_frame
{
    VdpVideoSurface surface;
    VdpDevice device;
    vdp_t *vdp;
    atomic_uintptr_t refs;
} vlc_vdp_video_frame_t;

typedef struct vlc_vdp_video_field
{
    picture_context_t context;
    vlc_vdp_video_frame_t *frame;
    VdpVideoMixerPictureStructure structure;
    VdpProcamp procamp;
    float sharpen;
} vlc_vdp_video_field_t;

#define VDPAU_FIELD_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), vlc_vdp_video_field_t, context)

typedef struct {
    vdp_t              *vdp;
    VdpDevice          device;
} vdpau_decoder_device_t;

static inline vdpau_decoder_device_t *GetVDPAUOpaqueDevice(vlc_decoder_device *device)
{
    if (device == NULL || device->type != VLC_DECODER_DEVICE_VDPAU)
        return NULL;
    return device->opaque;
}

static inline vdpau_decoder_device_t *GetVDPAUOpaqueContext(vlc_video_context *vctx)
{
    vlc_decoder_device *device = vctx ? vlc_video_context_HoldDevice(vctx) : NULL;
    if (unlikely(device == NULL))
        return NULL;
    vdpau_decoder_device_t *res = NULL;
    if (device->type == VLC_DECODER_DEVICE_VDPAU)
    {
        assert(device->opaque != NULL);
        res = GetVDPAUOpaqueDevice(device);
    }
    vlc_decoder_device_Release(device);
    return res;
}

/**
 * Attaches a VDPAU video surface as context of a VLC picture.
 */
VdpStatus vlc_vdp_video_attach(vdp_t *, VdpVideoSurface, vlc_video_context *, picture_t *);

/**
 * Wraps a VDPAU video surface into a VLC picture context.
 */
vlc_vdp_video_field_t *vlc_vdp_video_create(vdp_t *, VdpVideoSurface);

static inline void vlc_vdp_video_destroy(vlc_vdp_video_field_t *f)
{
    f->context.destroy(&f->context);
}

/**
 * Performs a shallow copy of a VDPAU video surface context
 * (the underlying VDPAU video surface is shared).
 */
static inline vlc_vdp_video_field_t *vlc_vdp_video_copy(
    vlc_vdp_video_field_t *fold)
{
    return VDPAU_FIELD_FROM_PICCTX(fold->context.copy(&fold->context));
}

/**
 * Clone a VPD field based picture context that contains a video context
 */
picture_context_t *VideoSurfaceCloneWithContext(picture_context_t *);

typedef struct vlc_vdp_output_surface
{
    VdpOutputSurface surface;
    VdpDevice device;
    vdp_t *vdp;
    ptrdiff_t gl_nv_surface;
} vlc_vdp_output_surface_t;

struct picture_pool_t;

struct picture_pool_t *vlc_vdp_output_pool_create(vdpau_decoder_device_t *,
                                                  VdpRGBAFormat,
                                                  const video_format_t *,
                                                  unsigned count);

#endif
