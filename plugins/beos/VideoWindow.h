/*****************************************************************************
 * VideoWindow.h: BeOS video window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: VideoWindow.h,v 1.10 2002/03/13 08:39:39 tcastley Exp $
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
class VLCView : public BView
{
public:
	VLCView( BRect bounds);
	~VLCView();
	
	void MouseDown(BPoint point);
};


class VideoWindow 
{
public:
    // standard constructor and destructor
    VideoWindow( int width, int height,
                 struct vout_thread_s *p_video_output); 
    ~VideoWindow();
    
    void resizeIfRequired(int newWidth, int newHeight);
    void drawBuffer(int bufferIndex);
    
    // this is the hook controling direct screen connection
    int32           i_width;     // incomming bitmap size 
    int32           i_height;
    float           f_w_width;   // current window size
    float           f_w_height;
    bool            resized;
    bool            is_zoomed;
    BBitmap	        *bitmap[2];
    VLCView	        *view;
    BWindow         *voutWindow;
    int             i_buffer;
    bool			teardownwindow;

private:
    // display_mode old_mode;
    thread_id              fDrawThreadID;
    struct vout_thread_s   *p_vout;

};

class bitmapWindow : public BWindow
{
public:
    bitmapWindow(BRect frame, VideoWindow *theOwner);
    ~bitmapWindow();
    // standard window member
    virtual void    FrameResized(float width, float height);
    virtual void	Zoom(BPoint origin, float width, float height);
private:
    bool            is_zoomed;
	BRect           origRect;
	VideoWindow     *owner;
};

 
class directWindow : public BDirectWindow
{
public:
    // standard constructor and destructor
    directWindow( BRect frame, VideoWindow *theOwner); 
    ~directWindow();

    // standard window member
    virtual void    FrameResized(float width, float height);
    virtual void	Zoom(BPoint origin, float width, float height);
    virtual void    DirectConnected(direct_buffer_info *info);
private:
    bool            is_zoomed;
	BRect           origRect;
    VideoWindow     *owner;
};
