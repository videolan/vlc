/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_beos.cpp,v 1.41 2002/02/15 13:32:52 sam Exp $
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
    //rudolf: sync init: 
    BScreen *screen; 
    display_mode disp_mode; 
    static uint32 refresh, oldrefresh = 0; 

    screen = new BScreen(); 
    screen-> GetMode(&disp_mode); 
    refresh = 
         (disp_mode.timing.pixel_clock * 1000)/((disp_mode.timing.h_total)* 
         (disp_mode.timing.v_total)); 
    if (!(refresh == oldrefresh)) 
    { 
        intf_WarnMsg( 1, "vout info: new refreshrate is %ld:Hz", refresh ); 
        oldrefresh = refresh; 
        if (refresh < 61 ) 
        { 
            intf_WarnMsg( 1, "vout info: enabling retrace sync" ); 
        } 
        else 
        { 
            intf_WarnMsg( 1, "vout info: disabling retrace sync" ); 
        } 
    } 

    VideoWindow* p_win; 
    p_win = (VideoWindow *) data; 
    if ( p_win-> voutWindow-> LockLooper() ) 
    { 
        //rudolf: sync: 
        if (refresh  < 61) 
        { 
            screen-> WaitForRetrace(22000);//set timeout for  < 45 Hz... 
        } 

        p_win-> view-> DrawBitmap( p_win-> bitmap[p_win-> i_buffer], 
                                 p_win-> bitmap[p_win-> i_buffer]-> Bounds(), 
                                 p_win-> voutWindow-> Bounds() );  
        p_win-> voutWindow-> UnlockLooper(); 
    } 
    return B_OK; 
}

/*****************************************************************************
 * bitmapWindow : This is the bitmap window output
 *****************************************************************************/
bitmapWindow::bitmapWindow(BRect frame, VideoWindow *theOwner)
        : BWindow( frame, NULL, B_TITLED_WINDOW, 
                   B_OUTLINE_RESIZE | B_NOT_CLOSABLE | B_NOT_MINIMIZABLE )
{
    is_zoomed = false;
    origRect = frame;
    owner = theOwner;
    SetTitle(VOUT_TITLE " (BBitmap output)");
}

bitmapWindow::~bitmapWindow()
{
}

void bitmapWindow::FrameResized( float width, float height )
{
    if (is_zoomed)
    {
        return;
    }
    float width_scale;
    float height_scale;

    width_scale = width / origRect.Width();
    height_scale = height / origRect.Height();
    
    /* if the width is proportionally smaller */
    if (width_scale <= height_scale)
    {
        ResizeTo(width, origRect.Height() * width_scale);
    }
    else /* if the height is proportionally smaller */
    {
        ResizeTo(origRect.Width() * height_scale, height);
    }
}

void bitmapWindow::Zoom(BPoint origin, float width, float height )
{
    if(is_zoomed)
    {
        MoveTo(origRect.left, origRect.top);
        ResizeTo(origRect.IntegerWidth(), origRect.IntegerHeight());
        be_app->ShowCursor();
    }
    else
    {
        BScreen *screen;
        screen = new BScreen(this);
        BRect rect = screen->Frame();
        delete screen;
        MoveTo(0,0);
        ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
        be_app->HideCursor();
    }
    is_zoomed = !is_zoomed;
}

/*****************************************************************************
 * directWindow : This is the bitmap window output
 *****************************************************************************/
directWindow::directWindow(BRect frame, VideoWindow *theOwner)
        : BDirectWindow( frame, NULL, B_TITLED_WINDOW, 
                   B_OUTLINE_RESIZE | B_NOT_CLOSABLE | B_NOT_MINIMIZABLE )
{
    is_zoomed = false;
    origRect = frame;
    owner = theOwner;
    SetTitle(VOUT_TITLE " (DirectWindow output)");
}

directWindow::~directWindow()
{
}

void directWindow::DirectConnected(direct_buffer_info *info)
{
}

void directWindow::FrameResized( float width, float height )
{
    if (is_zoomed)
    {
        return;
    }
    float width_scale;
    float height_scale;

    width_scale = width / origRect.Width();
    height_scale = height / origRect.Height();
    
    /* if the width is proportionally smaller */
    if (width_scale <= height_scale)
    {
        ResizeTo(width, origRect.Height() * width_scale);
    }
    else /* if the height is proportionally smaller */
    {
        ResizeTo(origRect.Width() * height_scale, height);
    }
}

void directWindow::Zoom(BPoint origin, float width, float height )
{
    if(is_zoomed)
    {
        SetFullScreen(false);
        MoveTo(origRect.left, origRect.top);
        ResizeTo(origRect.IntegerWidth(), origRect.IntegerHeight());
        be_app->ShowCursor();
    }
    else
    {
        SetFullScreen(true);
        BScreen *screen;
        screen = new BScreen(this);
        BRect rect = screen->Frame();
        delete screen;
        MoveTo(0,0);
        ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
        be_app->HideCursor();
    }
    is_zoomed = !is_zoomed;
}

/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/
VideoWindow::VideoWindow( int width, int height, 
                          vout_thread_t *p_video_output )
{
    if ( BDirectWindow::SupportsWindowMode() )
    { 
        voutWindow = new directWindow( BRect( 80, 50, 
                                          80 + width, 50 + height ), this );
    }
    else
    {
        voutWindow = new bitmapWindow( BRect( 80, 50, 
                                          80 + width, 50 + height ), this );
    }

    /* set the VideoWindow variables */
    teardownwindow = false;
    
    /* create the view to do the display */
    view = new VLCView( voutWindow->Bounds() );
    voutWindow->AddChild(view);
    
    /* Bitmap mode overlay not available, set the system to 32bits
     * and let BeOS do all the work */
    bitmap[0] = new BBitmap( voutWindow->Bounds(), B_RGB32);
    bitmap[1] = new BBitmap( voutWindow->Bounds(), B_RGB32);
    memset(bitmap[0]->Bits(), 0, bitmap[0]->BitsLength());
    memset(bitmap[1]->Bits(), 0, bitmap[1]->BitsLength());

    i_width = bitmap[0]->Bounds().IntegerWidth();
    i_height = bitmap[0]->Bounds().IntegerHeight();

    voutWindow->Show();
}

VideoWindow::~VideoWindow()
{
    int32 result;

    voutWindow->Hide();
    voutWindow->Sync();
    voutWindow->Lock();
    voutWindow->Quit();
    teardownwindow = true;
    wait_for_thread(fDrawThreadID, &result);
    delete bitmap[0];
    delete bitmap[1];
}

void VideoWindow::resizeIfRequired( int newWidth, int newHeight )
{
    if (( newWidth != i_width + 1) &&
        ( newHeight != i_height + 1) &&
        ( newWidth != 0 ))
    {
        if ( voutWindow->Lock() )
        {
            view->ClearViewBitmap();
            i_width = newWidth - 1;
            i_height = newHeight -1;
            voutWindow->ResizeTo((float) i_width, (float) i_height); 
            voutWindow->Unlock();
        }
    }
}

void VideoWindow::drawBuffer(int bufferIndex)
{
    status_t status;
    
    i_buffer = bufferIndex; 
    
    fDrawThreadID = spawn_thread(Draw, "drawing_thread",
                    B_DISPLAY_PRIORITY, (void*) this);
    wait_for_thread(fDrawThreadID, &status);
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
 * VLCVIew::~VLCView
 *****************************************************************************/
void VLCView::MouseDown(BPoint point)
{
    BWindow *win = Window();
    win->Zoom();
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

    if( p_vout->render.i_height * p_vout->render.i_aspect
         >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        p_vout->p_sys->i_width = p_vout->render.i_height
            * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        p_vout->p_sys->i_height = p_vout->render.i_height;
    }
    else
    {
        p_vout->p_sys->i_width = p_vout->render.i_width;
        p_vout->p_sys->i_height = p_vout->render.i_width
            * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }

    /* Open and initialize device */
    if( BeosOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize BeOS video thread output method
 *****************************************************************************/
int vout_Init( vout_thread_t *p_vout )
{
    VideoWindow *p_win = p_vout->p_sys->p_window;

    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_width  = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR
                               / p_vout->p_sys->i_height;
    p_vout->output.i_chroma = FOURCC_RV32;

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

    p_pic->p->i_red_mask   = 0x00ff0000;
    p_pic->p->i_green_mask = 0x0000ff00;
    p_pic->p->i_blue_mask  = 0x000000ff;

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
    /* place code here to end the video */
}

/*****************************************************************************
 * vout_Destroy: destroy BeOS video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void vout_Destroy( vout_thread_t *p_vout )
{
    BeosCloseDisplay( p_vout );
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
    VideoWindow * p_win = p_vout->p_sys->p_window;
    
//    p_win->resizeIfRequired(p_vout->p_sys->pp_buffer[p_vout->p_sys->i_index].i_pic_width,
//                            p_vout->p_sys->pp_buffer[p_vout->p_sys->i_index].i_pic_height);
                            
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
    p_vout->p_sys->p_window = new VideoWindow(
            main_GetIntVariable( VOUT_WIDTH_VAR, VOUT_WIDTH_DEFAULT ) - 1,
            main_GetIntVariable( VOUT_HEIGHT_VAR, VOUT_HEIGHT_DEFAULT ) - 1,
            p_vout );

    if( p_vout->p_sys->p_window == NULL )
    {
        intf_ErrMsg( "error: cannot allocate memory for VideoWindow" );
        return( 1 );
    }   
    
    p_vout->p_sys->i_width    = p_vout->p_sys->p_window->i_width + 1;
    p_vout->p_sys->i_height   = p_vout->p_sys->p_window->i_height + 1;

    p_vout->p_sys->pp_buffer[0] = (u8*)p_vout->p_sys->p_window->bitmap[0]->Bits();
    p_vout->p_sys->pp_buffer[1] = (u8*)p_vout->p_sys->p_window->bitmap[1]->Bits();

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
