/*****************************************************************************
 * x11_event.h: X11 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_event.h,v 1.1 2003/04/28 14:32:57 asmax Exp $
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


#ifndef VLC_SKIN_X11_EVENT
#define VLC_SKIN_X11_EVENT

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- X11 ------------------------------------------------------------------
#include <X11/Xlib.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class SkinWindow;

//---------------------------------------------------------------------------
class X11Event : Event
{
    private:
        Window GetWindowFromName( string name );
        Window Wnd;
    public:
        // Constructor
        X11Event( intf_thread_t *p_intf, string Desc, string shortcut );
        X11Event( intf_thread_t *p_intf, Window wnd, unsigned int msg,
                    unsigned int par1, long par2 );
        X11Event( intf_thread_t *p_intf, SkinWindow *win, unsigned int msg,
                    unsigned int par1, long par2 );

        // Destructor
        virtual ~X11Event();

        // Event sending
        virtual bool SendEvent();

        // General operations on events
        virtual void CreateOSEvent( string para1, string para2, string para3 );
        virtual bool IsEqual( Event *evt );

        // Getters
        Window GetWindow()    { return Wnd; }
};
//---------------------------------------------------------------------------

#endif
