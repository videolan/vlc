/*****************************************************************************
 * x11_window.h: X11 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_window.h,v 1.3 2003/06/07 00:36:28 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


#ifndef VLC_SKIN_X11_WIN
#define VLC_SKIN_X11_WIN

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//---------------------------------------------------------------------------
class Graphics;
class Event;

//---------------------------------------------------------------------------
class X11Window : public SkinWindow
{
    private:
        // General parameters
        Window Wnd;
        Display *display;
        GC Gc;
        int CursorX;
        int CursorY;
        int WindowX;
        int WindowY;
        string Name;

        // Drag&Drop
        X11DropObject *DropObject;

        // Tooltip texts
        Window ToolTipWindow;
//        TOOLINFO ToolTipInfo;

        // Double-click handling
        int ClickedX;
        int ClickedY;
        int ClickedTime;
        int DblClickDelay;

        // Left button down
        bool LButtonDown;
        // Right button down
        bool RButtonDown;

    public:
        // Cosntructors
        X11Window( intf_thread_t *_p_intf, Window wnd, int x, int y,
            bool visible, int transition, int normalalpha, int movealpha,
            bool dragdrop, string name );

        // Destructors
        virtual ~X11Window();

        // Event processing
        virtual bool ProcessOSEvent( Event *evt );

        // Window graphic aspect
        virtual void OSShow( bool show );
        virtual void RefreshFromImage( int x, int y, int w, int h );
        virtual void SetTransparency( int Value = -1 );
        virtual void WindowManualMove();
        virtual void WindowManualMoveInit();

        // Window methods
        virtual void Move( int left, int top );
        virtual void Size( int width, int height );

        // Specific X11 methods
        Window GetHandle() { return Wnd; };

        // Tooltip texts
        virtual void ChangeToolTipText( string text );

        // Getters
        string GetName() { return Name; }
};
//---------------------------------------------------------------------------

#endif
