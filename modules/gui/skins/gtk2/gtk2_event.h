/*****************************************************************************
 * gtk2_event.h: GTK2 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_event.h,v 1.2 2003/04/21 21:51:16 asmax Exp $
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


#ifndef VLC_SKIN_GTK2_EVENT
#define VLC_SKIN_GTK2_EVENT

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- GTK2 ------------------------------------------------------------------
#include <gdk/gdk.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class SkinWindow;

//---------------------------------------------------------------------------
class GTK2Event : Event
{
    private:
        GdkWindow *GetWindowFromName( string name );
        GdkWindow *gWnd;
    public:
        // Constructor
        GTK2Event( intf_thread_t *p_intf, string Desc, string shortcut );
        GTK2Event( intf_thread_t *p_intf, GdkWindow *gwnd, unsigned int msg,
                    unsigned int par1, long par2 );
        GTK2Event( intf_thread_t *p_intf, SkinWindow *win, unsigned int msg,
                    unsigned int par1, long par2 );

        // Destructor
        virtual ~GTK2Event();

        // Event sending
        virtual bool SendEvent();

        // General operations on events
        virtual void CreateOSEvent( string para1, string para2, string para3 );
        virtual bool IsEqual( Event *evt );

        // Getters
        GdkWindow *GetWindow()    { return gWnd; }
};
//---------------------------------------------------------------------------

#endif
