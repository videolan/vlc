/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: VideoWindow.h,v 1.14 2002/03/31 08:13:38 tcastley Exp $
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
#define DIRECT  1
#define OVERLAY 2
#define OPENGL  3

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
    
    // this is the hook controling direct screen connection
    int32           i_width;     // incomming bitmap size 
    int32           i_height;
    BRect           winSize;     // current window size
//    float           width_scale, height_scale;
//    float           out_top, out_left, out_height, out_width;
    bool            is_zoomed, vsync, is_overlay;
    BBitmap	        *bitmap[2];
    BBitmap         *overlaybitmap;
    VLCView	        *view;
    int             i_buffer;
    bool			teardownwindow;
    thread_id       fDrawThreadID;
    int             mode;

private:
    // display_mode old_mode;
    struct vout_thread_s   *p_vout;

};

 