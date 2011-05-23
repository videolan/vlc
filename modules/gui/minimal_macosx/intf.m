/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Kühne <fkuehne at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_keys.h>

#include <vlc_input.h>
#import <vlc_interface.h>

#import <intf.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    memset( p_intf->p_sys, 0, sizeof( *p_intf->p_sys ) );

    p_intf->pf_run = Run;
    p_intf->b_should_run_on_first_thread = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    free( p_intf->p_sys );
}

/* Dock Connection */
typedef struct CPSProcessSerNum
{
        UInt32                lo;
        UInt32                hi;
} CPSProcessSerNum;

extern OSErr    CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr    CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr    CPSSetFrontProcess( CPSProcessSerNum *psn);

/*****************************************************************************
 * KillerThread: Thread that kill the application
 *****************************************************************************/
static void * KillerThread( void *user_data )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    intf_thread_t *p_intf = user_data;

    vlc_mutex_init( &p_intf->p_sys->lock );
    vlc_cond_init( &p_intf->p_sys->wait );

    vlc_mutex_lock ( &p_intf->p_sys->lock );
    while( vlc_object_alive( p_intf ) )
        vlc_cond_wait( &p_intf->p_sys->wait, &p_intf->p_sys->lock );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    vlc_mutex_destroy( &p_intf->p_sys->lock );
    vlc_cond_destroy( &p_intf->p_sys->wait );

    msg_Dbg( p_intf, "Killing the Minimal Mac OS X module" );

    /* We are dead, terminate */
    [NSApp terminate: nil];
    [o_pool release];
    return NULL;
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    sigset_t set;

    /* Make sure the "force quit" menu item does quit instantly.
     * VLC overrides SIGTERM which is sent by the "force quit"
     * menu item to make sure deamon mode quits gracefully, so
     * we un-override SIGTERM here. */
    sigemptyset( &set );
    sigaddset( &set, SIGTERM );
    pthread_sigmask( SIG_UNBLOCK, &set, NULL );

    /* Setup a thread that will monitor the module killing */
    pthread_t killer_thread;
    pthread_create( &killer_thread, NULL, KillerThread, p_intf );

    CPSProcessSerNum PSN;
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    if (!CPSGetCurrentProcess(&PSN))
        if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
            if (!CPSSetFrontProcess(&PSN))
                [NSApplication sharedApplication];
    [NSApp run];

    pthread_join( killer_thread, NULL );

    [pool release];
}

