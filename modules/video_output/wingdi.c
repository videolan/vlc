/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include <commctrl.h>

#define SHFS_SHOWSIPBUTTON 0x0004
#define SHFS_HIDESIPBUTTON 0x0008

#if defined(UNDER_CE) && !defined(__PLUGIN__) /*FIXME*/
#   define MENU_HEIGHT 26
    BOOL SHFullScreen(HWND hwndRequester, DWORD dwState);
#else
#   define MENU_HEIGHT 0
#   define SHFullScreen(a,b)
#endif

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
#endif /* MODULE_NAME_IS_wingapi */

#define MAX_DIRECTBUFFERS 10

#ifdef UNDER_CE
#ifndef WS_OVERLAPPEDWINDOW
#   define WS_OVERLAPPEDWINDOW 0xcf0000
#endif
#ifndef WS_EX_NOPARENTNOTIFY
#   define WS_EX_NOPARENTNOTIFY 4
#endif
#ifndef WS_EX_APPWINDOW
#define WS_EX_APPWINDOW 0x40000
#endif
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define GWLP_USERDATA GWL_USERDATA
#endif //UNDER_CE

#ifndef WS_NONAVDONEBUTTON
#define WS_NONAVDONEBUTTON 0
#endif
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenVideo  ( vlc_object_t * );
static void CloseVideo ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
#ifdef MODULE_NAME_IS_wingapi
static void DisplayGAPI( vout_thread_t *, picture_t * );
static int GAPILockSurface( vout_thread_t *, picture_t * );
static int GAPIUnlockSurface( vout_thread_t *, picture_t * );
#else
static void DisplayGDI( vout_thread_t *, picture_t * );
#endif
static void SetPalette( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

static void EventThread        ( vlc_object_t * );
static long FAR PASCAL WndProc ( HWND, UINT, WPARAM, LPARAM );
static void InitBuffers        ( vout_thread_t * );
static void UpdateRects        ( vout_thread_t *, vlc_bool_t );

static int Control( vout_thread_t *p_vout, int i_query, va_list args );

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct vout_sys_t
{
    /* The event thread */
    vlc_object_t * p_event;

    /* Our video output window */
    HWND hwnd;
    HWND hvideownd;
    HWND hfswnd;
    int  i_depth;
    HWND                 hparent;             /* Handle of the parent window */
    WNDPROC              pf_wndproc;             /* Window handling callback */
    volatile uint16_t    i_changes;     /* changes made to the video display */
    RECT window_placement;

    /* Window position and size */
    int          i_window_x;
    int          i_window_y;
    int          i_window_width;
    int          i_window_height;
    int          render_width;
    int          render_height;

    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         rect_src;
    RECT         rect_src_clipped;
    RECT         rect_dest;
    RECT         rect_dest_clipped;
    RECT         rect_parent;
    RECT         rect_display;

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

    /* WINCE stuff */
    vlc_bool_t   b_video_display;

    /* Window focus states */
    vlc_bool_t b_focus;
    vlc_bool_t b_parent_focus;

#ifdef MODULE_NAME_IS_wingapi
    HINSTANCE  gapi_dll;                    /* handle of the opened gapi dll */

    /* GAPI functions */
    int (*GXOpenDisplay)( HWND hWnd, DWORD dwFlags );
    int (*GXCloseDisplay)();
    void *(*GXBeginDraw)();
    int (*GXEndDraw)();
    GXDisplayProperties (*GXGetDisplayProperties)();
    int (*GXSuspend)();
    int (*GXResume)();
#endif
};

#define GXOpenDisplay p_vout->p_sys->GXOpenDisplay
#define GXCloseDisplay p_vout->p_sys->GXCloseDisplay
#define GXBeginDraw p_vout->p_sys->GXBeginDraw
#define GXEndDraw p_vout->p_sys->GXEndDraw
#define GXGetDisplayProperties p_vout->p_sys->GXGetDisplayProperties

#ifdef MODULE_NAME_IS_wingapi
#   define GXSuspend p_vout->p_sys->GXSuspend
#   define GXResume p_vout->p_sys->GXResume
#else
#   define GXSuspend()
#   define GXResume()
#endif

#define DX_POSITION_CHANGE 0x1000

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
#ifdef MODULE_NAME_IS_wingapi
    set_shortname( _("Windows GAPI") );
    set_description( _("Windows GAPI video output") );
    set_capability( "video output", 20 );
#else
    set_shortname( _("Windows GDI") );
    set_description( _("Windows GDI video output") );
    set_capability( "video output", 10 );
#endif
    set_callbacks( OpenVideo, CloseVideo );
vlc_module_end();

/*****************************************************************************
 * OpenVideo: activate GDI video thread output method
 *****************************************************************************/
static int OpenVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;

    p_vout->p_sys = (vout_sys_t *)malloc( sizeof(vout_sys_t) );
    if( !p_vout->p_sys ) return VLC_ENOMEM;

#ifdef MODULE_NAME_IS_wingapi
    /* Load GAPI */
    p_vout->p_sys->gapi_dll = LoadLibrary( _T("GX.DLL") );
    if( p_vout->p_sys->gapi_dll == NULL )
    {
        msg_Warn( p_vout, "failed loading gx.dll" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    GXOpenDisplay = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXOpenDisplay@@YAHPAUHWND__@@K@Z") );
    GXCloseDisplay = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXCloseDisplay@@YAHXZ") );
    GXBeginDraw = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXBeginDraw@@YAPAXXZ") );
    GXEndDraw = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXEndDraw@@YAHXZ") );
    GXGetDisplayProperties = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ") );
    GXSuspend = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXSuspend@@YAHXZ") );
    GXResume = GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXResume@@YAHXZ") );

    if( !GXOpenDisplay || !GXCloseDisplay || !GXBeginDraw || !GXEndDraw ||
        !GXGetDisplayProperties || !GXSuspend || !GXResume )
    {
        msg_Err( p_vout, "failed GetProcAddress on gapi.dll" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "GAPI DLL loaded" );

    p_vout->p_sys->render_width = p_vout->render.i_width;
    p_vout->p_sys->render_height = p_vout->render.i_height;
#endif

    p_vout->p_sys->p_event = (vlc_object_t *)
        vlc_object_create( p_vout, VLC_OBJECT_GENERIC );
    if( !p_vout->p_sys->p_event )
    {
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    var_Create( p_vout->p_sys->p_event, "p_vout", VLC_VAR_ADDRESS );
    val.p_address = (void *)p_vout;
    var_Set( p_vout->p_sys->p_event, "p_vout", val );

    SetRectEmpty( &p_vout->p_sys->rect_display );
    SetRectEmpty( &p_vout->p_sys->rect_parent );

    if( vlc_thread_create( p_vout->p_sys->p_event, "GDI Event Thread",
                           EventThread, 0, 1 ) )
    {
        msg_Err( p_vout, "cannot spawn EventThread" );
        return VLC_ETHREAD;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
#ifdef MODULE_NAME_IS_wingapi
    p_vout->pf_display = DisplayGAPI;
#else
    p_vout->pf_display = DisplayGDI;
#endif
    p_vout->p_sys->i_changes = 0;

    p_vout->p_sys->b_focus = 0;
    p_vout->p_sys->b_parent_focus = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseVideo: deactivate the GDI video output
 *****************************************************************************/
static void CloseVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    p_vout->p_sys->p_event->b_die = VLC_TRUE;
    PostMessage( p_vout->p_sys->hwnd, WM_NULL, 0, 0 );
    vlc_thread_join( p_vout->p_sys->p_event );

#ifdef MODULE_NAME_IS_wingapi
    FreeLibrary( p_vout->p_sys->gapi_dll );
#endif

    var_Destroy( p_vout->p_sys->p_event, "p_vout" );
    vlc_object_destroy( p_vout->p_sys->p_event );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    picture_t *p_pic;

    p_vout->p_sys->rect_display.left = 0;
    p_vout->p_sys->rect_display.top = 0;
    p_vout->p_sys->rect_display.right  = GetSystemMetrics(SM_CXSCREEN);
    p_vout->p_sys->rect_display.bottom = GetSystemMetrics(SM_CYSCREEN);

    p_vout->p_sys->b_video_display = VLC_TRUE;
    p_vout->p_sys->p_event->b_die = VLC_FALSE;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    switch( p_vout->p_sys->i_depth )
    {
    case 8:
        p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
        p_vout->output.pf_setpalette = SetPalette;
        break;
    case 15:
        p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5');
        p_vout->output.i_rmask  = 0x7c00;
        p_vout->output.i_gmask  = 0x03e0;
        p_vout->output.i_bmask  = 0x001f;
        break;
    case 16:
        p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
        p_vout->output.i_rmask  = 0xf800;
        p_vout->output.i_gmask  = 0x07e0;
        p_vout->output.i_bmask  = 0x001f;
        break;
    case 24:
        p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
        p_vout->output.i_rmask  = 0x00ff0000;
        p_vout->output.i_gmask  = 0x0000ff00;
        p_vout->output.i_bmask  = 0x000000ff;
        break;
    case 32:
        p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
        p_vout->output.i_rmask  = 0x00ff0000;
        p_vout->output.i_gmask  = 0x0000ff00;
        p_vout->output.i_bmask  = 0x000000ff;
        break;
    default:
        msg_Err( p_vout, "screen depth %i not supported",
                 p_vout->p_sys->i_depth );
        return VLC_EGENERIC;
        break;
    }

    p_pic = &p_vout->p_picture[0];

#ifdef MODULE_NAME_IS_wingapi
    p_vout->output.i_width  = 0;
    p_vout->output.i_height = 0;
    p_pic->pf_lock  = GAPILockSurface;
    p_pic->pf_unlock = GAPIUnlockSurface;
    Manage( p_vout );
    GAPILockSurface( p_vout, p_pic );
    p_vout->i_changes = 0;
    p_vout->output.i_width  = p_vout->p_sys->render_width;
    p_vout->output.i_height = p_vout->p_sys->render_height;

#else
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
#endif
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_pic->p->p_pixels = p_vout->p_sys->p_pic_buffer;
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_visible_lines = p_vout->output.i_height;
    p_pic->p->i_pitch = p_vout->p_sys->i_pic_pitch;
    p_pic->p->i_pixel_pitch = p_vout->p_sys->i_pic_pixel_pitch;
    p_pic->p->i_visible_pitch = p_vout->output.i_width *
        p_pic->p->i_pixel_pitch;
    p_pic->i_planes = 1;
    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES++ ] = p_pic;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
}

/*****************************************************************************
 * Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
#ifndef UNDER_CE
    WINDOWPLACEMENT window_placement;
#endif

    /* If we do not control our window, we check for geometry changes
     * ourselves because the parent might not send us its events. */
    if( p_vout->p_sys->hparent && !p_vout->b_fullscreen )
    {
        RECT rect_parent;
        POINT point;

        GetClientRect( p_vout->p_sys->hparent, &rect_parent );
        point.x = point.y = 0;
        ClientToScreen( p_vout->p_sys->hparent, &point );
        OffsetRect( &rect_parent, point.x, point.y );

        if( !EqualRect( &rect_parent, &p_vout->p_sys->rect_parent ) )
        {
            int i_x, i_y, i_width, i_height;
            p_vout->p_sys->rect_parent = rect_parent;

            /* This one is to force the update even if only
             * the position has changed */
            SetWindowPos( p_vout->p_sys->hwnd, 0, 1, 1,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

            vout_PlacePicture( p_vout, rect_parent.right - rect_parent.left,
                               rect_parent.bottom - rect_parent.top,
                               &i_x, &i_y, &i_width, &i_height );

            SetWindowPos( p_vout->p_sys->hvideownd, HWND_TOP,
                          i_x, i_y, i_width, i_height, 0 );
        }
    }

    /* We used to call the Win32 PeekMessage function here to read the window
     * messages. But since window can stay blocked into this function for a
     * long time (for example when you move your window on the screen), I
     * decided to isolate PeekMessage in another thread. */

    /*
     * Fullscreen change
     */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE
        || p_vout->p_sys->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        int i_style = 0;
        vlc_value_t val;

        HWND hwnd = (p_vout->p_sys->hparent && p_vout->p_sys->hfswnd) ?
            p_vout->p_sys->hfswnd : p_vout->p_sys->hwnd;

        p_vout->b_fullscreen = ! p_vout->b_fullscreen;

        /* We need to switch between Maximized and Normal sized window */
#ifndef UNDER_CE
        window_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement( hwnd, &window_placement );
#endif
        if( p_vout->b_fullscreen )
        {
#ifndef UNDER_CE
            /* Change window style, no borders and no title bar */
            int i_style = WS_CLIPCHILDREN | WS_VISIBLE;
            SetWindowLong( hwnd, GWL_STYLE, i_style );

            if( p_vout->p_sys->hparent )
            {
                /* Retrieve current window position so fullscreen will happen
                 * on the right screen */
                POINT point = {0,0};
                RECT rect;
                ClientToScreen( p_vout->p_sys->hwnd, &point );
                GetClientRect( p_vout->p_sys->hwnd, &rect );
                SetWindowPos( hwnd, 0, point.x, point.y,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
                GetWindowPlacement( hwnd, &window_placement );
            }

            /* Maximize window */
            window_placement.showCmd = SW_SHOWMAXIMIZED;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
#endif

            if( p_vout->p_sys->hparent )
            {
                RECT rect;
                GetClientRect( hwnd, &rect );
                SetParent( p_vout->p_sys->hwnd, hwnd );
                SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
            }

            ShowWindow( hwnd, SW_SHOW );
            SetForegroundWindow( hwnd );
        }
        else
        {
            /* Change window style, no borders and no title bar */
            //SetWindowLong( hwnd, GWL_STYLE, p_vout->p_sys->i_window_style );

#ifndef UNDER_CE
            /* Normal window */
            window_placement.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
#endif

            if( p_vout->p_sys->hparent )
            {
                RECT rect;
                GetClientRect( p_vout->p_sys->hparent, &rect );
                SetParent( p_vout->p_sys->hwnd, p_vout->p_sys->hparent );
                SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );

                ShowWindow( hwnd, SW_HIDE );
                SetForegroundWindow( p_vout->p_sys->hparent );
            }

            /* Make sure the mouse cursor is displayed */
            //PostMessage( p_vout->p_sys->hwnd, WM_VLC_SHOW_MOUSE, 0, 0 );
        }

        /* Change window style, borders and title bar */
        ShowWindow( p_vout->p_sys->hwnd, SW_SHOW );
        UpdateWindow( p_vout->p_sys->hwnd );

        /* Update the object variable and trigger callback */
        val.b_bool = p_vout->b_fullscreen;
        var_Set( p_vout, "fullscreen", val );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->p_sys->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
#define rect_src p_vout->p_sys->rect_src
#define rect_src_clipped p_vout->p_sys->rect_src_clipped
#define rect_dest p_vout->p_sys->rect_dest
#define rect_dest_clipped p_vout->p_sys->rect_dest_clipped

#ifndef MODULE_NAME_IS_wingapi
static void DisplayGDI( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    RECT rect_dst = rect_dest_clipped;
    HDC hdc = GetDC( p_sys->hvideownd );

    OffsetRect( &rect_dst, -rect_dest.left, -rect_dest.top );
    SelectObject( p_sys->off_dc, p_sys->off_bitmap );

    if( rect_dest_clipped.right - rect_dest_clipped.left !=
        rect_src_clipped.right - rect_src_clipped.left ||
        rect_dest_clipped.bottom - rect_dest_clipped.top !=
        rect_src_clipped.bottom - rect_src_clipped.top )
    {
        StretchBlt( hdc, rect_dst.left, rect_dst.top,
                    rect_dst.right, rect_dst.bottom,
                    p_sys->off_dc, rect_src_clipped.left, rect_src_clipped.top,
                    rect_src_clipped.right, rect_src_clipped.bottom, SRCCOPY );
    }
    else
    {
        BitBlt( hdc, rect_dst.left, rect_dst.top,
                rect_dst.right, rect_dst.bottom,
                p_sys->off_dc, rect_src_clipped.left,
                rect_src_clipped.top, SRCCOPY );
    }

    ReleaseDC( p_sys->hwnd, hdc );
}
#else

static int GAPILockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_x, i_y, i_width, i_height;
    RECT video_rect;
    POINT point;

    /* Undo the display */
    if( ( GetForegroundWindow() != GetParent(p_sys->hwnd) ) ||
        ( p_sys->b_video_display == VLC_FALSE ) )
    {
        //return VLC_EGENERIC;
    }

    GetClientRect( p_sys->hwnd, &video_rect);
    vout_PlacePicture( p_vout, video_rect.right - video_rect.left,
                       video_rect.bottom - video_rect.top,
                       &i_x, &i_y, &i_width, &i_height );
    point.x = point.y = 0;
    ClientToScreen( p_sys->hwnd, &point );
    i_x += point.x + video_rect.left;
    i_y += point.y + video_rect.top;

    if( i_width != p_vout->output.i_width ||
        i_height != p_vout->output.i_height )
    {
        GXDisplayProperties gxdisplayprop = GXGetDisplayProperties();

        p_sys->render_width = i_width;
        p_sys->render_height = i_height;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;

        msg_Dbg( p_vout, "vout size change (%ix%i -> %ix%i)",
                 i_width, i_height, p_vout->output.i_width,
                 p_vout->output.i_height );

        p_vout->p_sys->i_pic_pixel_pitch = gxdisplayprop.cbxPitch;
        p_vout->p_sys->i_pic_pitch = gxdisplayprop.cbyPitch;
        return VLC_EGENERIC;
    }
    else
    {
        GXDisplayProperties gxdisplayprop;
        RECT display_rect, dest_rect;
        uint8_t *p_dest, *p_src = p_pic->p->p_pixels;

        video_rect.left = i_x; video_rect.top = i_y;
        video_rect.right = i_x + i_width;
        video_rect.bottom = i_y + i_height;

        gxdisplayprop = GXGetDisplayProperties();
        display_rect.left = 0; display_rect.top = 0;
        display_rect.right = gxdisplayprop.cxWidth;
        display_rect.bottom = gxdisplayprop.cyHeight;

        if( !IntersectRect( &dest_rect, &video_rect, &display_rect ) )
        {
            return VLC_EGENERIC;
        }

#if 0
        msg_Err( p_vout, "video (%d,%d,%d,%d) display (%d,%d,%d,%d) "
                 "dest (%d,%d,%d,%d)",
                 video_rect.left, video_rect.right,
                 video_rect.top, video_rect.bottom,
                 display_rect.left, display_rect.right,
                 display_rect.top, display_rect.bottom,
                 dest_rect.left, dest_rect.right,
                 dest_rect.top, dest_rect.bottom );
#endif

        if( !(p_dest = GXBeginDraw()) )
        {
#if 0
            msg_Err( p_vout, "GXBeginDraw error %d ", GetLastError() );
#endif
            return VLC_EGENERIC;
        }

        p_src += (dest_rect.left - video_rect.left) * gxdisplayprop.cbxPitch +
            (dest_rect.top - video_rect.top) * p_pic->p->i_pitch;
        p_dest += dest_rect.left * gxdisplayprop.cbxPitch +
            dest_rect.top * gxdisplayprop.cbyPitch;
        i_width = dest_rect.right - dest_rect.left;
        i_height = dest_rect.bottom - dest_rect.top;

        p_pic->p->p_pixels = p_dest;
    }

    return VLC_SUCCESS;
}

static int GAPIUnlockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    GXEndDraw();
    return VLC_SUCCESS;
}

static void DisplayGAPI( vout_thread_t *p_vout, picture_t *p_pic )
{
}
#endif

#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    msg_Err( p_vout, "FIXME: SetPalette unimplemented" );
}

/*****************************************************************************
 * EventThread: Event handling thread
 *****************************************************************************/
static void EventThread ( vlc_object_t *p_event )
{
    vout_thread_t *p_vout;
    vlc_value_t   val;

    int        i_style;
    WNDCLASS   wc;
    MSG        msg;

    /* Initialisations */
    var_Get( p_event, "p_vout", &val );
    p_vout = (vout_thread_t *)val.p_address;

    /* Register window class */
    memset( &wc, 0, sizeof(wc) );
    wc.style          = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc    = (WNDPROC)WndProc;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = 0;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.hIcon          = 0;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wc.lpszMenuName   = 0;
    wc.lpszClassName  = _T("VLC WinGDI");
    RegisterClass( &wc );

    /* Register the video sub-window class */
    wc.lpszClassName = _T("VLC WinGDI video"); wc.hIcon = 0;
    RegisterClass(&wc);

    /* Create output window */
    p_vout->p_sys->hparent = (HWND)
        vout_RequestWindow( p_vout, &p_vout->p_sys->i_window_x,
                            &p_vout->p_sys->i_window_y,
                            (unsigned int *)&p_vout->p_sys->i_window_width,
                            (unsigned int *)&p_vout->p_sys->i_window_height );

    if( p_vout->p_sys->hparent )
        ShowWindow( p_vout->p_sys->hparent, SW_SHOW );

    if( p_vout->p_sys->hparent )
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;
    else
        i_style = WS_OVERLAPPEDWINDOW|WS_SIZEBOX|WS_VISIBLE|WS_CLIPCHILDREN;

    p_vout->p_sys->hwnd =
        CreateWindow( _T("VLC WinGDI"), _T(VOUT_TITLE), i_style,
                      (p_vout->p_sys->i_window_x < 0) ? CW_USEDEFAULT :
                      p_vout->p_sys->i_window_x,   /* default X coordinate */
                      (p_vout->p_sys->i_window_y < 0) ? CW_USEDEFAULT :
                      p_vout->p_sys->i_window_y,   /* default Y coordinate */
                      p_vout->p_sys->i_window_width,
                      p_vout->p_sys->i_window_height + 10,
                      p_vout->p_sys->hparent, NULL,
                      GetModuleHandle(NULL), (LPVOID)p_vout );

    if( !p_vout->p_sys->hwnd )
    {
        msg_Warn( p_vout, "couldn't create window" );
        return;
    }
    msg_Warn( p_vout, "Created WinGDI window" );

    if( p_vout->p_sys->hparent )
    {
        LONG i_style;

        /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( p_vout->p_sys->hparent, GWL_STYLE );

        if( !(i_style & WS_CLIPCHILDREN) )
            /* Hmmm, apparently this is a blocking call... */
            SetWindowLong( p_vout->p_sys->hparent, GWL_STYLE,
                           i_style | WS_CLIPCHILDREN );

        /* Create our fullscreen window */
        p_vout->p_sys->hfswnd =
            CreateWindowEx( WS_EX_APPWINDOW, _T("VLC WinGDI"),
                            _T(VOUT_TITLE),
                            WS_NONAVDONEBUTTON|WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, GetModuleHandle(NULL), (LPVOID)p_vout);
    }

    /* Display our window */
    ShowWindow( p_vout->p_sys->hwnd, SW_SHOW );
    UpdateWindow( p_vout->p_sys->hwnd );

    /* Create video sub-window */
    p_vout->p_sys->hvideownd =
        CreateWindow( _T("VLC WinGDI video"), _T(""),   /* window class */
                    WS_CHILD | WS_VISIBLE,                   /* window style */
                    CW_USEDEFAULT, CW_USEDEFAULT,     /* default coordinates */
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    p_vout->p_sys->hwnd,                  /* parent window */
                    NULL, GetModuleHandle(NULL),
                    (LPVOID)p_vout );            /* send p_vout to WM_CREATE */

    /* Initialize offscreen buffer */
    InitBuffers( p_vout );

    p_vout->pf_control = Control;

    /* Tell the video output we're ready to receive data */
    vlc_thread_ready( p_event );

    while( !p_event->b_die && GetMessage( &msg, 0, 0, 0 ) )
    {
        /* Check if we are asked to exit */
        if( p_event->b_die ) break;

        switch( msg.message )
        {
        case WM_KEYDOWN:
            switch( msg.wParam )
            {
            case VK_ESCAPE:
                p_event->p_vlc->b_die = VLC_TRUE;
                break;
            }
            TranslateMessage( &msg );
            break;

        case WM_CHAR:
            switch( msg.wParam )
            {
            case 'q':
            case 'Q':
                p_event->p_vlc->b_die = VLC_TRUE;
                break;
            }
            break;

        default:
            TranslateMessage( &msg );
            DispatchMessage( &msg );
            break;
        }
    }

    msg_Dbg( p_vout, "CloseWindow" );

#ifdef MODULE_NAME_IS_wingapi
    GXCloseDisplay();
#else
    DeleteDC( p_vout->p_sys->off_dc );
    DeleteObject( p_vout->p_sys->off_bitmap );
#endif

    DestroyWindow( p_vout->p_sys->hwnd );
    if( p_vout->p_sys->hfswnd ) DestroyWindow( p_vout->p_sys->hfswnd );

    if( p_vout->p_sys->hparent )
        vout_ReleaseWindow( p_vout, (void *)p_vout->p_sys->hparent );
}

/*****************************************************************************
 * UpdateRects: update clipping rectangles
 *****************************************************************************
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
static void UpdateRects( vout_thread_t *p_vout, vlc_bool_t b_force )
{
#define rect_src p_vout->p_sys->rect_src
#define rect_src_clipped p_vout->p_sys->rect_src_clipped
#define rect_dest p_vout->p_sys->rect_dest
#define rect_dest_clipped p_vout->p_sys->rect_dest_clipped

    int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

    /* Retrieve the window size */
    GetClientRect( p_vout->p_sys->hwnd, &rect );

    /* Retrieve the window position */
    point.x = point.y = 0;
    ClientToScreen( p_vout->p_sys->hwnd, &point );

    /* If nothing changed, we can return */
    if( !b_force
         && p_vout->p_sys->i_window_width == rect.right
         && p_vout->p_sys->i_window_height == rect.bottom
         && p_vout->p_sys->i_window_x == point.x
         && p_vout->p_sys->i_window_y == point.y )
    {
        return;
    }

    /* Update the window position and size */
    p_vout->p_sys->i_window_x = point.x;
    p_vout->p_sys->i_window_y = point.y;
    p_vout->p_sys->i_window_width = rect.right;
    p_vout->p_sys->i_window_height = rect.bottom;

    vout_PlacePicture( p_vout, rect.right, rect.bottom,
                       &i_x, &i_y, &i_width, &i_height );

    if( p_vout->p_sys->hvideownd )
        SetWindowPos( p_vout->p_sys->hvideownd, HWND_TOP,
                      i_x, i_y, i_width, i_height, 0 );

    /* Destination image position and dimensions */
    rect_dest.left = point.x + i_x;
    rect_dest.right = rect_dest.left + i_width;
    rect_dest.top = point.y + i_y;
    rect_dest.bottom = rect_dest.top + i_height;

    /* Clip the destination window */
    if( !IntersectRect( &rect_dest_clipped, &rect_dest,
                        &p_vout->p_sys->rect_display ) )
    {
        SetRectEmpty( &rect_src_clipped );
        return;
    }

#if 0
    msg_Dbg( p_vout, "image_dst_clipped coords: %i,%i,%i,%i",
                     rect_dest_clipped.left, rect_dest_clipped.top,
                     rect_dest_clipped.right, rect_dest_clipped.bottom );
#endif

    /* the 2 following lines are to fix a bug when clicking on the desktop */
    if( (rect_dest_clipped.right - rect_dest_clipped.left)==0 ||
        (rect_dest_clipped.bottom - rect_dest_clipped.top)==0 )
    {
        SetRectEmpty( &rect_src_clipped );
        return;
    }

    /* src image dimensions */
    rect_src.left = 0;
    rect_src.top = 0;
    rect_src.right = p_vout->output.i_width;
    rect_src.bottom = p_vout->output.i_height;

    /* Clip the source image */
    rect_src_clipped.left = (rect_dest_clipped.left - rect_dest.left) *
      p_vout->output.i_width / (rect_dest.right - rect_dest.left);
    rect_src_clipped.right = p_vout->output.i_width -
      (rect_dest.right - rect_dest_clipped.right) * p_vout->output.i_width /
      (rect_dest.right - rect_dest.left);
    rect_src_clipped.top = (rect_dest_clipped.top - rect_dest.top) *
      p_vout->output.i_height / (rect_dest.bottom - rect_dest.top);
    rect_src_clipped.bottom = p_vout->output.i_height -
      (rect_dest.bottom - rect_dest_clipped.bottom) * p_vout->output.i_height /
      (rect_dest.bottom - rect_dest.top);

#if 0
    msg_Dbg( p_vout, "image_src_clipped coords: %i,%i,%i,%i",
                     rect_src_clipped.left, rect_src_clipped.top,
                     rect_src_clipped.right, rect_src_clipped.bottom );
#endif

    /* The destination coordinates need to be relative to the current
     * directdraw primary surface (display) */
    rect_dest_clipped.left -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.right -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.top -= p_vout->p_sys->rect_display.top;
    rect_dest_clipped.bottom -= p_vout->p_sys->rect_display.top;

    /* Signal the change in size/position */
    p_vout->p_sys->i_changes |= DX_POSITION_CHANGE;

#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
}

/*****************************************************************************
 * Message handler for the main window
 *****************************************************************************/
static long FAR PASCAL WndProc( HWND hWnd, UINT message,
                                WPARAM wParam, LPARAM lParam )
{
    vout_thread_t *p_vout;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_vout = (vout_thread_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)p_vout );
        if( p_vout ) msg_Dbg( p_vout, "create: %p", hWnd );
    }
    else
    {
        p_vout = (vout_thread_t *)GetWindowLongPtr( hWnd, GWLP_USERDATA );
    }

#ifndef UNDER_CE
    /* Catch the screensaver and the monitor turn-off */
    if( message == WM_SYSCOMMAND &&
        ( wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER ) )
    {
        //if( p_vout ) msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND screensaver" );
        return 0; /* this stops them from happening */
    }
#endif

    if( !p_vout )
    {
        /* Hmmm mozilla does manage somehow to save the pointer to our
         * windowproc and still calls it after the vout has been closed. */
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    if( hWnd != p_vout->p_sys->hwnd &&
        hWnd != p_vout->p_sys->hfswnd &&
        hWnd != p_vout->p_sys->hvideownd )
        return DefWindowProc(hWnd, message, wParam, lParam);

    switch( message )
    {
    case WM_WINDOWPOSCHANGED:
        if( hWnd == p_vout->p_sys->hwnd )
            UpdateRects( p_vout, VLC_TRUE );
        break;

#if 0
    case WM_ACTIVATE:
        msg_Err( p_vout, "WM_ACTIVATE: %i", wParam );
        if( wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE )
            GXResume();
        else if( wParam == WA_INACTIVE )
            GXSuspend();
        break;
#endif

    case WM_KILLFOCUS:
        p_vout->p_sys->b_focus = VLC_FALSE;
        if( !p_vout->p_sys->b_parent_focus ) GXSuspend();

        if( hWnd == p_vout->p_sys->hfswnd )
        {
#ifdef UNDER_CE
            HWND htbar = FindWindow( _T("HHTaskbar"), NULL );
            ShowWindow( htbar, SW_SHOW );
#endif
        }

        if( !p_vout->p_sys->hparent ||
            hWnd == p_vout->p_sys->hfswnd )
        {
            SHFullScreen( hWnd, SHFS_SHOWSIPBUTTON );
        }
        break;

    case WM_SETFOCUS:
        p_vout->p_sys->b_focus = VLC_TRUE;
        GXResume();

        if( p_vout->p_sys->hparent &&
            hWnd != p_vout->p_sys->hfswnd && p_vout->b_fullscreen )
            p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;

        if( hWnd == p_vout->p_sys->hfswnd )
        {
#ifdef UNDER_CE
            HWND htbar = FindWindow( _T("HHTaskbar"), NULL );
            ShowWindow( htbar, SW_HIDE );
#endif
        }

        if( !p_vout->p_sys->hparent ||
            hWnd == p_vout->p_sys->hfswnd )
        {
            SHFullScreen( hWnd, SHFS_HIDESIPBUTTON );
        }
        break;

    case WM_LBUTTONDOWN:
        p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;
        break;
    case WM_MOUSEMOVE:
        break;
    case WM_LBUTTONUP:
        break;

    case WM_INITMENUPOPUP:
        p_vout->p_sys->b_video_display = VLC_FALSE;
        break;

    case WM_NOTIFY:
        // Redo the video display because menu can be closed
        // FIXME verify if p_child_window exits
        if( (((NMHDR *)lParam)->code) == NM_CUSTOMDRAW )
            p_vout->p_sys->b_video_display = VLC_TRUE;
        break;

    case WM_DESTROY:
        msg_Dbg( p_vout, "WinProc WM_DESTROY" );
        PostQuitMessage( 0 );
        break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}

/*****************************************************************************
 * InitBuffers: initialize an offscreen bitmap for direct buffer operations.
 *****************************************************************************/
static void InitBuffers( vout_thread_t *p_vout )
{
    BITMAPINFOHEADER *p_header = &p_vout->p_sys->bitmapinfo.bmiHeader;
    BITMAPINFO *p_info = &p_vout->p_sys->bitmapinfo;
    int i_pixels = p_vout->render.i_height * p_vout->render.i_width;
    HDC window_dc = GetDC( p_vout->p_sys->hvideownd );

    /* Get screen properties */
#ifdef MODULE_NAME_IS_wingapi
    GXDisplayProperties gx_displayprop = GXGetDisplayProperties();
    p_vout->p_sys->i_depth = gx_displayprop.cBPP;
#else
    p_vout->p_sys->i_depth = GetDeviceCaps( window_dc, PLANES ) *
        GetDeviceCaps( window_dc, BITSPIXEL );
#endif
    msg_Dbg( p_vout, "GDI depth is %i", p_vout->p_sys->i_depth );

#ifdef MODULE_NAME_IS_wingapi
    GXOpenDisplay( p_vout->p_sys->hvideownd, GX_FULLSCREEN );

#else

    /* Initialize offscreen bitmap */
    memset( p_info, 0, sizeof( BITMAPINFO ) + 3 * sizeof( RGBQUAD ) );

    p_header->biSize = sizeof( BITMAPINFOHEADER );
    p_header->biSizeImage = 0;
    p_header->biPlanes = 1;
    switch( p_vout->p_sys->i_depth )
    {
    case 8:
        p_header->biBitCount = 8;
        p_header->biCompression = BI_RGB;
        /* FIXME: we need a palette here */
        break;
    case 15:
        p_header->biBitCount = 15;
        p_header->biCompression = BI_BITFIELDS;//BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00007c00;
        ((DWORD*)p_info->bmiColors)[1] = 0x000003e0;
        ((DWORD*)p_info->bmiColors)[2] = 0x0000001f;
        break;
    case 16:
        p_header->biBitCount = 16;
        p_header->biCompression = BI_BITFIELDS;//BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x0000f800;
        ((DWORD*)p_info->bmiColors)[1] = 0x000007e0;
        ((DWORD*)p_info->bmiColors)[2] = 0x0000001f;
        break;
    case 24:
        p_header->biBitCount = 24;
        p_header->biCompression = BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00ff0000;
        ((DWORD*)p_info->bmiColors)[1] = 0x0000ff00;
        ((DWORD*)p_info->bmiColors)[2] = 0x000000ff;
        break;
    case 32:
        p_header->biBitCount = 32;
        p_header->biCompression = BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00ff0000;
        ((DWORD*)p_info->bmiColors)[1] = 0x0000ff00;
        ((DWORD*)p_info->bmiColors)[2] = 0x000000ff;
        break;
    default:
        msg_Err( p_vout, "screen depth %i not supported",
                 p_vout->p_sys->i_depth );
        return;
        break;
    }
    p_header->biWidth = p_vout->render.i_width;
    p_header->biHeight = -p_vout->render.i_height;
    p_header->biClrImportant = 0;
    p_header->biClrUsed = 0;
    p_header->biXPelsPerMeter = 0;
    p_header->biYPelsPerMeter = 0;

    p_vout->p_sys->i_pic_pixel_pitch = p_header->biBitCount / 8;
    p_vout->p_sys->i_pic_pitch = p_header->biBitCount * p_header->biWidth / 8;

    p_vout->p_sys->off_bitmap =
        CreateDIBSection( window_dc, (BITMAPINFO *)p_header, DIB_RGB_COLORS,
                          (void**)&p_vout->p_sys->p_pic_buffer, NULL, 0 );

    p_vout->p_sys->off_dc = CreateCompatibleDC( window_dc );

    SelectObject( p_vout->p_sys->off_dc, p_vout->p_sys->off_bitmap );
    ReleaseDC( 0, window_dc );
#endif
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    vlc_bool_t b_bool;

    switch( i_query )
    {
    case VOUT_SET_FOCUS:
        b_bool = va_arg( args, vlc_bool_t );

        p_vout->p_sys->b_parent_focus = b_bool;
        if( b_bool ) GXResume();
        else if( !p_vout->p_sys->b_focus ) GXSuspend();

        return VLC_SUCCESS;

    default:
        return vout_vaControlDefault( p_vout, i_query, args );
    }
}
