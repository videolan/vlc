/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

/**
 *   \file
 *   This file contains functions related to interface management
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                                   /* FILE */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "audio_output.h"

#include "vlc_interface.h"
#include "vlc_video.h"
#include "video_output.h"

#ifdef SYS_DARWIN
#    include "Cocoa/Cocoa.h"
#endif /* SYS_DARWIN */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Manager( intf_thread_t *p_intf );
static void RunInterface( intf_thread_t *p_intf );

static int SwitchIntfCallback( vlc_object_t *, char const *,
                               vlc_value_t , vlc_value_t , void * );
static int AddIntfCallback( vlc_object_t *, char const *,
                            vlc_value_t , vlc_value_t , void * );

#ifdef SYS_DARWIN
/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/
@interface VLCApplication : NSApplication
{
   vlc_t *o_vlc;
}

- (void)setVLC: (vlc_t *)p_vlc;

@end
#endif

/*****************************************************************************
 * intf_Create: prepare interface before main loop
 *****************************************************************************
 * This function opens output devices and creates specific interfaces. It sends
 * its own error messages.
 *****************************************************************************/
/**
 * Create the interface, and prepare it for main loop.
 *
 * \param p_this the calling vlc_object_t
 * \param psz_module a prefered interface module
 * \return a pointer to the created interface thread, NULL on error
 */
intf_thread_t* __intf_Create( vlc_object_t *p_this, const char *psz_module )
{
    intf_thread_t * p_intf;

    /* Allocate structure */
    p_intf = vlc_object_create( p_this, VLC_OBJECT_INTF );
    if( !p_intf )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }
    p_intf->pf_request_window = NULL;
    p_intf->pf_release_window = NULL;
    p_intf->pf_control_window = NULL;
    p_intf->b_play = VLC_FALSE;
    p_intf->b_interaction = VLC_FALSE;

    /* Choose the best module */
    p_intf->p_module = module_Need( p_intf, "interface", psz_module, 0 );

    if( p_intf->p_module == NULL )
    {
        msg_Err( p_intf, "no suitable intf module" );
        vlc_object_destroy( p_intf );
        return NULL;
    }

    /* Initialize structure */
    p_intf->b_menu        = VLC_FALSE;
    p_intf->b_menu_change = VLC_FALSE;

    /* Initialize mutexes */
    vlc_mutex_init( p_intf, &p_intf->change_lock );

    msg_Dbg( p_intf, "interface initialized" );

    /* Attach interface to its parent object */
    vlc_object_attach( p_intf, p_this );

    return p_intf;
}

/*****************************************************************************
 * intf_RunThread: launch the interface thread
 *****************************************************************************
 * This function either creates a new thread and runs the interface in it,
 * or runs the interface in the current thread, depending on b_block.
 *****************************************************************************/
/**
 * Run the interface thread.
 *
 * If b_block is not set, runs the interface in the thread, else,
 * creates a new thread and runs the interface.
 * \param p_intf the interface thread
 * \return VLC_SUCCESS on success, an error number else
 */
int intf_RunThread( intf_thread_t *p_intf )
{
#ifdef SYS_DARWIN
    NSAutoreleasePool * o_pool;

    if( p_intf->b_block )
    {
        /* This is the primary intf */
        /* Run a manager thread, launch the interface, kill the manager */
        if( vlc_thread_create( p_intf, "manager", Manager,
                               VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
        {
            msg_Err( p_intf, "cannot spawn manager thread" );
            return VLC_EGENERIC;
        }
    }

    if( p_intf->b_block && strncmp( p_intf->p_vlc->psz_object_name,
                                    "clivlc", 6) )
    {
        o_pool = [[NSAutoreleasePool alloc] init];
        [VLCApplication sharedApplication];
        [NSApp setVLC: p_intf->p_vlc];
    }

    if( p_intf->b_block &&
        ( !strncmp( p_intf->p_module->psz_object_name, "macosx" , 6 ) ||
          !strncmp( p_intf->p_vlc->psz_object_name, "clivlc", 6 ) ) )
    {
        /* VLC in normal primary interface mode */
        RunInterface( p_intf );
        p_intf->b_die = VLC_TRUE;
    }
    else
    {
        /* Run the interface in a separate thread */
        if( !strcmp( p_intf->p_module->psz_object_name, "macosx" ) )
        {
            msg_Err( p_intf, "You cannot run the MacOS X module as an extrainterface. Please read the README.MacOSX.rtf file");
            return VLC_EGENERIC;
        }
        if( vlc_thread_create( p_intf, "interface", RunInterface,
                               VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
        {
            msg_Err( p_intf, "cannot spawn interface thread" );
            return VLC_EGENERIC;
        }

        if( p_intf->b_block )
        {
            /* VLC in primary interface mode with a working macosx vout */
            [NSApp run];
            p_intf->b_die = VLC_TRUE;
        }
    }
#else
    if( p_intf->b_block )
    {
        /* Run a manager thread, launch the interface, kill the manager */
        if( vlc_thread_create( p_intf, "manager", Manager,
                               VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
        {
            msg_Err( p_intf, "cannot spawn manager thread" );
            return VLC_EGENERIC;
        }

        RunInterface( p_intf );

        p_intf->b_die = VLC_TRUE;
        /* Do not join the thread... intf_StopThread will do it for us */
    }
    else
    {
        /* Run the interface in a separate thread */
        if( vlc_thread_create( p_intf, "interface", RunInterface,
                               VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
        {
            msg_Err( p_intf, "cannot spawn interface thread" );
            return VLC_EGENERIC;
        }
    }
#endif

    return VLC_SUCCESS;
}

/**
 * Stops the interface thread
 *
 * This function asks the interface thread to stop
 * \param p_intf the interface thread
 * \return nothing
 */
void intf_StopThread( intf_thread_t *p_intf )
{
    /* Tell the interface to die */
    if( !p_intf->b_block )
    {
        p_intf->b_die = VLC_TRUE;
    }

    /* Wait for the thread to exit */
    vlc_thread_join( p_intf );
}

/**
 * \brief Destroy the interface after the main loop endeed.
 *
 * Destroys interfaces and closes output devices
 * \param p_intf the interface thread
 * \return nothing
 */
void intf_Destroy( intf_thread_t *p_intf )
{
    /* Unlock module if present (a switch may have failed) */
    if( p_intf->p_module )
    {
        module_Unneed( p_intf, p_intf->p_module );
    }

    vlc_mutex_destroy( &p_intf->change_lock );

    /* Free structure */
    vlc_object_destroy( p_intf );
}


/* Following functions are local */

/*****************************************************************************
 * Manager: helper thread for blocking interfaces
 *****************************************************************************
 * If the interface is launched in the main thread, it will not listen to
 * p_vlc->b_die events because it is only supposed to listen to p_intf->b_die.
 * This thread takes care of the matter.
 *****************************************************************************/
/**
 * \brief Helper thread for blocking interfaces.
 * \ingroup vlc_interface
 *
 * This is a local function
 * If the interface is launched in the main thread, it will not listen to
 * p_vlc->b_die events because it is only supposed to listen to p_intf->b_die.
 * This thread takes care of the matter.
 * \see intf_RunThread
 * \param p_intf an interface thread
 * \return nothing
 */
static void Manager( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        msleep( INTF_IDLE_SLEEP );

        if( p_intf->p_vlc->b_die )
        {
            p_intf->b_die = VLC_TRUE;
#ifdef SYS_DARWIN
    if( strncmp( p_intf->p_vlc->psz_object_name, "clivlc", 6 ) )
    {
        [NSApp stop: NULL];
    }
#endif
            return;
        }
    }
}

/*****************************************************************************
 * RunInterface: setups necessary data and give control to the interface
 *****************************************************************************/
static void RunInterface( intf_thread_t *p_intf )
{
    static char *ppsz_interfaces[] =
    {
        "skins2", "Skins 2",
        "wxwidgets", "wxWidgets",
        NULL, NULL
    };
    char **ppsz_parser;

    vlc_list_t *p_list;
    int i;
    vlc_value_t val, text;
    char *psz_intf;

    /* Variable used for interface switching */
    p_intf->psz_switch_intf = NULL;
    var_Create( p_intf, "intf-switch", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Switch interface");
    var_Change( p_intf, "intf-switch", VLC_VAR_SETTEXT, &text, NULL );

    /* Only fill the list with available modules */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( ppsz_parser = ppsz_interfaces; *ppsz_parser; ppsz_parser += 2 )
    {
        for( i = 0; i < p_list->i_count; i++ )
        {
            module_t *p_module = (module_t *)p_list->p_values[i].p_object;
            if( !strcmp( p_module->psz_object_name, ppsz_parser[0] ) )
            {
                val.psz_string = ppsz_parser[0];
                text.psz_string = ppsz_parser[1];
                var_Change( p_intf, "intf-switch", VLC_VAR_ADDCHOICE,
                            &val, &text );
                break;
            }
        }
    }
    vlc_list_release( p_list );

    var_AddCallback( p_intf, "intf-switch", SwitchIntfCallback, NULL );

    /* Variable used for interface spawning */
    var_Create( p_intf, "intf-add", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Add Interface");
    var_Change( p_intf, "intf-add", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = "rc"; text.psz_string = "Console";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "telnet"; text.psz_string = "Telnet Interface";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "http"; text.psz_string = "Web Interface";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "logger"; text.psz_string = "Debug logging";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "gestures"; text.psz_string = "Mouse Gestures";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_intf, "intf-add", AddIntfCallback, NULL );

    do
    {
        /* Give control to the interface */
        p_intf->pf_run( p_intf );

        /* Reset play on start status */
        p_intf->b_play = VLC_FALSE;

        if( !p_intf->psz_switch_intf )
        {
            break;
        }

        /* Make sure the old interface is completely uninitialized */
        module_Unneed( p_intf, p_intf->p_module );

        /* Provide ability to switch the main interface on the fly */
        psz_intf = p_intf->psz_switch_intf;
        p_intf->psz_switch_intf = NULL;

        vlc_mutex_lock( &p_intf->object_lock );
        p_intf->b_die = VLC_FALSE;
        p_intf->b_dead = VLC_FALSE;
        vlc_mutex_unlock( &p_intf->object_lock );

        p_intf->p_module = module_Need( p_intf, "interface", psz_intf, 0 );
        free( psz_intf );
    }
    while( p_intf->p_module );
}

static int SwitchIntfCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    p_intf->psz_switch_intf =
        malloc( strlen(newval.psz_string) + sizeof(",none") );
    sprintf( p_intf->psz_switch_intf, "%s,none", newval.psz_string );
    p_intf->b_die = VLC_TRUE;

    return VLC_SUCCESS;
}

static int AddIntfCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf;
    char *psz_intf = malloc( strlen(newval.psz_string) + sizeof(",none") );

    /* Try to create the interface */
    sprintf( psz_intf, "%s,none", newval.psz_string );
    p_intf = intf_Create( p_this->p_vlc, psz_intf );
    free( psz_intf );
    if( p_intf == NULL )
    {
        msg_Err( p_this, "interface \"%s\" initialization failed",
                 newval.psz_string );
        return VLC_EGENERIC;
    }

    /* Try to run the interface */
    p_intf->b_block = VLC_FALSE;
    if( intf_RunThread( p_intf ) != VLC_SUCCESS )
    {
        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#ifdef SYS_DARWIN
/*****************************************************************************
 * VLCApplication implementation 
 *****************************************************************************/
@implementation VLCApplication 

- (void)setVLC: (vlc_t *) p_vlc
{
    o_vlc = p_vlc;
}

- (void)stop: (id)sender
{
    NSEvent *o_event;
    NSAutoreleasePool *o_pool;
    [super stop:sender];

    o_pool = [[NSAutoreleasePool alloc] init];
    /* send a dummy event to break out of the event loop */
    o_event = [NSEvent mouseEventWithType: NSLeftMouseDown
                location: NSMakePoint( 1, 1 ) modifierFlags: 0
                timestamp: 1 windowNumber: [[NSApp mainWindow] windowNumber]
                context: [NSGraphicsContext currentContext] eventNumber: 1
                clickCount: 1 pressure: 0.0];
    [NSApp postEvent: o_event atStart: YES];
    [o_pool release];
}

- (void)terminate: (id)sender
{
    o_vlc->b_die = VLC_TRUE;
}

@end
#endif

