/*****************************************************************************
 * x11_dragdrop.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "x11_dragdrop.hpp"
#include "x11_display.hpp"
#include "x11_factory.hpp"
#include "../commands/cmd_add_item.hpp"
#include "../events/evt_dragndrop.hpp"

#include <string>
#include <list>


X11DragDrop::X11DragDrop( intf_thread_t *pIntf, X11Display &rDisplay,
                          Window win, bool playOnDrop, GenericWindow *pWin ):
    SkinObject( pIntf ), m_rDisplay( rDisplay ), m_wnd( win ),
    m_playOnDrop( playOnDrop ), m_pWin( pWin ), m_xPos( -1 ), m_yPos( -1 )
{
}


void X11DragDrop::dndEnter( ldata_t data )
{
    Window src = data[0];
    int version = data[1]>>24;
    m_xPos = m_yPos = -1;
    (void)version;

    // Retrieve available data types
    std::list<std::string> dataTypes;
    if( data[1] & 1 )   // More than 3 data types ?
    {
        Atom type;
        int format;
        unsigned long nitems, nbytes;
        Atom *dataList;
        Atom typeListAtom = XInternAtom( XDISPLAY, "XdndTypeList", 0 );
        XGetWindowProperty( XDISPLAY, src, typeListAtom, 0, 65536, False,
                            XA_ATOM, &type, &format, &nitems, &nbytes,
                            (unsigned char**)&dataList );
        for( unsigned long i=0; i<nitems; i++ )
        {
            std::string dataType = XGetAtomName( XDISPLAY, dataList[i] );
            dataTypes.push_back( dataType );
        }
        XFree( (void*)dataList );
    }
    else
    {
        for( int i = 2; i < 5; i++ )
        {
            if( data[i] != None )
            {
                std::string dataType = XGetAtomName( XDISPLAY, data[i] );
                dataTypes.push_back( dataType );
            }
        }
    }

    // list all data types available
    std::list<std::string>::iterator it;
    for( it = dataTypes.begin(); it != dataTypes.end(); ++it )
        msg_Dbg( getIntf(), "D&D data type: %s", (*it).c_str() );

    // Find the right target
    m_target = None;
    for( it = dataTypes.begin(); it != dataTypes.end(); ++it )
    {
        if( *it == "text/uri-list" ||
            *it == "text/plain" ||
            *it == "text/plain;charset=utf-8" ||
            *it == "STRING" ||
            *it == "UTF8_STRING" )
        {
            m_target = XInternAtom( XDISPLAY, (*it).c_str(), 0 );
            msg_Dbg( getIntf(), "D&D data type chosen: %s", (*it).c_str() );
            break;
        }
    }

    // transmit DragEnter event
    EvtDragEnter evt( getIntf() );
    m_pWin->processEvent( evt );
}


void X11DragDrop::dndPosition( ldata_t data )
{
    Window src = data[0];
    m_xPos = data[2] >> 16;
    m_yPos = data[2] & 0xffff;
    Time time = data[3];
    Atom action = data[4];
    (void)time; (void)action;

    Atom actionAtom = XInternAtom( XDISPLAY, "XdndActionCopy", 0 );
    Atom typeAtom = XInternAtom( XDISPLAY, "XdndStatus", 0 );

    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = src;
    event.xclient.display = XDISPLAY;
    event.xclient.message_type = typeAtom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = m_wnd;
    // Accept the drop (1), or not (0).
    event.xclient.data.l[1] = m_target != None ? 1 : 0;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = actionAtom;

    // Tell the source whether we accept the drop
    XSendEvent( XDISPLAY, src, False, 0, &event );

    // transmit DragOver event
    EvtDragOver evt( getIntf(), m_xPos, m_yPos );
    m_pWin->processEvent( evt );
}


void X11DragDrop::dndLeave( ldata_t data )
{
    (void)data;
    m_target = None;

    // transmit DragLeave event
    EvtDragLeave evt( getIntf() );
    m_pWin->processEvent( evt );
}


void X11DragDrop::dndDrop( ldata_t data )
{
    std::list<std::string> files;

    Window src = data[0];
    Time time = data[2];

    Atom selectionAtom = XInternAtom( XDISPLAY, "XdndSelection", 0 );
    Atom propAtom = XInternAtom( XDISPLAY, "VLC_SELECTION", 0 );
    // Convert the selection into the given target
    XConvertSelection( XDISPLAY, selectionAtom, m_target, propAtom, src,
                       time );
    // Needed to ensure XGetWindowProperty returns something
    XSync( XDISPLAY, False );

    // Read the selection
    Atom type;
    int format;
    unsigned long nitems, nbytes_after_return;
    char *buffer;
    long length_max = 1024;
    XGetWindowProperty( XDISPLAY, src, propAtom, 0, length_max, False,
                        AnyPropertyType, &type, &format, &nitems,
                        &nbytes_after_return, (unsigned char**)&buffer );
    if( buffer && nbytes_after_return > 0 )
    {
        XFree( buffer );
        length_max += nbytes_after_return;
        XGetWindowProperty( XDISPLAY, src, propAtom, 0, length_max, False,
                        AnyPropertyType, &type, &format, &nitems,
                        &nbytes_after_return, (unsigned char**)&buffer );
    }
    if( buffer != NULL )
    {
        msg_Dbg( getIntf(), "buffer received: %s", buffer );
        char* psz_dup = strdup( buffer );
        char* psz_new = psz_dup;
        while( psz_new && *psz_new )
        {
            int skip = 0;
            const char* sep[] = { "\r\n", "\n", NULL };
            for( int i = 0; sep[i]; i++ )
            {
                char* psz_end = strstr( psz_new, sep[i] );
                if( !psz_end )
                    continue;
                *psz_end = '\0';
                skip = strlen( sep[i] );
                break;
            }
            if( *psz_new && strstr( psz_new, "://" ) )
            {
                files.push_back( psz_new );
            }

            psz_new += strlen( psz_new ) + skip;
        }
        free( psz_dup );
        XFree( buffer );
    }

    Atom actionAtom = XInternAtom( XDISPLAY, "XdndActionCopy", 0 );
    Atom typeAtom = XInternAtom( XDISPLAY, "XdndFinished", 0 );

    // Tell the source we accepted the drop
    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = src;
    event.xclient.display = XDISPLAY;
    event.xclient.message_type = typeAtom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = m_wnd;
    event.xclient.data.l[1] = 1;            // drop accepted
    event.xclient.data.l[2] = actionAtom;
    XSendEvent( XDISPLAY, src, False, 0, &event );

    // transmit DragDrop event
    EvtDragDrop evt( getIntf(), m_xPos, m_yPos, files );
    m_pWin->processEvent( evt );
}

#endif
