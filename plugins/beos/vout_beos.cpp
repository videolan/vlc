/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_beos.cpp,v 1.58.2.5 2002/09/03 12:00:24 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
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
#include "MsgVals.h"

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

//    u8 *pp_buffer[3];
    u32 source_chroma;
    int i_index;

} vout_sys_t;

#define MOUSE_IDLE_TIMEOUT 2000000	// two seconds
#define MIN_AUTO_VSYNC_REFRESH 61	// Hz

/*****************************************************************************
 * beos_GetAppWindow : retrieve a BWindow pointer from the window name
 *****************************************************************************/
BWindow*
beos_GetAppWindow(char *name)
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
 * get_interface_window
 *****************************************************************************/
BWindow*
get_interface_window()
{
	return beos_GetAppWindow(VOUT_TITLE);
}

class BackgroundView : public BView
{
 public:
							BackgroundView(BRect frame, VLCView* view)
							: BView(frame, "background",
									B_FOLLOW_ALL, B_FULL_UPDATE_ON_RESIZE),
							  fVideoView(view)
							{
								SetViewColor(kBlack);
							}
	virtual					~BackgroundView() {}

	virtual	void			MouseDown(BPoint where)
							{
								// convert coordinates
								where = fVideoView->ConvertFromParent(where);
								// let him handle it
								fVideoView->MouseDown(where);
							}
	virtual	void			MouseMoved(BPoint where, uint32 transit,
									   const BMessage* dragMessage)
							{
								// convert coordinates
								where = fVideoView->ConvertFromParent(where);
								// let him handle it
								fVideoView->MouseMoved(where, transit, dragMessage);
								// notice: It might look like transit should be
								// B_OUTSIDE_VIEW regardless, but leave it like this,
								// otherwise, unwanted things will happen!
							}

 private:
	VLCView*				fVideoView;
};

/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/
VideoWindow::VideoWindow(int v_width, int v_height, BRect frame)
	: BWindow(frame, NULL, B_TITLED_WINDOW, B_NOT_CLOSABLE | B_NOT_MINIMIZABLE),
	  i_width(frame.IntegerWidth()),
	  i_height(frame.IntegerHeight()),
	  is_zoomed(false),
	  vsync(false),
	  i_buffer(0),
	  teardownwindow(false),
	  fTrueWidth(v_width),
	  fTrueHeight(v_height),
	  fCorrectAspect(true),
	  fCachedFeel(B_NORMAL_WINDOW_FEEL),
	  fInterfaceShowing(false),
	  fInitStatus(B_ERROR)
{
    // create the view to do the display
    view = new VLCView( Bounds() );

	// create background view
    BView *mainView =  new BackgroundView( Bounds(), view );
    AddChild(mainView);
    mainView->AddChild(view);

	// figure out if we should use vertical sync by default
	BScreen screen(this);
	if (screen.IsValid())
	{
		display_mode mode; 
		screen.GetMode(&mode); 
		float refresh = (mode.timing.pixel_clock * 1000)
						/ ((mode.timing.h_total)* (mode.timing.v_total)); 
		vsync = (refresh < MIN_AUTO_VSYNC_REFRESH);
	}

	// allocate bitmap buffers
	for (int32 i = 0; i < 3; i++)
		bitmap[i] = NULL;
	fInitStatus = _AllocateBuffers(v_width, v_height, &mode);

	// make sure we layout the view correctly
    FrameResized(i_width, i_height);

    if (fInitStatus >= B_OK && mode == OVERLAY)
    {
       overlay_restrictions r;

       bitmap[1]->GetOverlayRestrictions(&r);
       SetSizeLimits((i_width * r.min_width_scale), i_width * r.max_width_scale,
                     (i_height * r.min_height_scale), i_height * r.max_height_scale);
    }
}

VideoWindow::~VideoWindow()
{
    int32 result;

    teardownwindow = true;
    wait_for_thread(fDrawThreadID, &result);
    _FreeBuffers();
}

/*****************************************************************************
 * VideoWindow::MessageReceived
 *****************************************************************************/
void
VideoWindow::MessageReceived( BMessage *p_message )
{
	switch( p_message->what )
	{
		case TOGGLE_FULL_SCREEN:
			BWindow::Zoom();
			break;
		case RESIZE_100:
		case RESIZE_200:
			if (is_zoomed)
				BWindow::Zoom();
			_SetVideoSize(p_message->what);
			break;
		case VERT_SYNC:
			vsync = !vsync;
			break;
		case WINDOW_FEEL:
			{
				window_feel winFeel;
				if (p_message->FindInt32("WinFeel", (int32*)&winFeel) == B_OK)
				{
					SetFeel(winFeel);
					fCachedFeel = winFeel;
				}
			}
			break;
		case ASPECT_CORRECT:
			SetCorrectAspectRatio(!fCorrectAspect);
			break;
		default:
			BWindow::MessageReceived( p_message );
			break;
	}
}

/*****************************************************************************
 * VideoWindow::Zoom
 *****************************************************************************/
void
VideoWindow::Zoom(BPoint origin, float width, float height )
{
	if(is_zoomed)
	{
		MoveTo(winSize.left, winSize.top);
		ResizeTo(winSize.IntegerWidth(), winSize.IntegerHeight());
		be_app->ShowCursor();
		fInterfaceShowing = true;
	}
	else
	{
		BScreen screen(this);
		BRect rect = screen.Frame();
		Activate();
		MoveTo(0.0, 0.0);
		ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
		be_app->ObscureCursor();
		fInterfaceShowing = false;
	}
	is_zoomed = !is_zoomed;
}

/*****************************************************************************
 * VideoWindow::FrameMoved
 *****************************************************************************/
void
VideoWindow::FrameMoved(BPoint origin) 
{
	if (is_zoomed) return ;
    winSize = Frame();
}

/*****************************************************************************
 * VideoWindow::FrameResized
 *****************************************************************************/
void
VideoWindow::FrameResized( float width, float height )
{
	int32 useWidth = fCorrectAspect ? i_width : fTrueWidth;
	int32 useHeight = fCorrectAspect ? i_height : fTrueHeight;
    float out_width, out_height;
    float out_left, out_top;
    float width_scale = width / useWidth;
    float height_scale = height / useHeight;

    if (width_scale <= height_scale)
    {
        out_width = (useWidth * width_scale);
        out_height = (useHeight * width_scale);
        out_left = 0; 
        out_top = (height - out_height) / 2;
    }
    else   /* if the height is proportionally smaller */
    {
        out_width = (useWidth * height_scale);
        out_height = (useHeight * height_scale);
        out_top = 0;
        out_left = (width - out_width) / 2;
    }
    view->MoveTo(out_left,out_top);
    view->ResizeTo(out_width, out_height);

	if (!is_zoomed)
        winSize = Frame();
}

/*****************************************************************************
 * VideoWindow::ScreenChanged
 *****************************************************************************/
void
VideoWindow::ScreenChanged(BRect frame, color_space format)
{
	BScreen screen(this);
	display_mode mode; 
	screen.GetMode(&mode); 
	float refresh = (mode.timing.pixel_clock * 1000)
					/ ((mode.timing.h_total) * (mode.timing.v_total)); 
    if (refresh < MIN_AUTO_VSYNC_REFRESH) 
        vsync = true; 
}

/*****************************************************************************
 * VideoWindow::Activate
 *****************************************************************************/
void
VideoWindow::WindowActivated(bool active)
{
}

/*****************************************************************************
 * VideoWindow::drawBuffer
 *****************************************************************************/
void
VideoWindow::drawBuffer(int bufferIndex)
{
    i_buffer = bufferIndex;

    // sync to the screen if required
    if (vsync)
    {
        BScreen screen(this);
        screen.WaitForRetrace(22000);
    }
    if (fInitStatus >= B_OK && LockLooper())
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
         view->DrawBitmap(bitmap[i_buffer], view->Bounds() );
       }
       UnlockLooper();
    }
}

/*****************************************************************************
 * VideoWindow::SetInterfaceShowing
 *****************************************************************************/
void
VideoWindow::ToggleInterfaceShowing()
{
	SetInterfaceShowing(!fInterfaceShowing);
}

/*****************************************************************************
 * VideoWindow::SetInterfaceShowing
 *****************************************************************************/
void
VideoWindow::SetInterfaceShowing(bool showIt)
{
	BWindow* window = get_interface_window();
	if (window)
	{
		if (showIt)
		{
			if (fCachedFeel != B_NORMAL_WINDOW_FEEL)
				SetFeel(B_NORMAL_WINDOW_FEEL);
			window->Activate(true);
			SendBehind(window);
		}
		else
		{
			SetFeel(fCachedFeel);
			Activate(true);
			window->SendBehind(this);
		}
		fInterfaceShowing = showIt;
	}
}

/*****************************************************************************
 * VideoWindow::SetCorrectAspectRatio
 *****************************************************************************/
void
VideoWindow::SetCorrectAspectRatio(bool doIt)
{
	if (fCorrectAspect != doIt)
	{
		fCorrectAspect = doIt;
		FrameResized(Bounds().Width(), Bounds().Height());
	}
}

/*****************************************************************************
 * VideoWindow::_AllocateBuffers
 *****************************************************************************/
status_t
VideoWindow::_AllocateBuffers(int width, int height, int* mode)
{
	// clear any old buffers
	_FreeBuffers();
	// set default mode
	*mode = BITMAP;

	BRect bitmapFrame( 0, 0, width, height );
	// read from config, if we are supposed to use overlay at all
    int noOverlay = !config_GetIntVariable( "overlay" );
	// test for overlay capability
    for (int i = 0; i < COLOR_COUNT; i++)
    {
        if (noOverlay) break;
        bitmap[0] = new BBitmap ( bitmapFrame, 
                                  B_BITMAP_WILL_OVERLAY,
                                  colspace[i].colspace);

        if(bitmap[0] && bitmap[0]->InitCheck() == B_OK) 
        {
            colspace_index = i;

            bitmap[1] = new BBitmap( bitmapFrame, B_BITMAP_WILL_OVERLAY,
                                     colspace[colspace_index].colspace);
            bitmap[2] = new BBitmap( bitmapFrame, B_BITMAP_WILL_OVERLAY,
                                     colspace[colspace_index].colspace);
            if ( (bitmap[2] && bitmap[2]->InitCheck() == B_OK) )
            {
               *mode = OVERLAY;
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
               _FreeBuffers();
               *mode = BITMAP; // might want to try again with normal bitmaps
            }
        }
        else
            delete bitmap[0];
	}

    if (*mode == BITMAP)
	{
        // fallback to RGB32
        colspace_index = DEFAULT_COL;
        SetTitle(VOUT_TITLE " (Bitmap)");
        bitmap[0] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace);
        bitmap[1] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace);
        bitmap[2] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace);
    }
    // see if everything went well
    status_t status = B_ERROR;
    for (int32 i = 0; i < 3; i++)
    {
    	if (bitmap[i])
    		status = bitmap[i]->InitCheck();
		if (status < B_OK)
			break;
    }
    if (status >= B_OK)
    {
	    // clear bitmaps to black
	    for (int32 i = 0; i < 3; i++)
	    	_BlankBitmap(bitmap[i]);
    }
    return status;
}

/*****************************************************************************
 * VideoWindow::_FreeBuffers
 *****************************************************************************/
void
VideoWindow::_FreeBuffers()
{
	delete bitmap[0];
	bitmap[0] = NULL;
	delete bitmap[1];
	bitmap[1] = NULL;
	delete bitmap[2];
	bitmap[2] = NULL;
	fInitStatus = B_ERROR;
}

/*****************************************************************************
 * VideoWindow::_BlankBitmap
 *****************************************************************************/
void
VideoWindow::_BlankBitmap(BBitmap* bitmap) const
{
	// no error checking (we do that earlier on and since it's a private function...

	// YCbCr: 
	// Loss/Saturation points are Y 16-235 (absoulte); Cb/Cr 16-240 (center 128)

	// YUV: 
	// Extrema points are Y 0 - 207 (absolute) U -91 - 91 (offset 128) V -127 - 127 (offset 128)

	// we only handle weird colorspaces with special care
	switch (bitmap->ColorSpace()) {
		case B_YCbCr422: {
			// Y0[7:0]  Cb0[7:0]  Y1[7:0]  Cr0[7:0]  Y2[7:0]  Cb2[7:0]  Y3[7:0]  Cr2[7:0]
			int32 height = bitmap->Bounds().IntegerHeight() + 1;
			uint8* bits = (uint8*)bitmap->Bits();
			int32 bpr = bitmap->BytesPerRow();
			for (int32 y = 0; y < height; y++) {
				// handle 2 bytes at a time
				for (int32 i = 0; i < bpr; i += 2) {
					// offset into line
					bits[i] = 16;
					bits[i + 1] = 128;
				}
				// next line
				bits += bpr;
			}
			break;
		}
		case B_YCbCr420: {
// TODO: untested!!
			// Non-interlaced only, Cb0  Y0  Y1  Cb2 Y2  Y3  on even scan lines ...
			// Cr0  Y0  Y1  Cr2 Y2  Y3  on odd scan lines
			int32 height = bitmap->Bounds().IntegerHeight() + 1;
			uint8* bits = (uint8*)bitmap->Bits();
			int32 bpr = bitmap->BytesPerRow();
			for (int32 y = 0; y < height; y += 1) {
				// handle 3 bytes at a time
				for (int32 i = 0; i < bpr; i += 3) {
					// offset into line
					bits[i] = 128;
					bits[i + 1] = 16;
					bits[i + 2] = 16;
				}
				// next line
				bits += bpr;
			}
			break;
		}
		case B_YUV422: {
// TODO: untested!!
			// U0[7:0]  Y0[7:0]   V0[7:0]  Y1[7:0]  U2[7:0]  Y2[7:0]   V2[7:0]  Y3[7:0]
			int32 height = bitmap->Bounds().IntegerHeight() + 1;
			uint8* bits = (uint8*)bitmap->Bits();
			int32 bpr = bitmap->BytesPerRow();
			for (int32 y = 0; y < height; y += 1) {
				// handle 2 bytes at a time
				for (int32 i = 0; i < bpr; i += 2) {
					// offset into line
					bits[i] = 128;
					bits[i + 1] = 0;
				}
				// next line
				bits += bpr;
			}
			break;
		}
		default:
			memset(bitmap->Bits(), 0, bitmap->BitsLength());
			break;
	}
}

/*****************************************************************************
 * VideoWindow::_SetVideoSize
 *****************************************************************************/
void
VideoWindow::_SetVideoSize(uint32 mode)
{
	// let size depend on aspect correction
	int32 width = fCorrectAspect ? i_width : fTrueWidth;
	int32 height = fCorrectAspect ? i_height : fTrueHeight;
	switch (mode)
	{
		case RESIZE_200:
			width *= 2;
			height *= 2;
			break;
		case RESIZE_100:
		default:
	        break;
	}
	ResizeTo(width, height);
	is_zoomed = false;
}



/*****************************************************************************
 * VLCView::VLCView
 *****************************************************************************/
VLCView::VLCView(BRect bounds)
	: BView(bounds, "video view", B_FOLLOW_NONE, B_WILL_DRAW | B_PULSE_NEEDED),
	  fLastMouseMovedTime(system_time()),
	  fCursorHidden(false),
	  fCursorInside(false)
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
 * VLCVIew::AttachedToWindow
 *****************************************************************************/
void
VLCView::AttachedToWindow()
{
	// in order to get keyboard events
	MakeFocus(true);
	// periodically check if we want to hide the pointer
	Window()->SetPulseRate(1000000);
}

/*****************************************************************************
 * VLCVIew::MouseDown
 *****************************************************************************/
void
VLCView::MouseDown(BPoint where)
{
	VideoWindow* videoWindow = dynamic_cast<VideoWindow*>(Window());
	BMessage* msg = Window()->CurrentMessage();
	int32 clicks;
	uint32 buttons;
	msg->FindInt32("clicks", &clicks);
	msg->FindInt32("buttons", (int32*)&buttons);

	if (videoWindow)
	{
		if (buttons & B_PRIMARY_MOUSE_BUTTON)
		{
			if (clicks == 2)
				Window()->Zoom();
			else
				videoWindow->ToggleInterfaceShowing();
		}
	    else
	    {
			if (buttons & B_SECONDARY_MOUSE_BUTTON) 
			{
				BPopUpMenu *menu = new BPopUpMenu("context menu");
				menu->SetRadioMode(false);
				// Toggle FullScreen
				BMenuItem *zoomItem = new BMenuItem("Fullscreen", new BMessage(TOGGLE_FULL_SCREEN));
				zoomItem->SetMarked(videoWindow->is_zoomed);
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
				vsyncItem->SetMarked(videoWindow->vsync);
				menu->AddItem(vsyncItem);
				// Correct Aspect Ratio
				BMenuItem *aspectItem = new BMenuItem("Correct Aspect Ratio", new BMessage(ASPECT_CORRECT));
				aspectItem->SetMarked(videoWindow->CorrectAspectRatio());
				menu->AddItem(aspectItem);
	
				menu->AddSeparatorItem();
	
				// Windwo Feel Items
				BMessage *winNormFeel = new BMessage(WINDOW_FEEL);
				winNormFeel->AddInt32("WinFeel", (int32)B_NORMAL_WINDOW_FEEL);
				BMenuItem *normWindItem = new BMenuItem("Normal Window", winNormFeel);
				normWindItem->SetMarked(videoWindow->Feel() == B_NORMAL_WINDOW_FEEL);
				menu->AddItem(normWindItem);
				
				BMessage *winFloatFeel = new BMessage(WINDOW_FEEL);
				winFloatFeel->AddInt32("WinFeel", (int32)B_FLOATING_APP_WINDOW_FEEL);
				BMenuItem *onTopWindItem = new BMenuItem("App Top", winFloatFeel);
				onTopWindItem->SetMarked(videoWindow->Feel() == B_FLOATING_APP_WINDOW_FEEL);
				menu->AddItem(onTopWindItem);
				
				BMessage *winAllFeel = new BMessage(WINDOW_FEEL);
				winAllFeel->AddInt32("WinFeel", (int32)B_FLOATING_ALL_WINDOW_FEEL);
				BMenuItem *allSpacesWindItem = new BMenuItem("On Top All Workspaces", winAllFeel);
				allSpacesWindItem->SetMarked(videoWindow->Feel() == B_FLOATING_ALL_WINDOW_FEEL);
				menu->AddItem(allSpacesWindItem);
			   
				menu->SetTargetForItems(this);
				ConvertToScreen(&where);
				menu->Go(where, true, false, true);
	        }
		}
	}
	fLastMouseMovedTime = system_time();
	fCursorHidden = false;
}

/*****************************************************************************
 * VLCVIew::MouseMoved
 *****************************************************************************/
void
VLCView::MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage)
{
	fLastMouseMovedTime = system_time();
	fCursorHidden = false;
	fCursorInside = (transit == B_INSIDE_VIEW || transit == B_ENTERED_VIEW);
}

/*****************************************************************************
 * VLCVIew::Pulse
 *****************************************************************************/
void 
VLCView::Pulse()
{
	// We are getting the pulse messages no matter if the mouse is over
	// this view. If we are in full screen mode, we want to hide the cursor
	// even if it is not.
	if (!fCursorHidden) {
	    VideoWindow *videoWindow = dynamic_cast<VideoWindow*>(Window());
		if (fCursorInside
			&& system_time() - fLastMouseMovedTime > MOUSE_IDLE_TIMEOUT) {
			be_app->ObscureCursor();
			fCursorHidden = true;
			// hide the interface window as well
			videoWindow->SetInterfaceShowing(false);
		}
	}
}

/*****************************************************************************
 * VLCVIew::KeyDown
 *****************************************************************************/
void
VLCView::KeyDown(const char *bytes, int32 numBytes)
{
    VideoWindow *videoWindow = dynamic_cast<VideoWindow*>(Window());
    BWindow* interfaceWindow = get_interface_window();
	if (videoWindow && numBytes > 0) {
		uint32 mods = modifiers();
		switch (*bytes) {
			case B_TAB:
				// toggle window and full screen mode
				// not passing on the tab key to the default KeyDown()
				// implementation also avoids loosing the keyboard focus
				videoWindow->PostMessage(TOGGLE_FULL_SCREEN);
				break;
			case B_ESCAPE:
				// go back to window mode
				if (videoWindow->is_zoomed)
					videoWindow->PostMessage(TOGGLE_FULL_SCREEN);
				break;
			case B_SPACE:
				// toggle playback
				if (interfaceWindow)
					interfaceWindow->PostMessage(PAUSE_PLAYBACK);
				break;
			case B_RIGHT_ARROW:
				if (interfaceWindow)
				{
					if (mods & B_SHIFT_KEY)
						// next title
						interfaceWindow->PostMessage(NEXT_CHAPTER);
					else
						// next chapter
						interfaceWindow->PostMessage(NEXT_TITLE);
				}
				break;
			case B_LEFT_ARROW:
				if (interfaceWindow)
				{
					if (mods & B_SHIFT_KEY)
						// previous title
						interfaceWindow->PostMessage(PREV_CHAPTER);
					else
						// previous chapter
						interfaceWindow->PostMessage(PREV_TITLE);
				}
				break;
			case B_UP_ARROW:
				// previous file in playlist?
				break;
			case B_DOWN_ARROW:
				// next file in playlist?
				break;
			default:
				BView::KeyDown(bytes, numBytes);
				break;
		}
	}
}

/*****************************************************************************
 * VLCVIew::Draw
 *****************************************************************************/
void
VLCView::Draw(BRect updateRect) 
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
    p_vout->p_sys->source_chroma = p_vout->render.i_chroma;

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

       p_pic->p->i_pixel_bytes = colspace[p_vout->p_sys->p_window->colspace_index].pixel_bytes;
       p_pic->i_planes = colspace[p_vout->p_sys->p_window->colspace_index].planes;
       p_pic->p->i_pitch = p_vout->p_sys->p_window->bitmap[buffer_index]->BytesPerRow(); 

       if (p_vout->p_sys->p_window->mode == OVERLAY)
       {
          p_pic->p->i_visible_bytes = (p_vout->p_sys->p_window->bitmap[buffer_index]->Bounds().IntegerWidth()+1) 
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

    if( p_vout->p_sys->p_window->InitCheck() < B_OK)
    {
    	delete p_vout->p_sys->p_window;
    	p_vout->p_sys->p_window = NULL;
        intf_ErrMsg( "error: cannot allocate memory for VideoWindow" );
        return( 1 );
    } else
    	p_vout->p_sys->p_window->Show();
    
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



} /* extern "C" */
