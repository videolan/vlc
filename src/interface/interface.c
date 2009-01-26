/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_aout.h>
#include <vlc_vout.h>

#include "vlc_interface.h"
#if defined( __APPLE__ ) || defined( WIN32 )
#include "../control/libvlc_internal.h"
#endif
#include "libvlc.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void* RunInterface( vlc_object_t *p_this );
#if defined( __APPLE__ ) || defined( WIN32 )
static void * MonitorLibVLCDeath( vlc_object_t *p_this );
#endif
static int AddIntfCallback( vlc_object_t *, char const *,
                            vlc_value_t , vlc_value_t , void * );

/**
 * Destroy the interface after the main loop endeed.
 *
 * @param p_obj: the interface thread
 */
static void intf_Destroy( vlc_object_t *obj )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;

    /* Unlock module if present (a switch may have failed) */
    if( p_intf->p_module )
        module_unneed( p_intf, p_intf->p_module );

    free( p_intf->psz_intf );
    config_ChainDestroy( p_intf->p_cfg );
    vlc_mutex_destroy( &p_intf->change_lock );
}


/**
 * Create the interface, and prepare it for main loop. It opens ouput device
 * and creates specific interfaces. Sends its own error messages.
 *
 * @param p_this the calling vlc_object_t
 * @param psz_module a preferred interface module
 * @return a pointer to the created interface thread, NULL on error
 */
intf_thread_t* __intf_Create( vlc_object_t *p_this, const char *psz_module )
{
    intf_thread_t * p_intf;

    /* Allocate structure */
    p_intf = vlc_object_create( p_this, VLC_OBJECT_INTF );
    if( !p_intf )
        return NULL;
#if defined( __APPLE__ ) || defined( WIN32 )
    p_intf->b_should_run_on_first_thread = false;
#endif

    /* Choose the best module */
    p_intf->p_cfg = NULL;
    char *psz_parser = *psz_module == '$'
                     ? var_CreateGetString(p_intf,psz_module+1)
                     : strdup( psz_module );
    char *psz_tmp = config_ChainCreate( &p_intf->psz_intf, &p_intf->p_cfg,
                                        psz_parser );
    free( psz_tmp );
    free( psz_parser );
    p_intf->p_module = module_need( p_intf, "interface", p_intf->psz_intf, true );

    if( p_intf->p_module == NULL )
    {
        msg_Err( p_intf, "no suitable interface module" );
        free( p_intf->psz_intf );
        vlc_object_release( p_intf );
        return NULL;
    }

    /* Initialize mutexes */
    vlc_mutex_init( &p_intf->change_lock );

    /* Attach interface to its parent object */
    vlc_object_attach( p_intf, p_this );
    vlc_object_set_destructor( p_intf, intf_Destroy );

    return p_intf;
}


/**
 * Starts and runs the interface thread.
 *
 * @param p_intf the interface thread
 * @return VLC_SUCCESS on success, an error number else
 */
int intf_RunThread( intf_thread_t *p_intf )
{
#if defined( __APPLE__ ) || defined( WIN32 )
    /* Hack to get Mac OS X Cocoa runtime running
     * (it needs access to the main thread) */
    if( p_intf->b_should_run_on_first_thread )
    {
        if( vlc_thread_create( p_intf, "interface", MonitorLibVLCDeath,
                               VLC_THREAD_PRIORITY_LOW ) )
        {
            msg_Err( p_intf, "cannot spawn libvlc death monitoring thread" );
            return VLC_EGENERIC;
        }
        RunInterface( VLC_OBJECT(p_intf) );

        /* Make sure our MonitorLibVLCDeath thread exit */
        vlc_object_kill( p_intf );
        /* It is monitoring libvlc, not the p_intf */
        vlc_object_kill( p_intf->p_libvlc );
        vlc_thread_join( p_intf );

        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        return VLC_SUCCESS;
    }
#endif
    /* Run the interface in a separate thread */
    if( vlc_thread_create( p_intf, "interface", RunInterface,
                           VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_intf, "cannot spawn interface thread" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/**
 * Stops the interface thread
 *
 * This function asks the interface thread to stop
 * @param p_intf the interface thread
 */
void intf_StopThread( intf_thread_t *p_intf )
{
    /* Tell the interface to die */
    vlc_object_kill( p_intf );
    vlc_thread_join( p_intf );
}



/* Following functions are local */

/**
 * RunInterface: setups necessary data and give control to the interface
 *
 * @param p_this: interface object
 */
static void* RunInterface( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    vlc_value_t val, text;
    int canc = vlc_savecancel ();

    /* Variable used for interface spawning */
    var_Create( p_intf, "intf-add", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Add Interface");
    var_Change( p_intf, "intf-add", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = (char *)"rc";
    text.psz_string = (char *)_("Console");
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"telnet";
    text.psz_string = (char *)_("Telnet Interface");
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"http";
    text.psz_string = (char *)_("Web Interface");
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"logger";
    text.psz_string = (char *)_("Debug logging");
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"gestures";
    text.psz_string = (char *)_("Mouse Gestures");
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_intf, "intf-add", AddIntfCallback, NULL );
    vlc_restorecancel (canc);

    /* Give control to the interface */
    if( p_intf->pf_run )
        p_intf->pf_run( p_intf );

    return NULL;
}

#if defined( __APPLE__ ) || defined( WIN32 )
#include "control/libvlc_internal.h" /* libvlc_InternalWait */
/**
 * MonitorLibVLCDeath: Used when b_should_run_on_first_thread is set.
 *
 * @param p_this: the interface object
 */
static void * MonitorLibVLCDeath( vlc_object_t * p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    libvlc_int_t * p_libvlc = p_intf->p_libvlc;
    int canc = vlc_savecancel ();

    libvlc_InternalWait( p_libvlc );

    vlc_object_kill( p_intf ); /* Kill the stupid first thread interface */
    vlc_restorecancel (canc);
    return NULL;
}
#endif

static int AddIntfCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_cmd; (void)oldval; (void)p_data;
    intf_thread_t *p_intf;
    char* psz_intf;

    /* Try to create the interface */
    if( asprintf( &psz_intf, "%s,none", newval.psz_string ) == -1 )
        return VLC_ENOMEM;

    p_intf = intf_Create( p_this->p_libvlc, psz_intf );
    free( psz_intf );
    if( p_intf == NULL )
    {
        msg_Err( p_this, "interface \"%s\" initialization failed",
                 newval.psz_string );
        return VLC_EGENERIC;
    }

    /* Try to run the interface */
    if( intf_RunThread( p_intf ) != VLC_SUCCESS )
    {
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

