/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: interface.c,v 1.107 2003/10/14 22:41:41 gbazin Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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

#include "stream_control.h"
#include "input_ext-intf.h"

#include "audio_output.h"

#include "vlc_interface.h"

#include "vlc_video.h"
#include "video_output.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Manager( intf_thread_t *p_intf );
static void RunInterface( intf_thread_t *p_intf );

static int SwitchIntfCallback( vlc_object_t *, char const *,
                               vlc_value_t , vlc_value_t , void * );
static int AddIntfCallback( vlc_object_t *, char const *,
                            vlc_value_t , vlc_value_t , void * );

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
    char *psz_intf;

    /* Allocate structure */
    p_intf = vlc_object_create( p_this, VLC_OBJECT_INTF );
    if( !p_intf )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }

    /* XXX: workaround for a bug in VLC 0.5.0 where the dvdplay plugin was
     * registering itself in $interface, which we do not want to happen. */
    psz_intf = config_GetPsz( p_intf, "intf" );
    if( psz_intf )
    {
        if( !strcasecmp( psz_intf, "dvdplay" ) )
        {
            config_PutPsz( p_intf, "intf", "" );
        }
        free( psz_intf );
    }

    /* Choose the best module */
    p_intf->p_module = module_Need( p_intf, "interface", psz_module );

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

    return VLC_SUCCESS;
}

/*****************************************************************************
 * intf_StopThread: end the interface thread
 *****************************************************************************
 * This function asks the interface thread to stop.
 *****************************************************************************/
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

/*****************************************************************************
 * intf_Destroy: clean interface after main loop
 *****************************************************************************
 * This function destroys specific interfaces and close output devices.
 *****************************************************************************/
/**
 * \brief Destroy the interface after the main loop endeed.
 * 
 * Destroys interfaces and output devices
 * \param p_intf the interface thread
 * \return nothing
 */
void intf_Destroy( intf_thread_t *p_intf )
{
    /* Unlock module */
    module_Unneed( p_intf, p_intf->p_module );

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
            return;
        }
    }
}

/*****************************************************************************
 * RunInterface: setups necessary data and give control to the interface
 *****************************************************************************/
static void RunInterface( intf_thread_t *p_intf )
{
    vlc_value_t val, text;

    /* Variable used for interface switching */
    p_intf->psz_switch_intf = NULL;
    var_Create( p_intf, "intf-switch", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Switch interface");
    var_Change( p_intf, "intf-switch", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = "skins"; text.psz_string = "Skins";
    var_Change( p_intf, "intf-switch", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "wxwin"; text.psz_string = "wxWindows";
    var_Change( p_intf, "intf-switch", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_intf, "intf-switch", SwitchIntfCallback, NULL );

    /* Variable used for interface spawning */
    var_Create( p_intf, "intf-add", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Add interface");
    var_Change( p_intf, "intf-add", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = "rc"; text.psz_string = "Console";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "logger"; text.psz_string = "Debug logging";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "http"; text.psz_string = "HTTP remote control";
    var_Change( p_intf, "intf-add", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_intf, "intf-add", AddIntfCallback, NULL );

    /* Give control to the interface */
    p_intf->pf_run( p_intf );

    /* Provide ability to switch the main interface on the fly */
    while( p_intf->psz_switch_intf )
    {
        char *psz_intf = p_intf->psz_switch_intf;
        p_intf->psz_switch_intf = NULL;
        p_intf->b_die = VLC_FALSE;

        /* Make sure the old interface is completely uninitialised */
        module_Unneed( p_intf, p_intf->p_module );

        p_intf->p_module = module_Need( p_intf, "interface", psz_intf );
        free( psz_intf );

        if( p_intf->p_module )
        {
            p_intf->pf_run( p_intf );
        }
        else break;
    }
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
    p_intf = intf_Create( p_this, psz_intf );
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
