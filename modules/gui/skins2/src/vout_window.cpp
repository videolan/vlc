/*****************************************************************************
 * vout_window.cpp
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

#include "vout_window.hpp"
#include "os_factory.hpp"
#include "os_window.hpp"


VoutWindow::VoutWindow( intf_thread_t *pIntf, int left, int top,
                        bool dragDrop, bool playOnDrop, GenericWindow &rParent ):
    GenericWindow( pIntf, left, top, dragDrop, playOnDrop,
                   &rParent )
{
}


VoutWindow::~VoutWindow()
{
    // XXX we should stop the vout before destroying the window!
}

