/*****************************************************************************
 * direct3d11.c: Windows Direct3D11 video output module
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>
 *          Steve Lhomme <robux4@gmail.com>
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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x601
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x601
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <assert.h>
#include <math.h>

#define COBJMACROS
#define INITGUID
#include <d3d11.h>

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "common.h"

#include "../../video_chroma/dxgi_fmt.h"

#if !VLC_WINSTORE_APP
# if USE_DXGI
#  define D3D11CreateDeviceAndSwapChain(args...) sys->OurD3D11CreateDeviceAndSwapChain(args)
# else
#  define D3D11CreateDevice(args...)             sys->OurD3D11CreateDevice(args)
# endif
# define D3DCompile(args...)                    sys->OurD3DCompile(args)
#endif

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);
DEFINE_GUID(GUID_CONTEXT_MUTEX,    0x472e8835, 0x3f8e, 0x4f93, 0xa0, 0xcb, 0x25, 0x79, 0x77, 0x6c, 0xed, 0x86);

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

#define D3D11_HELP N_("Recommended video output for Windows 8 and later versions")
#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitle/OSD blending.")

vlc_module_begin ()
    set_shortname("Direct3D11")
    set_description(N_("Direct3D11 video output"))
    set_help(D3D11_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d11-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)

#if VLC_WINSTORE_APP
    add_integer("winrt-d3ddevice",     0x0, NULL, NULL, true); /* ID3D11Device*        */
    add_integer("winrt-d3dcontext",    0x0, NULL, NULL, true); /* ID3D11DeviceContext* */
    add_integer("winrt-swapchain",     0x0, NULL, NULL, true); /* IDXGISwapChain1*     */
#endif

    set_capability("vout display", 240)
    add_shortcut("direct3d11")
    set_callbacks(Open, Close)
vlc_module_end ()

#ifdef HAVE_ID3D11VIDEODECODER
/* VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    ID3D11Texture2D               *texture;
    ID3D11DeviceContext           *context;
    unsigned                      slice_index;
};
#endif

/* internal picture_t pool  */
typedef struct
{
    ID3D11Texture2D               *texture;
    vout_display_t                *vd;
} picture_sys_pool_t;

/* matches the D3D11_INPUT_ELEMENT_DESC we setup */
typedef struct d3d_vertex_t {
    struct {
        FLOAT x;
        FLOAT y;
        FLOAT z;
    } position;
    struct {
        FLOAT x;
        FLOAT y;
    } texture;
} d3d_vertex_t;

typedef struct {
    FLOAT Opacity;
    FLOAT padding[3];
} PS_CONSTANT_BUFFER;

typedef struct {
    FLOAT RotX[16];
    FLOAT RotY[16];
    FLOAT RotZ[16];
    FLOAT View[16];
    FLOAT Projection[16];
} VS_PROJECTION_CONST;

#define SPHERE_RADIUS 1.f

#define RECTWidth(r)   (int)((r).right - (r).left)
#define RECTHeight(r)  (int)((r).bottom - (r).top)

static picture_pool_t *Pool(vout_display_t *vd, unsigned count);

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void Display(vout_display_t *, picture_t *, subpicture_t *subpicture);

static HINSTANCE Direct3D11LoadShaderLibrary(void);
static void Direct3D11Destroy(vout_display_t *);

static int  Direct3D11Open (vout_display_t *, video_format_t *);
static void Direct3D11Close(vout_display_t *);

static int  Direct3D11CreateResources (vout_display_t *, video_format_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static int  Direct3D11CreatePool (vout_display_t *, video_format_t *);
static void Direct3D11DestroyPool(vout_display_t *);

static void DestroyDisplayPicture(picture_t *);
static void DestroyDisplayPoolPicture(picture_t *);
static int  Direct3D11MapTexture(picture_t *);
static int  Direct3D11UnmapTexture(picture_t *);
static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static int AllocQuad(vout_display_t *, const video_format_t *, d3d_quad_t *,
                     d3d_quad_cfg_t *, ID3D11PixelShader *, bool b_visible,
                     video_projection_mode_t);
static void ReleaseQuad(d3d_quad_t *);
static void UpdatePicQuadPosition(vout_display_t *);
static void UpdateQuadOpacity(vout_display_t *, const d3d_quad_t *, float);

static int Control(vout_display_t *vd, int query, va_list args);
static void Manage(vout_display_t *vd);

/* All the #if USE_DXGI contain an alternative method to setup dx11
   They both need to be benchmarked to see which performs better */
#if USE_DXGI
/* I have no idea why MS decided dxgi headers do not define this
   As they do have prototypes for d3d11 functions */
typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY)(REFIID riid, void **ppFactory);
#endif

/* TODO: Move to a direct3d11_shaders header */
static const char* globVertexShaderFlat = "\
  struct VS_INPUT\
  {\
    float4 Position   : POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  struct VS_OUTPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  VS_OUTPUT VS( VS_INPUT In )\
  {\
    return In;\
  }\
";

static const char* globVertexShaderProjection = "\
  cbuffer VS_PROJECTION_CONST : register(b0)\
  {\
     float4x4 RotX;\
     float4x4 RotY;\
     float4x4 RotZ;\
     float4x4 View;\
     float4x4 Projection;\
  };\
  struct VS_INPUT\
  {\
    float4 Position   : POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  struct VS_OUTPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  VS_OUTPUT VS( VS_INPUT In )\
  {\
    VS_OUTPUT Output;\
    float4 pos = In.Position;\
    pos = mul(RotY, pos);\
    pos = mul(RotX, pos);\
    pos = mul(RotZ, pos);\
    pos = mul(View, pos);\
    pos = mul(Projection, pos);\
    Output.Position = pos;\
    Output.Texture = In.Texture;\
    return Output;\
  }\
";

static const char* globPixelShaderDefault = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTexture;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float4 rgba; \
    \
    rgba = shaderTexture.Sample(SampleType, In.Texture);\
    rgba.a = rgba.a * Opacity;\
    return rgba; \
  }\
";

static const char *globPixelShaderBiplanarYUV_BT601_2RGB = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTextureY;\
  Texture2D shaderTextureUV;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float3 yuv;\
    float4 rgba;\
    yuv.x  = shaderTextureY.Sample(SampleType, In.Texture).x;\
    yuv.yz = shaderTextureUV.Sample(SampleType, In.Texture).xy;\
    yuv.x  = 1.164383561643836 * (yuv.x-0.0625);\
    yuv.y  = yuv.y - 0.5;\
    yuv.z  = yuv.z - 0.5;\
    rgba.x = saturate(yuv.x + 1.596026785714286 * yuv.z);\
    rgba.y = saturate(yuv.x - 0.812967647237771 * yuv.z - 0.391762290094914 * yuv.y);\
    rgba.z = saturate(yuv.x + 2.017232142857142 * yuv.y);\
    rgba.a = Opacity;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarYUV_BT709_2RGB = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTextureY;\
  Texture2D shaderTextureUV;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float3 yuv;\
    float4 rgba;\
    yuv.x  = shaderTextureY.Sample(SampleType, In.Texture).x;\
    yuv.yz = shaderTextureUV.Sample(SampleType, In.Texture).xy;\
    yuv.x  = 1.164383561643836 * (yuv.x-0.0625);\
    yuv.y  = yuv.y - 0.5;\
    yuv.z  = yuv.z - 0.5;\
    rgba.x = saturate(yuv.x + 1.792741071428571 * yuv.z);\
    rgba.y = saturate(yuv.x - 0.532909328559444 * yuv.z - 0.21324861427373 * yuv.y);\
    rgba.z = saturate(yuv.x + 2.112401785714286 * yuv.y);\
    rgba.a = Opacity;\
    return rgba;\
  }\
";

/* RGB-709 to RGB-2020 based on https://www.researchgate.net/publication/258434326_Beyond_BT709 */
static const char *globPixelShaderBiplanarYUV_BT2020_2RGB = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTextureY;\
  Texture2D shaderTextureUV;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float3 yuv;\
    float4 rgba;\
    yuv.x  = shaderTextureY.Sample(SampleType, In.Texture).x;\
    yuv.yz = shaderTextureUV.Sample(SampleType, In.Texture).xy;\
    yuv.x  = 1.164383561643836 * (yuv.x-0.0625);\
    yuv.y  = yuv.y - 0.5;\
    yuv.z  = yuv.z - 0.5;\
    rgba.x = yuv.x + 1.792741071428571 * yuv.z;\
    rgba.y = yuv.x - 0.532909328559444 * yuv.z - 0.21324861427373 * yuv.y;\
    rgba.z = yuv.x + 2.112401785714286 * yuv.y;\
    rgba.x = saturate( 1.661 * rgba.x - 0.588 * rgba.y - 0.073 * rgba.z);\
    rgba.y = saturate(-0.125 * rgba.x + 1.133 * rgba.y - 0.008 * rgba.z);\
    rgba.z = saturate(-0.018 * rgba.x - 0.101 * rgba.y + 1.119 * rgba.z);\
    rgba.a = Opacity;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarYUYV_BT709_2RGB = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTextureYUYV;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float3 yuv;\
    float4 rgba;\
    yuv.x  = shaderTextureYUYV.Sample(SampleType, In.Texture).x;\
    yuv.y  = shaderTextureYUYV.Sample(SampleType, In.Texture).y;\
    yuv.z  = shaderTextureYUYV.Sample(SampleType, In.Texture).a;\
    yuv.x  = 1.164383561643836 * (yuv.x-0.0625);\
    yuv.y  = yuv.y - 0.5;\
    yuv.z  = yuv.z - 0.5;\
    rgba.x = saturate(yuv.x + 1.792741071428571 * yuv.z);\
    rgba.y = saturate(yuv.x - 0.532909328559444 * yuv.z - 0.21324861427373 * yuv.y);\
    rgba.z = saturate(yuv.x + 2.112401785714286 * yuv.y);\
    rgba.a = Opacity;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarYUYV_BT601_2RGB = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\
  {\
    float Opacity;\
    float ignoreA;\
    float ignoreB;\
    float ignoreC;\
  };\
  Texture2D shaderTextureYUYV;\
  SamplerState SampleType;\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float3 yuv;\
    float4 rgba;\
    yuv.x  = shaderTextureYUYV.Sample(SampleType, In.Texture).x;\
    yuv.y  = shaderTextureYUYV.Sample(SampleType, In.Texture).y;\
    yuv.z  = shaderTextureYUYV.Sample(SampleType, In.Texture).a;\
    yuv.x  = 1.164383561643836 * (yuv.x-0.0625);\
    yuv.y  = yuv.y - 0.5;\
    yuv.z  = yuv.z - 0.5;\
    rgba.x = saturate(yuv.x + 1.596026785714286 * yuv.z);\
    rgba.y = saturate(yuv.x - 0.812967647237771 * yuv.z - 0.391762290094914 * yuv.y);\
    rgba.z = saturate(yuv.x + 2.017232142857142 * yuv.y);\
    rgba.a = Opacity;\
    return rgba;\
  }\
";

#if !VLC_WINSTORE_APP
static int OpenHwnd(vout_display_t *vd)
{
    HINSTANCE hd3d11_dll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!hd3d11_dll) {
        msg_Warn(vd, "cannot load d3d11.dll, aborting");
        return VLC_EGENERIC;
    }

    HINSTANCE hd3dcompiler_dll = Direct3D11LoadShaderLibrary();
    if (!hd3dcompiler_dll) {
        msg_Err(vd, "cannot load d3dcompiler.dll, aborting");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

# if USE_DXGI
    HINSTANCE hdxgi_dll = LoadLibrary(TEXT("DXGI.DLL"));
    if (!hdxgi_dll) {
        msg_Warn(vd, "cannot load dxgi.dll, aborting");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }
# endif

    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    sys->hd3d11_dll       = hd3d11_dll;
    sys->hd3dcompiler_dll = hd3dcompiler_dll;

    sys->OurD3DCompile = (void *)GetProcAddress(sys->hd3dcompiler_dll, "D3DCompile");
    if (!sys->OurD3DCompile) {
        msg_Err(vd, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

# if USE_DXGI
    sys->hdxgi_dll = hdxgi_dll;

    /* TODO : enable all dxgi versions from 1.3 -> 1.1 */
    PFN_CREATE_DXGI_FACTORY OurCreateDXGIFactory =
        (void *)GetProcAddress(hdxgi_dll, "CreateDXGIFactory");
    if (!OurCreateDXGIFactory) {
        msg_Err(vd, "Cannot locate reference to CreateDXGIFactory in dxgi DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

    UINT i_factory_flags = 0;
#ifndef NDEBUG
    i_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    /* TODO : detect the directx version supported and use IID_IDXGIFactory3 or 2 */
    HRESULT hr = OurCreateDXGIFactory(&IID_IDXGIFactory2, (void **)&sys->dxgifactory);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not create dxgi factory. (hr=0x%lX)", hr);
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

    sys->OurD3D11CreateDeviceAndSwapChain =
        (void *)GetProcAddress(hd3d11_dll, "D3D11CreateDeviceAndSwapChain");
    if (!sys->OurD3D11CreateDeviceAndSwapChain) {
        msg_Err(vd, "Cannot locate reference to D3D11CreateDeviceAndSwapChain in d3d11 DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

# else
    sys->OurD3D11CreateDevice =
        (void *)GetProcAddress(hd3d11_dll, "D3D11CreateDevice");
    if (!sys->OurD3D11CreateDevice) {
        msg_Err(vd, "Cannot locate reference to D3D11CreateDevice in d3d11 DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }
# endif
    return VLC_SUCCESS;
}
#else
static int OpenCoreW(vout_display_t *vd)
{
    IDXGISwapChain1* dxgiswapChain  = var_InheritInteger(vd, "winrt-swapchain");
    if (!dxgiswapChain)
        return VLC_EGENERIC;
    ID3D11Device* d3ddevice         = var_InheritInteger(vd, "winrt-d3ddevice");
    if (!d3ddevice)
        return VLC_EGENERIC;
    ID3D11DeviceContext* d3dcontext = var_InheritInteger(vd, "winrt-d3dcontext");
    if (!d3dcontext)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    sys->dxgiswapChain = dxgiswapChain;
    sys->d3ddevice     = d3ddevice;
    sys->d3dcontext    = d3dcontext;
    IDXGISwapChain_AddRef     (sys->dxgiswapChain);
    ID3D11Device_AddRef       (sys->d3ddevice);
    ID3D11DeviceContext_AddRef(sys->d3dcontext);

    return VLC_SUCCESS;
}
#endif

static bool is_d3d11_opaque(vlc_fourcc_t chroma)
{
    switch (chroma)
    {
    case VLC_CODEC_D3D11_OPAQUE:
    case VLC_CODEC_D3D11_OPAQUE_10B:
        return true;
    default:
        return false;
    }
}

static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

#if !VLC_WINSTORE_APP
    int ret = OpenHwnd(vd);
#else
    int ret = OpenCoreW(vd);
#endif

    if (ret != VLC_SUCCESS)
        return ret;

    if (CommonInit(vd))
        goto error;

    video_format_t fmt;
    if (Direct3D11Open(vd, &fmt)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vout_display_info_t info  = vd->info;
    info.is_slow              = !is_d3d11_opaque(fmt.i_chroma);
    info.has_double_click     = true;
    info.has_hide_mouse       = false;
    info.has_event_thread     = true;
    info.has_pictures_invalid = !is_d3d11_opaque(fmt.i_chroma);

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        vd->sys->d3dregion_format != DXGI_FORMAT_UNKNOWN)
        info.subpicture_chromas = vd->sys->pSubpictureChromas;
    else
        info.subpicture_chromas = NULL;

    video_format_Clean(&vd->fmt);
    video_format_Copy(&vd->fmt, &fmt);
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;

error:
    Close(object);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    vout_display_t * vd = (vout_display_t *)object;

    Direct3D11Close(vd);
    CommonClean(vd);
    Direct3D11Destroy(vd);
    free(vd->sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned pool_size)
{
    if ( vd->sys->pool != NULL )
        return vd->sys->pool;

    if (pool_size > 30) {
        msg_Err(vd, "Avoid crashing when using ID3D11VideoDecoderOutputView with too many slices");
        return NULL;
    }

#ifdef HAVE_ID3D11VIDEODECODER
    picture_t**       pictures = NULL;
    unsigned          picture_count = 0;
    HRESULT           hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( vd->sys->d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
        ID3D10Multithread_Release(pMultithread);
    }

    pictures = calloc(pool_size, sizeof(*pictures));
    if (!pictures)
        goto error;

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = vd->fmt.i_width;
    texDesc.Height = vd->fmt.i_height;
    texDesc.MipLevels = 1;
    texDesc.Format = vd->sys->picQuadConfig.textureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DECODER;
    texDesc.CPUAccessFlags = 0;

    texDesc.ArraySize = pool_size;

    ID3D11Texture2D *texture;
    hr = ID3D11Device_CreateTexture2D( vd->sys->d3ddevice, &texDesc, NULL, &texture );
    if (FAILED(hr)) {
        msg_Err(vd, "CreateTexture2D failed for the %d pool. (hr=0x%0lx)", pool_size, hr);
        goto error;
    }

    for (picture_count = 0; picture_count < pool_size; picture_count++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        ID3D11Texture2D_AddRef(texture);
        picsys->texture = texture;
        picsys->slice_index = picture_count;
        picsys->context = vd->sys->d3dcontext;

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyDisplayPoolPicture,
        };

        picture_t *picture = picture_NewFromResource(&vd->fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            msg_Err( vd, "Failed to create picture %d in the pool.", picture_count );
            goto error;
        }

        pictures[picture_count] = picture;
        /* each picture_t holds a ref to the context and release it on Destroy */
        ID3D11DeviceContext_AddRef(picsys->context);
    }
    ID3D11Texture2D_Release(texture);

    msg_Dbg(vd, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d) texture 0x%p context 0x%p",
            pool_size, vd->fmt.i_width, vd->fmt.i_height, texture, vd->sys->d3dcontext);

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = pool_size;
    pool_cfg.picture       = pictures;

    vd->sys->pool = picture_pool_NewExtended( &pool_cfg );

error:
    if (vd->sys->pool ==NULL && pictures) {
        msg_Dbg(vd, "Failed to create the picture d3d11 pool");
        for (unsigned i=0;i<picture_count; ++i)
            DestroyDisplayPoolPicture(pictures[i]);
        free(pictures);

        /* create an empty pool to avoid crashing */
        picture_pool_configuration_t pool_cfg;
        memset( &pool_cfg, 0, sizeof( pool_cfg ) );
        pool_cfg.picture_count = 0;

        vd->sys->pool = picture_pool_NewExtended( &pool_cfg );
    }
#endif
    return vd->sys->pool;
}

#ifdef HAVE_ID3D11VIDEODECODER
static void DestroyDisplayPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = (picture_sys_t*) picture->p_sys;

    if (p_sys->texture)
        ID3D11Texture2D_Release(p_sys->texture);
    if (p_sys->context)
        ID3D11DeviceContext_Release(p_sys->context);

    free(p_sys);
    free(picture);
}
#endif

static void DestroyDisplayPicture(picture_t *picture)
{
    picture_sys_pool_t *p_sys = (picture_sys_pool_t*) picture->p_sys;

    if (p_sys->texture)
        ID3D11Texture2D_Release(p_sys->texture);

    free(p_sys);
    free(picture);
}

static HRESULT UpdateBackBuffer(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    ID3D11Texture2D* pDepthStencil;
    ID3D11Texture2D* pBackBuffer;
    uint32_t i_width  = RECTWidth(sys->rect_dest_clipped);
    uint32_t i_height = RECTHeight(sys->rect_dest_clipped);
#if VLC_WINSTORE_APP
    UINT dataSize = sizeof(i_width);
    hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
    if (FAILED(hr)) {
        msg_Err(vd, "Can't get swapchain width, size %d. (hr=0x%lX)", hr, dataSize);
        return hr;
    }
    dataSize = sizeof(i_height);
    hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_HEIGHT, &dataSize, &i_height);
    if (FAILED(hr)) {
        msg_Err(vd, "Can't get swapchain height, size %d. (hr=0x%lX)", hr, dataSize);
        return hr;
    }
#endif

    if (sys->d3drenderTargetView) {
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
        sys->d3drenderTargetView = NULL;
    }
    if (sys->d3ddepthStencilView) {
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
        sys->d3ddepthStencilView = NULL;
    }

    hr = IDXGISwapChain_ResizeBuffers(sys->dxgiswapChain, 0, i_width, i_height,
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
       msg_Err(vd, "Failed to resize the backbuffer. (hr=0x%lX)", hr);
       return hr;
    }

    hr = IDXGISwapChain_GetBuffer(sys->dxgiswapChain, 0, &IID_ID3D11Texture2D, (LPVOID *)&pBackBuffer);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the backbuffer for the Swapchain. (hr=0x%lX)", hr);
       return hr;
    }

    hr = ID3D11Device_CreateRenderTargetView(sys->d3ddevice, (ID3D11Resource *)pBackBuffer, NULL, &sys->d3drenderTargetView);
    ID3D11Texture2D_Release(pBackBuffer);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create the target view. (hr=0x%lX)", hr);
        return hr;
    }

    D3D11_TEXTURE2D_DESC deptTexDesc;
    memset(&deptTexDesc, 0,sizeof(deptTexDesc));
    deptTexDesc.ArraySize = 1;
    deptTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    deptTexDesc.CPUAccessFlags = 0;
    deptTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    deptTexDesc.Width = i_width;
    deptTexDesc.Height = i_height;
    deptTexDesc.MipLevels = 1;
    deptTexDesc.MiscFlags = 0;
    deptTexDesc.SampleDesc.Count = 1;
    deptTexDesc.SampleDesc.Quality = 0;
    deptTexDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = ID3D11Device_CreateTexture2D(sys->d3ddevice, &deptTexDesc, NULL, &pDepthStencil);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil texture. (hr=0x%lX)", hr);
       return hr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
    memset(&depthViewDesc, 0, sizeof(depthViewDesc));

    depthViewDesc.Format = deptTexDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = ID3D11Device_CreateDepthStencilView(sys->d3ddevice, (ID3D11Resource *)pDepthStencil, &depthViewDesc, &sys->d3ddepthStencilView);
    ID3D11Texture2D_Release(pDepthStencil);

    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil view. (hr=0x%lX)", hr);
       return hr;
    }

    return S_OK;
}

/* rotation around the Z axis */
static void getZRotMatrix(float theta, FLOAT matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const FLOAT m[] = {
    /*  x    y    z    w */
        ct,  -st, 0.f, 0.f,
        st,  ct,  0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the Y axis */
static void getYRotMatrix(float theta, FLOAT matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const FLOAT m[] = {
    /*  x    y    z    w */
        ct,  0.f, -st, 0.f,
        0.f, 1.f, 0.f, 0.f,
        st,  0.f, ct,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the X axis */
static void getXRotMatrix(float phi, FLOAT matrix[static 16])
{
    float sp, cp;

    sincosf(phi, &sp, &cp);

    const FLOAT m[] = {
    /*  x    y    z    w */
        1.f, 0.f, 0.f, 0.f,
        0.f, cp,  sp,  0.f,
        0.f, -sp, cp,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

static void getZoomMatrix(float zoom, FLOAT matrix[static 16]) {

    const FLOAT m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, FLOAT matrix[static 16]) {

    float zFar  = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const FLOAT m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,    -(zNear + zFar) / (zNear - zFar),  1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}

static void SetQuadVSProjection(vout_display_t *vd, d3d_quad_t *quad, const vlc_viewpoint_t *p_vp)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        VS_PROJECTION_CONST *dst_data = mapped.pData;
#define RAD(d) ((float) ((d) * M_PI / 180.f))
        getXRotMatrix(-RAD(p_vp->pitch), dst_data->RotX);
        getYRotMatrix(-RAD(p_vp->yaw),   dst_data->RotY);
        getZRotMatrix(-RAD(p_vp->roll),  dst_data->RotZ);
        getZoomMatrix(SPHERE_RADIUS * p_vp->zoom, dst_data->View);
        float sar = (float) vd->cfg->display.width / vd->cfg->display.height;
        getProjectionMatrix(sar, RAD(p_vp->fov), dst_data->Projection);
#undef RAD
    }
    ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0);
}

static void CropStagingFormat(vout_display_t *vd, video_format_t *backup_fmt)
{
    if ( vd->sys->stagingQuad.pTexture == NULL )
        return;

    video_format_Copy( backup_fmt, &vd->source );
    /* the texture we display is a cropped version of the source */
    vd->source.i_x_offset = 0;
    vd->source.i_y_offset = 0;
    vd->source.i_width  = vd->source.i_visible_width;
    vd->source.i_height = vd->source.i_visible_height;
}

static void UncropStagingFormat(vout_display_t *vd, video_format_t *backup_fmt)
{
    if ( vd->sys->stagingQuad.pTexture == NULL )
        return;
    video_format_Copy( &vd->source, backup_fmt );
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    video_format_t core_source;
    CropStagingFormat( vd, &core_source );
    int res = CommonControl( vd, query, args );

    if (query == VOUT_DISPLAY_CHANGE_VIEWPOINT)
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t*);
        if ( vd->sys->picQuad.pVertexShaderConstants )
        {
            SetQuadVSProjection( vd, &vd->sys->picQuad, &cfg->viewpoint );
            res = VLC_SUCCESS;
        }
    }

    UncropStagingFormat( vd, &core_source );
    return res;
}

static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    RECT size_before = sys->rect_dest_clipped;

    video_format_t core_source;
    CropStagingFormat( vd, &core_source );
    CommonManage(vd);

    if (RECTWidth(size_before)  != RECTWidth(sys->rect_dest_clipped) ||
        RECTHeight(size_before) != RECTHeight(sys->rect_dest_clipped))
    {
#if defined(HAVE_ID3D11VIDEODECODER)
        if( sys->context_lock != INVALID_HANDLE_VALUE )
        {
            WaitForSingleObjectEx( sys->context_lock, INFINITE, FALSE );
        }
#endif
        msg_Dbg(vd, "Manage detected size change %dx%d", RECTWidth(sys->rect_dest_clipped),
                RECTHeight(sys->rect_dest_clipped));

        UpdateBackBuffer(vd);

        UpdatePicQuadPosition(vd);
#if defined(HAVE_ID3D11VIDEODECODER)
        if( sys->context_lock != INVALID_HANDLE_VALUE )
        {
            ReleaseMutex( sys->context_lock );
        }
#endif
    }
    UncropStagingFormat( vd, &core_source );
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if ( !is_d3d11_opaque(picture->format.i_chroma) &&
         sys->stagingQuad.pTexture != NULL )
    {
        Direct3D11UnmapTexture(picture);

        D3D11_BOX box;
        box.left   = picture->format.i_x_offset;
        /* box.right  = picture->format.i_x_offset + picture->format.i_visible_width; */
        box.top    = picture->format.i_y_offset;
        /* box.bottom = picture->format.i_y_offset + picture->format.i_visible_height; */
        box.back = 1;
        box.front = 0;

        D3D11_TEXTURE2D_DESC dstDesc;
        ID3D11Texture2D_GetDesc(sys->picQuad.pTexture, &dstDesc);
        box.bottom = box.top  + dstDesc.Height;
        box.right  = box.left + dstDesc.Width;

        ID3D11DeviceContext_CopySubresourceRegion(sys->d3dcontext,
                                                  (ID3D11Resource*) sys->picQuad.pTexture,
                                                  0, 0, 0, 0,
                                                  (ID3D11Resource*) sys->stagingQuad.pTexture,
                                                  0, &box);
    }

#ifdef HAVE_ID3D11VIDEODECODER
    if (is_d3d11_opaque(picture->format.i_chroma)) {
        if( sys->context_lock != INVALID_HANDLE_VALUE )
        {
            WaitForSingleObjectEx( sys->context_lock, INFINITE, FALSE );
        }
        D3D11_BOX box;
        picture_sys_t *p_sys = picture->p_sys;
        D3D11_TEXTURE2D_DESC texDesc;
        ID3D11Texture2D_GetDesc( p_sys->texture, &texDesc );
        if (texDesc.Format == DXGI_FORMAT_NV12 || texDesc.Format == DXGI_FORMAT_P010)
        {
            box.left   = (picture->format.i_x_offset + 1) & ~1;
            box.right  = (picture->format.i_x_offset + picture->format.i_visible_width) & ~1;
            box.top    = (picture->format.i_y_offset + 1) & ~1;
            box.bottom = (picture->format.i_y_offset + picture->format.i_visible_height) & ~1;
        }
        else
        {
            box.left   = picture->format.i_x_offset;
            box.right  = picture->format.i_x_offset + picture->format.i_visible_width;
            box.top    = picture->format.i_y_offset;
            box.bottom = picture->format.i_y_offset + picture->format.i_visible_height;
        }
        box.back = 1;
        box.front = 0;

        ID3D11DeviceContext_CopySubresourceRegion(sys->d3dcontext,
                                                  (ID3D11Resource*) sys->picQuad.pTexture,
                                                  0, 0, 0, 0,
                                                  (ID3D11Resource*) p_sys->texture,
                                                  p_sys->slice_index, &box);
    }
#endif

    if (subpicture) {
        int subpicture_region_count    = 0;
        picture_t **subpicture_regions = NULL;
        Direct3D11MapSubpicture(vd, &subpicture_region_count, &subpicture_regions, subpicture);
        Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregions      = subpicture_regions;
    }
}

static void DisplayD3DPicture(vout_display_sys_t *sys, d3d_quad_t *quad)
{
    UINT stride = sizeof(d3d_vertex_t);
    UINT offset = 0;

    /* Render the quad */
    /* vertex shader */
    ID3D11DeviceContext_IASetVertexBuffers(sys->d3dcontext, 0, 1, &quad->pVertexBuffer, &stride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(sys->d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->pVertexShaderConstants )
        ID3D11DeviceContext_VSSetConstantBuffers(sys->d3dcontext, 0, 1, &quad->pVertexShaderConstants);

    ID3D11DeviceContext_VSSetShader(sys->d3dcontext, quad->d3dvertexShader, NULL, 0);

    /* pixel shader */
    ID3D11DeviceContext_PSSetShader(sys->d3dcontext, quad->d3dpixelShader, NULL, 0);

    ID3D11DeviceContext_PSSetConstantBuffers(sys->d3dcontext, 0, 1, &quad->pPixelShaderConstants);
    ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 0, 1, &quad->d3dresViewY);
    if( quad->d3dresViewUV )
        ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 1, 1, &quad->d3dresViewUV);

    ID3D11DeviceContext_RSSetViewports(sys->d3dcontext, 1, &quad->cropViewport);

    ID3D11DeviceContext_DrawIndexed(sys->d3dcontext, quad->indexCount, 0, 0);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    FLOAT blackRGBA[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(sys->d3dcontext, sys->d3drenderTargetView, blackRGBA);

    /* no ID3D11Device operations should come here */

    ID3D11DeviceContext_OMSetRenderTargets(sys->d3dcontext, 1, &sys->d3drenderTargetView, sys->d3ddepthStencilView);

    ID3D11DeviceContext_ClearDepthStencilView(sys->d3dcontext, sys->d3ddepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if ( !is_d3d11_opaque(picture->format.i_chroma) &&
         sys->stagingQuad.pTexture == NULL )
        Direct3D11UnmapTexture(picture);

    /* Render the quad */
    DisplayD3DPicture(sys, &sys->picQuad);

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
                DisplayD3DPicture(sys, (d3d_quad_t *) sys->d3dregions[i]->p_sys);
        }
    }

    DXGI_PRESENT_PARAMETERS presentParams;
    memset(&presentParams, 0, sizeof(presentParams));
    HRESULT hr = IDXGISwapChain1_Present1(sys->dxgiswapChain, 0, 0, &presentParams);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        /* TODO device lost */
        msg_Dbg(vd, "SwapChain Present failed. (hr=0x%lX)", hr);
    }
#if defined(HAVE_ID3D11VIDEODECODER)
    if( is_d3d11_opaque(picture->format.i_chroma) && sys->context_lock != INVALID_HANDLE_VALUE) {
        ReleaseMutex( sys->context_lock );
    }
#endif

    picture_Release(picture);
    if (subpicture)
        subpicture_Delete(subpicture);

    CommonDisplay(vd);
}

static void Direct3D11Destroy(vout_display_t *vd)
{
#if !VLC_WINSTORE_APP
    vout_display_sys_t *sys = vd->sys;

# if USE_DXGI
    if (sys->hdxgi_dll)
        FreeLibrary(sys->hdxgi_dll);
# endif

    if (sys->hd3d11_dll)
        FreeLibrary(sys->hd3d11_dll);
    if (sys->hd3dcompiler_dll)
        FreeLibrary(sys->hd3dcompiler_dll);

    sys->OurD3D11CreateDevice = NULL;
    sys->OurD3D11CreateDeviceAndSwapChain = NULL;
    sys->OurD3DCompile = NULL;
    sys->hdxgi_dll = NULL;
    sys->hd3d11_dll = NULL;
    sys->hd3dcompiler_dll = NULL;
#else
    VLC_UNUSED(vd);
#endif
}

#if !VLC_WINSTORE_APP
static HINSTANCE Direct3D11LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    /* d3dcompiler_47 is the latest on windows 8.1 */
    for (int i = 47; i > 41; --i) {
        TCHAR filename[19];
        _sntprintf(filename, 19, TEXT("D3DCOMPILER_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance) break;
    }
    return instance;
}
#endif


static int Direct3D11Open(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    *fmt = vd->source;

#if !VLC_WINSTORE_APP

    UINT creationFlags = 0;
    HRESULT hr = S_OK;

# if !defined(NDEBUG) && defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
# endif

    DXGI_SWAP_CHAIN_DESC1 scd;
    memset(&scd, 0, sizeof(scd));
    scd.BufferCount = 2;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Width = fmt->i_visible_width;
    scd.Height = fmt->i_visible_height;
    switch(fmt->i_chroma)
    {
    case VLC_CODEC_D3D11_OPAQUE_10B:
        scd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
        break;
    default:
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; /* TODO: use DXGI_FORMAT_NV12 */
        break;
    }
    //scd.Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    IDXGIAdapter *dxgiadapter;
    static const D3D_FEATURE_LEVEL featureLevels[] =
    {
        0xc000 /* D3D_FEATURE_LEVEL_12_1 */,
        0xc100 /* D3D_FEATURE_LEVEL_12_0 */,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

# if USE_DXGI
    /* TODO : list adapters for the user to choose from */
    hr = IDXGIFactory2_EnumAdapters(sys->dxgifactory, 0, &dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create find factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    IDXGIOutput* output;
    hr = IDXGIAdapter_EnumOutputs(dxgiadapter, 0, &output);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Enumerate DXGI Outputs. (hr=0x%lX)", hr);
       IDXGIAdapter_Release(dxgiadapter);
       return VLC_EGENERIC;
    }

    DXGI_MODE_DESC md;
    memset(&md, 0, sizeof(md));
    md.Width  = fmt->i_visible_width;
    md.Height = fmt->i_visible_height;
    md.Format = scd.BufferDesc.Format;
    md.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    hr = IDXGIOutput_FindClosestMatchingMode(output, &md, &scd.BufferDesc, NULL);
    if (FAILED(hr)) {
       msg_Err(vd, "Failed to find a supported video mode. (hr=0x%lX)", hr);
       IDXGIAdapter_Release(dxgiadapter);
       return VLC_EGENERIC;
    }

    /* mode desc doesn't carry over the width and height*/
    scd.BufferDesc.Width = fmt->i_visible_width;
    scd.BufferDesc.Height = fmt->i_visible_height;

    hr = D3D11CreateDeviceAndSwapChain(dxgiadapter,
                    D3D_DRIVER_TYPE_UNKNOWN, NULL, creationFlags,
                    featureLevels, ARRAYSIZE(featureLevels),
                    D3D11_SDK_VERSION, &scd, &sys->dxgiswapChain,
                    &sys->d3ddevice, NULL, &sys->d3dcontext);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Create the D3D11 device and SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

# else

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
#ifndef NDEBUG
        D3D_DRIVER_TYPE_REFERENCE,
#endif
    };

    for (UINT driver = 0; driver < ARRAYSIZE(driverAttempts); driver++) {
        D3D_FEATURE_LEVEL i_feature_level;
        hr = D3D11CreateDevice(NULL, driverAttempts[driver], NULL, creationFlags,
                    featureLevels, 9, D3D11_SDK_VERSION,
                    &sys->d3ddevice, &i_feature_level, &sys->d3dcontext);
        if (SUCCEEDED(hr)) {
#ifndef NDEBUG
            msg_Dbg(vd, "Created the D3D11 device 0x%p ctx 0x%p type %d level %x.",
                    (void *)sys->d3ddevice, (void *)sys->d3dcontext,
                    driverAttempts[driver], i_feature_level);
#endif
            break;
        }
    }

    if (FAILED(hr)) {
       msg_Err(vd, "Could not Create the D3D11 device. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    IDXGIDevice *pDXGIDevice = NULL;
    hr = ID3D11Device_QueryInterface(sys->d3ddevice, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Query DXGI Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &dxgiadapter);
    IDXGIAdapter_Release(pDXGIDevice);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Adapter. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&sys->dxgifactory);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(sys->dxgifactory, (IUnknown *)sys->d3ddevice,
                                              sys->hvideownd, &scd, NULL, NULL, &sys->dxgiswapChain);
    IDXGIFactory2_Release(sys->dxgifactory);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

# endif
#endif

    vlc_fourcc_t i_src_chroma = fmt->i_chroma;
    fmt->i_chroma = 0;

    // look for the requested pixel format first
    UINT i_quadSupportFlags = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    UINT i_formatSupport;
    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if( i_src_chroma == output_format->fourcc )
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           output_format->formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags )
            {
                msg_Dbg( vd, "Using pixel format %s from chroma %4.4s", output_format->name,
                             (char *)&i_src_chroma );
                fmt->i_chroma = output_format->fourcc;
                DxgiFormatMask( output_format->formatTexture, fmt );
                sys->picQuadConfig.textureFormat      = output_format->formatTexture;
                sys->picQuadConfig.resourceFormatYRGB = output_format->formatY;
                sys->picQuadConfig.resourceFormatUV   = output_format->formatUV;
                break;
            }
        }
    }

    // look for any pixel format that we can handle with enough pixels per channel
    if ( !fmt->i_chroma )
    {
        uint8_t bits_per_channel;
        switch (i_src_chroma)
        {
        case VLC_CODEC_D3D11_OPAQUE:
            bits_per_channel = 8;
            break;
        case VLC_CODEC_D3D11_OPAQUE_10B:
            bits_per_channel = 10;
            break;
        default:
            {
                const vlc_chroma_description_t *p_format = vlc_fourcc_GetChromaDescription(i_src_chroma);
                bits_per_channel = p_format == NULL || p_format->pixel_bits == 0 ? 8 : p_format->pixel_bits;
            }
            break;
        }

        for (const d3d_format_t *output_format = GetRenderFormatList();
             output_format->name != NULL; ++output_format)
        {
            if( bits_per_channel <= output_format->bitsPerChannel &&
                !is_d3d11_opaque(output_format->fourcc) )
            {
                if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                               output_format->formatTexture,
                                                               &i_formatSupport)) &&
                        ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags )
                {
                    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", output_format->name,
                                 (char *)&i_src_chroma );
                    fmt->i_chroma = output_format->fourcc;
                    DxgiFormatMask( output_format->formatTexture, fmt );
                    sys->picQuadConfig.textureFormat      = output_format->formatTexture;
                    sys->picQuadConfig.resourceFormatYRGB = output_format->formatY;
                    sys->picQuadConfig.resourceFormatUV   = output_format->formatUV;
                    break;
                }
            }
        }
    }
    // look for any pixel format that we can handle
    if ( !fmt->i_chroma )
    {
        for (const d3d_format_t *output_format = GetRenderFormatList();
             output_format->name != NULL; ++output_format)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           output_format->formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags &&
                    !is_d3d11_opaque(output_format->fourcc) )
            {
                msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", output_format->name,
                             (char *)&i_src_chroma );
                fmt->i_chroma = output_format->fourcc;
                DxgiFormatMask( output_format->formatTexture, fmt );
                sys->picQuadConfig.textureFormat      = output_format->formatTexture;
                sys->picQuadConfig.resourceFormatYRGB = output_format->formatY;
                sys->picQuadConfig.resourceFormatUV   = output_format->formatUV;
                break;
            }
        }
    }
    if ( !fmt->i_chroma )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    /* check the region pixel format */
    i_quadSupportFlags |= D3D11_FORMAT_SUPPORT_BLENDABLE;
    if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                   DXGI_FORMAT_R8G8B8A8_UNORM,
                                                   &i_formatSupport)) &&
            ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags) {
        sys->d3dregion_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sys->pSubpictureChromas[0] = VLC_CODEC_RGBA;
        sys->pSubpictureChromas[1] = 0;
    } else if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                          DXGI_FORMAT_B8G8R8A8_UNORM,
                                                          &i_formatSupport)) &&
                   ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags) {
        sys->d3dregion_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sys->pSubpictureChromas[0] = VLC_CODEC_BGRA;
        sys->pSubpictureChromas[1] = 0;
    } else {
        sys->d3dregion_format = DXGI_FORMAT_UNKNOWN;
    }

    if (sys->picQuadConfig.resourceFormatYRGB == DXGI_FORMAT_R8_UNORM ||
        sys->picQuadConfig.resourceFormatYRGB == DXGI_FORMAT_R16_UNORM)
    {
        if (vd->fmt.space == COLOR_SPACE_BT2020)
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT2020_2RGB;
        else if (vd->fmt.space == COLOR_SPACE_BT709)
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT709_2RGB;
        else if (vd->fmt.space == COLOR_SPACE_BT601)
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT601_2RGB;
        else if( fmt->i_height > 576 )
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT709_2RGB;
        else
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT601_2RGB;
    }
    else
    if (fmt->i_chroma == VLC_CODEC_YUYV)
    {
        if( fmt->i_height > 576 )
            sys->d3dPxShader = globPixelShaderBiplanarYUYV_BT709_2RGB;
        else
            sys->d3dPxShader = globPixelShaderBiplanarYUYV_BT601_2RGB;
    }
    else
        sys->d3dPxShader = globPixelShaderDefault;

    if (sys->d3dregion_format != DXGI_FORMAT_UNKNOWN)
        sys->psz_rgbaPxShader = globPixelShaderDefault;
    else
        sys->psz_rgbaPxShader = NULL;

    if ( fmt->i_height != fmt->i_visible_height || fmt->i_width != fmt->i_visible_width )
    {
        msg_Dbg( vd, "use a staging texture to crop to visible size" );
        AllocQuad( vd, fmt, &sys->stagingQuad, &sys->picQuadConfig, NULL, false,
                   PROJECTION_MODE_RECTANGULAR );
    }

    video_format_t core_source;
    CropStagingFormat( vd, &core_source );
    UpdateRects(vd, NULL, NULL, true);
    UncropStagingFormat( vd, &core_source );

#if defined(HAVE_ID3D11VIDEODECODER)
    if( sys->context_lock != INVALID_HANDLE_VALUE )
    {
        WaitForSingleObjectEx( sys->context_lock, INFINITE, FALSE );
    }
#endif
    if (Direct3D11CreateResources(vd, fmt)) {
#if defined(HAVE_ID3D11VIDEODECODER)
        if( sys->context_lock != INVALID_HANDLE_VALUE )
        {
            ReleaseMutex( sys->context_lock );
        }
#endif
        msg_Err(vd, "Failed to allocate resources");
        Direct3D11DestroyResources(vd);
        return VLC_EGENERIC;
    }
#if defined(HAVE_ID3D11VIDEODECODER)
    if( sys->context_lock != INVALID_HANDLE_VALUE )
    {
        ReleaseMutex( sys->context_lock );
    }
#endif

#if !VLC_WINSTORE_APP
    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (Direct3D11 output)");
#endif

    msg_Dbg(vd, "Direct3D11 device adapter successfully initialized");
    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);
    if (sys->d3dcontext)
    {
        ID3D11DeviceContext_Flush(sys->d3dcontext);
        ID3D11DeviceContext_Release(sys->d3dcontext);
        sys->d3dcontext = NULL;
    }
    if (sys->d3ddevice)
    {
        ID3D11Device_Release(sys->d3ddevice);
        sys->d3ddevice = NULL;
    }
    if (sys->dxgiswapChain)
    {
        IDXGISwapChain_Release(sys->dxgiswapChain);
        sys->dxgiswapChain = NULL;
    }

    msg_Dbg(vd, "Direct3D11 device adapter closed");
}

static void UpdatePicQuadPosition(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    int i_width  = RECTWidth(sys->rect_dest_clipped);
    int i_height = RECTHeight(sys->rect_dest_clipped);

    int i_top = sys->rect_src_clipped.top * i_height;
    i_top /= vd->source.i_visible_height;
    i_top -= sys->rect_dest_clipped.top;
    int i_left = sys->rect_src_clipped.left * i_width;
    i_left /= vd->source.i_visible_width;
    i_left -= sys->rect_dest_clipped.left;

    sys->picQuad.cropViewport.Width =  (FLOAT) vd->source.i_width  * i_width  / vd->source.i_visible_width;
    sys->picQuad.cropViewport.Height = (FLOAT) vd->source.i_height * i_height / vd->source.i_visible_height;
    sys->picQuad.cropViewport.TopLeftX = -i_left;
    sys->picQuad.cropViewport.TopLeftY = -i_top;

    sys->picQuad.cropViewport.MinDepth = 0.0f;
    sys->picQuad.cropViewport.MaxDepth = 1.0f;

#ifndef NDEBUG
    msg_Dbg(vd, "picQuad position (%.02f,%.02f) %.02fx%.02f", sys->picQuad.cropViewport.TopLeftX, sys->picQuad.cropViewport.TopLeftY, sys->picQuad.cropViewport.Width, sys->picQuad.cropViewport.Height );
#endif
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateResources(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

#if defined(HAVE_ID3D11VIDEODECODER)
    sys->context_lock = CreateMutexEx( NULL, NULL, 0, SYNCHRONIZE );
    ID3D11Device_SetPrivateData( sys->d3ddevice, &GUID_CONTEXT_MUTEX, sizeof( sys->context_lock ), &sys->context_lock );
#endif

    hr = UpdateBackBuffer(vd);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not update the backbuffer. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    ID3D11BlendState *pSpuBlendState;
    D3D11_BLEND_DESC spuBlendDesc = { 0 };
    spuBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    spuBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    spuBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    spuBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    spuBlendDesc.RenderTarget[1].BlendEnable = TRUE;
    spuBlendDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[1].DestBlend = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = ID3D11Device_CreateBlendState(sys->d3ddevice, &spuBlendDesc, &pSpuBlendState);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create SPU blend state. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    ID3D11DeviceContext_OMSetBlendState(sys->d3dcontext, pSpuBlendState, NULL, 0xFFFFFFFF);
    ID3D11BlendState_Release(pSpuBlendState);

    /* disable depth testing as we're only doing 2D
     * see https://msdn.microsoft.com/en-us/library/windows/desktop/bb205074%28v=vs.85%29.aspx
     * see http://rastertek.com/dx11tut11.html
    */
    D3D11_DEPTH_STENCIL_DESC stencilDesc;
    ZeroMemory(&stencilDesc, sizeof(stencilDesc));

    ID3D11DepthStencilState *pDepthStencilState;
    hr = ID3D11Device_CreateDepthStencilState(sys->d3ddevice, &stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_OMSetDepthStencilState(sys->d3dcontext, pDepthStencilState, 0);
        ID3D11DepthStencilState_Release(pDepthStencilState);
    }

    ID3DBlob* pVSBlob = NULL;
    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    hr = D3DCompile(globVertexShaderFlat, strlen(globVertexShaderFlat),
                    NULL, NULL, NULL, "VS", "vs_4_0_level_9_1", 0, 0, &pVSBlob, NULL);

    if( FAILED(hr)) {
      msg_Err(vd, "The flat Vertex Shader is invalid. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    hr = ID3D11Device_CreateVertexShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->flatVSShader);

    if(FAILED(hr)) {
      ID3D11Device_Release(pVSBlob);
      msg_Err(vd, "Failed to create the flat vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D11InputLayout* pVertexLayout = NULL;
    hr = ID3D11Device_CreateInputLayout(sys->d3ddevice, layout, 2, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), &pVertexLayout);

    ID3D10Blob_Release(pVSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D11DeviceContext_IASetInputLayout(sys->d3dcontext, pVertexLayout);
    ID3D11InputLayout_Release(pVertexLayout);
    
    hr = D3DCompile(globVertexShaderProjection, strlen(globVertexShaderProjection),
                    NULL, NULL, NULL, "VS", "vs_4_0_level_9_1", 0, 0, &pVSBlob, NULL);

    if( FAILED(hr)) {
      msg_Err(vd, "The projection Vertex Shader is invalid. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    hr = ID3D11Device_CreateVertexShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->projectionVSShader);

    if(FAILED(hr)) {
      ID3D11Device_Release(pVSBlob);
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D10Blob_Release(pVSBlob);

    ID3D11DeviceContext_IASetPrimitiveTopology(sys->d3dcontext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3DBlob* pPSBlob = NULL;

    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    hr = D3DCompile(sys->d3dPxShader, strlen(sys->d3dPxShader),
                    NULL, NULL, NULL, "PS", "ps_4_0_level_9_1", 0, 0, &pPSBlob, NULL);


    if( FAILED(hr)) {
      msg_Err(vd, "The Pixel Shader is invalid. (hr=0x%lX)", hr );
      return VLC_EGENERIC;
    }

    ID3D11PixelShader *pPicQuadShader;
    hr = ID3D11Device_CreatePixelShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pPSBlob),
                                        ID3D10Blob_GetBufferSize(pPSBlob), NULL, &pPicQuadShader);

    ID3D10Blob_Release(pPSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the pixel shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    if (sys->psz_rgbaPxShader != NULL)
    {
        hr = D3DCompile(sys->psz_rgbaPxShader, strlen(sys->psz_rgbaPxShader),
                        NULL, NULL, NULL, "PS", "ps_4_0_level_9_1", 0, 0, &pPSBlob, NULL);
        if( FAILED(hr)) {
          ID3D11PixelShader_Release(pPicQuadShader);
          msg_Err(vd, "The RGBA Pixel Shader is invalid. (hr=0x%lX)", hr );
          return VLC_EGENERIC;
        }

        hr = ID3D11Device_CreatePixelShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pPSBlob),
                                            ID3D10Blob_GetBufferSize(pPSBlob), NULL, &sys->pSPUPixelShader);

        ID3D10Blob_Release(pPSBlob);

        if(FAILED(hr)) {
          ID3D11PixelShader_Release(pPicQuadShader);
          msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
          return VLC_EGENERIC;
        }
    }

    if (AllocQuad( vd, fmt, &sys->picQuad, &sys->picQuadConfig, pPicQuadShader,
                   true, vd->fmt.projection_mode) != VLC_SUCCESS) {
        ID3D11PixelShader_Release(pPicQuadShader);
        msg_Err(vd, "Could not Create the main quad picture. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    ID3D11PixelShader_Release(pPicQuadShader);

    video_format_t core_source;
    CropStagingFormat( vd, &core_source );
    UpdatePicQuadPosition(vd);
    UncropStagingFormat( vd, &core_source );

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState *d3dsampState;
    hr = ID3D11Device_CreateSamplerState(sys->d3ddevice, &sampDesc, &d3dsampState);

    if (FAILED(hr)) {
      msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D11DeviceContext_PSSetSamplers(sys->d3dcontext, 0, 1, &d3dsampState);
    ID3D11SamplerState_Release(d3dsampState);

    if (Direct3D11CreatePool(vd, fmt))
    {
        msg_Err(vd, "Direct3D picture pool initialization failed");
        return VLC_EGENERIC;
    }

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static int Direct3D11CreatePool(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    if ( is_d3d11_opaque(fmt->i_chroma) )
        /* a D3D11VA pool will be created when needed */
        return VLC_SUCCESS;

    picture_sys_pool_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL)) {
        return VLC_ENOMEM;
    }

    if ( sys->stagingQuad.pTexture != NULL )
        picsys->texture  = sys->stagingQuad.pTexture;
    else
        picsys->texture  = sys->picQuad.pTexture;
    picsys->vd       = vd;

    picture_resource_t resource = {
        .p_sys = (picture_sys_t*) picsys,
        .pf_destroy = DestroyDisplayPicture,
    };

    picture_t *picture = picture_NewFromResource(fmt, &resource);
    if (!picture) {
        free(picsys);
        return VLC_ENOMEM;
    }
    ID3D11Texture2D_AddRef(picsys->texture);

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = Direct3D11MapTexture;
    //pool_cfg.unlock        = Direct3D11UnmapTexture;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool) {
        picture_Release(picture);
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static void Direct3D11DestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Release(sys->pool);
    sys->pool = NULL;
}

static void SetupQuadFlat(d3d_vertex_t *dst_data, WORD *triangle_pos)
{
    float right = 1.0f;
    float left = -1.0f;
    float top = 1.0f;
    float bottom = -1.0f;

    // bottom left
    dst_data[0].position.x = left;
    dst_data[0].position.y = bottom;
    dst_data[0].position.z = 0.0f;
    dst_data[0].texture.x = 0.0f;
    dst_data[0].texture.y = 1.0f;

    // bottom right
    dst_data[1].position.x = right;
    dst_data[1].position.y = bottom;
    dst_data[1].position.z = 0.0f;
    dst_data[1].texture.x = 1.0f;
    dst_data[1].texture.y = 1.0f;

    // top right
    dst_data[2].position.x = right;
    dst_data[2].position.y = top;
    dst_data[2].position.z = 0.0f;
    dst_data[2].texture.x = 1.0f;
    dst_data[2].texture.y = 0.0f;

    // top left
    dst_data[3].position.x = left;
    dst_data[3].position.y = top;
    dst_data[3].position.z = 0.0f;
    dst_data[3].texture.x = 0.0f;
    dst_data[3].texture.y = 0.0f;

    triangle_pos[0] = 3;
    triangle_pos[1] = 1;
    triangle_pos[2] = 0;

    triangle_pos[3] = 2;
    triangle_pos[4] = 1;
    triangle_pos[5] = 3;
}

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

static void SetupQuadSphere(d3d_vertex_t *dst_data, WORD *triangle_pos)
{
    for (unsigned lat = 0; lat <= nbLatBands; lat++) {
        float theta = lat * (float) M_PI / nbLatBands;
        float sinTheta, cosTheta;

        sincosf(theta, &sinTheta, &cosTheta);

        for (unsigned lon = 0; lon <= nbLonBands; lon++) {
            float phi = lon * 2 * (float) M_PI / nbLonBands;
            float sinPhi, cosPhi;

            sincosf(phi, &sinPhi, &cosPhi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            unsigned off1 = lat * (nbLonBands + 1) + lon;
            dst_data[off1].position.x = SPHERE_RADIUS * x;
            dst_data[off1].position.y = SPHERE_RADIUS * y;
            dst_data[off1].position.z = SPHERE_RADIUS * z;

            dst_data[off1].texture.x = lon / (float) nbLonBands; // 0(left) to 1(right)
            dst_data[off1].texture.y = lat / (float) nbLatBands; // 0(top) to 1 (bottom)
        }
    }

    for (unsigned lat = 0; lat < nbLatBands; lat++) {
        for (unsigned lon = 0; lon < nbLonBands; lon++) {
            unsigned first = (lat * (nbLonBands + 1)) + lon;
            unsigned second = first + nbLonBands + 1;

            unsigned off = (lat * nbLatBands + lon) * 3 * 2;

            triangle_pos[off] = first;
            triangle_pos[off + 1] = second;
            triangle_pos[off + 2] = first + 1;

            triangle_pos[off + 3] = second;
            triangle_pos[off + 4] = second + 1;
            triangle_pos[off + 5] = first + 1;
        }
    }
}

static bool AllocQuadVertices(vout_display_t *vd, d3d_quad_t *quad, video_projection_mode_t projection)
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    vout_display_sys_t *sys = vd->sys;

    if (projection == PROJECTION_MODE_RECTANGULAR)
    {
        quad->vertexCount = 4;
        quad->indexCount = 2 * 3;
    }
    else if (projection == PROJECTION_MODE_EQUIRECTANGULAR)
    {
        quad->vertexCount = (SPHERE_SLICES+1) * (SPHERE_SLICES+1);
        quad->indexCount = nbLatBands * nbLonBands * 2 * 3;
    }
    else
        return false;

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(d3d_vertex_t) * quad->vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &bd, NULL, &quad->pVertexBuffer);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create vertex buffer. (hr=%lX)", hr);
      return false;
    }

    /* create the index of the vertices */
    D3D11_BUFFER_DESC quadDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(WORD) * quad->indexCount,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &quadDesc, NULL, &quad->pIndexBuffer);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the quad indices. (hr=0x%lX)", hr);
        return false;
    }

    /* create the vertices */
    hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to lock the vertex buffer (hr=0x%lX)", hr);
        return false;
    }
    d3d_vertex_t *dst_data = mappedResource.pData;

    /* create the vertex indices */
    hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to lock the index buffer (hr=0x%lX)", hr);
        ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);
        return false;
    }
    WORD *triangle_pos = mappedResource.pData;

    if ( projection == PROJECTION_MODE_RECTANGULAR )
        SetupQuadFlat(dst_data, triangle_pos);
    else
        SetupQuadSphere(dst_data, triangle_pos);

    ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0);
    ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);

    return true;
}

static int AllocQuad(vout_display_t *vd, const video_format_t *fmt, d3d_quad_t *quad,
                     d3d_quad_cfg_t *cfg, ID3D11PixelShader *d3dpixelShader, bool b_visible,
                     video_projection_mode_t projection)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;

    /* pixel shader constant buffer */
    PS_CONSTANT_BUFFER defaultConstants = {
      .Opacity = 1,
    };
    static_assert((sizeof(defaultConstants)%16)==0,"Constant buffers require 16-byte alignment");
    D3D11_BUFFER_DESC constantDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(defaultConstants),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    D3D11_SUBRESOURCE_DATA constantInit = { .pSysMem = &defaultConstants };
    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &constantDesc, &constantInit, &quad->pPixelShaderConstants);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    /* vertex shader constant buffer */
    constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
    static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &constantDesc, NULL, &quad->pVertexShaderConstants);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    SetQuadVSProjection( vd, quad, &vd->cfg->viewpoint );

    D3D11_TEXTURE2D_DESC texDesc;
    memset(&texDesc, 0, sizeof(texDesc));
    texDesc.Width  = b_visible ? fmt->i_visible_width  : fmt->i_width;
    texDesc.Height = b_visible ? fmt->i_visible_height : fmt->i_height;
    texDesc.MipLevels = texDesc.ArraySize = 1;
    texDesc.Format = cfg->textureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    texDesc.MiscFlags = 0;

    /* remove half pixels, we don't want green lines */
    const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( fmt->i_chroma );
    for (unsigned plane = 0; plane < p_chroma_desc->plane_count; ++plane)
    {
        unsigned i_extra;
        i_extra = (texDesc.Width  * p_chroma_desc->p[plane].w.num) % p_chroma_desc->p[plane].w.den;
        if ( i_extra )
            texDesc.Width -= p_chroma_desc->p[plane].w.den / p_chroma_desc->p[plane].w.num - i_extra;
        i_extra = (texDesc.Height  * p_chroma_desc->p[plane].h.num) % p_chroma_desc->p[plane].h.den;
        if ( i_extra )
            texDesc.Height -= p_chroma_desc->p[plane].h.den / p_chroma_desc->p[plane].h.num - i_extra;
    }
    if (texDesc.Format == DXGI_FORMAT_NV12 || texDesc.Format == DXGI_FORMAT_P010)
    {
        texDesc.Width  &= ~1;
        texDesc.Height &= ~1;
    }

    hr = ID3D11Device_CreateTexture2D(sys->d3ddevice, &texDesc, NULL, &quad->pTexture);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3d11 Texture. (hr=0x%lX)", hr);
        goto error;
    }

    hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) ) {
        msg_Err(vd, "The texture cannot be mapped. (hr=0x%lX)", hr);
        goto error;
    }
    ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pTexture, 0);
    if (mappedResource.RowPitch < p_chroma_desc->pixel_size * texDesc.Width) {
        msg_Err( vd, "The texture row pitch is too small (%d instead of %d)", mappedResource.RowPitch,
                 p_chroma_desc->pixel_size * texDesc.Width );
        goto error;
    }

    /* map texture planes to resource views */
    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc;
    memset(&resviewDesc, 0, sizeof(resviewDesc));
    resviewDesc.Format = cfg->resourceFormatYRGB;
    resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resviewDesc.Texture2D.MipLevels = texDesc.MipLevels;

    hr = ID3D11Device_CreateShaderResourceView(sys->d3ddevice, (ID3D11Resource *)quad->pTexture, &resviewDesc, &quad->d3dresViewY);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the Y/RGB D3d11 Texture ResourceView. (hr=0x%lX)", hr);
        goto error;
    }

    if( cfg->resourceFormatUV )
    {
        resviewDesc.Format = cfg->resourceFormatUV;
        hr = ID3D11Device_CreateShaderResourceView(sys->d3ddevice, (ID3D11Resource *)quad->pTexture, &resviewDesc, &quad->d3dresViewUV);
        if (FAILED(hr)) {
            msg_Err(vd, "Could not Create the UV D3d11 Texture ResourceView. (hr=0x%lX)", hr);
            goto error;
        }
    }

    if ( d3dpixelShader != NULL )
    {
        if (!AllocQuadVertices(vd, quad, projection))
            goto error;

        if (projection == PROJECTION_MODE_RECTANGULAR)
            quad->d3dvertexShader = sys->flatVSShader;
        else
            quad->d3dvertexShader = sys->projectionVSShader;

        quad->d3dpixelShader = d3dpixelShader;
        ID3D11PixelShader_AddRef(quad->d3dpixelShader);
    }

    return VLC_SUCCESS;

error:
    ReleaseQuad(quad);
    return VLC_EGENERIC;
}

static void ReleaseQuad(d3d_quad_t *quad)
{
    if (quad->pPixelShaderConstants)
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants);
        quad->pPixelShaderConstants = NULL;
    }
    if (quad->pVertexBuffer)
    {
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
    }
    quad->d3dvertexShader = NULL;
    if (quad->pIndexBuffer)
    {
        ID3D11Buffer_Release(quad->pIndexBuffer);
        quad->pIndexBuffer = NULL;
    }
    if (quad->pVertexShaderConstants)
    {
        ID3D11Buffer_Release(quad->pVertexShaderConstants);
        quad->pVertexShaderConstants = NULL;
    }
    if (quad->pTexture)
    {
        ID3D11Texture2D_Release(quad->pTexture);
        quad->pTexture = NULL;
    }
    if (quad->d3dresViewY)
    {
        ID3D11ShaderResourceView_Release(quad->d3dresViewY);
        quad->d3dresViewY = NULL;
    }
    if (quad->d3dresViewUV)
    {
        ID3D11ShaderResourceView_Release(quad->d3dresViewUV);
        quad->d3dresViewUV = NULL;
    }
    if (quad->d3dpixelShader)
    {
        ID3D11VertexShader_Release(quad->d3dpixelShader);
        quad->d3dpixelShader = NULL;
    }
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyPool(vd);

    if ( sys->stagingQuad.pTexture )
        ReleaseQuad(&sys->stagingQuad);
    ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;

    if (sys->flatVSShader)
    {
        ID3D11VertexShader_Release(sys->flatVSShader);
        sys->flatVSShader = NULL;
    }
    if (sys->projectionVSShader)
    {
        ID3D11VertexShader_Release(sys->projectionVSShader);
        sys->projectionVSShader = NULL;
    }
    if (sys->d3drenderTargetView)
    {
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
        sys->d3drenderTargetView = NULL;
    }
    if (sys->d3ddepthStencilView)
    {
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
        sys->d3ddepthStencilView = NULL;
    }
    if (sys->pSPUPixelShader)
    {
        ID3D11VertexShader_Release(sys->pSPUPixelShader);
        sys->pSPUPixelShader = NULL;
    }
#if defined(HAVE_ID3D11VIDEODECODER)
    if( sys->context_lock != INVALID_HANDLE_VALUE )
    {
        CloseHandle( sys->context_lock );
        sys->context_lock = INVALID_HANDLE_VALUE;
    }
#endif

    msg_Dbg(vd, "Direct3D11 resources destroyed");
}

static int Direct3D11MapTexture(picture_t *picture)
{
    picture_sys_pool_t *p_sys = (picture_sys_pool_t*) picture->p_sys;
    vout_display_t     *vd = p_sys->vd;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    hr = ID3D11DeviceContext_Map(vd->sys->d3dcontext, (ID3D11Resource *)p_sys->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
    {
        msg_Dbg( vd, "failed to map the texture (hr=0x%lX)", hr );
        return VLC_EGENERIC;
    }
    return CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
}

static int Direct3D11UnmapTexture(picture_t *picture)
{
    picture_sys_pool_t *p_sys = (picture_sys_pool_t*)picture->p_sys;
    vout_display_t     *vd = p_sys->vd;
    ID3D11DeviceContext_Unmap(vd->sys->d3dcontext, (ID3D11Resource *)p_sys->texture, 0);
    return VLC_SUCCESS;
}

static void Direct3D11DeleteRegions(int count, picture_t **region)
{
    for (int i = 0; i < count; ++i) {
        if (region[i]) {
            picture_Release(region[i]);
        }
    }
    free(region);
}

static void DestroyPictureQuad(picture_t *p_picture)
{
    ReleaseQuad( (d3d_quad_t *) p_picture->p_sys );
    free( p_picture );
}

static void UpdateQuadOpacity(vout_display_t *vd, const d3d_quad_t *quad, float opacity)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    HRESULT hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        FLOAT *dst_data = mappedResource.pData;
        *dst_data = opacity;
        ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants, 0);
    }
    else {
        msg_Err(vd, "Failed to lock the subpicture vertex buffer (hr=0x%lX)", hr);
    }
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    int count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *region = calloc(count, sizeof(picture_t *));
    if (unlikely(*region==NULL))
        return VLC_ENOMEM;
    *subpicture_region_count = count;

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        if (!r->fmt.i_width || !r->fmt.i_height)
            continue; // won't render anything, keep the cache for later

        for (int j = 0; j < sys->d3dregion_count; j++) {
            picture_t *cache = sys->d3dregions[j];
            if (cache != NULL && ((d3d_quad_t *) cache->p_sys)->pTexture) {
                ID3D11Texture2D_GetDesc( ((d3d_quad_t *) cache->p_sys)->pTexture, &texDesc );
                if (texDesc.Format == sys->d3dregion_format &&
                    texDesc.Width  == r->fmt.i_visible_width &&
                    texDesc.Height == r->fmt.i_visible_height) {
                    (*region)[i] = cache;
                    memset(&sys->d3dregions[j], 0, sizeof(cache)); // do not reuse this cached value
                    break;
                }
            }
        }

        picture_t *quad_picture = (*region)[i];
        if (quad_picture == NULL) {
            d3d_quad_t *d3dquad = calloc(1, sizeof(*d3dquad));
            if (unlikely(d3dquad==NULL)) {
                continue;
            }
            d3d_quad_cfg_t rgbaCfg = {
                .textureFormat      = sys->d3dregion_format,
                .resourceFormatYRGB = sys->d3dregion_format,
            };
            err = AllocQuad( vd, &r->fmt, d3dquad, &rgbaCfg, sys->pSPUPixelShader,
                             false, PROJECTION_MODE_RECTANGULAR );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to create %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            picture_resource_t picres = {
                .p_sys      = (picture_sys_t *) d3dquad,
                .pf_destroy = DestroyPictureQuad,
            };
            (*region)[i] = picture_NewFromResource(&r->fmt, &picres);
            if ((*region)[i] == NULL) {
                msg_Err(vd, "Failed to create %dx%d picture for OSD",
                        r->fmt.i_width, r->fmt.i_height);
                ReleaseQuad(d3dquad);
                continue;
            }
            quad_picture = (*region)[i];
        }

        hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)((d3d_quad_t *) quad_picture->p_sys)->pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            err = CommonUpdatePicture(quad_picture, NULL, mappedResource.pData, mappedResource.RowPitch);
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to set the buffer on the SPU picture" );
                picture_Release(quad_picture);
                continue;
            }

            picture_CopyPixels(quad_picture, r->p_picture);

            ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)((d3d_quad_t *) quad_picture->p_sys)->pTexture, 0);
        } else {
            msg_Err(vd, "Failed to map the SPU texture (hr=0x%lX)", hr );
            picture_Release(quad_picture);
            continue;
        }

        d3d_quad_t *quad = (d3d_quad_t *) quad_picture->p_sys;

        quad->cropViewport.Width =  (FLOAT) r->fmt.i_visible_width  * RECTWidth(sys->rect_dest)  / subpicture->i_original_picture_width;
        quad->cropViewport.Height = (FLOAT) r->fmt.i_visible_height * RECTHeight(sys->rect_dest) / subpicture->i_original_picture_height;
        quad->cropViewport.MinDepth = 0.0f;
        quad->cropViewport.MaxDepth = 1.0f;
        quad->cropViewport.TopLeftX = sys->rect_dest.left + (FLOAT) r->i_x * RECTWidth(sys->rect_dest) / subpicture->i_original_picture_width;
        quad->cropViewport.TopLeftY = sys->rect_dest.top  + (FLOAT) r->i_y * RECTHeight(sys->rect_dest) / subpicture->i_original_picture_height;

        UpdateQuadOpacity(vd, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}

