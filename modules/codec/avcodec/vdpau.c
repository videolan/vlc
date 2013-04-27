/*****************************************************************************
 * vdpau.c: VDPAU decoder for libav
 *****************************************************************************
 * Copyright (C) 2012-2013 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#include <vdpau/vdpau.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>

#include <X11/Xlib.h>
#include <vdpau/vdpau_x11.h>
#include <vlc_xlib.h>

#include "avcodec.h"
#include "va.h"

static int Open (vlc_va_t *, int, const es_format_t *);
static void Close (vlc_va_t *);

vlc_module_begin ()
    set_description (N_("Video Decode and Presentation API for Unix (VDPAU)"))
    set_capability ("hw decoder", 100)
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_VCODEC)
    set_callbacks (Open, Close)
vlc_module_end ()

#define MAX_SURFACES (16+1)

struct vlc_va_sys_t
{
    VdpDevice device;
    VdpDecoderProfile profile;
    AVVDPAUContext context;
    VdpVideoSurface surfaces[MAX_SURFACES];
    uint32_t available;
    uint16_t width;
    uint16_t height;
    void *display;

    VdpGetErrorString *GetErrorString;
    VdpGetInformationString *GetInformationString;
    VdpDeviceDestroy *DeviceDestroy;
    VdpVideoSurfaceQueryCapabilities *VideoSurfaceQueryCapabilities;
    VdpVideoSurfaceQueryGetPutBitsYCbCrCapabilities
                                 *VideoSurfaceQueryGetPutBitsYCbCrCapabilities;
    VdpVideoSurfaceCreate *VideoSurfaceCreate;
    VdpVideoSurfaceDestroy *VideoSurfaceDestroy;
    VdpVideoSurfaceGetBitsYCbCr *VideoSurfaceGetBitsYCbCr;
    VdpDecoderCreate *DecoderCreate;
    VdpDecoderDestroy *DecoderDestroy;
    VdpDecoderRender *DecoderRender;
};

static int Lock (vlc_va_t *va, AVFrame *ff)
{
    vlc_va_sys_t *sys = va->sys;

    for (unsigned i = 0; i < AV_NUM_DATA_POINTERS; i++)
    {
        ff->data[i] = NULL;
        ff->linesize[i] = 0;
    }

    if (!sys->available)
    {
        msg_Err (va, "no free surfaces");
        return VLC_EGENERIC;
    }

    unsigned idx = ctz (sys->available);
    sys->available &= ~(1 << idx);

    VdpVideoSurface *surface = sys->surfaces + idx;
    assert (*surface != VDP_INVALID_HANDLE);
    ff->data[0] = (void *)surface; /* must be non-NULL */
    ff->data[3] = (void *)(uintptr_t)*surface;
    ff->opaque = surface;
    return VLC_SUCCESS;
}

static void Unlock (vlc_va_t *va, AVFrame *ff)
{
    vlc_va_sys_t *sys = va->sys;
    VdpVideoSurface *surface = ff->opaque;
    unsigned idx = surface - sys->surfaces;

    assert (idx < MAX_SURFACES);
    sys->available |= (1 << idx);

    ff->data[0] = ff->data[3] = NULL;
    ff->opaque = NULL;
}

static int Copy (vlc_va_t *va, picture_t *pic, AVFrame *ff)
{
    vlc_va_sys_t *sys = va->sys;
    VdpVideoSurface *surface = ff->opaque;
    void *planes[3];
    uint32_t pitches[3];
    VdpStatus err;

    for (unsigned i = 0; i < 3; i++)
    {
         planes[i] = pic->p[i].p_pixels;
         pitches[i] = pic->p[i].i_pitch;
    }

    err = sys->VideoSurfaceGetBitsYCbCr (*surface, VDP_YCBCR_FORMAT_YV12,
                                         planes, pitches);
    if (err != VDP_STATUS_OK)
    {
        msg_Err (va, "surface copy failure: %s", sys->GetErrorString (err));
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Init (vlc_va_t *va, void **ctxp, vlc_fourcc_t *chromap,
                 int width, int height)
{
    vlc_va_sys_t *sys = va->sys;
    VdpStatus err;

    width = (width + 1) & ~1;
    height = (height + 3) & ~3;

    unsigned surfaces = 2;
    if (sys->profile == VDP_DECODER_PROFILE_H264_HIGH)
        surfaces = 16;

    err = sys->DecoderCreate (sys->device, sys->profile, width, height,
                              surfaces, &sys->context.decoder);
    if (err != VDP_STATUS_OK)
    {
        msg_Err (va, "decoder creation failure: %s",
                 sys->GetErrorString (err));
        sys->context.decoder = VDP_INVALID_HANDLE;
        return VLC_EGENERIC;
    }

    assert (width > 0 && height > 0);
    surfaces++;
    assert (surfaces <= MAX_SURFACES);
    sys->available = 0;

    /* TODO: select better chromas when appropriate */
    for (unsigned i = 0; i < surfaces; i++)
    {
        err = sys->VideoSurfaceCreate (sys->device, VDP_CHROMA_TYPE_420, width,
                                       height, sys->surfaces + i);
        if (err != VDP_STATUS_OK)
        {
             msg_Err (va, "surface creation failure: %s",
                      sys->GetErrorString (err));
             sys->surfaces[i] = VDP_INVALID_HANDLE;
             break;
        }
        sys->available |= (1 << i);
    }

    *ctxp = &sys->context;
    *chromap = VLC_CODEC_YV12;
    return VLC_SUCCESS;
}

static void Deinit (vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    for (unsigned i = 0; i < MAX_SURFACES; i++)
    {
        VdpVideoSurface *surface = sys->surfaces + i;

        if (*surface != VDP_INVALID_HANDLE)
        {
            sys->VideoSurfaceDestroy (*surface);
            *surface = VDP_INVALID_HANDLE;
        }
    }

    assert (sys->context.decoder != VDP_INVALID_HANDLE);
    sys->DecoderDestroy (sys->context.decoder);
    av_freep (&sys->context.bitstream_buffers);
}

static int Setup (vlc_va_t *va, void **ctxp, vlc_fourcc_t *chromap,
                 int width, int height)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->context.decoder != VDP_INVALID_HANDLE)
    {
        if (sys->width == width && sys->height == height)
            return VLC_SUCCESS;
        Deinit (va);
        sys->context.decoder = VDP_INVALID_HANDLE;
    }

    sys->width = width;
    sys->height = height;
    return Init (va, ctxp, chromap, width, height);
}

static int vdp_device_Create (vlc_object_t *obj, void **sysp, VdpDevice *devp,
                              VdpGetProcAddress **gpap)
{
    VdpStatus err;

    if (!vlc_xlib_init (obj))
    {
        msg_Err (obj, "Xlib is required for VDPAU");
        return VLC_EGENERIC;
    }

    Display *x11 = XOpenDisplay (NULL);
    if (x11 == NULL)
    {
        msg_Err (obj, "windowing system failure failure");
        return VLC_EGENERIC;
    }

    err = vdp_device_create_x11 (x11, XDefaultScreen (x11), devp, gpap);
    if (err)
    {
        msg_Err (obj, "device creation failure: error %d", (int)err);
        XCloseDisplay (x11);
        return VLC_EGENERIC;
    }
    *sysp = x11;
    return VLC_SUCCESS;
}

static int Open (vlc_va_t *va, int codec, const es_format_t *fmt)
{
    VdpStatus err;
    int profile;

    switch (codec)
    {
        case AV_CODEC_ID_MPEG1VIDEO:
            profile = VDP_DECODER_PROFILE_MPEG1;
            break;
        case AV_CODEC_ID_MPEG2VIDEO:
            profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
            break;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_MPEG4:
            profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
            break;
        case AV_CODEC_ID_H264:
            profile = VDP_DECODER_PROFILE_H264_HIGH;
            break;
        case AV_CODEC_ID_WMV3:
        case AV_CODEC_ID_VC1:
            profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
            break;
        default:
            msg_Err (va, "unknown codec (%d)", codec);
            return VLC_EGENERIC;
    }

    vlc_va_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    VdpDevice device;
    VdpGetProcAddress *GetProcAddress;
    if (vdp_device_Create (VLC_OBJECT(va), &sys->display, &device,
                           &GetProcAddress))
    {
        free (sys);
        return VLC_EGENERIC;
    }

    sys->device = device;
    sys->profile = profile;
    memset (&sys->context, 0, sizeof (sys->context));
    sys->context.decoder = VDP_INVALID_HANDLE;
    for (unsigned i = 0; i < MAX_SURFACES; i++)
        sys->surfaces[i] = VDP_INVALID_HANDLE;

#define PROC(id,name) \
    do { \
        void *ptr; \
        err = GetProcAddress (device, VDP_FUNC_ID_##id, &ptr); \
        if (unlikely(err)) \
            abort (); \
        sys->name = ptr; \
    } while (0)

    /* We are really screwed if any function fails. We cannot even delete the
     * already allocated device. */
    PROC(GET_ERROR_STRING, GetErrorString);
    PROC(GET_INFORMATION_STRING, GetInformationString);
    PROC(DEVICE_DESTROY, DeviceDestroy);
    /* NOTE: We do not really need to retain QueryCap pointers in *sys */
    PROC(VIDEO_SURFACE_QUERY_CAPABILITIES, VideoSurfaceQueryCapabilities);
    PROC(VIDEO_SURFACE_QUERY_GET_PUT_BITS_Y_CB_CR_CAPABILITIES,
                                 VideoSurfaceQueryGetPutBitsYCbCrCapabilities);
    PROC(VIDEO_SURFACE_CREATE, VideoSurfaceCreate);
    PROC(VIDEO_SURFACE_DESTROY, VideoSurfaceDestroy);
    PROC(VIDEO_SURFACE_GET_BITS_Y_CB_CR, VideoSurfaceGetBitsYCbCr);
    PROC(DECODER_CREATE, DecoderCreate);
    PROC(DECODER_DESTROY, DecoderDestroy);
    PROC(DECODER_RENDER, DecoderRender);
    sys->context.render = sys->DecoderRender;

    /* Check capabilities */
    VdpBool support;
    uint32_t width, height;

    if (sys->VideoSurfaceQueryCapabilities (device, VDP_CHROMA_TYPE_420,
                                   &support, &width, &height) != VDP_STATUS_OK)
        support = VDP_FALSE;
    if (!support || width < fmt->video.i_width || height < fmt->video.i_height)
    {
        msg_Err (va, "video surface not supported: %s %ux%u",
                 "YUV 4:2:0", fmt->video.i_width, fmt->video.i_height);
        goto error;
    }
    msg_Dbg (va, "video surface supported maximum: %s %"PRIu32"x%"PRIu32,
                 "YUV 4:2:0", width, height);

    if (sys->VideoSurfaceQueryGetPutBitsYCbCrCapabilities (device,
        VDP_CHROMA_TYPE_420, VDP_YCBCR_FORMAT_YV12, &support) != VDP_STATUS_OK)
        support = VDP_FALSE;
    if (!support)
    {
        msg_Err (va, "video surface reading not supported: %s as %s",
                 "YUV 4:2:0", "YV12");
        goto error;
    }

    /* TODO: check decoder */

    const char *infos;
    if (sys->GetInformationString (&infos) != VDP_STATUS_OK)
        infos = "VDPAU";

    va->sys = sys;
    va->description = (char *)infos;
    va->pix_fmt = AV_PIX_FMT_VDPAU;
    va->setup = Setup;
    va->get = Lock;
    va->release = Unlock;
    va->extract = Copy;
    return VLC_SUCCESS;

error:
    sys->DeviceDestroy (device);
    XCloseDisplay (sys->display);
    free (sys);
    return VLC_EGENERIC;
}

static void Close (vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->context.decoder != VDP_INVALID_HANDLE)
        Deinit (va);
    sys->DeviceDestroy (sys->device);
    XCloseDisplay (sys->display);
    free (sys);
}
