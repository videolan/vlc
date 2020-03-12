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
#include <vlc_codec.h>

#define COBJMACROS
#include <d3d9.h>

#include "dxgi_fmt.h"

#define DXVAHD_TEXT N_("Use DXVA-HD for color conversion")
#define DXVAHD_LONGTEXT N_("Use DXVA-HD for color conversion")

/* owned by the vout for VLC_CODEC_D3D9_OPAQUE */
typedef struct
{
    IDirect3DSurface9    *surface;
} picture_sys_d3d9_t;

struct d3d9_pic_context
{
    picture_context_t         s;
    picture_sys_d3d9_t        picsys;
};

typedef struct
{
    HINSTANCE               hdll;       /* handle of the opened d3d9 dll */
    union {
        IDirect3D9          *obj;
        IDirect3D9Ex        *objex;
    };
    bool                    use_ex;
} d3d9_handle_t;

typedef struct
{
    union
    {
        IDirect3DDevice9    *dev;
        IDirect3DDevice9Ex  *devex;
    };

    /* creation parameters */
    UINT                    adapterId;

    D3DCAPS9                caps;
    D3DADAPTER_IDENTIFIER9  identifier;
} d3d9_device_t;

typedef struct
{
    d3d9_handle_t  hd3d;
    d3d9_device_t  d3ddev;
} d3d9_decoder_device_t;

typedef struct
{
    D3DFORMAT        format;
} d3d9_video_context_t;

static inline bool is_d3d9_opaque(vlc_fourcc_t chroma)
{
    switch (chroma)
    {
    case VLC_CODEC_D3D9_OPAQUE:
    case VLC_CODEC_D3D9_OPAQUE_10B:
        return true;
    default:
        return false;
    }
}

extern const struct vlc_video_context_operations d3d9_vctx_ops;

picture_sys_d3d9_t *ActiveD3D9PictureSys(picture_t *);

static inline d3d9_decoder_device_t *GetD3D9OpaqueDevice(vlc_decoder_device *device)
{
    if (device == NULL || device->type != VLC_DECODER_DEVICE_DXVA2)
        return NULL;
    return device->opaque;
}

static inline d3d9_decoder_device_t *GetD3D9OpaqueContext(vlc_video_context *vctx)
{
    vlc_decoder_device *device = vctx ? vlc_video_context_HoldDevice(vctx) : NULL;
    if (unlikely(device == NULL))
        return NULL;
    d3d9_decoder_device_t *res = NULL;
    if (device->type == VLC_DECODER_DEVICE_DXVA2)
    {
        assert(device->opaque != NULL);
        res = GetD3D9OpaqueDevice(device);
    }
    vlc_decoder_device_Release(device);
    return res;
}

static inline d3d9_video_context_t *GetD3D9ContextPrivate(vlc_video_context *vctx)
{
    return (d3d9_video_context_t *) vlc_video_context_GetPrivate( vctx, VLC_VIDEO_CONTEXT_DXVA2 );
}

static inline void AcquireD3D9PictureSys(picture_sys_d3d9_t *p_sys)
{
    IDirect3DSurface9_AddRef(p_sys->surface);
}

static inline void ReleaseD3D9PictureSys(picture_sys_d3d9_t *p_sys)
{
    IDirect3DSurface9_Release(p_sys->surface);
}

d3d9_decoder_device_t *D3D9_CreateDevice(vlc_object_t *);
#define D3D9_CreateDevice(a) D3D9_CreateDevice( VLC_OBJECT(a) )

void D3D9_ReleaseDevice(d3d9_decoder_device_t *);

int D3D9_ResetDevice(vlc_object_t *, d3d9_decoder_device_t *);

void d3d9_pic_context_destroy(picture_context_t *);
picture_context_t *d3d9_pic_context_copy(picture_context_t *);

#endif /* VLC_VIDEOCHROMA_D3D9_FMT_H_ */
