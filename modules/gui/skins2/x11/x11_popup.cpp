/*****************************************************************************
 * x11_popup.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "x11_popup.hpp"


X11Popup::X11Popup( intf_thread_t *pIntf, X11Display &rDisplay )
    : OSPopup( pIntf )
{
    // TODO
    (void)rDisplay;
}


X11Popup::~X11Popup()
{
    // TODO
}


void X11Popup::show( int xPos, int yPos )
{
    // TODO
    (void)xPos; (void)yPos;
}


void X11Popup::hide()
{
    // TODO
}


void X11Popup::addItem( const string &rLabel, int pos )
{
    // TODO
    (void)rLabel; (void)pos;
}


void X11Popup::addSeparator( int pos )
{
    // TODO
    (void)pos;
}


int X11Popup::getPosFromId( int id ) const
{
    // TODO
    (void)id;
    return 0;
}


#endif

