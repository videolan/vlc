/*****************************************************************************
 * beos_window.h: beos window class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 * Jean-Marc Dressler
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
 
    struct vout_thread_s   *p_vout;
    BView * p_view;
     
    // additional events
    bool            b_resized;
};

class InterfaceWindow : public BWindow
{
public:
    InterfaceWindow( BRect frame, const char *name, intf_thread_t  *p_intf );
    ~InterfaceWindow();

    // standard window member
    virtual bool    QuitRequested();
    virtual void    MessageReceived(BMessage *message);
    
    intf_thread_t  *p_intf;
};

class InterfaceView : public BView
{
public:
    InterfaceView();
    ~InterfaceView();

    virtual void    MessageReceived(BMessage *message);
   
};
