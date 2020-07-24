/*****************************************************************************
 * nvdec.c: NVDEC hw video decoder
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Jai Luthra <me@jailuthra.in>
 *          Steve Lhomme <robux4@videolabs.io>
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
#include <vlc_codec.h>
#include <vlc_messages.h>
#include <vlc_picture_pool.h>
#include <vlc_atomic.h>

#define FFNV_LOG_FUNC(logctx, msg, ...)        msg_Err((vlc_object_t*)logctx, msg, __VA_ARGS__)
#define FFNV_DEBUG_LOG_FUNC(logctx, msg, ...)  msg_Dbg((vlc_object_t*)logctx, msg, __VA_ARGS__)

#include <ffnvcodec/dynlink_loader.h>
#include "../../codec/hxxx_helper.h"
#include "nvdec_fmt.h"

static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
static int DecoderContextOpen(vlc_decoder_device *, vout_window_t *);

#define DEINTERLACE_MODULE_TEXT N_("Integrated deinterlacing")
#define DEINTERLACE_MODULE_LONGTEXT N_( "Specify the deinterlace mode to use." )

static const char *const ppsz_deinterlace_type[] =
{
    N_("Disable"), N_("Bob"), N_("Adaptive")
};

static const int ppsi_deinterlace_type[] = {
    cudaVideoDeinterlaceMode_Weave,
    cudaVideoDeinterlaceMode_Bob,
    cudaVideoDeinterlaceMode_Adaptive,
};

vlc_module_begin ()
    set_description(N_("NVDEC video decoder"))
    set_shortname("nvdec")
    set_capability("video decoder", 60)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    add_integer( "nvdec-deint", cudaVideoDeinterlaceMode_Bob,
                 DEINTERLACE_MODULE_TEXT, DEINTERLACE_MODULE_LONGTEXT, true )
        change_integer_list( ppsi_deinterlace_type, ppsz_deinterlace_type )
    set_callbacks(OpenDecoder, CloseDecoder)
    add_submodule()
        set_callback_dec_device(DecoderContextOpen, 3)
        add_shortcut("nvdec")
vlc_module_end ()

/* */
#define MAX_HXXX_SURFACES (16 + 1)
#define NVDEC_DISPLAY_SURFACES 1
#define MAX_POOL_SIZE     4 // number of in-flight buffers, if more are needed the decoder waits

#define OUTPUT_WIDTH_ALIGN   16

typedef struct nvdec_pool_t {
    vlc_video_context           *vctx;
    decoder_device_nvdec_t      *nvdec_dev;

    CUdeviceptr                 outputDevicePtr[MAX_POOL_SIZE];
    picture_pool_t              *picture_pool;

    vlc_atomic_rc_t             rc;
} nvdec_pool_t;

typedef struct pic_pool_context_nvdec_t {
  pic_context_nvdec_t ctx;
  nvdec_pool_t        *pool;
} pic_pool_context_nvdec_t;

typedef struct nvdec_ctx {
    decoder_device_nvdec_t      *devsys;
    CuvidFunctions              *cuvidFunctions;
    CUVIDDECODECAPS             selectedDecoder;
    CUvideodecoder              cudecoder;
    CUvideoparser               cuparser;
    union {
        struct hxxx_helper      hh;
        int                     vc1_header_offset;
    };
    bool                        b_is_hxxx;
    int                         i_nb_surface; ///<  number of GPU surfaces allocated
    bool                        b_xps_pushed; ///< (for xvcC) parameter sets pushed (SPS/PPS/VPS)
    block_t *                   (*process_block)(decoder_t *, block_t *);
    cudaVideoDeinterlaceMode    deintMode;
    // NVDEC doesn't stop even if HandleVideoSequence fails
    bool                        b_nvparser_success;
    size_t                      decoderHeight;

    unsigned int                outputPitch;
    nvdec_pool_t                *out_pool;

    vlc_video_context           *vctx_out;
} nvdec_ctx_t;

#define CALL_CUDA_DEC(func, ...) CudaCheckErr(VLC_OBJECT(p_dec),  p_sys->devsys->cudaFunctions, p_sys->devsys->cudaFunctions->func(__VA_ARGS__), #func)
#define CALL_CUDA_DEV(func, ...) CudaCheckErr(VLC_OBJECT(device), p_sys->cudaFunctions, p_sys->cudaFunctions->func(__VA_ARGS__), #func)
#define CALL_CUVID(func, ...)    CudaCheckErr(VLC_OBJECT(p_dec),  p_sys->devsys->cudaFunctions, p_sys->cuvidFunctions->func(__VA_ARGS__), #func)
#define CALL_CUDA_POOL(func, ...) pool->nvdec_dev->cudaFunctions->func(__VA_ARGS__)

#define NVDEC_PICPOOLCTX_FROM_PICCTX(pic_ctx)  \
    container_of(NVDEC_PICCONTEXT_FROM_PICCTX(pic_ctx), pic_pool_context_nvdec_t, ctx)

static void nvdec_pool_Destroy(nvdec_pool_t *pool)
{
    for (size_t i=0; i < ARRAY_SIZE(pool->outputDevicePtr); i++)
        CALL_CUDA_POOL(cuMemFree, pool->outputDevicePtr[i]);

    picture_pool_Release(pool->picture_pool);
    vlc_video_context_Release(pool->vctx);
}

static void nvdec_pool_AddRef(nvdec_pool_t *pool)
{
    vlc_atomic_rc_inc(&pool->rc);
}

static void nvdec_pool_Release(nvdec_pool_t *pool)
{
    if (!vlc_atomic_rc_dec(&pool->rc))
        return;

    nvdec_pool_Destroy(pool);
}

static nvdec_pool_t* nvdec_pool_Create(vlc_video_context *vctx,
                                       const video_format_t *fmt,
                                       size_t buffer_pitch,
                                       size_t buffer_height)
{
    vlc_decoder_device *dec_dev = NULL;
    nvdec_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool)
        goto error;

    dec_dev = vlc_video_context_HoldDevice(vctx);
    if (dec_dev == NULL)
        goto error;

    pool->nvdec_dev = GetNVDECOpaqueDevice(dec_dev);
    assert(pool->nvdec_dev != NULL);

    int ret = CALL_CUDA_POOL(cuCtxPushCurrent, pool->nvdec_dev->cuCtx);
    if (ret != CUDA_SUCCESS)
        goto error;

    picture_t *pics[ARRAY_SIZE(pool->outputDevicePtr)] = {0};
    for (size_t i=0; i < ARRAY_SIZE(pool->outputDevicePtr); i++)
    {
        ret = CALL_CUDA_POOL(cuMemAlloc,
                             &pool->outputDevicePtr[i],
                             buffer_pitch * buffer_height);
        if (ret != CUDA_SUCCESS || pool->outputDevicePtr[i] == 0)
            goto free_pool;

        picture_resource_t res = {
            .p_sys = pool->outputDevicePtr[i],
        };
        pics[i] = picture_NewFromResource(fmt, &res);
        if (!pics[i])
            goto free_pool;
    }

    pool->picture_pool = picture_pool_New(ARRAY_SIZE(pool->outputDevicePtr), pics);
    if (!pool->picture_pool)
        goto free_pool;

    CALL_CUDA_POOL(cuCtxPopCurrent, NULL);
    vlc_decoder_device_Release(dec_dev);

    pool->vctx = vctx;
    vlc_video_context_Hold(pool->vctx);

    vlc_atomic_rc_init(&pool->rc);
    return pool;

free_pool:
    for (size_t i=0; i < ARRAY_SIZE(pool->outputDevicePtr); i++)
    {
        if (pool->outputDevicePtr[i] != 0)
            CALL_CUDA_POOL(cuMemFree, pool->outputDevicePtr[i]);
        if (pics[i] != NULL)
            picture_Release(pics[i]);
    }
    CALL_CUDA_POOL(cuCtxPopCurrent, NULL);
error:
    if (dec_dev)
        vlc_decoder_device_Release(dec_dev);
    if (pool)
        free(pool);
    return NULL;
}

static void nvdec_picture_CtxDestroy(struct picture_context_t *picctx)
{
    pic_pool_context_nvdec_t *srcpic = NVDEC_PICPOOLCTX_FROM_PICCTX(picctx);
    nvdec_pool_Release(srcpic->pool);
    free(srcpic);
}

static struct picture_context_t *nvdec_picture_CtxClone(struct picture_context_t *srcctx)
{
    pic_pool_context_nvdec_t *clonectx = malloc(sizeof(*clonectx));
    if (unlikely(clonectx == NULL))
        return NULL;
    pic_pool_context_nvdec_t *srcpic = NVDEC_PICPOOLCTX_FROM_PICCTX(srcctx);

    *clonectx = *srcpic;
    vlc_video_context_Hold(clonectx->ctx.ctx.vctx);
    nvdec_pool_AddRef(clonectx->pool);
    return &clonectx->ctx.ctx;
}

static picture_t* nvdec_pool_Wait(nvdec_pool_t *pool)
{
    picture_t *pic = picture_pool_Wait(pool->picture_pool);
    if (!pic)
        return NULL;

    pic_pool_context_nvdec_t *picctx = malloc(sizeof(*picctx));
    if (!picctx)
        goto error;

    picctx->ctx.ctx = (picture_context_t) {
        nvdec_picture_CtxDestroy,
        nvdec_picture_CtxClone,
        pool->vctx,
    };
    vlc_video_context_Hold(picctx->ctx.ctx.vctx);

    picctx->pool = pool;
    nvdec_pool_AddRef(picctx->pool);

    pic->context = &picctx->ctx.ctx;
    return pic;

error:
    picture_Release(pic);
    return NULL;
}

static vlc_fourcc_t MapSurfaceChroma(cudaVideoChromaFormat chroma, unsigned bitDepth)
{
    switch (chroma) {
        case cudaVideoChromaFormat_420:
            if (bitDepth <= 8)
                return VLC_CODEC_NV12;
            if (bitDepth <= 10)
                return VLC_CODEC_P010;
            return VLC_CODEC_P016;
        case cudaVideoChromaFormat_444:
            if (bitDepth <= 8)
                return VLC_CODEC_I444;
            return VLC_CODEC_I444_16L;
        default:
            return 0;
    }
}

static vlc_fourcc_t MapSurfaceOpaqueChroma(cudaVideoChromaFormat chroma, unsigned bitDepth)
{
    switch (chroma) {
        case cudaVideoChromaFormat_420:
            if (bitDepth <= 8)
                return VLC_CODEC_NVDEC_OPAQUE;
            if (bitDepth <= 10)
                return VLC_CODEC_NVDEC_OPAQUE_10B;
            return VLC_CODEC_NVDEC_OPAQUE_16B;
        case cudaVideoChromaFormat_444:
            if (bitDepth <= 8)
                return VLC_CODEC_NVDEC_OPAQUE_444;
            return VLC_CODEC_NVDEC_OPAQUE_444_16B;
        default:
            return 0;
    }
}

static cudaVideoSurfaceFormat MapSurfaceFmt(int i_vlc_fourcc)
{
    switch (i_vlc_fourcc) {
        case VLC_CODEC_NVDEC_OPAQUE_10B:
        case VLC_CODEC_NVDEC_OPAQUE_16B:
        case VLC_CODEC_P010:
        case VLC_CODEC_P016:
            return cudaVideoSurfaceFormat_P016;
        case VLC_CODEC_NVDEC_OPAQUE:
        case VLC_CODEC_NV12:
            return cudaVideoSurfaceFormat_NV12;
        case VLC_CODEC_NVDEC_OPAQUE_444:
        case VLC_CODEC_I444:
            return cudaVideoSurfaceFormat_YUV444;
        case VLC_CODEC_NVDEC_OPAQUE_444_16B:
        case VLC_CODEC_I444_16L:
             return cudaVideoSurfaceFormat_YUV444_16Bit;
        default:             vlc_assert_unreachable();
    }
}

static int CUtoFMT(video_format_t *fmt, const CUVIDEOFORMAT *p_format)
{
    // bit depth and chroma
    unsigned int i_bpp = p_format->bit_depth_luma_minus8 + 8;
    vlc_fourcc_t i_chroma;
    if (is_nvdec_opaque(fmt->i_chroma))
        i_chroma = MapSurfaceOpaqueChroma(p_format->chroma_format, i_bpp);
    else
        i_chroma = MapSurfaceChroma(p_format->chroma_format, i_bpp);
    if (i_chroma == 0)
        return VLC_EGENERIC;

    fmt->i_chroma = i_chroma;
    // use the real padded size when we know it fmt->i_width = p_format->coded_width;
    fmt->i_height = p_format->coded_height;
    fmt->i_x_offset = p_format->display_area.left;
    fmt->i_y_offset = p_format->display_area.top;
    fmt->i_visible_width = p_format->display_area.right - p_format->display_area.left;
    fmt->i_visible_height = p_format->display_area.bottom - p_format->display_area.top;
    // frame rate
    fmt->i_frame_rate = p_format->frame_rate.numerator;
    fmt->i_frame_rate_base = p_format->frame_rate.denominator;
    fmt->i_bits_per_pixel = i_bpp;
    return VLC_SUCCESS;
}

static int CUDAAPI HandleVideoSequence(void *p_opaque, CUVIDEOFORMAT *p_format)
{
    decoder_t *p_dec = (decoder_t *) p_opaque;
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    int ret;

    if ( is_nvdec_opaque(p_dec->fmt_out.video.i_chroma) )
    {
        if (p_sys->out_pool)
        {
            nvdec_pool_Release(p_sys->out_pool);
            p_sys->out_pool = NULL;
        }
    }

    // update vlc's output format using NVDEC parser's output
    ret = CUtoFMT(&p_dec->fmt_out.video, p_format);
    if (ret != VLC_SUCCESS)
    {
        msg_Dbg(p_dec, "unsupported Chroma %d + BitDepth %d", p_format->chroma_format, p_format->bit_depth_luma_minus8 + 8);
        goto error;
    }
    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    ret = CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
    if (ret != VLC_SUCCESS)
        goto error;

    if (p_sys->cudecoder)
    {
        CALL_CUVID(cuvidDestroyDecoder, p_sys->cudecoder);
        p_sys->cudecoder = NULL;
    }

    CUVIDDECODECREATEINFO dparams = {
        .ulWidth             = p_format->coded_width,
        .ulHeight            = p_format->coded_height,
        .ulTargetWidth       = p_dec->fmt_out.video.i_width,
        .ulTargetHeight      = p_dec->fmt_out.video.i_height,
        .bitDepthMinus8      = p_format->bit_depth_luma_minus8,
        .OutputFormat        = MapSurfaceFmt(p_dec->fmt_out.video.i_chroma),
        .CodecType           = p_format->codec,
        .ChromaFormat        = p_format->chroma_format,
        .ulNumDecodeSurfaces = p_sys->i_nb_surface,
        .ulNumOutputSurfaces = 1,
        .DeinterlaceMode     = p_sys->deintMode
    };
    ret = CALL_CUVID(cuvidCreateDecoder, &p_sys->cudecoder, &dparams);
    if (ret != VLC_SUCCESS)
        goto cuda_error;

    // ensure the output surfaces have the same pitch so copies can work properly
    if ( is_nvdec_opaque(p_dec->fmt_out.video.i_chroma) )
    {
        // get the real decoder pitch
        CUdeviceptr frameDevicePtr = 0;
        CUVIDPROCPARAMS params = {
            .progressive_frame = 1,
            .top_field_first = 1,
        };
        ret = CALL_CUVID( cuvidMapVideoFrame, p_sys->cudecoder, 0, &frameDevicePtr, &p_sys->outputPitch, &params );
        if (ret != VLC_SUCCESS)
            goto cuda_error;
        CALL_CUVID(cuvidUnmapVideoFrame, p_sys->cudecoder, frameDevicePtr);

        unsigned int ByteWidth = p_sys->outputPitch;
        unsigned int Height = p_dec->fmt_out.video.i_height;
        switch (dparams.OutputFormat)
        {
            case cudaVideoSurfaceFormat_YUV444:
            case cudaVideoSurfaceFormat_YUV444_16Bit:
                Height += 2 * Height; // 3 planes
                break;
            case cudaVideoSurfaceFormat_NV12:
            case cudaVideoSurfaceFormat_P016:
                Height += Height / 2; // U and V at quarter resolution
                break;
            default:
                vlc_assert_unreachable();
        }

        p_sys->out_pool = nvdec_pool_Create(p_sys->vctx_out,
                                            &p_dec->fmt_out.video,
                                            ByteWidth,
                                            Height);
        if (p_sys->out_pool == NULL)
            goto cuda_error;
    }

    p_sys->decoderHeight = p_format->coded_height;

    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);

    ret = decoder_UpdateVideoOutput(p_dec, p_sys->vctx_out);
    return (ret == VLC_SUCCESS);

cuda_error:
    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);
error:
    p_sys->b_nvparser_success = false;
    return 0;
}

static int CUDAAPI HandlePictureDecode(void *p_opaque, CUVIDPICPARAMS *p_picparams)
{
    decoder_t *p_dec = (decoder_t *) p_opaque;
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    int ret;

    ret = CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
    if (ret != VLC_SUCCESS)
        return 0;

    ret = CALL_CUVID(cuvidDecodePicture, p_sys->cudecoder, p_picparams);
    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);

    return (ret == VLC_SUCCESS);
}

static int CUDAAPI HandlePictureDisplay(void *p_opaque, CUVIDPARSERDISPINFO *p_dispinfo)
{
    decoder_t *p_dec = (decoder_t *) p_opaque;
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = NULL;
    CUdeviceptr frameDevicePtr = 0;
    CUVIDPROCPARAMS params = {
        .progressive_frame = p_sys->deintMode == cudaVideoDeinterlaceMode_Weave ? 1 : p_dispinfo->progressive_frame,
        .top_field_first = p_dispinfo->top_field_first,
        .second_field = p_dispinfo->repeat_first_field + 1,
        .unpaired_field = p_dispinfo->repeat_first_field < 0,
    };
    int result;

    if ( is_nvdec_opaque(p_dec->fmt_out.video.i_chroma) )
    {
        p_pic = nvdec_pool_Wait(p_sys->out_pool);
        if (unlikely(p_pic == NULL))
            return 0;
        pic_context_nvdec_t *picctx = NVDEC_PICCONTEXT_FROM_PICCTX(p_pic->context);

        result = CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
        if (unlikely(result != VLC_SUCCESS))
        {
            picture_Release(p_pic);
            return 0;
        }

        unsigned int i_pitch;

        // Map decoded frame to a device pointer
        result = CALL_CUVID( cuvidMapVideoFrame, p_sys->cudecoder, p_dispinfo->picture_index,
                            &frameDevicePtr, &i_pitch, &params );
        if (result != VLC_SUCCESS)
            goto error;

        picctx->devicePtr = (CUdeviceptr)p_pic->p_sys;
        picctx->bufferPitch = p_sys->outputPitch;
        picctx->bufferHeight = p_sys->decoderHeight;

        size_t srcY = 0;
        size_t dstY = 0;
        if (p_pic->format.i_chroma == VLC_CODEC_NVDEC_OPAQUE_444 || p_pic->format.i_chroma == VLC_CODEC_NVDEC_OPAQUE_444_16B)
        {
            for (int i_plane = 0; i_plane < 3; i_plane++) {
                CUDA_MEMCPY2D cu_cpy = {
                    .srcMemoryType  = CU_MEMORYTYPE_DEVICE,
                    .srcDevice      = frameDevicePtr,
                    .srcY           = srcY,
                    .srcPitch       = i_pitch,
                    .dstMemoryType  = CU_MEMORYTYPE_DEVICE,
                    .dstDevice      = picctx->devicePtr,
                    .dstPitch       = picctx->bufferPitch,
                    .dstY           = dstY,
                    .WidthInBytes   = i_pitch,
                    .Height         = __MIN(picctx->bufferHeight, p_dec->fmt_out.video.i_y_offset + p_dec->fmt_out.video.i_visible_height),
                };
                result = CALL_CUDA_DEC(cuMemcpy2DAsync, &cu_cpy, 0);
                if (unlikely(result != VLC_SUCCESS))
                    goto error;

                srcY += picctx->bufferHeight;
                dstY += p_sys->decoderHeight;
            }
        }
        else
        {
            for (int i_plane = 0; i_plane < 2; i_plane++) {
                CUDA_MEMCPY2D cu_cpy = {
                    .srcMemoryType  = CU_MEMORYTYPE_DEVICE,
                    .srcDevice      = frameDevicePtr,
                    .srcY           = srcY,
                    .srcPitch       = i_pitch,
                    .dstMemoryType  = CU_MEMORYTYPE_DEVICE,
                    .dstDevice      = picctx->devicePtr,
                    .dstPitch       = picctx->bufferPitch,
                    .dstY           = dstY,
                    .WidthInBytes   = i_pitch,
                    .Height         = __MIN(picctx->bufferHeight, p_dec->fmt_out.video.i_y_offset + p_dec->fmt_out.video.i_visible_height),
                };
                if (i_plane == 1)
                    cu_cpy.Height >>= 1;
                result = CALL_CUDA_DEC(cuMemcpy2DAsync, &cu_cpy, 0);
                if (unlikely(result != VLC_SUCCESS))
                    goto error;

                srcY += picctx->bufferHeight;
                dstY += p_sys->decoderHeight;
            }
        }
    }
    else
    {
        p_pic = decoder_NewPicture(p_dec);
        if (unlikely(p_pic == NULL))
            return 0;

        result = CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
        if (unlikely(result != VLC_SUCCESS))
        {
            picture_Release(p_pic);
            return 0;
        }

        unsigned int i_pitch;

        // Map decoded frame to a device pointer
        result = CALL_CUVID( cuvidMapVideoFrame, p_sys->cudecoder, p_dispinfo->picture_index,
                            &frameDevicePtr, &i_pitch, &params );
        if (result != VLC_SUCCESS)
            goto error;

        // Copy decoded frame into a new VLC picture
        size_t srcY = 0;
        for (int i_plane = 0; i_plane < p_pic->i_planes; i_plane++) {
            plane_t plane = p_pic->p[i_plane];
            CUDA_MEMCPY2D cu_cpy = {
                .srcMemoryType  = CU_MEMORYTYPE_DEVICE,
                .srcDevice      = frameDevicePtr,
                .srcY           = srcY,
                .srcPitch       = i_pitch,
                .dstMemoryType  = CU_MEMORYTYPE_HOST,
                .dstHost        = plane.p_pixels,
                .dstPitch       = plane.i_pitch,
                .WidthInBytes   = __MIN(i_pitch, (unsigned)plane.i_pitch),
                .Height         = plane.i_visible_lines,
            };
            result = CALL_CUDA_DEC(cuMemcpy2DAsync, &cu_cpy, 0);
            if (result != VLC_SUCCESS)
                goto error;
            srcY += p_sys->decoderHeight;
        }
    }

    // Wait until copies are finished
    result = CALL_CUDA_DEC(cuStreamSynchronize, 0);
    if (unlikely(result != VLC_SUCCESS))
        goto error;

    // Release surface on GPU
    result = CALL_CUVID(cuvidUnmapVideoFrame, p_sys->cudecoder, frameDevicePtr);
    if (unlikely(result != VLC_SUCCESS))
        goto error;

    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);

    if (p_sys->deintMode == cudaVideoDeinterlaceMode_Weave)
    {
        // the picture has not been deinterlaced, forward the field parameters
        p_pic->b_progressive = p_dispinfo->progressive_frame;
        p_pic->b_top_field_first = p_dispinfo->top_field_first;
        p_pic->i_nb_fields = 2 + p_dispinfo->repeat_first_field;
    }
    else
    {
        p_pic->b_progressive = true;
    }
    p_pic->date = p_dispinfo->timestamp;

    // Push decoded frame to display queue
    decoder_QueueVideo(p_dec, p_pic);
    return 1;

error:
    if (frameDevicePtr)
    {
        // Synchronize stream to wait for potentitally pending copies
        // then unmap the frame.
        // No need to check for errors, there is nothing we can do anyway
        CALL_CUDA_DEC(cuStreamSynchronize, 0);
        CALL_CUVID(cuvidUnmapVideoFrame, p_sys->cudecoder, frameDevicePtr);
    }
    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);
    if (p_pic)
        picture_Release(p_pic);
    return 0;
}

static int CuvidPushRawBlock(decoder_t *p_dec, uint8_t *buf, size_t bufsize)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;

    CUVIDSOURCEDATAPACKET cupacket = {
        .payload_size = bufsize,
        .payload = buf,
    };

    return CALL_CUVID(cuvidParseVideoData, p_sys->cuparser, &cupacket);
}

static int CuvidPushBlock(decoder_t *p_dec, block_t *p_block)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;

    CUVIDSOURCEDATAPACKET cupacket = {0};
    cupacket.flags |= CUVID_PKT_TIMESTAMP;
    cupacket.payload_size = p_block->i_buffer;
    cupacket.payload = p_block->p_buffer;
    cupacket.timestamp = p_block->i_pts == VLC_TICK_INVALID ? p_block->i_dts : p_block->i_pts;

    int ret = CALL_CUVID(cuvidParseVideoData, p_sys->cuparser, &cupacket);
    block_Release(p_block);
    return ret;
}

static block_t * HXXXProcessBlock(decoder_t *p_dec, block_t *p_block)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    if (p_sys->hh.b_is_xvcC && !p_sys->b_xps_pushed) {
        block_t *p_xps_blocks;   // parameter set blocks (SPS/PPS/VPS)
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264) {
            p_xps_blocks = h264_helper_get_annexb_config(&p_sys->hh);
        } else if (p_dec->fmt_in.i_codec == VLC_CODEC_HEVC) {
            p_xps_blocks = hevc_helper_get_annexb_config(&p_sys->hh);
        } else {
            return NULL;
        }
        for (block_t *p_b = p_xps_blocks; p_b != NULL; p_b = p_b->p_next) {
            CuvidPushRawBlock(p_dec, p_b->p_buffer, p_b->i_buffer);
        }
        p_sys->b_xps_pushed = true;
    }

    return p_sys->hh.pf_process_block(&p_sys->hh, p_block, NULL);
}

static block_t * ProcessVC1Block(decoder_t *p_dec, block_t *p_block)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    if (!p_sys->b_xps_pushed)
    {
        uint8_t *p_extra = p_dec->fmt_in.p_extra;
        CuvidPushRawBlock(p_dec, &p_extra[p_sys->vc1_header_offset], p_dec->fmt_in.i_extra - p_sys->vc1_header_offset);
        p_sys->b_xps_pushed = true;
    }

    /* Adding frame start code */
    p_block = block_Realloc(p_block, 4, p_block->i_buffer);
    if (p_block == NULL)
        return NULL;
    p_block->p_buffer[0] = 0x00;
    p_block->p_buffer[1] = 0x00;
    p_block->p_buffer[2] = 0x01;
    p_block->p_buffer[3] = 0x0d;

    return p_block;
}

static int CuvidPushEOS(decoder_t *p_dec)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;

    CUVIDSOURCEDATAPACKET cupacket = {0};
    cupacket.flags |= CUVID_PKT_ENDOFSTREAM;
    cupacket.payload_size = 0;
    cupacket.payload = NULL;
    cupacket.timestamp = 0;

    return CALL_CUVID(cuvidParseVideoData, p_sys->cuparser, &cupacket);
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    // If HandleVideoSequence fails, we give up decoding
    if (!p_sys->b_nvparser_success) {
        if (p_block != NULL) {
            block_Release(p_block);
        }
        return VLCDEC_ECRITICAL;
    }
    if (p_block == NULL) {
        // Flush stream
        return CuvidPushEOS(p_dec);
    }
    if (p_sys->process_block) {
        p_block = p_sys->process_block(p_dec, p_block);
        if (p_block == NULL) {
            // try next block
            return VLCDEC_SUCCESS;
        }
    }
    return CuvidPushBlock(p_dec, p_block);
}

static int MapCodecID(int i_vlc_fourcc)
{
    switch (i_vlc_fourcc) {
        case VLC_CODEC_H264: return cudaVideoCodec_H264;
        case VLC_CODEC_HEVC: return cudaVideoCodec_HEVC;
        case VLC_CODEC_VC1:  return cudaVideoCodec_VC1;
        case VLC_CODEC_WMV3: return cudaVideoCodec_VC1;
        case VLC_CODEC_MP1V: return cudaVideoCodec_MPEG1;
        case VLC_CODEC_MP2V: return cudaVideoCodec_MPEG2;
        case VLC_CODEC_MPGV: return cudaVideoCodec_MPEG2;
        case VLC_CODEC_MP4V: return cudaVideoCodec_MPEG4;
        case VLC_CODEC_VP8:  return cudaVideoCodec_VP8;
        case VLC_CODEC_VP9:  return cudaVideoCodec_VP9;
        default:             vlc_assert_unreachable();
    }
}

static cudaVideoChromaFormat MapChomaIDC(uint8_t chroma_idc)
{
    switch (chroma_idc)
    {
        case 0: return cudaVideoChromaFormat_Monochrome;
        case 1: return cudaVideoChromaFormat_420;
        case 2: return cudaVideoChromaFormat_422;
        case 3: return cudaVideoChromaFormat_444;
        default: vlc_assert_unreachable();
    }
}

static int ProbeDecoder(decoder_t *p_dec, uint8_t bitDepth, cudaVideoChromaFormat chroma)
{
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    int result = CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
    if (unlikely(result != VLC_SUCCESS))
        return result;

    p_sys->selectedDecoder.eCodecType         = MapCodecID(p_dec->fmt_in.i_codec);
    p_sys->selectedDecoder.eChromaFormat      = chroma;
    p_sys->selectedDecoder.nBitDepthMinus8    = bitDepth - 8;

    result =  CALL_CUVID(cuvidGetDecoderCaps, &p_sys->selectedDecoder);
    if (!p_sys->selectedDecoder.bIsSupported) {
        msg_Err(p_dec, "Codec %d Chroma %d not supported!", p_sys->selectedDecoder.eCodecType,
                                                            p_sys->selectedDecoder.eChromaFormat);
        result = VLC_EGENERIC;
        goto error;
    }
    if (result != VLC_SUCCESS) {
        msg_Err(p_dec, "No hardware for Codec %d Chroma %d", p_sys->selectedDecoder.eCodecType,
                                                             p_sys->selectedDecoder.eChromaFormat);
        goto error;
    }
    result = VLC_SUCCESS;

error:
    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);
    return result;
}

static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *) p_this;
    int result;
    nvdec_ctx_t *p_sys = vlc_obj_calloc(VLC_OBJECT(p_dec), 1, sizeof(*p_sys));
    if (unlikely(!p_sys))
        return VLC_ENOMEM;

    p_dec->p_sys = p_sys;

    switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            p_sys->b_is_hxxx = true;
            p_sys->i_nb_surface = MAX_HXXX_SURFACES;
            hxxx_helper_init(&p_sys->hh, VLC_OBJECT(p_dec),
                             p_dec->fmt_in.i_codec, false);
            result = hxxx_helper_set_extra(&p_sys->hh, p_dec->fmt_in.p_extra,
                                           p_dec->fmt_in.i_extra);
            if (result != VLC_SUCCESS) {
                hxxx_helper_clean(&p_sys->hh);
                return VLC_EGENERIC;
            }
            p_sys->process_block = HXXXProcessBlock;
            break;
        case VLC_CODEC_VC1:
        case VLC_CODEC_WMV3:
            if (p_dec->fmt_in.i_extra >= 4)
            {
                uint8_t *p_extra = p_dec->fmt_in.p_extra;
                /* Initialisation data starts with : 0x00 0x00 0x01 0x0f */
                /* Skipping unecessary data */
                static const uint8_t vc1_start_code[4] = {0x00, 0x00, 0x01, 0x0f};
                for (; p_sys->vc1_header_offset < p_dec->fmt_in.i_extra - 4 ; ++p_sys->vc1_header_offset)
                {
                    if (!memcmp(&p_extra[p_sys->vc1_header_offset], vc1_start_code, 4))
                        break;
                }
                if (p_sys->vc1_header_offset < p_dec->fmt_in.i_extra - 4)
                {
                    p_sys->process_block = ProcessVC1Block;
                    p_sys->i_nb_surface = MAX_HXXX_SURFACES;
                    break;
                }
            }
            return VLC_EGENERIC;
        case VLC_CODEC_MP1V:
        case VLC_CODEC_MP2V:
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP4V:
        case VLC_CODEC_VP8:
            p_sys->i_nb_surface = 3;
            break;
        case VLC_CODEC_VP9:
            if (p_dec->fmt_in.i_profile != 0 && p_dec->fmt_in.i_profile != 2)
            {
                msg_Warn(p_dec, "Unsupported VP9 profile %d", p_dec->fmt_in.i_profile);
                return VLC_EGENERIC;
            }
            p_sys->i_nb_surface = 10;
            break;
        default:
            return VLC_EGENERIC;
    }

    vlc_decoder_device *dec_device = decoder_GetDecoderDevice( p_dec );
    if (dec_device == NULL)
        return VLC_ENOOBJ;
    p_sys->devsys = GetNVDECOpaqueDevice(dec_device);
    if (p_sys->devsys == NULL)
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_ENOOBJ;
    }
    p_sys->vctx_out = vlc_video_context_Create( dec_device, VLC_VIDEO_CONTEXT_NVDEC, 0, NULL );
    vlc_decoder_device_Release(dec_device);
    if (unlikely(p_sys->vctx_out == NULL))
    {
        msg_Err(p_dec, "failed to create a video context");
        return VLC_ENOOBJ;
    }

    result = cuvid_load_functions(&p_sys->cuvidFunctions, p_dec);
    if (result != VLC_SUCCESS) {
        if (p_sys->b_is_hxxx)
            hxxx_helper_clean(&p_sys->hh);
        goto error;
    }

    CUVIDPARSERPARAMS pparams = {
        .CodecType               = MapCodecID(p_dec->fmt_in.i_codec),
        .ulClockRate             = CLOCK_FREQ,
        .ulMaxDisplayDelay       = NVDEC_DISPLAY_SURFACES,
        .ulMaxNumDecodeSurfaces  = p_sys->i_nb_surface,
        .pUserData               = p_dec,
        .pfnSequenceCallback     = HandleVideoSequence,
        .pfnDecodePicture        = HandlePictureDecode,
        .pfnDisplayPicture       = HandlePictureDisplay,
    };
    result = CALL_CUVID(cuvidCreateVideoParser, &p_sys->cuparser, &pparams);
    if (result != VLC_SUCCESS) {
        msg_Err(p_dec, "Unable to create NVDEC video parser");
        goto error;
    }

    uint8_t i_depth_luma;
    cudaVideoChromaFormat cudaChroma;

    int i_sar_num, i_sar_den = 0;

    // try different output
    if (p_sys->b_is_hxxx)
    {
        uint8_t i_chroma_idc, i_depth_chroma;
        result = hxxx_helper_get_chroma_chroma(&p_sys->hh, &i_chroma_idc,
                                            &i_depth_luma, &i_depth_chroma);
        if (result != VLC_SUCCESS) {
            if (p_sys->b_is_hxxx)
                hxxx_helper_clean(&p_sys->hh);
            return VLC_EGENERIC;
        }
        cudaChroma = MapChomaIDC(i_chroma_idc);

        unsigned i_w, i_h, i_vw, i_vh;
        result = hxxx_helper_get_current_picture_size(&p_sys->hh, &i_w, &i_h, &i_vw, &i_vh);
        if (result != VLC_SUCCESS)
            goto error;

        if(p_dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF)
        {
            video_color_primaries_t primaries;
            video_transfer_func_t transfer;
            video_color_space_t colorspace;
            video_color_range_t full_range;
            if (hxxx_helper_get_colorimetry(&p_sys->hh,
                                            &primaries,
                                            &transfer,
                                            &colorspace,
                                            &full_range) == VLC_SUCCESS)
            {
                p_dec->fmt_out.video.primaries = primaries;
                p_dec->fmt_out.video.transfer = transfer;
                p_dec->fmt_out.video.space = colorspace;
                p_dec->fmt_out.video.color_range = full_range;
            }
        }

        p_dec->fmt_out.video.i_width = vlc_align(i_w, OUTPUT_WIDTH_ALIGN);
        p_dec->fmt_out.video.i_height = i_h;

        if (!p_dec->fmt_in.video.i_visible_width || !p_dec->fmt_in.video.i_visible_height)
        {
            p_dec->fmt_out.video.i_visible_width = i_vw;
            p_dec->fmt_out.video.i_visible_height = i_vh;
        }

        if (VLC_SUCCESS !=
            hxxx_helper_get_current_sar(&p_sys->hh, &i_sar_num, &i_sar_den))
        {
            i_sar_den = 0;
        }
    }
    else
    {
        p_dec->fmt_out.video.i_width = vlc_align(p_dec->fmt_in.video.i_width, OUTPUT_WIDTH_ALIGN);
        p_dec->fmt_out.video.i_height = p_dec->fmt_in.video.i_height;
        cudaChroma = cudaVideoChromaFormat_420;
        i_depth_luma = 8;
        if (p_dec->fmt_in.i_codec == VLC_CODEC_VP9)
        {
            switch (p_dec->fmt_in.i_profile)
            {
                case 0: // 8 bits 4:2:0
                    i_depth_luma = 8;
                    break;
                case 2: // 10/12 bits 4:2:0
                    i_depth_luma = 10;
                    break;
                case 1: // 8 bits 4:2:2 / 4:4:4
                case 3: // 10/12 bits 4:2:2 / 4:4:4
                    // NOT SUPPORTED/TESTED yet
                    assert(0);
                default:
                    msg_Dbg(p_dec, "VP9 with unknown profile not supported");
                    goto error;
            }
        }
    }
    if (p_dec->fmt_in.video.i_sar_den != 0)
    {
        i_sar_num = p_dec->fmt_in.video.i_sar_num;
        i_sar_den = p_dec->fmt_in.video.i_sar_den;
    }
    if (i_sar_den == 0)
    {
        i_sar_num = 1;
        i_sar_den = 1;
    }

    p_dec->fmt_out.video.i_sar_num = i_sar_num;
    p_dec->fmt_out.video.i_sar_den = i_sar_den;
#undef ALIGN
    p_dec->fmt_out.video.i_bits_per_pixel = i_depth_luma;
    p_dec->fmt_out.video.i_frame_rate = p_dec->fmt_in.video.i_frame_rate;
    p_dec->fmt_out.video.i_frame_rate_base = p_dec->fmt_in.video.i_frame_rate_base;

    result = ProbeDecoder(p_dec, i_depth_luma, cudaChroma);
    if (result != VLC_SUCCESS)
        goto error;

    if ( p_dec->fmt_out.video.i_width < p_sys->selectedDecoder.nMinWidth ||
         p_dec->fmt_out.video.i_height < p_sys->selectedDecoder.nMinHeight )
    {
        msg_Err( p_dec, "dimensions too small: needed %dx%d, got %dx%d",
                 p_sys->selectedDecoder.nMinWidth, p_sys->selectedDecoder.nMinHeight,
                 p_dec->fmt_out.video.i_width, p_dec->fmt_out.video.i_height);
        goto error;
    }

    if ( p_dec->fmt_out.video.i_width > p_sys->selectedDecoder.nMaxWidth ||
         p_dec->fmt_out.video.i_height > p_sys->selectedDecoder.nMaxHeight )
    {
        msg_Err( p_dec, "dimensions too big: max %dx%d, got %dx%d",
                 p_sys->selectedDecoder.nMaxWidth, p_sys->selectedDecoder.nMaxHeight,
                 p_dec->fmt_out.video.i_width, p_dec->fmt_out.video.i_height);
        goto error;
    }

    vlc_fourcc_t output_chromas[3];
    size_t chroma_idx = 0;
    output_chromas[chroma_idx++] = MapSurfaceOpaqueChroma(cudaChroma, i_depth_luma);
    output_chromas[chroma_idx++] = MapSurfaceChroma(cudaChroma, i_depth_luma);
    output_chromas[chroma_idx++] = 0;

    for (chroma_idx = 0; output_chromas[chroma_idx] != 0; chroma_idx++)
    {
        p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma = output_chromas[chroma_idx];
        result = decoder_UpdateVideoOutput(p_dec, p_sys->vctx_out);
        if (result == VLC_SUCCESS)
        {
            msg_Dbg(p_dec, "using chroma %4.4s", (char*)&p_dec->fmt_out.video.i_chroma);
            break;
        }
        msg_Warn(p_dec, "Failed to use output chroma %4.4s", (char*)&p_dec->fmt_out.video.i_chroma);
    }
    if (result != VLC_SUCCESS)
        goto error;

    int deinterlace_mode    = var_InheritInteger(p_dec, "nvdec-deint");
    if (deinterlace_mode <= 0)
        p_sys->deintMode = cudaVideoDeinterlaceMode_Weave;
    else if (deinterlace_mode == 1)
        p_sys->deintMode = cudaVideoDeinterlaceMode_Bob;
    else
        p_sys->deintMode = cudaVideoDeinterlaceMode_Adaptive;

    p_dec->pf_decode = DecodeBlock;

    p_sys->b_nvparser_success = true;

    return VLC_SUCCESS;

error:
    CloseDecoder(p_this);
    p_dec->p_sys = NULL;
    return VLC_EGENERIC;
}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *) p_this;
    nvdec_ctx_t *p_sys = p_dec->p_sys;
    CALL_CUDA_DEC(cuCtxPushCurrent, p_sys->devsys->cuCtx);
    CALL_CUDA_DEC(cuCtxPopCurrent, NULL);

    if (p_sys->out_pool)
        nvdec_pool_Release(p_sys->out_pool);
    if (p_sys->cudecoder)
        CALL_CUVID(cuvidDestroyDecoder, p_sys->cudecoder);
    if (p_sys->cuparser)
        CALL_CUVID(cuvidDestroyVideoParser, p_sys->cuparser);
    if (p_sys->vctx_out)
        vlc_video_context_Release(p_sys->vctx_out);
    cuvid_free_functions(&p_sys->cuvidFunctions);
    if (p_sys->b_is_hxxx)
        hxxx_helper_clean(&p_sys->hh);
}

/** Decoder Device **/
static void DecoderContextClose(vlc_decoder_device *device)
{
    decoder_device_nvdec_t *p_sys = GetNVDECOpaqueDevice(device);
    if (p_sys->cuCtx)
        CALL_CUDA_DEV(cuCtxDestroy, p_sys->cuCtx);
    cuda_free_functions(&p_sys->cudaFunctions);
}

static const struct vlc_decoder_device_operations dev_ops = {
    .close = DecoderContextClose,
};

static int
DecoderContextOpen(vlc_decoder_device *device, vout_window_t *window)
{
    VLC_UNUSED(window);

    decoder_device_nvdec_t *p_sys = vlc_obj_malloc(VLC_OBJECT(device), sizeof(*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;
    device->opaque = p_sys;
    device->ops = &dev_ops;
    device->type = VLC_DECODER_DEVICE_NVDEC;
    p_sys->cudaFunctions = NULL;

    int result = cuda_load_functions(&p_sys->cudaFunctions, device);
    if (result != VLC_SUCCESS) {
        return VLC_EGENERIC;
    }

    result = CALL_CUDA_DEV(cuInit, 0);
    if (result != VLC_SUCCESS)
    {
        DecoderContextClose(device);
        return result;
    }

    result = CALL_CUDA_DEV(cuCtxCreate, &p_sys->cuCtx, 0, 0);
    if (result != VLC_SUCCESS)
    {
        DecoderContextClose(device);
        return result;
    }

    return VLC_SUCCESS;
}

