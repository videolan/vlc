/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: VideoWindow.h,v 1.19.2.6 2002/09/29 12:04:27 titer Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
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

#ifndef BEOS_VIDEO_WINDOW_H
#define BEOS_VIDEO_WINDOW_H

#include <View.h>
#include <Window.h>

#define BITMAP  0
#define OVERLAY 1
#define OPENGL  2

typedef struct colorcombo
{
	color_space colspace;
	const char *name;
	u32 chroma;
	int planes;
	int pixel_bytes;
} colorcombo;

colorcombo colspace[]=
{
	{B_YCbCr420, "B_YCbCr420", FOURCC_I420, 3, 2},
	{B_YUV422,   "B_YUV422",   FOURCC_Y422, 3, 2},
	{B_YCbCr422, "B_YCbCr422", FOURCC_YUY2, 3, 2},
	{B_RGB32,    "B_RGB32",    FOURCC_RV32, 1, 4},
	{B_RGB16,    "B_RGB16",    FOURCC_RV16, 1, 2}
};

#define COLOR_COUNT 5
#define DEFAULT_COL 4


class VLCView : public BView
{
 public:
							VLCView( BRect bounds);
	virtual					~VLCView();

	virtual	void			AttachedToWindow();
	virtual	void			MouseDown(BPoint where);
	virtual	void			MouseMoved(BPoint where, uint32 transit,
									   const BMessage* dragMessage);
	virtual	void			Pulse();
	virtual	void			Draw(BRect updateRect);
	virtual	void			KeyDown(const char* bytes, int32 numBytes);

 private:
			bigtime_t		fLastMouseMovedTime;
			bool			fCursorHidden;
			bool			fCursorInside;
			bool			fIgnoreDoubleClick;
};


class VideoWindow : public BWindow
{
public:
							VideoWindow(int v_width, 
										int v_height,
										BRect frame); 
	virtual					~VideoWindow();

							// BWindow    
	virtual	void			MessageReceived(BMessage* message);
	virtual	void			Zoom(BPoint origin,
								 float width, float height);
	virtual	void			FrameResized(float width, float height);
	virtual	void			FrameMoved(BPoint origin);
	virtual	void			ScreenChanged(BRect frame,
										  color_space mode);
	virtual	void			WindowActivated(bool active);

							// VideoWindow
			void			drawBuffer(int bufferIndex);

			void			ToggleInterfaceShowing();
			void			SetInterfaceShowing(bool showIt);

			void			SetCorrectAspectRatio(bool doIt);
	inline	bool			CorrectAspectRatio() const
								{ return fCorrectAspect; }
	inline	status_t		InitCheck() const
								{ return fInitStatus; }


    // this is the hook controling direct screen connection
    int32           i_width;     // aspect corrected bitmap size 
    int32           i_height;
    BRect           winSize;     // current window size
    bool            is_zoomed, vsync;
    BBitmap	        *bitmap[3];
    BBitmap         *overlaybitmap;
    VLCView	        *view;
    int             i_buffer;
	volatile bool	teardownwindow;
    thread_id       fDrawThreadID;
    int             mode;
    int             colspace_index;

private:
			status_t		_AllocateBuffers(int width,
											 int height,
											 int* mode);
			void			_FreeBuffers();
			void			_BlankBitmap(BBitmap* bitmap) const;
			void			_SetVideoSize(uint32 mode);

			void			_SaveScreenShot( BBitmap* bitmap,
											 char* path,
											 uint32 translatorID ) const;
	static	int32			_save_screen_shot( void* cookie );

	struct screen_shot_info
	{
		BBitmap*	bitmap;
		char*		path;
		uint32		translatorID;
		int32		width;
		int32		height;
	};

    struct vout_thread_s   *p_vout;

			int32			fTrueWidth;     // incomming bitmap size 
			int32			fTrueHeight;
			bool			fCorrectAspect;
			window_feel		fCachedFeel;
			bool			fInterfaceShowing;
			status_t		fInitStatus;
};

#endif	// BEOS_VIDEO_WINDOW_H
 
