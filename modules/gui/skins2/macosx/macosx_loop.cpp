/*****************************************************************************
 * macosx_loop.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifdef MACOSX_SKINS

#include "macosx_loop.hpp"
#include "macosx_window.hpp"
#include "../src/generic_window.hpp"
#include "../events/evt_refresh.hpp"

static pascal OSStatus WinEventHandler( EventHandlerCallRef handler,
                                        EventRef event, void *data )
{
    GenericWindow *pWin = (GenericWindow*)data;
    intf_thread_t *pIntf = pWin->getIntf();

    //fprintf(stderr, "event\n" );
    UInt32 evclass = GetEventClass( event );
    UInt32 evkind = GetEventKind( event );

    switch( evclass )
    {
    case kEventClassWindow:
        EvtRefresh evt( pIntf, 0, 0, -1, -1);
        pWin->processEvent( evt );
        break;
    }
}


MacOSXLoop::MacOSXLoop( intf_thread_t *pIntf ):
    OSLoop( pIntf ), m_exit( false )
{
}


MacOSXLoop::~MacOSXLoop()
{
}


OSLoop *MacOSXLoop::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_osLoop == NULL )
    {
        OSLoop *pOsLoop = new MacOSXLoop( pIntf );
        pIntf->p_sys->p_osLoop = pOsLoop;
    }
    return pIntf->p_sys->p_osLoop;
}


void MacOSXLoop::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_osLoop;
    pIntf->p_sys->p_osLoop = NULL;
}


void MacOSXLoop::run()
{
    // Main event loop
    while( !m_exit )
    {
        sleep(1);
    }
}


void MacOSXLoop::exit()
{
    m_exit = true;
}


void MacOSXLoop::registerWindow( GenericWindow &rGenWin, WindowRef win )
{
    // Create the event handler
    EventTypeSpec evList[] = {
        { kEventClassWindow, kEventWindowUpdate },
        { kEventClassMouse, kEventMouseMoved }
    };
    EventHandlerUPP handler = NewEventHandlerUPP( WinEventHandler );
    InstallWindowEventHandler( win, handler, GetEventTypeCount( evList ),
                               evList, &rGenWin, NULL );
}

#endif
