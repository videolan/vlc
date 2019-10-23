/*****************************************************************************
 * d3d9_fmt.c : D3D9 helper calls
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <initguid.h>
#include "d3d9_fmt.h"

#define D3D9_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), struct d3d9_pic_context, s)

picture_sys_d3d9_t *ActiveD3D9PictureSys(picture_t *pic)
{
    if (unlikely(pic->context == NULL))
        return pic->p_sys;

    struct d3d9_pic_context *pic_ctx = D3D9_PICCONTEXT_FROM_PICCTX(pic->context);
    return &pic_ctx->picsys;
}

#undef D3D9_CreateDevice
HRESULT D3D9_CreateDevice(vlc_object_t *o, d3d9_handle_t *hd3d, int AdapterToUse,
                          d3d9_device_t *out)
{
    HRESULT hr;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

    if (AdapterToUse == -1)
    {
        AdapterToUse = D3DADAPTER_DEFAULT;
#ifndef NDEBUG
        // Check for 'NVIDIA PerfHUD' adapter
        // If it is present, override default settings for performance debugging
        // see https://developer.nvidia.com/nvidia-perfhud up to Win7
        for (UINT Adapter=0; Adapter< IDirect3D9_GetAdapterCount(hd3d->obj); ++Adapter) {
            D3DADAPTER_IDENTIFIER9 Identifier;
            hr = IDirect3D9_GetAdapterIdentifier(hd3d->obj, Adapter, 0, &Identifier);
            if (SUCCEEDED(hr) && strstr(Identifier.Description,"PerfHUD") != 0) {
                AdapterToUse = Adapter;
                DeviceType = D3DDEVTYPE_REF;
                break;
            }
        }
#endif
    }

    /*
    ** Get device capabilities
    */
    ZeroMemory(&out->caps, sizeof(out->caps));
    hr = IDirect3D9_GetDeviceCaps(hd3d->obj, AdapterToUse, DeviceType, &out->caps);
    if (FAILED(hr)) {
       msg_Err(o, "Could not read adapter capabilities. (hr=0x%lX)", hr);
       return hr;
    }
    msg_Dbg(o, "D3D9 device caps 0x%lX / 0x%lX", out->caps.DevCaps, out->caps.DevCaps2);

    /* TODO: need to test device capabilities and select the right render function */
    if (!(out->caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) {
        msg_Err(o, "Device does not support stretching from textures.");
        return E_INVALIDARG;
    }

    out->adapterId = AdapterToUse;
    /* TODO only create a device for the decoder dimensions */
    D3DPRESENT_PARAMETERS d3dpp;
    if (D3D9_FillPresentationParameters(hd3d, out, &d3dpp))
    {
        msg_Err(o, "Could not presentation parameters");
        return E_INVALIDARG;
    }

    /* */
    D3DADAPTER_IDENTIFIER9 d3dai;
    if (FAILED(IDirect3D9_GetAdapterIdentifier(hd3d->obj, AdapterToUse,0, &d3dai))) {
        msg_Warn(o, "IDirect3D9_GetAdapterIdentifier failed");
    } else {
        msg_Dbg(o, "Direct3d9 Device: %s %lx %lx %lx", d3dai.Description,
                d3dai.VendorId, d3dai.DeviceId, d3dai.Revision );
    }

    DWORD thread_modes[] = { D3DCREATE_MULTITHREADED, 0 };
    DWORD vertex_modes[] = { D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
                             D3DCREATE_HARDWARE_VERTEXPROCESSING,
                             D3DCREATE_MIXED_VERTEXPROCESSING,
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING };

    for (size_t t = 0; t < ARRAY_SIZE(thread_modes); t++)
    {
        for (size_t v = 0; v < ARRAY_SIZE(vertex_modes); v++)
        {
            DWORD creationFlags = thread_modes[t] | vertex_modes[v];
            if (hd3d->use_ex)
                hr = IDirect3D9Ex_CreateDeviceEx(hd3d->objex, AdapterToUse,
                                                 DeviceType, NULL,
                                                 creationFlags,
                                                 &d3dpp, NULL, &out->devex);
            else
                hr = IDirect3D9_CreateDevice(hd3d->obj, AdapterToUse,
                                             DeviceType, NULL,
                                             creationFlags,
                                             &d3dpp, &out->dev);
            if (SUCCEEDED(hr))
            {
                out->BufferFormat = d3dpp.BackBufferFormat;
                out->owner = true;
                return hr;
            }
        }
    }

    msg_Err(o, "failed to create the D3D9%s device %d/%d. (hr=0x%lX)",
               hd3d->use_ex?"Ex":"", AdapterToUse, DeviceType, hr);
    return hr;
}

HRESULT D3D9_CreateDeviceExternal(IDirect3DDevice9 *dev, d3d9_handle_t *hd3d,
                                  d3d9_device_t *out)
{
    D3DDEVICE_CREATION_PARAMETERS params;
    HRESULT hr = IDirect3DDevice9_GetCreationParameters(dev, &params);
    if (FAILED(hr))
       return hr;
    out->dev   = dev;
    out->owner = false;
    out->adapterId = params.AdapterOrdinal;
    ZeroMemory(&out->caps, sizeof(out->caps));
    hr = IDirect3D9_GetDeviceCaps(hd3d->obj, out->adapterId, params.DeviceType, &out->caps);
    if (FAILED(hr))
       return hr;
    D3DDISPLAYMODE d3ddm;
    hr = IDirect3D9_GetAdapterDisplayMode(hd3d->obj, out->adapterId, &d3ddm);
    if (FAILED(hr))
        return hr;
    IDirect3DDevice9_AddRef(out->dev);
    out->BufferFormat = d3ddm.Format;
    return S_OK;
}

void D3D9_ReleaseDevice(d3d9_device_t *d3d_dev)
{
    if (d3d_dev->dev)
    {
       IDirect3DDevice9_Release(d3d_dev->dev);
       d3d_dev->dev = NULL;
    }
}

/**
 * It setup vout_display_sys_t::d3dpp and vout_display_sys_t::rect_display
 * from the default adapter.
 */
int D3D9_FillPresentationParameters(d3d9_handle_t *hd3d,
                                    const d3d9_device_t *d3ddev,
                                    D3DPRESENT_PARAMETERS *d3dpp)
{
    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = IDirect3D9_GetAdapterDisplayMode(hd3d->obj, d3ddev->adapterId, &d3ddm);
    if (FAILED(hr))
        return VLC_EGENERIC;

    /* Set up the structure used to create the D3DDevice. */
    ZeroMemory(d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->EnableAutoDepthStencil = FALSE;
    d3dpp->hDeviceWindow          = NULL;
    d3dpp->SwapEffect             = D3DSWAPEFFECT_COPY;
    d3dpp->BackBufferFormat       = d3ddm.Format;
    d3dpp->BackBufferCount        = 1;
    d3dpp->BackBufferWidth        = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    d3dpp->BackBufferHeight       = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    return VLC_SUCCESS;
}

void D3D9_Destroy(d3d9_handle_t *hd3d)
{
    if (hd3d->obj)
    {
       IDirect3D9_Release(hd3d->obj);
       hd3d->obj = NULL;
    }
    if (hd3d->hdll)
    {
        FreeLibrary(hd3d->hdll);
        hd3d->hdll = NULL;
    }
}

/**
 * It initializes an instance of Direct3D9
 */
#undef D3D9_Create
int D3D9_Create(vlc_object_t *o, d3d9_handle_t *hd3d)
{
    hd3d->hdll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!hd3d->hdll) {
        msg_Warn(o, "cannot load d3d9.dll, aborting");
        return VLC_EGENERIC;
    }

    IDirect3D9 *(WINAPI *OurDirect3DCreate9)(UINT SDKVersion);
    OurDirect3DCreate9 =
        (void *)GetProcAddress(hd3d->hdll, "Direct3DCreate9");
    if (!OurDirect3DCreate9) {
        msg_Err(o, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        goto error;
    }

    HRESULT (WINAPI *OurDirect3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex **ppD3D);
    OurDirect3DCreate9Ex =
        (void *)GetProcAddress(hd3d->hdll, "Direct3DCreate9Ex");

    /* Create the D3D object. */
    hd3d->use_ex = false;
    if (OurDirect3DCreate9Ex) {
        HRESULT hr = OurDirect3DCreate9Ex(D3D_SDK_VERSION, &hd3d->objex);
        if(!FAILED(hr)) {
            msg_Dbg(o, "Using Direct3D9 Extended API!");
            hd3d->use_ex = true;
        }
    }

    if (!hd3d->obj)
    {
        hd3d->obj = OurDirect3DCreate9(D3D_SDK_VERSION);
        if (!hd3d->obj) {
            msg_Err(o, "Could not create Direct3D9 instance.");
            goto error;
        }
    }
    return VLC_SUCCESS;
error:
    D3D9_Destroy( hd3d );
    return VLC_EGENERIC;
}

int D3D9_CreateExternal(d3d9_handle_t *hd3d, IDirect3DDevice9 *d3d9dev)
{
    HRESULT hr = IDirect3DDevice9_GetDirect3D(d3d9dev, &hd3d->obj);
    if (unlikely(FAILED(hr)))
        return VLC_EGENERIC;
    hd3d->hdll = NULL;
    hd3d->use_ex = false; /* we don't care */
    return VLC_SUCCESS;
}

void D3D9_CloneExternal(d3d9_handle_t *hd3d, IDirect3D9 *dev)
{
    hd3d->obj = dev;
    IDirect3D9_AddRef( hd3d->obj );
    hd3d->hdll = NULL;

    void *pv = NULL;
    hd3d->use_ex = SUCCEEDED(IDirect3D9_QueryInterface(dev, &IID_IDirect3D9Ex, &pv));
    if (hd3d->use_ex && pv)
        IDirect3D9Ex_Release((IDirect3D9Ex*) pv);
}

static void ReleaseD3D9ContextPrivate(void *private)
{
    d3d9_video_context_t *octx = private;
    IDirect3DDevice9_Release(octx->dev);
}

const struct vlc_video_context_operations d3d9_vctx_ops = {
    ReleaseD3D9ContextPrivate,
};

void d3d9_pic_context_destroy(picture_context_t *ctx)
{
    struct d3d9_pic_context *pic_ctx = D3D9_PICCONTEXT_FROM_PICCTX(ctx);
    ReleaseD3D9PictureSys(&pic_ctx->picsys);
    free(pic_ctx);
}

picture_context_t *d3d9_pic_context_copy(picture_context_t *ctx)
{
    struct d3d9_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    *pic_ctx = *D3D9_PICCONTEXT_FROM_PICCTX(ctx);
    AcquireD3D9PictureSys(&pic_ctx->picsys);
    return &pic_ctx->s;
}
