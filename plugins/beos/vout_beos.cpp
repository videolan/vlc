/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_beos.cpp,v 1.65 2002/07/31 20:56:50 sam Exp $
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include "VideoWindow.h"
#include "DrawingTidbits.h"
#include "MsgVals.h"


/*****************************************************************************
 * vout_sys_t: BeOS video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the BeOS specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    VideoWindow *  p_window;

    s32 i_width;
    s32 i_height;

    u32 source_chroma;
    int i_index;
};

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
    vsync = false;
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
    
    mode = SelectDrawingMode(v_width, v_height);

    // remember current settings
    i_width = v_width;
    i_height = v_height;
    FrameResized(v_width, v_height);

    if (mode == OVERLAY)
    {
       overlay_restrictions r;

       bitmap[1]->GetOverlayRestrictions(&r);
       SetSizeLimits((i_width * r.min_width_scale) + 1, i_width * r.max_width_scale,
                     (i_height * r.min_height_scale) + 1, i_height * r.max_height_scale);
    }
    Show();
}

VideoWindow::~VideoWindow()
{
    teardownwindow = true;
    delete bitmap[0];
    delete bitmap[1];
    delete bitmap[2];
}

void VideoWindow::MessageReceived( BMessage *p_message )
{
    switch( p_message->what )
    {
    case TOGGLE_FULL_SCREEN:
        ((BWindow *)this)->Zoom();
        break;
    case RESIZE_100:
        if (is_zoomed)
        {
           ((BWindow *)this)->Zoom();
        }
        ResizeTo(i_width, i_height);
        break;
    case RESIZE_200:
        if (is_zoomed)
        {
           ((BWindow *)this)->Zoom();
        }
        ResizeTo(i_width * 2, i_height * 2);
        break;
    case VERT_SYNC:
        vsync = !vsync;
        break;
    case WINDOW_FEEL:
        {
            int16 winFeel;
            if (p_message->FindInt16("WinFeel", &winFeel) == B_OK)
            {
                SetFeel((window_feel)winFeel);
            }
        }
        break;
    default:
        BWindow::MessageReceived( p_message );
        break;
    }
}

void VideoWindow::drawBuffer(int bufferIndex)
{
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
}

void VideoWindow::WindowActivated(bool active)
{
}

int VideoWindow::SelectDrawingMode(int width, int height)
{
    int drawingMode = BITMAP;
    int noOverlay = 0;

//    int noOverlay = !config_GetIntVariable( "overlay" );
    for (int i = 0; i < COLOR_COUNT; i++)
    {
        if (noOverlay) break;
        bitmap[0] = new BBitmap ( BRect( 0, 0, width, height ), 
                                  B_BITMAP_WILL_OVERLAY,
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
        // fallback to RGB16
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
    BMessage* msg = Window()->CurrentMessage();
    int32 clicks = msg->FindInt32("clicks");

    VideoWindow *vWindow = (VideoWindow *)Window();
    uint32 mouseButtons;
    BPoint where;
    GetMouse(&where, &mouseButtons, true);

    if ((mouseButtons & B_PRIMARY_MOUSE_BUTTON) && (clicks == 2))
    {
       Window()->Zoom();
       return;
    }
    else
    {
       if (mouseButtons & B_SECONDARY_MOUSE_BUTTON) 
       {
           BPopUpMenu *menu = new BPopUpMenu("context menu");
           menu->SetRadioMode(false);
           // Toggle FullScreen
           BMenuItem *zoomItem = new BMenuItem("Fullscreen", new BMessage(TOGGLE_FULL_SCREEN));
           zoomItem->SetMarked(vWindow->is_zoomed);
           menu->AddItem(zoomItem);
           // Resize to 100%
           BMenuItem *origItem = new BMenuItem("100%", new BMessage(RESIZE_100));
           menu->AddItem(origItem);
           // Resize to 200%
           BMenuItem *doubleItem = new BMenuItem("200%", new BMessage(RESIZE_200));
           menu->AddItem(doubleItem);
           menu->AddSeparatorItem();
           // Toggle vSync
           BMenuItem *vsyncItem = new BMenuItem("Vertical Sync", new BMessage(VERT_SYNC));
           vsyncItem->SetMarked(vWindow->vsync);
           menu->AddItem(vsyncItem);
           menu->AddSeparatorItem();

		   // Windwo Feel Items
		   BMessage *winNormFeel = new BMessage(WINDOW_FEEL);
		   winNormFeel->AddInt16("WinFeel", (int16)B_NORMAL_WINDOW_FEEL);
           BMenuItem *normWindItem = new BMenuItem("Normal Window", winNormFeel);
           normWindItem->SetMarked(vWindow->Feel() == B_NORMAL_WINDOW_FEEL);
           menu->AddItem(normWindItem);
           
		   BMessage *winFloatFeel = new BMessage(WINDOW_FEEL);
		   winFloatFeel->AddInt16("WinFeel", (int16)B_MODAL_ALL_WINDOW_FEEL);
           BMenuItem *onTopWindItem = new BMenuItem("App Top", winFloatFeel);
           onTopWindItem->SetMarked(vWindow->Feel() == B_MODAL_ALL_WINDOW_FEEL);
           menu->AddItem(onTopWindItem);
           
		   BMessage *winAllFeel = new BMessage(WINDOW_FEEL);
		   winAllFeel->AddInt16("WinFeel", (int16)B_FLOATING_ALL_WINDOW_FEEL);
           BMenuItem *allSpacesWindItem = new BMenuItem("On Top All Workspaces", winAllFeel);
           allSpacesWindItem->SetMarked(vWindow->Feel() == B_FLOATING_ALL_WINDOW_FEEL);
           menu->AddItem(allSpacesWindItem);
		   
           menu->SetTargetForItems(this);
           ConvertToScreen(&where);
           menu->Go(where, true, false, true);
        }
	} 
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init       ( vout_thread_t * );
static void End        ( vout_thread_t * );
static int  Manage     ( vout_thread_t * );
static void Display    ( vout_thread_t *, picture_t * );

static int  BeosOpenDisplay ( vout_thread_t *p_vout );
static void BeosCloseDisplay( vout_thread_t *p_vout );

/*****************************************************************************
 * OpenVideo: allocates BeOS video thread output method
 *****************************************************************************
 * This function allocates and initializes a BeOS vout method.
 *****************************************************************************/
int E_(OpenVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = (vout_sys_t*) malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }
    p_vout->p_sys->i_width = p_vout->render.i_width;
    p_vout->p_sys->i_height = p_vout->render.i_height;
    p_vout->p_sys->source_chroma = p_vout->render.i_chroma;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize BeOS video thread output method
 *****************************************************************************/
int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Open and initialize device */
    if( BeosOpenDisplay( p_vout ) )
    {
        msg_Err(p_vout, "vout error: can't open display");
        return 0;
    }
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
    p_vout->output.i_chroma = colspace[p_vout->p_sys->p_window->colspace_index].chroma;
    p_vout->p_sys->i_index = 0;

    p_vout->b_direct = 1;

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
       p_pic->p->p_pixels = (u8*)p_vout->p_sys->p_window->bitmap[buffer_index]->Bits();
       p_pic->p->i_lines = p_vout->p_sys->i_height;

       p_pic->p->i_pixel_pitch = colspace[p_vout->p_sys->p_window->colspace_index].pixel_bytes;
       p_pic->i_planes = colspace[p_vout->p_sys->p_window->colspace_index].planes;
       p_pic->p->i_pitch = p_vout->p_sys->p_window->bitmap[buffer_index]->BytesPerRow(); 
       p_pic->p->i_visible_pitch = p_pic->p->i_pixel_pitch * ( p_vout->p_sys->p_window->bitmap[buffer_index]->Bounds().IntegerWidth() + 1 );

       p_pic->i_status = DESTROYED_PICTURE;
       p_pic->i_type   = DIRECT_PICTURE;
 
       PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

       I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * End: terminate BeOS video thread output method
 *****************************************************************************/
void End( vout_thread_t *p_vout )
{
    BeosCloseDisplay( p_vout );
}

/*****************************************************************************
 * CloseVideo: destroy BeOS video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void E_(CloseVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to BeOS image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;

    /* draw buffer if required */    
    if (!p_win->teardownwindow)
    { 
       p_win->drawBuffer(p_vout->p_sys->i_index);
    }
    /* change buffer */
    p_vout->p_sys->i_index = ++p_vout->p_sys->i_index % 3;
    p_pic->p->p_pixels = (u8*)p_vout->p_sys->p_window->bitmap[p_vout->p_sys->i_index]->Bits();
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
                                                      20 + p_vout->i_window_width - 1, 
                                                      50 + p_vout->i_window_height - 1 ));

    if( p_vout->p_sys->p_window == NULL )
    {
        msg_Err( p_vout, "cannot allocate VideoWindow" );
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
    p_win = NULL;
}

