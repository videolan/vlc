/*****************************************************************************
 * intf_macosx.c: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "interface.h"
#include "intf_msg.h"
#include "intf_playlist.h"

#include "main.h"

#include "modules.h"
#include "modules_export.h"

#include "macosx_qt_common.h"


/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/* OS specific */
#define kMainLoopFrequency  (kEventDurationSecond)		//45 for good measure

static pascal OSStatus FS_suspend_resume_handler(EventHandlerCallRef ref, EventRef event, void *dummy) ;
static pascal void APP_timer_handler(EventLoopTimerRef timer, void *dummy) ;

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
    if( TestMethod( INTF_METHOD_VAR, "macosx_qt" ) )
    {
        return( 999 );
    }

    /* Under MacOS X, this plugin always works */
    return( 90 );
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

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: main loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    static EventTypeSpec suspendResumeEvent[2] = {{kEventClassApplication,kEventAppActivated}, {kEventClassApplication,kEventAppDeactivated}} ;

    BeginFullScreen(&p_intf->p_sys->before_fullscreen, nil, 0, 0, &p_intf->p_sys->p_window, 0, fullScreenAllowEvents) ;
    InstallStandardEventHandler(GetApplicationEventTarget()) ;
    InstallApplicationEventHandler(NewEventHandlerUPP(FS_suspend_resume_handler), 2, suspendResumeEvent, &p_intf->p_sys->p_window, NULL) ;
    InstallEventLoopTimer(GetMainEventLoop(), 0, kMainLoopFrequency, NewEventLoopTimerUPP(APP_timer_handler), NULL, &p_intf->p_sys->r_timer) ;
    ShowWindow(p_intf->p_sys->p_window );
    p_intf->p_sys->b_active = 1 ;

    RunApplicationEventLoop() ;

    p_intf->p_sys->b_active = 0 ;
    EndFullScreen(p_intf->p_sys->before_fullscreen, nil) ;
}

static pascal void APP_timer_handler(EventLoopTimerRef timer, void *dummy)
{
	p_main->p_intf->pf_manage(p_main->p_intf) ;
	
	if (p_main->p_intf->b_die) QuitApplicationEventLoop() ;
}

static pascal OSStatus FS_suspend_resume_handler(EventHandlerCallRef ref, EventRef event, void *dummy)
{
	switch (GetEventKind(event))
	{
		case kEventAppActivated:	ShowWindow(p_main->p_intf->p_sys->p_window) ;
						SetPortWindowPort(p_main->p_intf->p_sys->p_window) ;
						intf_WarnMsg(1, "Application is on foreground") ;
						break ;
		case kEventAppDeactivated:	HideWindow(p_main->p_intf->p_sys->p_window) ;
						intf_WarnMsg(1, "Application sent to background") ;
						break ;
	}

	return noErr ;
}

