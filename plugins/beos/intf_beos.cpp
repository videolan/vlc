/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 * Jean-Marc Dressler
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
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
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
#include "plugins.h"

#include "input.h"
#include "video.h"
#include "video_output.h"

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
 * intf_BeCreate: initialize dummy interface
 *****************************************************************************/
int intf_BeCreate( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t*) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );
    }
    p_intf->p_sys->i_key = -1;
    
    /* Create the interface window */
    p_intf->p_sys->p_window =
        new InterfaceWindow( BRect( 100, 100, 200, 200 ), "Interface :)", p_intf );
    if( p_intf->p_sys->p_window == 0 )
    {
        free( p_intf->p_sys );
        intf_ErrMsg( "error: cannot allocate memory for InterfaceWindow\n" );
        return( 1 );
    }
    
    /* Spawn video output thread */
    if( p_main->b_video )
    {
        p_intf->p_vout = vout_CreateThread( NULL, 0, 0, 0, NULL, 0, NULL );
        if( p_intf->p_vout == NULL )                                /* error */
        {
            intf_ErrMsg("intf error: can't create output thread\n" );
            return( 1 );
        }
    }
    return( 0 );
}

/*****************************************************************************
 * intf_BeDestroy: destroy dummy interface
 *****************************************************************************/
void intf_BeDestroy( intf_thread_t *p_intf )
{
    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Destroy the interface window */
    p_intf->p_sys->p_window->Lock();
    p_intf->p_sys->p_window->Quit();    

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_BeManage: event loop
 *****************************************************************************/
void intf_BeManage( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->i_key != -1 )
    {
        intf_ProcessKey( p_intf, p_intf->p_sys->i_key );
        p_intf->p_sys->i_key = -1;
    }
}

} /* extern "C" */
