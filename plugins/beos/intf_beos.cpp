/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: intf_beos.cpp,v 1.8 2001/02/18 03:32:02 polux Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <kernel/OS.h>
#include <View.h>
#include <Application.h>
#include <Message.h>
#include <Locker.h>
#include <DirectWindow.h>
#include <malloc.h>
#include <string.h>

extern "C"
{
#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "intf_msg.h"
#include "interface.h"

#include "main.h"
}

#include "beos_window.h"

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    InterfaceWindow * p_window;
    char              i_key;
} intf_sys_t;

/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/
 
InterfaceWindow::InterfaceWindow( BRect frame, const char *name , intf_thread_t  *p_intf )
    : BWindow(frame, name, B_TITLED_WINDOW, B_NOT_RESIZABLE|B_NOT_ZOOMABLE)
{
    p_interface = p_intf;
    SetName( "interface" );
    
    BView * p_view;
    
    p_view = new BView( Bounds(), "", B_FOLLOW_ALL, B_WILL_DRAW );
    AddChild( p_view );
    
    Show();
}

InterfaceWindow::~InterfaceWindow()
{
}

/*****************************************************************************
 * InterfaceWindow::MessageReceived
 *****************************************************************************/

void InterfaceWindow::MessageReceived( BMessage * p_message )
{
    char * psz_key;
    
    switch( p_message->what )
    {
    case B_KEY_DOWN:
        p_message->FindString( "bytes", (const char **)&psz_key );
        p_interface->p_sys->i_key = psz_key[0];
        break;
        
    default:
        BWindow::MessageReceived( p_message );
        break;
    }
}

/*****************************************************************************
 * InterfaceWindow::QuitRequested
 *****************************************************************************/

bool InterfaceWindow::QuitRequested()
{
    return( false );
}


extern "C"
{

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize Gnome and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "beos" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * intf_Open: initialize dummy interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
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
        new InterfaceWindow( BRect( 50, 50, 400, 100 ),
                             VOUT_TITLE " (BeOS interface)", p_intf );
    if( p_intf->p_sys->p_window == 0 )
    {
        free( p_intf->p_sys );
        intf_ErrMsg( "error: cannot allocate memory for InterfaceWindow" );
        return( 1 );
    }
    
    /* Bind normal keys. */
    intf_AssignNormalKeys( p_intf );

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

        /* Manage keys */
        if( p_intf->p_sys->i_key != -1 )
        {
            intf_ProcessKey( p_intf, p_intf->p_sys->i_key );
            p_intf->p_sys->i_key = -1;
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
}

} /* extern "C" */
