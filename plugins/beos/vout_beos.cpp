/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_beos.cpp,v 1.57 2002/05/22 12:23:41 tcastley Exp $
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
#include "DrawingTidbits.h"

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

    u8 *pp_buffer[3];
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

/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/
VideoWindow::VideoWindow( int v_width, int v_height, 
                          BRect frame )
            : BWindow( frame, NULL, B_TITLED_WINDOW, 
                    B_NOT_CLOSABLE | B_NOT_MINIMIZABLE )
{
    BView *mainView =  new BView( Bounds(), "mainView", 
                                  B_FOLLOW_ALL, B_FULL_UPDATE_ON_RESIZE);
    AddChild(mainView);
    mainView->SetViewColor(kBlack);
                                  
    /* create the view to do the display */
    view = new VLCView( Bounds() );
    mainView->AddChild(view);

    /* set the VideoWindow variables */
    teardownwindow = false;
    is_zoomed = false;
    i_buffer = 0;

    /* call ScreenChanged to set vsync correctly */
    BScreen *screen;
    display_mode disp_mode; 
    float refresh;

    screen = new BScreen(this);
    
    screen-> GetMode(&disp_mode); 
    refresh = 
         (disp_mode.timing.pixel_clock * 1000)/((disp_mode.timing.h_total)* 
         (disp_mode.timing.v_total)); 
    if (refresh  < 61) 
    { 
        vsync = true; 
    } 
    delete screen;
    
    // remember current settings
    i_width = frame.IntegerWidth();
    i_height = frame.IntegerHeight();
    FrameResized(frame.IntegerWidth(), frame.IntegerHeight());

    mode = SelectDrawingMode(v_width, v_height);
    Show();
}

VideoWindow::~VideoWindow()
{
    int32 result;

    teardownwindow = true;
    wait_for_thread(fDrawThreadID, &result);
    delete bitmap[0];
    delete bitmap[1];
    delete bitmap[2];
}

void VideoWindow::drawBuffer(int bufferIndex)
{
    status_t status;

    i_buffer = bufferIndex;
    // sync to the screen if required
    if (vsync)
    {
        BScreen *screen;
        screen = new BScreen(this);
        screen-> WaitForRetrace(22000);
        delete screen;
    }
    if (LockLooper())
    {
       // switch the overlay bitmap
       if (mode == OVERLAY)
       {
          rgb_color key;
          view->SetViewOverlay(bitmap[i_buffer], 
                            bitmap[i_buffer]->Bounds() ,
                            view->Bounds(),
                            &key, B_FOLLOW_ALL,
		                    B_OVERLAY_FILTER_HORIZONTAL|B_OVERLAY_FILTER_VERTICAL|
		                    B_OVERLAY_TRANSFER_CHANNEL);
		   view->SetViewColor(key);
	   }
       else
       {
         // switch the bitmap
         view-> DrawBitmap(bitmap[i_buffer], view->Bounds() );
       }
       UnlockLooper();
    }
}

void VideoWindow::Zoom(BPoint origin, float width, float height )
{
    if(is_zoomed)
    {
        is_zoomed = !is_zoomed;
        MoveTo(winSize.left, winSize.top);
        ResizeTo(winSize.IntegerWidth(), winSize.IntegerHeight());
        be_app->ShowCursor();
    }
    else
    {
        is_zoomed = !is_zoomed;
        BScreen *screen;
        screen = new BScreen(this);
        BRect rect = screen->Frame();
        delete screen;
        MoveTo(0,0);
        ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
        be_app->ObscureCursor();
    }
}

void VideoWindow::FrameMoved(BPoint origin) 
{
	if (is_zoomed) return ;
    winSize = Frame();
}

void VideoWindow::FrameResized( float width, float height )
{
    float out_width, out_height;
    float out_left, out_top;
    float width_scale = width / i_width;
    float height_scale = height / i_height;

    if (width_scale <= height_scale)
    {
        out_width = (i_width * width_scale);
        out_height = (i_height * width_scale);
        out_left = 0; 
        out_top = (height - out_height) / 2;
    }
    else   /* if the height is proportionally smaller */
    {
        out_width = (i_width * height_scale);
        out_height = (i_height * height_scale);
        out_top = 0;
        out_left = (width - out_width) /2;
    }
    view->MoveTo(out_left,out_top);
    view->ResizeTo(out_width, out_height);
	if (!is_zoomed)
	{
        winSize = Frame();
    }
}

void VideoWindow::ScreenChanged(BRect frame, color_space mode)
{
    BScreen *screen;
    float refresh;
    
    screen = new BScreen(this);
    display_mode disp_mode; 
    
    screen-> GetMode(&disp_mode); 
    refresh = 
         (disp_mode.timing.pixel_clock * 1000)/((disp_mode.timing.h_total)* 
         (disp_mode.timing.v_total)); 
    if (refresh  < 61) 
    { 
        vsync = true; 
    } 
    rgb_color key;
    view->SetViewOverlay(bitmap[i_buffer], 
                         bitmap[i_buffer]->Bounds() ,
                         view->Bounds(),
                         &key, B_FOLLOW_ALL,
                         B_OVERLAY_FILTER_HORIZONTAL|B_OVERLAY_FILTER_VERTICAL);
    view->SetViewColor(key);
}

void VideoWindow::WindowActivated(bool active)
{
}

int VideoWindow::SelectDrawingMode(int width, int height)
{
    int drawingMode = BITMAP;

    int noOverlay = config_GetIntVariable( "nooverlay" );
    for (int i = 0; i < COLOR_COUNT; i++)
    {
        if (noOverlay) break;
        bitmap[0] = new BBitmap ( BRect( 0, 0, width, height ), 
                                  B_BITMAP_WILL_OVERLAY|B_BITMAP_RESERVE_OVERLAY_CHANNEL,
                                  colspace[i].colspace);

        if(bitmap[0] && bitmap[0]->InitCheck() == B_OK) 
        {
            colspace_index = i;

            bitmap[1] = new BBitmap( BRect( 0, 0, width, height ), B_BITMAP_WILL_OVERLAY,
                                     colspace[colspace_index].colspace);
            bitmap[2] = new BBitmap( BRect( 0, 0, width, height ), B_BITMAP_WILL_OVERLAY,
                                     colspace[colspace_index].colspace);
            if ( (bitmap[2] && bitmap[2]->InitCheck() == B_OK) )
            {
               drawingMode = OVERLAY;
               rgb_color key;
               view->SetViewOverlay(bitmap[0], 
                                    bitmap[0]->Bounds() ,
                                    view->Bounds(),
                                    &key, B_FOLLOW_ALL,
		                            B_OVERLAY_FILTER_HORIZONTAL|B_OVERLAY_FILTER_VERTICAL);
		       view->SetViewColor(key);
               SetTitle(VOUT_TITLE " (Overlay)");
               break;
            }
            else
            {
               delete bitmap[0];
               delete bitmap[1];
               delete bitmap[2];
            }
        }
        else
        {
            delete bitmap[0];
        }        
	}

    if (drawingMode == BITMAP)
	{
        // fallback to RGB32
        colspace_index = DEFAULT_COL;
        SetTitle(VOUT_TITLE " (Bitmap)");
        bitmap[0] = new BBitmap( BRect( 0, 0, width, height ), colspace[colspace_index].colspace);
        bitmap[1] = new BBitmap( BRect( 0, 0, width, height ), colspace[colspace_index].colspace);
        bitmap[2] = new BBitmap( BRect( 0, 0, width, height ), colspace[colspace_index].colspace);
    }
    return drawingMode;
}

/*****************************************************************************
 * VLCView::VLCView
 *****************************************************************************/
VLCView::VLCView(BRect bounds) : BView(bounds, "", B_FOLLOW_NONE,
                                       B_WILL_DRAW)

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
    VideoWindow *win = (VideoWindow *) Window();
    if (win->mode == BITMAP)
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
    p_vout->p_sys->pp_buffer[2] = (u8*)p_vout->p_sys->p_window->bitmap[2]->Bits();
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
    p_vout->output.i_chroma = colspace[p_vout->p_sys->p_window->colspace_index].chroma;
    p_vout->p_sys->i_index = 0;

    p_vout->output.i_rmask  = 0x00ff0000;
    p_vout->output.i_gmask  = 0x0000ff00;
    p_vout->output.i_bmask  = 0x000000ff;

    for (int buffer_index = 0 ; buffer_index < 3; buffer_index++)
    {
       p_pic = NULL;
       /* Find an empty picture slot */
       for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
       {
           p_pic = NULL;
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
       p_pic->p->p_pixels = p_vout->p_sys->pp_buffer[0];
       p_pic->p->i_lines = p_vout->p_sys->i_height;

       p_pic->p->i_pixel_bytes = colspace[p_vout->p_sys->p_window->colspace_index].pixel_bytes;
       p_pic->i_planes = colspace[p_vout->p_sys->p_window->colspace_index].planes;
       p_pic->p->i_pitch = p_vout->p_sys->p_window->bitmap[0]->BytesPerRow(); 

       if (p_vout->p_sys->p_window->mode == OVERLAY)
       {
          p_pic->p->i_visible_bytes = (p_vout->p_sys->p_window->bitmap[0]->Bounds().IntegerWidth()+1) 
                                     * p_pic->p->i_pixel_bytes; 
          p_pic->p->b_margin = 1;
          p_pic->p->b_hidden = 0;
       }
       else
       {
          p_pic->p->b_margin = 0;
          p_pic->p->i_visible_bytes = p_pic->p->i_pitch;
       }

       p_pic->i_status = DESTROYED_PICTURE;
       p_pic->i_type   = DIRECT_PICTURE;
 
       PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

       I_OUTPUTPICTURES++;
    }

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
    p_vout->p_sys->i_index = ++p_vout->p_sys->i_index % 3;
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
                                               BRect( 20, 50,
                                                      20 + p_vout->i_window_width -1, 
                                                      50 + p_vout->i_window_height ));

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
    VideoWindow * p_win = p_vout->p_sys->p_window;
    /* Destroy the video window */
    if( p_win != NULL && !p_win->teardownwindow)
    {
        p_win->Lock();
        p_win->teardownwindow = true;
        p_win->Hide();
        p_win->Quit();
    }
}



} /* extern "C" */
