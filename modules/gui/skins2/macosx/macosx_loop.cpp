/*****************************************************************************
 * macosx_loop.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifdef MACOSX_SKINS

#include <Carbon/Carbon.h>
#include "macosx_loop.hpp"


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
    if( pIntf->p_sys->p_osLoop )
    {
        delete pIntf->p_sys->p_osLoop;
        pIntf->p_sys->p_osLoop = NULL;
    }
}


void MacOSXLoop::run()
{
    // Main event loop
    while( !m_exit )
    {
        EventRef pEvent;
        OSStatus err = ReceiveNextEvent( 0, NULL, kEventDurationForever, true,
                                         &pEvent );
        if( err != noErr )
        {
            // Get the event type
            UInt32 evClass = GetEventClass( pEvent );

            switch( evClass )
            {
                case kEventClassMouse:
                {
                    break;
                }

                case kEventClassKeyboard:
                {
                    break;
                }

                case kEventClassWindow:
                {
                    handleWindowEvent( pEvent );
                    break;
                }

                default:
                {
                    EventTargetRef pTarget;
                    pTarget = GetEventDispatcherTarget();
                    SendEventToEventTarget( pEvent, pTarget );
                    ReleaseEvent( pEvent );
                }
            }
        }
    }
}


void MacOSXLoop::exit()
{
    m_exit = true;
}


void MacOSXLoop::handleWindowEvent( EventRef pEvent )
{
    UInt32 evKind = GetEventKind( pEvent );

}


#endif
