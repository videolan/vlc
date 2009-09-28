/*****************************************************************************
 * vout.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

/*****************************************************************************
 * event_thread_t: event thread
 *****************************************************************************/
#include "events.h"

#ifdef MODULE_NAME_IS_wingapi
    typedef struct GXDisplayProperties {
        DWORD cxWidth;
        DWORD cyHeight;
        long cbxPitch;
        long cbyPitch;
        long cBPP;
        DWORD ffFormat;
    } GXDisplayProperties;

    typedef struct GXScreenRect {
        DWORD dwTop;
        DWORD dwLeft;
        DWORD dwWidth;
        DWORD dwHeight;
    } GXScreenRect;

#   define GX_FULLSCREEN    0x01
#   define GX_NORMALKEYS    0x02
#   define GX_LANDSCAPEKEYS 0x03

#   ifndef kfLandscape
#       define kfLandscape      0x8
#       define kfPalette        0x10
#       define kfDirect         0x20
#       define kfDirect555      0x40
#       define kfDirect565      0x80
#       define kfDirect888      0x100
#       define kfDirect444      0x200
#       define kfDirectInverted 0x400
#   endif

#endif

struct vout_window_t;

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the module specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    HWND                 hwnd;                  /* Handle of the main window */
    HWND                 hvideownd;        /* Handle of the video sub-window */
    struct vout_window_t *parent_window;         /* Parent window VLC object */
    HWND                 hparent;             /* Handle of the parent window */
    HWND                 hfswnd;          /* Handle of the fullscreen window */

    /* Multi-monitor support */
    HMONITOR             hmonitor;          /* handle of the current monitor */
    GUID                 *p_display_driver;
    HMONITOR             (WINAPI* MonitorFromWindow)( HWND, DWORD );
    BOOL                 (WINAPI* GetMonitorInfo)( HMONITOR, LPMONITORINFO );

    /* size of the display */
    RECT         rect_display;
    int          i_display_depth;

    /* size of the overall window (including black bands) */
    RECT         rect_parent;

    /* Window position and size */
    int          i_window_x;
    int          i_window_y;
    int          i_window_width;
    int          i_window_height;
    int          i_window_style;

    volatile uint16_t i_changes;        /* changes made to the video display */

    /* Misc */
    bool      b_on_top_change;

#ifndef UNDER_CE

    /* screensaver system settings to be restored when vout is closed */
    UINT i_spi_lowpowertimeout;
    UINT i_spi_powerofftimeout;
    UINT i_spi_screensavetimeout;

#endif

    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         rect_src;
    RECT         rect_src_clipped;
    RECT         rect_dest;
    RECT         rect_dest_clipped;

    bool   b_hw_yuv;    /* Should we use hardware YUV->RGB conversions */


#ifdef MODULE_NAME_IS_directx
    /* Overlay alignment restrictions */
    int          i_align_src_boundary;
    int          i_align_src_size;
    int          i_align_dest_boundary;
    int          i_align_dest_size;

    bool      b_wallpaper;    /* show as desktop wallpaper ? */

    bool   b_using_overlay;         /* Are we using an overlay surface */
    bool   b_use_sysmem;   /* Should we use system memory for surfaces */
    bool   b_3buf_overlay;   /* Should we use triple buffered overlays */

    /* DDraw capabilities */
    int          b_caps_overlay_clipping;

    unsigned int    i_rgb_colorkey;      /* colorkey in RGB used by the overlay */
    unsigned int    i_colorkey;                 /* colorkey used by the overlay */

    COLORREF        color_bkg;
    COLORREF        color_bkgtxt;

    LPDIRECTDRAW2        p_ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE2 p_display;                        /* Display device */
    LPDIRECTDRAWSURFACE2 p_current_surface;   /* surface currently displayed */
    LPDIRECTDRAWCLIPPER  p_clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */
#endif

#ifdef MODULE_NAME_IS_glwin32
    HDC hGLDC;
    HGLRC hGLRC;
#endif

#ifdef MODULE_NAME_IS_direct3d
    /* show video on desktop window ? */
    bool      b_desktop;

    // core objects
    HINSTANCE               hd3d9_dll;       /* handle of the opened d3d9 dll */
    LPDIRECT3D9             p_d3dobj;
    LPDIRECT3DDEVICE9       p_d3ddev;
    D3DFORMAT               bbFormat;
    // scene objects
    LPDIRECT3DTEXTURE9      p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc;
#endif

#ifdef MODULE_NAME_IS_wingdi

    int  i_depth;

    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;
    uint8_t *  p_pic_buffer;
    int        i_pic_pitch;
    int        i_pic_pixel_pitch;

    BITMAPINFO bitmapinfo;
    RGBQUAD    red;
    RGBQUAD    green;
    RGBQUAD    blue;
#endif

#ifdef MODULE_NAME_IS_wingapi
    int        i_depth;
    int        render_width;
    int        render_height;
	    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;
    uint8_t *  p_pic_buffer;
    int        i_pic_pitch;
    int        i_pic_pixel_pitch;

    BITMAPINFO bitmapinfo;
    RGBQUAD    red;
    RGBQUAD    green;
    RGBQUAD    blue;

    HINSTANCE  gapi_dll;                   /* handle of the opened gapi dll */

    /* GAPI functions */
    int (*GXOpenDisplay)( HWND hWnd, DWORD dwFlags );
    int (*GXCloseDisplay)();
    void *(*GXBeginDraw)();
    int (*GXEndDraw)();
    GXDisplayProperties (*GXGetDisplayProperties)();
    int (*GXSuspend)();
    int (*GXResume)();
#endif

#ifndef UNDER_CE
    /* suspend display */
    bool   b_suspend_display;
#endif

    event_thread_t *p_event;
    vlc_mutex_t    lock;
};

#ifdef MODULE_NAME_IS_wingapi
#   define GXOpenDisplay p_vout->p_sys->GXOpenDisplay
#   define GXCloseDisplay p_vout->p_sys->GXCloseDisplay
#   define GXBeginDraw p_vout->p_sys->GXBeginDraw
#   define GXEndDraw p_vout->p_sys->GXEndDraw
#   define GXGetDisplayProperties p_vout->p_sys->GXGetDisplayProperties
#   define GXSuspend p_vout->p_sys->GXSuspend
#   define GXResume p_vout->p_sys->GXResume
#endif

/*****************************************************************************
 * Prototypes from directx.c
 *****************************************************************************/
int DirectDrawUpdateOverlay( vout_thread_t *p_vout );

/*****************************************************************************
 * Prototypes from common.c
 *****************************************************************************/
int  CommonInit( vout_thread_t * );
void CommonClean( vout_thread_t * );
void CommonManage( vout_thread_t * );

int Control( vout_thread_t *p_vout, int i_query, va_list args );

void UpdateRects ( vout_thread_t *p_vout, bool b_force );
void Win32ToggleFullscreen ( vout_thread_t *p_vout );
void ExitFullscreen( vout_thread_t *p_vout );
#ifndef UNDER_CE
void DisableScreensaver ( vout_thread_t *p_vout );
void RestoreScreensaver ( vout_thread_t *p_vout );
#endif

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define WM_VLC_HIDE_MOUSE WM_APP
#define WM_VLC_SHOW_MOUSE WM_APP + 1
#define WM_VLC_CHANGE_TEXT WM_APP + 2
#define IDM_TOGGLE_ON_TOP WM_USER + 1
#define DX_POSITION_CHANGE 0x1000
#define DX_WALLPAPER_CHANGE 0x2000
#define DX_DESKTOP_CHANGE 0x4000

/*****************************************************************************
 * WinCE helpers
 *****************************************************************************/
#ifdef UNDER_CE

#define AdjustWindowRect(a,b,c)

#ifndef GCL_HBRBACKGROUND
#   define GCL_HBRBACKGROUND (-10)
#endif

//#define FindWindowEx(a,b,c,d) 0

#define GetWindowPlacement(a,b)
#define SetWindowPlacement(a,b)
/*typedef struct _WINDOWPLACEMENT {
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT;*/

#ifndef WM_NCMOUSEMOVE
#   define WM_NCMOUSEMOVE 160
#endif
#ifndef CS_OWNDC
#   define CS_OWNDC 32
#endif
#ifndef SC_SCREENSAVE
#   define SC_SCREENSAVE 0xF140
#endif
#ifndef SC_MONITORPOWER
#   define SC_MONITORPOWER 0xF170
#endif
#ifndef WM_NCPAINT
#   define WM_NCPAINT 133
#endif
#ifndef WS_OVERLAPPEDWINDOW
#   define WS_OVERLAPPEDWINDOW 0xcf0000
#endif
#ifndef WS_EX_NOPARENTNOTIFY
#   define WS_EX_NOPARENTNOTIFY 4
#endif
#ifndef WS_EX_APPWINDOW
#define WS_EX_APPWINDOW 0x40000
#endif

//#define SetWindowLongPtr SetWindowLong
//#define GetWindowLongPtr GetWindowLong
//#define GWLP_USERDATA GWL_USERDATA

#endif //UNDER_CE
