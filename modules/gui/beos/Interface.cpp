/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <InterfaceKit.h>
#include <Application.h>
#include <Message.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <aout_internal.h>

#include "InterfaceWindow.h"
#include "MsgVals.h"

/*****************************************************************************
 * intf_sys_t: internal variables of the BeOS interface
 *****************************************************************************/
struct intf_sys_t
{
    InterfaceWindow * p_window;
};

/*****************************************************************************
 * Local prototype
 *****************************************************************************/
static void Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t * p_intf = (intf_thread_t*) p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t*) malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_EGENERIC;
    }
    
    p_intf->pf_run = Run;

    /* Create the interface window */
    BScreen screen( B_MAIN_SCREEN_ID );
    BRect rect   = screen.Frame();
    rect.top     = rect.bottom - 100;
    rect.bottom -= 50;
    rect.left   += 50;
    rect.right   = rect.left + 350;
    p_intf->p_sys->p_window =
        new InterfaceWindow( p_intf, rect, "VLC " VERSION );
    if( !p_intf->p_sys->p_window )
    {
        free( p_intf->p_sys );
        msg_Err( p_intf, "cannot allocate InterfaceWindow" );
        return VLC_EGENERIC;
    }

    /* Make the be_app aware the interface has been created */
    BMessage message( INTERFACE_CREATED );
    message.AddPointer( "window", p_intf->p_sys->p_window );
    be_app->PostMessage( &message );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * intf_Close: destroy dummy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    /* Destroy the interface window */
    if( p_intf->p_sys->p_window->Lock() )
        p_intf->p_sys->p_window->Quit();

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_Run: event loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        p_intf->p_sys->p_window->UpdateInterface();
        msleep( INTF_IDLE_SLEEP );
    }
}
