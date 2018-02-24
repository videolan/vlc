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

#include <d3d11.h>

#include "dxgi_fmt.h"

DEFINE_GUID(GUID_CONTEXT_MUTEX, 0x472e8835, 0x3f8e, 0x4f93, 0xa0, 0xcb, 0x25, 0x79, 0x77, 0x6c, 0xed, 0x86);

/* see https://msdn.microsoft.com/windows/hardware/commercialize/design/compatibility/device-graphics */
struct wddm_version
{
    int wddm, d3d_features, revision, build;
};

typedef struct
{
    ID3D11Device             *d3ddevice;       /* D3D device */
    ID3D11DeviceContext      *d3dcontext;      /* D3D context */
    bool                     owner;
#if !VLC_WINSTORE_APP
    struct wddm_version      WDDM;
#endif
} d3d11_device_t;

typedef struct
{
#if !VLC_WINSTORE_APP
    HINSTANCE                 hdll;       /* handle of the opened d3d11 dll */
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE                 dxgidebug_dll;
#endif
#endif
} d3d11_handle_t;

/* owned by the vout for VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    union {
        ID3D11Texture2D           *texture[D3D11_MAX_SHADER_VIEW];
        ID3D11Resource            *resource[D3D11_MAX_SHADER_VIEW];
    };
    ID3D11DeviceContext           *context;
    unsigned                      slice_index;
    ID3D11VideoProcessorInputView  *processorInput;  /* when used as processor input */
    ID3D11VideoProcessorOutputView *processorOutput; /* when used as processor output */
    ID3D11ShaderResourceView      *resourceView[D3D11_MAX_SHADER_VIEW];
    DXGI_FORMAT                   formatTexture;
};

#include "../codec/avcodec/va_surface.h"

picture_sys_t *ActivePictureSys(picture_t *p_pic);

/* index to use for texture/resource that use a known DXGI format
 * (ie not DXGI_FORMAT_UNKNWON) */
#define KNOWN_DXGI_INDEX   0

static inline bool is_d3d11_opaque(vlc_fourcc_t chroma)
{
    return chroma == VLC_CODEC_D3D11_OPAQUE ||
           chroma == VLC_CODEC_D3D11_OPAQUE_10B;
}

void AcquirePictureSys(picture_sys_t *p_sys);

void ReleasePictureSys(picture_sys_t *p_sys);

/* map texture planes to resource views */
int AllocateShaderView(vlc_object_t *obj, ID3D11Device *d3ddevice,
                              const d3d_format_t *format,
                              ID3D11Texture2D *p_texture[D3D11_MAX_SHADER_VIEW], UINT slice_index,
                              ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW]);

HRESULT D3D11_CreateDevice(vlc_object_t *obj, d3d11_handle_t *,
                           bool hw_decoding, d3d11_device_t *out);
#define D3D11_CreateDevice(a,b,c,d)  D3D11_CreateDevice( VLC_OBJECT(a), b, c, d )

void D3D11_ReleaseDevice(d3d11_device_t *);

int D3D11_Create(vlc_object_t *, d3d11_handle_t *);
#define D3D11_Create(a,b) D3D11_Create( VLC_OBJECT(a), b )

void D3D11_Destroy(d3d11_handle_t *);

bool isXboxHardware(ID3D11Device *d3ddev);
bool CanUseVoutPool(d3d11_device_t *, UINT slices);
IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev);
int D3D11CheckDriverVersion(d3d11_device_t *, UINT vendorId,
                            const struct wddm_version *min_ver);
void D3D11_GetDriverVersion(vlc_object_t *, d3d11_device_t *);
#define D3D11_GetDriverVersion(a,b) D3D11_GetDriverVersion(VLC_OBJECT(a),b)

static inline bool DeviceSupportsFormat(ID3D11Device *d3ddevice,
                                        DXGI_FORMAT format, UINT supportFlags)
{
    UINT i_formatSupport;
    return SUCCEEDED( ID3D11Device_CheckFormatSupport(d3ddevice, format,
                                                      &i_formatSupport) )
            && ( i_formatSupport & supportFlags ) == supportFlags;
}

const d3d_format_t *FindD3D11Format(ID3D11Device *d3ddevice,
                                    vlc_fourcc_t i_src_chroma,
                                    bool rgb_only,
                                    uint8_t bits_per_channel,
                                    bool allow_opaque,
                                    UINT supportFlags);

int AllocateTextures(vlc_object_t *obj, d3d11_device_t *d3d_dev,
                     const d3d_format_t *cfg, const video_format_t *fmt,
                     unsigned pool_size, ID3D11Texture2D *textures[]);

#ifndef NDEBUG
void D3D11_LogProcessorSupport(vlc_object_t*, ID3D11VideoProcessorEnumerator*);
#define D3D11_LogProcessorSupport(a,b) D3D11_LogProcessorSupport( VLC_OBJECT(a), b )
#endif

#endif /* include-guard */
