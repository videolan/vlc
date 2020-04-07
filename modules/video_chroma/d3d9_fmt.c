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

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <initguid.h>
#include "d3d9_fmt.h"

#define D3D9_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), struct d3d9_pic_context, s)

picture_sys_d3d9_t *ActiveD3D9PictureSys(picture_t *pic)
{
    assert(pic->context != NULL);
    assert(pic->p_sys == NULL);
    struct d3d9_pic_context *pic_ctx = D3D9_PICCONTEXT_FROM_PICCTX(pic->context);
    return &pic_ctx->picsys;
}

typedef struct {
    void                            *opaque;
    libvlc_video_output_cleanup_cb  cleanupDeviceCb;

    d3d9_decoder_device_t           dec_device;
} d3d9_decoder_device;

/**
 * It setup vout_display_sys_t::d3dpp and vout_display_sys_t::rect_display
 * from the default adapter.
 */
static void FillPresentationParameters(D3DPRESENT_PARAMETERS *d3dpp)
{
    /* Set up the structure used to create the D3DDevice. */
    ZeroMemory(d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->EnableAutoDepthStencil = FALSE;
    d3dpp->hDeviceWindow          = NULL;
    d3dpp->SwapEffect             = D3DSWAPEFFECT_COPY;
    d3dpp->BackBufferCount        = 1;
#if !VLC_WINSTORE_APP
    d3dpp->BackBufferWidth        = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    d3dpp->BackBufferHeight       = GetSystemMetrics(SM_CYVIRTUALSCREEN);
#endif // VLC_WINSTORE_APP
}

int D3D9_ResetDevice(vlc_object_t *o, d3d9_decoder_device_t *dec_dev)
{
    D3DPRESENT_PARAMETERS d3dpp;
    FillPresentationParameters(&d3dpp);

    /* */
    HRESULT hr;
    if (dec_dev->hd3d.use_ex){
        hr = IDirect3DDevice9Ex_ResetEx(dec_dev->d3ddev.devex, &d3dpp, NULL);
    } else {
        hr = IDirect3DDevice9_Reset(dec_dev->d3ddev.dev, &d3dpp);
    }
    if (FAILED(hr)) {
        msg_Err(o, "IDirect3DDevice9_Reset failed! (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void D3D9_Destroy(d3d9_handle_t *hd3d)
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
static int D3D9_Create(vlc_object_t *o, d3d9_handle_t *hd3d)
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

static void D3D9_CloneExternal(d3d9_handle_t *hd3d, IDirect3D9 *dev)
{
    hd3d->obj = dev;
    IDirect3D9_AddRef( hd3d->obj );
    hd3d->hdll = NULL;

    void *pv = NULL;
    hd3d->use_ex = SUCCEEDED(IDirect3D9_QueryInterface(dev, &IID_IDirect3D9Ex, &pv));
    if (hd3d->use_ex && pv)
        IDirect3D9Ex_Release((IDirect3D9Ex*) pv);
}

d3d9_decoder_device_t *(D3D9_CreateDevice)(vlc_object_t *o)
{
    HRESULT hr;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

    d3d9_decoder_device *sys = vlc_obj_malloc(o, sizeof(*sys));
    if (unlikely(sys==NULL))
        return NULL;

d3d9_device_t *out = &sys->dec_device.d3ddev;
d3d9_handle_t *hd3d = &sys->dec_device.hd3d;

    int AdapterToUse;

    sys->cleanupDeviceCb = NULL;
    libvlc_video_engine_t engineType = var_InheritInteger( o, "vout-cb-type" );
    libvlc_video_output_setup_cb setupDeviceCb = NULL;
    if (engineType == libvlc_video_engine_d3d9)
        setupDeviceCb = var_InheritAddress( o, "vout-cb-setup" );
    if ( setupDeviceCb != NULL)
    {
        /* external rendering */
        libvlc_video_setup_device_info_t extern_out = { .d3d9.adapter = -1 };
        sys->opaque          = var_InheritAddress( o, "vout-cb-opaque" );
        sys->cleanupDeviceCb = var_InheritAddress( o, "vout-cb-cleanup" );
        libvlc_video_setup_device_cfg_t cfg = {
            .hardware_decoding = true, /* ignored anyway */
        };
        if (!setupDeviceCb( &sys->opaque, &cfg, &extern_out ))
            goto error;

        D3D9_CloneExternal( hd3d, (IDirect3D9 *) extern_out.d3d9.device );
        AdapterToUse = extern_out.d3d9.adapter;
    }
    else if ( engineType == libvlc_video_engine_disable ||
              engineType == libvlc_video_engine_d3d9 ||
              engineType == libvlc_video_engine_opengl )
    {
        /* internal rendering */
        if (D3D9_Create(o, hd3d) != VLC_SUCCESS)
        {
            msg_Err( o, "Direct3D9 could not be initialized" );
            goto error;
        }
        /* find the best adapter to use, not based on the HWND used */
        AdapterToUse = -1;
    }
    else
        goto error;

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
       goto error;
    }
    msg_Dbg(o, "D3D9 device caps 0x%lX / 0x%lX", out->caps.DevCaps, out->caps.DevCaps2);

    /* TODO: need to test device capabilities and select the right render function */
    if (!(out->caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) {
        msg_Err(o, "Device does not support stretching from textures.");
        goto error;
    }

    out->adapterId = AdapterToUse;

    D3DPRESENT_PARAMETERS d3dpp;
    FillPresentationParameters(&d3dpp);

    /* */
    if (FAILED(IDirect3D9_GetAdapterIdentifier(hd3d->obj, AdapterToUse,0, &out->identifier))) {
        msg_Warn(o, "IDirect3D9_GetAdapterIdentifier failed");
    } else {
        msg_Dbg(o, "Direct3d9 Device: %s %lx %lx %lx", out->identifier.Description,
                out->identifier.VendorId, out->identifier.DeviceId, out->identifier.Revision );
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
                return &sys->dec_device;
            }
        }
    }

    msg_Err(o, "failed to create the D3D9%s device %d/%d. (hr=0x%lX)",
               hd3d->use_ex?"Ex":"", AdapterToUse, DeviceType, hr);

error:
    if ( sys->cleanupDeviceCb )
        sys->cleanupDeviceCb( sys->opaque );
    vlc_obj_free( o, sys );
    return NULL;
}

void D3D9_ReleaseDevice(d3d9_decoder_device_t *dec_dev)
{
    d3d9_decoder_device *sys = container_of(dec_dev, d3d9_decoder_device, dec_device);
    if (dec_dev->d3ddev.dev)
       IDirect3DDevice9_Release(dec_dev->d3ddev.dev);
    D3D9_Destroy( &dec_dev->hd3d );
    if ( sys->cleanupDeviceCb )
        sys->cleanupDeviceCb( sys->opaque );
}

const struct vlc_video_context_operations d3d9_vctx_ops = {
    NULL,
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
    vlc_video_context_Hold(pic_ctx->s.vctx);
    AcquireD3D9PictureSys(&pic_ctx->picsys);
    return &pic_ctx->s;
}
