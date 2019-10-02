/*****************************************************************************
 * chroma.c: NVDEC/CUDA chroma conversion filter
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_codec.h>

#include "nvdec_fmt.h"

static int OpenCUDAToCPU( vlc_object_t * );

vlc_module_begin()
    set_shortname(N_("CUDA converter"))
    set_description(N_("CUDA/NVDEC Chroma Converter filter"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video converter", 10)
    set_callbacks(OpenCUDAToCPU, NULL)
vlc_module_end()

typedef struct
{
    vlc_decoder_device *device;
} nvdec_filter_sys_t;

#define CALL_CUDA(func, ...) CudaCheckErr(VLC_OBJECT(p_filter), devsys->cudaFunctions, devsys->cudaFunctions->func(__VA_ARGS__), #func)


static picture_t * FilterCUDAToCPU( filter_t *p_filter, picture_t *src )
{
    picture_t *dst = filter_NewPicture( p_filter );
    if (unlikely(dst == NULL))
    {
        picture_Release(src);
        return NULL;
    }

    pic_context_nvdec_t *srcpic = container_of(src->context, pic_context_nvdec_t, ctx);
    decoder_device_nvdec_t *devsys = &srcpic->nvdecDevice;

    int result;
    result = CALL_CUDA(cuCtxPushCurrent, devsys->cuCtx);
    if (result != VLC_SUCCESS)
    {
        picture_Release(dst);
        picture_Release(src);
        return NULL;
    }

    size_t srcY = 0;
    for (int i_plane = 0; i_plane < dst->i_planes; i_plane++) {
        plane_t plane = dst->p[i_plane];
        CUDA_MEMCPY2D cu_cpy = {
            .srcMemoryType  = CU_MEMORYTYPE_DEVICE,
            .srcDevice      = srcpic->devidePtr,
            .srcY           = srcY,
            .srcPitch       = srcpic->bufferPitch,
            .dstMemoryType  = CU_MEMORYTYPE_HOST,
            .dstHost        = plane.p_pixels,
            .dstPitch       = plane.i_pitch,
            .WidthInBytes   = __MIN(srcpic->bufferPitch, (unsigned)dst->p[0].i_pitch),
            .Height         = __MIN(srcpic->bufferHeight, (unsigned)plane.i_visible_lines),
        };
        result = CALL_CUDA(cuMemcpy2DAsync, &cu_cpy, 0);
        if (result != VLC_SUCCESS)
        {
            picture_Release(dst);
            dst = NULL;
            goto done;
        }
        srcY += srcpic->bufferHeight;
    }
    picture_CopyProperties(dst, src);

done:
    CALL_CUDA(cuCtxPopCurrent, NULL);
    picture_Release(src);
    return dst;
}

static int OpenCUDAToCPU( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if ( !( ( p_filter->fmt_in.video.i_chroma  == VLC_CODEC_NVDEC_OPAQUE &&
              p_filter->fmt_out.video.i_chroma == VLC_CODEC_NV12 ) ||
            ( p_filter->fmt_in.video.i_chroma  == VLC_CODEC_NVDEC_OPAQUE_10B &&
              p_filter->fmt_out.video.i_chroma == VLC_CODEC_P010 ) ||
            ( p_filter->fmt_in.video.i_chroma  == VLC_CODEC_NVDEC_OPAQUE_16B &&
              p_filter->fmt_out.video.i_chroma == VLC_CODEC_P016 )
           ) )
        return VLC_EGENERIC;

    p_filter->pf_video_filter = FilterCUDAToCPU;

    return VLC_SUCCESS;
}
