/*****************************************************************************
 * d3d9_fmt.h : D3D9 helper calls
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

#ifndef VLC_VIDEOCHROMA_D3D9_FMT_H_
#define VLC_VIDEOCHROMA_D3D9_FMT_H_

#include <vlc_picture.h>

#define COBJMACROS
#include <d3d9.h>
#include <dxva2api.h>

#include "dxgi_fmt.h"

/* owned by the vout for VLC_CODEC_D3D9_OPAQUE */
struct picture_sys_t
{
    IDirect3DSurface9    *surface;
    /* decoder only */
    IDirectXVideoDecoder *decoder; /* keep a reference while the surface exist */
    HINSTANCE            dxva2_dll;
};

typedef struct
{
    HINSTANCE               hdll;       /* handle of the opened d3d9 dll */
    union {
        LPDIRECT3D9         obj;
        LPDIRECT3D9EX       objex;
    };
    bool                    use_ex;
} d3d9_handle_t;

typedef struct
{
    /* d3d9_handle_t           hd3d; TODO */
    union
    {
        LPDIRECT3DDEVICE9   dev;
        LPDIRECT3DDEVICE9EX devex;
    };
    bool                    owner;

    /* creation parameters */
    D3DPRESENT_PARAMETERS   pp;
    UINT                    adapterId;
    HWND                    hwnd;
    D3DCAPS9                caps;
} d3d9_device_t;

#include "../codec/avcodec/va_surface.h"

picture_sys_t *ActivePictureSys(picture_t *p_pic);

static inline void AcquirePictureSys(picture_sys_t *p_sys)
{
    IDirect3DSurface9_AddRef(p_sys->surface);
    if (p_sys->decoder)
        IDirectXVideoDecoder_AddRef(p_sys->decoder);
    p_sys->dxva2_dll = LoadLibrary(TEXT("DXVA2.DLL"));
}

static inline void ReleasePictureSys(picture_sys_t *p_sys)
{
    IDirect3DSurface9_Release(p_sys->surface);
    if (p_sys->decoder)
        IDirectXVideoDecoder_Release(p_sys->decoder);
    FreeLibrary(p_sys->dxva2_dll);
}

HRESULT D3D9_CreateDevice(vlc_object_t *, d3d9_handle_t *, HWND,
                          const video_format_t *, d3d9_device_t *out);
#define D3D9_CreateDevice(a,b,c,d,e) D3D9_CreateDevice( VLC_OBJECT(a), b, c, d, e )

void D3D9_ReleaseDevice(d3d9_device_t *);
int D3D9_Create(vlc_object_t *, d3d9_handle_t *);
#define D3D9_Create(a,b) D3D9_Create( VLC_OBJECT(a), b )

void D3D9_Destroy(d3d9_handle_t *);

int D3D9_FillPresentationParameters(d3d9_handle_t *, const video_format_t *, d3d9_device_t *);

struct wddm_version
{
    int wddm, d3d_features, revision, build;
};
int D3D9CheckDriverVersion(d3d9_handle_t *hd3d, d3d9_device_t *d3d_dev, UINT vendorId,
                           const struct wddm_version *min_ver);


#endif /* VLC_VIDEOCHROMA_D3D9_FMT_H_ */
