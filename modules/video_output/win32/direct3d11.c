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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
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
#include <initguid.h>
#include <d3d11.h>
#ifdef HAVE_DXGI1_6_H
# include <dxgi1_6.h>
#else
# include <dxgi1_5.h>
#endif
#include <d3dcompiler.h>

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "../../video_chroma/d3d11_fmt.h"

#include "common.h"

#if !VLC_WINSTORE_APP
# define D3DCompile(args...)                    sys->OurD3DCompile(args)
#endif

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

#define DEFAULT_BRIGHTNESS         100
#define DEFAULT_SRGB_BRIGHTNESS    100
#define MAX_PQ_BRIGHTNESS        10000

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
    add_integer("winrt-d3dcontext",    0x0, NULL, NULL, true); /* ID3D11DeviceContext* */
    add_integer("winrt-swapchain",     0x0, NULL, NULL, true); /* IDXGISwapChain1*     */
#endif

    set_capability("vout display", 300)
    add_shortcut("direct3d11")
    set_callbacks(Open, Close)
vlc_module_end ()

typedef struct {
    FLOAT Opacity;
    FLOAT BoundaryX;
    FLOAT BoundaryY;
    FLOAT LuminanceScale;
} PS_CONSTANT_BUFFER;

/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    picture_sys_t             picSys;
    UINT                      resourceCount;
    ID3D11Buffer              *pVertexBuffer;
    UINT                      vertexCount;
    ID3D11VertexShader        *d3dvertexShader;
    ID3D11Buffer              *pIndexBuffer;
    UINT                      indexCount;
    ID3D11Buffer              *pVertexShaderConstants;
    ID3D11Buffer              *pPixelShaderConstants[2];
    UINT                       PSConstantsCount;
    ID3D11PixelShader         *d3dpixelShader;
    D3D11_VIEWPORT            cropViewport;
    unsigned int              i_width;
    unsigned int              i_height;

    PS_CONSTANT_BUFFER        shaderConstants;
} d3d_quad_t;

typedef enum video_color_axis {
    COLOR_AXIS_RGB,
    COLOR_AXIS_YCBCR,
} video_color_axis;

typedef struct {
    DXGI_COLOR_SPACE_TYPE   dxgi;
    const char              *name;
    video_color_axis        axis;
    video_color_primaries_t primaries;
    video_transfer_func_t   transfer;
    video_color_space_t     color;
    bool                    b_full_range;
} dxgi_color_space;

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;

    struct { /* TODO may go in vout_display_info_t without DXGI */
        const dxgi_color_space   *colorspace;
        unsigned                 luminance_peak;
    } display;

#if !VLC_WINSTORE_APP
    HINSTANCE                hdxgi_dll;        /* handle of the opened dxgi dll */
    d3d11_handle_t           hd3d;
    HINSTANCE                hd3dcompiler_dll; /* handle of the opened d3dcompiler dll */
    pD3DCompile                            OurD3DCompile;
#endif
#if defined(HAVE_ID3D11VIDEODECODER)
    HANDLE                   context_lock;     /* D3D11 Context lock necessary
                                                  for hw decoding */
#endif
    IDXGISwapChain1          *dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4          *dxgiswapChain4;  /* DXGI 1.5 for HDR */
    d3d11_device_t           d3d_dev;
    d3d_quad_t               picQuad;
    const d3d_format_t       *picQuadConfig;
    ID3D11PixelShader        *picQuadPixelShader;

    picture_sys_t            stagingSys;

    ID3D11RenderTargetView   *d3drenderTargetView;
    ID3D11DepthStencilView   *d3ddepthStencilView;

    ID3D11VertexShader        *flatVSShader;
    ID3D11VertexShader        *projectionVSShader;

    /* copy from the decoder pool into picSquad before display
     * Uses a Texture2D with slices rather than a Texture2DArray for the decoder */
    bool                     legacy_shader;

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2];
    ID3D11PixelShader        *pSPUPixelShader;
    const d3d_format_t       *d3dregion_format;
    int                      d3dregion_count;
    picture_t                **d3dregions;
};

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
    FLOAT WhitePoint[4*4];
    FLOAT Colorspace[4*4];
} PS_COLOR_TRANSFORM;

typedef struct {
    FLOAT RotX[4*4];
    FLOAT RotY[4*4];
    FLOAT RotZ[4*4];
    FLOAT View[4*4];
    FLOAT Projection[4*4];
} VS_PROJECTION_CONST;

#define SPHERE_RADIUS 1.f

#define RECTWidth(r)   (int)((r).right - (r).left)
#define RECTHeight(r)  (int)((r).bottom - (r).top)

static picture_pool_t *Pool(vout_display_t *vd, unsigned count);

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void Display(vout_display_t *, picture_t *, subpicture_t *subpicture);

static HINSTANCE Direct3D11LoadShaderLibrary(void);
static void Direct3D11Destroy(vout_display_t *);

static int  Direct3D11Open (vout_display_t *);
static void Direct3D11Close(vout_display_t *);

static int SetupOutputFormat(vout_display_t *, video_format_t *);
static int  Direct3D11CreateFormatResources (vout_display_t *, const video_format_t *);
static int  Direct3D11CreateGenericResources(vout_display_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static void Direct3D11DestroyPool(vout_display_t *);

static void DestroyDisplayPoolPicture(picture_t *);
static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static int SetupQuad(vout_display_t *, const video_format_t *, d3d_quad_t *, const RECT *,
                     const d3d_format_t *, ID3D11PixelShader *, video_projection_mode_t,
                     video_orientation_t);
static bool UpdateQuadPosition( vout_display_t *vd, d3d_quad_t *quad,
                                const RECT *output,
                                video_projection_mode_t projection,
                                video_orientation_t orientation );
static void ReleaseQuad(d3d_quad_t *);
static void UpdatePicQuadPosition(vout_display_t *);

static int Control(vout_display_t *vd, int query, va_list args);
static void Manage(vout_display_t *vd);

/* TODO: Move to a direct3d11_shaders header */
static const char* globVertexShaderFlat = "\
  struct VS_INPUT\
  {\
    float4 Position   : POSITION;\
    float4 Texture    : TEXCOORD0;\
  };\
  \
  struct VS_OUTPUT\
  {\
    float4 Position   : SV_POSITION;\
    float4 Texture    : TEXCOORD0;\
  };\
  \
  VS_OUTPUT main( VS_INPUT In )\
  {\
    return In;\
  }\
";

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

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
    float4 Texture    : TEXCOORD0;\
  };\
  \
  struct VS_OUTPUT\
  {\
    float4 Position   : SV_POSITION;\
    float4 Texture    : TEXCOORD0;\
  };\
  \
  VS_OUTPUT main( VS_INPUT In )\
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
    float BoundaryX;\
    float BoundaryY;\
    float LuminanceScale;\
  };\
  cbuffer PS_COLOR_TRANSFORM : register(b1)\
  {\
    float4x4 WhitePoint;\
    float4x4 Colorspace;\
  };\
  Texture2D%s shaderTexture[" STRINGIZE(D3D11_MAX_SHADER_VIEW) "];\
  SamplerState SamplerStates[2];\
  \
  struct PS_INPUT\
  {\
    float4 Position   : SV_POSITION;\
    float4 Texture    : TEXCOORD0;\
  };\
  \
  /* see http://filmicworlds.com/blog/filmic-tonemapping-operators/ */\
  inline float hable(float x) {\
      const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;\
      return ((x * (A*x + (C*B))+(D*E))/(x * (A*x + B) + (D*F))) - E/F;\
  }\
  \
  inline float3 hable(float3 x) {\
      x.r = hable(x.r);\
      x.g = hable(x.g);\
      x.b = hable(x.b);\
      return x;\
  }\
  \
  /* https://en.wikipedia.org/wiki/Hybrid_Log-Gamma#Technical_details */\
  inline float inverse_HLG(float x){\
      const float B67_a = 0.17883277;\
      const float B67_b = 0.28466892;\
      const float B67_c = 0.55991073;\
      const float B67_inv_r2 = 4.0; /* 1/0.5² */\
      if (x <= 0.5)\
          x = x * x * B67_inv_r2;\
      else\
          x = exp((x - B67_c) / B67_a) + B67_b;\
      return x;\
  }\
  \
  inline float3 sourceToLinear(float3 rgb) {\
      %s;\
  }\
  \
  inline float3 linearToDisplay(float3 rgb) {\
      %s;\
  }\
  \
  inline float3 toneMapping(float3 rgb) {\
      %s;\
  }\
  \
  inline float3 adjustRange(float3 rgb) {\
      %s;\
  }\
  \
  inline float4 sampleTexture(SamplerState samplerState, float4 coords) {\
      float4 sample;\
      %s /* sampling routine in sample */\
      return sample;\
  }\
  \
  float4 main( PS_INPUT In ) : SV_TARGET\
  {\
    float4 sample;\
    \
    if (In.Texture.x > BoundaryX || In.Texture.y > BoundaryY) \
        sample = sampleTexture( SamplerStates[1], In.Texture );\
    else\
        sample = sampleTexture( SamplerStates[0], In.Texture );\
    float4 rgba = mul(mul(sample, WhitePoint), Colorspace);\
    float opacity = rgba.a * Opacity;\
    float3 rgb = (float3)rgba;\
    rgb = sourceToLinear(rgb);\
    rgb = toneMapping(rgb);\
    rgb = linearToDisplay(rgb);\
    rgb = adjustRange(rgb);\
    return float4(rgb, saturate(opacity));\
  }\
";

#define ST2084_PQ_CONSTANTS  "const float ST2084_m1 = 2610.0 / (4096.0 * 4);\
const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\
const float ST2084_c1 = 3424.0 / 4096.0;\
const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\
const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;"

static int Direct3D11MapPoolTexture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    hr = ID3D11DeviceContext_Map(p_sys->context, p_sys->resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
    {
        return VLC_EGENERIC;
    }
    return CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
}

static void Direct3D11UnmapPoolTexture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ID3D11DeviceContext_Unmap(p_sys->context, p_sys->resource[KNOWN_DXGI_INDEX], 0);
}

#if !VLC_WINSTORE_APP
static int OpenHwnd(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    if (D3D11_Create(vd, &sys->hd3d) != VLC_SUCCESS)
        return VLC_EGENERIC;

    sys->hd3dcompiler_dll = Direct3D11LoadShaderLibrary();
    if (!sys->hd3dcompiler_dll) {
        msg_Err(vd, "cannot load d3dcompiler.dll, aborting");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }

    sys->OurD3DCompile = (void *)GetProcAddress(sys->hd3dcompiler_dll, "D3DCompile");
    if (!sys->OurD3DCompile) {
        msg_Err(vd, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        Direct3D11Destroy(vd);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
#else
static int OpenCoreW(vout_display_t *vd)
{
    IDXGISwapChain1* dxgiswapChain  = var_InheritInteger(vd, "winrt-swapchain");
    if (!dxgiswapChain)
        return VLC_EGENERIC;
    ID3D11DeviceContext* d3dcontext = var_InheritInteger(vd, "winrt-d3dcontext");
    if (!d3dcontext)
        return VLC_EGENERIC;
    ID3D11Device* d3ddevice = NULL;
    ID3D11DeviceContext_GetDevice(d3dcontext, &d3ddevice);
    if (!d3ddevice)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    sys->dxgiswapChain = dxgiswapChain;
    sys->d3d_dev.d3ddevice     = d3ddevice;
    sys->d3d_dev.d3dcontext    = d3dcontext;
    IDXGISwapChain_AddRef     (sys->dxgiswapChain);
    ID3D11Device_AddRef       (sys->d3d_dev.d3ddevice);
    ID3D11DeviceContext_AddRef(sys->d3d_dev.d3dcontext);

    return VLC_SUCCESS;
}
#endif

#if VLC_WINSTORE_APP
static bool GetRect(const vout_display_sys_win32_t *p_sys, RECT *out)
{
    const vout_display_sys_t *sys = (const vout_display_sys_t *)p_sys;
    out->left   = 0;
    out->top    = 0;
    uint32_t i_width;
    uint32_t i_height;
    UINT dataSize = sizeof(i_width);
    HRESULT hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
    if (FAILED(hr)) {
        return false;
    }
    dataSize = sizeof(i_height);
    hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_HEIGHT, &dataSize, &i_height);
    if (FAILED(hr)) {
        return false;
    }
    out->right  = i_width;
    out->bottom = i_height;
    return true;
}
#endif

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

#if VLC_WINSTORE_APP
    vd->sys->sys.pf_GetRect = GetRect;
#endif

    if (Direct3D11Open(vd)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

#if !VLC_WINSTORE_APP
    EventThreadUpdateTitle(vd->sys->sys.event, VOUT_TITLE " (Direct3D11 output)");
#endif
    msg_Dbg(vd, "Direct3D11 device adapter successfully initialized");

    vd->info.has_double_click     = true;
    vd->info.has_pictures_invalid = vd->info.is_slow;

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        vd->sys->d3dregion_format != NULL)
    {
        vd->sys->pSubpictureChromas[0] = vd->sys->d3dregion_format->fourcc;
        vd->sys->pSubpictureChromas[1] = 0;
        vd->info.subpicture_chromas = vd->sys->pSubpictureChromas;
    }
    else
        vd->info.subpicture_chromas = NULL;

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
    /* compensate for extra hardware decoding pulling extra pictures from our pool */
    pool_size += 2;

    vout_display_sys_t *sys = vd->sys;
    ID3D11Texture2D  *textures[pool_size * D3D11_MAX_SHADER_VIEW];
    picture_t **pictures = NULL;
    picture_t *picture;
    unsigned  picture_count = 0;

    memset(textures, 0, sizeof(textures));

    if (sys->sys.pool)
        return sys->sys.pool;

    if (vd->info.is_slow)
        pool_size = 1;

    video_format_t surface_fmt = vd->fmt;
    surface_fmt.i_width  = sys->picQuad.i_width;
    surface_fmt.i_height = sys->picQuad.i_height;

    if (SetupQuad( vd, &surface_fmt, &sys->picQuad, &sys->sys.rect_src_clipped,
                   sys->picQuadConfig, sys->picQuadPixelShader,
                   surface_fmt.projection_mode, vd->fmt.orientation ) != VLC_SUCCESS) {
        msg_Err(vd, "Could not Create the main quad picture.");
        return NULL;
    }

    if (!vd->info.is_slow) {
        HRESULT           hr;
        ID3D10Multithread *pMultithread;
        hr = ID3D11Device_QueryInterface( sys->d3d_dev.d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
        if (SUCCEEDED(hr)) {
            ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
            ID3D10Multithread_Release(pMultithread);
        }
    }

    if (sys->picQuadConfig->formatTexture == DXGI_FORMAT_UNKNOWN)
        sys->sys.pool = picture_pool_NewFromFormat( &surface_fmt, pool_size );
    else
    {
        unsigned slices = pool_size;
        if (!CanUseVoutPool(&sys->d3d_dev, pool_size))
            /* only provide enough for the filters, we can still do direct rendering */
            slices = __MIN(slices, 6);

        if (AllocateTextures(VLC_OBJECT(vd), &sys->d3d_dev, sys->picQuadConfig, &surface_fmt, slices, textures))
            goto error;

        pictures = calloc(pool_size, sizeof(*pictures));
        if (!pictures)
            goto error;

        for (picture_count = 0; picture_count < pool_size; picture_count++) {
            picture_sys_t *picsys = calloc(1, sizeof(*picsys));
            if (unlikely(picsys == NULL))
                goto error;

            for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
                picsys->texture[plane] = textures[picture_count * D3D11_MAX_SHADER_VIEW + plane];

            picsys->slice_index = picture_count;
            picsys->formatTexture = sys->picQuadConfig->formatTexture;
            picsys->context = sys->d3d_dev.d3dcontext;

            picture_resource_t resource = {
                .p_sys = picsys,
                .pf_destroy = DestroyDisplayPoolPicture,
            };

            picture = picture_NewFromResource(&surface_fmt, &resource);
            if (unlikely(picture == NULL)) {
                free(picsys);
                msg_Err( vd, "Failed to create picture %d in the pool.", picture_count );
                goto error;
            }

            pictures[picture_count] = picture;
            /* each picture_t holds a ref to the context and release it on Destroy */
            ID3D11DeviceContext_AddRef(picsys->context);
        }

#ifdef HAVE_ID3D11VIDEODECODER
        if (is_d3d11_opaque(surface_fmt.i_chroma) && !sys->legacy_shader)
#endif
        {
            sys->picQuad.resourceCount = DxgiResourceCount(sys->picQuadConfig);
            for (picture_count = 0; picture_count < pool_size; picture_count++) {
                if (!pictures[picture_count]->p_sys->texture[0])
                    continue;
                if (AllocateShaderView(VLC_OBJECT(vd), sys->d3d_dev.d3ddevice, sys->picQuadConfig,
                                       pictures[picture_count]->p_sys->texture, picture_count,
                                       pictures[picture_count]->p_sys->resourceView))
                    goto error;
            }
        }

        picture_pool_configuration_t pool_cfg = {
            .picture       = pictures,
            .picture_count = pool_size,
        };
        if (vd->info.is_slow && !is_d3d11_opaque(surface_fmt.i_chroma)) {
            pool_cfg.lock          = Direct3D11MapPoolTexture;
            //pool_cfg.unlock        = Direct3D11UnmapPoolTexture;
        }
        sys->sys.pool = picture_pool_NewExtended( &pool_cfg );
    }

error:
    if (sys->sys.pool == NULL) {
        picture_pool_configuration_t pool_cfg = {
            .picture_count = 0,
        };
        if (pictures) {
            msg_Dbg(vd, "Failed to create the picture d3d11 pool");
            for (unsigned i=0;i<picture_count; ++i)
                picture_Release(pictures[i]);
            free(pictures);
        }

        /* create an empty pool to avoid crashing */
        sys->sys.pool = picture_pool_NewExtended( &pool_cfg );
    } else {
        msg_Dbg(vd, "D3D11 pool succeed with %d surfaces (%dx%d) context 0x%p",
                pool_size, surface_fmt.i_width, surface_fmt.i_height, sys->d3d_dev.d3dcontext);
    }
    return sys->sys.pool;
}

static void DestroyDisplayPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ReleasePictureSys( p_sys );
    free(p_sys);
    free(picture);
}

static HRESULT UpdateBackBuffer(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    ID3D11Texture2D* pDepthStencil;
    ID3D11Texture2D* pBackBuffer;
    RECT rect;
#if VLC_WINSTORE_APP
    if (!GetRect(&sys->sys, &rect))
#endif
        rect = sys->sys.rect_dest_clipped;
    uint32_t i_width = RECTWidth(rect);
    uint32_t i_height = RECTHeight(rect);
    D3D11_TEXTURE2D_DESC dsc = { 0 };

    if (sys->d3drenderTargetView) {
        ID3D11Resource *res = NULL;
        ID3D11RenderTargetView_GetResource(sys->d3drenderTargetView, &res);
        if (res)
        {
            ID3D11Texture2D_GetDesc((ID3D11Texture2D*) res, &dsc);
            ID3D11Resource_Release(res);
        }
    }

    if (dsc.Width == i_width && dsc.Height == i_height)
        return S_OK; /* nothing changed */

    if (sys->d3drenderTargetView) {
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
        sys->d3drenderTargetView = NULL;
    }
    if (sys->d3ddepthStencilView) {
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
        sys->d3ddepthStencilView = NULL;
    }

    /* TODO detect is the size is the same as the output and switch to fullscreen mode */
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

    hr = ID3D11Device_CreateRenderTargetView(sys->d3d_dev.d3ddevice, (ID3D11Resource *)pBackBuffer, NULL, &sys->d3drenderTargetView);
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

    hr = ID3D11Device_CreateTexture2D(sys->d3d_dev.d3ddevice, &deptTexDesc, NULL, &pDepthStencil);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil texture. (hr=0x%lX)", hr);
       return hr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
    memset(&depthViewDesc, 0, sizeof(depthViewDesc));

    depthViewDesc.Format = deptTexDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = ID3D11Device_CreateDepthStencilView(sys->d3d_dev.d3ddevice, (ID3D11Resource *)pDepthStencil, &depthViewDesc, &sys->d3ddepthStencilView);
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
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}

static float UpdateFOVy(float f_fovx, float f_sar)
{
    return 2 * atanf(tanf(f_fovx / 2) / f_sar);
}

static float UpdateZ(float f_fovx, float f_fovy)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(f_fovx / 2);
    float tan_fovy_2 = tanf(f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    float f_z;
    if (f_fovx <= z_thresh * M_PI / 180)
        f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        f_z = f * f_fovx - f * z_thresh * M_PI / 180;
        if (f_z < z_min)
            f_z = z_min;
    }
    return f_z;
}

static void SetQuadVSProjection(vout_display_t *vd, d3d_quad_t *quad, const vlc_viewpoint_t *p_vp)
{
    if (!quad->pVertexShaderConstants)
        return;

#define RAD(d) ((float) ((d) * M_PI / 180.f))
    float f_fovx = RAD(p_vp->fov);
    if ( f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f ||
         f_fovx < -0.001f )
        return;

    float f_sar = (float) vd->cfg->display.width / vd->cfg->display.height;
    float f_teta = RAD(p_vp->yaw) - (float) M_PI_2;
    float f_phi  = RAD(p_vp->pitch);
    float f_roll = RAD(p_vp->roll);
    float f_fovy = UpdateFOVy(f_fovx, f_sar);
    float f_z = UpdateZ(f_fovx, f_fovy);

    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        VS_PROJECTION_CONST *dst_data = mapped.pData;
        getXRotMatrix(f_phi, dst_data->RotX);
        getYRotMatrix(f_teta,   dst_data->RotY);
        getZRotMatrix(f_roll,  dst_data->RotZ);
        getZoomMatrix(SPHERE_RADIUS * f_z, dst_data->View);
        getProjectionMatrix(f_sar, f_fovy, dst_data->Projection);
    }
    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0);
#undef RAD
}

static void UpdateSize(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    msg_Dbg(vd, "Detected size change %dx%d", RECTWidth(sys->sys.rect_dest_clipped),
            RECTHeight(sys->sys.rect_dest_clipped));

    UpdateBackBuffer(vd);

#if defined(HAVE_ID3D11VIDEODECODER)
    if( sys->context_lock != INVALID_HANDLE_VALUE )
    {
        WaitForSingleObjectEx( sys->context_lock, INFINITE, FALSE );
    }
#endif

    UpdatePicQuadPosition(vd);

    UpdateQuadPosition(vd, &sys->picQuad, &sys->sys.rect_src_clipped,
                       vd->fmt.projection_mode, vd->fmt.orientation);

#if defined(HAVE_ID3D11VIDEODECODER)
    if( sys->context_lock != INVALID_HANDLE_VALUE )
    {
        ReleaseMutex( sys->context_lock );
    }
#endif
}

static inline bool RectEquals(const RECT *r1, const RECT *r2)
{
    return r1->bottom == r2->bottom && r1->top == r2->top &&
           r1->left == r2->left && r1->right == r2->right;
}

#define BEFORE_UPDATE_RECTS \
    unsigned int i_outside_width  = vd->fmt.i_width; \
    unsigned int i_outside_height = vd->fmt.i_height; \
    vd->fmt.i_width  = vd->sys->picQuad.i_width; \
    vd->fmt.i_height = vd->sys->picQuad.i_height;
#define AFTER_UPDATE_RECTS \
    vd->fmt.i_width  = i_outside_width; \
    vd->fmt.i_height = i_outside_height;

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    RECT before_src_clipped  = sys->sys.rect_src_clipped;
    RECT before_dest_clipped = sys->sys.rect_dest_clipped;
    RECT before_dest         = sys->sys.rect_dest;

    BEFORE_UPDATE_RECTS;
    int res = CommonControl( vd, query, args );
    AFTER_UPDATE_RECTS;

    if (query == VOUT_DISPLAY_CHANGE_VIEWPOINT)
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t*);
        if ( sys->picQuad.pVertexShaderConstants )
        {
            SetQuadVSProjection( vd, &sys->picQuad, &cfg->viewpoint );
            res = VLC_SUCCESS;
        }
    }

    if (!RectEquals(&before_src_clipped,  &sys->sys.rect_src_clipped) ||
        !RectEquals(&before_dest_clipped, &sys->sys.rect_dest_clipped) ||
        !RectEquals(&before_dest,         &sys->sys.rect_dest) )
    {
        UpdateSize(vd);
    }

    return res;
}

static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    RECT before_src_clipped  = sys->sys.rect_src_clipped;
    RECT before_dest_clipped = sys->sys.rect_dest_clipped;
    RECT before_dest         = sys->sys.rect_dest;

    BEFORE_UPDATE_RECTS;
    CommonManage(vd);
    AFTER_UPDATE_RECTS;

    if (!RectEquals(&before_src_clipped, &sys->sys.rect_src_clipped) ||
        !RectEquals(&before_dest_clipped, &sys->sys.rect_dest_clipped) ||
        !RectEquals(&before_dest, &sys->sys.rect_dest))
    {
        UpdateSize(vd);
    }
}

static bool D3D11_ShaderUpdateConstants(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        msg_Err(o, "Failed to lock the picture shader constants (hr=0x%lX)", hr);
        return false;
    }

    PS_CONSTANT_BUFFER *dst_data = mappedResource.pData;
    *dst_data = quad->shaderConstants;
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[0], 0);
    return true;
}

static void UpdateQuadLuminanceScale(vout_display_t *vd, d3d_quad_t *quad, float luminanceScale)
{
    vout_display_sys_t *sys = vd->sys;
    if (quad->shaderConstants.LuminanceScale == luminanceScale)
        return;

    float old = quad->shaderConstants.LuminanceScale;
    quad->shaderConstants.LuminanceScale = luminanceScale;
    if (!D3D11_ShaderUpdateConstants(VLC_OBJECT(vd), &sys->d3d_dev, quad))
        quad->shaderConstants.LuminanceScale = old;
}

static void DisplayD3DPicture(vout_display_sys_t *sys, d3d_quad_t *quad, ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW])
{
    UINT stride = sizeof(d3d_vertex_t);
    UINT offset = 0;

    /* Render the quad */
    /* vertex shader */
    ID3D11DeviceContext_IASetVertexBuffers(sys->d3d_dev.d3dcontext, 0, 1, &quad->pVertexBuffer, &stride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(sys->d3d_dev.d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->pVertexShaderConstants )
        ID3D11DeviceContext_VSSetConstantBuffers(sys->d3d_dev.d3dcontext, 0, 1, &quad->pVertexShaderConstants);

    ID3D11DeviceContext_VSSetShader(sys->d3d_dev.d3dcontext, quad->d3dvertexShader, NULL, 0);

    /* pixel shader */
    ID3D11DeviceContext_PSSetShader(sys->d3d_dev.d3dcontext, quad->d3dpixelShader, NULL, 0);

    ID3D11DeviceContext_PSSetConstantBuffers(sys->d3d_dev.d3dcontext, 0, quad->PSConstantsCount, quad->pPixelShaderConstants);
    ID3D11DeviceContext_PSSetShaderResources(sys->d3d_dev.d3dcontext, 0, quad->resourceCount, resourceView);

    ID3D11DeviceContext_RSSetViewports(sys->d3d_dev.d3dcontext, 1, &quad->cropViewport);

    ID3D11DeviceContext_DrawIndexed(sys->d3d_dev.d3dcontext, quad->indexCount, 0, 0);
}

static float GetFormatLuminance(vlc_object_t *o, const video_format_t *fmt)
{
    switch (fmt->transfer)
    {
        case TRANSFER_FUNC_SMPTE_ST2084:
            /* that's the default PQ value if the metadata are not set */
            return MAX_PQ_BRIGHTNESS;
        case TRANSFER_FUNC_HLG:
            return 1000;
        case TRANSFER_FUNC_BT470_BG:
        case TRANSFER_FUNC_BT470_M:
        case TRANSFER_FUNC_BT709:
        case TRANSFER_FUNC_SRGB:
            return DEFAULT_BRIGHTNESS;
        default:
            msg_Dbg(o, "unhandled source transfer %d", fmt->transfer);
            return DEFAULT_BRIGHTNESS;
    }
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picQuadConfig->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        D3D11_TEXTURE2D_DESC texDesc;
        int i;
        HRESULT hr;
        plane_t planes[PICTURE_PLANE_MAX];

        bool b_mapped = true;
        for (i = 0; i < picture->i_planes; i++) {
            hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i],
                                         0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if( unlikely(FAILED(hr)) )
            {
                while (i-- > 0)
                    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i], 0);
                b_mapped = false;
                break;
            }
            ID3D11Texture2D_GetDesc(sys->stagingSys.texture[i], &texDesc);
            planes[i].i_lines = texDesc.Height;
            planes[i].i_pitch = mappedResource.RowPitch;
            planes[i].p_pixels = mappedResource.pData;

            planes[i].i_visible_lines = picture->p[i].i_visible_lines;
            planes[i].i_visible_pitch = picture->p[i].i_visible_pitch;
        }

        if (b_mapped)
        {
            for (i = 0; i < picture->i_planes; i++)
                plane_CopyPixels(&planes[i], &picture->p[i]);

            for (i = 0; i < picture->i_planes; i++)
                ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i], 0);
        }
    }
    else
    {
        picture_sys_t *p_sys = ActivePictureSys(picture);

#if defined(HAVE_ID3D11VIDEODECODER)
        if (sys->context_lock != INVALID_HANDLE_VALUE && is_d3d11_opaque(picture->format.i_chroma))
            WaitForSingleObjectEx( sys->context_lock, INFINITE, FALSE );
#endif
        if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader) {
            D3D11_TEXTURE2D_DESC srcDesc,texDesc;
            if (!is_d3d11_opaque(picture->format.i_chroma))
                Direct3D11UnmapPoolTexture(picture);
            ID3D11Texture2D_GetDesc(p_sys->texture[KNOWN_DXGI_INDEX], &srcDesc);
            ID3D11Texture2D_GetDesc(sys->stagingSys.texture[0], &texDesc);
            D3D11_BOX box = {
                .top = 0,
                .bottom = __MIN(srcDesc.Height, texDesc.Height),
                .left = 0,
                .right = __MIN(srcDesc.Width, texDesc.Width),
                .back = 1,
            };
            ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev.d3dcontext,
                                                      sys->stagingSys.resource[KNOWN_DXGI_INDEX],
                                                      0, 0, 0, 0,
                                                      p_sys->resource[KNOWN_DXGI_INDEX],
                                                      p_sys->slice_index, &box);
        }
        else
        {
            D3D11_TEXTURE2D_DESC texDesc;
            ID3D11Texture2D_GetDesc(p_sys->texture[0], &texDesc);
            if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
            {
                /* for performance reason we don't want to allocate this during
                 * display, do it preferrably when creating the texture */
                assert(p_sys->resourceView[0]!=NULL);
            }
            if ( sys->picQuad.i_height != texDesc.Height ||
                 sys->picQuad.i_width != texDesc.Width )
            {
                /* the decoder produced different sizes than the vout, we need to
                 * adjust the vertex */
                sys->picQuad.i_height = texDesc.Height;
                sys->picQuad.i_width = texDesc.Width;

                BEFORE_UPDATE_RECTS;
                UpdateRects(vd, NULL, true);
                AFTER_UPDATE_RECTS;
                UpdateSize(vd);
            }
        }
    }

    if (subpicture) {
        int subpicture_region_count    = 0;
        picture_t **subpicture_regions = NULL;
        Direct3D11MapSubpicture(vd, &subpicture_region_count, &subpicture_regions, subpicture);
        Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregions      = subpicture_regions;
    }

    if (picture->format.mastering.max_luminance)
    {
        UpdateQuadLuminanceScale(vd, &sys->picQuad, GetFormatLuminance(VLC_OBJECT(vd), &picture->format) / (float)sys->display.luminance_peak);

        if (sys->dxgiswapChain4)
        {
            DXGI_HDR_METADATA_HDR10 hdr10 = {0};
            hdr10.GreenPrimary[0] = picture->format.mastering.primaries[0];
            hdr10.GreenPrimary[1] = picture->format.mastering.primaries[1];
            hdr10.BluePrimary[0]  = picture->format.mastering.primaries[2];
            hdr10.BluePrimary[1]  = picture->format.mastering.primaries[3];
            hdr10.RedPrimary[0]   = picture->format.mastering.primaries[4];
            hdr10.RedPrimary[1]   = picture->format.mastering.primaries[5];
            hdr10.WhitePoint[0] = picture->format.mastering.white_point[0];
            hdr10.WhitePoint[1] = picture->format.mastering.white_point[1];
            hdr10.MinMasteringLuminance = picture->format.mastering.min_luminance;
            hdr10.MaxMasteringLuminance = picture->format.mastering.max_luminance;
            hdr10.MaxContentLightLevel = picture->format.lighting.MaxCLL;
            hdr10.MaxFrameAverageLightLevel = picture->format.lighting.MaxFALL;
            IDXGISwapChain4_SetHDRMetaData(sys->dxgiswapChain4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr10), &hdr10);
        }
    }

    FLOAT blackRGBA[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(sys->d3d_dev.d3dcontext, sys->d3drenderTargetView, blackRGBA);

    /* no ID3D11Device operations should come here */

    ID3D11DeviceContext_OMSetRenderTargets(sys->d3d_dev.d3dcontext, 1, &sys->d3drenderTargetView, sys->d3ddepthStencilView);

    ID3D11DeviceContext_ClearDepthStencilView(sys->d3d_dev.d3dcontext, sys->d3ddepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    /* Render the quad */
    if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader)
        DisplayD3DPicture(sys, &sys->picQuad, sys->stagingSys.resourceView);
    else {
        picture_sys_t *p_sys = ActivePictureSys(picture);
        DisplayD3DPicture(sys, &sys->picQuad, p_sys->resourceView);
    }

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
            {
                d3d_quad_t *quad = (d3d_quad_t *) sys->d3dregions[i]->p_sys;
                DisplayD3DPicture(sys, quad, quad->picSys.resourceView);
            }
        }
    }

#if defined(HAVE_ID3D11VIDEODECODER)
    if (sys->context_lock != INVALID_HANDLE_VALUE && is_d3d11_opaque(picture->format.i_chroma))
    {
        ReleaseMutex( sys->context_lock );
    }
#endif

    ID3D11DeviceContext_Flush(sys->d3d_dev.d3dcontext);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    DXGI_PRESENT_PARAMETERS presentParams;
    memset(&presentParams, 0, sizeof(presentParams));
    HRESULT hr = IDXGISwapChain1_Present1(sys->dxgiswapChain, 0, 0, &presentParams);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        /* TODO device lost */
        msg_Dbg(vd, "SwapChain Present failed. (hr=0x%lX)", hr);
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

    if (sys->hd3dcompiler_dll)
        FreeLibrary(sys->hd3dcompiler_dll);

    sys->OurD3DCompile = NULL;
    sys->hdxgi_dll = NULL;
    sys->hd3dcompiler_dll = NULL;
    D3D11_Destroy( &vd->sys->hd3d );
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

#define COLOR_RANGE_FULL   1 /* 0-255 */
#define COLOR_RANGE_STUDIO 0 /* 16-235 */

#define TRANSFER_FUNC_10    TRANSFER_FUNC_LINEAR
#define TRANSFER_FUNC_22    TRANSFER_FUNC_SRGB
#define TRANSFER_FUNC_2084  TRANSFER_FUNC_SMPTE_ST2084

#define COLOR_PRIMARIES_BT601  COLOR_PRIMARIES_BT601_525

static const dxgi_color_space color_spaces[] = {
#define DXGIMAP(AXIS, RANGE, GAMMA, SITTING, PRIMARIES) \
    { DXGI_COLOR_SPACE_##AXIS##_##RANGE##_G##GAMMA##_##SITTING##_P##PRIMARIES, \
      #AXIS " Rec." #PRIMARIES " gamma:" #GAMMA " range:" #RANGE, \
      COLOR_AXIS_##AXIS, COLOR_PRIMARIES_BT##PRIMARIES, TRANSFER_FUNC_##GAMMA, \
      COLOR_SPACE_BT##PRIMARIES, COLOR_RANGE_##RANGE},

    DXGIMAP(RGB,   FULL,     22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   601)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   601)
    DXGIMAP(RGB,   FULL,     10,    NONE,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   709)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,  2020)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,  2020)
    DXGIMAP(YCBCR, STUDIO,   22, TOPLEFT,  2020)
    DXGIMAP(RGB,   FULL,     22,    NONE,  2020)
    DXGIMAP(RGB,   FULL,   2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084,    LEFT,  2020)
    DXGIMAP(RGB,   STUDIO, 2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084, TOPLEFT,  2020)
    /*DXGIMAP(YCBCR, FULL,     22,    NONE,  2020, 601)*/
    {DXGI_COLOR_SPACE_RESERVED, NULL, 0, 0, 0, 0, 0},
#undef DXGIMAP
};

static void D3D11SetColorSpace(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    int best = -1;
    int score, best_score = 0;
    UINT support;
    IDXGIOutput *dxgiOutput = NULL;
    IDXGISwapChain3 *dxgiswapChain3 = NULL;
    sys->display.colorspace = &color_spaces[0];

    hr = IDXGISwapChain_QueryInterface( sys->dxgiswapChain, &IID_IDXGISwapChain3, (void **)&dxgiswapChain3);
    if (FAILED(hr)) {
        msg_Warn(vd, "could not get a IDXGISwapChain3");
        goto done;
    }

    bool src_full_range = vd->source.b_color_range_full ||
                          /* the YUV->RGB conversion already output full range */
                          is_d3d11_opaque(vd->source.i_chroma) ||
                          vlc_fourcc_IsYUV(vd->source.i_chroma);

    /* pick the best output based on color support and transfer */
    /* TODO support YUV output later */
    for (int i=0; color_spaces[i].name; ++i)
    {
        hr = IDXGISwapChain3_CheckColorSpaceSupport(dxgiswapChain3, color_spaces[i].dxgi, &support);
        if (SUCCEEDED(hr) && support) {
            msg_Dbg(vd, "supports colorspace %s", color_spaces[i].name);
            score = 0;
            if (color_spaces[i].primaries == vd->source.primaries)
                score++;
            if (color_spaces[i].color == vd->source.space)
                score += 2; /* we don't want to translate color spaces */
            if (color_spaces[i].transfer == vd->source.transfer ||
                /* favor 2084 output for HLG source */
                (color_spaces[i].transfer == TRANSFER_FUNC_SMPTE_ST2084 && vd->source.transfer == TRANSFER_FUNC_HLG))
                score++;
            if (color_spaces[i].b_full_range == src_full_range)
                score++;
            if (score > best_score || (score && best == -1)) {
                best = i;
                best_score = score;
            }
        }
    }

    if (best == -1)
    {
        best = 0;
        msg_Warn(vd, "no matching colorspace found force %s", color_spaces[best].name);
    }

#ifdef HAVE_DXGI1_6_H
    if (SUCCEEDED(IDXGISwapChain_GetContainingOutput( sys->dxgiswapChain, &dxgiOutput )))
    {
        IDXGIOutput6 *dxgiOutput6 = NULL;
        if (SUCCEEDED(IDXGIOutput_QueryInterface( dxgiOutput, &IID_IDXGIOutput6, (void **)&dxgiOutput6 )))
        {
            DXGI_OUTPUT_DESC1 desc1;
            if (SUCCEEDED(IDXGIOutput6_GetDesc1( dxgiOutput6, &desc1 )))
            {
                const dxgi_color_space *csp = NULL;
                for (int i=0; color_spaces[i].name; ++i)
                {
                    if (color_spaces[i].dxgi == desc1.ColorSpace)
                    {
                        best = i;
                        csp = &color_spaces[i];
                        break;
                    }
                }

                msg_Dbg(vd, "Output max luminance: %.1f, colorspace %s, bits per pixel %d", desc1.MaxFullFrameLuminance, csp?csp->name:"unknown", desc1.BitsPerColor);
                //sys->display.luminance_peak = desc1.MaxFullFrameLuminance;
            }
            IDXGIOutput6_Release( dxgiOutput6 );
        }
        IDXGIOutput_Release( dxgiOutput );
    }
#endif

    hr = IDXGISwapChain3_SetColorSpace1(dxgiswapChain3, color_spaces[best].dxgi);
    if (SUCCEEDED(hr))
    {
        sys->display.colorspace = &color_spaces[best];
        msg_Dbg(vd, "using colorspace %s", sys->display.colorspace->name);
    }
    else
        msg_Err(vd, "Failed to set colorspace %s. (hr=0x%lX)", sys->display.colorspace->name, hr);
done:
    /* guestimate the display peak luminance */
    switch (sys->display.colorspace->transfer)
    {
    case TRANSFER_FUNC_LINEAR:
    case TRANSFER_FUNC_SRGB:
        sys->display.luminance_peak = DEFAULT_SRGB_BRIGHTNESS;
        break;
    case TRANSFER_FUNC_SMPTE_ST2084:
        sys->display.luminance_peak = MAX_PQ_BRIGHTNESS;
        break;
    /* there is no other output transfer on Windows */
    default:
        vlc_assert_unreachable();
    }

    if (dxgiswapChain3)
        IDXGISwapChain3_Release(dxgiswapChain3);
}

static const d3d_format_t *GetDirectRenderingFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (is_d3d11_opaque(i_src_chroma))
        supportFlags |= D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd->sys->d3d_dev.d3ddevice, i_src_chroma, false, 0, is_d3d11_opaque(i_src_chroma), supportFlags );
}

static const d3d_format_t *GetDirectDecoderFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd->sys->d3d_dev.d3ddevice, i_src_chroma, false, 0, is_d3d11_opaque(i_src_chroma), supportFlags );
}

static const d3d_format_t *GetDisplayFormatByDepth(vout_display_t *vd, uint8_t bit_depth, bool from_processor, bool rgb_only)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (from_processor)
        supportFlags |= D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
    return FindD3D11Format( vd->sys->d3d_dev.d3ddevice, 0, rgb_only, bit_depth, false, supportFlags );
}

static const d3d_format_t *GetBlendableFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_BLENDABLE;
    return FindD3D11Format( vd->sys->d3d_dev.d3ddevice, i_src_chroma, false, 0, false, supportFlags );
}

static int Direct3D11Open(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    IDXGIFactory2 *dxgifactory;

#if !VLC_WINSTORE_APP
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 scd;
    memset(&scd, 0, sizeof(scd));
    scd.BufferCount = 3;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Width = vd->source.i_visible_width;
    scd.Height = vd->source.i_visible_height;
    switch(vd->source.i_chroma)
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

    hr = D3D11_CreateDevice(vd, &sys->hd3d,
                            is_d3d11_opaque(vd->source.i_chroma),
                            &sys->d3d_dev);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not Create the D3D11 device. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(sys->d3d_dev.d3ddevice);
    if (unlikely(dxgiadapter==NULL)) {
       msg_Err(vd, "Could not get the DXGI Adapter");
       return VLC_EGENERIC;
    }

    hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)sys->d3d_dev.d3ddevice,
                                              sys->sys.hvideownd, &scd, NULL, NULL, &sys->dxgiswapChain);
    IDXGIFactory2_Release(dxgifactory);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
#endif

    IDXGISwapChain_QueryInterface( sys->dxgiswapChain, &IID_IDXGISwapChain4, (void **)&sys->dxgiswapChain4);

    D3D11SetColorSpace(vd);

    video_format_t fmt;
    video_format_Copy(&fmt, &vd->source);
    int err = SetupOutputFormat(vd, &fmt);
    if (err != VLC_SUCCESS)
    {
        if (!is_d3d11_opaque(vd->source.i_chroma)
#if !VLC_WINSTORE_APP
            && vd->obj.force
#endif
                )
        {
            const vlc_fourcc_t *list = vlc_fourcc_IsYUV(vd->source.i_chroma) ?
                        vlc_fourcc_GetYUVFallback(vd->source.i_chroma) :
                        vlc_fourcc_GetRGBFallback(vd->source.i_chroma);
            for (unsigned i = 0; list[i] != 0; i++) {
                fmt.i_chroma = list[i];
                if (fmt.i_chroma == vd->source.i_chroma)
                    continue;
                err = SetupOutputFormat(vd, &fmt);
                if (err == VLC_SUCCESS)
                    break;
            }
        }
        if (err != VLC_SUCCESS)
            return err;
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
        Direct3D11DestroyResources(vd);
        return VLC_EGENERIC;
    }

    video_format_Clean(&vd->fmt);
    vd->fmt = fmt;

    return VLC_SUCCESS;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    // look for the requested pixel format first
    sys->picQuadConfig = GetDirectRenderingFormat(vd, fmt->i_chroma);

    /* look for a decoder format that can be decoded but not used in shaders */
    const d3d_format_t *decoder_format = NULL;
    if ( !sys->picQuadConfig && is_d3d11_opaque(fmt->i_chroma) )
        decoder_format = GetDirectDecoderFormat(vd, fmt->i_chroma);
    else
        decoder_format = sys->picQuadConfig;

    // look for any pixel format that we can handle with enough pixels per channel
    if ( !sys->picQuadConfig )
    {
        uint8_t bits_per_channel;
        switch (fmt->i_chroma)
        {
        case VLC_CODEC_D3D11_OPAQUE:
            bits_per_channel = 8;
            break;
        case VLC_CODEC_D3D11_OPAQUE_10B:
            bits_per_channel = 10;
            break;
        default:
            {
                const vlc_chroma_description_t *p_format = vlc_fourcc_GetChromaDescription(fmt->i_chroma);
                bits_per_channel = p_format == NULL ||
                                   p_format->pixel_bits == 0 ? 8 : p_format->pixel_bits /
                                                               (p_format->plane_count==1 ? p_format->pixel_size : 1);
            }
            break;
        }

        bool is_rgb = !vlc_fourcc_IsYUV(fmt->i_chroma);
        sys->picQuadConfig = GetDisplayFormatByDepth(vd, bits_per_channel, decoder_format!=NULL, is_rgb);
        if (!sys->picQuadConfig && is_rgb)
            sys->picQuadConfig = GetDisplayFormatByDepth(vd, bits_per_channel, decoder_format!=NULL, false);
    }

    // look for any pixel format that we can handle
    if ( !sys->picQuadConfig )
        sys->picQuadConfig = GetDisplayFormatByDepth(vd, 0, false, false);

    if ( !sys->picQuadConfig )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    fmt->i_chroma = decoder_format ? decoder_format->fourcc : sys->picQuadConfig->fourcc;

    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", sys->picQuadConfig->name,
                 (char *)&fmt->i_chroma );
    DxgiFormatMask( sys->picQuadConfig->formatTexture, fmt );

    /* check the region pixel format */
    sys->d3dregion_format = GetBlendableFormat(vd, VLC_CODEC_RGBA);
    if (!sys->d3dregion_format)
        sys->d3dregion_format = GetBlendableFormat(vd, VLC_CODEC_BGRA);

    if (Direct3D11CreateFormatResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate format resources");
        Direct3D11DestroyResources(vd);
        return VLC_EGENERIC;
    }
    vd->fmt = *fmt;

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);
    if (sys->dxgiswapChain4)
    {
        IDXGISwapChain_Release(sys->dxgiswapChain4);
        sys->dxgiswapChain4 = NULL;
    }
    if (sys->dxgiswapChain)
    {
        IDXGISwapChain_Release(sys->dxgiswapChain);
        sys->dxgiswapChain = NULL;
    }

    D3D11_ReleaseDevice( &sys->d3d_dev );

    msg_Dbg(vd, "Direct3D11 device adapter closed");
}

static void UpdatePicQuadPosition(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    sys->picQuad.cropViewport.Width    = RECTWidth(sys->sys.rect_dest_clipped);
    sys->picQuad.cropViewport.Height   = RECTHeight(sys->sys.rect_dest_clipped);
    sys->picQuad.cropViewport.TopLeftX = sys->sys.rect_dest_clipped.left;
    sys->picQuad.cropViewport.TopLeftY = sys->sys.rect_dest_clipped.top;

    sys->picQuad.cropViewport.MinDepth = 0.0f;
    sys->picQuad.cropViewport.MaxDepth = 1.0f;

    SetQuadVSProjection(vd, &sys->picQuad, &vd->cfg->viewpoint);

#ifndef NDEBUG
    msg_Dbg(vd, "picQuad position (%.02f,%.02f) %.02fx%.02f", sys->picQuad.cropViewport.TopLeftX, sys->picQuad.cropViewport.TopLeftY, sys->picQuad.cropViewport.Width, sys->picQuad.cropViewport.Height );
#endif
}

static ID3DBlob* CompileShader(vout_display_t *vd, const char *psz_shader, bool pixel)
{
    vout_display_sys_t *sys = vd->sys;
    ID3DBlob* pShaderBlob = NULL, *pErrBlob;

    /* TODO : Match the version to the D3D_FEATURE_LEVEL */
    HRESULT hr = D3DCompile(psz_shader, strlen(psz_shader),
                            NULL, NULL, NULL, "main",
                            pixel ? (sys->legacy_shader ? "ps_4_0_level_9_1" : "ps_4_0") :
                                    (sys->legacy_shader ? "vs_4_0_level_9_1" : "vs_4_0"),
                            0, 0, &pShaderBlob, &pErrBlob);

    if (FAILED(hr)) {
        char *err = pErrBlob ? ID3D10Blob_GetBufferPointer(pErrBlob) : NULL;
        msg_Err(vd, "invalid %s Shader (hr=0x%lX): %s", pixel?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        return NULL;
    }
    return pShaderBlob;
}

static bool IsRGBShader(const d3d_format_t *cfg)
{
    return cfg->resourceFormat[0] != DXGI_FORMAT_R8_UNORM &&
           cfg->resourceFormat[0] != DXGI_FORMAT_R16_UNORM &&
           cfg->formatTexture != DXGI_FORMAT_YUY2;
}

static HRESULT CompilePixelShader(vout_display_t *vd, const d3d_format_t *format,
                                  video_transfer_func_t transfer, bool src_full_range,
                                  ID3D11PixelShader **output)
{
    vout_display_sys_t *sys = vd->sys;

    static const char *DEFAULT_NOOP = "return rgb";
    const char *psz_sampler;
    const char *psz_src_transform     = DEFAULT_NOOP;
    const char *psz_display_transform = DEFAULT_NOOP;
    const char *psz_tone_mapping      = DEFAULT_NOOP;
    const char *psz_adjust_range      = DEFAULT_NOOP;
    char *psz_range = NULL;

    switch (format->formatTexture)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        psz_sampler =
                "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\
                sample.yz = shaderTexture[1].Sample(samplerState, coords).xy;\
                sample.a  = 1;";
        break;
    case DXGI_FORMAT_YUY2:
        psz_sampler =
                "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\
                sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\
                sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\
                sample.a  = 1;";
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
        psz_sampler =
                "sample = shaderTexture[0].Sample(samplerState, coords);";
        break;
    case DXGI_FORMAT_UNKNOWN:
        if (format->fourcc == VLC_CODEC_I420_10L)
            psz_sampler =
                   "sample.x  = shaderTexture[0].Sample(samplerState, coords).x * 64;\
                    sample.y  = shaderTexture[1].Sample(samplerState, coords).x * 64;\
                    sample.z  = shaderTexture[2].Sample(samplerState, coords).x * 64;\
                    sample.a  = 1;";
        else
            psz_sampler =
                   "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\
                    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\
                    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\
                    sample.a  = 1;";
        break;
    default:
        vlc_assert_unreachable();
    }

    video_transfer_func_t src_transfer;

    if (transfer != sys->display.colorspace->transfer)
    {
        /* we need to go in linear mode */
        switch (transfer)
        {
            case TRANSFER_FUNC_SMPTE_ST2084:
                /* ST2084 to Linear */
                psz_src_transform =
                       ST2084_PQ_CONSTANTS
                       "rgb = pow(rgb, 1.0/ST2084_m2);\
                        rgb = max(rgb - ST2084_c1, 0.0) / (ST2084_c2 - ST2084_c3 * rgb);\
                        rgb = pow(rgb, 1.0/ST2084_m1);\
                        return rgb";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_HLG:
                /* HLG to Linear */
                psz_src_transform =
                       "rgb.r = inverse_HLG(rgb.r);\
                        rgb.g = inverse_HLG(rgb.g);\
                        rgb.b = inverse_HLG(rgb.b);\
                        return rgb / 20.0";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT709:
                psz_src_transform = "return pow(rgb, 1.0 / 0.45)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_M:
            case TRANSFER_FUNC_SRGB:
                psz_src_transform = "return pow(rgb, 2.2)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_BG:
                psz_src_transform = "return pow(rgb, 2.8)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            default:
                msg_Dbg(vd, "unhandled source transfer %d", transfer);
                src_transfer = transfer;
                break;
        }

        switch (sys->display.colorspace->transfer)
        {
            case TRANSFER_FUNC_SRGB:
                if (src_transfer == TRANSFER_FUNC_LINEAR)
                {
                    /* Linear to sRGB */
                    psz_display_transform = "return pow(rgb, 1.0 / 2.2)";

                    if (transfer == TRANSFER_FUNC_SMPTE_ST2084 || transfer == TRANSFER_FUNC_HLG)
                    {
                        /* HDR tone mapping */
                        psz_tone_mapping =
                            "static const float3 HABLE_DIV = hable(11.2);\
                            rgb = hable(rgb * LuminanceScale) / HABLE_DIV;\
                            return rgb";
                    }
                }
                else
                    msg_Warn(vd, "don't know how to transfer from %d to sRGB", src_transfer);
                break;

            case TRANSFER_FUNC_SMPTE_ST2084:
                if (src_transfer == TRANSFER_FUNC_LINEAR)
                {
                    /* Linear to ST2084 */
                    psz_display_transform =
                           ST2084_PQ_CONSTANTS
                           "rgb = pow(rgb, ST2084_m1);\
                            rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);\
                            rgb = pow(rgb, ST2084_m2);\
                            return rgb";
                }
                else
                    msg_Warn(vd, "don't know how to transfer from %d to SMPTE ST 2084", src_transfer);
                break;
            default:
                msg_Warn(vd, "don't know how to transfer from %d to %d", src_transfer, sys->display.colorspace->transfer);
                break;
        }
    }

    int range_adjust = 0;
    if (sys->display.colorspace->b_full_range) {
        if (!src_full_range)
            range_adjust = 1; /* raise the source to full range */
    } else {
        if (src_full_range)
            range_adjust = -1; /* lower the source to studio range */
    }
    if (!IsRGBShader(format))
        range_adjust--; /* the YUV->RGB conversion already output full range */

    if (range_adjust != 0)
    {
        psz_range = malloc(256);
        if (likely(psz_range))
        {
            FLOAT itu_black_level;
            FLOAT itu_range_factor;
            FLOAT itu_white_level;
            switch (format->bitsPerChannel)
            {
            case 8:
                /* Rec. ITU-R BT.709-6 §4.6 */
                itu_black_level  =              16.f / 255.f;
                itu_white_level  =             235.f / 255.f;
                itu_range_factor = (float)(235 - 16) / 255.f;
                break;
            case 10:
                /* Rec. ITU-R BT.709-6 §4.6 */
                itu_black_level  =              64.f / 1023.f;
                itu_white_level  =             940.f / 1023.f;
                itu_range_factor = (float)(940 - 64) / 1023.f;
                break;
            case 12:
                /* Rec. ITU-R BT.2020-2 Table 5 */
                itu_black_level  =               256.f / 4095.f;
                itu_white_level  =              3760.f / 4095.f;
                itu_range_factor = (float)(3760 - 256) / 4095.f;
                break;
            default:
                /* unknown bitdepth, use approximation for infinite bit depth */
                itu_black_level  =              16.f / 256.f;
                itu_white_level  =             235.f / 256.f;
                itu_range_factor = (float)(235 - 16) / 256.f;
                break;
            }

            FLOAT black_level = 0;
            FLOAT range_factor = 1.0f;
            if (range_adjust > 0)
            {
                /* expand the range from studio to full range */
                while (range_adjust--)
                {
                    black_level -= itu_black_level;
                    range_factor /= itu_range_factor;
                }
                sprintf(psz_range, "return max(0,min(1,(rgb + %f) * %f))",
                        black_level, range_factor);
            }
            else
            {
                /* shrink the range to studio range */
                while (range_adjust++)
                {
                    black_level += itu_black_level;
                    range_factor *= itu_range_factor;
                }
                sprintf(psz_range, "return clamp(rgb + %f * %f,%f,%f)",
                        black_level, range_factor, itu_black_level, itu_white_level);
            }
            psz_adjust_range = psz_range;
        }
    }

    char *shader = malloc(strlen(globPixelShaderDefault) + 32 + strlen(psz_sampler) +
                          strlen(psz_src_transform) + strlen(psz_display_transform) +
                          strlen(psz_tone_mapping) + strlen(psz_adjust_range));
    if (!shader)
    {
        msg_Err(vd, "no room for the Pixel Shader");
        free(psz_range);
        return E_OUTOFMEMORY;
    }
    sprintf(shader, globPixelShaderDefault, sys->legacy_shader ? "" : "Array", psz_src_transform,
            psz_display_transform, psz_tone_mapping, psz_adjust_range, psz_sampler);
#ifndef NDEBUG
    if (!IsRGBShader(format)) {
        msg_Dbg(vd,"psz_src_transform %s", psz_src_transform);
        msg_Dbg(vd,"psz_tone_mapping %s", psz_tone_mapping);
        msg_Dbg(vd,"psz_display_transform %s", psz_display_transform);
        msg_Dbg(vd,"psz_adjust_range %s", psz_adjust_range);
        msg_Dbg(vd,"psz_sampler %s", psz_sampler);
    }
#endif
    free(psz_range);

    ID3DBlob *pPSBlob = CompileShader(vd, shader, true);
    free(shader);
    if (!pPSBlob)
        return E_INVALIDARG;

    HRESULT hr = ID3D11Device_CreatePixelShader(sys->d3d_dev.d3ddevice,
                                                (void *)ID3D10Blob_GetBufferPointer(pPSBlob),
                                                ID3D10Blob_GetBufferSize(pPSBlob), NULL, output);

    ID3D10Blob_Release(pPSBlob);
    return hr;
}

static bool CanUseTextureArray(vout_display_t *vd)
{
#ifndef HAVE_ID3D11VIDEODECODER
    (void) vd;
    return false;
#else
    struct wddm_version WDDM = {
        .wddm         = 22,
        .d3d_features = 19,
        .revision     = 162,
        .build        = 0,
    };
    if (D3D11CheckDriverVersion(&vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM) == VLC_SUCCESS)
        return true;

    msg_Dbg(vd, "fallback to legacy shader mode for old AMD drivers");
    return false;
#endif
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateFormatResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    sys->legacy_shader = !CanUseTextureArray(vd);

    hr = CompilePixelShader(vd, sys->picQuadConfig, fmt->transfer, fmt->b_color_range_full, &sys->picQuadPixelShader);
    if (FAILED(hr))
    {
#ifdef HAVE_ID3D11VIDEODECODER
        if (!sys->legacy_shader)
        {
            sys->legacy_shader = true;
            msg_Dbg(vd, "fallback to legacy shader mode");
            hr = CompilePixelShader(vd, sys->picQuadConfig, fmt->transfer, fmt->b_color_range_full, &sys->picQuadPixelShader);
        }
#endif
        if (FAILED(hr))
        {
            msg_Err(vd, "Failed to create the pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    sys->picQuad.i_width  = fmt->i_width;
    sys->picQuad.i_height = fmt->i_height;
    if (!sys->legacy_shader && is_d3d11_opaque(fmt->i_chroma))
    {
        sys->picQuad.i_width  = (sys->picQuad.i_width  + 0x7F) & ~0x7F;
        sys->picQuad.i_height = (sys->picQuad.i_height + 0x7F) & ~0x7F;
    }
    else
    if ( sys->picQuadConfig->formatTexture != DXGI_FORMAT_R8G8B8A8_UNORM &&
         sys->picQuadConfig->formatTexture != DXGI_FORMAT_B5G6R5_UNORM )
    {
        sys->picQuad.i_width  = (sys->picQuad.i_width  + 0x01) & ~0x01;
        sys->picQuad.i_height = (sys->picQuad.i_height + 0x01) & ~0x01;
    }

    BEFORE_UPDATE_RECTS;
    UpdateRects(vd, NULL, true);
    AFTER_UPDATE_RECTS;

#ifdef HAVE_ID3D11VIDEODECODER
    if (!is_d3d11_opaque(fmt->i_chroma) || sys->legacy_shader)
    {
        /* we need a staging texture */
        ID3D11Texture2D *textures[D3D11_MAX_SHADER_VIEW] = {0};
        video_format_t surface_fmt = *fmt;
        surface_fmt.i_width  = sys->picQuad.i_width;
        surface_fmt.i_height = sys->picQuad.i_height;

        if (AllocateTextures(VLC_OBJECT(vd), &sys->d3d_dev, sys->picQuadConfig, &surface_fmt, 1, textures))
        {
            msg_Err(vd, "Failed to allocate the staging texture");
            return VLC_EGENERIC;
        }

        sys->picQuad.resourceCount = DxgiResourceCount(sys->picQuadConfig);
        if (AllocateShaderView(VLC_OBJECT(vd), sys->d3d_dev.d3ddevice, sys->picQuadConfig,
                               textures, 0, sys->stagingSys.resourceView))
        {
            msg_Err(vd, "Failed to allocate the staging shader view");
            return VLC_EGENERIC;
        }

        for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
            sys->stagingSys.texture[plane] = textures[plane];
    }
#endif

    vd->info.is_slow = !is_d3d11_opaque(fmt->i_chroma) && sys->picQuadConfig->formatTexture != DXGI_FORMAT_UNKNOWN;
    return VLC_SUCCESS;
}

static int Direct3D11CreateGenericResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

#if defined(HAVE_ID3D11VIDEODECODER)
    sys->context_lock = CreateMutexEx( NULL, NULL, 0, SYNCHRONIZE );
    ID3D11Device_SetPrivateData( sys->d3d_dev.d3ddevice, &GUID_CONTEXT_MUTEX, sizeof( sys->context_lock ), &sys->context_lock );
#endif

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
    hr = ID3D11Device_CreateBlendState(sys->d3d_dev.d3ddevice, &spuBlendDesc, &pSpuBlendState);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create SPU blend state. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    ID3D11DeviceContext_OMSetBlendState(sys->d3d_dev.d3dcontext, pSpuBlendState, NULL, 0xFFFFFFFF);
    ID3D11BlendState_Release(pSpuBlendState);

    /* disable depth testing as we're only doing 2D
     * see https://msdn.microsoft.com/en-us/library/windows/desktop/bb205074%28v=vs.85%29.aspx
     * see http://rastertek.com/dx11tut11.html
    */
    D3D11_DEPTH_STENCIL_DESC stencilDesc;
    ZeroMemory(&stencilDesc, sizeof(stencilDesc));

    ID3D11DepthStencilState *pDepthStencilState;
    hr = ID3D11Device_CreateDepthStencilState(sys->d3d_dev.d3ddevice, &stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_OMSetDepthStencilState(sys->d3d_dev.d3dcontext, pDepthStencilState, 0);
        ID3D11DepthStencilState_Release(pDepthStencilState);
    }

    hr = UpdateBackBuffer(vd);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not update the backbuffer. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    if (sys->d3dregion_format != NULL)
    {
        hr = CompilePixelShader(vd, sys->d3dregion_format, TRANSFER_FUNC_SRGB, true, &sys->pSPUPixelShader);
        if (FAILED(hr))
        {
            ID3D11PixelShader_Release(sys->picQuadPixelShader);
            sys->picQuadPixelShader = NULL;
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    ID3DBlob *pVSBlob = CompileShader(vd, globVertexShaderFlat , false);
    if (!pVSBlob)
        return VLC_EGENERIC;

    hr = ID3D11Device_CreateVertexShader(sys->d3d_dev.d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->flatVSShader);

    if(FAILED(hr)) {
      ID3D10Blob_Release(pVSBlob);
      msg_Err(vd, "Failed to create the flat vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D11InputLayout* pVertexLayout = NULL;
    hr = ID3D11Device_CreateInputLayout(sys->d3d_dev.d3ddevice, layout, 2, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), &pVertexLayout);

    ID3D10Blob_Release(pVSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D11DeviceContext_IASetInputLayout(sys->d3d_dev.d3dcontext, pVertexLayout);
    ID3D11InputLayout_Release(pVertexLayout);

    pVSBlob = CompileShader(vd, globVertexShaderProjection, false);
    if (!pVSBlob)
        return VLC_EGENERIC;

    hr = ID3D11Device_CreateVertexShader(sys->d3d_dev.d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->projectionVSShader);

    if(FAILED(hr)) {
      ID3D10Blob_Release(pVSBlob);
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D10Blob_Release(pVSBlob);

    ID3D11DeviceContext_IASetPrimitiveTopology(sys->d3d_dev.d3dcontext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UpdatePicQuadPosition(vd);

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState *d3dsampState[2];
    hr = ID3D11Device_CreateSamplerState(sys->d3d_dev.d3ddevice, &sampDesc, &d3dsampState[0]);
    if (FAILED(hr)) {
      msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = ID3D11Device_CreateSamplerState(sys->d3d_dev.d3ddevice, &sampDesc, &d3dsampState[1]);
    if (FAILED(hr)) {
      msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
      ID3D11SamplerState_Release(d3dsampState[0]);
      return VLC_EGENERIC;
    }

    ID3D11DeviceContext_PSSetSamplers(sys->d3d_dev.d3dcontext, 0, 2, d3dsampState);
    ID3D11SamplerState_Release(d3dsampState[0]);
    ID3D11SamplerState_Release(d3dsampState[1]);

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->sys.pool)
        picture_pool_Release(sys->sys.pool);
    sys->sys.pool = NULL;
}

/**
 * Compute the vertex ordering needed to rotate the video. Without
 * rotation, the vertices of the rectangle are defined in a counterclockwise
 * order. This function computes a remapping of the coordinates to
 * implement the rotation, given fixed texture coordinates.
 * The unrotated order is the following:
 * 3--2
 * |  |
 * 0--1
 * For a 180 degrees rotation it should like this:
 * 1--0
 * |  |
 * 2--3
 * Vertex 0 should be assigned coordinates at index 2 from the
 * unrotated order and so on, thus yielding order: 2 3 0 1.
 */
static void orientationVertexOrder(video_orientation_t orientation, int vertex_order[static 4])
{
    switch (orientation) {
        case ORIENT_ROTATED_90:
            vertex_order[0] = 3;
            vertex_order[1] = 0;
            vertex_order[2] = 1;
            vertex_order[3] = 2;
            break;
        case ORIENT_ROTATED_270:
            vertex_order[0] = 1;
            vertex_order[1] = 2;
            vertex_order[2] = 3;
            vertex_order[3] = 0;
            break;
        case ORIENT_ROTATED_180:
            vertex_order[0] = 2;
            vertex_order[1] = 3;
            vertex_order[2] = 0;
            vertex_order[3] = 1;
            break;
        case ORIENT_TRANSPOSED:
            vertex_order[0] = 2;
            vertex_order[1] = 1;
            vertex_order[2] = 0;
            vertex_order[3] = 3;
            break;
        case ORIENT_HFLIPPED:
            vertex_order[0] = 1;
            vertex_order[1] = 0;
            vertex_order[2] = 3;
            vertex_order[3] = 2;
            break;
        case ORIENT_VFLIPPED:
            vertex_order[0] = 3;
            vertex_order[1] = 2;
            vertex_order[2] = 1;
            vertex_order[3] = 0;
            break;
        case ORIENT_ANTI_TRANSPOSED: /* transpose + vflip */
            vertex_order[0] = 0;
            vertex_order[1] = 3;
            vertex_order[2] = 2;
            vertex_order[3] = 1;
            break;
       default:
            vertex_order[0] = 0;
            vertex_order[1] = 1;
            vertex_order[2] = 2;
            vertex_order[3] = 3;
            break;
    }
}

static void SetupQuadFlat(d3d_vertex_t *dst_data, const RECT *output,
                          const d3d_quad_t *quad,
                          WORD *triangle_pos, video_orientation_t orientation)
{
    unsigned int src_width = quad->i_width;
    unsigned int src_height = quad->i_height;
    float MidX,MidY;

    float top, bottom, left, right;
    switch (orientation)
    {
    case ORIENT_ROTATED_90: /* 90° anti clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -(src_height - MidY) / (output->bottom - MidY);
        right  =   MidX / (MidX - output->left);
        break;
    case ORIENT_ROTATED_180: /* 180° */
        /* right/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case ORIENT_ROTATED_270: /* 90° clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_ANTI_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -(src_height - MidY) / (output->bottom - MidY);
        right  =  MidX / (MidX - output->left);
        break;
    case ORIENT_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_VFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case ORIENT_HFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left   = -(src_width  - MidX) / (output->right - MidX);
        right  =  MidX / (MidX - output->left);
        break;
    case ORIENT_NORMAL:
    default:
        /* left/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    }

    const float vertices_coords[4][2] = {
        { left,  bottom },
        { right, bottom },
        { right, top    },
        { left,  top    },
    };

    /* Compute index remapping necessary to implement the rotation. */
    int vertex_order[4];
    orientationVertexOrder(orientation, vertex_order);

    for (int i = 0; i < 4; ++i) {
        dst_data[i].position.x  = vertices_coords[vertex_order[i]][0];
        dst_data[i].position.y  = vertices_coords[vertex_order[i]][1];
    }

    // bottom left
    dst_data[0].position.z = 0.0f;
    dst_data[0].texture.x = 0.0f;
    dst_data[0].texture.y = 1.0f;

    // bottom right
    dst_data[1].position.z = 0.0f;
    dst_data[1].texture.x = 1.0f;
    dst_data[1].texture.y = 1.0f;

    // top right
    dst_data[2].position.z = 0.0f;
    dst_data[2].texture.x = 1.0f;
    dst_data[2].texture.y = 0.0f;

    // top left
    dst_data[3].position.z = 0.0f;
    dst_data[3].texture.x = 0.0f;
    dst_data[3].texture.y = 0.0f;

    /* Make sure surfaces are facing the right way */
    if( orientation == ORIENT_TOP_RIGHT || orientation == ORIENT_BOTTOM_LEFT
     || orientation == ORIENT_LEFT_TOP  || orientation == ORIENT_RIGHT_BOTTOM )
    {
        triangle_pos[0] = 0;
        triangle_pos[1] = 1;
        triangle_pos[2] = 3;

        triangle_pos[3] = 3;
        triangle_pos[4] = 1;
        triangle_pos[5] = 2;
    }
    else
    {
        triangle_pos[0] = 3;
        triangle_pos[1] = 1;
        triangle_pos[2] = 0;

        triangle_pos[3] = 2;
        triangle_pos[4] = 1;
        triangle_pos[5] = 3;
    }
}

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

static void SetupQuadSphere(d3d_vertex_t *dst_data, const RECT *output,
                            const d3d_quad_t *quad, WORD *triangle_pos)
{
    const float scaleX = (float)(output->right  - output->left) / quad->i_width;
    const float scaleY = (float)(output->bottom - output->top)   / quad->i_height;
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

            dst_data[off1].texture.x = scaleX * lon / (float) nbLonBands; // 0(left) to 1(right)
            dst_data[off1].texture.y = scaleY * lat / (float) nbLatBands; // 0(top) to 1 (bottom)
        }
    }

    for (unsigned lat = 0; lat < nbLatBands; lat++) {
        for (unsigned lon = 0; lon < nbLonBands; lon++) {
            unsigned first = (lat * (nbLonBands + 1)) + lon;
            unsigned second = first + nbLonBands + 1;

            unsigned off = (lat * nbLatBands + lon) * 3 * 2;

            triangle_pos[off] = first;
            triangle_pos[off + 1] = first + 1;
            triangle_pos[off + 2] = second;

            triangle_pos[off + 3] = second;
            triangle_pos[off + 4] = first + 1;
            triangle_pos[off + 5] = second + 1;
        }
    }
}

static bool AllocQuadVertices(vout_display_t *vd, d3d_quad_t *quad,
                              video_projection_mode_t projection)
{
    HRESULT hr;
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
    {
        msg_Warn(vd, "Projection mode %d not handled", projection);
        return false;
    }

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(d3d_vertex_t) * quad->vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateBuffer(sys->d3d_dev.d3ddevice, &bd, NULL, &quad->pVertexBuffer);
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

    hr = ID3D11Device_CreateBuffer(sys->d3d_dev.d3ddevice, &quadDesc, NULL, &quad->pIndexBuffer);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the quad indices. (hr=0x%lX)", hr);
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
        return false;
    }

    return true;
}

static bool UpdateQuadPosition( vout_display_t *vd, d3d_quad_t *quad,
                                const RECT *output,
                                video_projection_mode_t projection,
                                video_orientation_t orientation )
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    if (unlikely(quad->pVertexBuffer == NULL))
        return false;

    /* create the vertices */
    hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to lock the vertex buffer (hr=0x%lX)", hr);
        return false;
    }
    d3d_vertex_t *dst_data = mappedResource.pData;

    /* create the vertex indices */
    hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to lock the index buffer (hr=0x%lX)", hr);
        ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);
        return false;
    }
    WORD *triangle_pos = mappedResource.pData;

    if ( projection == PROJECTION_MODE_RECTANGULAR )
        SetupQuadFlat(dst_data, output, quad, triangle_pos, orientation);
    else
        SetupQuadSphere(dst_data, output, quad, triangle_pos);

    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0);
    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);

    return true;
}

static int SetupQuad(vout_display_t *vd, const video_format_t *fmt, d3d_quad_t *quad,
                     const RECT *output,
                     const d3d_format_t *cfg, ID3D11PixelShader *d3dpixelShader,
                     video_projection_mode_t projection, video_orientation_t orientation)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    const bool RGB_shader = IsRGBShader(cfg);

    quad->shaderConstants.LuminanceScale = GetFormatLuminance(VLC_OBJECT(vd), fmt) / (float)sys->display.luminance_peak;

    /* pixel shader constant buffer */
    quad->shaderConstants.Opacity = 1.0;
    if (fmt->i_visible_width == fmt->i_width)
        quad->shaderConstants.BoundaryX = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants.BoundaryX = (FLOAT) (fmt->i_visible_width - 1) / fmt->i_width;
    if (fmt->i_visible_height == fmt->i_height)
        quad->shaderConstants.BoundaryY = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants.BoundaryY = (FLOAT) (fmt->i_visible_height - 1) / fmt->i_height;

    static_assert((sizeof(PS_CONSTANT_BUFFER)%16)==0,"Constant buffers require 16-byte alignment");
    D3D11_BUFFER_DESC constantDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(PS_CONSTANT_BUFFER),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    D3D11_SUBRESOURCE_DATA constantInit = { .pSysMem = &quad->shaderConstants };
    hr = ID3D11Device_CreateBuffer(sys->d3d_dev.d3ddevice, &constantDesc, &constantInit, &quad->pPixelShaderConstants[0]);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    FLOAT itu_black_level = 0.f;
    FLOAT itu_achromacy   = 0.f;
    if (!RGB_shader)
    {
        switch (cfg->bitsPerChannel)
        {
        case 8:
            /* Rec. ITU-R BT.709-6 §4.6 */
            itu_black_level  =              16.f / 255.f;
            itu_achromacy    =             128.f / 255.f;
            break;
        case 10:
            /* Rec. ITU-R BT.709-6 §4.6 */
            itu_black_level  =              64.f / 1023.f;
            itu_achromacy    =             512.f / 1023.f;
            break;
        case 12:
            /* Rec. ITU-R BT.2020-2 Table 5 */
            itu_black_level  =               256.f / 4095.f;
            itu_achromacy    =              2048.f / 4095.f;
            break;
        default:
            /* unknown bitdepth, use approximation for infinite bit depth */
            itu_black_level  =              16.f / 256.f;
            itu_achromacy    =             128.f / 256.f;
            break;
        }
    }

    static const FLOAT IDENTITY_4X4[4 * 4] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };

    /* matrices for studio range */
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion, in studio range */
    static const FLOAT COLORSPACE_BT601_TO_FULL[4*4] = {
        1.164383561643836f,                 0.f,  1.596026785714286f, 0.f,
        1.164383561643836f, -0.391762290094914f, -0.812967647237771f, 0.f,
        1.164383561643836f,  2.017232142857142f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion, in studio range */
    static const FLOAT COLORSPACE_BT709_TO_FULL[4*4] = {
        1.164383561643836f,                 0.f,  1.792741071428571f, 0.f,
        1.164383561643836f, -0.213248614273730f, -0.532909328559444f, 0.f,
        1.164383561643836f,  2.112401785714286f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion, in studio range */
    static const FLOAT COLORSPACE_BT2020_TO_FULL[4*4] = {
        1.164383561643836f,  0.000000000000f,  1.678674107143f, 0.f,
        1.164383561643836f, -0.127007098661f, -0.440987687946f, 0.f,
        1.164383561643836f,  2.141772321429f,  0.000000000000f, 0.f,
                       0.f,              0.f,              0.f, 1.f,
    };

    PS_COLOR_TRANSFORM colorspace;

    memcpy(colorspace.WhitePoint, IDENTITY_4X4, sizeof(colorspace.WhitePoint));

    const FLOAT *ppColorspace;
    if (RGB_shader)
        ppColorspace = IDENTITY_4X4;
    else {
        switch (fmt->space){
            case COLOR_SPACE_BT709:
                ppColorspace = COLORSPACE_BT709_TO_FULL;
                break;
            case COLOR_SPACE_BT2020:
                ppColorspace = COLORSPACE_BT2020_TO_FULL;
                break;
            case COLOR_SPACE_BT601:
                ppColorspace = COLORSPACE_BT601_TO_FULL;
                break;
            default:
            case COLOR_SPACE_UNDEF:
                if( fmt->i_height > 576 )
                    ppColorspace = COLORSPACE_BT709_TO_FULL;
                else
                    ppColorspace = COLORSPACE_BT601_TO_FULL;
                break;
        }
        /* all matrices work in studio range and output in full range */
        colorspace.WhitePoint[0*4 + 3] = -itu_black_level;
        colorspace.WhitePoint[1*4 + 3] = -itu_achromacy;
        colorspace.WhitePoint[2*4 + 3] = -itu_achromacy;
    }

    memcpy(colorspace.Colorspace, ppColorspace, sizeof(colorspace.Colorspace));

    constantInit.pSysMem = &colorspace;

    static_assert((sizeof(PS_COLOR_TRANSFORM)%16)==0,"Constant buffers require 16-byte alignment");
    constantDesc.ByteWidth = sizeof(PS_COLOR_TRANSFORM);
    hr = ID3D11Device_CreateBuffer(sys->d3d_dev.d3ddevice, &constantDesc, &constantInit, &quad->pPixelShaderConstants[1]);
    if(FAILED(hr)) {
        msg_Err(vd, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }
    quad->PSConstantsCount = 2;

    /* vertex shader constant buffer */
    if ( projection == PROJECTION_MODE_EQUIRECTANGULAR )
    {
        constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
        static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
        hr = ID3D11Device_CreateBuffer(sys->d3d_dev.d3ddevice, &constantDesc, NULL, &quad->pVertexShaderConstants);
        if(FAILED(hr)) {
            msg_Err(vd, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
            goto error;
        }

        SetQuadVSProjection( vd, quad, &vd->cfg->viewpoint );
    }

    quad->picSys.formatTexture = cfg->formatTexture;
    quad->picSys.context = sys->d3d_dev.d3dcontext;
    ID3D11DeviceContext_AddRef(quad->picSys.context);

    if (!AllocQuadVertices(vd, quad, projection))
        goto error;
    if (!UpdateQuadPosition(vd, quad, output, projection, orientation))
        goto error;

    quad->d3dpixelShader = d3dpixelShader;
    if (projection == PROJECTION_MODE_RECTANGULAR)
        quad->d3dvertexShader = sys->flatVSShader;
    else
        quad->d3dvertexShader = sys->projectionVSShader;

    return VLC_SUCCESS;

error:
    ReleaseQuad(quad);
    return VLC_EGENERIC;
}

static void ReleaseQuad(d3d_quad_t *quad)
{
    if (quad->pPixelShaderConstants[0])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[0]);
        quad->pPixelShaderConstants[0] = NULL;
    }
    if (quad->pPixelShaderConstants[1])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[1]);
        quad->pPixelShaderConstants[1] = NULL;
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
    ReleasePictureSys(&quad->picSys);
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyPool(vd);

    ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;

    ReleasePictureSys(&sys->stagingSys);

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
        ID3D11PixelShader_Release(sys->pSPUPixelShader);
        sys->pSPUPixelShader = NULL;
    }
    if (sys->picQuadPixelShader)
    {
        ID3D11PixelShader_Release(sys->picQuadPixelShader);
        sys->picQuadPixelShader = NULL;
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

static void UpdateQuadOpacity(vout_display_t *vd, d3d_quad_t *quad, float opacity)
{
    vout_display_sys_t *sys = vd->sys;
    if (quad->shaderConstants.Opacity == opacity)
        return;

    float old = quad->shaderConstants.Opacity;
    quad->shaderConstants.Opacity = opacity;
    if (!D3D11_ShaderUpdateConstants(VLC_OBJECT(vd), &sys->d3d_dev, quad))
        quad->shaderConstants.Opacity = old;
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    if (sys->d3dregion_format == NULL)
        return VLC_EGENERIC;

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
            if (cache != NULL && ((d3d_quad_t *) cache->p_sys)->picSys.texture[KNOWN_DXGI_INDEX]) {
                ID3D11Texture2D_GetDesc( ((d3d_quad_t *) cache->p_sys)->picSys.texture[KNOWN_DXGI_INDEX], &texDesc );
                if (texDesc.Format == sys->d3dregion_format->formatTexture &&
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
            if (AllocateTextures(VLC_OBJECT(vd), &sys->d3d_dev, sys->d3dregion_format, &r->fmt, 1, d3dquad->picSys.texture)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                for (int j=0; j<D3D11_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        ID3D11Texture2D_Release(d3dquad->picSys.texture[j]);
                free(d3dquad);
                continue;
            }

            d3dquad->resourceCount = DxgiResourceCount(sys->d3dregion_format);
            if (AllocateShaderView(VLC_OBJECT(vd), sys->d3d_dev.d3ddevice, sys->d3dregion_format,
                                   d3dquad->picSys.texture, 0,
                                   d3dquad->picSys.resourceView)) {
                msg_Err(vd, "Failed to create %dx%d shader view for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            d3dquad->i_width    = r->fmt.i_width;
            d3dquad->i_height   = r->fmt.i_height;
            RECT output;
            output.left   = r->fmt.i_x_offset;
            output.right  = r->fmt.i_x_offset + r->fmt.i_width;
            output.top    = r->fmt.i_y_offset;
            output.bottom = r->fmt.i_y_offset + r->fmt.i_height;

            err = SetupQuad( vd, &r->fmt, d3dquad, &output,
                             sys->d3dregion_format, sys->pSPUPixelShader,
                             PROJECTION_MODE_RECTANGULAR, ORIENT_NORMAL );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to create %dx%d quad for OSD",
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

        hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            err = CommonUpdatePicture(quad_picture, NULL, mappedResource.pData, mappedResource.RowPitch);
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to set the buffer on the SPU picture" );
                picture_Release(quad_picture);
                continue;
            }

            picture_CopyPixels(quad_picture, r->p_picture);

            ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0);
        } else {
            msg_Err(vd, "Failed to map the SPU texture (hr=0x%lX)", hr );
            picture_Release(quad_picture);
            continue;
        }

        d3d_quad_t *quad = (d3d_quad_t *) quad_picture->p_sys;

        quad->cropViewport.Width =  (FLOAT) r->fmt.i_visible_width  * RECTWidth(sys->sys.rect_dest)  / subpicture->i_original_picture_width;
        quad->cropViewport.Height = (FLOAT) r->fmt.i_visible_height * RECTHeight(sys->sys.rect_dest) / subpicture->i_original_picture_height;
        quad->cropViewport.MinDepth = 0.0f;
        quad->cropViewport.MaxDepth = 1.0f;
        quad->cropViewport.TopLeftX = sys->sys.rect_dest.left + (FLOAT) r->i_x * RECTWidth(sys->sys.rect_dest) / subpicture->i_original_picture_width;
        quad->cropViewport.TopLeftY = sys->sys.rect_dest.top  + (FLOAT) r->i_y * RECTHeight(sys->sys.rect_dest) / subpicture->i_original_picture_height;

        UpdateQuadOpacity(vd, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}

