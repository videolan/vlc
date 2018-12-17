/*****************************************************************************
 * directdraw.c: Windows DirectDraw video output
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * This plugin will use YUV overlay if supported, using overlay will result in
 * the best video quality (hardware interpolation when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures. This fallback method also enables us to
 * display video in window mode.
 *
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_charset.h>    /* FromT */

#include <windows.h>
#include <ddraw.h>
#include <commctrl.h>       /* ListView_(Get|Set)* */

#include "common.h"

/* Unicode function "DirectDrawEnumerateExW" has been desactivated
   since in some cases this function fails and the callbacks are not
   called. If the Unicode mode is restored, one should modify the
   prototype of the callbacks and call the FromT conversion function.
*/
#define DIRECTDRAWENUMERATEEX_NAME "DirectDrawEnumerateExA"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define HW_YUV_TEXT N_("Use hardware YUV->RGB conversions")
#define HW_YUV_LONGTEXT N_(\
    "Try to use hardware acceleration for YUV->RGB conversions. " \
    "This option doesn't have any effect when using overlays.")

#define OVERLAY_TEXT N_("Overlay video output")
#define OVERLAY_LONGTEXT N_(\
    "Overlay is the hardware acceleration capability of your video card " \
    "(ability to render video directly). VLC will try to use it by default.")

#define SYSMEM_TEXT N_("Use video buffers in system memory")
#define SYSMEM_LONGTEXT N_(\
    "Create video buffers in system memory instead of video memory. This " \
    "isn't recommended as usually using video memory allows benefiting from " \
    "more hardware acceleration (like rescaling or YUV->RGB conversions). " \
    "This option doesn't have any effect when using overlays.")

#define TRIPLEBUF_TEXT N_("Use triple buffering for overlays")
#define TRIPLEBUF_LONGTEXT N_(\
    "Try to use triple buffering when using YUV overlays. That results in " \
    "much better video quality (no flickering).")

#define DEVICE_TEXT N_("Name of desired display device")
#define DEVICE_LONGTEXT N_("In a multiple monitor configuration, you can " \
    "specify the Windows device name of the display that you want the video " \
    "window to open on. For example, \"\\\\.\\DISPLAY1\" or " \
    "\"\\\\.\\DISPLAY2\".")

#define DX_HELP N_("Recommended video output for Windows XP. " \
    "Incompatible with Vista's Aero interface" )

static int  Open (vout_display_t *, const vout_display_cfg_t *,
                  video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

static int FindDevicesCallback(const char *, char ***, char ***);

vlc_module_begin()
    set_shortname("DirectDraw")
    set_description(N_("DirectX (DirectDraw) video output"))
    set_help(DX_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_bool("directx-hw-yuv", true, HW_YUV_TEXT, HW_YUV_LONGTEXT,
              true)
    add_bool("directx-overlay", true, OVERLAY_TEXT, OVERLAY_LONGTEXT, false)
    add_bool("directx-use-sysmem", false, SYSMEM_TEXT, SYSMEM_LONGTEXT,
              true)
    add_bool("directx-3buffering", true, TRIPLEBUF_TEXT,
              TRIPLEBUF_LONGTEXT, true)
    add_string("directx-device", "", DEVICE_TEXT, DEVICE_LONGTEXT, true)
        change_string_cb(FindDevicesCallback)

    set_capability("vout display", 230)
    add_shortcut("directx", "directdraw")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

typedef struct
{
    LPDIRECTDRAWSURFACE2 surface;
    LPDIRECTDRAWSURFACE2 front_surface;
    picture_t            *fallback;
} picture_sys_t;

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;
    /* Multi-monitor support */
    HMONITOR             hmonitor;          /* handle of the current monitor */
    GUID                 *display_driver;

    bool   use_wallpaper;   /* show as desktop wallpaper ? */

    bool   restore_overlay;

    /* DDraw capabilities */
    bool            can_blit_fourcc;

    uint32_t        i_rgb_colorkey;      /* colorkey in RGB used by the overlay */
    uint32_t        i_colorkey;                 /* colorkey used by the overlay */

    COLORREF        color_bkg;
    COLORREF        color_bkgtxt;

    LPDIRECTDRAW2        ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE2 display;                        /* Display device */
    LPDIRECTDRAWCLIPPER  clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */

    picture_sys_t        *picsys;

    /* It protects the following variables */
    vlc_mutex_t    lock;
    bool           ch_wallpaper;
    bool           wallpaper_requested;
};

static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

/* */
static int WallpaperCallback(vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void *);

static int  DirectXOpen(vout_display_t *, video_format_t *fmt);
static void DirectXClose(vout_display_t *);

static int  DirectXLock(picture_t *);
static void DirectXUnlock(picture_t *);

static int DirectXUpdateOverlay(vout_display_t *, LPDIRECTDRAWSURFACE2 surface);

static void WallpaperChange(vout_display_t *vd, bool use_wallpaper);

/** This function allocates and initialize the DirectX vout display.
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* Load direct draw DLL */
    sys->hddraw_dll = LoadLibrary(_T("DDRAW.DLL"));
    if (!sys->hddraw_dll) {
        msg_Warn(vd, "DirectXInitDDraw failed loading ddraw.dll");
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    sys->use_wallpaper = var_CreateGetBool(vd, "video-wallpaper");
    /* FIXME */
    sys->sys.use_overlay = false;//var_CreateGetBool(vd, "overlay"); /* FIXME */
    sys->restore_overlay = false;
    var_Create(vd, "directx-device", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

    /* Initialisation */
    if (CommonInit(vd, false, cfg))
        goto error;

    /* */
    video_format_t fmt = *fmtp;

    if (DirectXOpen(vd, &fmt))
        goto error;

    /* */
    vout_display_info_t info = vd->info;
    info.is_slow = true;
    info.has_double_click = true;
    info.has_pictures_invalid = true;

    /* Interaction TODO support starting with wallpaper mode */
    vlc_mutex_init(&sys->lock);
    sys->ch_wallpaper = sys->use_wallpaper;
    sys->wallpaper_requested = sys->use_wallpaper;
    sys->use_wallpaper = false;

    var_Change(vd, "video-wallpaper", VLC_VAR_SETTEXT, _("Wallpaper"));
    var_AddCallback(vd, "video-wallpaper", WallpaperCallback, NULL);

    /* Setup vout_display now that everything is fine */
    video_format_Clean(fmtp);
    video_format_Copy(fmtp, &fmt);
    vd->info    = info;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    return VLC_SUCCESS;

error:
    DirectXClose(vd);
    CommonClean(vd);
    if (sys->hddraw_dll)
        FreeLibrary(sys->hddraw_dll);
    free(sys);
    return VLC_EGENERIC;
}

/** Terminate a vout display created by Open.
 */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    var_DelCallback(vd, "video-wallpaper", WallpaperCallback, NULL);
    vlc_mutex_destroy(&sys->lock);

    /* Make sure the wallpaper is restored */
    WallpaperChange(vd, false);

    DirectXClose(vd);

    CommonClean(vd);

    if (sys->hddraw_dll)
        FreeLibrary(sys->hddraw_dll);
    free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->sys.pool;
}
static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *p_sys = picture->p_sys;

    assert(sys->display);

    /* Our surface can be lost so be sure to check this
     * and restore it if need be */
    if (IDirectDrawSurface2_IsLost(sys->display) == DDERR_SURFACELOST) {
        if (IDirectDrawSurface2_Restore(sys->display) == DD_OK) {
            if (sys->sys.use_overlay)
                DirectXUpdateOverlay(vd, NULL);
        }
    }
    if (sys->restore_overlay)
        DirectXUpdateOverlay(vd, NULL);

    /* */
    DirectXUnlock(picture);

    if (sys->sys.use_overlay) {
        /* Flip the overlay buffers if we are using back buffers */
        if (p_sys->surface != p_sys->front_surface) {
            HRESULT hr = IDirectDrawSurface2_Flip(p_sys->front_surface,
                                                  NULL, DDFLIP_WAIT);
            if (hr != DD_OK)
                msg_Warn(vd, "could not flip overlay (error %li)", hr);
        }
    } else {
        /* Blit video surface to display with the NOTEARING option */
        DDBLTFX  ddbltfx;
        ZeroMemory(&ddbltfx, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

        HRESULT hr = IDirectDrawSurface2_Blt(sys->display,
                                             &sys->sys.rect_dest_clipped,
                                             p_sys->surface,
                                             &sys->sys.rect_src_clipped,
                                             DDBLT_ASYNC, &ddbltfx);
        if (hr != DD_OK)
            msg_Warn(vd, "could not blit surface (error %li)", hr);
    }
    DirectXLock(picture);

    if (sys->sys.is_first_display) {
        IDirectDraw_WaitForVerticalBlank(sys->ddobject,
                                         DDWAITVB_BLOCKBEGIN, NULL);
        if (sys->sys.use_overlay) {
            HBRUSH brush = CreateSolidBrush(sys->i_rgb_colorkey);
            /* set the colorkey as the backgound brush for the video window */
            SetClassLongPtr(sys->sys.hvideownd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
        }
    }
    CommonDisplay(vd);

    Manage(vd);
}
static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
        video_format_t *fmt = va_arg(args, video_format_t *);
        DirectXClose(vd);
        /* Make sure the wallpaper is restored */
        if (sys->use_wallpaper) {

            vlc_mutex_lock(&sys->lock);
            if (!sys->ch_wallpaper) {
                sys->ch_wallpaper = true;
                sys->wallpaper_requested = true;
            }
            vlc_mutex_unlock(&sys->lock);

            WallpaperChange(vd, false);
        }
        (void) cfg;
        return DirectXOpen(vd, fmt);
    }
    default:
        return CommonControl(vd, query, args);
    }
}
static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    if (sys->sys.changes & DX_POSITION_CHANGE) {
        /* Update overlay */
        if (sys->sys.use_overlay)
            DirectXUpdateOverlay(vd, NULL);

        /* Check if we are still on the same monitor */
        HMONITOR hmon = MonitorFromWindow(sys->sys.hwnd, MONITOR_DEFAULTTONEAREST);
        if (sys->hmonitor != hmon) {
            vout_display_SendEventPicturesInvalid(vd);
        }
        /* */
        sys->sys.changes &= ~DX_POSITION_CHANGE;
    }

    /* Wallpaper mode change */
    vlc_mutex_lock(&sys->lock);
    const bool ch_wallpaper = sys->ch_wallpaper;
    const bool wallpaper_requested = sys->wallpaper_requested;
    sys->ch_wallpaper = false;
    vlc_mutex_unlock(&sys->lock);

    if (ch_wallpaper)
        WallpaperChange(vd, wallpaper_requested);

    /* */
    if (sys->restore_overlay)
        DirectXUpdateOverlay(vd, NULL);
}

/* */
static int  DirectXOpenDDraw(vout_display_t *);
static void DirectXCloseDDraw(vout_display_t *);

static int  DirectXOpenDisplay(vout_display_t *vd);
static void DirectXCloseDisplay(vout_display_t *vd);

static int  DirectXCreatePool(vout_display_t *, bool *, video_format_t *);
static void DirectXDestroyPool(vout_display_t *);

static int DirectXOpen(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    assert(!sys->ddobject);
    assert(!sys->display);
    assert(!sys->clipper);

    /* Initialise DirectDraw */
    if (DirectXOpenDDraw(vd)) {
        msg_Err(vd, "cannot initialize DirectX DirectDraw");
        return VLC_EGENERIC;
    }

    /* Create the directx display */
    if (DirectXOpenDisplay(vd)) {
        msg_Err(vd, "cannot initialize DirectX DirectDraw");
        return VLC_EGENERIC;
    }
    UpdateRects(vd, true);

    /* Create the picture pool */
    if (DirectXCreatePool(vd, &sys->sys.use_overlay, fmt)) {
        msg_Err(vd, "cannot create any DirectX surface");
        return VLC_EGENERIC;
    }

    /* */
    if (sys->sys.use_overlay)
        DirectXUpdateOverlay(vd, NULL);
    if (!sys->sys.b_windowless)
    {
        EventThreadUseOverlay(sys->sys.event, sys->sys.use_overlay);

        /* Change the window title bar text */
        const char *fallback;
        if (sys->sys.use_overlay)
            fallback = VOUT_TITLE " (hardware YUV overlay DirectX output)";
        else if (vlc_fourcc_IsYUV(fmt->i_chroma))
            fallback = VOUT_TITLE " (hardware YUV DirectX output)";
        else
            fallback = VOUT_TITLE " (software RGB DirectX output)";
        EventThreadUpdateTitle(sys->sys.event, fallback);
    }

    return VLC_SUCCESS;
}
static void DirectXClose(vout_display_t *vd)
{
    DirectXDestroyPool(vd);
    DirectXCloseDisplay(vd);
    DirectXCloseDDraw(vd);
}

/* */
static BOOL WINAPI DirectXOpenDDrawCallback(GUID *guid, LPSTR desc,
                                            LPSTR drivername, VOID *context,
                                            HMONITOR hmon)
{
    vout_display_t *vd = context;
    vout_display_sys_t *sys = vd->sys;

    /* This callback function is called by DirectDraw once for each
     * available DirectDraw device.
     *
     * Returning TRUE keeps enumerating.
     */
    if (!hmon)
        return TRUE;

    char *psz_drivername = drivername;
    char *psz_desc = desc;

    msg_Dbg(vd, "DirectXEnumCallback: %s, %s", psz_desc, psz_drivername);

    char *device = var_GetString(vd, "directx-device");

    /* Check for forced device */
    if (device && *device && !strcmp(psz_drivername, device)) {
        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof(MONITORINFO);

        if (GetMonitorInfoA(hmon, &monitor_info)) {
            RECT rect;

            /* Move window to the right screen */
            GetWindowRect(sys->sys.hwnd, &rect);
            if (!IntersectRect(&rect, &rect, &monitor_info.rcWork)) {
                rect.left = monitor_info.rcWork.left;
                rect.top = monitor_info.rcWork.top;
                msg_Dbg(vd, "DirectXEnumCallback: setting window "
                            "position to %ld,%ld", rect.left, rect.top);
                SetWindowPos(sys->sys.hwnd, NULL,
                             rect.left, rect.top, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        sys->hmonitor = hmon;
    }
    free(device);

    if (hmon == sys->hmonitor) {
        msg_Dbg(vd, "selecting %s, %s", psz_desc, psz_drivername);

        free(sys->display_driver);
        sys->display_driver = malloc(sizeof(*guid));
        if (sys->display_driver)
            *sys->display_driver = *guid;
    }

    return TRUE;
}
/**
 * Probe the capabilities of the hardware
 *
 * It is nice to know which features are supported by the hardware so we can
 * find ways to optimize our rendering.
 */
static void DirectXGetDDrawCaps(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* This is just an indication of whether or not we'll support overlay,
     * but with this test we don't know if we support YUV overlay */
    DDCAPS ddcaps;
    ZeroMemory(&ddcaps, sizeof(ddcaps));
    ddcaps.dwSize = sizeof(ddcaps);
    HRESULT hr = IDirectDraw2_GetCaps(sys->ddobject, &ddcaps, NULL);
    if (hr != DD_OK) {
        msg_Warn(vd, "cannot get caps");
        return;
    }

    /* Determine if the hardware supports overlay surfaces */
    const bool has_overlay = ddcaps.dwCaps & DDCAPS_OVERLAY;
    /* Determine if the hardware supports overlay surfaces */
    const bool has_overlay_fourcc = ddcaps.dwCaps & DDCAPS_OVERLAYFOURCC;
    /* Determine if the hardware supports overlay deinterlacing */
    const bool can_deinterlace = ddcaps.dwCaps & DDCAPS2_CANFLIPODDEVEN;
    /* Determine if the hardware supports colorkeying */
    const bool has_color_key = ddcaps.dwCaps & DDCAPS_COLORKEY;
    /* Determine if the hardware supports scaling of the overlay surface */
    const bool can_stretch = ddcaps.dwCaps & DDCAPS_OVERLAYSTRETCH;
    /* Determine if the hardware supports color conversion during a blit */
    sys->can_blit_fourcc = ddcaps.dwCaps & DDCAPS_BLTFOURCC;
    /* Determine overlay source boundary alignment */
    const bool align_boundary_src  = ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC;
    /* Determine overlay destination boundary alignment */
    const bool align_boundary_dest = ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST;
    /* Determine overlay destination size alignment */
    const bool align_size_src  = ddcaps.dwCaps & DDCAPS_ALIGNSIZESRC;
    /* Determine overlay destination size alignment */
    const bool align_size_dest = ddcaps.dwCaps & DDCAPS_ALIGNSIZEDEST;

    msg_Dbg(vd, "DirectDraw Capabilities: overlay=%i yuvoverlay=%i "
                "can_deinterlace_overlay=%i colorkey=%i stretch=%i "
                "bltfourcc=%i",
                has_overlay, has_overlay_fourcc, can_deinterlace,
                has_color_key, can_stretch, sys->can_blit_fourcc);

    if (align_boundary_src || align_boundary_dest || align_size_src || align_size_dest) {
        if (align_boundary_src)
            vd->sys->sys.i_align_src_boundary = ddcaps.dwAlignBoundarySrc;
        if (align_boundary_dest)
            vd->sys->sys.i_align_dest_boundary = ddcaps.dwAlignBoundaryDest;
        if (align_size_src)
            vd->sys->sys.i_align_src_size = ddcaps.dwAlignSizeSrc;
        if (align_size_dest)
            vd->sys->sys.i_align_dest_size = ddcaps.dwAlignSizeDest;

        msg_Dbg(vd,
                "align_boundary_src=%i,%i align_boundary_dest=%i,%i "
                "align_size_src=%i,%i align_size_dest=%i,%i",
                align_boundary_src,  vd->sys->sys.i_align_src_boundary,
                align_boundary_dest, vd->sys->sys.i_align_dest_boundary,
                align_size_src,  vd->sys->sys.i_align_src_size,
                align_size_dest, vd->sys->sys.i_align_dest_size);
    }
}



/* */
static int DirectXOpenDDraw(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    /* */
    HRESULT (WINAPI *OurDirectDrawCreate)(GUID *,LPDIRECTDRAW *,IUnknown *);
    OurDirectDrawCreate =
        (void *)GetProcAddress(sys->hddraw_dll, "DirectDrawCreate");
    if (!OurDirectDrawCreate) {
        msg_Err(vd, "DirectXInitDDraw failed GetProcAddress");
        return VLC_EGENERIC;
    }

    /* */
    HRESULT (WINAPI *OurDirectDrawEnumerateEx)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    OurDirectDrawEnumerateEx =
      (void *)GetProcAddress(sys->hddraw_dll, DIRECTDRAWENUMERATEEX_NAME);

    if (OurDirectDrawEnumerateEx) {
        char *device = var_GetString(vd, "directx-device");
        if (device) {
            msg_Dbg(vd, "directx-device: %s", device);
            free(device);
        }

        sys->hmonitor = MonitorFromWindow(sys->sys.hwnd, MONITOR_DEFAULTTONEAREST);

        /* Enumerate displays */
        OurDirectDrawEnumerateEx(DirectXOpenDDrawCallback,
                                 vd, DDENUM_ATTACHEDSECONDARYDEVICES);
    }

    /* Initialize DirectDraw now */
    LPDIRECTDRAW ddobject;
    hr = OurDirectDrawCreate(sys->display_driver, &ddobject, NULL);
    if (hr != DD_OK) {
        msg_Err(vd, "DirectXInitDDraw cannot initialize DDraw");
        return VLC_EGENERIC;
    }

    /* Get the IDirectDraw2 interface */
    void *ptr;
    hr = IDirectDraw_QueryInterface(ddobject, &IID_IDirectDraw2,
                                    &ptr);
    /* Release the unused interface */
    IDirectDraw_Release(ddobject);

    if (hr != DD_OK) {
        msg_Err(vd, "cannot get IDirectDraw2 interface");
        sys->ddobject = NULL;
        return VLC_EGENERIC;
    }
    sys->ddobject = ptr;

    /* Set DirectDraw Cooperative level, ie what control we want over Windows
     * display */
    hr = IDirectDraw2_SetCooperativeLevel(sys->ddobject, NULL, DDSCL_NORMAL);
    if (hr != DD_OK) {
        msg_Err(vd, "cannot set direct draw cooperative level");
        return VLC_EGENERIC;
    }

    /* Get the size of the current display device */
    if (sys->hmonitor) {
        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoA(vd->sys->hmonitor, &monitor_info);
        sys->sys.rect_display = monitor_info.rcMonitor;
    } else {
        sys->sys.rect_display.left   = 0;
        sys->sys.rect_display.top    = 0;
        sys->sys.rect_display.right  = GetSystemMetrics(SM_CXSCREEN);
        sys->sys.rect_display.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    msg_Dbg(vd, "screen dimensions (%lix%li,%lix%li)",
            sys->sys.rect_display.left,
            sys->sys.rect_display.top,
            sys->sys.rect_display.right,
            sys->sys.rect_display.bottom);

    /* Probe the capabilities of the hardware */
    DirectXGetDDrawCaps(vd);

    return VLC_SUCCESS;
}

static void DirectXCloseDDraw(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    if (sys->ddobject)
        IDirectDraw2_Release(sys->ddobject);

    sys->ddobject = NULL;

    free(sys->display_driver);
    sys->display_driver = NULL;

    sys->hmonitor = NULL;
}

/**
 * Create a clipper that will be used when blitting the RGB surface to the main display.
 *
 * This clipper prevents us to modify by mistake anything on the screen
 * which doesn't belong to our window. For example when a part of our video
 * window is hidden by another window.
 */
static void DirectXCreateClipper(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    /* Create the clipper */
    hr = IDirectDraw2_CreateClipper(sys->ddobject, 0, &sys->clipper, NULL);
    if (hr != DD_OK) {
        msg_Warn(vd, "cannot create clipper (error %li)", hr);
        goto error;
    }

    /* Associate the clipper to the window */
    hr = IDirectDrawClipper_SetHWnd(sys->clipper, 0, sys->sys.hvideownd);
    if (hr != DD_OK) {
        msg_Warn(vd, "cannot attach clipper to window (error %li)", hr);
        goto error;
    }

    /* associate the clipper with the surface */
    hr = IDirectDrawSurface_SetClipper(sys->display, sys->clipper);
    if (hr != DD_OK)
    {
        msg_Warn(vd, "cannot attach clipper to surface (error %li)", hr);
        goto error;
    }

    return;

error:
    if (sys->clipper)
        IDirectDrawClipper_Release(sys->clipper);
    sys->clipper = NULL;
}

/**
 * It finds out the 32bits RGB pixel value of the colorkey.
 */
static uint32_t DirectXFindColorkey(vout_display_t *vd, uint32_t *color)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    /* */
    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);
    hr = IDirectDrawSurface2_Lock(sys->display, NULL, &ddsd, DDLOCK_WAIT, NULL);
    if (hr != DD_OK)
        return 0;

    uint32_t backup = *(uint32_t *)ddsd.lpSurface;

    switch (ddsd.ddpfPixelFormat.dwRGBBitCount) {
    case 4:
        *(uint8_t *)ddsd.lpSurface = *color | (*color << 4);
        break;
    case 8:
        *(uint8_t *)ddsd.lpSurface = *color;
        break;
    case 15:
    case 16:
        *(uint16_t *)ddsd.lpSurface = *color;
        break;
    case 24:
        /* Seems to be problematic so we'll just put black as the colorkey */
        *color = 0;
    default:
        *(uint32_t *)ddsd.lpSurface = *color;
        break;
    }
    IDirectDrawSurface2_Unlock(sys->display, NULL);

    /* */
    HDC hdc;
    COLORREF rgb;
    if (IDirectDrawSurface2_GetDC(sys->display, &hdc) == DD_OK) {
        rgb = GetPixel(hdc, 0, 0);
        IDirectDrawSurface2_ReleaseDC(sys->display, hdc);
    } else {
        rgb = 0;
    }

    /* Restore the pixel value */
    ddsd.dwSize = sizeof(ddsd);
    if (IDirectDrawSurface2_Lock(sys->display, NULL, &ddsd, DDLOCK_WAIT, NULL) == DD_OK) {
        *(uint32_t *)ddsd.lpSurface = backup;
        IDirectDrawSurface2_Unlock(sys->display, NULL);
    }

    return rgb;
}

/**
 * Create and initialize display according to preferences specified in the vout
 * thread fields.
 */
static int DirectXOpenDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    /* Now get the primary surface. This surface is what you actually see
     * on your screen */
    DDSURFACEDESC ddsd;
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    LPDIRECTDRAWSURFACE display;
    hr = IDirectDraw2_CreateSurface(sys->ddobject, &ddsd, &display, NULL);
    if (hr != DD_OK) {
        msg_Err(vd, "cannot get primary surface (error %li)", hr);
        return VLC_EGENERIC;
    }

    void *ptr;
    hr = IDirectDrawSurface_QueryInterface(display, &IID_IDirectDrawSurface2,
                                           &ptr);
    /* Release the old interface */
    IDirectDrawSurface_Release(display);

    if (hr != DD_OK) {
        msg_Err(vd, "cannot query IDirectDrawSurface2 interface (error %li)", hr);
        sys->display = NULL;
        return VLC_EGENERIC;
    }
    sys->display = ptr;

    /* The clipper will be used only in non-overlay mode */
    DirectXCreateClipper(vd);

    /* Make sure the colorkey will be painted */
    sys->i_colorkey = 1;
    sys->i_rgb_colorkey = DirectXFindColorkey(vd, &sys->i_colorkey);

    return VLC_SUCCESS;
}
static void DirectXCloseDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->clipper != NULL)
        IDirectDrawClipper_Release(sys->clipper);

    if (sys->display != NULL)
        IDirectDrawSurface2_Release(sys->display);

    sys->clipper = NULL;
    sys->display = NULL;
}

/**
 * Create an YUV overlay or RGB surface for the video.
 *
 * The best method of display is with an YUV overlay because the YUV->RGB
 * conversion is done in hardware.
 * You can also create a plain RGB surface.
 * (Maybe we could also try an RGB overlay surface, which could have hardware
 * scaling and which would also be faster in window mode because you don't
 * need to do any blitting to the main display...)
 */
static int DirectXCreateSurface(vout_display_t *vd,
                                LPDIRECTDRAWSURFACE2 *surface,
                                const video_format_t *fmt,
                                DWORD fourcc,
                                bool use_overlay,
                                bool use_sysmem,
                                int backbuffer_count)
{
    vout_display_sys_t *sys = vd->sys;

    DDSURFACEDESC ddsd;

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize   = sizeof(ddsd);
    ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
    ddsd.dwFlags  = DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth  = fmt->i_width;
    ddsd.dwHeight = fmt->i_height;
    if (fourcc) {
        ddsd.dwFlags |= DDSD_PIXELFORMAT;
        ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
        ddsd.ddpfPixelFormat.dwFourCC = fourcc;
    }
    ddsd.dwFlags |= DDSD_CAPS;
    if (use_overlay) {
        ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
        if (backbuffer_count > 0) {
            ddsd.ddsCaps.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_FLIP;
            ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
            ddsd.dwBackBufferCount = backbuffer_count;
        }
    } else {
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        if (use_sysmem)
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
        else
            ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
    }

    /* Create the video surface */
    LPDIRECTDRAWSURFACE surface_v1;
    HRESULT hr = IDirectDraw2_CreateSurface(sys->ddobject, &ddsd, &surface_v1, NULL);
    if (hr == DDERR_INVALIDCAPS)
    {
        msg_Dbg(vd, "failed to create a DirectDrawSurface with invalid caps %lx", ddsd.ddsCaps.dwCaps);
        return VLC_EGENERIC;
    }
    if (hr != DD_OK)
    {
        msg_Dbg(vd, "failed to create a DirectDrawSurface (error %li)", hr);
        return VLC_EGENERIC;
    }

    /* Now that the surface is created, try to get a newer DirectX interface */
    hr = IDirectDrawSurface_QueryInterface(surface_v1,
                                           &IID_IDirectDrawSurface2,
                                           (LPVOID *)surface);
    IDirectDrawSurface_Release(surface_v1);
    if (hr != DD_OK) {
        msg_Err(vd, "cannot query IDirectDrawSurface2 interface (error %li)", hr);
        return VLC_EGENERIC;
    }

    if (use_overlay) {
        /* Check the overlay is useable as some graphics cards allow creating
         * several overlays but only one can be used at one time. */
        if (DirectXUpdateOverlay(vd, *surface)) {
            IDirectDrawSurface2_Release(*surface);
            msg_Err(vd, "overlay unuseable (might already be in use)");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void DirectXDestroySurface(LPDIRECTDRAWSURFACE2 surface)
{
    IDirectDrawSurface2_Release(surface);
}
/**
 * This function locks a surface and get the surface descriptor.
 */
static int DirectXLockSurface(LPDIRECTDRAWSURFACE2 front_surface,
                              LPDIRECTDRAWSURFACE2 surface,
                              DDSURFACEDESC *ddsd)
{
    HRESULT hr;

    DDSURFACEDESC ddsd_dummy;
    if (!ddsd)
        ddsd = &ddsd_dummy;

    ZeroMemory(ddsd, sizeof(*ddsd));
    ddsd->dwSize = sizeof(*ddsd);
    hr = IDirectDrawSurface2_Lock(surface, NULL, ddsd, DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL);
    if (hr != DD_OK) {
        if (hr == DDERR_INVALIDPARAMS) {
            /* DirectX 3 doesn't support the DDLOCK_NOSYSLOCK flag, resulting
             * in an invalid params error */
            hr = IDirectDrawSurface2_Lock(surface, NULL, ddsd, DDLOCK_WAIT, NULL);
        }
        if (hr == DDERR_SURFACELOST) {
            /* Your surface can be lost so be sure
             * to check this and restore it if needed */

            /* When using overlays with back-buffers, we need to restore
             * the front buffer so the back-buffers get restored as well. */
            if (front_surface != surface)
                IDirectDrawSurface2_Restore(front_surface);
            else
                IDirectDrawSurface2_Restore(surface);

            hr = IDirectDrawSurface2_Lock(surface, NULL, ddsd, DDLOCK_WAIT, NULL);
        }
        if (hr != DD_OK)
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
static void DirectXUnlockSurface(LPDIRECTDRAWSURFACE2 front_surface,
                                 LPDIRECTDRAWSURFACE2 surface)
{
    VLC_UNUSED(front_surface);
    IDirectDrawSurface2_Unlock(surface, NULL);
}
static int DirectXCheckLockingSurface(LPDIRECTDRAWSURFACE2 front_surface,
                                      LPDIRECTDRAWSURFACE2 surface)
{
    if (DirectXLockSurface(front_surface, surface, NULL))
        return VLC_EGENERIC;

    DirectXUnlockSurface(front_surface, surface);
    return VLC_SUCCESS;
}



typedef struct {
    vlc_fourcc_t codec;
    DWORD        fourcc;
} dx_format_t;

static DWORD DirectXGetFourcc(vlc_fourcc_t codec)
{
    static const dx_format_t dx_formats[] = {
        { VLC_CODEC_YUYV, MAKEFOURCC('Y','U','Y','2') },
        { VLC_CODEC_UYVY, MAKEFOURCC('U','Y','V','Y') },
        { VLC_CODEC_YVYU, MAKEFOURCC('Y','V','Y','U') },
        { VLC_CODEC_YV12, MAKEFOURCC('Y','V','1','2') },
        { VLC_CODEC_I420, MAKEFOURCC('Y','V','1','2') },
        { VLC_CODEC_J420, MAKEFOURCC('Y','V','1','2') },
        { 0, 0 }
    };

    for (unsigned i = 0; dx_formats[i].codec != 0; i++) {
        if (dx_formats[i].codec == codec)
            return dx_formats[i].fourcc;
    }
    return 0;
}

static int DirectXCreatePictureResourceYuvOverlay(vout_display_t *vd,
                                                  const video_format_t *fmt,
                                                  DWORD fourcc)
{
    vout_display_sys_t *sys = vd->sys;

    bool allow_3buf    = var_InheritBool(vd, "directx-3buffering");

    /* The overlay surface that we create won't be used to decode directly
     * into it because accessing video memory directly is way to slow (remember
     * that pictures are decoded macroblock per macroblock). Instead the video
     * will be decoded in picture buffers in system memory which will then be
     * memcpy() to the overlay surface. */
    LPDIRECTDRAWSURFACE2 front_surface;
    int ret = VLC_EGENERIC;
    if (allow_3buf) {
        /* Triple buffering rocks! it doesn't have any processing overhead
         * (you don't have to wait for the vsync) and provides for a very nice
         * video quality (no tearing). */
        ret = DirectXCreateSurface(vd, &front_surface, fmt, fourcc, true, false, 2);
    }
    if (ret)
        ret = DirectXCreateSurface(vd, &front_surface, fmt, fourcc, true, false, 0);
    if (ret)
        return VLC_EGENERIC;
    msg_Dbg(vd, "YUV overlay surface (%4.4s) created successfully", (const char *)&fourcc);

    /* Get the back buffer */
    LPDIRECTDRAWSURFACE2 surface;
    DDSCAPS dds_caps;
    ZeroMemory(&dds_caps, sizeof(dds_caps));
    dds_caps.dwCaps = DDSCAPS_BACKBUFFER;
    if (IDirectDrawSurface2_GetAttachedSurface(front_surface, &dds_caps, &surface) != DD_OK) {
        msg_Warn(vd, "Failed to get surface back buffer");
        /* front buffer is the same as back buffer */
        surface = front_surface;
    }

    if (DirectXCheckLockingSurface(front_surface, surface)) {
        DirectXDestroySurface(front_surface);
        return VLC_EGENERIC;
    }

    /* */
    picture_sys_t *picsys = sys->picsys;
    picsys->front_surface = front_surface;
    picsys->surface       = surface;
    picsys->fallback      = NULL;
    return VLC_SUCCESS;
}
static int DirectXCreatePictureResourceYuv(vout_display_t *vd,
                                           const video_format_t *fmt,
                                           DWORD fourcc)
{
    vout_display_sys_t *sys = vd->sys;

    bool allow_sysmem  = var_InheritBool(vd, "directx-use-sysmem");

    /* As we can't have an overlay, we'll try to create a plain offscreen
     * surface. This surface will reside in video memory because there's a
     * better chance then that we'll be able to use some kind of hardware
     * acceleration like rescaling, blitting or YUV->RGB conversions.
     * We then only need to blit this surface onto the main display when we
     * want to display it */

    /* Check if the chroma is supported first. This is required
     * because a few buggy drivers don't mind creating the surface
     * even if they don't know about the chroma. */
    DWORD count;
    if (IDirectDraw2_GetFourCCCodes(sys->ddobject, &count, NULL) != DD_OK)
        return VLC_EGENERIC;

    DWORD *list = calloc(count, sizeof(*list));
    if (!list)
        return VLC_ENOMEM;
    if (IDirectDraw2_GetFourCCCodes(sys->ddobject, &count, list) != DD_OK) {
        free(list);
        return VLC_EGENERIC;
    }
    unsigned index;
    for (index = 0; index < count; index++) {
        if (list[index] == fourcc)
            break;
    }
    free(list);
    if (index >= count)
        return VLC_EGENERIC;

    /* */
    LPDIRECTDRAWSURFACE2 surface;
    if (DirectXCreateSurface(vd, &surface, fmt, fourcc, false, allow_sysmem, 0))
        return VLC_EGENERIC;
    msg_Dbg(vd, "YUV plain surface (%4.4s) created successfully", (const char *)&fourcc);

    if (DirectXCheckLockingSurface(surface, surface)) {
        DirectXDestroySurface(surface);
        return VLC_EGENERIC;
    }

    /* */
    picture_sys_t *picsys = sys->picsys;
    picsys->front_surface = surface;
    picsys->surface       = surface;
    picsys->fallback      = NULL;
    return VLC_SUCCESS;
}
static int DirectXCreatePictureResourceRgb(vout_display_t *vd,
                                           video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    bool allow_sysmem  = var_InheritBool(vd, "directx-use-sysmem");

    /* Our last choice is to use a plain RGB surface */
    DDPIXELFORMAT ddpfPixelFormat;
    ZeroMemory(&ddpfPixelFormat, sizeof(ddpfPixelFormat));
    ddpfPixelFormat.dwSize = sizeof(ddpfPixelFormat);

    IDirectDrawSurface2_GetPixelFormat(sys->display, &ddpfPixelFormat);
    if ((ddpfPixelFormat.dwFlags & DDPF_RGB) == 0)
        return VLC_EGENERIC;

    switch (ddpfPixelFormat.dwRGBBitCount) {
    case 8:
        fmt->i_chroma = VLC_CODEC_RGB8;
        break;
    case 15:
        fmt->i_chroma = VLC_CODEC_RGB15;
        break;
    case 16:
        fmt->i_chroma = VLC_CODEC_RGB16;
        break;
    case 24:
        fmt->i_chroma = VLC_CODEC_RGB24;
        break;
    case 32:
        fmt->i_chroma = VLC_CODEC_RGB32;
        break;
    default:
        msg_Err(vd, "unknown screen depth");
        return VLC_EGENERIC;
    }
    fmt->i_rmask = ddpfPixelFormat.dwRBitMask;
    fmt->i_gmask = ddpfPixelFormat.dwGBitMask;
    fmt->i_bmask = ddpfPixelFormat.dwBBitMask;

    /* */
    LPDIRECTDRAWSURFACE2 surface;
    int ret = DirectXCreateSurface(vd, &surface, fmt, 0, false, allow_sysmem, 0);
    if (ret && !allow_sysmem)
        ret = DirectXCreateSurface(vd, &surface, fmt, 0, false, true, 0);
    if (ret)
        return VLC_EGENERIC;
    msg_Dbg(vd, "RGB plain surface (%4.4s) created successfully", (const char *)&fmt->i_chroma);

    if (DirectXCheckLockingSurface(surface, surface)) {
        DirectXDestroySurface(surface);
        return VLC_EGENERIC;
    }

    /* */
    picture_sys_t *picsys = sys->picsys;
    picsys->front_surface = surface;
    picsys->surface       = surface;
    picsys->fallback      = NULL;
    return VLC_SUCCESS;
}

static int DirectXCreatePictureResource(vout_display_t *vd,
                                        bool *use_overlay,
                                        video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    picture_sys_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL))
        return VLC_ENOMEM;
    sys->picsys = picsys;

    /* */
    bool allow_hw_yuv  = sys->can_blit_fourcc &&
                         vlc_fourcc_IsYUV(fmt->i_chroma) &&
                         var_InheritBool(vd, "directx-hw-yuv");
    bool allow_overlay = var_InheritBool(vd, "directx-overlay");

    /* Try to use an yuv surface */
    if (allow_hw_yuv) {
        const vlc_fourcc_t *list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
        /*  Try with overlay first */
        for (unsigned pass = allow_overlay ? 0 : 1; pass < 2; pass++) {
            for (unsigned i = 0; list[i] != 0; i++) {
                const DWORD fourcc = DirectXGetFourcc(list[i]);
                if (!fourcc)
                    continue;

                if (pass == 0) {
                    if (DirectXCreatePictureResourceYuvOverlay(vd, fmt, fourcc))
                    {
#ifndef NDEBUG
                        msg_Dbg(vd, "Failed to create YUV overlay surface %4.4s", (const char*)&fourcc);
#endif
                        continue;
                    }
                } else {
                    if (DirectXCreatePictureResourceYuv(vd, fmt, fourcc))
                    {
#ifndef NDEBUG
                        msg_Dbg(vd, "Failed to create YUV surface %4.4s", (const char*)&fourcc);
#endif
                        continue;
                    }
                }
                /* */
                *use_overlay = pass == 0;
                fmt->i_chroma = list[i];
                return VLC_SUCCESS;
            }
        }
    }

    /* Try plain RGB */
    return DirectXCreatePictureResourceRgb(vd, fmt);
}
static void DirectXDestroyPictureResource(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picsys->front_surface != sys->picsys->surface)
        DirectXDestroySurface(sys->picsys->surface);
    DirectXDestroySurface(sys->picsys->front_surface);
    if (sys->picsys->fallback)
        picture_Release(sys->picsys->fallback);
}

static int DirectXLock(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    DDSURFACEDESC ddsd;
    if (DirectXLockSurface(p_sys->front_surface,
                           p_sys->surface, &ddsd))
        return CommonUpdatePicture(picture, &p_sys->fallback, NULL, 0);

    CommonUpdatePicture(picture, NULL, ddsd.lpSurface, ddsd.lPitch);
    return VLC_SUCCESS;
}
static void DirectXUnlock(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    DirectXUnlockSurface(p_sys->front_surface,
                         p_sys->surface);
}
static void DirectXDestroyPicture(picture_t *picture)
{
    free(picture->p_sys);
}

static int DirectXCreatePool(vout_display_t *vd,
                             bool *use_overlay, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    *fmt = vd->source;

    if (DirectXCreatePictureResource(vd, use_overlay, fmt))
        return VLC_EGENERIC;

    /* Create the associated picture */
    picture_resource_t resource = {
        .p_sys = sys->picsys, .pf_destroy = DirectXDestroyPicture,
    };
    picture_t *picture = picture_NewFromResource(fmt, &resource);
    if (!picture) {
        DirectXDestroyPictureResource(vd);
        free(sys->picsys);
        return VLC_ENOMEM;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = 1;
    cfg.picture       = &picture;
    cfg.lock          = DirectXLock;
    cfg.unlock        = DirectXUnlock;

    sys->sys.pool = picture_pool_NewExtended(&cfg);
    if (!sys->sys.pool) {
        picture_Release(picture);
        DirectXDestroyPictureResource(vd);
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}
static void DirectXDestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->sys.pool) {
        DirectXDestroyPictureResource(vd);
        picture_pool_Release(sys->sys.pool);
    }
    sys->sys.pool = NULL;
}

/**
 * Move or resize overlay surface on video display.
 *
 * This function is used to move or resize an overlay surface on the screen.
 * Ususally the overlay is moved by the user and thus, by a move or resize
 * event.
 */
static int DirectXUpdateOverlay(vout_display_t *vd, LPDIRECTDRAWSURFACE2 surface)
{
    vout_display_sys_t *sys = vd->sys;

    RECT src = sys->sys.rect_src_clipped;
    RECT dst = sys->sys.rect_dest_clipped;

    if (sys->use_wallpaper) {
        src.left   = vd->source.i_x_offset;
        src.top    = vd->source.i_y_offset;
        src.right  = vd->source.i_x_offset + vd->source.i_visible_width;
        src.bottom = vd->source.i_y_offset + vd->source.i_visible_height;
        AlignRect(&src, sys->sys.i_align_src_boundary, sys->sys.i_align_src_size);

        vout_display_cfg_t cfg = sys->sys.vdcfg;
        cfg.display.width  = sys->sys.rect_display.right;
        cfg.display.height = sys->sys.rect_display.bottom;

        vout_display_place_t place;
        vout_display_PlacePicture(&place, &vd->source, &cfg, false);

        if (place.x < 0) {
            place.width += -place.x;
            place.x = 0;
        }
        if (place.y < 0) {
            place.height += -place.y;
            place.y = 0;
        }
        if (place.width > cfg.display.width)
            place.width = cfg.display.width;
        if (place.height > cfg.display.height)
            place.height = cfg.display.height;

        dst.left   = sys->sys.rect_display.left + place.x;
        dst.top    = sys->sys.rect_display.top  + place.y;
        dst.right  = dst.left + place.width;
        dst.bottom = dst.top  + place.height;
        AlignRect(&dst, sys->sys.i_align_dest_boundary, sys->sys.i_align_dest_size);
    }

    if (!surface) {
        if (!sys->sys.pool)
            return VLC_EGENERIC;
        surface = sys->picsys->front_surface;
    }

    /* The new window dimensions should already have been computed by the
     * caller of this function */

    /* Position and show the overlay */
    DDOVERLAYFX ddofx;
    ZeroMemory(&ddofx, sizeof(ddofx));
    ddofx.dwSize = sizeof(ddofx);
    ddofx.dckDestColorkey.dwColorSpaceLowValue = sys->i_colorkey;
    ddofx.dckDestColorkey.dwColorSpaceHighValue = sys->i_colorkey;

    HRESULT hr = IDirectDrawSurface2_UpdateOverlay(surface,
                                                   &src, sys->display, &dst,
                                                   DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE | DDOVER_DDFX,
                                                   &ddofx);
    sys->restore_overlay = hr != DD_OK;

    if (hr != DD_OK) {
        msg_Warn(vd, "DirectDrawUpdateOverlay cannot move/resize overlay. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/* */
static void WallpaperChange(vout_display_t *vd, bool use_wallpaper)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->use_wallpaper == !use_wallpaper)
        return;

    HWND hwnd = FindWindow(_T("Progman"), NULL);
    if (hwnd)
        hwnd = FindWindowEx(hwnd, NULL, _T("SHELLDLL_DefView"), NULL);
    if (hwnd)
        hwnd = FindWindowEx(hwnd, NULL, _T("SysListView32"), NULL);
    if (!hwnd) {
        msg_Warn(vd, "couldn't find \"SysListView32\" window, "
                     "wallpaper mode not supported");
        return;
    }

    msg_Dbg(vd, "wallpaper mode %s", use_wallpaper ? "enabled" : "disabled");
    sys->use_wallpaper = use_wallpaper;

    if (sys->use_wallpaper) {
        sys->color_bkg    = ListView_GetBkColor(hwnd);
        sys->color_bkgtxt = ListView_GetTextBkColor(hwnd);

        ListView_SetBkColor(hwnd,     sys->i_rgb_colorkey);
        ListView_SetTextBkColor(hwnd, sys->i_rgb_colorkey);
    } else {
        ListView_SetBkColor(hwnd,     sys->color_bkg);
        ListView_SetTextBkColor(hwnd, sys->color_bkgtxt);
    }

    /* Update desktop */
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);

    if (sys->sys.use_overlay)
      DirectXUpdateOverlay(vd, NULL);
}

/* */
static int WallpaperCallback(vlc_object_t *object, char const *cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);

    vlc_mutex_lock(&sys->lock);
    const bool ch_wallpaper = !sys->wallpaper_requested != !newval.b_bool;
    sys->ch_wallpaper |= ch_wallpaper;
    sys->wallpaper_requested = newval.b_bool;
    vlc_mutex_unlock(&sys->lock);
    return VLC_SUCCESS;
}

typedef struct
{
    char **values;
    char **descs;
    size_t count;
} enum_context_t;

/*****************************************************************************
 * config variable callback
 *****************************************************************************/
static BOOL WINAPI DirectXEnumCallback2(GUID *guid, LPSTR desc,
                                        LPSTR drivername, VOID *data,
                                        HMONITOR hmon)
{
    enum_context_t *ctx = data;

    VLC_UNUSED(guid); VLC_UNUSED(desc); VLC_UNUSED(hmon);

    char *psz_drivername = drivername;
    ctx->values = xrealloc(ctx->values, (ctx->count + 1) * sizeof(char *));
    ctx->descs = xrealloc(ctx->descs, (ctx->count + 1) * sizeof(char *));

    ctx->values[ctx->count] = strdup(psz_drivername);
    ctx->descs[ctx->count] = strdup(psz_drivername);
    ctx->count++;

    return TRUE; /* Keep enumerating */
}

static int FindDevicesCallback(const char *name, char ***values, char ***descs)
{
    enum_context_t ctx;

    ctx.values = xmalloc(sizeof(char *));
    ctx.descs = xmalloc(sizeof(char *));
    ctx.values[0] = strdup("");
    ctx.descs[0] = strdup(_("Default"));
    ctx.count = 1;

    /* Load direct draw DLL */
    HINSTANCE hddraw_dll = LoadLibrary(_T("DDRAW.DLL"));
    if (hddraw_dll != NULL)
    {
        /* Enumerate displays */
        HRESULT (WINAPI *OurDirectDrawEnumerateEx)(LPDDENUMCALLBACKEXA,
                                                   LPVOID, DWORD) =
              (void *)GetProcAddress(hddraw_dll, DIRECTDRAWENUMERATEEX_NAME);
        if (OurDirectDrawEnumerateEx != NULL)
            OurDirectDrawEnumerateEx(DirectXEnumCallback2, &ctx,
                                     DDENUM_ATTACHEDSECONDARYDEVICES);
        FreeLibrary(hddraw_dll);
    }

    VLC_UNUSED(name);

    *values = ctx.values;
    *descs = ctx.descs;
    return ctx.count;
}


