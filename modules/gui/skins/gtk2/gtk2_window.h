/*****************************************************************************
 * gtk2_window.h: GTK2 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_window.h,v 1.5 2003/04/15 20:33:58 karibu Exp $
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


#ifndef VLC_SKIN_GTK2_WIN
#define VLC_SKIN_GTK2_WIN

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//---------------------------------------------------------------------------
class Graphics;
class Event;

//---------------------------------------------------------------------------
class GTK2Window : public Window
{
    private:
        // General parameters
        GdkWindow *gWnd;
        GdkGC *gc;
        int CursorX;
        int CursorY;
        int WindowX;
        int WindowY;
        string Name;

        // Tooltip texts
        GdkWindow *ToolTipWindow;
//        TOOLINFO ToolTipInfo;

        // Drag & drop
//        LPDROPTARGET DropTarget;

        // Left button down
        bool LButtonDown;
        bool RButtonDown;

    public:
        // Cosntructors
        GTK2Window( intf_thread_t *_p_intf, GdkWindow *gwnd, int x, int y,
            bool visible, int transition, int normalalpha, int movealpha,
            bool dragdrop, string name );

        // Destructors
        virtual ~GTK2Window();

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

        // Specific gtk2 methods
        GdkWindow *GetHandle() { return gWnd; };

        // Tooltip texts
        virtual void ChangeToolTipText( string text );

        // Getters
        string GetName() { return Name; }
};
//---------------------------------------------------------------------------

#endif
