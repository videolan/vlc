/*****************************************************************************
 * x11_dragdrop.cpp: X11 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_dragdrop.cpp,v 1.4 2003/06/09 00:32:58 asmax Exp $
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

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>
#include <X11/Xatom.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/skin_common.h"
#include "../src/event.h"
#include "../os_api.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "x11_dragdrop.h"

#include <string.h>


//---------------------------------------------------------------------------
X11DropObject::X11DropObject( intf_thread_t *_p_intf, Window win)
{
    p_intf = _p_intf;
    Win = win;
    display = p_intf->p_sys->display;
}
//---------------------------------------------------------------------------
X11DropObject::~X11DropObject()
{
}
//---------------------------------------------------------------------------
void X11DropObject::DndEnter( ldata_t data )
{
    Window src = data[0];

    // Retrieve available data types
    list<string> dataTypes;
    if( data[1] & 1 )   // more than 3 data types ?
    {
        Atom type;
        int format;
        unsigned long nitems, nbytes;
        Atom *dataList;
        XLOCK;
        Atom typeListAtom = XInternAtom( display, "XdndTypeList", 0 );
        XGetWindowProperty( display, src, typeListAtom, 0, 65536, False,
                            XA_ATOM, &type, &format, &nitems, &nbytes,  
                            (unsigned char**)&dataList );
        XUNLOCK;
        for( unsigned long i=0; i<nitems; i++ )
        {
            XLOCK;
            string dataType = XGetAtomName( display, dataList[i] );
            XUNLOCK;
            dataTypes.push_back( dataType );
        }
        XFree( (void*)dataList );
    }
    else
    {
        for( int i=2; i<5; i++ )
        {
            if( data[i] != None )
            {
                XLOCK;
                string dataType = XGetAtomName( display, data[i] );
                XUNLOCK;
                dataTypes.push_back( dataType );
            }
        }
    }

    // Find the right target
    target = None;
    list<string>::iterator it;
    for( it = dataTypes.begin(); it != dataTypes.end(); it++ )
    {
        if( *it == "text/plain" || *it == "STRING" )
        {
            XLOCK;
            target = XInternAtom( display, (*it).c_str(), 0 );
            XUNLOCK;
            break;
        }
    }
}
//---------------------------------------------------------------------------
void X11DropObject::DndPosition( ldata_t data )
{   
    Window src = data[0];
    
    XLOCK;
    Atom actionAtom = XInternAtom( display, "XdndActionCopy", 0 );
    Atom typeAtom = XInternAtom( display, "XdndStatus", 0 );

    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = src;
    event.xclient.display = display;
    event.xclient.message_type = typeAtom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = Win;
    if( target != None )
    {
        event.xclient.data.l[1] = 1;          // accept the drop
    }
    else
    {
        event.xclient.data.l[1] = 0;          // do not accept
    }
    event.xclient.data.l[2] = 0;              // empty rectangle
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = actionAtom;
 
    // Tell the source whether we accept the drop
    XSendEvent( display, src, False, 0, &event );
    XUNLOCK;
}
//---------------------------------------------------------------------------
void X11DropObject::DndLeave( ldata_t data )
{
}
//---------------------------------------------------------------------------
void X11DropObject::DndDrop( ldata_t data )
{
    Window src = data[0];
    Time time = data[2];

    XLOCK;
    Atom selectionAtom = XInternAtom( display, "XdndSelection", 0 );
    Atom targetAtom = XInternAtom( display, "text/plain", 0 );
    Atom propAtom = XInternAtom( display, "VLC_SELECTION", 0 );
   
    Atom actionAtom = XInternAtom( display, "XdndActionCopy", 0 );
    Atom typeAtom = XInternAtom( display, "XdndFinished", 0 );

    // Convert the selection into the given target
    XConvertSelection( display, selectionAtom, targetAtom, propAtom, src, 
                       time );

    // Read the selection 
    Atom type;
    int format;
    unsigned long nitems, nbytes;
    char *buffer;
    XGetWindowProperty( display, src, propAtom, 0, 1024, False, 
                        AnyPropertyType, &type, &format, &nitems, &nbytes, 
                        (unsigned char**)&buffer );
    string selection = "";
    if( buffer != NULL )
    {
        selection = buffer;
    }
    XFree( buffer );
    XUNLOCK;
 
    if( selection != "" )
    {
        // TODO: multiple files handling
        string::size_type end = selection.find( "\n", 0 );
        selection = selection.substr( 0, end -1 );     
        end = selection.find( "\r", 0 );
        selection = selection.substr( 0, end -1 );

        // Find the protocol, if any
        string::size_type pos = selection.find( ":", 0 );
        if( selection.find("///", pos + 1 ) == pos + 1 )
        {
            selection.erase( pos + 1, 2 );
        }
    
        char *name = new char[selection.size()+1];
        strncpy( name, selection.c_str(), selection.size()+1 );
        msg_Dbg( p_intf, "drop: %s\n", name );
        OSAPI_PostMessage( NULL, VLC_DROP, (unsigned int)name, 0 );
    }
    
    // Tell the source we accepted the drop
    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = src;
    event.xclient.display = display;
    event.xclient.message_type = typeAtom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = Win;
    event.xclient.data.l[1] = 1;          // drop accepted
    event.xclient.data.l[2] = actionAtom;
    XLOCK;
    XSendEvent( display, src, False, 0, &event );
    XUNLOCK;
}

#endif
