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

class VideoWindow : public BDirectWindow
{
public:
    // standard constructor and destructor
    VideoWindow(BRect frame, const char *name, vout_thread_t *p_video_output); 
    ~VideoWindow();

    // standard window member
    virtual bool    QuitRequested();
    virtual void    MessageReceived(BMessage *message);
	
	// this is the hook controling direct screen connection
    virtual void    DirectConnected(direct_buffer_info *info);

    int32           i_bytes_per_pixel;
    int32           i_screen_depth;
    vout_thread_t   *p_vout;
     
    uint8           *fBits;
    int32           fRowBytes;
    color_space     fFormat;
    clipping_rect   fBounds;

    uint32          fNumClipRects;
    clipping_rect   *fClipList;

    bool            fDirty;
    bool            fReady;
    bool            fConnected;
    bool            fConnectionDisabled;
    BLocker         *locker;
    thread_id       fDrawThreadID;
};

class InterfaceWindow : public BWindow
{
public:
    InterfaceWindow( BRect frame, const char *name, intf_thread_t  *p_intf );
    ~InterfaceWindow();

    // standard window member
    virtual bool    QuitRequested();
    virtual void    MessageReceived(BMessage *message);
    
    intf_thread_t  *p_interface;
};

class InterfaceView : public BView
{
public:
    InterfaceView();
    ~InterfaceView();

    virtual void    MessageReceived(BMessage *message);
   
};
