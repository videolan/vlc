/*****************************************************************************
 * converter_nvdec.c: OpenGL NVDEC opaque converter
 *****************************************************************************
 * Copyright (C) 2019 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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

#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_codec.h>
#include <vlc_plugin.h>

#include <ffnvcodec/dynlink_loader.h>

#include "nvdec_fmt.h"

#include "../../video_output/opengl/internal.h"
#include "../../video_output/opengl/interop.h"

// glew.h conflicts with glext.h, but also makes glext.h unnecessary.
#ifndef __GLEW_H__
#  include <GL/glext.h>
#endif

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description("NVDEC OpenGL surface converter")
    set_capability("glinterop", 2)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("nvdec")
vlc_module_end ()

typedef struct {
    vlc_decoder_device *device;
    CUcontext cuConverterCtx;
    CUgraphicsResource cu_res[PICTURE_PLANE_MAX]; // Y, UV for NV12/P010
    CUarray mappedArray[PICTURE_PLANE_MAX];
} converter_sys_t;

#define CALL_CUDA(func, ...) CudaCheckErr(VLC_OBJECT(interop->gl), devsys->cudaFunctions, devsys->cudaFunctions->func(__VA_ARGS__), #func)

static int tc_nvdec_gl_allocate_texture(const struct vlc_gl_interop *interop, GLuint *textures,
                                const GLsizei *tex_width, const GLsizei *tex_height)
{
    converter_sys_t *p_sys = interop->priv;
    vlc_decoder_device *device = p_sys->device;
    decoder_device_nvdec_t *devsys = GetNVDECOpaqueDevice(device);

    int result;
    result = CALL_CUDA(cuCtxPushCurrent, p_sys->cuConverterCtx ? p_sys->cuConverterCtx : devsys->cuCtx);
    if (result != VLC_SUCCESS)
        return result;

    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        interop->vt->BindTexture(interop->tex_target, textures[i]);
        interop->vt->TexImage2D(interop->tex_target, 0, interop->texs[i].internal,
                           tex_width[i], tex_height[i], 0, interop->texs[i].format,
                           interop->texs[i].type, NULL);
        if (interop->vt->GetError() != GL_NO_ERROR)
        {
            msg_Err(interop->gl, "could not alloc PBO buffers");
            return VLC_EGENERIC;
        }

        result = CALL_CUDA(cuGraphicsGLRegisterImage, &p_sys->cu_res[i], textures[i], interop->tex_target, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);

        result = CALL_CUDA(cuGraphicsMapResources, 1, &p_sys->cu_res[i], 0);
        result = CALL_CUDA(cuGraphicsSubResourceGetMappedArray, &p_sys->mappedArray[i], p_sys->cu_res[i], 0, 0);
        result = CALL_CUDA(cuGraphicsUnmapResources, 1, &p_sys->cu_res[i], 0);

        interop->vt->BindTexture(interop->tex_target, 0);
    }

    CALL_CUDA(cuCtxPopCurrent, NULL);
    return result;
}

static int
tc_nvdec_gl_update(const struct vlc_gl_interop *interop, GLuint textures[],
                   GLsizei const tex_widths[], GLsizei const tex_heights[],
                   picture_t *pic, size_t const plane_offsets[])
{
    VLC_UNUSED(plane_offsets);
    VLC_UNUSED(textures);

    converter_sys_t *p_sys = interop->priv;
    vlc_decoder_device *device = p_sys->device;
    decoder_device_nvdec_t *devsys = GetNVDECOpaqueDevice(device);
    pic_context_nvdec_t *srcpic = container_of(pic->context, pic_context_nvdec_t, ctx);

    int result;
    result = CALL_CUDA(cuCtxPushCurrent, p_sys->cuConverterCtx ? p_sys->cuConverterCtx : devsys->cuCtx);
    if (result != VLC_SUCCESS)
        return result;

    // copy the planes from the pic context to mappedArray
    size_t srcY = 0;
    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        CUDA_MEMCPY2D cu_cpy = {
            .srcMemoryType  = CU_MEMORYTYPE_DEVICE,
            .srcDevice      = srcpic->devicePtr,
            .srcPitch       = srcpic->bufferPitch,
            .srcY           = srcY,
            .dstMemoryType = CU_MEMORYTYPE_ARRAY,
            .dstArray = p_sys->mappedArray[i],
            .WidthInBytes = tex_widths[0],
            .Height = tex_heights[i],
        };
        if (interop->fmt_in.i_chroma != VLC_CODEC_NVDEC_OPAQUE && interop->fmt_in.i_chroma != VLC_CODEC_NVDEC_OPAQUE_444)
            cu_cpy.WidthInBytes *= 2;
        result = CALL_CUDA(cuMemcpy2DAsync, &cu_cpy, 0);
        if (result != VLC_SUCCESS)
            goto error;
        srcY += srcpic->bufferHeight;
    }

error:
    CALL_CUDA(cuCtxPopCurrent, NULL);
    return result;
}

static void Close(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *)obj;
    converter_sys_t *p_sys = interop->priv;
    vlc_decoder_device_Release(p_sys->device);
}

static int Open(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;
    if (!is_nvdec_opaque(interop->fmt_in.i_chroma))
        return VLC_EGENERIC;

    vlc_decoder_device *device = vlc_video_context_HoldDevice(interop->vctx);
    if (device == NULL || device->type != VLC_DECODER_DEVICE_NVDEC)
        return VLC_EGENERIC;

    converter_sys_t *p_sys = vlc_obj_malloc(VLC_OBJECT(interop), sizeof(*p_sys));
    if (unlikely(p_sys == NULL))
    {
        vlc_decoder_device_Release(device);
        return VLC_ENOMEM;
    }
    for (size_t i=0; i < ARRAY_SIZE(p_sys->cu_res); i++)
        p_sys->cu_res[i] = NULL;
    p_sys->cuConverterCtx = NULL;
    p_sys->device = device;

    decoder_device_nvdec_t *devsys = GetNVDECOpaqueDevice(device);
    int result;
    CUdevice cuDecDevice = 0;
    unsigned int device_count;
    result = CALL_CUDA(cuGLGetDevices, &device_count, &cuDecDevice, 1, CU_GL_DEVICE_LIST_ALL);
    if (result < 0)
    {
        vlc_decoder_device_Release(device);
        return result;
    }

    CUdevice cuConverterDevice;
    CALL_CUDA(cuCtxPushCurrent, devsys->cuCtx);
    result = CALL_CUDA(cuCtxGetDevice, &cuConverterDevice);
    CALL_CUDA(cuCtxPopCurrent, NULL);

    if (cuConverterDevice != cuDecDevice)
    {
        result = CALL_CUDA(cuCtxCreate, &p_sys->cuConverterCtx, 0, cuConverterDevice);
        if (result != VLC_SUCCESS)
        {
        }
    }

    vlc_fourcc_t render_chroma;
    switch (interop->fmt_in.i_chroma)
    {
        case VLC_CODEC_NVDEC_OPAQUE_10B: render_chroma = VLC_CODEC_P010; break;
        case VLC_CODEC_NVDEC_OPAQUE_16B: render_chroma = VLC_CODEC_P016; break;
        case VLC_CODEC_NVDEC_OPAQUE_444:     render_chroma = VLC_CODEC_I444; break;
        case VLC_CODEC_NVDEC_OPAQUE_444_16B: render_chroma = VLC_CODEC_I444_16L; break;
        case VLC_CODEC_NVDEC_OPAQUE:
        default:                         render_chroma = VLC_CODEC_NV12; break;
    }

    int ret = opengl_interop_init(interop, GL_TEXTURE_2D, render_chroma, interop->fmt_in.space);
    if (ret != VLC_SUCCESS)
    {
        vlc_decoder_device_Release(device);
        return ret;
    }

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = tc_nvdec_gl_allocate_texture,
        .update_textures = tc_nvdec_gl_update,
    };
    interop->ops = &ops;
    interop->priv = p_sys;

    return VLC_SUCCESS;
}
