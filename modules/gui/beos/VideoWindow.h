/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
	uint32_t chroma;
	int planes;
	int pixel_bytes;
} colorcombo;

colorcombo colspace[]=
{
	{B_YCbCr420, "B_YCbCr420", VLC_FOURCC('I','4','2','0'), 3, 2},
	{B_YUV422,   "B_YUV422",   VLC_FOURCC('Y','4','2','2'), 3, 2},
	{B_YCbCr422, "B_YCbCr422", VLC_FOURCC('Y','U','Y','2'), 3, 2},
	{B_RGB32,    "B_RGB32",    VLC_FOURCC('R','V','3','2'), 1, 4},
	{B_RGB16,    "B_RGB16",    VLC_FOURCC('R','V','1','6'), 1, 2}
};

#define COLOR_COUNT 5
#define DEFAULT_COL 3

class VideoSettings
{
 public:
							VideoSettings( const VideoSettings& clone );
	virtual					~VideoSettings();

	static	VideoSettings*	DefaultSettings();

	enum
	{
		SIZE_OTHER			= 0,
		SIZE_50				= 1,
		SIZE_100			= 2,
		SIZE_200			= 3,
	};

			void			SetVideoSize( uint32_t mode );
	inline	uint32_t			VideoSize() const
								{ return fVideoSize; }
	enum
	{
		FLAG_CORRECT_RATIO	= 0x0001,
		FLAG_SYNC_RETRACE	= 0x0002,
		FLAG_ON_TOP_ALL		= 0x0004,
		FLAG_FULL_SCREEN	= 0x0008,
	};

	inline	void			SetFlags( uint32_t flags )
								{ fFlags = flags; }
	inline	void			AddFlags( uint32_t flags )
								{ fFlags |= flags; }
	inline	void			ClearFlags( uint32_t flags )
								{ fFlags &= ~flags; }
	inline	bool			HasFlags( uint32_t flags ) const
								{ return fFlags & flags; }
	inline	uint32_t			Flags() const
								{ return fFlags; }

 private:
							VideoSettings(); // reserved for default settings

	static	VideoSettings	fDefaultSettings;

			uint32_t			fVideoSize;
			uint32_t			fFlags;
			BMessage*		fSettings;
};


class VLCView : public BView
{
 public:
							VLCView( BRect bounds, vout_thread_t *p_vout );
	virtual					~VLCView();

	virtual	void			AttachedToWindow();
	virtual	void			MouseDown(BPoint where);
	virtual	void			MouseUp(BPoint where);
	virtual	void			MouseMoved(BPoint where, uint32 transit,
									   const BMessage* dragMessage);
	virtual	void			Pulse();
	virtual	void			Draw(BRect updateRect);

 private:
            vout_thread_t   *p_vout;

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
										BRect frame,
										vout_thread_t *p_vout);
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
			bool			CorrectAspectRatio() const;
			void			ToggleFullScreen();
			void			SetFullScreen(bool doIt);
			bool			IsFullScreen() const;
			void			SetOnTop(bool doIt);
			bool			IsOnTop() const;
			void			SetSyncToRetrace(bool doIt);
			bool			IsSyncedToRetrace() const;
	inline	status_t		InitCheck() const
								{ return fInitStatus; }


    // this is the hook controling direct screen connection
    int32_t         i_width;     // aspect corrected bitmap size
    int32_t         i_height;
    BRect           winSize;     // current window size
    BBitmap	        *bitmap[3];
//    BBitmap         *overlaybitmap;
    VLCView	        *view;
    int             i_buffer;
	volatile bool	teardownwindow;
    thread_id       fDrawThreadID;
    int             mode;
    int             bitmap_count;
    int             colspace_index;

private:
			status_t		_AllocateBuffers(int width,
											 int height,
											 int* mode);
			void			_FreeBuffers();
			void			_BlankBitmap(BBitmap* bitmap) const;
			void			_SetVideoSize(uint32_t mode);
			void			_SetToSettings();

    vout_thread_t   *p_vout;

	int32_t			fTrueWidth;     // incomming bitmap size
	int32_t			fTrueHeight;
	window_feel		fCachedFeel;
	bool			fInterfaceShowing;
	status_t		fInitStatus;
	VideoSettings*	fSettings;
};

#endif	// BEOS_VIDEO_WINDOW_H

