/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: Interface.cpp,v 1.2 2002/09/30 18:30:27 titer Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
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

#include "VlcWrapper.h"
#include "InterfaceWindow.h"
#include "MsgVals.h"

/*****************************************************************************
 * Local prototype
 *****************************************************************************/
static void Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    BScreen *screen;
    screen = new BScreen();
    BRect rect = screen->Frame();
    rect.top = rect.bottom-100;
    rect.bottom -= 50;
    rect.left += 50;
    rect.right = rect.left + 350;
    delete screen;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t*) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    }
    
    p_intf->p_sys->p_input = NULL;

    p_intf->pf_run = Run;

    /* Create the interface window */
    p_intf->p_sys->p_window =
        new InterfaceWindow( rect,
                             VOUT_TITLE " (BeOS interface)", p_intf );
    if( p_intf->p_sys->p_window == 0 )
    {
        free( p_intf->p_sys );
        msg_Err( p_intf, "cannot allocate InterfaceWindow" );
        return( 1 );
    } else {
    	BMessage message(INTERFACE_CREATED);
    	message.AddPointer("window", p_intf->p_sys->p_window);
    	be_app->PostMessage(&message);
    }
    p_intf->p_sys->b_disabled_menus = 0;
    p_intf->p_sys->i_saved_volume = AOUT_VOLUME_DEFAULT;
    p_intf->p_sys->b_loop = 0;
    p_intf->p_sys->b_mute = 0;
    
    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy dummy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }
    
    /* Destroy the interface window */
    p_intf->p_sys->p_window->Lock();
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
        /* Update the input */
        if( p_intf->p_sys->p_input != NULL )
        {
            if( p_intf->p_sys->p_input->b_dead )
            {
                vlc_object_release( p_intf->p_sys->p_input );
                p_intf->p_sys->p_input = NULL;
            }
        /* Manage the slider */
            p_intf->p_sys->p_window->updateInterface();
        }
    
        if( p_intf->p_sys->p_input == NULL )
        {
            p_intf->p_sys->p_input = 
                        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                          FIND_ANYWHERE );
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
}
