/*****************************************************************************
 * direct3d9.c: Windows Direct3D9 video output module
 *****************************************************************************
 * Copyright (C) 2006-2014 VLC authors and VideoLAN
 *$Id$
 *
 * Authors: Martell Malone <martellmalone@gmail.com>,
 *          Damien Fouilleul <damienf@videolan.org>,
 *          Sasha Koruga <skoruga@gmail.com>,
 *          Felix Abecassis <felix.abecassis@gmail.com>
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

/*****************************************************************************
 * Preamble:
 *
 * This plugin will use YUV surface if supported, using YUV will result in
 * the best video quality (hardware filtering when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures.
 *
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// For dynamic use of DXVA-HD
#if _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_charset.h> /* ToT function */

#include <windows.h>
#include <d3d9.h>
#ifdef HAVE_D3DX9EFFECT_H
#include <d3dx9effect.h>
#endif
#include "../../video_chroma/d3d9_fmt.h"
#include <dxvahd.h>

#include "common.h"
#include "builtin_shaders.h"

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

static int  GLConvOpen(vlc_object_t *);
static void GLConvClose(vlc_object_t *);

#define DESKTOP_LONGTEXT N_(\
    "The desktop mode allows you to display the video on the desktop.")

#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitle/OSD blending.")

#define PIXEL_SHADER_TEXT N_("Pixel Shader")
#define PIXEL_SHADER_LONGTEXT N_(\
        "Choose a pixel shader to apply.")
#define PIXEL_SHADER_FILE_TEXT N_("Path to HLSL file")
#define PIXEL_SHADER_FILE_LONGTEXT N_("Path to an HLSL file containing a single pixel shader.")
/* The latest option in the selection list: used for loading a shader file. */
#define SELECTED_SHADER_FILE N_("HLSL File")

#define D3D9_HELP N_("Recommended video output for Windows Vista and later versions")

static int FindShadersCallback(vlc_object_t *, const char *,
                               char ***, char ***);

vlc_module_begin ()
    set_shortname("Direct3D9")
    set_description(N_("Direct3D9 video output"))
    set_help(D3D9_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d9-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)

    add_string("direct3d9-shader", "", PIXEL_SHADER_TEXT, PIXEL_SHADER_LONGTEXT, true)
        change_string_cb(FindShadersCallback)
    add_loadfile("direct3d9-shader-file", NULL, PIXEL_SHADER_FILE_TEXT, PIXEL_SHADER_FILE_LONGTEXT, false)

    set_capability("vout display", 280)
    add_shortcut("direct3d9", "direct3d")
    set_callbacks(Open, Close)

#ifdef HAVE_GL
    add_submodule()
    set_description("DX OpenGL surface converter for D3D9")
    set_capability("glconv", 1)
    set_callbacks(GLConvOpen, GLConvClose)
#endif
vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static const vlc_fourcc_t d3d_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

typedef struct
{
    const char   *name;
    D3DFORMAT    format;    /* D3D format */
    vlc_fourcc_t fourcc;    /* VLC fourcc */
    uint32_t     rmask;
    uint32_t     gmask;
    uint32_t     bmask;
} d3d9_format_t;

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;

    bool allow_hw_yuv;    /* Should we use hardware YUV->RGB conversions */
    struct {
        bool is_fullscreen;
        bool is_on_top;
        RECT win;
    } desktop_save;
    vout_display_cfg_t cfg_saved; /* configuration used before going into desktop mode */

    // core objects
    d3d9_handle_t           hd3d;
    HINSTANCE               hxdll;      /* handle of the opened d3d9x dll */
    IDirect3DPixelShader9*  d3dx_shader;
    d3d9_device_t           d3d_dev;

    UINT                    texture_width;
    UINT                    texture_height;

    // scene objects
    LPDIRECT3DTEXTURE9      d3dtex;
    LPDIRECT3DVERTEXBUFFER9 d3dvtc;
    D3DFORMAT               d3dregion_format;    /* Backbuffer output format */
    int                     d3dregion_count;
    struct d3d_region_t     *d3dregion;
    const d3d9_format_t      *d3dtexture_format;  /* Rendering texture(s) format */

    /* */
    bool                    reset_device;
    bool                    reopen_device;
    bool                    lost_not_ready;
    bool                    clear_scene;

    /* It protects the following variables */
    vlc_mutex_t    lock;
    bool           ch_desktop;
    bool           desktop_requested;

    /* range converter */
    struct {
        HMODULE                 dll;
        IDXVAHD_VideoProcessor *proc;
    } processor;
};

static const d3d9_format_t *Direct3DFindFormat(vout_display_t *vd, vlc_fourcc_t chroma, D3DFORMAT target);
static const d3d9_format_t *FindBufferFormat(vout_display_t *, D3DFORMAT);

static int  Open(vlc_object_t *);

static picture_pool_t *Direct3D9CreatePicturePool  (vlc_object_t *, d3d9_device_t *,
     const d3d9_format_t *, const video_format_t *, unsigned);

static void           Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void           Display(vout_display_t *, picture_t *, subpicture_t *subpicture);
static picture_pool_t*DisplayPool(vout_display_t *, unsigned);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

static int  Direct3D9Reset  (vout_display_t *);
static void Direct3D9Destroy(vout_display_sys_t *);

static int  Direct3D9Open (vout_display_t *, video_format_t *);
static void Direct3D9Close(vout_display_t *);

/* */
typedef struct
{
    FLOAT       x,y,z;      // vertex untransformed position
    FLOAT       rhw;        // eye distance
    D3DCOLOR    diffuse;    // diffuse color
    FLOAT       tu, tv;     // texture relative coordinates
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

typedef struct d3d_region_t {
    D3DFORMAT          format;
    unsigned           width;
    unsigned           height;
    CUSTOMVERTEX       vertex[4];
    LPDIRECT3DTEXTURE9 texture;
} d3d_region_t;

static void Direct3D9DeleteRegions(int, d3d_region_t *);

static int  Direct3D9ImportPicture(vout_display_t *vd, d3d_region_t *, LPDIRECT3DSURFACE9 surface);
static void Direct3D9ImportSubpicture(vout_display_t *vd, int *, d3d_region_t **, subpicture_t *);

static void Direct3D9RenderScene(vout_display_t *vd, d3d_region_t *, int, d3d_region_t *);

/* */
static int DesktopCallback(vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void *);

static bool is_d3d9_opaque(vlc_fourcc_t chroma)
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

static HINSTANCE Direct3D9LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    for (int i = 43; i > 23; --i) {
        TCHAR filename[16];
        _sntprintf(filename, 16, TEXT("D3dx9_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance)
            break;
    }
    return instance;
}

static unsigned int GetPictureWidth(const vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    return sys->texture_width;
}

static unsigned int GetPictureHeight(const vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    return sys->texture_height;
}

/**
 * It creates a Direct3D vout display.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    if ( !vd->obj.force && vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR)
        return VLC_EGENERIC; /* let a module who can handle it do it */

    if ( !vd->obj.force && vd->source.mastering.max_luminance != 0)
        return VLC_EGENERIC; /* let a module who can handle it do it */

#if !VLC_WINSTORE_APP
    /* do not use D3D9 on XP unless forced */
    if (!vd->obj.force)
    {
        bool isVistaOrGreater = false;
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (likely(hKernel32 != NULL))
            isVistaOrGreater = GetProcAddress(hKernel32, "EnumResourceLanguagesExW") != NULL;
        if (!isVistaOrGreater)
            return VLC_EGENERIC;
    }
#endif

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    if (D3D9_Create(vd, &sys->hd3d)) {
        msg_Err(vd, "Direct3D9 could not be initialized");
        free(sys);
        return VLC_EGENERIC;
    }

    sys->hxdll = Direct3D9LoadShaderLibrary();
    if (!sys->hxdll)
        msg_Warn(object, "cannot load Direct3D9 Shader Library; HLSL pixel shading will be disabled.");

    sys->sys.use_desktop = var_CreateGetBool(vd, "video-wallpaper");
    sys->reset_device = false;
    sys->reopen_device = false;
    sys->lost_not_ready = false;
    sys->allow_hw_yuv = var_CreateGetBool(vd, "directx-hw-yuv");
    sys->desktop_save.is_fullscreen = vd->cfg->is_fullscreen;
    sys->desktop_save.is_on_top     = false;
    sys->desktop_save.win.left      = var_InheritInteger(vd, "video-x");
    sys->desktop_save.win.right     = vd->cfg->display.width;
    sys->desktop_save.win.top       = var_InheritInteger(vd, "video-y");
    sys->desktop_save.win.bottom    = vd->cfg->display.height;

    if (CommonInit(vd))
        goto error;

    sys->sys.pf_GetPictureWidth  = GetPictureWidth;
    sys->sys.pf_GetPictureHeight = GetPictureHeight;

    /* */
    video_format_t fmt;
    if (Direct3D9Open(vd, &fmt)) {
        msg_Err(vd, "Direct3D9 could not be opened");
        goto error;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.is_slow = !is_d3d9_opaque(fmt.i_chroma);
    info.has_double_click = true;
    info.has_pictures_invalid = !is_d3d9_opaque(fmt.i_chroma);
    if (var_InheritBool(vd, "direct3d9-hw-blending") &&
        sys->d3dregion_format != D3DFMT_UNKNOWN &&
        (sys->d3d_dev.caps.SrcBlendCaps  & D3DPBLENDCAPS_SRCALPHA) &&
        (sys->d3d_dev.caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA) &&
        (sys->d3d_dev.caps.TextureCaps   & D3DPTEXTURECAPS_ALPHA) &&
        (sys->d3d_dev.caps.TextureOpCaps & D3DTEXOPCAPS_SELECTARG1) &&
        (sys->d3d_dev.caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE))
        info.subpicture_chromas = d3d_subpicture_chromas;
    else
        info.subpicture_chromas = NULL;

    /* Interaction */
    vlc_mutex_init(&sys->lock);
    sys->ch_desktop = false;
    sys->desktop_requested = sys->sys.use_desktop;

    vlc_value_t val;
    val.psz_string = _("Desktop");
    var_Change(vd, "video-wallpaper", VLC_VAR_SETTEXT, &val, NULL);
    var_AddCallback(vd, "video-wallpaper", DesktopCallback, NULL);

    /* Setup vout_display now that everything is fine */
    video_format_Clean(&vd->fmt);
    video_format_Copy(&vd->fmt, &fmt);
    vd->info = info;

    vd->pool = DisplayPool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* Fix state in case of desktop mode */
    if (sys->sys.use_desktop && vd->cfg->is_fullscreen)
        vout_display_SendEventFullscreen(vd, false, false);

    return VLC_SUCCESS;
error:
    Direct3D9Close(vd);
    CommonClean(vd);
    Direct3D9Destroy(sys);
    free(vd->sys);
    return VLC_EGENERIC;
}

/**
 * It destroyes a Direct3D vout display.
 */
static void Close(vlc_object_t *object)
{
    vout_display_t * vd = (vout_display_t *)object;

    var_DelCallback(vd, "video-wallpaper", DesktopCallback, NULL);
    vlc_mutex_destroy(&vd->sys->lock);

    Direct3D9Close(vd);

    CommonClean(vd);

    Direct3D9Destroy(vd->sys);

    free(vd->sys);
}

static void DestroyPicture(picture_t *picture)
{
    ReleasePictureSys(picture->p_sys);

    free(picture->p_sys);
    free(picture);
}

/**
 * It locks the surface associated to the picture and get the surface
 * descriptor which amongst other things has the pointer to the picture
 * data and its pitch.
 */
static int Direct3D9LockSurface(picture_t *picture)
{
    /* Lock the surface to get a valid pointer to the picture buffer */
    D3DLOCKED_RECT d3drect;
    HRESULT hr = IDirect3DSurface9_LockRect(picture->p_sys->surface, &d3drect, NULL, 0);
    if (FAILED(hr)) {
        return VLC_EGENERIC;
    }

    CommonUpdatePicture(picture, NULL, d3drect.pBits, d3drect.Pitch);
    return VLC_SUCCESS;
}
/**
 * It unlocks the surface associated to the picture.
 */
static void Direct3D9UnlockSurface(picture_t *picture)
{
    /* Unlock the Surface */
    HRESULT hr = IDirect3DSurface9_UnlockRect(picture->p_sys->surface);
    if (FAILED(hr)) {
        //msg_Dbg(vd, "Failed IDirect3DSurface9_UnlockRect: 0x%0lx", hr);
    }
}

/* */
static picture_pool_t *Direct3D9CreatePicturePool(vlc_object_t *o,
    d3d9_device_t *p_d3d9_dev, const d3d9_format_t *default_d3dfmt, const video_format_t *fmt, unsigned count)
{
    picture_pool_t*   pool = NULL;
    picture_t**       pictures = NULL;
    unsigned          picture_count = 0;

    pictures = calloc(count, sizeof(*pictures));
    if (!pictures)
        goto error;

    D3DFORMAT format;
    switch (fmt->i_chroma)
    {
    case VLC_CODEC_D3D9_OPAQUE_10B:
        format = MAKEFOURCC('P','0','1','0');
        break;
    case VLC_CODEC_D3D9_OPAQUE:
        format = MAKEFOURCC('N','V','1','2');
        break;
    default:
        if (!default_d3dfmt)
            goto error;
        format = default_d3dfmt->format;
        break;
    }

    for (picture_count = 0; picture_count < count; ++picture_count)
    {
        picture_sys_t *picsys = malloc(sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;
        memset(picsys, 0, sizeof(*picsys));

        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(p_d3d9_dev->dev,
                                                          fmt->i_width,
                                                          fmt->i_height,
                                                          format,
                                                          D3DPOOL_DEFAULT,
                                                          &picsys->surface,
                                                          NULL);
        if (FAILED(hr)) {
           msg_Err(o, "Failed to allocate surface %d (hr=0x%0lx)", picture_count, hr);
           free(picsys);
           goto error;
        }

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyPicture,
        };

        picture_t *picture = picture_NewFromResource(fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto error;
        }

        pictures[picture_count] = picture;
    }

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = count;
    pool_cfg.picture       = pictures;
    if( !is_d3d9_opaque( fmt->i_chroma ) )
    {
        pool_cfg.lock = Direct3D9LockSurface;
        pool_cfg.unlock = Direct3D9UnlockSurface;
    }

    pool = picture_pool_NewExtended( &pool_cfg );

error:
    if (pool == NULL && pictures) {
        for (unsigned i=0;i<picture_count; ++i)
            DestroyPicture(pictures[i]);
    }
    free(pictures);
    return pool;
}

static picture_pool_t *DisplayPool(vout_display_t *vd, unsigned count)
{
    if ( vd->sys->sys.pool != NULL )
        return vd->sys->sys.pool;
    vd->sys->sys.pool = Direct3D9CreatePicturePool(VLC_OBJECT(vd), &vd->sys->d3d_dev,
        vd->sys->d3dtexture_format, &vd->fmt, count);
    return vd->sys->sys.pool;
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DSURFACE9 surface = picture->p_sys->surface;
    d3d9_device_t *p_d3d9_dev = &sys->d3d_dev;

    /* FIXME it is a bit ugly, we need the surface to be unlocked for
     * rendering.
     *  The clean way would be to release the picture (and ensure that
     * the vout doesn't keep a reference). But because of the vout
     * wrapper, we can't */
    if ( !is_d3d9_opaque(picture->format.i_chroma) )
        Direct3D9UnlockSurface(picture);
    else if (picture->context)
    {
        const struct va_pic_context *pic_ctx = (struct va_pic_context*)picture->context;
        if (pic_ctx->picsys.surface != surface)
        {
            D3DSURFACE_DESC srcDesc, dstDesc;
            IDirect3DSurface9_GetDesc(pic_ctx->picsys.surface, &srcDesc);
            IDirect3DSurface9_GetDesc(surface, &dstDesc);
            if ( srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height )
                surface = pic_ctx->picsys.surface;
            else
            {
                HRESULT hr;
                RECT visibleSource;
                visibleSource.left = 0;
                visibleSource.top = 0;
                visibleSource.right = picture->format.i_visible_width;
                visibleSource.bottom = picture->format.i_visible_height;

                hr = IDirect3DDevice9_StretchRect( p_d3d9_dev->dev, pic_ctx->picsys.surface, &visibleSource, surface, &visibleSource, D3DTEXF_NONE);
                if (FAILED(hr)) {
                    msg_Err(vd, "Failed to copy the hw surface to the decoder surface (hr=0x%0lx)", hr );
                }
            }
        }
    }

    /* check if device is still available */
    HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(p_d3d9_dev->dev);
    if (FAILED(hr)) {
        if (hr == D3DERR_DEVICENOTRESET && !sys->reset_device) {
            vout_display_SendEventPicturesInvalid(vd);
            sys->reset_device = true;
            sys->lost_not_ready = false;
        }
        if (hr == D3DERR_DEVICELOST && !sys->lost_not_ready) {
            /* Device is lost but not yet ready for reset. */
            sys->lost_not_ready = true;
        }
        return;
    }

    d3d_region_t picture_region;
    if (!Direct3D9ImportPicture(vd, &picture_region, surface)) {
        picture_region.width = picture->format.i_visible_width;
        picture_region.height = picture->format.i_visible_height;
        int subpicture_region_count     = 0;
        d3d_region_t *subpicture_region = NULL;
        if (subpicture)
            Direct3D9ImportSubpicture(vd, &subpicture_region_count, &subpicture_region,
                                     subpicture);

        Direct3D9RenderScene(vd, &picture_region,
                            subpicture_region_count, subpicture_region);

        Direct3D9DeleteRegions(sys->d3dregion_count, sys->d3dregion);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregion       = subpicture_region;
    }
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    const d3d9_device_t *p_d3d9_dev = &sys->d3d_dev;

    if (sys->lost_not_ready) {
        picture_Release(picture);
        if (subpicture)
            subpicture_Delete(subpicture);
        return;
    }

    // Present the back buffer contents to the display
    // No stretching should happen here !
    const RECT src = sys->sys.rect_dest_clipped;
    const RECT dst = sys->sys.rect_dest_clipped;

    HRESULT hr;
    if (sys->hd3d.use_ex) {
        hr = IDirect3DDevice9Ex_PresentEx(p_d3d9_dev->devex, &src, &dst, NULL, NULL, 0);
    } else {
        hr = IDirect3DDevice9_Present(p_d3d9_dev->dev, &src, &dst, NULL, NULL);
    }
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_Present: 0x%0lx", hr);
    }

    /* XXX See Prepare() */
    if ( !is_d3d9_opaque(picture->format.i_chroma) )
        Direct3D9LockSurface(picture);
    picture_Release(picture);
    if (subpicture)
        subpicture_Delete(subpicture);

    CommonDisplay(vd);
}

static int ControlReopenDevice(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->sys.use_desktop) {
        /* Save non-desktop state */
        sys->desktop_save.is_fullscreen = vd->cfg->is_fullscreen;
        sys->desktop_save.is_on_top     = sys->sys.is_on_top;

        WINDOWPLACEMENT wp = { .length = sizeof(wp), };
        GetWindowPlacement(sys->sys.hparent ? sys->sys.hparent : sys->sys.hwnd, &wp);
        sys->desktop_save.win = wp.rcNormalPosition;
    }

    /* */
    Direct3D9Close(vd);
    EventThreadStop(sys->sys.event);

    /* */
    vlc_mutex_lock(&sys->lock);
    sys->sys.use_desktop = sys->desktop_requested;
    sys->ch_desktop = false;
    vlc_mutex_unlock(&sys->lock);

    /* */
    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_desktop = sys->sys.use_desktop;
    if (!sys->sys.use_desktop) {
        cfg.x      = sys->desktop_save.win.left;
        cfg.y      = sys->desktop_save.win.top;
        cfg.width  = sys->desktop_save.win.right  - sys->desktop_save.win.left;
        cfg.height = sys->desktop_save.win.bottom - sys->desktop_save.win.top;
    }

    event_hwnd_t hwnd;
    if (EventThreadStart(sys->sys.event, &hwnd, &cfg)) {
        msg_Err(vd, "Failed to restart event thread");
        return VLC_EGENERIC;
    }
    sys->sys.parent_window = hwnd.parent_window;
    sys->sys.hparent       = hwnd.hparent;
    sys->sys.hwnd          = hwnd.hwnd;
    sys->sys.hvideownd     = hwnd.hvideownd;
    sys->sys.hfswnd        = hwnd.hfswnd;
    SetRectEmpty(&sys->sys.rect_parent);

    /* */
    video_format_t fmt;
    if (Direct3D9Open(vd, &fmt)) {
        CommonClean(vd);
        msg_Err(vd, "Failed to reopen device");
        return VLC_EGENERIC;
    }
    vd->fmt = fmt;
    sys->sys.is_first_display = true;

    if (sys->sys.use_desktop) {
        /* Disable fullscreen/on_top while using desktop */
        if (sys->desktop_save.is_fullscreen)
            vout_display_SendEventFullscreen(vd, false, false);
        if (sys->desktop_save.is_on_top)
            vout_display_SendWindowState(vd, VOUT_WINDOW_STATE_NORMAL);
    } else {
        /* Restore fullscreen/on_top */
        if (sys->desktop_save.is_fullscreen)
            vout_display_SendEventFullscreen(vd, true, false);
        if (sys->desktop_save.is_on_top)
            vout_display_SendWindowState(vd, VOUT_WINDOW_STATE_ABOVE);
    }
    return VLC_SUCCESS;
}
static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
        /* FIXME what to do here in case of failure */
        if (sys->reset_device) {
            if (Direct3D9Reset(vd)) {
                msg_Err(vd, "Failed to reset device");
                return VLC_EGENERIC;
            }
            sys->reset_device = false;
        } else if(sys->reopen_device) {
            if (ControlReopenDevice(vd)) {
                msg_Err(vd, "Failed to reopen device");
                return VLC_EGENERIC;
            }
            sys->reopen_device = false;
        }
        return VLC_SUCCESS;
    default:
        return CommonControl(vd, query, args);
    }
}
static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    /* Desktop mode change */
    vlc_mutex_lock(&sys->lock);
    const bool ch_desktop = sys->ch_desktop;
    sys->ch_desktop = false;
    vlc_mutex_unlock(&sys->lock);

    if (ch_desktop) {
        sys->reopen_device = true;
        if (vd->info.has_pictures_invalid)
            vout_display_SendEventPicturesInvalid(vd);
    }

    /* Position Change */
    if (sys->sys.changes & DX_POSITION_CHANGE) {
#if 0 /* need that when bicubic filter is available */
        RECT rect;
        UINT width, height;

        GetClientRect(p_sys->sys.hvideownd, &rect);
        width  = rect.right-rect.left;
        height = rect.bottom-rect.top;

        if (width != p_sys->pp.BackBufferWidth || height != p_sys->pp.BackBufferHeight)
        {
            msg_Dbg(vd, "resizing device back buffers to (%lux%lu)", width, height);
            // need to reset D3D device to resize back buffer
            if (VLC_SUCCESS != Direct3D9ResetDevice(vd, width, height))
                return VLC_EGENERIC;
        }
#endif
        sys->clear_scene = true;
        sys->sys.changes &= ~DX_POSITION_CHANGE;
    }
}

/**
 * It releases an instance of Direct3D9
 */
static void Direct3D9Destroy(vout_display_sys_t *sys)
{
    if (sys->processor.proc)
    {
        IDXVAHD_VideoProcessor_Release(sys->processor.proc);
        FreeLibrary(sys->processor.dll);
    }
    D3D9_Destroy( &sys->hd3d );

    if (sys->hxdll)
    {
        FreeLibrary(sys->hxdll);
        sys->hxdll = NULL;
    }
}

/* */
static int  Direct3D9CreateResources (vout_display_t *, video_format_t *);
static void Direct3D9DestroyResources(vout_display_t *);

static void SetupProcessorInput(vout_display_t *vd, const video_format_t *fmt, const d3d9_format_t *d3dfmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    DXVAHD_STREAM_STATE_D3DFORMAT_DATA d3dformat = { d3dfmt->format };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_D3DFORMAT, sizeof(d3dformat), &d3dformat );

    DXVAHD_STREAM_STATE_FRAME_FORMAT_DATA frame_format = { DXVAHD_FRAME_FORMAT_PROGRESSIVE };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_FRAME_FORMAT, sizeof(frame_format), &frame_format );

    DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA colorspace = { 0 };
    colorspace.RGB_Range = fmt->b_color_range_full ? 0 : 1;
    colorspace.YCbCr_xvYCC = fmt->b_color_range_full ? 1 : 0;
    colorspace.YCbCr_Matrix = fmt->space == COLOR_SPACE_BT601 ? 0 : 1;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE, sizeof(colorspace), &colorspace );

    DXVAHD_STREAM_STATE_SOURCE_RECT_DATA srcRect;
    srcRect.Enable = TRUE;
    srcRect.SourceRect = (RECT) {
        .left   = vd->source.i_x_offset,
        .right  = vd->source.i_x_offset + vd->source.i_visible_width,
        .top    = vd->source.i_y_offset,
        .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
    };;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_SOURCE_RECT, sizeof(srcRect), &srcRect );

    DXVAHD_BLT_STATE_TARGET_RECT_DATA dstRect;
    dstRect.Enable = TRUE;
    dstRect.TargetRect = (RECT) {
        .left   = 0,
        .right  = vd->source.i_visible_width,
        .top    = 0,
        .bottom = vd->source.i_visible_height,
    };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessBltState( sys->processor.proc, DXVAHD_BLT_STATE_TARGET_RECT, sizeof(dstRect), &dstRect);
}

static void GetFrameRate(DXVAHD_RATIONAL *r, const video_format_t *fmt)
{
    if (fmt->i_frame_rate && fmt->i_frame_rate_base)
    {
        r->Numerator   = fmt->i_frame_rate;
        r->Denominator = fmt->i_frame_rate_base;
    }
    else
    {
        r->Numerator   = 0;
        r->Denominator = 0;
    }
}

static int InitRangeProcessor(vout_display_t *vd, const d3d9_format_t *d3dfmt)
{
    vout_display_sys_t *sys = vd->sys;

    HRESULT hr;

    sys->processor.dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!sys->processor.dll)
        return VLC_EGENERIC;

    D3DFORMAT *formatsList = NULL;
    DXVAHD_VPCAPS *capsList = NULL;
    IDXVAHD_Device *hd_device = NULL;

    HRESULT (WINAPI *CreateDevice)(IDirect3DDevice9Ex *,const DXVAHD_CONTENT_DESC *,DXVAHD_DEVICE_USAGE,PDXVAHDSW_Plugin,IDXVAHD_Device **);
    CreateDevice = (void *)GetProcAddress(sys->processor.dll, "DXVAHD_CreateDevice");
    if (CreateDevice == NULL)
    {
        msg_Err(vd, "Can't create HD device (not Windows 7+)");
        goto error;
    }

    DXVAHD_CONTENT_DESC desc;
    desc.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    GetFrameRate( &desc.InputFrameRate, &vd->source );
    desc.InputWidth       = vd->source.i_visible_width;
    desc.InputHeight      = vd->source.i_visible_height;
    desc.OutputFrameRate  = desc.InputFrameRate;
    desc.OutputWidth      = vd->source.i_visible_width;
    desc.OutputHeight     = vd->source.i_visible_height;

    hr = CreateDevice(sys->d3d_dev.devex, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &hd_device);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to create the device (error 0x%lX)", hr);
        goto error;
    }

    DXVAHD_VPDEVCAPS devcaps = { 0 };
    hr = IDXVAHD_Device_GetVideoProcessorDeviceCaps( hd_device, &devcaps );
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Failed to get the device capabilities (error 0x%lX)", hr);
        goto error;
    }
    if (devcaps.VideoProcessorCount == 0)
    {
        msg_Warn(vd, "No good video processor found for range conversion");
        goto error;
    }

    formatsList = malloc(devcaps.InputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
        goto error;

    hr = IDXVAHD_Device_GetVideoProcessorInputFormats( hd_device, devcaps.InputFormatCount, formatsList);
    UINT i;
    for (i=0; i<devcaps.InputFormatCount; i++)
    {
        if (formatsList[i] == d3dfmt->format)
            break;
    }
    if (i == devcaps.InputFormatCount)
    {
        msg_Warn(vd, "Input format %s not supported for range conversion", d3dfmt->name);
        goto error;
    }

    free(formatsList);
    formatsList = malloc(devcaps.OutputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
        goto error;

    hr = IDXVAHD_Device_GetVideoProcessorOutputFormats( hd_device, devcaps.OutputFormatCount, formatsList);
    for (i=0; i<devcaps.OutputFormatCount; i++)
    {
        if (formatsList[i] == sys->d3d_dev.pp.BackBufferFormat)
            break;
    }
    if (i == devcaps.OutputFormatCount)
    {
        msg_Warn(vd, "Output format %s not supported for range conversion", d3dfmt->name);
        goto error;
    }

    capsList = malloc(devcaps.VideoProcessorCount * sizeof(*capsList));
    if (unlikely(capsList == NULL))
        goto error;
    hr = IDXVAHD_Device_GetVideoProcessorCaps( hd_device, devcaps.VideoProcessorCount, capsList);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to get the processor caps (error 0x%lX)", hr);
        goto error;
    }

    hr = IDXVAHD_Device_CreateVideoProcessor( hd_device, &capsList->VPGuid, &sys->processor.proc );
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to create the processor (error 0x%lX)", hr);
        goto error;
    }
    IDXVAHD_Device_Release( hd_device );

    SetupProcessorInput(vd, &vd->source, d3dfmt);

    DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA colorspace;
    colorspace.Usage = 0; // playback
    colorspace.RGB_Range = 0; // full range display
    colorspace.YCbCr_xvYCC = 1;
    colorspace.YCbCr_Matrix = 1; // BT.709
    hr = IDXVAHD_VideoProcessor_SetVideoProcessBltState( sys->processor.proc, DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE, sizeof(colorspace), &colorspace);

    return VLC_SUCCESS;

error:
    free(capsList);
    free(formatsList);
    if (hd_device)
        IDXVAHD_Device_Release(hd_device);
    FreeLibrary(sys->processor.dll);
    return VLC_EGENERIC;
}

/**
 * It creates a Direct3D9 device and the associated resources.
 */
static int Direct3D9Open(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    if (FAILED(D3D9_CreateDevice(vd, &sys->hd3d, sys->sys.hvideownd,
                                 &vd->source, &sys->d3d_dev)))
        return VLC_EGENERIC;

    const d3d9_device_t *p_d3d9_dev = &sys->d3d_dev;
    /* */
    RECT *display = &vd->sys->sys.rect_display;
    display->left   = 0;
    display->top    = 0;
    display->right  = p_d3d9_dev->pp.BackBufferWidth;
    display->bottom = p_d3d9_dev->pp.BackBufferHeight;

    *fmt = vd->source;

    /* Find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
     * the requested chroma which is usable by the hardware in an offscreen surface, as they
     * typically support more formats than textures */
    const d3d9_format_t *d3dfmt = Direct3DFindFormat(vd, fmt->i_chroma, p_d3d9_dev->pp.BackBufferFormat);
    if (!d3dfmt) {
        msg_Err(vd, "surface pixel format is not supported.");
        goto error;
    }
    const d3d9_format_t *d3dbuffer = FindBufferFormat(vd, p_d3d9_dev->pp.BackBufferFormat);
    if (!d3dbuffer)
        msg_Warn(vd, "Unknown back buffer format 0x%X", p_d3d9_dev->pp.BackBufferFormat);
    else if (!vd->source.b_color_range_full && d3dbuffer->rmask && !d3dfmt->rmask)
    {
        D3DADAPTER_IDENTIFIER9 d3dai;
        if (sys->hd3d.use_ex && SUCCEEDED(IDirect3D9Ex_GetAdapterIdentifier(sys->hd3d.objex, sys->d3d_dev.adapterId, 0, &d3dai)) &&
            d3dai.VendorId == GPU_MANUFACTURER_NVIDIA) {

            // NVIDIA bug, YUV to RGB internal conversion in StretchRect always converts from limited to limited range
            InitRangeProcessor( vd, d3dfmt );
        }
    }

    fmt->i_chroma = d3dfmt->fourcc;
    fmt->i_rmask  = d3dfmt->rmask;
    fmt->i_gmask  = d3dfmt->gmask;
    fmt->i_bmask  = d3dfmt->bmask;
    sys->d3dtexture_format = d3dfmt;

    UpdateRects(vd, NULL, true);

    if (Direct3D9CreateResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate resources");
        goto error;
    }

    /* Change the window title bar text */
    EventThreadUpdateTitle(sys->sys.event, VOUT_TITLE " (Direct3D9 output)");

    msg_Dbg(vd, "Direct3D9 device adapter successfully initialized");
    return VLC_SUCCESS;

error:
    D3D9_ReleaseDevice(&sys->d3d_dev);
    return VLC_EGENERIC;
}

/**
 * It releases the Direct3D9 device and its resources.
 */
static void Direct3D9Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D9DestroyResources(vd);
    D3D9_ReleaseDevice(&sys->d3d_dev);
}

/**
 * It reset the Direct3D9 device and its resources.
 */
static int Direct3D9Reset(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    d3d9_device_t *p_d3d9_dev = &sys->d3d_dev;

    if (D3D9_FillPresentationParameters(&sys->hd3d, &vd->source, p_d3d9_dev))
    {
        msg_Err(vd, "Could not presentation parameters to reset device");
        return VLC_EGENERIC;
    }

    /* release all D3D objects */
    Direct3D9DestroyResources(vd);

    /* */
    HRESULT hr;
    if (sys->hd3d.use_ex){
        hr = IDirect3DDevice9Ex_ResetEx(p_d3d9_dev->devex, &p_d3d9_dev->pp, NULL);
    } else {
        hr = IDirect3DDevice9_Reset(p_d3d9_dev->dev, &p_d3d9_dev->pp);
    }
    if (FAILED(hr)) {
        msg_Err(vd, "IDirect3DDevice9_Reset failed! (hr=0x%0lx)", hr);
        return VLC_EGENERIC;
    }

    UpdateRects(vd, NULL, true);

    /* re-create them */
    if (Direct3D9CreateResources(vd, &vd->fmt)) {
        msg_Dbg(vd, "Direct3D9CreateResources failed !");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/* */
static int  Direct3D9CreateScene(vout_display_t *vd, const video_format_t *fmt);
static void Direct3D9DestroyScene(vout_display_t *vd);

static int  Direct3D9CreateShaders(vout_display_t *vd);
static void Direct3D9DestroyShaders(vout_display_t *vd);

/**
 * It creates the picture and scene resources.
 */
static int Direct3D9CreateResources(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    if (Direct3D9CreateScene(vd, fmt)) {
        msg_Err(vd, "Direct3D scene initialization failed !");
        return VLC_EGENERIC;
    }
    if (Direct3D9CreateShaders(vd)) {
        /* Failing to initialize shaders is not fatal. */
        msg_Warn(vd, "Direct3D shaders initialization failed !");
    }

    sys->d3dregion_format = D3DFMT_UNKNOWN;
    for (int i = 0; i < 2; i++) {
        D3DFORMAT dfmt = i == 0 ? D3DFMT_A8B8G8R8 : D3DFMT_A8R8G8B8;
        if (SUCCEEDED(IDirect3D9_CheckDeviceFormat(sys->hd3d.obj,
                                                   D3DADAPTER_DEFAULT,
                                                   D3DDEVTYPE_HAL,
                                                   sys->d3d_dev.pp.BackBufferFormat,
                                                   D3DUSAGE_DYNAMIC,
                                                   D3DRTYPE_TEXTURE,
                                                   dfmt))) {
            sys->d3dregion_format = dfmt;
            break;
        }
    }
    return VLC_SUCCESS;
}
/**
 * It destroys the picture and scene resources.
 */
static void Direct3D9DestroyResources(vout_display_t *vd)
{
    Direct3D9DestroyScene(vd);
    if (vd->sys->sys.pool)
    {
        picture_pool_Release(vd->sys->sys.pool);
        vd->sys->sys.pool = NULL;
    }
    Direct3D9DestroyShaders(vd);
}

/**
 * It tests if the conversion from src to dst is supported.
 */
static int Direct3D9CheckConversion(vout_display_t *vd,
                                   D3DFORMAT src, D3DFORMAT dst)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3D9 d3dobj = sys->hd3d.obj;
    HRESULT hr;

    /* test whether device can create a surface of that format */
    hr = IDirect3D9_CheckDeviceFormat(d3dobj, D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL, dst, 0,
                                      D3DRTYPE_SURFACE, src);
    if (SUCCEEDED(hr)) {
        /* test whether device can perform color-conversion
        ** from that format to target format
        */
        hr = IDirect3D9_CheckDeviceFormatConversion(d3dobj,
                                                    D3DADAPTER_DEFAULT,
                                                    D3DDEVTYPE_HAL,
                                                    src, dst);
    }
    if (!SUCCEEDED(hr)) {
        if (D3DERR_NOTAVAILABLE != hr)
            msg_Err(vd, "Could not query adapter supported formats. (hr=0x%0lx)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static const d3d9_format_t d3d_formats[] = {
    /* YV12 is always used for planar 420, the planes are then swapped in Lock() */
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_I420,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_J420,  0,0,0 },
    { "NV12",       MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12,  0,0,0 },
    { "DXA9",       MAKEFOURCC('N','V','1','2'),    VLC_CODEC_D3D9_OPAQUE,  0,0,0 },
    { "DXA9_10",    MAKEFOURCC('P','0','1','0'),    VLC_CODEC_D3D9_OPAQUE_10B,  0,0,0 },
    { "UYVY",       D3DFMT_UYVY,    VLC_CODEC_UYVY,  0,0,0 },
    { "YUY2",       D3DFMT_YUY2,    VLC_CODEC_YUYV,  0,0,0 },
    { "X8R8G8B8",   D3DFMT_X8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "A8R8G8B8",   D3DFMT_A8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "8G8B8",      D3DFMT_R8G8B8,  VLC_CODEC_RGB24, 0xff0000, 0x00ff00, 0x0000ff },
    { "R5G6B5",     D3DFMT_R5G6B5,  VLC_CODEC_RGB16, 0x1f<<11, 0x3f<<5,  0x1f<<0 },
    { "X1R5G5B5",   D3DFMT_X1R5G5B5,VLC_CODEC_RGB15, 0x1f<<10, 0x1f<<5,  0x1f<<0 },

    { NULL, 0, 0, 0,0,0}
};

static const d3d9_format_t *FindBufferFormat(vout_display_t *vd, D3DFORMAT fmt)
{
    for (unsigned j = 0; d3d_formats[j].name; j++) {
        const d3d9_format_t *format = &d3d_formats[j];

        if (format->format != fmt)
            continue;

        return format;
    }
    return NULL;
}

/**
 * It returns the format (closest to chroma) that can be converted to target */
static const d3d9_format_t *Direct3DFindFormat(vout_display_t *vd, vlc_fourcc_t chroma, D3DFORMAT target)
{
    vout_display_sys_t *sys = vd->sys;
    bool hardware_scale_ok = !(vd->fmt.i_visible_width & 1) && !(vd->fmt.i_visible_height & 1);
    if( !hardware_scale_ok )
        msg_Warn( vd, "Disabling hardware chroma conversion due to odd dimensions" );

    for (unsigned pass = 0; pass < 2; pass++) {
        const vlc_fourcc_t *list;
        const vlc_fourcc_t dxva_chroma[] = {chroma, 0};

        if (pass == 0 && is_d3d9_opaque(chroma))
            list = dxva_chroma;
        else if (pass == 0 && hardware_scale_ok && sys->allow_hw_yuv && vlc_fourcc_IsYUV(chroma))
            list = vlc_fourcc_GetYUVFallback(chroma);
        else if (pass == 1)
            list = vlc_fourcc_GetRGBFallback(chroma);
        else
            continue;

        for (unsigned i = 0; list[i] != 0; i++) {
            for (unsigned j = 0; d3d_formats[j].name; j++) {
                const d3d9_format_t *format = &d3d_formats[j];

                if (format->fourcc != list[i])
                    continue;

                msg_Warn(vd, "trying surface pixel format: %s",
                         format->name);
                if (!Direct3D9CheckConversion(vd, format->format, target)) {
                    msg_Dbg(vd, "selected surface pixel format is %s",
                            format->name);
                    return format;
                }
            }
        }
    }
    return NULL;
}

/**
 * It allocates and initializes the resources needed to render the scene.
 */
static int Direct3D9CreateScene(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    const d3d9_device_t *p_d3d9_dev = &sys->d3d_dev;
    LPDIRECT3DDEVICE9       d3ddev = p_d3d9_dev->dev;
    HRESULT hr;

    // On nVidia & AMD, StretchRect will fail if the visible size isn't even.
    // When copying the entire buffer, the margin end up being blended in the actual picture
    // on nVidia (regardless of even/odd dimensions)
    sys->texture_width  = fmt->i_visible_width;
    sys->texture_height = fmt->i_visible_height;
    if (sys->texture_width  & 1) sys->texture_width++;
    if (sys->texture_height & 1) sys->texture_height++;

    /*
     * Create a texture for use when rendering a scene
     * for performance reason, texture format is identical to backbuffer
     * which would usually be a RGB format
     */
    LPDIRECT3DTEXTURE9 d3dtex;
    hr = IDirect3DDevice9_CreateTexture(d3ddev,
                                        sys->texture_width,
                                        sys->texture_height,
                                        1,
                                        D3DUSAGE_RENDERTARGET,
                                        p_d3d9_dev->pp.BackBufferFormat,
                                        D3DPOOL_DEFAULT,
                                        &d3dtex,
                                        NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create texture. (hr=0x%lx)", hr);
        return VLC_EGENERIC;
    }

#ifndef NDEBUG
    msg_Dbg(vd, "Direct3D created texture: %ix%i", sys->texture_width, sys->texture_height);
#endif

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    LPDIRECT3DVERTEXBUFFER9 d3dvtc;
    hr = IDirect3DDevice9_CreateVertexBuffer(d3ddev,
                                             sizeof(CUSTOMVERTEX)*4,
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             D3DFVF_CUSTOMVERTEX,
                                             D3DPOOL_DEFAULT,
                                             &d3dvtc,
                                             NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create vertex buffer. (hr=0x%lx)", hr);
        IDirect3DTexture9_Release(d3dtex);
        return VLC_EGENERIC;
    }

    /* */
    sys->d3dtex = d3dtex;
    sys->d3dvtc = d3dvtc;

    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;

    sys->clear_scene = true;

    // Texture coordinates outside the range [0.0, 1.0] are set
    // to the texture color at 0.0 or 1.0, respectively.
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    if (sys->d3d_dev.caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) {
        //msg_Dbg(vd, "Using D3DTEXF_LINEAR for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    } else {
        //msg_Dbg(vd, "Using D3DTEXF_POINT for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }
    if (sys->d3d_dev.caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) {
        //msg_Dbg(vd, "Using D3DTEXF_LINEAR for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    } else {
        //msg_Dbg(vd, "Using D3DTEXF_POINT for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    }

    // set maximum ambient light
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_LIGHTING, FALSE);

    // Enable dithering
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_STENCILENABLE, FALSE);

    // manage blending
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

    if (sys->d3d_dev.caps.AlphaCmpCaps & D3DPCMPCAPS_GREATER) {
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHATESTENABLE,TRUE);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAREF, 0x00);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAFUNC,D3DCMP_GREATER);
    }

    // Set texture states
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLORARG1,D3DTA_TEXTURE);

    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);

    msg_Dbg(vd, "Direct3D9 scene created successfully");

    return VLC_SUCCESS;
}

/**
 * It releases the scene resources.
 */
static void Direct3D9DestroyScene(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D9DeleteRegions(sys->d3dregion_count, sys->d3dregion);

    LPDIRECT3DVERTEXBUFFER9 d3dvtc = sys->d3dvtc;
    if (d3dvtc)
        IDirect3DVertexBuffer9_Release(d3dvtc);

    LPDIRECT3DTEXTURE9 d3dtex = sys->d3dtex;
    if (d3dtex)
        IDirect3DTexture9_Release(d3dtex);

    sys->d3dvtc = NULL;
    sys->d3dtex = NULL;

    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;

    msg_Dbg(vd, "Direct3D9 scene released successfully");
}

static int Direct3D9CompileShader(vout_display_t *vd, const char *shader_source, size_t source_length)
{
#ifdef HAVE_D3DX9EFFECT_H
    vout_display_sys_t *sys = vd->sys;

    HRESULT (WINAPI * OurD3DXCompileShader)(
            LPCSTR pSrcData,
            UINT srcDataLen,
            const D3DXMACRO *pDefines,
            LPD3DXINCLUDE pInclude,
            LPCSTR pFunctionName,
            LPCSTR pProfile,
            DWORD Flags,
            LPD3DXBUFFER *ppShader,
            LPD3DXBUFFER *ppErrorMsgs,
            LPD3DXCONSTANTTABLE *ppConstantTable);

    OurD3DXCompileShader = (void*)GetProcAddress(sys->hxdll, "D3DXCompileShader");
    if (!OurD3DXCompileShader) {
        msg_Warn(vd, "Cannot locate reference to D3DXCompileShader; pixel shading will be disabled");
        return VLC_EGENERIC;
    }

    LPD3DXBUFFER error_msgs = NULL;
    LPD3DXBUFFER compiled_shader = NULL;

    DWORD shader_flags = 0;
    HRESULT hr = OurD3DXCompileShader(shader_source, source_length, NULL, NULL,
                "main", "ps_3_0", shader_flags, &compiled_shader, &error_msgs, NULL);

    if (FAILED(hr)) {
        msg_Warn(vd, "D3DXCompileShader Error (hr=0x%0lx)", hr);
        if (error_msgs) {
            msg_Warn(vd, "HLSL Compilation Error: %s", (char*)ID3DXBuffer_GetBufferPointer(error_msgs));
            ID3DXBuffer_Release(error_msgs);
    }
        return VLC_EGENERIC;
    }

    hr = IDirect3DDevice9_CreatePixelShader(sys->d3d_dev.dev,
            ID3DXBuffer_GetBufferPointer(compiled_shader),
            &sys->d3dx_shader);

    if (compiled_shader)
        ID3DXBuffer_Release(compiled_shader);
    if (error_msgs)
        ID3DXBuffer_Release(error_msgs);

    if (FAILED(hr)) {
        msg_Warn(vd, "IDirect3DDevice9_CreatePixelShader error (hr=0x%0lx)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
#else
    return VLC_EGENERIC;
#endif
}

#define MAX_SHADER_FILE_SIZE 1024*1024

static int Direct3D9CreateShaders(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->hxdll)
        return VLC_EGENERIC;

    /* Find which shader was selected in the list. */
    char *selected_shader = var_InheritString(vd, "direct3d9-shader");
    if (!selected_shader)
        return VLC_SUCCESS; /* Nothing to do */

    const char *shader_source_builtin = NULL;
    char *shader_source_file = NULL;
    FILE *fs = NULL;

    for (size_t i = 0; i < BUILTIN_SHADERS_COUNT; ++i) {
        if (!strcmp(selected_shader, builtin_shaders[i].name)) {
            shader_source_builtin = builtin_shaders[i].code;
            break;
        }
    }

    if (shader_source_builtin) {
        /* A builtin shader was selected. */
        int err = Direct3D9CompileShader(vd, shader_source_builtin, strlen(shader_source_builtin));
        if (err)
            goto error;
    } else {
        if (strcmp(selected_shader, SELECTED_SHADER_FILE))
            goto error; /* Unrecognized entry in the list. */
        /* The source code of the shader needs to be read from a file. */
        char *filepath = var_InheritString(vd, "direct3d9-shader-file");
        if (!filepath || !*filepath)
        {
            free(filepath);
            goto error;
        }
        /* Open file, find its size with fseek/ftell and read its content in a buffer. */
        fs = fopen(filepath, "rb");
        if (!fs)
            goto error;
        int ret = fseek(fs, 0, SEEK_END);
        if (ret == -1)
            goto error;
        long length = ftell(fs);
        if (length == -1 || length >= MAX_SHADER_FILE_SIZE)
            goto error;
        rewind(fs);
        shader_source_file = vlc_alloc(length, sizeof(*shader_source_file));
        if (!shader_source_file)
            goto error;
        ret = fread(shader_source_file, length, 1, fs);
        if (ret != 1)
            goto error;
        ret = Direct3D9CompileShader(vd, shader_source_file, length);
        if (ret)
            goto error;
    }

    free(selected_shader);
    free(shader_source_file);
    fclose(fs);

    return VLC_SUCCESS;

error:
    Direct3D9DestroyShaders(vd);
    free(selected_shader);
    free(shader_source_file);
    if (fs)
        fclose(fs);
    return VLC_EGENERIC;
}

static void Direct3D9DestroyShaders(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->d3dx_shader)
        IDirect3DPixelShader9_Release(sys->d3dx_shader);
    sys->d3dx_shader = NULL;
}

/**
 * Compute the vertex ordering needed to rotate the video. Without
 * rotation, the vertices of the rectangle are defined in a clockwise
 * order. This function computes a remapping of the coordinates to
 * implement the rotation, given fixed texture coordinates.
 * The unrotated order is the following:
 * 0--1
 * |  |
 * 3--2
 * For a 180 degrees rotation it should like this:
 * 2--3
 * |  |
 * 1--0
 * Vertex 0 should be assigned coordinates at index 2 from the
 * unrotated order and so on, thus yielding order: 2 3 0 1.
 */
static void orientationVertexOrder(video_orientation_t orientation, int vertex_order[static 4])
{
    switch (orientation) {
        case ORIENT_ROTATED_90:      /* ORIENT_RIGHT_TOP */
            vertex_order[0] = 1;
            vertex_order[1] = 2;
            vertex_order[2] = 3;
            vertex_order[3] = 0;
            break;
        case ORIENT_ROTATED_270:     /* ORIENT_LEFT_BOTTOM */
            vertex_order[0] = 3;
            vertex_order[1] = 0;
            vertex_order[2] = 1;
            vertex_order[3] = 2;
            break;
        case ORIENT_ROTATED_180:     /* ORIENT_BOTTOM_RIGHT */
            vertex_order[0] = 2;
            vertex_order[1] = 3;
            vertex_order[2] = 0;
            vertex_order[3] = 1;
            break;
        case ORIENT_TRANSPOSED:      /* ORIENT_LEFT_TOP */
            vertex_order[0] = 0;
            vertex_order[1] = 3;
            vertex_order[2] = 2;
            vertex_order[3] = 1;
            break;
        case ORIENT_HFLIPPED:        /* ORIENT_TOP_RIGHT */
            vertex_order[0] = 1;
            vertex_order[1] = 0;
            vertex_order[2] = 3;
            vertex_order[3] = 2;
            break;
        case ORIENT_VFLIPPED:        /* ORIENT_BOTTOM_LEFT */
            vertex_order[0] = 3;
            vertex_order[1] = 2;
            vertex_order[2] = 1;
            vertex_order[3] = 0;
            break;
        case ORIENT_ANTI_TRANSPOSED: /* ORIENT_RIGHT_BOTTOM */
            vertex_order[0] = 2;
            vertex_order[1] = 1;
            vertex_order[2] = 0;
            vertex_order[3] = 3;
            break;
       default:
            vertex_order[0] = 0;
            vertex_order[1] = 1;
            vertex_order[2] = 2;
            vertex_order[3] = 3;
            break;
    }
}

static void  Direct3D9SetupVertices(CUSTOMVERTEX *vertices,
                                  const RECT *full_texture, const RECT *visible_texture,
                                  const RECT *rect_in_display,
                                  int alpha,
                                  video_orientation_t orientation)
{
    /* Vertices of the dst rectangle in the unrotated (clockwise) order. */
    const int vertices_coords[4][2] = {
        { rect_in_display->left,  rect_in_display->top    },
        { rect_in_display->right, rect_in_display->top    },
        { rect_in_display->right, rect_in_display->bottom },
        { rect_in_display->left,  rect_in_display->bottom },
    };

    /* Compute index remapping necessary to implement the rotation. */
    int vertex_order[4];
    orientationVertexOrder(orientation, vertex_order);

    for (int i = 0; i < 4; ++i) {
        vertices[i].x  = vertices_coords[vertex_order[i]][0];
        vertices[i].y  = vertices_coords[vertex_order[i]][1];
    }

    float texture_right  = (float)visible_texture->right / (float)full_texture->right;
    float texture_left   = (float)visible_texture->left  / (float)full_texture->right;
    float texture_top    = (float)visible_texture->top    / (float)full_texture->bottom;
    float texture_bottom = (float)visible_texture->bottom / (float)full_texture->bottom;

    vertices[0].tu = texture_left;
    vertices[0].tv = texture_top;

    vertices[1].tu = texture_right;
    vertices[1].tv = texture_top;

    vertices[2].tu = texture_right;
    vertices[2].tv = texture_bottom;

    vertices[3].tu = texture_left;
    vertices[3].tv = texture_bottom;

    for (int i = 0; i < 4; i++) {
        /* -0.5f is a "feature" of DirectX and it seems to apply to Direct3d also */
        /* http://www.sjbrown.co.uk/2003/05/01/fix-directx-rasterisation/ */
        vertices[i].x -= 0.5;
        vertices[i].y -= 0.5;

        vertices[i].z       = 0.0f;
        vertices[i].rhw     = 1.0f;
        vertices[i].diffuse = D3DCOLOR_ARGB(alpha, 255, 255, 255);
    }
}

/**
 * It copies picture surface into a texture and setup the associated d3d_region_t.
 */
static int Direct3D9ImportPicture(vout_display_t *vd,
                                 d3d_region_t *region,
                                 LPDIRECT3DSURFACE9 source)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    if (!source) {
        msg_Dbg(vd, "no surface to render?");
        return VLC_EGENERIC;
    }

    /* retrieve texture top-level surface */
    LPDIRECT3DSURFACE9 destination;
    hr = IDirect3DTexture9_GetSurfaceLevel(sys->d3dtex, 0, &destination);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DTexture9_GetSurfaceLevel: 0x%0lx", hr);
        return VLC_EGENERIC;
    }

    if (sys->processor.proc)
    {
        DXVAHD_STREAM_DATA inputStream = { 0 };
        inputStream.Enable = TRUE;
        inputStream.pInputSurface = source;
        hr = IDXVAHD_VideoProcessor_VideoProcessBltHD( sys->processor.proc, destination, 0, 1, &inputStream );
    }
    else
    {
        /* Copy picture surface into texture surface
        * color space conversion happen here */
        RECT texture_visible_rect = sys->sys.rect_src_clipped;
        // On nVidia & AMD, StretchRect will fail if the visible size isn't even.
        // When copying the entire buffer, the margin end up being blended in the actual picture
        // on nVidia (regardless of even/odd dimensions)
        if ( texture_visible_rect.right & 1 ) texture_visible_rect.right++;
        if ( texture_visible_rect.left & 1 ) texture_visible_rect.left--;
        if ( texture_visible_rect.bottom & 1 ) texture_visible_rect.bottom++;
        if ( texture_visible_rect.top & 1 ) texture_visible_rect.top--;
        hr = IDirect3DDevice9_StretchRect(sys->d3d_dev.dev, source, &texture_visible_rect, destination,
                                        &texture_visible_rect, D3DTEXF_NONE);
    }
    IDirect3DSurface9_Release(destination);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_StretchRect: source 0x%p 0x%0lx",
                (LPVOID)source, hr);
        return VLC_EGENERIC;
    }

    /* */
    region->texture = sys->d3dtex;
    Direct3D9SetupVertices(region->vertex, &vd->sys->sys.rect_src, &vd->sys->sys.rect_src_clipped,
                           &vd->sys->sys.rect_dest_clipped, 255, vd->fmt.orientation);
    return VLC_SUCCESS;
}

static void Direct3D9DeleteRegions(int count, d3d_region_t *region)
{
    for (int i = 0; i < count; i++) {
        if (region[i].texture)
            IDirect3DTexture9_Release(region[i].texture);
    }
    free(region);
}

static void Direct3D9ImportSubpicture(vout_display_t *vd,
                                     int *count_ptr, d3d_region_t **region,
                                     subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    int count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *count_ptr = count;
    *region    = calloc(count, sizeof(**region));
    if (*region == NULL) {
        *count_ptr = 0;
        return;
    }

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        d3d_region_t *d3dr = &(*region)[i];
        HRESULT hr;

        d3dr->texture = NULL;
        for (int j = 0; j < sys->d3dregion_count; j++) {
            d3d_region_t *cache = &sys->d3dregion[j];
            if (cache->texture &&
                cache->format == sys->d3dregion_format &&
                cache->width  == r->fmt.i_width &&
                cache->height == r->fmt.i_height) {
                *d3dr = *cache;
                memset(cache, 0, sizeof(*cache));
                break;
            }
        }
        if (!d3dr->texture) {
            d3dr->format = sys->d3dregion_format;
            d3dr->width  = r->fmt.i_width;
            d3dr->height = r->fmt.i_height;
            hr = IDirect3DDevice9_CreateTexture(sys->d3d_dev.dev,
                                                d3dr->width, d3dr->height,
                                                1,
                                                D3DUSAGE_DYNAMIC,
                                                d3dr->format,
                                                D3DPOOL_DEFAULT,
                                                &d3dr->texture,
                                                NULL);
            if (FAILED(hr)) {
                d3dr->texture = NULL;
                msg_Err(vd, "Failed to create %dx%d texture for OSD (hr=0x%0lx)",
                        d3dr->width, d3dr->height, hr);
                continue;
            }
#ifndef NDEBUG
            msg_Dbg(vd, "Created %dx%d texture for OSD",
                    r->fmt.i_width, r->fmt.i_height);
#endif
        }

        D3DLOCKED_RECT lock;
        hr = IDirect3DTexture9_LockRect(d3dr->texture, 0, &lock, NULL, 0);
        if (SUCCEEDED(hr)) {
            uint8_t  *dst_data   = lock.pBits;
            int       dst_pitch  = lock.Pitch;
            uint8_t  *src_data   = r->p_picture->p->p_pixels;
            int       src_pitch  = r->p_picture->p->i_pitch;

            if (d3dr->format == D3DFMT_A8B8G8R8) {
                if (dst_pitch == r->p_picture->p->i_pitch) {
                    memcpy(dst_data, src_data, r->fmt.i_height * dst_pitch);
                } else {
                    int copy_pitch = __MIN(dst_pitch, r->p_picture->p->i_pitch);
                    for (unsigned y = 0; y < r->fmt.i_height; y++) {
                        memcpy(&dst_data[y * dst_pitch], &src_data[y * src_pitch], copy_pitch);
                    }
                }
            } else {
                int copy_pitch = __MIN(dst_pitch, r->p_picture->p->i_pitch);
                for (unsigned y = 0; y < r->fmt.i_height; y++) {
                    for (int x = 0; x < copy_pitch; x += 4) {
                        dst_data[y * dst_pitch + x + 0] = src_data[y * src_pitch + x + 2];
                        dst_data[y * dst_pitch + x + 1] = src_data[y * src_pitch + x + 1];
                        dst_data[y * dst_pitch + x + 2] = src_data[y * src_pitch + x + 0];
                        dst_data[y * dst_pitch + x + 3] = src_data[y * src_pitch + x + 3];
                    }
                }
            }
            hr = IDirect3DTexture9_UnlockRect(d3dr->texture, 0);
            if (FAILED(hr))
                msg_Err(vd, "Failed to unlock the texture");
        } else {
            msg_Err(vd, "Failed to lock the texture");
        }

        /* Map the subpicture to sys->sys.rect_dest */
        const RECT video = sys->sys.rect_dest;
        const float scale_w = (float)(video.right  - video.left) / subpicture->i_original_picture_width;
        const float scale_h = (float)(video.bottom - video.top)  / subpicture->i_original_picture_height;

        RECT rect_in_display;
        rect_in_display.left   = video.left + scale_w * r->i_x,
        rect_in_display.right  = rect_in_display.left + scale_w * r->fmt.i_visible_width,
        rect_in_display.top    = video.top  + scale_h * r->i_y,
        rect_in_display.bottom = rect_in_display.top  + scale_h * r->fmt.i_visible_height;

        RECT texture_rect;
        texture_rect.left   = 0;
        texture_rect.right  = r->fmt.i_width;
        texture_rect.top    = 0;
        texture_rect.bottom = r->fmt.i_height;

        RECT texture_visible_rect;
        texture_visible_rect.left   = r->fmt.i_x_offset;
        texture_visible_rect.right  = r->fmt.i_x_offset + r->fmt.i_visible_width;
        texture_visible_rect.top    = r->fmt.i_y_offset;
        texture_visible_rect.bottom = r->fmt.i_y_offset + r->fmt.i_visible_height;

        Direct3D9SetupVertices(d3dr->vertex, &texture_rect, &texture_visible_rect,
                              &rect_in_display, subpicture->i_alpha * r->i_alpha / 255, ORIENT_NORMAL);
    }
}

static int Direct3D9RenderRegion(vout_display_t *vd,
                                d3d_region_t *region,
                                bool use_pixel_shader)
{
    vout_display_sys_t *sys = vd->sys;

    LPDIRECT3DDEVICE9 d3ddev = vd->sys->d3d_dev.dev;

    LPDIRECT3DVERTEXBUFFER9 d3dvtc = sys->d3dvtc;
    LPDIRECT3DTEXTURE9      d3dtex = region->texture;

    HRESULT hr;

    /* Import vertices */
    void *vertex;
    hr = IDirect3DVertexBuffer9_Lock(d3dvtc, 0, 0, &vertex, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DVertexBuffer9_Lock: 0x%0lx", hr);
        return -1;
    }
    memcpy(vertex, region->vertex, sizeof(region->vertex));
    hr = IDirect3DVertexBuffer9_Unlock(d3dvtc);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DVertexBuffer9_Unlock: 0x%0lx", hr);
        return -1;
    }

    // Setup our texture. Using textures introduces the texture stage states,
    // which govern how textures get blended together (in the case of multiple
    // textures) and lighting information. In this case, we are modulating
    // (blending) our texture with the diffuse color of the vertices.
    hr = IDirect3DDevice9_SetTexture(d3ddev, 0, (LPDIRECT3DBASETEXTURE9)d3dtex);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_SetTexture: 0x%0lx", hr);
        return -1;
    }

    if (sys->d3dx_shader) {
        if (use_pixel_shader)
        {
            hr = IDirect3DDevice9_SetPixelShader(d3ddev, sys->d3dx_shader);
            float shader_data[4] = { region->width, region->height, 0, 0 };
            hr = IDirect3DDevice9_SetPixelShaderConstantF(d3ddev, 0, shader_data, 1);
            if (FAILED(hr)) {
                msg_Dbg(vd, "Failed IDirect3DDevice9_SetPixelShaderConstantF: 0x%0lx", hr);
                return -1;
            }
        }
        else /* Disable any existing pixel shader. */
            hr = IDirect3DDevice9_SetPixelShader(d3ddev, NULL);
        if (FAILED(hr)) {
            msg_Dbg(vd, "Failed IDirect3DDevice9_SetPixelShader: 0x%0lx", hr);
            return -1;
        }
    }

    // Render the vertex buffer contents
    hr = IDirect3DDevice9_SetStreamSource(d3ddev, 0, d3dvtc, 0, sizeof(CUSTOMVERTEX));
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_SetStreamSource: 0x%0lx", hr);
        return -1;
    }

    // we use FVF instead of vertex shader
    hr = IDirect3DDevice9_SetFVF(d3ddev, D3DFVF_CUSTOMVERTEX);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_SetFVF: 0x%0lx", hr);
        return -1;
    }

    // draw rectangle
    hr = IDirect3DDevice9_DrawPrimitive(d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_DrawPrimitive: 0x%0lx", hr);
        return -1;
    }
    return 0;
}

/**
 * It renders the scene.
 *
 * This function is intented for higher end 3D cards, with pixel shader support
 * and at least 64 MiB of video RAM.
 */
static void Direct3D9RenderScene(vout_display_t *vd,
                                d3d_region_t *picture,
                                int subpicture_count,
                                d3d_region_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9 d3ddev = sys->d3d_dev.dev;
    HRESULT hr;

    if (sys->clear_scene) {
        /* Clear the backbuffer and the zbuffer */
        hr = IDirect3DDevice9_Clear(d3ddev, 0, NULL, D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (FAILED(hr)) {
            msg_Dbg(vd, "Failed IDirect3DDevice9_Clear: 0x%0lx", hr);
            return;
        }
        sys->clear_scene = false;
    }

    // Begin the scene
    hr = IDirect3DDevice9_BeginScene(d3ddev);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_BeginScene: 0x%0lx", hr);
        return;
    }

    Direct3D9RenderRegion(vd, picture, true);

    if (subpicture_count > 0)
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
    for (int i = 0; i < subpicture_count; i++) {
        d3d_region_t *r = &subpicture[i];
        if (r->texture)
            Direct3D9RenderRegion(vd, r, false);
    }
    if (subpicture_count > 0)
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);

    // End the scene
    hr = IDirect3DDevice9_EndScene(d3ddev);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DDevice9_EndScene: 0x%0lx", hr);
        return;
    }
}

/*****************************************************************************
 * DesktopCallback: desktop mode variable callback
 *****************************************************************************/
static int DesktopCallback(vlc_object_t *object, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);

    vlc_mutex_lock(&sys->lock);
    const bool ch_desktop = !sys->desktop_requested != !newval.b_bool;
    sys->ch_desktop |= ch_desktop;
    sys->desktop_requested = newval.b_bool;
    vlc_mutex_unlock(&sys->lock);
    return VLC_SUCCESS;
}

typedef struct
{
    char **values;
    char **descs;
    size_t count;
} enum_context_t;

static void ListShaders(enum_context_t *ctx)
{
    size_t num_shaders = BUILTIN_SHADERS_COUNT;
    ctx->values = xrealloc(ctx->values, (ctx->count + num_shaders + 1) * sizeof(char *));
    ctx->descs = xrealloc(ctx->descs, (ctx->count + num_shaders + 1) * sizeof(char *));
    for (size_t i = 0; i < num_shaders; ++i) {
        ctx->values[ctx->count] = strdup(builtin_shaders[i].name);
        ctx->descs[ctx->count] = strdup(builtin_shaders[i].name);
        ctx->count++;
    }
    ctx->values[ctx->count] = strdup(SELECTED_SHADER_FILE);
    ctx->descs[ctx->count] = strdup(SELECTED_SHADER_FILE);
    ctx->count++;
}

/* Populate the list of available shader techniques in the options */
static int FindShadersCallback(vlc_object_t *object, const char *name,
                               char ***values, char ***descs)
{
    VLC_UNUSED(object);
    VLC_UNUSED(name);

    enum_context_t ctx = { NULL, NULL, 0 };

    ListShaders(&ctx);

    *values = ctx.values;
    *descs = ctx.descs;
    return ctx.count;

}

#ifdef HAVE_GL
#include "../opengl/converter.h"
#include <GL/wglew.h>

struct wgl_vt {
    PFNWGLDXSETRESOURCESHAREHANDLENVPROC DXSetResourceShareHandleNV;
    PFNWGLDXOPENDEVICENVPROC             DXOpenDeviceNV;
    PFNWGLDXCLOSEDEVICENVPROC            DXCloseDeviceNV;
    PFNWGLDXREGISTEROBJECTNVPROC         DXRegisterObjectNV;
    PFNWGLDXUNREGISTEROBJECTNVPROC       DXUnregisterObjectNV;
    PFNWGLDXLOCKOBJECTSNVPROC            DXLockObjectsNV;
    PFNWGLDXUNLOCKOBJECTSNVPROC          DXUnlockObjectsNV;
};
struct glpriv
{
    struct wgl_vt vt;
    d3d9_handle_t hd3d;
    d3d9_device_t d3d_dev;
    HANDLE gl_handle_d3d;
    HANDLE gl_render;
    IDirect3DSurface9 *dx_render;
};

static int
GLConvUpdate(const opengl_tex_converter_t *tc, GLuint *textures,
             const GLsizei *tex_width, const GLsizei *tex_height,
             picture_t *pic, const size_t *plane_offset)
{
    VLC_UNUSED(textures); VLC_UNUSED(tex_width); VLC_UNUSED(tex_height); VLC_UNUSED(plane_offset);
    struct glpriv *priv = tc->priv;
    HRESULT hr;

    picture_sys_t *picsys = ActivePictureSys(pic);
    if (unlikely(!picsys || !priv->gl_render))
        return VLC_EGENERIC;

    if (!priv->vt.DXUnlockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(tc->gl, "DXUnlockObjectsNV failed");
        return VLC_EGENERIC;
    }

    const RECT rect = {
        .left = 0,
        .top = 0,
        .right = pic->format.i_visible_width,
        .bottom = pic->format.i_visible_height
    };
    hr = IDirect3DDevice9Ex_StretchRect(priv->d3d_dev.devex, picsys->surface,
                                        &rect, priv->dx_render, NULL, D3DTEXF_NONE);
    if (FAILED(hr))
    {
        msg_Warn(tc->gl, "IDirect3DDevice9Ex_StretchRect failed");
        return VLC_EGENERIC;
    }

    if (!priv->vt.DXLockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(tc->gl, "DXLockObjectsNV failed");
        priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        priv->gl_render = NULL;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static picture_pool_t *
GLConvGetPool(const opengl_tex_converter_t *tc, unsigned requested_count)
{
    struct glpriv *priv = tc->priv;
    return Direct3D9CreatePicturePool(VLC_OBJECT(tc->gl), &priv->d3d_dev, NULL,
                                      &tc->fmt, requested_count);
}

static int
GLConvAllocateTextures(const opengl_tex_converter_t *tc, GLuint *textures,
                       const GLsizei *tex_width, const GLsizei *tex_height)
{
    VLC_UNUSED(tex_width); VLC_UNUSED(tex_height);
    struct glpriv *priv = tc->priv;

    priv->gl_render =
        priv->vt.DXRegisterObjectNV(priv->gl_handle_d3d, priv->dx_render,
                                    textures[0], GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
    if (!priv->gl_render)
    {
        msg_Warn(tc->gl, "DXRegisterObjectNV failed: %lu", GetLastError());
        return VLC_EGENERIC;
    }

    if (!priv->vt.DXLockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(tc->gl, "DXLockObjectsNV failed");
        priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        priv->gl_render = NULL;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void
GLConvClose(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *)obj;
    struct glpriv *priv = tc->priv;

    if (priv->gl_handle_d3d)
    {
        if (priv->gl_render)
        {
            priv->vt.DXUnlockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render);
            priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        }

        priv->vt.DXCloseDeviceNV(priv->gl_handle_d3d);
    }

    if (priv->dx_render)
        IDirect3DSurface9_Release(priv->dx_render);

    D3D9_ReleaseDevice(&priv->d3d_dev);
    D3D9_Destroy(&priv->hd3d);
    free(tc->priv);
}

static int
GLConvOpen(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *) obj;

    if (tc->fmt.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && tc->fmt.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;

    if (tc->gl->ext != VLC_GL_EXT_WGL || !tc->gl->wgl.getExtensionsString)
        return VLC_EGENERIC;

    const char *wglExt = tc->gl->wgl.getExtensionsString(tc->gl);

    if (wglExt == NULL || !HasExtension(wglExt, "WGL_NV_DX_interop"))
        return VLC_EGENERIC;

    struct wgl_vt vt;
#define LOAD_EXT(name, type) do { \
    vt.name = (type) vlc_gl_GetProcAddress(tc->gl, "wgl" #name); \
    if (!vt.name) { \
        msg_Warn(obj, "'wgl " #name "' could not be loaded"); \
        return VLC_EGENERIC; \
    } \
} while(0)

    LOAD_EXT(DXSetResourceShareHandleNV, PFNWGLDXSETRESOURCESHAREHANDLENVPROC);
    LOAD_EXT(DXOpenDeviceNV, PFNWGLDXOPENDEVICENVPROC);
    LOAD_EXT(DXCloseDeviceNV, PFNWGLDXCLOSEDEVICENVPROC);
    LOAD_EXT(DXRegisterObjectNV, PFNWGLDXREGISTEROBJECTNVPROC);
    LOAD_EXT(DXUnregisterObjectNV, PFNWGLDXUNREGISTEROBJECTNVPROC);
    LOAD_EXT(DXLockObjectsNV, PFNWGLDXLOCKOBJECTSNVPROC);
    LOAD_EXT(DXUnlockObjectsNV, PFNWGLDXUNLOCKOBJECTSNVPROC);

    struct glpriv *priv = calloc(1, sizeof(struct glpriv));
    if (!priv)
        return VLC_ENOMEM;
    tc->priv = priv;
    priv->vt = vt;

    if (D3D9_Create(obj, &priv->hd3d) != VLC_SUCCESS)
        goto error;

    if (!priv->hd3d.use_ex)
    {
        msg_Warn(obj, "DX/GL interrop only working on d3d9x");
        goto error;
    }

    if (FAILED(D3D9_CreateDevice(obj, &priv->hd3d, tc->gl->surface->handle.hwnd,
                                 &tc->fmt, &priv->d3d_dev)))
        goto error;

    HRESULT hr;
    HANDLE shared_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(priv->d3d_dev.devex,
                                               tc->fmt.i_visible_width,
                                               tc->fmt.i_visible_height,
                                               D3DFMT_X8R8G8B8,
                                               D3DMULTISAMPLE_NONE, 0, FALSE,
                                               &priv->dx_render, &shared_handle);
    if (FAILED(hr))
    {
        msg_Warn(obj, "IDirect3DDevice9_CreateOffscreenPlainSurface failed");
        goto error;
    }

   if (shared_handle)
        priv->vt.DXSetResourceShareHandleNV(priv->dx_render, shared_handle);

    priv->gl_handle_d3d = priv->vt.DXOpenDeviceNV(priv->d3d_dev.dev);
    if (!priv->gl_handle_d3d)
    {
        msg_Warn(obj, "DXOpenDeviceNV failed: %lu", GetLastError());
        goto error;
    }

    tc->pf_update  = GLConvUpdate;
    tc->pf_get_pool = GLConvGetPool;
    tc->pf_allocate_textures = GLConvAllocateTextures;

    tc->fshader = opengl_fragment_shader_init(tc, GL_TEXTURE_2D, VLC_CODEC_RGB32,
                                              COLOR_SPACE_UNDEF);
    if (tc->fshader == 0)
        goto error;

    return VLC_SUCCESS;

error:
    GLConvClose(obj);
    return VLC_EGENERIC;
}
#endif
