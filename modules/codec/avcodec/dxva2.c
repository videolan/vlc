/*****************************************************************************
 * va.c: Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_fourcc.h>
#include <vlc_cpu.h>
#include <assert.h>

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#   ifdef HAVE_AVCODEC_DXVA2
#       include <libavcodec/dxva2.h>
#   endif
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "avcodec.h"
#include "va.h"

#ifdef HAVE_AVCODEC_DXVA2

#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <d3d9.h>

/* FIXME */
#define CoTaskMemFree(x)

/* */
#define DXVA2_E_NOT_INITIALIZED     MAKE_HRESULT(1, 4, 4096)
#define DXVA2_E_NEW_VIDEO_DEVICE    MAKE_HRESULT(1, 4, 4097)
#define DXVA2_E_VIDEO_DEVICE_LOCKED MAKE_HRESULT(1, 4, 4098)
#define DXVA2_E_NOT_AVAILABLE       MAKE_HRESULT(1, 4, 4099)

static const GUID DXVA2_ModeMPEG2_MoComp = {
    0xe6a9f44b, 0x61b0, 0x4563, {0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66}
};
static const GUID DXVA2_ModeMPEG2_IDCT = {
  0xbf22ad00, 0x03ea, 0x4690, {0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e}
};
static const GUID DXVA2_ModeMPEG2_VLD = {
   0xee27417f, 0x5e28, 0x4e65, {0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9}
};

static const GUID DXVA2_ModeH264_A = {
    0x1b81be64, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_B = {
    0x1b81be65, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_C = {
    0x1b81be66, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_D = {
    0x1b81be67, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_E = {
    0x1b81be68, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_F = {
    0x1b81be69, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV8_A = {
    0x1b81be80, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV8_B = {
    0x1b81be81, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_A = {
    0x1b81be90, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_B = {
    0x1b81be91, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_C = {
    0x1b81be94, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};

static const GUID DXVA2_ModeVC1_A = {
    0x1b81beA0, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_B = {
    0x1b81beA1, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_C = {
    0x1b81beA2, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_D = {
    0x1b81beA3, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};

/* */
typedef struct {
    const char   *name;
    const GUID   *guid;
    int          codec;
} dxva2_mode_t;
/* XXX Prefered modes must come first */
static const dxva2_mode_t dxva2_modes[] = {
    { "DXVA2_ModeMPEG2_VLD",    &DXVA2_ModeMPEG2_VLD,     0 },
    { "DXVA2_ModeMPEG2_MoComp", &DXVA2_ModeMPEG2_MoComp,  0 },
    { "DXVA2_ModeMPEG2_IDCT",   &DXVA2_ModeMPEG2_IDCT,    0 },

    { "H.264 variable-length decoder (VLD), FGT",               &DXVA2_ModeH264_F, CODEC_ID_H264 },
    { "H.264 VLD, no FGT",                                      &DXVA2_ModeH264_E, CODEC_ID_H264 },
    { "H.264 IDCT, FGT",                                        &DXVA2_ModeH264_D, 0,            },
    { "H.264 inverse discrete cosine transform (IDCT), no FGT", &DXVA2_ModeH264_C, 0,            },
    { "H.264 MoComp, FGT",                                      &DXVA2_ModeH264_B, 0,            },
    { "H.264 motion compensation (MoComp), no FGT",             &DXVA2_ModeH264_A, 0,            },

    { "Windows Media Video 8 MoComp",           &DXVA2_ModeWMV8_B, 0 },
    { "Windows Media Video 8 post processing",  &DXVA2_ModeWMV8_A, 0 },

    {  "Windows Media Video 9 IDCT",            &DXVA2_ModeWMV9_C, 0 },
    {  "Windows Media Video 9 MoComp",          &DXVA2_ModeWMV9_B, 0 },
    {  "Windows Media Video 9 post processing", &DXVA2_ModeWMV9_A, 0 },

    { "VC-1 VLD",             &DXVA2_ModeVC1_D, 0 },
    { "VC-1 IDCT",            &DXVA2_ModeVC1_C, 0 },
    { "VC-1 MoComp",          &DXVA2_ModeVC1_B, 0 },
    { "VC-1 post processing", &DXVA2_ModeVC1_A, 0 },

    { NULL, NULL, 0 }
};

static const dxva2_mode_t *Dxva2FindMode(const GUID *guid)
{
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        if (IsEqualGUID(dxva2_modes[i].guid, guid))
            return &dxva2_modes[i];
    }
    return NULL;
}

/* */
#define VLC_CODEC_NV12 VLC_FOURCC('N','V','1','2') /* TODO move to vlc_fourcc.h */
typedef struct {
    const char   *name;
    D3DFORMAT    format;
    vlc_fourcc_t codec;
} d3d_format_t;
/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "YV12",   MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12 },
    { "NV12",   MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12 },

    { NULL, 0, 0 }
};

static const d3d_format_t *D3dFindFormat(D3DFORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

static const GUID IID_IDirectXVideoDecoderService = {
    0xfc51a551, 0xd5e7, 0x11d9, {0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02}
};
static const GUID IID_IDirectXVideoAccelerationService = {
    0xfc51a550, 0xd5e7, 0x11d9, {0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02}
};

/* */
typedef struct {
    LPDIRECT3DSURFACE9 d3d;
    int                refcount;
    unsigned int       order;
} vlc_va_surface_t;

#define VA_DXVA2_MAX_SURFACE_COUNT (64)
typedef struct
{
    /* */
    vlc_va_t va;

    /* */
    vlc_object_t *log;
    int          codec_id;

    /* DLL */
	HINSTANCE             hd3d9_dll;
    HINSTANCE             hdxva2_dll;

    /* Direct3D */
	D3DPRESENT_PARAMETERS d3dpp;
	LPDIRECT3D9           d3dobj;
	LPDIRECT3DDEVICE9     d3ddev;

    /* Device manager */
    UINT                     token;
    LPDIRECT3DDEVICEMANAGER9 devmng;
    HANDLE                   device;

    /* Video service */
    LPDIRECTXVIDEODECODERSERVICE vs;
    GUID                         input;
    D3DFORMAT                    render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    cfg;
    LPDIRECTXVIDEODECODER        decoder;

    /* Option conversion */
    D3DFORMAT                    output;
    uint8_t                      *surface_cache_base;
    uint8_t                      *surface_cache;
    size_t                       surface_cache_size;

    /* */
    struct dxva_context hw;

    /* */
    unsigned     surface_count;
    unsigned     surface_order;
    int          surface_width;
    int          surface_height;
    vlc_fourcc_t surface_chroma;

    vlc_va_surface_t surface[VA_DXVA2_MAX_SURFACE_COUNT];
    LPDIRECT3DSURFACE9 hw_surface[VA_DXVA2_MAX_SURFACE_COUNT];
} vlc_va_dxva2_t;

/* */
static vlc_va_dxva2_t *vlc_va_dxva2_Get(void *external)
{
    assert(external == (void*)(&((vlc_va_dxva2_t*)external)->va));
    return external;
}

/* */
static int D3dCreateDevice(vlc_va_dxva2_t *);
static void D3dDestroyDevice(vlc_va_dxva2_t *);
static char *DxDescribe(vlc_va_dxva2_t *);

static int D3dCreateDeviceManager(vlc_va_dxva2_t *);
static void D3dDestroyDeviceManager(vlc_va_dxva2_t *);

static int DxCreateVideoService(vlc_va_dxva2_t *);
static void DxDestroyVideoService(vlc_va_dxva2_t *);
static int DxFindVideoServiceConversion(vlc_va_dxva2_t *, GUID *input, D3DFORMAT *output);

static int DxCreateVideoDecoder(vlc_va_dxva2_t *,
                                int codec_id, const video_format_t *);
static void DxDestroyVideoDecoder(vlc_va_dxva2_t *);
static int DxResetVideoDecoder(vlc_va_dxva2_t *);

static void DxCreateVideoConversion(vlc_va_dxva2_t *);
static void DxDestroyVideoConversion(vlc_va_dxva2_t *);

static void CopyFromNv12(picture_t *dst, const D3DLOCKED_RECT *src,
                         uint8_t *cache, size_t cache_size,
                         unsigned width, unsigned height);
static void CopyFromYv12(picture_t *dst, const D3DLOCKED_RECT *src,
                         uint8_t *cache, size_t cache_size,
                         unsigned width, unsigned height);

/* */
static int Setup(vlc_va_t *external, void **hw, vlc_fourcc_t *chroma,
                 int width, int height)
{
    vlc_va_dxva2_t *va = vlc_va_dxva2_Get(external);

    if (va->surface_width  == width &&
        va->surface_height == height)
        goto ok;

    /* */
    DxDestroyVideoConversion(va);
    DxDestroyVideoDecoder(va);

    *hw = NULL;
    *chroma = 0;
    if (width <= 0 || height <= 0)
        return VLC_EGENERIC;

    /* FIXME transmit a video_format_t by VaSetup directly */
    video_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_width = width;
    fmt.i_height = height;

    if (DxCreateVideoDecoder(va, va->codec_id, &fmt))
        return VLC_EGENERIC;
    /* */
    va->hw.decoder = va->decoder;
    if (va->codec_id == CODEC_ID_H264)
        va->hw.cfg = &va->cfg;
    va->hw.surface_count = va->surface_count;
    va->hw.surface = va->hw_surface;
    for (unsigned i = 0; i < va->surface_count; i++)
        va->hw.surface[i] = va->surface[i].d3d;

    /* */
    DxCreateVideoConversion(va);

    /* */
ok:
    *hw = &va->hw;
    const d3d_format_t *output = D3dFindFormat(va->output);
    *chroma = output->codec;

    return VLC_SUCCESS;
}

static int Extract(vlc_va_t *external, picture_t *picture, AVFrame *ff)
{
    vlc_va_dxva2_t *va = vlc_va_dxva2_Get(external);
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)ff->data[3];

    if (!va->surface_cache)
        return VLC_EGENERIC;

    /* */
    assert(va->output == MAKEFOURCC('Y','V','1','2'));

    /* */
    D3DLOCKED_RECT lock;
    if (FAILED(IDirect3DSurface9_LockRect(d3d, &lock, NULL, D3DLOCK_READONLY))) {
        msg_Err(va->log, "Failed to lock surface");
        return VLC_EGENERIC;
    }

    if (va->render == MAKEFOURCC('Y','V','1','2')) {
        CopyFromYv12(picture, &lock,
                     va->surface_cache, va->surface_cache_size,
                     va->surface_width, va->surface_height);
    } else {
        assert(va->render == MAKEFOURCC('N','V','1','2'));
        CopyFromNv12(picture, &lock,
                     va->surface_cache, va->surface_cache_size,
                     va->surface_width, va->surface_height);
    }

    /* */
    IDirect3DSurface9_UnlockRect(d3d);
    return VLC_SUCCESS;
}
/* FIXME it is nearly common with VAAPI */
static int Get(vlc_va_t *external, AVFrame *ff)
{
    vlc_va_dxva2_t *va = vlc_va_dxva2_Get(external);

    /* Check the device */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(va->devmng, va->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
            return VLC_EGENERIC;
    } else if (FAILED(hr)) {
        msg_Err(va->log, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with ffmpeg */
    unsigned i, old;
    for (i = 0, old = 0; i < va->surface_count; i++) {
        vlc_va_surface_t *surface = &va->surface[i];

        if (!surface->refcount)
            break;

        if (surface->order < va->surface[old].order)
            old = i;
    }
    if (i >= va->surface_count)
        i = old;

    vlc_va_surface_t *surface = &va->surface[i];

    surface->refcount = 1;
    surface->order = va->surface_order++;

    /* */
    for (int i = 0; i < 4; i++) {
        ff->data[i] = NULL;
        ff->linesize[i] = 0;

        if (i == 0 || i == 3)
            ff->data[i] = (void*)surface->d3d;/* Yummie */
    }
    return VLC_SUCCESS;
}
static void Release(vlc_va_t *external, AVFrame *ff)
{
    vlc_va_dxva2_t *va = vlc_va_dxva2_Get(external);
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)ff->data[3];

    for (unsigned i = 0; i < va->surface_count; i++) {
        vlc_va_surface_t *surface = &va->surface[i];

        if (surface->d3d == d3d)
            surface->refcount--;
    }
}
static void Close(vlc_va_t *external)
{
    vlc_va_dxva2_t *va = vlc_va_dxva2_Get(external);

    DxDestroyVideoConversion(va);
    DxDestroyVideoDecoder(va);
    DxDestroyVideoService(va);
    D3dDestroyDeviceManager(va);
    D3dDestroyDevice(va);

    if (va->hdxva2_dll)
        FreeLibrary(va->hdxva2_dll);
    if (va->hd3d9_dll)
        FreeLibrary(va->hd3d9_dll);

    free(va->va.description);
    free(va);
}

vlc_va_t *vlc_va_NewDxva2(vlc_object_t *log, int codec_id)
{
    vlc_va_dxva2_t *va = calloc(1, sizeof(*va));
    if (!va)
        return NULL;

    /* */
    va->log = log;
    va->codec_id = codec_id;

    /* Load dll*/
    va->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!va->hd3d9_dll) {
        msg_Warn(va->log, "cannot load d3d9.dll");
        goto error;
    }
    va->hdxva2_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!va->hdxva2_dll) {
        msg_Warn(va->log, "cannot load dxva2.dll");
        goto error;
    }
    msg_Dbg(va->log, "DLLs loaded");

    /* */
    if (D3dCreateDevice(va)) {
        msg_Err(va->log, "Failed to create Direct3D device");
        goto error;
    }
    msg_Dbg(va->log, "D3dCreateDevice succeed");

    if (D3dCreateDeviceManager(va)) {
        msg_Err(va->log, "D3dCreateDeviceManager failed");
        goto error;
    }

    if (DxCreateVideoService(va)) {
        msg_Err(va->log, "DxCreateVideoService failed");
        goto error;
    }

    /* */
    if (DxFindVideoServiceConversion(va, &va->input, &va->render)) {
        msg_Err(va->log, "DxFindVideoServiceConversion failed");
        goto error;
    }

    /* TODO print the hardware name/vendor for debugging purposes */
    va->va.description = DxDescribe(va);
    va->va.setup   = Setup;
    va->va.get     = Get;
    va->va.release = Release;
    va->va.extract = Extract;
    va->va.close   = Close;
    return &va->va;

error:
    Close(&va->va);
    return NULL;
}
/* */

/**
 * It creates a Direct3D device usable for DXVA 2
 */
static int D3dCreateDevice(vlc_va_dxva2_t *va)
{
    /* */
    LPDIRECT3D9 (WINAPI *Create9)(UINT SDKVersion);
    Create9 = (void *)GetProcAddress(va->hd3d9_dll,
                                     TEXT("Direct3DCreate9"));
    if (!Create9) {
        msg_Err(va->log, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        return VLC_EGENERIC;
    }

    /* */
    LPDIRECT3D9 d3dobj;
    d3dobj = Create9(D3D_SDK_VERSION);
    if (!d3dobj) {
        msg_Err(va->log, "Direct3DCreate9 failed");
        return VLC_EGENERIC;
    }
    va->d3dobj = d3dobj;

    /* */
    D3DPRESENT_PARAMETERS *d3dpp = &va->d3dpp;
    ZeroMemory(d3dpp, sizeof(*d3dpp));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->hDeviceWindow          = NULL;
    d3dpp->SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->BackBufferCount        = 0;                  /* FIXME what to put here */
    d3dpp->BackBufferFormat       = D3DFMT_X8R8G8B8;    /* FIXME what to put here */
    d3dpp->BackBufferWidth        = 0;
    d3dpp->BackBufferHeight       = 0;
    d3dpp->EnableAutoDepthStencil = FALSE;

    /* Direct3D needs a HWND to create a device, even without using ::Present
    this HWND is used to alert Direct3D when there's a change of focus window.
    For now, use GetShellWindow, as it looks harmless */
    LPDIRECT3DDEVICE9 d3ddev;
    if (FAILED(IDirect3D9_CreateDevice(d3dobj, D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL, GetShellWindow(),
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                       D3DCREATE_MULTITHREADED,
                                       d3dpp, &d3ddev))) {
        msg_Err(va->log, "IDirect3D9_CreateDevice failed");
        return VLC_EGENERIC;
    }
    va->d3ddev = d3ddev;

    return VLC_SUCCESS;
}
/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_dxva2_t *va)
{
    if (va->d3ddev)
        IDirect3DDevice9_Release(va->d3ddev);
    if (va->d3dobj)
        IDirect3D9_Release(va->d3dobj);
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(vlc_va_dxva2_t *va)
{
    D3DADAPTER_IDENTIFIER9 id;
    ZeroMemory(&id, sizeof(id));

    if (FAILED(IDirect3D9_GetAdapterIdentifier(va->d3dobj,
                                               D3DADAPTER_DEFAULT, 0, &id)))
        return strdup("DXVA2 (unknown)");

    char *description;
    if (asprintf(&description, "DXVA2 (%.*s, vendor %d, device %d, revision %d)",
                 sizeof(id.Description), id.Description,
                 id.VendorId, id.DeviceId, id.Revision) < 0)
        return NULL;
    return description;
}

/**
 * It creates a Direct3D device manager
 */
static int D3dCreateDeviceManager(vlc_va_dxva2_t *va)
{
    HRESULT (WINAPI *CreateDeviceManager9)(UINT *pResetToken,
                                           IDirect3DDeviceManager9 **);
    CreateDeviceManager9 =
      (void *)GetProcAddress(va->hdxva2_dll,
                             TEXT("DXVA2CreateDirect3DDeviceManager9"));

    if (!CreateDeviceManager9) {
        msg_Err(va->log, "cannot load function\n");
        return VLC_EGENERIC;
    }
    msg_Dbg(va->log, "OurDirect3DCreateDeviceManager9 Success!");

    UINT token;
    LPDIRECT3DDEVICEMANAGER9 devmng;
    if (FAILED(CreateDeviceManager9(&token, &devmng))) {
        msg_Err(va->log, " OurDirect3DCreateDeviceManager9 failed");
        return VLC_EGENERIC;
    }
    va->token  = token;
    va->devmng = devmng;
    msg_Info(va->log, "obtained IDirect3DDeviceManager9");

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, va->d3ddev, token);
    if (FAILED(hr)) {
        msg_Err(va->log, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/**
 * It destroys a Direct3D device manager
 */
static void D3dDestroyDeviceManager(vlc_va_dxva2_t *va)
{
    if (va->devmng)
        IDirect3DDeviceManager9_Release(va->devmng);
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_dxva2_t *va)
{
    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(va->hdxva2_dll,
                             TEXT("DXVA2CreateVideoService"));

    if (!CreateVideoService) {
        msg_Err(va->log, "cannot load function\n");
        return 4;
    }
    msg_Info(va->log, "DXVA2CreateVideoService Success!");

    HRESULT hr;

    HANDLE device;
    hr = IDirect3DDeviceManager9_OpenDeviceHandle(va->devmng, &device);
    if (FAILED(hr)) {
        msg_Err(va->log, "OpenDeviceHandle failed");
        return VLC_EGENERIC;
    }
    va->device = device;

    LPDIRECTXVIDEODECODERSERVICE vs;
    hr = IDirect3DDeviceManager9_GetVideoService(va->devmng, device,
                                                 &IID_IDirectXVideoDecoderService,
                                                 &vs);
    if (FAILED(hr)) {
        msg_Err(va->log, "GetVideoService failed");
        return VLC_EGENERIC;
    }
    va->vs = vs;

    return VLC_SUCCESS;
}
/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_dxva2_t *va)
{
    if (va->device)
        IDirect3DDeviceManager9_CloseDeviceHandle(va->devmng, va->device);
    if (va->vs)
        IDirectXVideoDecoderService_Release(va->vs);
}
/**
 * Find the best suited decoder mode GUID and render format.
 */
static int DxFindVideoServiceConversion(vlc_va_dxva2_t *va, GUID *input, D3DFORMAT *output)
{
    /* Retreive supported modes from the decoder service */
    UINT input_count = 0;
    GUID *input_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(va->vs,
                                                                 &input_count,
                                                                 &input_list))) {
        msg_Err(va->log, "IDirectXVideoDecoderService_GetDecoderDeviceGuids failed");
        return VLC_EGENERIC;
    }
    for (unsigned i = 0; i < input_count; i++) {
        const GUID *g = &input_list[i];
        const dxva2_mode_t *mode = Dxva2FindMode(g);
        if (mode) {
            msg_Dbg(va->log, "- '%s' is supported by hardware", mode->name);
        } else {
            msg_Warn(va->log, "- Unknown GUID = %08X-%04x-%04x-XXXX\n",
                     (unsigned)g->Data1, g->Data2, g->Data3);
        }
    }

    /* Try all supported mode by our priority */
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        const dxva2_mode_t *mode = &dxva2_modes[i];
        if (!mode->codec || mode->codec != va->codec_id)
            continue;

        /* */
        bool is_suported = false;
        for (const GUID *g = &input_list[0]; !is_suported && g < &input_list[input_count]; g++) {
            is_suported = IsEqualGUID(mode->guid, g);
        }
        if (!is_suported)
            continue;

        /* */
        msg_Dbg(va->log, "Trying to use '%s' as input", mode->name);
        UINT      output_count = 0;
        D3DFORMAT *output_list = NULL;
        if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(va->vs, mode->guid,
                                                                       &output_count,
                                                                       &output_list))) {
            msg_Err(va->log, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
            continue;
        }
        for (unsigned j = 0; j < output_count; j++) {
            const D3DFORMAT f = output_list[j];
            const d3d_format_t *format = D3dFindFormat(f);
            if (format) {
                msg_Dbg(va->log, "%s is supported for output", format->name);
            } else {
                msg_Dbg(va->log, "%d is supported for output (%4.4s)", f, (const char*)&f);
            }
        }

        /* */
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            bool is_suported = false;
            for (unsigned k = 0; !is_suported && k < output_count; k++) {
                is_suported = format->format == output_list[k];
            }
            if (!is_suported)
                continue;

            /* We have our solution */
            msg_Dbg(va->log, "Using '%s' to decode to '%s'", mode->name, format->name);
            *input  = *mode->guid;
            *output = format->format;
            CoTaskMemFree(output_list);
            CoTaskMemFree(input_list);
            return VLC_SUCCESS;
        }
        CoTaskMemFree(output_list);
    }
    CoTaskMemFree(input_list);
    return VLC_EGENERIC;
}

/**
 * It creates a DXVA2 decoder using the given video format
 */
static int DxCreateVideoDecoder(vlc_va_dxva2_t *va,
                                int codec_id, const video_format_t *fmt)
{
    /* */
    msg_Dbg(va->log, "DxCreateVideoDecoder id %d %dx%d",
            codec_id, fmt->i_width, fmt->i_height);

    /* Allocates all surfaces needed for the decoder */
    switch (codec_id) {
    case CODEC_ID_H264:
        va->surface_count = 16 + 1;
        break;
    default:
        va->surface_count = 2 + 1;
        break;
    }
    LPDIRECT3DSURFACE9 surface_list[VA_DXVA2_MAX_SURFACE_COUNT];
    if (FAILED(IDirectXVideoDecoderService_CreateSurface(va->vs,
                                                         fmt->i_width,
                                                         fmt->i_height,
                                                         va->surface_count - 1,
                                                         va->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         surface_list,
                                                         NULL))) {
        msg_Err(va->log, "IDirectXVideoAccelerationService_CreateSurface failed\n");
        va->surface_count = 0;
        return VLC_EGENERIC;
    }
    for (unsigned i = 0; i < va->surface_count; i++) {
        vlc_va_surface_t *surface = &va->surface[i];
        surface->d3d = surface_list[i];
        surface->refcount = 0;
        surface->order = 0;
    }
    va->surface_width  = fmt->i_width;
    va->surface_height = fmt->i_height;
    msg_Dbg(va->log, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            va->surface_count, fmt->i_width, fmt->i_height);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = fmt->i_width;
    dsc.SampleHeight    = fmt->i_height;
    dsc.Format          = va->render;
    if (fmt->i_frame_rate > 0 && fmt->i_frame_rate_base > 0) {
        dsc.InputSampleFreq.Numerator   = fmt->i_frame_rate;
        dsc.InputSampleFreq.Denominator = fmt->i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
    dsc.UABProtectionLevel = FALSE;
    dsc.Reserved = 0;

    /* FIXME I am unsure we can let unknown everywhere */
    DXVA2_ExtendedFormat *ext = &dsc.SampleFormat;
    ext->SampleFormat = 0;//DXVA2_SampleUnknown;
    ext->VideoChromaSubsampling = 0;//DXVA2_VideoChromaSubsampling_Unknown;
    ext->NominalRange = 0;//DXVA2_NominalRange_Unknown;
    ext->VideoTransferMatrix = 0;//DXVA2_VideoTransferMatrix_Unknown;
    ext->VideoLighting = 0;//DXVA2_VideoLighting_Unknown;
    ext->VideoPrimaries = 0;//DXVA2_VideoPrimaries_Unknown;
    ext->VideoTransferFunction = 0;//DXVA2_VideoTransFunc_Unknown;

    /* List all configurations available for the decoder */
    UINT                      cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderConfigurations(va->vs,
                                                                    &va->input,
                                                                    &dsc,
                                                                    NULL,
                                                                    &cfg_count,
                                                                    &cfg_list))) {
        msg_Err(va->log, "IDirectXVideoDecoderService_GetDecoderConfigurations failed\n");
        return VLC_EGENERIC;
    }
    msg_Dbg(va->log, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    bool has_cfg = false;
    for (unsigned i = 0; i < cfg_count; i++) {
        const DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

        /* */
        msg_Dbg(va->log, "configuration[%d] ConfigBitstreamRaw %d",
                i, cfg->ConfigBitstreamRaw);

        /* */
        if ((!has_cfg && cfg->ConfigBitstreamRaw == 1) ||
            (codec_id == CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)) {
            va->cfg = *cfg;
            has_cfg = true;
        }
    }
    CoTaskMemFree(cfg_list);
    if (!has_cfg) {
        msg_Err(va->log, "Failed to find a supported decoder configuration");
        return VLC_EGENERIC;
    }

    /* Create the decoder */
    LPDIRECTXVIDEODECODER decoder;
    if (FAILED(IDirectXVideoDecoderService_CreateVideoDecoder(va->vs,
                                                              &va->input,
                                                              &dsc,
                                                              &va->cfg,
                                                              surface_list,
                                                              va->surface_count,
                                                              &decoder))) {
        msg_Err(va->log, "IDirectXVideoDecoderService_CreateVideoDecoder failed\n");
        return VLC_EGENERIC;
    }
    va->decoder = decoder;
    msg_Dbg(va->log, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return VLC_SUCCESS;
}
static void DxDestroyVideoDecoder(vlc_va_dxva2_t *va)
{
    if (va->decoder)
        IDirectXVideoDecoder_Release(va->decoder);
    va->decoder = NULL;

    for (unsigned i = 0; i < va->surface_count; i++)
        IDirect3DSurface9_Release(va->surface[i].d3d);
    va->surface_count = 0;
}
static int DxResetVideoDecoder(vlc_va_dxva2_t *va)
{
    msg_Err(va->log, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}

static void DxCreateVideoConversion(vlc_va_dxva2_t *va)
{
    switch (va->render) {
    case MAKEFOURCC('N','V','1','2'):
        va->output = MAKEFOURCC('Y','V','1','2');
        break;
    default:
        va->output = va->render;
        break;
    }
    va->surface_cache_size = __MAX((va->surface_width + 0x0f) & ~ 0x0f, 4096);
    va->surface_cache_base = malloc(16 + va->surface_cache_size);
    va->surface_cache      = &va->surface_cache_base[16 - ((intptr_t)va->surface_cache_base & 0x0f)];
}
static void DxDestroyVideoConversion(vlc_va_dxva2_t *va)
{
    free(va->surface_cache_base);
    va->surface_cache_base = NULL;
    va->surface_cache      = NULL;
    va->surface_cache_size = 0;
}

/* Copy 64 bytes from srcp to dsp loading data with the SSE>=2 instruction load and
 * storing data with the SSE>=2 instruction store.
 */
#define COPY64(dstp, srcp, load, store) \
    asm volatile (                      \
        load "  0(%[src]), %%xmm1\n"    \
        load " 16(%[src]), %%xmm2\n"    \
        load " 32(%[src]), %%xmm3\n"    \
        load " 48(%[src]), %%xmm4\n"    \
        store " %%xmm1,    0(%[dst])\n" \
        store " %%xmm2,   16(%[dst])\n" \
        store " %%xmm3,   32(%[dst])\n" \
        store " %%xmm4,   48(%[dst])\n" \
        : : [dst]"r"(dstp), [src]"r"(srcp) : "memory")

/* Execute the instruction op only if SSE2 is supported. */
#ifdef CAN_COMPILE_SSE2
#   define ASM_SSE2(cpu, op) do {          \
        if (cpu & CPU_CAPABILITY_SSE2)  \
            asm volatile (op);    \
    } while (0)
#else
#   define ASM_SSE2(cpu, op)
#endif

/* Optimized copy from "Uncacheable Speculative Write Combining" memory
 * as used by some video surface.
 * XXX It is really efficient only when SSE4.1 is available.
 */
static void CopyFromUswc(uint8_t *dst, size_t dst_pitch,
                         const uint8_t *src, size_t src_pitch,
                         unsigned unaligned,
                         unsigned width, unsigned height,
                         unsigned cpu)
{
    assert(((intptr_t)dst & 0x0f) == 0 && (dst_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");
    for (unsigned y = 0; y < height; y++) {
        unsigned x;

        for (x = 0; x < unaligned; x++)
            dst[x] = src[x];

#ifdef CAN_COMPILE_SSE4_1
        if (cpu & CPU_CAPABILITY_SSE4_1) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqa");
            } else {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqu");
            }
        } else
#endif
#ifdef CAN_COMPILE_SSE2
        if (cpu & CPU_CAPABILITY_SSE2) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqa");
            } else {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqu");
            }
        }
#endif

        for (; x < width; x++)
            dst[x] = src[x];

        src += src_pitch;
        dst += dst_pitch;
    }
}

static void Copy2d(uint8_t *dst, size_t dst_pitch,
                   const uint8_t *src, size_t src_pitch,
                   unsigned width, unsigned height,
                   unsigned cpu)
{
    assert(((intptr_t)src & 0x0f) == 0 && (src_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;
        bool unaligned = ((intptr_t)dst & 0x0f) != 0;

#ifdef CAN_COMPILE_SSE2
        if (cpu & CPU_CAPABILITY_SSE2) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movntdq");
            } else {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqu");
            }
        }
#endif

        for (; x < width; x++)
            dst[x] = src[x];

        src += src_pitch;
        dst += dst_pitch;
    }
}

static void SplitUV(uint8_t *dstu, size_t dstu_pitch,
                    uint8_t *dstv, size_t dstv_pitch,
                    const uint8_t *src, size_t src_pitch,
                    unsigned width, unsigned height, unsigned cpu)
{
    const uint8_t shuffle[] = { 0, 2, 4, 6, 8, 10, 12, 14,
                                1, 3, 5, 7, 9, 11, 13, 15 };
    const uint8_t mask[] = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
                             0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };

    assert(((intptr_t)src & 0x0f) == 0 && (src_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;

#define LOAD64 \
    "movdqa  0(%[src]), %%xmm0\n" \
    "movdqa 16(%[src]), %%xmm1\n" \
    "movdqa 32(%[src]), %%xmm2\n" \
    "movdqa 48(%[src]), %%xmm3\n"

#define STORE2X32 \
    "movq   %%xmm0,   0(%[dst1])\n" \
    "movq   %%xmm1,   8(%[dst1])\n" \
    "movhpd %%xmm0,   0(%[dst2])\n" \
    "movhpd %%xmm1,   8(%[dst2])\n" \
    "movq   %%xmm2,  16(%[dst1])\n" \
    "movq   %%xmm3,  24(%[dst1])\n" \
    "movhpd %%xmm2,  16(%[dst2])\n" \
    "movhpd %%xmm3,  24(%[dst2])\n"

#ifdef CAN_COMPILE_SSSE3
        if (cpu & CPU_CAPABILITY_SSSE3) {
            for (x = 0; x < (width & ~31); x += 32) {
                asm volatile (
                    "movdqu (%[shuffle]), %%xmm7\n"
                    LOAD64
                    "pshufb  %%xmm7, %%xmm0\n"
                    "pshufb  %%xmm7, %%xmm1\n"
                    "pshufb  %%xmm7, %%xmm2\n"
                    "pshufb  %%xmm7, %%xmm3\n"
                    STORE2X32
                    : : [dst1]"r"(&dstu[x]), [dst2]"r"(&dstv[x]), [src]"r"(&src[2*x]), [shuffle]"r"(shuffle) : "memory");
            }
        } else
#endif
#ifdef CAN_COMPILE_SSE2
        if (cpu & CPU_CAPABILITY_SSE2) {
            for (x = 0; x < (width & ~31); x += 32) {
                asm volatile (
                    "movdqu (%[mask]), %%xmm7\n"
                    LOAD64
                    "movdqa   %%xmm0, %%xmm4\n"
                    "movdqa   %%xmm1, %%xmm5\n"
                    "movdqa   %%xmm2, %%xmm6\n"
                    "psrlw    $8,     %%xmm0\n"
                    "psrlw    $8,     %%xmm1\n"
                    "pand     %%xmm7, %%xmm4\n"
                    "pand     %%xmm7, %%xmm5\n"
                    "pand     %%xmm7, %%xmm6\n"
                    "packuswb %%xmm4, %%xmm0\n"
                    "packuswb %%xmm5, %%xmm1\n"
                    "pand     %%xmm3, %%xmm7\n"
                    "psrlw    $8,     %%xmm2\n"
                    "psrlw    $8,     %%xmm3\n"
                    "packuswb %%xmm6, %%xmm2\n"
                    "packuswb %%xmm7, %%xmm3\n"
                    STORE2X32
                    : : [dst2]"r"(&dstu[x]), [dst1]"r"(&dstv[x]), [src]"r"(&src[2*x]), [mask]"r"(mask) : "memory");
            }
        }
#endif
#undef STORE2X32
#undef LOAD64

        for (; x < width; x++) {
            dstu[x] = src[2*x+0];
            dstv[x] = src[2*x+1];
        }
        src  += src_pitch;
        dstu += dstu_pitch;
        dstv += dstv_pitch;
    }
}

static void CopyPlane(uint8_t *dst, size_t dst_pitch, const uint8_t *src, size_t src_pitch,
                      uint8_t *cache, size_t cache_size,
                      unsigned width, unsigned height,
                      unsigned cpu)
{
    const unsigned w16 = (width+15) & ~15;
    const unsigned hstep = cache_size / w16;
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned unaligned = (intptr_t)src & 0x0f;
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16,
                     src, src_pitch,
                     unaligned,
                     width, hblock, cpu);

        /* Copy from our cache to the destination */
        Copy2d(dst, dst_pitch,
               cache, w16,
               width, hblock, cpu);

        /* */
        src += src_pitch * hblock;
        dst += dst_pitch * hblock;
    }

    ASM_SSE2(cpu, "mfence");
}
static void SplitPlanes(uint8_t *dstu, size_t dstu_pitch,
                        uint8_t *dstv, size_t dstv_pitch,
                        const uint8_t *src, size_t src_pitch,
                        uint8_t *cache, size_t cache_size,
                        unsigned width, unsigned height,
                        unsigned cpu)
{
    const unsigned w2_16 = (2*width+15) & ~15;
    const unsigned hstep = cache_size / w2_16;
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned unaligned = (intptr_t)src & 0x0f;
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w2_16,
                     src, src_pitch,
                     unaligned,
                     2*width, hblock, cpu);

        /* Copy from our cache to the destination */
        SplitUV(dstu, dstu_pitch,
                dstv, dstv_pitch,
                cache, w2_16,
                width, hblock, cpu);

        /* */
        src  += src_pitch  * hblock;
        dstu += dstu_pitch * hblock;
        dstv += dstv_pitch * hblock;
    }

    ASM_SSE2(cpu, "mfence");
}

static void CopyFromNv12(picture_t *dst, const D3DLOCKED_RECT *src,
                         uint8_t *cache, size_t cache_size,
                         unsigned width, unsigned height)
{
    const unsigned cpu = vlc_CPU();

    /* */
    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src->pBits, src->Pitch,
              cache, cache_size,
              width, height, cpu);
    SplitPlanes(dst->p[2].p_pixels, dst->p[2].i_pitch,
                dst->p[1].p_pixels, dst->p[1].i_pitch,
                (const uint8_t*)src->pBits + src->Pitch * height, src->Pitch,
                cache, cache_size,
                width/2, height/2, cpu);

    ASM_SSE2(cpu, "emms");
}
static void CopyFromYv12(picture_t *dst, const D3DLOCKED_RECT *src,
                         uint8_t *cache, size_t cache_size,
                         unsigned width, unsigned height)
{
    const unsigned cpu = vlc_CPU();

    /* */
    for (unsigned n = 0, offset = 0; n < 3; n++) {
        const unsigned d = n > 0 ? 2 : 1;
        CopyPlane(dst->p[n].p_pixels, dst->p[n].i_pitch,
                  (const uint8_t*)src->pBits + offset, src->Pitch/d,
                  cache, cache_size,
                  width/d, height/d, cpu);
        offset += (src->Pitch/d) * (height/d);
    }
    ASM_SSE2(cpu, "emms");
}
#undef ASM_SSE2
#undef COPY64

#else
vlc_va_t *vlc_va_NewDxva2(vlc_object_t *log, int codec_id)
{
    (void)log;
    (void)codec_id;
    return NULL;
}
#endif
