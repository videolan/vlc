/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          Felix KŸhne <fkuehne at videolan dot org>
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
#include <vlc/vlc.h>

#include <vlc_keys.h>

#include <vlc_input.h>

#import <intf.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    }

    memset( p_intf->p_sys, 0, sizeof( *p_intf->p_sys ) );

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
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
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    sigset_t set;

    /* Do it again - for some unknown reason, vlc_thread_create() often
     * fails to go to real-time priority with the first launched thread
     * (???) --Meuuh */
    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );

    /* Make sure the "force quit" menu item does quit instantly.
     * VLC overrides SIGTERM which is sent by the "force quit"
     * menu item to make sure deamon mode quits gracefully, so
     * we un-override SIGTERM here. */
    sigemptyset( &set );
    sigaddset( &set, SIGTERM );
    pthread_sigmask( SIG_UNBLOCK, &set, NULL );
    CPSProcessSerNum PSN;
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    if (!CPSGetCurrentProcess(&PSN))
        if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
            if (!CPSSetFrontProcess(&PSN))
                [NSApplication sharedApplication];
    [NSApp run];
    [pool release];
}

