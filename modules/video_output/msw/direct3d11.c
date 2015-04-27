/*****************************************************************************
 * direct3d11.c: Windows Direct3D11 video output module
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>
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
#include <vlc_vout_display.h>

#define COBJMACROS
#define INITGUID
#include <d3d11.h>

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "common.h"

#if !VLC_WINSTORE_APP
# define D3D11CreateDeviceAndSwapChain(args...) sys->OurD3D11CreateDeviceAndSwapChain(args)
# define D3D11CreateDevice(args...)             sys->OurD3D11CreateDevice(args)
# define D3DCompile(args...)                    sys->OurD3DCompile(args)
#else
# define IDXGISwapChain_Present(args...)        IDXGISwapChain_Present1(args)
# define IDXGIFactory_CreateSwapChain(a,b,c,d)  IDXGIFactory2_CreateSwapChainForComposition(a,b,c,NULL,d)
# define DXGI_SWAP_CHAIN_DESC                   DXGI_SWAP_CHAIN_DESC1
#endif

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

#define D3D11_HELP N_("Recommended video output for Windows 8 and later versions")

vlc_module_begin ()
    set_shortname("Direct3D11")
    set_description(N_("Direct3D11 video output"))
    set_help(D3D11_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 240)
    add_shortcut("direct3d11")
    set_callbacks(Open, Close)
vlc_module_end ()

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    DXGI_FORMAT  formatY;
    DXGI_FORMAT  formatUV;
} d3d_format_t;

static const d3d_format_t d3d_formats[] = {
    { "I420",     DXGI_FORMAT_NV12,           VLC_CODEC_I420,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "YV12",     DXGI_FORMAT_NV12,           VLC_CODEC_YV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
#ifdef BROKEN_PIXEL
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_I422,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "AYUV",     DXGI_FORMAT_AYUV,           VLC_CODEC_YUVA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L, DXGI_FORMAT_R16G16B16A16_UINT,  0 },
#endif
#ifdef UNTESTED
    { "P010",     DXGI_FORMAT_P010,           VLC_CODEC_I420_10L, DXGI_FORMAT_R16_UNORM,          DXGI_FORMAT_R16_UNORM },
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L, DXGI_FORMAT_R16G16B16A16_UNORM, 0 },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444_10L, DXGI_FORMAT_R10G10B10A2_UNORM,  0 },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
#endif
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,     DXGI_FORMAT_B8G8R8A8_UNORM,     0 },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,    DXGI_FORMAT_B8G8R8X8_UNORM,     0 },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,    DXGI_FORMAT_B5G6R5_UNORM,       0 },

    { NULL, 0, 0, 0, 0}
};

static const vlc_fourcc_t d3d_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

struct picture_sys_t
{
    ID3D11Texture2D     *texture;
    ID3D11DeviceContext *context;
    vout_display_t      *vd;
};

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *object);

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void Display(vout_display_t *, picture_t *, subpicture_t *subpicture);

static HINSTANCE Direct3D11LoadShaderLibrary(void);
static void Direct3D11Destroy(vout_display_t *);

static int  Direct3D11Open (vout_display_t *, video_format_t *);
static void Direct3D11Close(vout_display_t *);

static int  Direct3D11CreateResources (vout_display_t *, video_format_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static int  Direct3D11MapTexture(picture_t *);

/* All the #if USE_DXGI contain an alternative method to setup dx11
   They both need to be benchmarked to see which performs better */
#if USE_DXGI
/* I have no idea why MS decided dxgi headers do not define this
   As they do have prototypes for d3d11 functions */
typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY)(REFIID riid, void **ppFactory);
#endif

/* TODO: Move to a direct3d11_shaders header */
static const char* globVertexShaderDefault = "\
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
    Output.Position = float4(In.Position.xy, 0.0f, 1.0f);\
    Output.Texture = In.Texture;\
    return Output;\
  }\
";

static const char* globPixelShaderDefault = "\
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
    return shaderTexture.Sample(SampleType, In.Texture);\
  }\
";

static const char *globPixelShaderBiplanarI420_BT601_2RGB = "\
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
    float Y;\
    float UCb;\
    float VCr;\
    float2 UCbPos;\
    float2 VCrPos;\
    float4 rgba;\
    \
    Y  = shaderTextureY.Sample(SampleType, In.Texture).x;\
    \
    VCrPos = In.Texture / 2;\
    VCr = shaderTextureUV.Sample(SampleType, VCrPos).x;\
    \
    UCbPos = In.Texture / 2;\
    UCbPos.y = UCbPos.y + 0.5;\
    UCb = shaderTextureUV.Sample(SampleType, UCbPos).x;\
    \
    Y = 1.164383561643836 * (Y - 0.0625);\
    UCb = UCb - 0.5;\
    VCr = VCr - 0.5;\
    \
    rgba.x = saturate(Y + 1.596026785714286 * VCr);\
    rgba.y = saturate(Y - 0.812967647237771 * VCr - 0.391762290094914 * UCb);\
    rgba.z = saturate(Y + 2.017232142857142 * UCb);\
    rgba.w = 1.0;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarI420_BT709_2RGB = "\
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
    float Y;\
    float UCb;\
    float VCr;\
    float2 UCbPos;\
    float2 VCrPos;\
    float4 rgba;\
    \
    Y  = shaderTextureY.Sample(SampleType, In.Texture).x;\
    \
    VCrPos = In.Texture / 2;\
    VCr = shaderTextureUV.Sample(SampleType, VCrPos).x;\
    \
    UCbPos = In.Texture / 2;\
    UCbPos.y = UCbPos.y + 0.5;\
    UCb = shaderTextureUV.Sample(SampleType, UCbPos).x;\
    \
    Y = 1.164383561643836 * (Y - 0.0625);\
    UCb = UCb - 0.5;\
    VCr = VCr - 0.5;\
    \
    rgba.x = saturate(Y + 1.792741071428571 * VCr);\
    rgba.y = saturate(Y - 0.532909328559444 * VCr - 0.21324861427373 * UCb);\
    rgba.z = saturate(Y + 2.112401785714286 * UCb);\
    rgba.w = 1.0;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarYUV_BT601_2RGB = "\
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
    rgba.w = 1.0;\
    return rgba;\
  }\
";

static const char *globPixelShaderBiplanarYUV_BT709_2RGB = "\
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
    rgba.w = 1.0;\
    return rgba;\
  }\
";

static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

#if !VLC_WINSTORE_APP
    HINSTANCE hd3d11_dll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!hd3d11_dll) {
        msg_Warn(vd, "cannot load d3d11.dll, aborting");
        return VLC_EGENERIC;
    }

    HINSTANCE hd3dcompiler_dll = Direct3D11LoadShaderLibrary();
    if (!hd3dcompiler_dll) {
        msg_Err(vd, "cannot load d3dcompiler.dll, aborting");
        FreeLibrary(hd3d11_dll);
        return VLC_EGENERIC;
    }

# if USE_DXGI
    HINSTANCE hdxgi_dll = LoadLibrary(TEXT("DXGI.DLL"));
    if (!hdxgi_dll) {
        msg_Warn(vd, "cannot load dxgi.dll, aborting");
        return VLC_EGENERIC;
    }
# endif

#else
    IDXGISwapChain1* dxgiswapChain  = var_InheritInteger(vd, "winrt-dxgiswapchain");
    if (!dxgiswapChain)
        return VLC_EGENERIC;
    ID3D11Device* d3ddevice         = var_InheritInteger(vd, "winrt-d3ddevice");
    if (!d3ddevice)
        return VLC_EGENERIC;
    ID3D11DeviceContext* d3dcontext = var_InheritInteger(vd, "winrt-d3dcontext");
    if (!d3dcontext)
        return VLC_EGENERIC;
#endif

    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

#if !VLC_WINSTORE_APP
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
        (void *)GetProcAddress(sys->hdxgi_dll, "CreateDXGIFactory");
    if (!OurCreateDXGIFactory) {
        msg_Err(vd, "Cannot locate reference to CreateDXGIFactory in dxgi DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

    /* TODO : detect the directx version supported and use IID_IDXGIFactory3 or 2 */
    HRESULT hr = OurCreateDXGIFactory(&IID_IDXGIFactory, (void **)&sys->dxgifactory);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not create dxgi factory. (hr=0x%lX)", hr);
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

    sys->OurD3D11CreateDeviceAndSwapChain =
        (void *)GetProcAddress(sys->hd3d11_dll, "D3D11CreateDeviceAndSwapChain");
    if (!sys->OurD3D11CreateDeviceAndSwapChain) {
        msg_Err(vd, "Cannot locate reference to D3D11CreateDeviceAndSwapChain in d3d11 DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

# else
    sys->OurD3D11CreateDevice =
        (void *)GetProcAddress(sys->hd3d11_dll, "D3D11CreateDevice");
    if (!sys->OurD3D11CreateDevice) {
        msg_Err(vd, "Cannot locate reference to D3D11CreateDevice in d3d11 DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }
# endif

#else
    sys->dxgiswapChain = dxgiswapChain;
    sys->d3ddevice     = d3ddevice;
    sys->d3dcontext    = d3dcontext;
#endif

    if (CommonInit(vd))
        goto error;

    video_format_t fmt;
    if (Direct3D11Open(vd, &fmt)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vout_display_info_t info  = vd->info;
    info.is_slow              = true;
    info.has_double_click     = true;
    info.has_hide_mouse       = false;
    info.has_pictures_invalid = true;
    info.has_event_thread     = true;

    /* TODO : subtitle support */
    info.subpicture_chromas   = NULL;

    video_format_Clean(&vd->fmt);
    video_format_Copy(&vd->fmt, &fmt);
    vd->info = info;

    vd->pool    = CommonPool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = CommonControl;
    vd->manage  = CommonManage;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;
error:
    Direct3D11Close(vd);
    CommonClean(vd);
    Direct3D11Destroy(vd);
    free(vd->sys);
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

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(subpicture);
    VLC_UNUSED(picture);

    /* float ClearColor[4] = { 1.0f, 0.125f, 0.3f, 1.0f }; */
    /* ID3D11DeviceContext_ClearRenderTargetView(sys->d3dcontext,sys->d3drenderTargetView, ClearColor); */
    ID3D11DeviceContext_ClearDepthStencilView(sys->d3dcontext,sys->d3ddepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    /* Render the quad */
    ID3D11DeviceContext_VSSetShader(sys->d3dcontext, sys->d3dvertexShader, NULL, 0);
    ID3D11DeviceContext_PSSetShader(sys->d3dcontext, sys->d3dpixelShader, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 0, 1, &sys->d3dresViewY);

    if( sys->d3dFormatUV )
        ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 1, 1, &sys->d3dresViewUV);

    ID3D11DeviceContext_PSSetSamplers(sys->d3dcontext, 0, 1, &sys->d3dsampState);
    ID3D11DeviceContext_DrawIndexed(sys->d3dcontext, 6, 0, 0);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    IDXGISwapChain_Present(sys->dxgiswapChain, 0, 0);

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

    /* TODO : add release of d3d11 objects here */

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

    static const D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

# if !defined(NDEBUG) && defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
# endif

    DXGI_SWAP_CHAIN_DESC scd;
    memset(&scd, 0, sizeof(scd));
    scd.BufferCount = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferDesc.Width = fmt->i_visible_width;
    scd.BufferDesc.Height = fmt->i_visible_height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

# if VLC_WINSTORE_APP
    /* TODO : check different values for performance */
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.Scaling = DXGI_SCALING_NONE;
# else
    scd.Windowed = TRUE;
    scd.OutputWindow = sys->hvideownd;
# endif

# if USE_DXGI

    /* TODO : list adapters for the user to choose from */
    hr = IDXGIFactory_EnumAdapters(sys->dxgifactory, 0, &sys->dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create find factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    IDXGIOutput* output;
    hr = IDXGIAdapter_EnumOutputs(sys->dxgiadapter, 0, &output);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Enumerate DXGI Outputs. (hr=0x%lX)", hr);
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
       return VLC_EGENERIC;
    }

    /* mode desc doesn't carry over the width and height*/
    scd.BufferDesc.Width = fmt->i_visible_width;
    scd.BufferDesc.Height = fmt->i_visible_height;

    hr = D3D11CreateDeviceAndSwapChain(sys->dxgiadapter,
                    D3D_DRIVER_TYPE_UNKNOWN, NULL, creationFlags,
                    featureLevels, ARRAYSIZE(featureLevels),
                    D3D11_SDK_VERSION, &scd, &sys->dxgiswapChain,
                    &sys->d3ddevice, &sys->d3dfeaturelevel, &sys->d3dcontext);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Create the D3D11 device and SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

# else

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    for (UINT driver = 0; driver < ARRAYSIZE(driverAttempts); driver++) {
        hr = D3D11CreateDevice(NULL, driverAttempts[driver], NULL, creationFlags,
                    featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                    &sys->d3ddevice, &sys->d3dfeaturelevel, &sys->d3dcontext);
        if (SUCCEEDED(hr)) break;
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

    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &sys->dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Adapter. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIAdapter_GetParent(sys->dxgiadapter, &IID_IDXGIFactory, (void **)&sys->dxgifactory);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIFactory_CreateSwapChain(sys->dxgifactory, (IUnknown *)sys->d3ddevice, &scd, &sys->dxgiswapChain);

    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

#  if VLC_WINSTORE_APP /* avoided until we can pass ISwapchainPanel without c++/cx mode */
    /* TODO: figure out how to get "ISwapChainPanel ^panel" into brokenpanel in gcc */
    ISwapChainPanel *brokenpanel;
    ISwapChainPanelNative *panelNative;
    hr = ISwapChainPanelNative_QueryInterface(brokenpanel, &IID_ISwapChainPanelNative, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the Native Panel. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = ISwapChainPanelNative_SetSwapChain(panelNative, sys->dxgiswapChain);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not link the SwapChain with the Native Panel. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

#  endif
# endif
#endif

    // look for the request pixel format first
    for (unsigned i = 0; d3d_formats[i].name != 0; i++)
    {
        if( fmt->i_chroma == d3d_formats[i].fourcc)
        {
            UINT i_formatSupport;
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D ))
            {
                msg_Dbg(vd, "Using pixel format %s", d3d_formats[i].name );
                sys->d3dFormatTex = d3d_formats[i].formatTexture;
                sys->vlcFormat    = d3d_formats[i].fourcc;
                sys->d3dFormatY   = d3d_formats[i].formatY;
                sys->d3dFormatUV  = d3d_formats[i].formatUV;
                break;
            }
        }
    }

    // look for any pixel format that we can handle
    if ( !sys->vlcFormat )
    {
        for (unsigned i = 0; d3d_formats[i].name != 0; i++)
        {
            UINT i_formatSupport;
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D ))
            {
                msg_Dbg(vd, "Using pixel format %s", d3d_formats[i].name );
                sys->d3dFormatTex = d3d_formats[i].formatTexture;
                sys->vlcFormat    = d3d_formats[i].fourcc;
                sys->d3dFormatY   = d3d_formats[i].formatY;
                sys->d3dFormatUV  = d3d_formats[i].formatUV;
                break;
            }
        }
    }
    if ( !sys->vlcFormat )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    switch (sys->vlcFormat)
    {
    case VLC_CODEC_NV12:
        if( fmt->i_height > 576 )
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT709_2RGB;
        else
            sys->d3dPxShader = globPixelShaderBiplanarYUV_BT601_2RGB;
        break;
    case VLC_CODEC_YV12:
    case VLC_CODEC_I420:
        if( fmt->i_height > 576 )
            sys->d3dPxShader = globPixelShaderBiplanarI420_BT709_2RGB;
        else
            sys->d3dPxShader = globPixelShaderBiplanarI420_BT601_2RGB;
        break;
    case VLC_CODEC_RGB32:
    case VLC_CODEC_BGRA:
    case VLC_CODEC_RGB16:
    default:
        sys->d3dPxShader = globPixelShaderDefault;
        break;
    }

    UpdateRects(vd, NULL, NULL, true);

    if (Direct3D11CreateResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate resources");
        Direct3D11DestroyResources(vd);
        return VLC_EGENERIC;
    }

    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (Direct3D11 output)");

    msg_Dbg(vd, "Direct3D11 device adapter successfully initialized");
    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);
    if ( sys->d3dcontext )
        ID3D11DeviceContext_Release(sys->d3dcontext);
    if ( sys->d3ddevice )
        ID3D11Device_Release(sys->d3ddevice);
    msg_Dbg(vd, "Direct3D11 device adapter closed");
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateResources(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    ID3D11Texture2D* pBackBuffer = NULL;
    ID3D11Texture2D* pDepthStencil= NULL;
    HRESULT hr;

    fmt->i_chroma = sys->vlcFormat;

    hr = IDXGISwapChain_GetBuffer(sys->dxgiswapChain, 0, &IID_ID3D11Texture2D, (LPVOID *)&pBackBuffer);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the backbuffer from the Swapchain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = ID3D11Device_CreateRenderTargetView(sys->d3ddevice, (ID3D11Resource *)pBackBuffer, NULL, &sys->d3drenderTargetView);

    ID3D11Texture2D_Release(pBackBuffer);

    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the render view target. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    D3D11_TEXTURE2D_DESC deptTexDesc;
    memset(&deptTexDesc, 0,sizeof(deptTexDesc));
    deptTexDesc.ArraySize = 1;
    deptTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    deptTexDesc.CPUAccessFlags = 0;
    deptTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    deptTexDesc.Width = fmt->i_visible_width;
    deptTexDesc.Height = fmt->i_visible_height;
    deptTexDesc.MipLevels = 1;
    deptTexDesc.MiscFlags = 0;
    deptTexDesc.SampleDesc.Count = 1;
    deptTexDesc.SampleDesc.Quality = 0;
    deptTexDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = ID3D11Device_CreateTexture2D(sys->d3ddevice, &deptTexDesc, NULL, &pDepthStencil);

    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil texture. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
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
       return VLC_EGENERIC;
    }

    ID3D11DeviceContext_OMSetRenderTargets(sys->d3dcontext, 1, &sys->d3drenderTargetView, sys->d3ddepthStencilView);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)fmt->i_visible_width;
    vp.Height = (FLOAT)fmt->i_visible_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    ID3D11DeviceContext_RSSetViewports(sys->d3dcontext, 1, &vp);

    ID3DBlob* pVSBlob = NULL;

    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    hr = D3DCompile(globVertexShaderDefault, strlen(globVertexShaderDefault),
                    NULL, NULL, NULL, "VS", "vs_4_0_level_9_1", 0, 0, &pVSBlob, NULL);

    if( FAILED(hr)) {
      msg_Err(vd, "The Vertex Shader is invalid.");
      return VLC_EGENERIC;
    }

    hr = ID3D11Device_CreateVertexShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->d3dvertexShader);

    if(FAILED(hr)) {
      ID3D11Device_Release(pVSBlob);
      msg_Err(vd, "Failed to create the vertex shader.");
      return VLC_EGENERIC;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D11InputLayout* pVertexLayout = NULL;
    hr = ID3D11Device_CreateInputLayout(sys->d3ddevice, layout, 2, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), &pVertexLayout);

    ID3D10Blob_Release(pVSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout");
      return VLC_EGENERIC;
    }

    ID3D11DeviceContext_IASetInputLayout(sys->d3dcontext, pVertexLayout);

    ID3DBlob* pPSBlob = NULL;

    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    hr = D3DCompile(sys->d3dPxShader, strlen(sys->d3dPxShader),
                    NULL, NULL, NULL, "PS", "ps_4_0_level_9_1", 0, 0, &pPSBlob, NULL);


    if( FAILED(hr)) {
      msg_Err(vd, "The Pixel Shader is invalid. (hr=0x%lX)", hr );
      return VLC_EGENERIC;
    }

    hr = ID3D11Device_CreatePixelShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pPSBlob),
                                        ID3D10Blob_GetBufferSize(pPSBlob), NULL, &sys->d3dpixelShader);

    ID3D10Blob_Release(pPSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the pixel shader.");
      return VLC_EGENERIC;
    }

    float vertices[] = {
    -1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
     1.0f,  1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, -1.0f, 0.0f, 0.0f,
    };

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(float) * 5 * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    memset(&InitData, 0, sizeof(InitData));
    InitData.pSysMem = vertices;

    ID3D11Buffer* pVertexBuffer = NULL;
    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &bd, &InitData, &pVertexBuffer);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create vertex buffer.");
      return VLC_EGENERIC;
    }

    UINT stride = sizeof(float) * 5;
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(sys->d3dcontext, 0, 1, &pVertexBuffer, &stride, &offset);

    ID3D11Buffer_Release(pVertexBuffer);

    WORD indices[] = {
      3, 1, 0,
      2, 1, 3,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD)*6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;

    ID3D11Buffer* pIndexBuffer = NULL;
    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &bd, &InitData, &pIndexBuffer);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create index buffer.");
      return VLC_EGENERIC;
    }

    ID3D11DeviceContext_IASetIndexBuffer(sys->d3dcontext, pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer_Release(pVertexBuffer);

    ID3D11DeviceContext_IASetPrimitiveTopology(sys->d3dcontext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_TEXTURE2D_DESC texDesc;
    memset(&texDesc, 0, sizeof(texDesc));
    texDesc.Width = fmt->i_visible_width;
    texDesc.Height = fmt->i_visible_height;
    texDesc.MipLevels = texDesc.ArraySize = 1;
    texDesc.Format = sys->d3dFormatTex;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    texDesc.MiscFlags = 0;

    hr = ID3D11Device_CreateTexture2D(sys->d3ddevice, &texDesc, NULL, &sys->d3dtexture);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3d11 Texture. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc;
    memset(&resviewDesc, 0, sizeof(resviewDesc));
    resviewDesc.Format = sys->d3dFormatY;
    resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resviewDesc.Texture2D.MipLevels = texDesc.MipLevels;

    hr = ID3D11Device_CreateShaderResourceView(sys->d3ddevice, (ID3D11Resource *)sys->d3dtexture, &resviewDesc, &sys->d3dresViewY);
    if (FAILED(hr)) {
        if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
        msg_Err(vd, "Could not Create the Y D3d11 Texture ResourceView. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    if( sys->d3dFormatUV )
    {
        resviewDesc.Format = sys->d3dFormatUV;
        hr = ID3D11Device_CreateShaderResourceView(sys->d3ddevice, (ID3D11Resource *)sys->d3dtexture, &resviewDesc, &sys->d3dresViewUV);
        if (FAILED(hr)) {
            if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
            msg_Err(vd, "Could not Create the UV D3d11 Texture ResourceView. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = ID3D11Device_CreateSamplerState(sys->d3ddevice, &sampDesc, &sys->d3dsampState);

    if (FAILED(hr)) {
      if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
      msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    picture_sys_t *picsys = malloc(sizeof(*picsys));
    if (unlikely(picsys == NULL)) {
        if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
        return VLC_ENOMEM;
    }

    picsys->texture  = sys->d3dtexture;
    picsys->context  = sys->d3dcontext;
    picsys->vd       = vd;

    picture_resource_t resource = { .p_sys = picsys };
    for (int i = 0; i < PICTURE_PLANE_MAX; i++)
        resource.p[i].i_lines = fmt->i_visible_height / (i > 0 ? 2 : 1);

    picture_t *picture = picture_NewFromResource(fmt, &resource);
    if (!picture) {
        if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
        free(picsys);
        return VLC_ENOMEM;
    }
    sys->picsys = picsys;

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = Direct3D11MapTexture;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool) {
        picture_Release(picture);
        if(sys->d3dtexture) ID3D11Texture2D_Release(sys->d3dtexture);
        return VLC_ENOMEM;
    }

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* TODO: Destroy Shaders? */
    if (sys->pool) {
        picture_sys_t *picsys = sys->picsys;
        ID3D11Texture2D_Release(picsys->texture);
        picture_pool_Release(sys->pool);
    }
    sys->pool = NULL;

    msg_Dbg(vd, "Direct3D11 resources destroyed");
}

static int Direct3D11MapTexture(picture_t *picture)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    int res;
    hr = ID3D11DeviceContext_Map(picture->p_sys->context, (ID3D11Resource *)picture->p_sys->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
    {
        msg_Dbg( picture->p_sys->vd, "failed to map the texture (hr=0x%lX)", hr );
        return VLC_EGENERIC;
    }
    res = CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
    ID3D11DeviceContext_Unmap(picture->p_sys->context,(ID3D11Resource *)picture->p_sys->texture, 0);
    return res;
}
