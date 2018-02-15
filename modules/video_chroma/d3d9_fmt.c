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

#include "d3d9_fmt.h"

#include "../codec/avcodec/va_surface.h"

picture_sys_t *ActivePictureSys(picture_t *p_pic)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)p_pic->context;
    return pic_ctx ? &pic_ctx->picsys : p_pic->p_sys;
}

#undef D3D9_CreateDevice
HRESULT D3D9_CreateDevice(vlc_object_t *o, d3d9_handle_t *hd3d, HWND hwnd,
                          const video_format_t *source, d3d9_device_t *out)
{
    HRESULT hr;

    UINT AdapterToUse = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

#ifndef NDEBUG
    // Look for 'NVIDIA PerfHUD' adapter
    // If it is present, override default settings
    for (UINT Adapter=0; Adapter< IDirect3D9_GetAdapterCount(hd3d->obj); ++Adapter) {
        D3DADAPTER_IDENTIFIER9 Identifier;
        hr = IDirect3D9_GetAdapterIdentifier(hd3d->obj,Adapter,0,&Identifier);
        if (SUCCEEDED(hr) && strstr(Identifier.Description,"PerfHUD") != 0) {
            AdapterToUse = Adapter;
            DeviceType = D3DDEVTYPE_REF;
            break;
        }
    }
#endif

    /*
    ** Get device capabilities
    */
    ZeroMemory(&out->caps, sizeof(out->caps));
    hr = IDirect3D9_GetDeviceCaps(hd3d->obj, AdapterToUse, DeviceType, &out->caps);
    if (FAILED(hr)) {
       msg_Err(o, "Could not read adapter capabilities. (hr=0x%0lx)", hr);
       return hr;
    }
    msg_Dbg(o, "D3D9 device caps 0x%0lX / 0x%0lX", out->caps.DevCaps, out->caps.DevCaps2);

    /* TODO: need to test device capabilities and select the right render function */
    if (!(out->caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) {
        msg_Err(o, "Device does not support stretching from textures.");
        return E_INVALIDARG;
    }

    if ( source->i_width > out->caps.MaxTextureWidth ||
         source->i_height > out->caps.MaxTextureHeight )
    {
        msg_Err(o, "Textures too large %ux%u max possible: %ux%u",
                source->i_width, source->i_height,
                (unsigned) out->caps.MaxTextureWidth,
                (unsigned) out->caps.MaxTextureHeight);
        return E_INVALIDARG;
    }

    out->adapterId = AdapterToUse;
    out->hwnd      = hwnd;
    if (D3D9_FillPresentationParameters(hd3d, source, out))
    {
        msg_Err(o, "Could not presentation parameters");
        return E_INVALIDARG;
    }

    /* */
    D3DADAPTER_IDENTIFIER9 d3dai;
    if (FAILED(IDirect3D9_GetAdapterIdentifier(hd3d->obj, AdapterToUse,0, &d3dai))) {
        msg_Warn(o, "IDirect3D9_GetAdapterIdentifier failed");
    } else {
        msg_Dbg(o, "Direct3d9 Device: %s %lu %lu %lu", d3dai.Description,
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
                                                 DeviceType, hwnd,
                                                 creationFlags,
                                                 &out->pp, NULL, &out->devex);
            else
                hr = IDirect3D9_CreateDevice(hd3d->obj, AdapterToUse,
                                             DeviceType, hwnd,
                                             creationFlags,
                                             &out->pp, &out->dev);
            if (SUCCEEDED(hr))
            {
                out->owner = true;
                return hr;
            }
        }
    }

    msg_Err(o, "failed to create the D3D9%s device %d/%d. (hr=0x%lX)",
               hd3d->use_ex?"Ex":"", AdapterToUse, DeviceType, hr);
    return hr;
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
                                    const video_format_t *source, d3d9_device_t *out)
{
    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    if (source->i_width)
    {
        HRESULT hr = IDirect3D9_GetAdapterDisplayMode(hd3d->obj, out->adapterId, &d3ddm);
        if (FAILED(hr))
           return VLC_EGENERIC;
    }

    /* Set up the structure used to create the D3DDevice. */
    D3DPRESENT_PARAMETERS *d3dpp = &out->pp;
    ZeroMemory(d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->EnableAutoDepthStencil = FALSE;
    if (source->i_width)
    {
        d3dpp->hDeviceWindow     = out->hwnd;
        d3dpp->SwapEffect        = D3DSWAPEFFECT_COPY;
        d3dpp->BackBufferFormat  = d3ddm.Format;
        d3dpp->BackBufferCount   = 1;
        d3dpp->BackBufferWidth   = __MAX((unsigned int)GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                              source->i_width);
        d3dpp->BackBufferHeight  = __MAX((unsigned int)GetSystemMetrics(SM_CYVIRTUALSCREEN),
                                              source->i_height);
    }
    else
    {
        d3dpp->hDeviceWindow     = NULL;
        d3dpp->SwapEffect        = D3DSWAPEFFECT_DISCARD;
        d3dpp->BackBufferCount   = 0;
        d3dpp->BackBufferFormat  = D3DFMT_X8R8G8B8;    /* FIXME what to put here */
    }

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

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);
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

int D3D9CheckDriverVersion(d3d9_handle_t *hd3d, d3d9_device_t *d3d_dev,
                           UINT vendorId, const struct wddm_version *min_ver)
{
    D3DADAPTER_IDENTIFIER9 identifier;
    HRESULT hr = IDirect3D9_GetAdapterIdentifier(hd3d->obj, d3d_dev->adapterId, 0, &identifier);
    if (FAILED(hr))
        return VLC_EGENERIC;

    if (vendorId && identifier.VendorId != vendorId)
        return VLC_SUCCESS;

    int wddm, d3d_features, revision, build;
    wddm         = (int) (identifier.DriverVersion.HighPart >> 16 & 0xFFFF);
    d3d_features = (int) (identifier.DriverVersion.HighPart >>  0 & 0xFFFF);
    revision     = (int) (identifier.DriverVersion.LowPart  >> 16 & 0xFFFF);
    build        = (int) (identifier.DriverVersion.LowPart  >>  0 & 0xFFFF);

    bool newer =
           wddm > min_ver->wddm ||
          (wddm == min_ver->wddm && (d3d_features > min_ver->d3d_features ||
                                    (d3d_features == min_ver->d3d_features &&
                                                (revision > min_ver->revision ||
                                                (revision == min_ver->revision &&
                                                       build > min_ver->build)))));

    return newer ? VLC_SUCCESS : VLC_EGENERIC;
}
