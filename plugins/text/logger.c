/*****************************************************************************
 * logger.c : file logging plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: logger.c,v 1.1 2002/02/19 00:50:19 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>

#include <videolan/vlc.h>

#include "interface.h"

#define LOG_FILE "vlc.log"
#define LOG_STRING( msg, file ) fwrite( msg, strlen( msg ), 1, file );

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    FILE *    p_file; /* The log file */
    intf_subscription_t *p_sub;

} intf_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions ( function_list_t * p_function_list );
static int  intf_Open         ( intf_thread_t *p_intf );
static void intf_Close        ( intf_thread_t *p_intf );
static void intf_Run          ( intf_thread_t *p_intf );

static void FlushQueue        ( intf_subscription_t *, FILE * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "file logging interface module" )
    ADD_CAPABILITY( INTF, 1 )
    ADD_SHORTCUT( "logger" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Open: initialize and create stuff
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    char *psz_filename;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg( "intf error: %s", strerror(ENOMEM) );
        return -1;
    }

    psz_filename = main_GetPszVariable( INTF_METHOD_VAR, NULL );

    while( *psz_filename && *psz_filename != ':' )
    {
        psz_filename++;
    }

    if( *psz_filename == ':' )
    {
        psz_filename++;
    }
    else
    {
        intf_ErrMsg( "intf error: no log filename provided, using `%s'",
                     LOG_FILE );
        psz_filename = LOG_FILE;
    }

    /* Open the log file */
    intf_WarnMsg( 1, "intf: opening logfile `%s'", psz_filename );
    p_intf->p_sys->p_file = fopen( psz_filename, "w" );

    p_intf->p_sys->p_sub = intf_MsgSub();

    if( p_intf->p_sys->p_file == NULL )
    {
        intf_ErrMsg( "intf error: error opening logfile `%s'", psz_filename );
        free( p_intf->p_sys );
        intf_MsgUnsub( p_intf->p_sys->p_sub );
        return -1;
    }

    LOG_STRING( "-- log plugin started --\n", p_intf->p_sys->p_file );

    return 0;
}

/*****************************************************************************
 * intf_Close: destroy interface stuff
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Flush the queue and unsubscribe from the message queue */
    FlushQueue( p_intf->p_sys->p_sub, p_intf->p_sys->p_file );
    intf_MsgUnsub( p_intf->p_sys->p_sub );

    LOG_STRING( "-- log plugin stopped --\n", p_intf->p_sys->p_file );

    /* Close the log file */
    fclose( p_intf->p_sys->p_file );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        p_intf->pf_manage( p_intf );

        FlushQueue( p_intf->p_sys->p_sub, p_intf->p_sys->p_file );

        msleep( INTF_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * FlushQueue: flush the message queue into the log file
 *****************************************************************************/
static void FlushQueue( intf_subscription_t *p_sub, FILE *p_file )
{
    int i_start, i_stop;
    char *psz_msg;

    vlc_mutex_lock( p_sub->p_lock );
    i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    /* Append all messages to log file */
    for( i_start = p_sub->i_start; i_start < i_stop; i_start++ )
    {
        psz_msg = p_sub->p_msg[i_start].psz_msg;
        LOG_STRING( psz_msg, p_file );
        LOG_STRING( "\n", p_file );
    }

    vlc_mutex_lock( p_sub->p_lock );
    p_sub->i_start = i_start;
    vlc_mutex_unlock( p_sub->p_lock );
}

