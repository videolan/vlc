/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>

/*#ifdef MODULE_NAME_IS_wingapi
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
#endif */ /* MODULE_NAME_IS_wingapi */

#include "vout.h"

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
//#define SetWindowLongPtr SetWindowLong
//#define GetWindowLongPtr GetWindowLong
//#define GWLP_USERDATA GWL_USERDATA
#define AdjustWindowRect(a,b,c)
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
static void FirstDisplayGAPI( vout_thread_t *, picture_t * );
static void DisplayGAPI( vout_thread_t *, picture_t * );
static int GAPILockSurface( vout_thread_t *, picture_t * );
static int GAPIUnlockSurface( vout_thread_t *, picture_t * );
#else
static void FirstDisplayGDI( vout_thread_t *, picture_t * );
static void DisplayGDI( vout_thread_t *, picture_t * );
#endif
static void SetPalette( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

static void InitBuffers        ( vout_thread_t * );



#define DX_POSITION_CHANGE 0x1000

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
#ifdef MODULE_NAME_IS_wingapi
    set_shortname( "Windows GAPI" )
    set_description( N_("Windows GAPI video output") )
    set_capability( "video output", 20 )
#else
    set_shortname( "Windows GDI" )
    set_description( N_("Windows GDI video output") )
    set_capability( "video output", 10 )
#endif
    set_callbacks( OpenVideo, CloseVideo )
vlc_module_end ()

/*****************************************************************************
 * OpenVideo: activate GDI video thread output method
 *****************************************************************************/
static int OpenVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    p_vout->p_sys = (vout_sys_t *)calloc( 1, sizeof(vout_sys_t) );
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
        vlc_object_create( p_vout, sizeof( vlc_object_t ) );
    if( !p_vout->p_sys->p_event )
    {
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
#ifdef MODULE_NAME_IS_wingapi
    p_vout->pf_display = FirstDisplayGAPI;

    p_vout->p_sys->b_focus = 0;
    p_vout->p_sys->b_parent_focus = 0;

#else
    p_vout->pf_display = FirstDisplayGDI;
#endif

    p_vout->p_sys->hwnd = p_vout->p_sys->hvideownd = NULL;
    p_vout->p_sys->hparent = p_vout->p_sys->hfswnd = NULL;
    p_vout->p_sys->i_changes = 0;
    vlc_mutex_init( &p_vout->p_sys->lock );
    SetRectEmpty( &p_vout->p_sys->rect_display );
    SetRectEmpty( &p_vout->p_sys->rect_parent );

    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "disable-screensaver", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    p_vout->p_sys->b_cursor_hidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;

    /* Set main window's size */
    p_vout->p_sys->i_window_width = p_vout->i_window_width;
    p_vout->p_sys->i_window_height = p_vout->i_window_height;

    /* Create the EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( p_vout, "creating Vout EventThread" );
    p_vout->p_sys->p_event =
        vlc_object_create( p_vout, sizeof(event_thread_t) );
    p_vout->p_sys->p_event->p_vout = p_vout;
    p_vout->p_sys->p_event->window_ready = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( vlc_thread_create( p_vout->p_sys->p_event, "Vout Events Thread",
                           EventThread, 0 ) )
    {
        msg_Err( p_vout, "cannot create Vout EventThread" );
        CloseHandle( p_vout->p_sys->p_event->window_ready );
        vlc_object_release( p_vout->p_sys->p_event );
        p_vout->p_sys->p_event = NULL;
        goto error;
    }
    WaitForSingleObject( p_vout->p_sys->p_event->window_ready, INFINITE );
    CloseHandle( p_vout->p_sys->p_event->window_ready );

    if( p_vout->p_sys->p_event->b_error )
    {
        msg_Err( p_vout, "Vout EventThread failed" );
        goto error;
    }

    vlc_object_attach( p_vout->p_sys->p_event, p_vout );

    msg_Dbg( p_vout, "Vout EventThread running" );

#ifndef UNDER_CE
    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

    /* disable screensaver by temporarily changing system settings */
    p_vout->p_sys->i_spi_lowpowertimeout = 0;
    p_vout->p_sys->i_spi_powerofftimeout = 0;
    p_vout->p_sys->i_spi_screensavetimeout = 0;
    if( var_GetBool( p_vout, "disable-screensaver" ) ) {
        msg_Dbg(p_vout, "disabling screen saver");
        SystemParametersInfo(SPI_GETLOWPOWERTIMEOUT,
            0, &(p_vout->p_sys->i_spi_lowpowertimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_lowpowertimeout ) {
            SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT, 0, NULL, 0);
        }
        SystemParametersInfo(SPI_GETPOWEROFFTIMEOUT, 0,
            &(p_vout->p_sys->i_spi_powerofftimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_powerofftimeout ) {
            SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT, 0, NULL, 0);
        }
        SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0,
            &(p_vout->p_sys->i_spi_screensavetimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_screensavetimeout ) {
            SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 0, NULL, 0);
        }
    }
#endif
    return VLC_SUCCESS;

error:
    CloseVideo( VLC_OBJECT(p_vout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseVideo: deactivate the GDI video output
 *****************************************************************************/
static void CloseVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "Quitting fullscreen" );
        Win32ToggleFullscreen( p_vout );
        /* Force fullscreen in the core for the next video */
        var_SetBool( p_vout, "fullscreen", true );
    }

    if( p_vout->p_sys->p_event )
    {
        vlc_object_detach( p_vout->p_sys->p_event );

        /* Kill Vout EventThread */
        vlc_object_kill( p_vout->p_sys->p_event );

        /* we need to be sure Vout EventThread won't stay stuck in
         * GetMessage, so we send a fake message */
        if( p_vout->p_sys->hwnd )
        {
            PostMessage( p_vout->p_sys->hwnd, WM_NULL, 0, 0);
        }

        vlc_thread_join( p_vout->p_sys->p_event );
        vlc_object_release( p_vout->p_sys->p_event );
    }
    vlc_mutex_destroy( &p_vout->p_sys->lock );

#ifndef UNDER_CE
    /* restore screensaver system settings */
    if( 0 != p_vout->p_sys->i_spi_lowpowertimeout ) {
        SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT,
            p_vout->p_sys->i_spi_lowpowertimeout, NULL, 0);
    }
    if( 0 != p_vout->p_sys->i_spi_powerofftimeout ) {
        SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT,
            p_vout->p_sys->i_spi_powerofftimeout, NULL, 0);
    }
    if( 0 != p_vout->p_sys->i_spi_screensavetimeout ) {
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT,
            p_vout->p_sys->i_spi_screensavetimeout, NULL, 0);
    }
#endif

#ifdef MODULE_NAME_IS_wingapi
    FreeLibrary( p_vout->p_sys->gapi_dll );
#endif

    free( p_vout->p_sys );
    p_vout->p_sys = NULL;
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    picture_t *p_pic;

    /* Initialize offscreen buffer */
    InitBuffers( p_vout );

    p_vout->p_sys->rect_display.left = 0;
    p_vout->p_sys->rect_display.top = 0;
    p_vout->p_sys->rect_display.right  = GetSystemMetrics(SM_CXSCREEN);
    p_vout->p_sys->rect_display.bottom = GetSystemMetrics(SM_CYSCREEN);

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

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;
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

    /* Change the window title bar text */
    PostMessage( p_vout->p_sys->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );
    UpdateRects( p_vout, true );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
#ifdef MODULE_NAME_IS_wingapi
    GXCloseDisplay();
#else
    DeleteDC( p_vout->p_sys->off_dc );
    DeleteObject( p_vout->p_sys->off_bitmap );
#endif
}

/*****************************************************************************
 * Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    /* If we do not control our window, we check for geometry changes
     * ourselves because the parent might not send us its events. */
    vlc_mutex_lock( &p_vout->p_sys->lock );
    if( p_vout->p_sys->hparent && !p_vout->b_fullscreen )
    {
        RECT rect_parent;
        POINT point;

        vlc_mutex_unlock( &p_vout->p_sys->lock );

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
    else
    {
        vlc_mutex_unlock( &p_vout->p_sys->lock );
    }

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = (int) ZOOM_FP_FACTOR;

        UpdateRects( p_vout, true );
    }

    /* scaling factor */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom =
            (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );
        UpdateRects( p_vout, true );
    }

    /* Check for cropping / aspect changes */
    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;
        UpdateRects( p_vout, true );
    }

    /*
     * Position Change
     */
    if( p_vout->p_sys->i_changes & DX_POSITION_CHANGE )
    {
        p_vout->p_sys->i_changes &= ~DX_POSITION_CHANGE;
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
        Win32ToggleFullscreen( p_vout );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->p_sys->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    if( p_vout->b_fullscreen && !p_vout->p_sys->b_cursor_hidden &&
        (mdate() - p_vout->p_sys->i_lastmoved) >
            p_vout->p_sys->i_mouse_hide_timeout )
    {
        POINT point;
        HWND hwnd;

        /* Hide the cursor only if it is inside our window */
        GetCursorPos( &point );
        hwnd = WindowFromPoint(point);
        if( hwnd == p_vout->p_sys->hwnd || hwnd == p_vout->p_sys->hvideownd )
        {
            PostMessage( p_vout->p_sys->hwnd, WM_VLC_HIDE_MOUSE, 0, 0 );
        }
        else
        {
            p_vout->p_sys->i_lastmoved = mdate();
        }
    }

    /*
     * "Always on top" status change
     */
    if( p_vout->p_sys->b_on_top_change )
    {
        vlc_value_t val;
        HMENU hMenu = GetSystemMenu( p_vout->p_sys->hwnd, FALSE );

        var_Get( p_vout, "video-on-top", &val );

        /* Set the window on top if necessary */
        if( val.b_bool && !( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_CHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }
        else
        /* The window shouldn't be on top */
        if( !val.b_bool && ( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_UNCHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }

        p_vout->p_sys->b_on_top_change = false;
    }

    /* Check if the event thread is still running */
    if( !vlc_object_alive (p_vout->p_sys->p_event) )
    {
        return VLC_EGENERIC; /* exit */
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
    (void)p_vout;
    (void)p_pic;
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

    ReleaseDC( p_sys->hvideownd, hdc );
}

static void FirstDisplayGDI( vout_thread_t *p_vout, picture_t *p_pic )
{
    /*
    ** Video window is initially hidden, show it now since we got a
    ** picture to show.
    */
    SetWindowPos( p_vout->p_sys->hvideownd, 0, 0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS|
        SWP_FRAMECHANGED|
        SWP_SHOWWINDOW|
        SWP_NOMOVE|
        SWP_NOSIZE|
        SWP_NOZORDER );

    /* get initial picture presented */
    DisplayGDI(p_vout, p_pic);

    /* use and restores proper display function for further pictures */
    p_vout->pf_display = DisplayGDI;
}

#else

static int GAPILockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_x, i_y, i_width, i_height;
    RECT video_rect;
    POINT point;

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

static void FirstDisplayGAPI( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* get initial picture presented through D3D */
    DisplayGAPI(p_vout, p_pic);

    /*
    ** Video window is initially hidden, show it now since we got a
    ** picture to show.
    */
    SetWindowPos( p_vout->p_sys->hvideownd, 0, 0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS|
        SWP_FRAMECHANGED|
        SWP_SHOWWINDOW|
        SWP_NOMOVE|
        SWP_NOSIZE|
        SWP_NOZORDER );

    /* use and restores proper display function for further pictures */
    p_vout->pf_display = DisplayGAPI;
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
 * InitBuffers: initialize an offscreen bitmap for direct buffer operations.
 *****************************************************************************/
static void InitBuffers( vout_thread_t *p_vout )
{
    BITMAPINFOHEADER *p_header = &p_vout->p_sys->bitmapinfo.bmiHeader;
    BITMAPINFO *p_info = &p_vout->p_sys->bitmapinfo;
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
    ReleaseDC( p_vout->p_sys->hvideownd, window_dc );
#endif
}

