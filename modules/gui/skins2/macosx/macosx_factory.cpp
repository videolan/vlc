/*****************************************************************************
 * macosx_factory.cpp
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

#include "macosx_factory.hpp"
#include "macosx_graphics.hpp"
#include "macosx_loop.hpp"
#include "macosx_popup.hpp"
#include "macosx_timer.hpp"
#include "macosx_window.hpp"
#include "macosx_tooltip.hpp"


MacOSXFactory::MacOSXFactory( intf_thread_t *pIntf ): OSFactory( pIntf ),
    m_dirSep( "/" )
{
    // TODO
}


MacOSXFactory::~MacOSXFactory()
{
    // TODO
}


bool MacOSXFactory::init()
{
    // TODO
    return true;
}


OSGraphics *MacOSXFactory::createOSGraphics( int width, int height )
{
    return new MacOSXGraphics( getIntf(), width, height );
}


OSLoop *MacOSXFactory::getOSLoop()
{
    return MacOSXLoop::instance( getIntf() );
}


void MacOSXFactory::destroyOSLoop()
{
    MacOSXLoop::destroy( getIntf() );
}

void MacOSXFactory::minimize()
{
    // TODO
}

void MacOSXFactory::restore()
{
    // TODO
}

void MacOSXFactory::addInTray()
{
    // TODO
}

void MacOSXFactory::removeFromTray()
{
    // TODO
}

void MacOSXFactory::addInTaskBar()
{
    // TODO
}

void MacOSXFactory::removeFromTaskBar()
{
    // TODO
}

OSTimer *MacOSXFactory::createOSTimer( CmdGeneric &rCmd )
{
    return new MacOSXTimer( getIntf(), rCmd );
}


OSWindow *MacOSXFactory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                      bool playOnDrop, OSWindow *pParent )
{
    return new MacOSXWindow( getIntf(), rWindow, dragDrop,
                             playOnDrop, (MacOSXWindow*)pParent );
}


OSTooltip *MacOSXFactory::createOSTooltip()
{
    return new MacOSXTooltip( getIntf() );
}


OSPopup *MacOSXFactory::createOSPopup()
{
    return new MacOSXPopup( getIntf() );
}


int MacOSXFactory::getScreenWidth() const
{
    // TODO
    return 0;
}


int MacOSXFactory::getScreenHeight() const
{
    // TODO
    return 0;
}


SkinsRect MacOSXFactory::getWorkArea() const
{
    // XXX
    return SkinsRect( 0, 0, getScreenWidth(), getScreenHeight() );
}


void MacOSXFactory::getMousePos( int &rXPos, int &rYPos ) const
{
    // TODO
}


void MacOSXFactory::rmDir( const string &rPath )
{
    // TODO
}


#endif
