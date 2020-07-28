/*****************************************************************************
 * d3d11_fmt.h : D3D11 helper calls
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_VIDEOCHROMA_D3D11_FMT_H_
#define VLC_VIDEOCHROMA_D3D11_FMT_H_

#include <vlc_codec.h>

#include <d3d11.h>

#include "dxgi_fmt.h"

DEFINE_GUID(GUID_CONTEXT_MUTEX, 0x472e8835, 0x3f8e, 0x4f93, 0xa0, 0xcb, 0x25, 0x79, 0x77, 0x6c, 0xed, 0x86);

/* see https://msdn.microsoft.com/windows/hardware/commercialize/design/compatibility/device-graphics
 *     https://docs.microsoft.com/en-us/windows-hardware/drivers/display/wddm-2-1-features#driver-versioning
 *     https://www.intel.com/content/www/us/en/support/articles/000005654/graphics-drivers.html
 */
struct wddm_version
{
    int wddm, d3d_features, revision, build;
};

typedef struct
{
    ID3D11Device             *d3ddevice;       /* D3D device */
    ID3D11DeviceContext      *d3dcontext;      /* D3D context */
    bool                     mutex_owner;
    HANDLE                   context_mutex;
    struct wddm_version      WDDM;
    D3D_FEATURE_LEVEL        feature_level;
    DXGI_ADAPTER_DESC        adapterDesc;
} d3d11_device_t;

/* owned by the vout for VLC_CODEC_D3D11_OPAQUE */
typedef struct
{
    union {
        ID3D11Texture2D           *texture[D3D11_MAX_SHADER_VIEW];
        ID3D11Resource            *resource[D3D11_MAX_SHADER_VIEW];
    };
    unsigned                      slice_index;
    ID3D11VideoProcessorInputView  *processorInput;  /* when used as processor input */
    ID3D11VideoProcessorOutputView *processorOutput; /* when used as processor output */
    ID3D11ShaderResourceView      *renderSrc[D3D11_MAX_SHADER_VIEW];
} picture_sys_d3d11_t;

struct d3d11_pic_context
{
    picture_context_t         s;
    picture_sys_d3d11_t       picsys;
};

typedef struct
{
    d3d11_device_t      d3d_dev;
} d3d11_decoder_device_t;

typedef struct
{
    DXGI_FORMAT         format;
} d3d11_video_context_t;

/* index to use for texture/resource that use a known DXGI format
 * (ie not DXGI_FORMAT_UNKNWON) */
#define KNOWN_DXGI_INDEX   0

static inline bool is_d3d11_opaque(vlc_fourcc_t chroma)
{
    return chroma == VLC_CODEC_D3D11_OPAQUE ||
           chroma == VLC_CODEC_D3D11_OPAQUE_10B ||
           chroma == VLC_CODEC_D3D11_OPAQUE_RGBA ||
           chroma == VLC_CODEC_D3D11_OPAQUE_BGRA;
}

extern const struct vlc_video_context_operations d3d11_vctx_ops;

picture_sys_d3d11_t *ActiveD3D11PictureSys(picture_t *);

static inline d3d11_decoder_device_t *GetD3D11OpaqueDevice(vlc_decoder_device *device)
{
    if (device == NULL || device->type != VLC_DECODER_DEVICE_D3D11VA)
        return NULL;
    return device->opaque;
}

static inline d3d11_decoder_device_t *GetD3D11OpaqueContext(vlc_video_context *vctx)
{
    vlc_decoder_device *device = vctx ? vlc_video_context_HoldDevice(vctx) : NULL;
    if (unlikely(device == NULL))
        return NULL;
    d3d11_decoder_device_t *res = NULL;
    if (device->type == VLC_DECODER_DEVICE_D3D11VA)
    {
        assert(device->opaque != NULL);
        res = GetD3D11OpaqueDevice(device);
    }
    vlc_decoder_device_Release(device);
    return res;
}

static inline d3d11_video_context_t *GetD3D11ContextPrivate(vlc_video_context *vctx)
{
    return (d3d11_video_context_t *) vlc_video_context_GetPrivate( vctx, VLC_VIDEO_CONTEXT_D3D11VA );
}

vlc_video_context *D3D11CreateVideoContext(vlc_decoder_device *, DXGI_FORMAT);

void AcquireD3D11PictureSys(picture_sys_d3d11_t *p_sys);

void ReleaseD3D11PictureSys(picture_sys_d3d11_t *p_sys);

/* map texture planes to resource views */
int D3D11_AllocateResourceView(vlc_object_t *obj, ID3D11Device *d3ddevice,
                             const d3d_format_t *format,
                             ID3D11Texture2D *p_texture[D3D11_MAX_SHADER_VIEW], UINT slice_index,
                             ID3D11ShaderResourceView *output[D3D11_MAX_SHADER_VIEW]);
#define D3D11_AllocateResourceView(a,b,c,d,e,f)  D3D11_AllocateResourceView(VLC_OBJECT(a),b,c,d,e,f)

d3d11_decoder_device_t *D3D11_CreateDevice(vlc_object_t *obj, IDXGIAdapter *,
                                   bool hw_decoding, bool forced);
#define D3D11_CreateDevice(a,b,c,d)  D3D11_CreateDevice( VLC_OBJECT(a), b, c, d )

void D3D11_ReleaseDevice(d3d11_decoder_device_t *);

void D3D11_LogResources(d3d11_decoder_device_t *);

bool isXboxHardware(const d3d11_device_t *);
IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev);
int D3D11CheckDriverVersion(const d3d11_device_t *, UINT vendorId,
                            const struct wddm_version *min_ver);

bool DeviceSupportsFormat(ID3D11Device *d3ddevice, DXGI_FORMAT format, UINT supportFlags);

#define D3D11_RGB_FORMAT  1
#define D3D11_YUV_FORMAT  2

#define D3D11_CHROMA_CPU 1
#define D3D11_CHROMA_GPU 2

const d3d_format_t *FindD3D11Format(vlc_object_t *,
                                    d3d11_device_t*,
                                    vlc_fourcc_t i_src_chroma,
                                    int rgb_yuv,
                                    uint8_t bits_per_channel,
                                    uint8_t widthDenominator,
                                    uint8_t heightDenominator,
                                    int cpu_gpu,
                                    UINT supportFlags);
#define FindD3D11Format(a,b,c,d,e,f,g,h,i)  \
    FindD3D11Format(VLC_OBJECT(a),b,c,d,e,f,g,h,i)

int AllocateTextures(vlc_object_t *, d3d11_device_t *, const d3d_format_t *,
                     const video_format_t *, ID3D11Texture2D *textures[],
                     plane_t planes[]);
#define AllocateTextures(a,b,c,d,e,f)  AllocateTextures(VLC_OBJECT(a),b,c,d,e,f)

static inline void d3d11_device_lock(d3d11_device_t *d3d_dev)
{
    if( d3d_dev->context_mutex != INVALID_HANDLE_VALUE )
        WaitForSingleObjectEx( d3d_dev->context_mutex, INFINITE, FALSE );
}

static inline void d3d11_device_unlock(d3d11_device_t *d3d_dev)
{
    if( d3d_dev->context_mutex  != INVALID_HANDLE_VALUE )
        ReleaseMutex( d3d_dev->context_mutex );
}

void d3d11_pic_context_destroy(picture_context_t *);
picture_context_t *d3d11_pic_context_copy(picture_context_t *);

picture_t *D3D11_AllocPicture(vlc_object_t *,
                              const video_format_t *, vlc_video_context *, const d3d_format_t *);

#endif /* include-guard */
