/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: intf_beos.cpp,v 1.38 2002/02/15 13:32:52 sam Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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
#include <string.h>

extern "C"
{
#include <videolan/vlc.h>

#include "stream_control.h"

#include "interface.h"
#include "input_ext-intf.h"
}

#include "InterfaceWindow.h"

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    InterfaceWindow * p_window;
    char              i_key;
} intf_sys_t;


extern "C"
{

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
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
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        return( 1 );
    }
    p_intf->p_sys->i_key = -1;

    /* Create the interface window */
    p_intf->p_sys->p_window =
        new InterfaceWindow( rect,
                             VOUT_TITLE " (BeOS interface)", p_intf );
    if( p_intf->p_sys->p_window == 0 )
    {
        free( p_intf->p_sys );
        intf_ErrMsg( "error: cannot allocate memory for InterfaceWindow" );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy dummy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy the interface window */
    p_intf->p_sys->p_window->Lock();
    p_intf->p_sys->p_window->Quit();

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_Run: event loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{

    while( !p_intf->b_die )
    {
        /* Manage core vlc functions through the callback */
        p_intf->pf_manage( p_intf );

        /* Manage the slider */
        if( p_input_bank->pp_input[0] != NULL
             && p_intf->p_sys->p_window != NULL)
        {
            p_intf->p_sys->p_window->updateInterface();
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
}

} /* extern "C" */

