/*****************************************************************************
 * direct3d.c: Windows Direct3D video output module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *$Id$
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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
 * Preamble:
 *
 * This plugin will use YUV surface if supported, using YUV will result in
 * the best video quality (hardware filering when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures.
 *
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

#include <windows.h>
#include <d3d9.h>

#include "vout.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenVideo  ( vlc_object_t * );
static void CloseVideo ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );
static void FirstDisplay( vout_thread_t *, picture_t * );

static int Direct3DVoutCreate     ( vout_thread_t * );
static void Direct3DVoutRelease   ( vout_thread_t * );

static int  Direct3DVoutOpen      ( vout_thread_t * );
static void Direct3DVoutClose     ( vout_thread_t * );

static int Direct3DVoutResetDevice( vout_thread_t * );

static int Direct3DVoutCreatePictures   ( vout_thread_t *, size_t );
static void Direct3DVoutReleasePictures ( vout_thread_t * );

static int Direct3DVoutLockSurface  ( vout_thread_t *, picture_t * );
static int Direct3DVoutUnlockSurface( vout_thread_t *, picture_t * );

static int Direct3DVoutCreateScene      ( vout_thread_t * );
static void Direct3DVoutReleaseScene    ( vout_thread_t * );
static void Direct3DVoutRenderScene     ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static bool IsVistaOrAbove(void)
{
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if( GetVersionEx(&winVer) )
    {
        if( winVer.dwMajorVersion > 5 )
        {
            /* Windows Vista or above, make this module the default */
            return true;
        }
    }
    /* Windows XP or lower, make sure this module isn't the default */
    return false;
}

static int OpenVideoXP( vlc_object_t *obj )
{
    return IsVistaOrAbove() ? VLC_EGENERIC : OpenVideo( obj );
}

static int OpenVideoVista( vlc_object_t *obj )
{
    return IsVistaOrAbove() ? OpenVideo( obj ) : VLC_EGENERIC;
}

vlc_module_begin ()
    set_shortname( "Direct3D" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("DirectX 3D video output") )
    set_capability( "video output", 50 )
    add_shortcut( "direct3d" )
    set_callbacks( OpenVideoXP, CloseVideo )

    /* FIXME: Hack to avoid unregistering our window class */
    linked_with_a_crap_library_which_uses_atexit ()

    add_submodule()
        set_capability( "video output", 150 )
        set_callbacks( OpenVideoVista, CloseVideo )
vlc_module_end ()

#if 0 /* FIXME */
    /* check if we registered a window class because we need to
     * unregister it */
    WNDCLASS wndclass;
    if( GetClassInfo( GetModuleHandle(NULL), "VLC DirectX", &wndclass ) )
        UnregisterClass( "VLC DirectX", GetModuleHandle(NULL) );
#endif

/*****************************************************************************
 * CUSTOMVERTEX:
 *****************************************************************************
 *****************************************************************************/
typedef struct
{
    FLOAT       x,y,z;      // vertex untransformed position
    FLOAT       rhw;        // eye distance
    D3DCOLOR    diffuse;    // diffuse color
    FLOAT       tu, tv;     // texture relative coordinates
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

/*****************************************************************************
 * OpenVideo: allocate Vout video thread output method
 *****************************************************************************
 * This function allocates and initialize the Direct3D vout method.
 *****************************************************************************/
static int OpenVideo( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    if( VLC_SUCCESS != Direct3DVoutCreate( p_vout ) )
    {
        msg_Err( p_vout, "Direct3D could not be initialized !");
        goto error;
    }

    /* Initialisations */
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Direct3DVoutRenderScene;
    p_vout->pf_display = FirstDisplay;

    p_vout->p_sys->hwnd = p_vout->p_sys->hvideownd = NULL;
    p_vout->p_sys->hparent = p_vout->p_sys->hfswnd = NULL;
    p_vout->p_sys->i_changes = 0;
    vlc_mutex_init( &p_vout->p_sys->lock );
    SetRectEmpty( &p_vout->p_sys->rect_display );
    SetRectEmpty( &p_vout->p_sys->rect_parent );

    var_Create( p_vout, "directx-hw-yuv", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "directx-device", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    p_vout->p_sys->b_cursor_hidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;

    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "disable-screensaver", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Set main window's size */
    p_vout->p_sys->i_window_width = p_vout->i_window_width;
    p_vout->p_sys->i_window_height = p_vout->i_window_height;

    /* Create the Vout EventThread, this thread is created by us to isolate
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

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_Get( p_vout, "video-on-top", &val );
    var_Set( p_vout, "video-on-top", val );

    /* disable screensaver by temporarily changing system settings */
    p_vout->p_sys->i_spi_lowpowertimeout = 0;
    p_vout->p_sys->i_spi_powerofftimeout = 0;
    p_vout->p_sys->i_spi_screensavetimeout = 0;
    var_Get( p_vout, "disable-screensaver", &val);
    if( val.b_bool ) {
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
    return VLC_SUCCESS;

 error:
    CloseVideo( VLC_OBJECT(p_vout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseVideo: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void CloseVideo( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    Direct3DVoutRelease( p_vout );

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

    free( p_vout->p_sys );
    p_vout->p_sys = NULL;
}

/*****************************************************************************
 * Init: initialize Direct3D video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_ret;
    vlc_value_t val;

    var_Get( p_vout, "directx-hw-yuv", &val );
    p_vout->p_sys->b_hw_yuv = val.b_bool;

    /* Initialise Direct3D */
    if( VLC_SUCCESS != Direct3DVoutOpen( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize Direct3D" );
        return VLC_EGENERIC;
    }

    /* Initialize the output structure.
     * Since Direct3D can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;
    UpdateRects( p_vout, true );

    /*  create picture pool */
    p_vout->output.i_chroma = 0;
    i_ret = Direct3DVoutCreatePictures(p_vout, 1);
    if( VLC_SUCCESS != i_ret )
    {
        msg_Err(p_vout, "Direct3D picture pool initialization failed !");
        return i_ret;
    }

    /* create scene */
    i_ret = Direct3DVoutCreateScene(p_vout);
    if( VLC_SUCCESS != i_ret )
    {
        msg_Err(p_vout, "Direct3D scene initialization failed !");
        Direct3DVoutReleasePictures(p_vout);
        return i_ret;
    }

    /* Change the window title bar text */
    PostMessage( p_vout->p_sys->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );

    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create.
 * It is called at the end of the thread.
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    Direct3DVoutReleaseScene(p_vout);
    Direct3DVoutReleasePictures(p_vout);
    Direct3DVoutClose( p_vout );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by the video output thread.
 * It returns a non null value if an error occurred.
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
            p_vout->p_sys->rect_parent = rect_parent;

            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top,
                          SWP_NOZORDER );
        }
    }
    else
    {
        vlc_mutex_unlock( &p_vout->p_sys->lock );
    }

    /*
     * Position Change
     */
    if( p_vout->p_sys->i_changes & DX_POSITION_CHANGE )
    {
#if 0 /* need that when bicubic filter is available */
        RECT rect;
        UINT width, height;

        GetClientRect(p_vout->p_sys->hvideownd, &rect);
        width  = rect.right-rect.left;
        height = rect.bottom-rect.top;

        if( (width != p_vout->p_sys->d3dpp.BackBufferWidth)
         || (height != p_vout->p_sys->d3dpp.BackBufferHeight) )
        {
            msg_Dbg(p_vout, "resizing device back buffers to (%lux%lu)", width, height);
            // need to reset D3D device to resize back buffer
            if( VLC_SUCCESS != Direct3DVoutResetDevice(p_vout, width, height) )
                return VLC_EGENERIC;
        }
#endif
        p_vout->p_sys->i_changes &= ~DX_POSITION_CHANGE;
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
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    LPDIRECT3DDEVICE9       p_d3ddev = p_vout->p_sys->p_d3ddev;
    // Present the back buffer contents to the display
    // stretching and filtering happens here
    HRESULT hr = IDirect3DDevice9_Present(p_d3ddev,
                    &(p_vout->p_sys->rect_src_clipped),
                    NULL, NULL, NULL);
    if( FAILED(hr) )
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
}

/*
** this function is only used once when the first picture is received
** this function will show the video window once a picture is ready
*/

static void FirstDisplay( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* get initial picture presented through D3D */
    Display(p_vout, p_pic);

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
    p_vout->pf_display = Display;
}

/*****************************************************************************
 * DirectD3DVoutCreate: Initialize and instance of Direct3D9
 *****************************************************************************
 * This function initialize Direct3D and analyze available resources from
 * default adapter.
 *****************************************************************************/
static int Direct3DVoutCreate( vout_thread_t *p_vout )
{
    HRESULT hr;
    LPDIRECT3D9 p_d3dobj;
    D3DCAPS9 d3dCaps;

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);

    p_vout->p_sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if( NULL == p_vout->p_sys->hd3d9_dll )
    {
        msg_Warn( p_vout, "cannot load d3d9.dll, aborting" );
        return VLC_EGENERIC;
    }

    OurDirect3DCreate9 =
      (void *)GetProcAddress( p_vout->p_sys->hd3d9_dll,
                              TEXT("Direct3DCreate9") );
    if( OurDirect3DCreate9 == NULL )
    {
        msg_Err( p_vout, "Cannot locate reference to Direct3DCreate9 ABI in DLL" );
        return VLC_EGENERIC;
    }

    /* Create the D3D object. */
    p_d3dobj = OurDirect3DCreate9( D3D_SDK_VERSION );
    if( NULL == p_d3dobj )
    {
       msg_Err( p_vout, "Could not create Direct3D9 instance.");
       return VLC_EGENERIC;
    }
    p_vout->p_sys->p_d3dobj = p_d3dobj;

    /*
    ** Get device capabilities
    */
    ZeroMemory(&d3dCaps, sizeof(d3dCaps));
    hr = IDirect3D9_GetDeviceCaps(p_d3dobj, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps);
    if( FAILED(hr) )
    {
       msg_Err( p_vout, "Could not read adapter capabilities. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    /* TODO: need to test device capabilities and select the right render function */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectD3DVoutRelease: release an instance of Direct3D9
 *****************************************************************************/

static void Direct3DVoutRelease( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->p_d3dobj )
    {
       IDirect3D9_Release(p_vout->p_sys->p_d3dobj);
       p_vout->p_sys->p_d3dobj = NULL;
    }
    if( NULL != p_vout->p_sys->hd3d9_dll )
    {
        FreeLibrary(p_vout->p_sys->hd3d9_dll);
        p_vout->p_sys->hd3d9_dll = NULL;
    }
}

static int Direct3DFillPresentationParameters(vout_thread_t *p_vout, D3DPRESENT_PARAMETERS *d3dpp)
{
    LPDIRECT3D9 p_d3dobj = p_vout->p_sys->p_d3dobj;
    D3DDISPLAYMODE d3ddm;
    HRESULT hr;

    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    hr = IDirect3D9_GetAdapterDisplayMode(p_d3dobj, D3DADAPTER_DEFAULT, &d3ddm );
    if( FAILED(hr))
    {
       msg_Err( p_vout, "Could not read adapter display mode. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    /* keep a copy of current desktop format */
    p_vout->p_sys->bbFormat = d3ddm.Format;

    /* Set up the structure used to create the D3DDevice. */
    ZeroMemory( d3dpp, sizeof(D3DPRESENT_PARAMETERS) );
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->hDeviceWindow          = p_vout->p_sys->hvideownd;
    d3dpp->BackBufferWidth        = p_vout->output.i_width;
    d3dpp->BackBufferHeight       = p_vout->output.i_height;
    d3dpp->SwapEffect             = D3DSWAPEFFECT_COPY;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->BackBufferFormat       = d3ddm.Format;
    d3dpp->BackBufferCount        = 1;
    d3dpp->EnableAutoDepthStencil = FALSE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectD3DVoutOpen: Takes care of all the Direct3D9 initialisations
 *****************************************************************************
 * This function creates Direct3D device
 * this must be called from the vout thread for performance reason, as
 * all Direct3D Device APIs are used in a non multithread safe environment
 *****************************************************************************/
static int Direct3DVoutOpen( vout_thread_t *p_vout )
{
    LPDIRECT3D9 p_d3dobj = p_vout->p_sys->p_d3dobj;
    LPDIRECT3DDEVICE9 p_d3ddev;
    D3DPRESENT_PARAMETERS d3dpp;
    HRESULT hr;

    if( VLC_SUCCESS != Direct3DFillPresentationParameters(p_vout, &d3dpp) )
        return VLC_EGENERIC;

    // Create the D3DDevice
    hr = IDirect3D9_CreateDevice(p_d3dobj, D3DADAPTER_DEFAULT,
                                 D3DDEVTYPE_HAL, p_vout->p_sys->hvideownd,
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING|
                                 D3DCREATE_MULTITHREADED,
                                 &d3dpp, &p_d3ddev );
    if( FAILED(hr) )
    {
       msg_Err(p_vout, "Could not create the D3D device! (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    p_vout->p_sys->p_d3ddev = p_d3ddev;

    msg_Dbg( p_vout, "Direct3D device adapter successfully initialized" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectD3DClose: release the Direct3D9 device
 *****************************************************************************/
static void Direct3DVoutClose( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->p_d3ddev )
    {
       IDirect3DDevice9_Release(p_vout->p_sys->p_d3ddev);
       p_vout->p_sys->p_d3ddev = NULL;
    }

    p_vout->p_sys->hmonitor = NULL;
}

/*****************************************************************************
 * DirectD3DClose: reset the Direct3D9 device
 *****************************************************************************
 * All resources must be deallocated before the reset occur, they will be
 * realllocated once the reset has been performed successfully
 *****************************************************************************/
static int Direct3DVoutResetDevice( vout_thread_t *p_vout )
{
    LPDIRECT3DDEVICE9       p_d3ddev = p_vout->p_sys->p_d3ddev;
    D3DPRESENT_PARAMETERS   d3dpp;
    HRESULT hr;

    if( VLC_SUCCESS != Direct3DFillPresentationParameters(p_vout, &d3dpp) )
        return VLC_EGENERIC;

    // release all D3D objects
    Direct3DVoutReleaseScene( p_vout );
    Direct3DVoutReleasePictures( p_vout );

    hr = IDirect3DDevice9_Reset(p_d3ddev, &d3dpp);
    if( SUCCEEDED(hr) )
    {
        // re-create them
        if( (VLC_SUCCESS != Direct3DVoutCreatePictures(p_vout, 1))
         || (VLC_SUCCESS != Direct3DVoutCreateScene(p_vout)) )
        {
            msg_Dbg(p_vout, "%s failed !", __FUNCTION__);
            return VLC_EGENERIC;
        }
    }
    else {
        msg_Err(p_vout, "%s failed ! (hr=%08lX)", __FUNCTION__, hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static D3DFORMAT Direct3DVoutSelectFormat( vout_thread_t *p_vout, D3DFORMAT target,
    const D3DFORMAT *formats, size_t count)
{
    LPDIRECT3D9 p_d3dobj = p_vout->p_sys->p_d3dobj;
    size_t c;

    for( c=0; c<count; ++c )
    {
        HRESULT hr;
        D3DFORMAT format = formats[c];
        /* test whether device can create a surface of that format */
        hr = IDirect3D9_CheckDeviceFormat(p_d3dobj, D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL, target, 0, D3DRTYPE_SURFACE, format);
        if( SUCCEEDED(hr) )
        {
            /* test whether device can perform color-conversion
            ** from that format to target format
            */
            hr = IDirect3D9_CheckDeviceFormatConversion(p_d3dobj,
                    D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                    format, target);
        }
        if( SUCCEEDED(hr) )
        {
            // found a compatible format
            switch( format )
            {
                case D3DFMT_UYVY:
                    msg_Dbg( p_vout, "selected surface pixel format is UYVY");
                    break;
                case D3DFMT_YUY2:
                    msg_Dbg( p_vout, "selected surface pixel format is YUY2");
                    break;
                case D3DFMT_X8R8G8B8:
                    msg_Dbg( p_vout, "selected surface pixel format is X8R8G8B8");
                    break;
                case D3DFMT_A8R8G8B8:
                    msg_Dbg( p_vout, "selected surface pixel format is A8R8G8B8");
                    break;
                case D3DFMT_R8G8B8:
                    msg_Dbg( p_vout, "selected surface pixel format is R8G8B8");
                    break;
                case D3DFMT_R5G6B5:
                    msg_Dbg( p_vout, "selected surface pixel format is R5G6B5");
                    break;
                case D3DFMT_X1R5G5B5:
                    msg_Dbg( p_vout, "selected surface pixel format is X1R5G5B5");
                    break;
                default:
                    msg_Dbg( p_vout, "selected surface pixel format is 0x%0X", format);
                    break;
            }
            return format;
        }
        else if( D3DERR_NOTAVAILABLE != hr )
        {
            msg_Err( p_vout, "Could not query adapter supported formats. (hr=0x%lX)", hr);
            break;
        }
    }
    return D3DFMT_UNKNOWN;
}

static D3DFORMAT Direct3DVoutFindFormat(vout_thread_t *p_vout, int i_chroma, D3DFORMAT target)
{
    //if( p_vout->p_sys->b_hw_yuv && ! _got_vista_or_above )
    if( p_vout->p_sys->b_hw_yuv )
    {
    /* it sounds like vista does not support YUV surfaces at all */
        switch( i_chroma )
        {
            case VLC_FOURCC('U','Y','V','Y'):
            case VLC_FOURCC('U','Y','N','V'):
            case VLC_FOURCC('Y','4','2','2'):
            {
                static const D3DFORMAT formats[] =
                    { D3DFMT_UYVY, D3DFMT_YUY2, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
            case VLC_FOURCC('I','4','2','0'):
            case VLC_FOURCC('I','4','2','2'):
            case VLC_FOURCC('Y','V','1','2'):
            {
                /* typically 3D textures don't support planar format
                ** fallback to packed version and use CPU for the conversion
                */
                static const D3DFORMAT formats[] =
                    { D3DFMT_YUY2, D3DFMT_UYVY, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
            case VLC_FOURCC('Y','U','Y','2'):
            case VLC_FOURCC('Y','U','N','V'):
            {
                static const D3DFORMAT formats[] =
                    { D3DFMT_YUY2, D3DFMT_UYVY, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
        }
    }

    switch( i_chroma )
    {
        case VLC_FOURCC('R', 'V', '1', '5'):
        {
            static const D3DFORMAT formats[] =
                { D3DFMT_X1R5G5B5 };
            return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case VLC_FOURCC('R', 'V', '1', '6'):
        {
            static const D3DFORMAT formats[] =
                { D3DFMT_R5G6B5 };
            return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case VLC_FOURCC('R', 'V', '2', '4'):
        {
            static const D3DFORMAT formats[] =
                { D3DFMT_R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8 };
            return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case VLC_FOURCC('R', 'V', '3', '2'):
        {
            static const D3DFORMAT formats[] =
                { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8 };
            return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        default:
        {
            /* use display default format */
            LPDIRECT3D9 p_d3dobj = p_vout->p_sys->p_d3dobj;
            D3DDISPLAYMODE d3ddm;

            HRESULT hr = IDirect3D9_GetAdapterDisplayMode(p_d3dobj, D3DADAPTER_DEFAULT, &d3ddm );
            if( SUCCEEDED(hr))
            {
                /*
                ** some professional cards could use some advanced pixel format as default,
                ** make sure we stick with chromas that we can handle internally
                */
                switch( d3ddm.Format )
                {
                    case D3DFMT_R8G8B8:
                    case D3DFMT_X8R8G8B8:
                    case D3DFMT_A8R8G8B8:
                    case D3DFMT_R5G6B5:
                    case D3DFMT_X1R5G5B5:
                        msg_Dbg( p_vout, "defaulting to adapter pixel format");
                        return Direct3DVoutSelectFormat(p_vout, target, &d3ddm.Format, 1);
                    default:
                    {
                        /* if we fall here, that probably means that we need to render some YUV format */
                        static const D3DFORMAT formats[] =
                            { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                        msg_Dbg( p_vout, "defaulting to built-in pixel format");
                        return Direct3DVoutSelectFormat(p_vout, target, formats, sizeof(formats)/sizeof(D3DFORMAT));
                    }
                }
            }
        }
    }
    return D3DFMT_UNKNOWN;
}

static int Direct3DVoutSetOutputFormat(vout_thread_t *p_vout, D3DFORMAT format)
{
    switch( format )
    {
        case D3DFMT_YUY2:
            p_vout->output.i_chroma = VLC_FOURCC('Y', 'U', 'Y', '2');
            break;
        case D3DFMT_UYVY:
            p_vout->output.i_chroma = VLC_FOURCC('U', 'Y', 'V', 'Y');
            break;
        case D3DFMT_R8G8B8:
            p_vout->output.i_chroma = VLC_FOURCC('R', 'V', '2', '4');
            p_vout->output.i_rmask = 0xff0000;
            p_vout->output.i_gmask = 0x00ff00;
            p_vout->output.i_bmask = 0x0000ff;
            break;
        case D3DFMT_X8R8G8B8:
        case D3DFMT_A8R8G8B8:
            p_vout->output.i_chroma = VLC_FOURCC('R', 'V', '3', '2');
            p_vout->output.i_rmask = 0x00ff0000;
            p_vout->output.i_gmask = 0x0000ff00;
            p_vout->output.i_bmask = 0x000000ff;
            break;
        case D3DFMT_R5G6B5:
            p_vout->output.i_chroma = VLC_FOURCC('R', 'V', '1', '6');
            p_vout->output.i_rmask = (0x1fL)<<11;
            p_vout->output.i_gmask = (0x3fL)<<5;
            p_vout->output.i_bmask = (0x1fL)<<0;
            break;
        case D3DFMT_X1R5G5B5:
            p_vout->output.i_chroma = VLC_FOURCC('R', 'V', '1', '5');
            p_vout->output.i_rmask = (0x1fL)<<10;
            p_vout->output.i_gmask = (0x1fL)<<5;
            p_vout->output.i_bmask = (0x1fL)<<0;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutCreatePictures: allocate a vector of identical pictures
 *****************************************************************************
 * Each picture has an associated offscreen surface in video memory
 * depending on hardware capabilities the picture chroma will be as close
 * as possible to the orginal render chroma to reduce CPU conversion overhead
 * and delegate this work to video card GPU
 *****************************************************************************/
static int Direct3DVoutCreatePictures( vout_thread_t *p_vout, size_t i_num_pics )
{
    LPDIRECT3DDEVICE9       p_d3ddev  = p_vout->p_sys->p_d3ddev;
    D3DFORMAT               format;
    HRESULT hr;
    size_t c;
    // if vout is already running, use current chroma, otherwise choose from upstream
    int i_chroma = p_vout->output.i_chroma ? : p_vout->render.i_chroma;

    I_OUTPUTPICTURES = 0;

    /*
    ** find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
    ** the requested chroma which is usable by the hardware in an offscreen surface, as they
    ** typically support more formats than textures
    */
    format = Direct3DVoutFindFormat(p_vout, i_chroma, p_vout->p_sys->bbFormat);
    if( VLC_SUCCESS != Direct3DVoutSetOutputFormat(p_vout, format) )
    {
        msg_Err(p_vout, "surface pixel format is not supported.");
        return VLC_EGENERIC;
    }

    for( c=0; c<i_num_pics; )
    {

        LPDIRECT3DSURFACE9 p_d3dsurf;
        picture_t *p_pic = p_vout->p_picture+c;

        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(p_d3ddev,
                p_vout->render.i_width,
                p_vout->render.i_height,
                format,
                D3DPOOL_DEFAULT,
                &p_d3dsurf,
                NULL);
        if( FAILED(hr) )
        {
            msg_Err(p_vout, "Failed to create picture surface. (hr=0x%lx)", hr);
            Direct3DVoutReleasePictures(p_vout);
            return VLC_EGENERIC;
        }

        /* fill surface with black color */
        IDirect3DDevice9_ColorFill(p_d3ddev, p_d3dsurf, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

        /* assign surface to internal structure */
        p_pic->p_sys = (void *)p_d3dsurf;

        /* Now that we've got our direct-buffer, we can finish filling in the
         * picture_t structures */
        switch( p_vout->output.i_chroma )
        {
            case VLC_FOURCC('R','G','B','2'):
                p_pic->p->i_lines = p_vout->output.i_height;
                p_pic->p->i_visible_lines = p_vout->output.i_height;
                p_pic->p->i_pixel_pitch = 1;
                p_pic->p->i_visible_pitch = p_vout->output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case VLC_FOURCC('R','V','1','5'):
            case VLC_FOURCC('R','V','1','6'):
                p_pic->p->i_lines = p_vout->output.i_height;
                p_pic->p->i_visible_lines = p_vout->output.i_height;
                p_pic->p->i_pixel_pitch = 2;
                p_pic->p->i_visible_pitch = p_vout->output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case VLC_FOURCC('R','V','2','4'):
                p_pic->p->i_lines = p_vout->output.i_height;
                p_pic->p->i_visible_lines = p_vout->output.i_height;
                p_pic->p->i_pixel_pitch = 3;
                p_pic->p->i_visible_pitch = p_vout->output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case VLC_FOURCC('R','V','3','2'):
                p_pic->p->i_lines = p_vout->output.i_height;
                p_pic->p->i_visible_lines = p_vout->output.i_height;
                p_pic->p->i_pixel_pitch = 4;
                p_pic->p->i_visible_pitch = p_vout->output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
                break;
            case VLC_FOURCC('U','Y','V','Y'):
            case VLC_FOURCC('Y','U','Y','2'):
                p_pic->p->i_lines = p_vout->output.i_height;
                p_pic->p->i_visible_lines = p_vout->output.i_height;
                p_pic->p->i_pixel_pitch = 2;
                p_pic->p->i_visible_pitch = p_vout->output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
                break;
            default:
                Direct3DVoutReleasePictures(p_vout);
                return VLC_EGENERIC;
        }
        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;
        p_pic->b_slow   = true;
        p_pic->pf_lock  = Direct3DVoutLockSurface;
        p_pic->pf_unlock = Direct3DVoutUnlockSurface;
        PP_OUTPUTPICTURE[c] = p_pic;

        I_OUTPUTPICTURES = ++c;
    }

    msg_Dbg( p_vout, "%u Direct3D pictures created successfully", c );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutReleasePictures: destroy a picture vector
 *****************************************************************************
 * release all video resources used for pictures
 *****************************************************************************/
static void Direct3DVoutReleasePictures( vout_thread_t *p_vout)
{
    size_t i_num_pics = I_OUTPUTPICTURES;
    size_t c;
    for( c=0; c<i_num_pics; ++c )
    {
        picture_t *p_pic = p_vout->p_picture+c;
        if( p_pic->p_sys )
        {
            LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

            p_pic->p_sys = NULL;

            if( p_d3dsurf )
            {
                IDirect3DSurface9_Release(p_d3dsurf);
            }
        }
    }
    msg_Dbg( p_vout, "%u Direct3D pictures released.", c);

    I_OUTPUTPICTURES = 0;
}

/*****************************************************************************
 * Direct3DVoutLockSurface: Lock surface and get picture data pointer
 *****************************************************************************
 * This function locks a surface and get the surface descriptor which amongst
 * other things has the pointer to the picture data.
 *****************************************************************************/
static int Direct3DVoutLockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    HRESULT hr;
    D3DLOCKED_RECT d3drect;
    LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

    if( NULL == p_d3dsurf )
        return VLC_EGENERIC;

    /* Lock the surface to get a valid pointer to the picture buffer */
    hr = IDirect3DSurface9_LockRect(p_d3dsurf, &d3drect, NULL, 0);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return VLC_EGENERIC;
    }

    /* fill in buffer info in first plane */
    p_pic->p->p_pixels = d3drect.pBits;
    p_pic->p->i_pitch  = d3drect.Pitch;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutUnlockSurface: Unlock a surface locked by Direct3DLockSurface().
 *****************************************************************************/
static int Direct3DVoutUnlockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    HRESULT hr;
    LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

    if( NULL == p_d3dsurf )
        return VLC_EGENERIC;

    /* Unlock the Surface */
    hr = IDirect3DSurface9_UnlockRect(p_d3dsurf);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutCreateScene: allocate and initialize a 3D scene
 *****************************************************************************
 * for advanced blending/filtering a texture needs be used in a 3D scene.
 *****************************************************************************/

static int Direct3DVoutCreateScene( vout_thread_t *p_vout )
{
    LPDIRECT3DDEVICE9       p_d3ddev  = p_vout->p_sys->p_d3ddev;
    LPDIRECT3DTEXTURE9      p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc;

    HRESULT hr;

    /*
    ** Create a texture for use when rendering a scene
    ** for performance reason, texture format is identical to backbuffer
    ** which would usually be a RGB format
    */
    hr = IDirect3DDevice9_CreateTexture(p_d3ddev,
            p_vout->render.i_width,
            p_vout->render.i_height,
            1,
            D3DUSAGE_RENDERTARGET,
            p_vout->p_sys->bbFormat,
            D3DPOOL_DEFAULT,
            &p_d3dtex,
            NULL);
    if( FAILED(hr))
    {
        msg_Err(p_vout, "Failed to create texture. (hr=0x%lx)", hr);
        return VLC_EGENERIC;
    }

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    hr = IDirect3DDevice9_CreateVertexBuffer(p_d3ddev,
            sizeof(CUSTOMVERTEX)*4,
            D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
            D3DFVF_CUSTOMVERTEX,
            D3DPOOL_DEFAULT,
            &p_d3dvtc,
            NULL);
    if( FAILED(hr) )
    {
        msg_Err(p_vout, "Failed to create vertex buffer. (hr=0x%lx)", hr);
            IDirect3DTexture9_Release(p_d3dtex);
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_d3dtex = p_d3dtex;
    p_vout->p_sys->p_d3dvtc = p_d3dvtc;

    // Texture coordinates outside the range [0.0, 1.0] are set
    // to the texture color at 0.0 or 1.0, respectively.
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // set maximum ambient light
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_LIGHTING, FALSE);

    // Enable dithering
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_STENCILENABLE, FALSE);

    // manage blending
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHATESTENABLE,TRUE);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHAREF, 0x10);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHAFUNC,D3DCMP_GREATER);

    // Set texture states
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLORARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLORARG2,D3DTA_DIFFUSE);

    // turn off alpha operation
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    msg_Dbg( p_vout, "Direct3D scene created successfully");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutReleaseScene
 *****************************************************************************/
static void Direct3DVoutReleaseScene( vout_thread_t *p_vout )
{
    LPDIRECT3DTEXTURE9      p_d3dtex = p_vout->p_sys->p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc = p_vout->p_sys->p_d3dvtc;

    if( p_d3dvtc )
    {
        IDirect3DVertexBuffer9_Release(p_d3dvtc);
        p_vout->p_sys->p_d3dvtc = NULL;
    }

    if( p_d3dtex )
    {
        IDirect3DTexture9_Release(p_d3dtex);
        p_vout->p_sys->p_d3dtex = NULL;
    }
    msg_Dbg( p_vout, "Direct3D scene released successfully");
}

/*****************************************************************************
 * Render: copy picture surface into a texture and render into a scene
 *****************************************************************************
 * This function is intented for higher end 3D cards, with pixel shader support
 * and at least 64 MB of video RAM.
 *****************************************************************************/
static void Direct3DVoutRenderScene( vout_thread_t *p_vout, picture_t *p_pic )
{
    LPDIRECT3DDEVICE9       p_d3ddev  = p_vout->p_sys->p_d3ddev;
    LPDIRECT3DTEXTURE9      p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc;
    LPDIRECT3DSURFACE9      p_d3dsrc, p_d3ddest;
    CUSTOMVERTEX            *p_vertices;
    HRESULT hr;
    float f_width, f_height;

    // check if device is still available
    hr = IDirect3DDevice9_TestCooperativeLevel(p_d3ddev);
    if( FAILED(hr) )
    {
        if( (D3DERR_DEVICENOTRESET != hr)
         || (VLC_SUCCESS != Direct3DVoutResetDevice(p_vout)) )
        {
            // device is not usable at present (lost device, out of video mem ?)
            return;
        }
    }
    p_d3dtex  = p_vout->p_sys->p_d3dtex;
    p_d3dvtc  = p_vout->p_sys->p_d3dvtc;

    /* Clear the backbuffer and the zbuffer */
    hr = IDirect3DDevice9_Clear( p_d3ddev, 0, NULL, D3DCLEAR_TARGET,
                              D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0 );
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    /*  retrieve picture surface */
    p_d3dsrc = (LPDIRECT3DSURFACE9)p_pic->p_sys;
    if( NULL == p_d3dsrc )
    {
        msg_Dbg( p_vout, "no surface to render ?");
        return;
    }

    /* retrieve texture top-level surface */
    hr = IDirect3DTexture9_GetSurfaceLevel(p_d3dtex, 0, &p_d3ddest);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    /* Copy picture surface into texture surface, color space conversion happens here */
    hr = IDirect3DDevice9_StretchRect(p_d3ddev, p_d3dsrc, NULL, p_d3ddest, NULL, D3DTEXF_NONE);
    IDirect3DSurface9_Release(p_d3ddest);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    /* Update the vertex buffer */
    hr = IDirect3DVertexBuffer9_Lock(p_d3dvtc, 0, 0, (VOID **)(&p_vertices), D3DLOCK_DISCARD);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    /* Setup vertices */
    f_width  = (float)(p_vout->output.i_width);
    f_height = (float)(p_vout->output.i_height);

    /* -0.5f is a "feature" of DirectX and it seems to apply to Direct3d also */
    /* http://www.sjbrown.co.uk/2003/05/01/fix-directx-rasterisation/ */
    p_vertices[0].x       = -0.5f;       // left
    p_vertices[0].y       = -0.5f;       // top
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[0].rhw     = 1.0f;
    p_vertices[0].tu      = 0.0f;
    p_vertices[0].tv      = 0.0f;

    p_vertices[1].x       = f_width - 0.5f;    // right
    p_vertices[1].y       = -0.5f;       // top
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[1].rhw     = 1.0f;
    p_vertices[1].tu      = 1.0f;
    p_vertices[1].tv      = 0.0f;

    p_vertices[2].x       = f_width - 0.5f;    // right
    p_vertices[2].y       = f_height - 0.5f;   // bottom
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[2].rhw     = 1.0f;
    p_vertices[2].tu      = 1.0f;
    p_vertices[2].tv      = 1.0f;

    p_vertices[3].x       = -0.5f;       // left
    p_vertices[3].y       = f_height - 0.5f;   // bottom
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[3].rhw     = 1.0f;
    p_vertices[3].tu      = 0.0f;
    p_vertices[3].tv      = 1.0f;

    hr= IDirect3DVertexBuffer9_Unlock(p_d3dvtc);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    // Begin the scene
    hr = IDirect3DDevice9_BeginScene(p_d3ddev);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    // Setup our texture. Using textures introduces the texture stage states,
    // which govern how textures get blended together (in the case of multiple
    // textures) and lighting information. In this case, we are modulating
    // (blending) our texture with the diffuse color of the vertices.
    hr = IDirect3DDevice9_SetTexture(p_d3ddev, 0, (LPDIRECT3DBASETEXTURE9)p_d3dtex);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // Render the vertex buffer contents
    hr = IDirect3DDevice9_SetStreamSource(p_d3ddev, 0, p_d3dvtc, 0, sizeof(CUSTOMVERTEX));
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // we use FVF instead of vertex shader
    hr = IDirect3DDevice9_SetFVF(p_d3ddev, D3DFVF_CUSTOMVERTEX);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // draw rectangle
    hr = IDirect3DDevice9_DrawPrimitive(p_d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // End the scene
    hr = IDirect3DDevice9_EndScene(p_d3ddev);
    if( FAILED(hr) )
    {
        msg_Dbg( p_vout, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }
}

