/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: wingdi.c,v 1.2 2002/11/22 15:19:47 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAX_DIRECTBUFFERS 10
#define DEFAULT_BPP 8

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenVideo  ( vlc_object_t * );
static void CloseVideo ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
static void Display   ( vout_thread_t *, picture_t * );
static void SetPalette( vout_thread_t *, u16 *, u16 *, u16 * );

static void EventThread        ( vlc_object_t * );
static long FAR PASCAL WndProc ( HWND, UINT, WPARAM, LPARAM );
static void InitOffscreen      ( vout_thread_t * );

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct vout_sys_t
{
    /* The event thread */
    vlc_object_t * p_event;

    /* Our video output window */
    HWND window;

    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;
    BITMAPINFO bitmapinfo;
    uint8_t *  p_buffer;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Windows GDI video output module") );
    set_capability( "video output", 10 );
    set_callbacks( OpenVideo, CloseVideo );
vlc_module_end();

/*****************************************************************************
 * OpenVideo: activate GDI video thread output method
 *****************************************************************************/
static int OpenVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;

    p_vout->p_sys = malloc( sizeof(vout_sys_t) );
    if( !p_vout->p_sys )
    {
        return VLC_ENOMEM;
    }

    p_vout->p_sys->p_event = vlc_object_create( p_vout, VLC_OBJECT_GENERIC );
    if( !p_vout->p_sys->p_event )
    {
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    var_Create( p_vout->p_sys->p_event, "p_vout", VLC_VAR_ADDRESS );
    val.p_address = (void *)p_vout;
    var_Set( p_vout->p_sys->p_event, "p_vout", val );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseVideo: deactivate the GDI video output
 *****************************************************************************/
static void CloseVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    var_Destroy( p_vout->p_sys->p_event, "p_vout" );
    vlc_object_destroy( p_vout->p_sys->p_event );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    if( vlc_thread_create( p_vout->p_sys->p_event, "GDI Event Thread",
                           EventThread, 0, 1 ) )
    {
        msg_Err( p_vout, "cannot spawn EventThread" );
        return VLC_ETHREAD;
    }

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure - we can use either 8 or 15bpp */
#if DEFAULT_BPP == 8
    p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
    p_vout->output.pf_setpalette = SetPalette;
#else
    p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5');
    p_vout->output.i_rmask  = 0x7c00;
    p_vout->output.i_gmask  = 0x03e0;
    p_vout->output.i_bmask  = 0x001f;
#endif
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to initialize MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL )
        {
            break;
        }

        vout_AllocatePicture( p_vout, p_pic, p_vout->output.i_width,
                              p_vout->output.i_height,
                              p_vout->output.i_chroma );

        if( p_pic->i_planes == 0 )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    p_vout->p_sys->p_event->b_die = VLC_TRUE;
    PostMessage( p_vout->p_sys->window, WM_NULL, 0, 0 );
    vlc_thread_join( p_vout->p_sys->p_event );
}

/*****************************************************************************
 * Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
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
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
    HDC hdc;
    int y;

    hdc = GetDC( p_vout->p_sys->window );
    SelectObject( p_vout->p_sys->off_dc, p_vout->p_sys->off_bitmap );

    /* Stupid GDI is upside-down */
    for( y = p_pic->p->i_lines ; y-- ; )
    {
        memcpy( p_vout->p_sys->p_buffer
                         + p_pic->p->i_pitch * (p_pic->p->i_lines-y),
                p_pic->p->p_pixels + p_pic->p->i_pitch * y,
                p_pic->p->i_pitch );
    }

    BitBlt( hdc, 0, 0, p_vout->output.i_width, p_vout->output.i_height,
            p_vout->p_sys->off_dc, 0, 0, SRCCOPY );

    ReleaseDC( p_vout->p_sys->window, hdc );
}

/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout, u16 *red, u16 *green, u16 *blue )
{
    msg_Err( p_vout, "FIXME: SetPalette unimplemented" );
}

/*****************************************************************************
 * EventThread: Event handling thread
 *****************************************************************************/
static void EventThread ( vlc_object_t *p_event )
{
    vout_thread_t * p_vout;
    vlc_value_t     val;

    HINSTANCE  instance;
    WNDCLASS   wc;
    MSG        msg;

    var_Get( p_event, "p_vout", &val );
    p_vout = (vout_thread_t *)val.p_address;

    instance = GetModuleHandle( NULL );

    /* Register window class */
    memset( &wc, 0, sizeof(wc) );
    wc.style          = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc    = (WNDPROC)WndProc;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = 0;
    wc.hInstance      = instance;
    wc.hIcon          = 0;
    wc.hCursor        = 0;
    wc.hbrBackground  = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wc.lpszMenuName   = 0;
    wc.lpszClassName  = L"VOUT";

    RegisterClass( &wc );

    /* Create output window */
    p_vout->p_sys->window =
             CreateWindow( L"VOUT", L"Video Output",
                           WS_VISIBLE | WS_SIZEBOX | WS_CAPTION,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           p_vout->render.i_width,
                           p_vout->render.i_height + 10,
                           NULL, NULL, instance, (LPVOID)p_vout );

    /* Display our window */
    ShowWindow( p_vout->p_sys->window, SW_SHOWNORMAL );
    UpdateWindow( p_vout->p_sys->window );

    vlc_thread_ready( p_event );

    while( !p_event->b_die
             && GetMessage( &msg, p_vout->p_sys->window, 0, 0 ) )
    {
        if( p_event->b_die )
        {
            break;
        }

        DispatchMessage( &msg );
    }

    DestroyWindow( p_vout->p_sys->window );
}

/*****************************************************************************
 * Message handler for the main window
 *****************************************************************************/
static long FAR PASCAL WndProc ( HWND hWnd, UINT message,
                                 WPARAM wParam, LPARAM lParam )
{
    /* Caution: this only works */
    vout_thread_t *p_vout;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_vout = (vout_thread_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLong( hWnd, GWL_USERDATA, (LONG)p_vout );
    }
    else
    {
        p_vout = (vout_thread_t *)GetWindowLong( hWnd, GWL_USERDATA );
    }

    switch( message )
    {
        case WM_LBUTTONDOWN:
            break;
        case WM_MOUSEMOVE:
            break;
        case WM_LBUTTONUP:
            break;

        case WM_CREATE:
            InitOffscreen( p_vout );
            break;

        case WM_DESTROY:
            DeleteDC( p_vout->p_sys->off_dc );
            DeleteObject( p_vout->p_sys->off_bitmap );
            PostQuitMessage( 0 );
            break;

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
   }

   return 0;
}

/*****************************************************************************
 * InitOffscreen: initialize an offscreen bitmap for direct buffer operations.
 *****************************************************************************/
static void InitOffscreen( vout_thread_t *p_vout )
{
    BITMAPINFOHEADER *p_header = &p_vout->p_sys->bitmapinfo.bmiHeader;
    HDC   window_dc;

    window_dc = GetDC( p_vout->p_sys->window );

    p_header->biSize = sizeof( BITMAPINFOHEADER );
    p_header->biPlanes = 1;
    p_header->biCompression = BI_RGB;
#if DEFAULT_BPP == 8
    p_header->biBitCount = 8;
    p_header->biSizeImage =
                    p_vout->render.i_height * p_vout->render.i_width;
#else
    p_header->biBitCount = 16;
    p_header->biSizeImage =
                    p_vout->render.i_height * p_vout->render.i_width * 2;
#endif
    p_header->biWidth = p_vout->render.i_width;
    p_header->biHeight = p_vout->render.i_height;
    p_header->biClrImportant = 0;
    p_header->biClrUsed = 0;
    p_header->biXPelsPerMeter = 0;
    p_header->biYPelsPerMeter = 0;

    p_vout->p_sys->off_bitmap =
          CreateDIBSection( window_dc, (BITMAPINFO *)p_header, DIB_RGB_COLORS,
                            (void**)&p_vout->p_sys->p_buffer, NULL, 0 );

    p_vout->p_sys->off_dc = CreateCompatibleDC( window_dc );

    SelectObject( p_vout->p_sys->off_dc, p_vout->p_sys->off_bitmap );
    ReleaseDC( 0, window_dc );
}

