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
# if USE_DXGI
#  define D3D11CreateDeviceAndSwapChain(args...) sys->OurD3D11CreateDeviceAndSwapChain(args)
# else
#  define D3D11CreateDevice(args...)             sys->OurD3D11CreateDevice(args)
# endif
# define D3DCompile(args...)                    sys->OurD3DCompile(args)
#endif

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

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
    { "VA_NV12",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM },
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
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_RGBA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,     DXGI_FORMAT_B8G8R8A8_UNORM,     0 },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,    DXGI_FORMAT_B8G8R8X8_UNORM,     0 },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,    DXGI_FORMAT_B5G6R5_UNORM,       0 },

    { NULL, 0, 0, 0, 0}
};

#ifdef HAVE_ID3D11VIDEODECODER 
/* VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    ID3D11Texture2D               *texture;
    ID3D11DeviceContext           *context;
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
    FLOAT       opacity;
} d3d_vertex_t;

#define RECTWidth(r)   (int)(r.right - r.left)
#define RECTHeight(r)  (int)(r.bottom - r.top)

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *object);

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
static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static int AllocQuad(vout_display_t *, const video_format_t *, d3d_quad_t *,
                     d3d_quad_cfg_t *, ID3D11PixelShader *);
static void ReleaseQuad(d3d_quad_t *);
static void UpdatePicQuadPosition(vout_display_t *);
static void UpdateQuadPosition(vout_display_t *, const d3d_quad_t *, const RECT *, int w, int h, float o);

static void Manage(vout_display_t *vd);

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
    float  Opacity    : OPACITY;\
  };\
  \
  struct VS_OUTPUT\
  {\
    float4 Position   : SV_POSITION;\
    float2 Texture    : TEXCOORD0;\
    float  Opacity    : OPACITY;\
  };\
  \
  VS_OUTPUT VS( VS_INPUT In )\
  {\
    VS_OUTPUT Output;\
    Output.Position = float4(In.Position.xy, 0.0f, 1.0f);\
    Output.Texture = In.Texture;\
    Output.Opacity = In.Opacity;\
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
    float  Opacity    : OPACITY;\
  };\
  \
  float4 PS( PS_INPUT In ) : SV_TARGET\
  {\
    float4 rgba; \
    \
    rgba = shaderTexture.Sample(SampleType, In.Texture);\
    rgba.a = rgba.a * In.Opacity;\
    return rgba; \
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
    float  Opacity    : OPACITY;\
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
    rgba.a = In.Opacity;\
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
    float  Opacity    : OPACITY;\
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
    rgba.a = In.Opacity;\
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
    float  Opacity    : OPACITY;\
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
    rgba.a = In.Opacity;\
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
    float  Opacity    : OPACITY;\
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
    rgba.a = In.Opacity;\
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
    IDXGISwapChain1* dxgiswapChain  = var_InheritInteger(vd, "winrt-swapchain");
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
    IDXGISwapChain_AddRef     (sys->dxgiswapChain);
    ID3D11Device_AddRef       (sys->d3ddevice);
    ID3D11DeviceContext_AddRef(sys->d3dcontext);
#endif

    if (CommonInit(vd))
        goto error;

    video_format_t fmt;
    if (Direct3D11Open(vd, &fmt)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vout_display_info_t info  = vd->info;
    info.is_slow              = fmt.i_chroma != VLC_CODEC_D3D11_OPAQUE;
    info.has_double_click     = true;
    info.has_hide_mouse       = false;
    info.has_pictures_invalid = true;
    info.has_event_thread     = true;
    info.has_pictures_invalid = fmt.i_chroma != VLC_CODEC_D3D11_OPAQUE;

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        sys->d3dregion_format != DXGI_FORMAT_UNKNOWN)
        info.subpicture_chromas = sys->pSubpictureChromas;
    else
        info.subpicture_chromas = NULL;

    video_format_Clean(&vd->fmt);
    video_format_Copy(&vd->fmt, &fmt);
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = CommonControl;
    vd->manage  = Manage;

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

static picture_pool_t *Pool(vout_display_t *vd, unsigned pool_size)
{
    if ( vd->sys->pool != NULL )
        return vd->sys->pool;

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
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    unsigned surface_count;
    for (surface_count = 0; surface_count < pool_size; surface_count++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        hr = ID3D11Device_CreateTexture2D( vd->sys->d3ddevice, &texDesc, NULL, &picsys->texture );
        if (FAILED(hr)) {
            msg_Err(vd, "CreateTexture2D %d failed. (hr=0x%0lx)", pool_size, hr);
            goto error;
        }

        picsys->context = vd->sys->d3dcontext;

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyDisplayPoolPicture,
        };

        picture_t *picture = picture_NewFromResource(&vd->fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto error;
        }

        pictures[surface_count] = picture;
        /* each picture_t holds a ref to the context and release it on Destroy */
        ID3D11DeviceContext_AddRef(picsys->context);
    }
    msg_Dbg(vd, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            pool_size, vd->fmt.i_width, vd->fmt.i_height);

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = pool_size;
    pool_cfg.picture       = pictures;

    vd->sys->pool = picture_pool_NewExtended( &pool_cfg );

error:
    if (vd->sys->pool ==NULL && pictures) {
        for (unsigned i=0;i<picture_count; ++i)
            DestroyDisplayPoolPicture(pictures[i]);
        free(pictures);
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

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)i_width;
    vp.Height = (FLOAT)i_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    ID3D11DeviceContext_RSSetViewports(sys->d3dcontext, 1, &vp);

    return S_OK;
}

static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    RECT size_before = sys->rect_dest_clipped;

    CommonManage(vd);

    if (RECTWidth(size_before)  != RECTWidth(sys->rect_dest_clipped) ||
        RECTHeight(size_before) != RECTHeight(sys->rect_dest_clipped))
    {
        msg_Dbg(vd, "Manage detected size change %dx%d", RECTWidth(sys->rect_dest_clipped),
                RECTHeight(sys->rect_dest_clipped));

        UpdateBackBuffer(vd);

        UpdatePicQuadPosition(vd);
    }
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

#ifdef HAVE_ID3D11VIDEODECODER 
    if (picture->format.i_chroma == VLC_CODEC_D3D11_OPAQUE) {
        D3D11_BOX box;
        box.left = 0;
        box.right = picture->format.i_visible_width;
        box.top = 0;
        box.bottom = picture->format.i_visible_height;
        box.back = 1;
        box.front = 0;

        picture_sys_t *p_sys = picture->p_sys;
        ID3D11DeviceContext_CopySubresourceRegion(sys->d3dcontext,
                                                  (ID3D11Resource*) sys->picQuad.pTexture,
                                                  0, 0, 0, 0,
                                                  (ID3D11Resource*) p_sys->texture,
                                                  0, &box);
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
    ID3D11DeviceContext_PSSetShader(sys->d3dcontext, quad->d3dpixelShader, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 0, 1, &quad->d3dresViewY);

    if( quad->d3dresViewUV )
        ID3D11DeviceContext_PSSetShaderResources(sys->d3dcontext, 1, 1, &quad->d3dresViewUV);

    ID3D11DeviceContext_IASetVertexBuffers(sys->d3dcontext, 0, 1, &quad->pVertexBuffer, &stride, &offset);
    ID3D11DeviceContext_DrawIndexed(sys->d3dcontext, 6, 0, 0);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    /* no ID3D11Device operations should come here */

    ID3D11DeviceContext_OMSetRenderTargets(sys->d3dcontext, 1, &sys->d3drenderTargetView, sys->d3ddepthStencilView);

    ID3D11DeviceContext_ClearDepthStencilView(sys->d3dcontext, sys->d3ddepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    /* Render the quad */
    DisplayD3DPicture(sys, &sys->picQuad);

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            DisplayD3DPicture(sys, (d3d_quad_t *) sys->d3dregions[i]->p_sys);
        }
    }

    HRESULT hr = IDXGISwapChain_Present(sys->dxgiswapChain, 0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        /* TODO device lost */
    }

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

    scd.Windowed = TRUE;
    scd.OutputWindow = sys->hvideownd;

    IDXGIAdapter *dxgiadapter;
# if USE_DXGI
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

    /* TODO : list adapters for the user to choose from */
    hr = IDXGIFactory_EnumAdapters(sys->dxgifactory, 0, &dxgiadapter);
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
        D3D_DRIVER_TYPE_REFERENCE,
    };

    for (UINT driver = 0; driver < ARRAYSIZE(driverAttempts); driver++) {
        hr = D3D11CreateDevice(NULL, driverAttempts[driver], NULL, creationFlags,
                    NULL, 0, D3D11_SDK_VERSION,
                    &sys->d3ddevice, NULL, &sys->d3dcontext);
        if (SUCCEEDED(hr)) {
#ifndef NDEBUG
            msg_Dbg(vd, "Created the D3D11 device 0x%p ctx 0x%p type %d.", sys->d3ddevice, sys->d3dcontext, driverAttempts[driver]);
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

    hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory, (void **)&sys->dxgifactory);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIFactory_CreateSwapChain(sys->dxgifactory, (IUnknown *)sys->d3ddevice, &scd, &sys->dxgiswapChain);
    IDXGIFactory_Release(sys->dxgifactory);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

# endif
#endif

    vlc_fourcc_t i_src_chroma = fmt->i_chroma;
    fmt->i_chroma = 0;

    // look for the request pixel format first
    UINT i_quadSupportFlags = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    UINT i_formatSupport;
    for (unsigned i = 0; d3d_formats[i].name != 0; i++)
    {
        if( i_src_chroma == d3d_formats[i].fourcc)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags )
            {
                msg_Dbg(vd, "Using pixel format %s", d3d_formats[i].name );
                fmt->i_chroma = d3d_formats[i].fourcc;
                sys->picQuadConfig.textureFormat      = d3d_formats[i].formatTexture;
                sys->picQuadConfig.resourceFormatYRGB = d3d_formats[i].formatY;
                sys->picQuadConfig.resourceFormatUV   = d3d_formats[i].formatUV;
                break;
            }
        }
    }

    // look for any pixel format that we can handle
    if ( !fmt->i_chroma )
    {
        for (unsigned i = 0; d3d_formats[i].name != 0; i++)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(sys->d3ddevice,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags )
            {
                msg_Dbg(vd, "Using pixel format %s", d3d_formats[i].name );
                fmt->i_chroma = d3d_formats[i].fourcc;
                sys->picQuadConfig.textureFormat      = d3d_formats[i].formatTexture;
                sys->picQuadConfig.resourceFormatYRGB = d3d_formats[i].formatY;
                sys->picQuadConfig.resourceFormatUV   = d3d_formats[i].formatUV;
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

    switch (fmt->i_chroma)
    {
    case VLC_CODEC_NV12:
    case VLC_CODEC_D3D11_OPAQUE:
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
    if (sys->d3dregion_format != DXGI_FORMAT_UNKNOWN)
        sys->psz_rgbaPxShader = globPixelShaderDefault;
    else
        sys->psz_rgbaPxShader = NULL;

    UpdateRects(vd, NULL, NULL, true);

    if (Direct3D11CreateResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate resources");
        Direct3D11DestroyResources(vd);
        return VLC_EGENERIC;
    }

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
#if VLC_WINSTORE_APP
    UINT dataSize = sizeof(i_width);
    HRESULT hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
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

    /* Map the subpicture to sys->rect_dest_clipped */
    UpdateQuadPosition(vd, &sys->picQuad, &sys->rect_dest_clipped, i_width, i_height, 1.0f);
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateResources(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

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
    stencilDesc.DepthEnable                  = FALSE;
    stencilDesc.StencilEnable                = TRUE;
    stencilDesc.DepthWriteMask               = D3D11_DEPTH_WRITE_MASK_ALL;
    stencilDesc.DepthFunc                    = D3D11_COMPARISON_LESS;
    stencilDesc.StencilReadMask              = 0xFF;
    stencilDesc.StencilWriteMask             = 0xFF;
    stencilDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    stencilDesc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    stencilDesc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    stencilDesc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
    stencilDesc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_DECR;
    stencilDesc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
    stencilDesc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;

    ID3D11DepthStencilState *pDepthStencilState;
    hr = ID3D11Device_CreateDepthStencilState(sys->d3ddevice, &stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_OMSetDepthStencilState(sys->d3dcontext, pDepthStencilState, 0);
        ID3D11DepthStencilState_Release(pDepthStencilState);
    }

    ID3DBlob* pVSBlob = NULL;

    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    hr = D3DCompile(globVertexShaderDefault, strlen(globVertexShaderDefault),
                    NULL, NULL, NULL, "VS", "vs_4_0_level_9_1", 0, 0, &pVSBlob, NULL);

    if( FAILED(hr)) {
      msg_Err(vd, "The Vertex Shader is invalid.");
      return VLC_EGENERIC;
    }

    ID3D11VertexShader *d3dvertexShader;
    hr = ID3D11Device_CreateVertexShader(sys->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &d3dvertexShader);

    if(FAILED(hr)) {
      ID3D11Device_Release(pVSBlob);
      msg_Err(vd, "Failed to create the vertex shader.");
      return VLC_EGENERIC;
    }
    ID3D11DeviceContext_VSSetShader(sys->d3dcontext, d3dvertexShader, NULL, 0);
    ID3D11VertexShader_Release(d3dvertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "OPACITY",  0, DXGI_FORMAT_R32_FLOAT,       0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D11InputLayout* pVertexLayout = NULL;
    hr = ID3D11Device_CreateInputLayout(sys->d3ddevice, layout, 3, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), &pVertexLayout);

    ID3D10Blob_Release(pVSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout");
      return VLC_EGENERIC;
    }

    ID3D11DeviceContext_IASetInputLayout(sys->d3dcontext, pVertexLayout);
    ID3D11SamplerState_Release(pVertexLayout);

    /* create the index of the vertices */
    WORD indices[] = {
      3, 1, 0,
      2, 1, 3,
    };

    D3D11_BUFFER_DESC quadDesc = {
        .Usage = D3D11_USAGE_DEFAULT,
        .ByteWidth = sizeof(WORD) * 6,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = 0,
    };

    D3D11_SUBRESOURCE_DATA quadIndicesInit = {
        .pSysMem = indices,
    };

    ID3D11Buffer* pIndexBuffer = NULL;
    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &quadDesc, &quadIndicesInit, &pIndexBuffer);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not Create the common quad indices. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    ID3D11DeviceContext_IASetIndexBuffer(sys->d3dcontext, pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer_Release(pIndexBuffer);

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
      msg_Err(vd, "Failed to create the pixel shader.");
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
          msg_Err(vd, "Failed to create the SPU pixel shader.");
          return VLC_EGENERIC;
        }
    }

    if (AllocQuad( vd, fmt, &sys->picQuad, &sys->picQuadConfig, pPicQuadShader) != VLC_SUCCESS) {
        ID3D11PixelShader_Release(pPicQuadShader);
        msg_Err(vd, "Could not Create the main quad picture. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    ID3D11PixelShader_Release(pPicQuadShader);

    UpdatePicQuadPosition(vd);

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
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

    if ( fmt->i_chroma == VLC_CODEC_D3D11_OPAQUE )
        /* a D3D11VA pool will be created when needed */
        return VLC_SUCCESS;

    picture_sys_pool_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL)) {
        return VLC_ENOMEM;
    }

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

static int AllocQuad(vout_display_t *vd, const video_format_t *fmt, d3d_quad_t *quad,
                     d3d_quad_cfg_t *cfg, ID3D11PixelShader *d3dpixelShader)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(d3d_vertex_t) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateBuffer(sys->d3ddevice, &bd, NULL, &quad->pVertexBuffer);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create vertex buffer.");
      goto error;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    memset(&texDesc, 0, sizeof(texDesc));
    texDesc.Width = fmt->i_visible_width;
    texDesc.Height = fmt->i_visible_height;
    texDesc.MipLevels = texDesc.ArraySize = 1;
    texDesc.Format = cfg->textureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    texDesc.MiscFlags = 0;

    hr = ID3D11Device_CreateTexture2D(sys->d3ddevice, &texDesc, NULL, &quad->pTexture);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3d11 Texture. (hr=0x%lX)", hr);
        goto error;
    }

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

    quad->d3dpixelShader = d3dpixelShader;
    ID3D11PixelShader_AddRef(quad->d3dpixelShader);

    return VLC_SUCCESS;

error:
    ReleaseQuad(quad);
    return VLC_EGENERIC;
}

static void ReleaseQuad(d3d_quad_t *quad)
{
    if (quad->pVertexBuffer)
        ID3D11Buffer_Release(quad->pVertexBuffer);
    if (quad->pTexture)
        ID3D11Texture2D_Release(quad->pTexture);
    if (quad->d3dresViewY)
        ID3D11ShaderResourceView_Release(quad->d3dresViewY);
    if (quad->d3dresViewUV)
        ID3D11ShaderResourceView_Release(quad->d3dresViewUV);
    if (quad->d3dpixelShader)
        ID3D11VertexShader_Release(quad->d3dpixelShader);
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyPool(vd);

    ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;

    if (sys->d3drenderTargetView)
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
    if (sys->d3ddepthStencilView)
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
    if (sys->pSPUPixelShader)
        ID3D11VertexShader_Release(sys->pSPUPixelShader);

    msg_Dbg(vd, "Direct3D11 resources destroyed");
}

static int Direct3D11MapTexture(picture_t *picture)
{
    picture_sys_pool_t *p_sys = (picture_sys_pool_t*) picture->p_sys;
    vout_display_t     *vd = p_sys->vd;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    int res;
    hr = ID3D11DeviceContext_Map(vd->sys->d3dcontext, (ID3D11Resource *)p_sys->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
    {
        msg_Dbg( vd, "failed to map the texture (hr=0x%lX)", hr );
        return VLC_EGENERIC;
    }
    res = CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
    ID3D11DeviceContext_Unmap(vd->sys->d3dcontext,(ID3D11Resource *)p_sys->texture, 0);
    return res;
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

static void UpdateQuadPosition(vout_display_t *vd, const d3d_quad_t *quad, const RECT *dst, int i_width, int i_height, float opacity)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // adjust with the center at 0,0 and the edges at -1/1
    float left   = -1.0f + 2.0f * ((float) dst->left   / i_width );
    float right  = -1.0f + 2.0f * ((float) dst->right  / i_width );
    float top    =  1.0f - 2.0f * ((float) dst->top    / i_height );
    float bottom =  1.0f - 2.0f * ((float) dst->bottom / i_height );

    HRESULT hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        d3d_vertex_t *dst_data = mappedResource.pData;

        // bottom left
        dst_data[0].position.x = left;
        dst_data[0].position.y = bottom;
        dst_data[0].position.z = 0.0f;
        dst_data[0].texture.x = 0.0f;
        dst_data[0].texture.y = 1.0f;
        dst_data[0].opacity = opacity;

        // bottom right
        dst_data[1].position.x = right;
        dst_data[1].position.y = bottom;
        dst_data[1].position.z = 0.0f;
        dst_data[1].texture.x = 1.0f;
        dst_data[1].texture.y = 1.0f;
        dst_data[1].opacity = opacity;

        // top right
        dst_data[2].position.x = right;
        dst_data[2].position.y = top;
        dst_data[2].position.z = 0.0f;
        dst_data[2].texture.x = 1.0f;
        dst_data[2].texture.y = 0.0f;
        dst_data[2].opacity = opacity;

        // top left
        dst_data[3].position.x = left;
        dst_data[3].position.y = top;
        dst_data[3].position.z = 0.0f;
        dst_data[3].texture.x = 0.0f;
        dst_data[3].texture.y = 0.0f;
        dst_data[3].opacity = opacity;

        ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);
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
        for (int j = 0; j < sys->d3dregion_count; j++) {
            picture_t *cache = sys->d3dregions[j];
            if (((d3d_quad_t *) cache->p_sys)->pTexture) {
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
            err = AllocQuad(vd, &r->fmt, d3dquad, &rgbaCfg, sys->pSPUPixelShader);
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
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                ReleaseQuad(d3dquad);
                continue;
            }
            quad_picture = (*region)[i];
            hr = ID3D11DeviceContext_Map(sys->d3dcontext, (ID3D11Resource *)((d3d_quad_t *) quad_picture->p_sys)->pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if( SUCCEEDED(hr) ) {
                err = CommonUpdatePicture(quad_picture, NULL, mappedResource.pData, mappedResource.RowPitch);
                ID3D11DeviceContext_Unmap(sys->d3dcontext, (ID3D11Resource *)((d3d_quad_t *) quad_picture->p_sys)->pTexture, 0);
                if (err != VLC_SUCCESS) {
                    msg_Err(vd, "Failed to set the buffer on the OSD picture" );
                    picture_Release(quad_picture);
                    continue;
                }
            } else {
                msg_Err(vd, "Failed to map the OSD texture (hr=0x%lX)", hr );
                picture_Release(quad_picture);
                continue;
            }
#ifndef NDEBUG
            msg_Dbg(vd, "Created %dx%d texture for OSD",
                    r->fmt.i_visible_width, r->fmt.i_visible_height);
#endif
        }

        picture_CopyPixels(quad_picture, r->p_picture);

        /* Map the subpicture to sys->rect_dest */
        int i_original_width  = subpicture->i_original_picture_width;
        int i_original_height = subpicture->i_original_picture_height;

        const RECT video = sys->rect_dest;
        const float scale_w = (float)(video.right  - video.left) / i_original_width;
        const float scale_h = (float)(video.bottom - video.top)  / i_original_height;

        RECT dst;
        dst.left   = video.left + scale_w * r->i_x,
        dst.right  = dst.left + scale_w * r->fmt.i_visible_width,
        dst.top    = video.top  + scale_h * r->i_y,
        dst.bottom = dst.top  + scale_h * r->fmt.i_visible_height;

        float opacity = (float)r->i_alpha / 255.0f;

        UpdateQuadPosition(vd, (d3d_quad_t *)quad_picture->p_sys, &dst,
                           i_original_width, i_original_height, opacity);
    }
    return VLC_SUCCESS;
}

