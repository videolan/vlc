/*****************************************************************************
 * intf_main.c: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *	        Florian G. Pflug <fgp@phlo.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include <videolan/vlc.h>

#include "interface.h"

/* OS specific */
#import <Cocoa/Cocoa.h>


/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    NSAutoreleasePool *o_pool ;
} intf_sys_t;


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
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function checks the interface can be run and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    /* Under MacOS X, this plugin always works */
    return( 100 );
}

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->p_sys->o_pool =[[NSAutoreleasePool alloc] init];

    [NSApplication sharedApplication];
    [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    [NSApp terminate:NSApp] ;
    [p_intf->p_sys->o_pool release] ;
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: main loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    [NSApp run] ;
}
