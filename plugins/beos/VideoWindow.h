/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: VideoWindow.h,v 1.7 2001/10/21 06:06:20 tcastley Exp $
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
#include <Slider.h>
#include <Accelerant.h>
#include <Bitmap.h>

class VLCView : public BView
{
public:
	VLCView( BRect bounds);
	~VLCView();
	
	virtual void MouseDown(BPoint point);
};


class VideoWindow : public BWindow
{
public:
    // standard constructor and destructor
    VideoWindow( BRect frame, const char *name,
                 struct vout_thread_s *p_video_output); 
    ~VideoWindow();

    // standard window member
    virtual bool    QuitRequested();
    virtual void    FrameResized(float width, float height);
    virtual void    MessageReceived(BMessage *message);
    virtual void	Zoom(BPoint origin, float width, float height);

    // this is the hook controling direct screen connection
    int32           i_bytes_per_pixel;
    int32           i_screen_depth;
    int32           i_width;
    int32           i_height;
    int32           fRowBytes;
    int				i_buffer_index;

    BBitmap			*bitmap[2];
    VLCView			*view;
    thread_id       fDrawThreadID;

    bool			teardownwindow;
    bool			is_zoomed;

    struct vout_thread_s   *p_vout;

private:
//	display_mode old_mode;
	BRect           rect;

};

