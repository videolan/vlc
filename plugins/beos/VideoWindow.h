/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: VideoWindow.h,v 1.20 2002/06/01 08:54:08 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
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
#define DEFAULT_COL 3


class VLCView : public BView
{
public:
	VLCView( BRect bounds);
	~VLCView();
	
	void MouseDown(BPoint point);
	void Draw(BRect updateRect);
};


class VideoWindow : public BWindow
{
public:
    // standard constructor and destructor
    VideoWindow( int v_width, int v_height,
                 BRect frame); 
    ~VideoWindow();
    
    void	        Zoom(BPoint origin, float width, float height);
    void            FrameResized(float width, float height);
    void            FrameMoved(BPoint origin);
    void            ScreenChanged(BRect frame, color_space mode);
    void            drawBuffer(int bufferIndex);
    void            WindowActivated(bool active);
    int             SelectDrawingMode(int width, int height);
    bool            QuitRequested();
    
    // this is the hook controling direct screen connection
    int32           i_width;     // incomming bitmap size 
    int32           i_height;
    BRect           winSize;     // current window size
    bool            is_zoomed, vsync;
    BBitmap	        *bitmap[3];
    BBitmap         *overlaybitmap;
    VLCView	        *view;
    int             i_buffer;
    bool			teardownwindow;
    thread_id       fDrawThreadID;
    int             mode;
    int             colspace_index;

private:
    struct vout_thread_s   *p_vout;

};

 
