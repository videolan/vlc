/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_beos.cpp,v 1.48 2002/03/22 13:16:35 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>
#include <string.h>                                            /* strerror() */
#include <InterfaceKit.h>
#include <DirectWindow.h>
#include <Application.h>
#include <Bitmap.h>

extern "C"
{
#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "interface.h"
}

#include "VideoWindow.h"

#define BITS_PER_PLANE  16
#define BYTES_PER_PIXEL 2

/*****************************************************************************
 * vout_sys_t: BeOS video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the BeOS specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    VideoWindow *  p_window;

    s32 i_width;
    s32 i_height;

    u8 *pp_buffer[2];
    int i_index;

} vout_sys_t;


/*****************************************************************************
 * beos_GetAppWindow : retrieve a BWindow pointer from the window name
 *****************************************************************************/
BWindow *beos_GetAppWindow(char *name)
{
    int32       index;
    BWindow     *window;
    
    for (index = 0 ; ; index++)
    {
        window = be_app->WindowAt(index);
        if (window == NULL)
            break;
        if (window->LockWithTimeout(20000) == B_OK)
        {
            if (strcmp(window->Name(), name) == 0)
            {
                window->Unlock();
                break;
            }
            window->Unlock();
        }
    }
    return window; 
}

/**************************************************************************** 
 * DrawingThread : thread that really does the drawing 
 ****************************************************************************/ 
int32 Draw(void *data) 
{ 
    VideoWindow* p_win; 
    p_win = (VideoWindow *) data; 

    if ( p_win-> voutWindow-> LockLooper() ) 
    {
        if (p_win->vsync)
        {
            BScreen *screen;
            screen = new BScreen(p_win->voutWindow);
            screen-> WaitForRetrace(22000);
            delete screen; 
        }
        if (p_win->resized)
        {
            p_win->resized = false;
            p_win-> view-> FillRect(p_win-> voutWindow-> Bounds());
        }  
        p_win-> view-> DrawBitmap( p_win-> bitmap[p_win-> i_buffer],  
                                   BRect(p_win->out_left, p_win->out_top,
                                         p_win->out_left + p_win->out_width, 
                                         p_win->out_top + p_win->out_height) );  
        p_win-> voutWindow-> UnlockLooper(); 
    } 
    return B_OK; 
}

/*****************************************************************************
 * bitmapWindow : This is the bitmap window output
 *****************************************************************************/
bitmapWindow::bitmapWindow(BRect frame, VideoWindow *theOwner)
        : BWindow( frame, NULL, B_TITLED_WINDOW, 
                    B_NOT_CLOSABLE | B_NOT_MINIMIZABLE )
{
    owner = theOwner;
    SetTitle(VOUT_TITLE " (BBitmap output)");
}

bitmapWindow::~bitmapWindow()
{
}

void bitmapWindow::FrameMoved(BPoint origin) 
{
    owner->FrameMoved(origin);
}

void bitmapWindow::FrameResized( float width, float height )
{
    owner->FrameResized(width, height);
}

void bitmapWindow::Zoom(BPoint origin, float width, float height )
{
    owner->Zoom( origin, width, height );
}

void bitmapWindow::ScreenChanged(BRect frame, color_space mode)
{
    owner->ScreenChanged(frame, mode);
}

void bitmapWindow::drawBuffer(int bufferIndex)
{
    status_t status;

    owner->i_buffer = bufferIndex; 
    owner->fDrawThreadID = spawn_thread(Draw, "drawing_thread",
                                        B_DISPLAY_PRIORITY, (void*) owner);
    wait_for_thread(owner->fDrawThreadID, &status);
    
}

/*****************************************************************************
 * directWindow : This is the bitmap window output
 *****************************************************************************/
directWindow::directWindow(BRect frame, VideoWindow *theOwner)
        : BDirectWindow( frame, NULL, B_TITLED_WINDOW, 
                    B_NOT_CLOSABLE | B_NOT_MINIMIZABLE )
{
    owner = theOwner;
    SetTitle(VOUT_TITLE " (DirectWindow output)");
}

directWindow::~directWindow()
{
}

void directWindow::DirectConnected(direct_buffer_info *info)
{
}

void directWindow::FrameMoved(BPoint origin) 
{
    owner->FrameMoved(origin);
}

void directWindow::FrameResized( float width, float height )
{
    owner->FrameResized(width, height);
}

void directWindow::Zoom(BPoint origin, float width, float height )
{
    owner->Zoom( origin, width, height );
}

void directWindow::ScreenChanged(BRect frame, color_space mode)
{
    owner->ScreenChanged(frame, mode);
}

void directWindow::drawBuffer(int bufferIndex)
{
    status_t status;

    owner->i_buffer = bufferIndex; 
    owner->fDrawThreadID = spawn_thread(Draw, "drawing_thread",
                                        B_DISPLAY_PRIORITY, (void*) owner);
    wait_for_thread(owner->fDrawThreadID, &status);
}

/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/
VideoWindow::VideoWindow( int v_width, int v_height, 
                          int w_width, int w_height )
{
    // need to centre the window on the screeen.
    if ( BDirectWindow::SupportsWindowMode() )
    { 
        voutWindow = new directWindow( BRect( 20, 50, 
                                              20 + w_width, 50 + w_height ), this );
        mode = DIRECT;
    }
    else
    {
        voutWindow = new bitmapWindow( BRect( 20, 50, 
                                              20 + w_width, 50 + w_height ), this );
        mode = BITMAP;
    }

    /* set the VideoWindow variables */
    teardownwindow = false;
    is_zoomed = false;
    resized = true;

    /* call ScreenChanged to set vsync correctly */
    BScreen *screen;
    screen = new BScreen(voutWindow);
    ScreenChanged(screen->Frame(), screen->ColorSpace());
    delete screen;
    
    /* create the view to do the display */
    view = new VLCView( voutWindow->Bounds() );
    voutWindow->AddChild(view);
    
    /* Bitmap mode overlay not available, set the system to 32bits
     * and let BeOS do all the work */
    bitmap[0] = new BBitmap( BRect( 0, 0, v_width, v_height ), B_RGB32);
    bitmap[1] = new BBitmap( BRect( 0, 0, v_width, v_height ), B_RGB32);
    memset(bitmap[0]->Bits(), 0, bitmap[0]->BitsLength());
    memset(bitmap[1]->Bits(), 0, bitmap[1]->BitsLength());

    // remember current settings
    i_width = w_width;
    i_height = w_height;
    FrameResized(w_width, w_height);
    voutWindow->Show();
}

VideoWindow::~VideoWindow()
{
    int32 result;

    teardownwindow = true;
    wait_for_thread(fDrawThreadID, &result);
    voutWindow->Hide();
    voutWindow->Sync();
    voutWindow->Lock();
    voutWindow->Quit();
    delete bitmap[0];
    delete bitmap[1];
}

void VideoWindow::drawBuffer(int bufferIndex)
{
    switch( mode )
    {
        case DIRECT:
        {
            directWindow *dW = (directWindow*)voutWindow;
            dW->drawBuffer(bufferIndex);
            break;
        }
        case BITMAP:
        default:
        {
            bitmapWindow *bW = (bitmapWindow*)voutWindow;
            bW->drawBuffer(bufferIndex);
            break;
        }
    }
}

void VideoWindow::Zoom(BPoint origin, float width, float height )
{
    if(is_zoomed)
    {
        is_zoomed = !is_zoomed;
        voutWindow->MoveTo(winSize.left, winSize.top);
        voutWindow->ResizeTo(winSize.IntegerWidth(), winSize.IntegerHeight());
        width_scale = winSize.IntegerWidth() / i_width;
        height_scale = winSize.IntegerHeight() / i_height;
        be_app->ShowCursor();
    }
    else
    {
        is_zoomed = !is_zoomed;
        BScreen *screen;
        screen = new BScreen(voutWindow);
        BRect rect = screen->Frame();
        delete screen;
        voutWindow->MoveTo(0,0);
        voutWindow->ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
        width_scale = rect.IntegerWidth() / i_width;
        height_scale = rect.IntegerHeight() / i_height;
        be_app->ObscureCursor();
    }
    resized = true;
}

void VideoWindow::FrameMoved(BPoint origin) 
{
	if (is_zoomed) return ;
    winSize = voutWindow->Frame();
    resized = true;
}

void VideoWindow::FrameResized( float width, float height )
{
    width_scale = width / i_width;
    height_scale = height / i_height;
    if (width_scale <= height_scale)
    {
        out_width = i_width * width_scale;
        out_height = i_height * width_scale;
        out_left = 0; 
        out_top = (voutWindow->Frame().Height() - out_height) / 2;
    }
    else   /* if the height is proportionally smaller */
    {
        out_width = i_width * height_scale;
        out_height = i_height * height_scale;
        out_top = 0;
        out_left = (voutWindow->Frame().Width() - out_width) /2;
    }

	if (is_zoomed) return ;
    winSize = voutWindow->Frame();
    width_scale = width / i_width;
    height_scale = height / i_height;
    resized = true;
}

void VideoWindow::ScreenChanged(BRect frame, color_space mode)
{
    BScreen *screen;
    float refresh;
    
    screen = new BScreen(voutWindow);
    display_mode disp_mode; 
    
    screen-> GetMode(&disp_mode); 
    refresh = 
         (disp_mode.timing.pixel_clock * 1000)/((disp_mode.timing.h_total)* 
         (disp_mode.timing.v_total)); 
    if (refresh  < 61) 
    { 
        vsync = true; 
    } 
}

/*****************************************************************************
 * VLCView::VLCView
 *****************************************************************************/
VLCView::VLCView(BRect bounds) : BView(bounds, "", B_FOLLOW_ALL, B_WILL_DRAW)
{
    SetViewColor(B_TRANSPARENT_32_BIT);
}

/*****************************************************************************
 * VLCView::~VLCView
 *****************************************************************************/
VLCView::~VLCView()
{
}

/*****************************************************************************
 * VLCVIew::MouseDown
 *****************************************************************************/
void VLCView::MouseDown(BPoint point)
{
    BWindow *win = Window();
    win->Zoom();
}

/*****************************************************************************
 * VLCVIew::Draw
 *****************************************************************************/
void VLCView::Draw(BRect updateRect) 
{
    FillRect(updateRect);
}


extern "C"
{

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create     ( vout_thread_t * );
static int  vout_Init       ( vout_thread_t * );
static void vout_End        ( vout_thread_t * );
static void vout_Destroy    ( vout_thread_t * );
static int  vout_Manage     ( vout_thread_t * );
static void vout_Display    ( vout_thread_t *, picture_t * );
static void vout_Render     ( vout_thread_t *, picture_t * );

static int  BeosOpenDisplay ( vout_thread_t *p_vout );
static void BeosCloseDisplay( vout_thread_t *p_vout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_render     = vout_Render;
}

/*****************************************************************************
 * vout_Create: allocates BeOS video thread output method
 *****************************************************************************
 * This function allocates and initializes a BeOS vout method.
 *****************************************************************************/
int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = (vout_sys_t*) malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    p_vout->p_sys->i_width = p_vout->render.i_width;
    p_vout->p_sys->i_height = p_vout->render.i_height;
      
    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize BeOS video thread output method
 *****************************************************************************/
int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Open and initialize device */
    if( BeosOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display");
        return 0;
    }
    /* Set the buffers */
    p_vout->p_sys->pp_buffer[0] = (u8*)p_vout->p_sys->p_window->bitmap[0]->Bits();
    p_vout->p_sys->pp_buffer[1] = (u8*)p_vout->p_sys->p_window->bitmap[1]->Bits();

    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;

    p_vout->output.i_chroma = FOURCC_RV32;

    p_vout->output.i_rmask  = 0x00ff0000;
    p_vout->output.i_gmask  = 0x0000ff00;
    p_vout->output.i_bmask  = 0x000000ff;

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

    if( p_pic == NULL )
    {
        return 0;
    }

    p_vout->p_sys->i_index = 0;
    p_pic->p->p_pixels = p_vout->p_sys->pp_buffer[0];
    p_pic->p->i_pixel_bytes = 4;
    p_pic->p->i_lines = p_vout->p_sys->i_height;
    p_pic->p->b_margin = 0;
    p_pic->p->i_pitch = 4 * p_vout->p_sys->i_width;

    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate BeOS video thread output method
 *****************************************************************************/
void vout_End( vout_thread_t *p_vout )
{
    BeosCloseDisplay( p_vout );
}

/*****************************************************************************
 * vout_Destroy: destroy BeOS video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void vout_Destroy( vout_thread_t *p_vout )
{
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle BeOS events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
int vout_Manage( vout_thread_t *p_vout )
{
                          
    return( 0 );
}

/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to BeOS image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;

    /* draw buffer if required */    
    if (!p_win->teardownwindow)
    {
       p_win->drawBuffer(p_vout->p_sys->i_index);
    }
    /* change buffer */
    p_vout->p_sys->i_index = ++p_vout->p_sys->i_index & 1;
    p_pic->p->p_pixels = p_vout->p_sys->pp_buffer[p_vout->p_sys->i_index];
}

/* following functions are local */

/*****************************************************************************
 * BeosOpenDisplay: open and initialize BeOS device
 *****************************************************************************/
static int BeosOpenDisplay( vout_thread_t *p_vout )
{ 

    p_vout->p_sys->p_window = new VideoWindow( p_vout->p_sys->i_width - 1,
                                               p_vout->p_sys->i_height - 1,
                                               p_vout->i_window_width,
                                               p_vout->i_window_height);

    if( p_vout->p_sys->p_window == NULL )
    {
        intf_ErrMsg( "error: cannot allocate memory for VideoWindow" );
        return( 1 );
    }   
    
    return( 0 );
}

/*****************************************************************************
 * BeosDisplay: close and reset BeOS device
 *****************************************************************************
 * Returns all resources allocated by BeosOpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void BeosCloseDisplay( vout_thread_t *p_vout )
{    
    /* Destroy the video window */
    delete p_vout->p_sys->p_window;
}

} /* extern "C" */
