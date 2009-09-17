/*****************************************************************************
 * netsync.c: synchronisation between several network clients.
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_playlist.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include <vlc_network.h>

#define NETSYNC_PORT 9875

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Activate( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define NETSYNC_TEXT N_( "Network master clock" )
#define NETSYNC_LONGTEXT N_( "When set then " \
  "this vlc instance shall dictate its clock for synchronisation" \
  "over clients listening on the masters network ip address" )

#define MIP_TEXT N_( "Master server ip address" )
#define MIP_LONGTEXT N_( "The IP address of " \
  "the network master clock to use for clock synchronisation." )

#define NETSYNC_TIMEOUT_TEXT N_( "UDP timeout (in ms)" )
#define NETSYNC_TIMEOUT_LONGTEXT N_("Amount of time (in ms) " \
  "to wait before aborting network reception of data." )

vlc_module_begin ()
    set_shortname( N_("Network Sync"))
    set_description( N_("Network synchronisation") )
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

    add_bool( "netsync-master", false, NULL,
              NETSYNC_TEXT, NETSYNC_LONGTEXT, true )
    add_string( "netsync-master-ip", NULL, NULL, MIP_TEXT, MIP_LONGTEXT,
                true )
    add_integer( "netsync-timeout", 500, NULL,
                 NETSYNC_TIMEOUT_TEXT, NETSYNC_TIMEOUT_LONGTEXT, true )

    set_capability( "interface", 0 )
    set_callbacks( Activate, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run( intf_thread_t *p_intf );

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    int fd;

    if( !var_InheritBool( p_intf, "netsync-master" ) )
    {
        char *psz_master = var_InheritString( p_intf, "netsync-master-ip" );
        if( psz_master == NULL )
        {
            msg_Err( p_intf, "master address not specified" );
            return VLC_EGENERIC;
        }
        fd = net_ConnectUDP( VLC_OBJECT(p_intf), psz_master, NETSYNC_PORT, -1 );
        free( psz_master );
    }
    else
        fd = net_ListenUDP1( VLC_OBJECT(p_intf), NULL, NETSYNC_PORT );

    if( fd == -1 )
    {
        msg_Err( p_intf, "Netsync socket failure" );
        return VLC_EGENERIC;
    }

    p_intf->p_sys = (void *)(intptr_t)fd;
    p_intf->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    net_Close( (intptr_t)p_intf->p_sys );
}

/*****************************************************************************
 * Run: interface thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
#define MAX_MSG_LENGTH (2 * sizeof(int64_t))
    int canc = vlc_savecancel();
    input_thread_t *p_input = NULL;
    char p_data[MAX_MSG_LENGTH];
    int i_socket;

    playlist_t *p_playlist = pl_Hold( p_intf );
    int i_timeout = __MIN( 500, var_InheritInteger( p_intf, "netsync-timeout" ) );
    bool b_master = var_InheritBool( p_intf, "netsync-master" );
    i_socket = (intptr_t)p_intf->p_sys;

    /* High priority thread */
    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_INPUT );

    while( vlc_object_alive( p_intf ) )
    {
        /* Update the input */
        if( p_input == NULL )
        {
            p_input = playlist_CurrentInput( p_playlist );
        }
        else if( p_input->b_dead || !vlc_object_alive( p_input ) )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }

        if( p_input == NULL )
        {
            /* Wait a bit */
            msleep( INTF_IDLE_SLEEP );
            continue;
        }

        /*
         * We now have an input
         */

        /* Initialize file descriptor set and timeout (0.5s) */
        /* FIXME: arbitrary tick */
        struct pollfd ufd = { .fd = i_socket, .events = POLLIN, };

        if( b_master )
        {
            struct sockaddr_storage from;
            mtime_t i_master_system;
            mtime_t i_client_system;
            mtime_t i_date;
            int i_struct_size, i_read, i_ret;

            /* Don't block */
            i_ret = poll( &ufd, 1, i_timeout );
            if( i_ret <= 0 ) continue;

            /* We received something */
            i_struct_size = sizeof( from );
            i_read = recvfrom( i_socket, p_data, MAX_MSG_LENGTH, 0,
                               (struct sockaddr*)&from,
                               (unsigned int *)&i_struct_size );

            /* not sure we need the client information to sync,
               since we are the master anyway */
            i_client_system = ntoh64(*(int64_t *)p_data);

            i_date = mdate();

            if( input_GetPcrSystem( p_input, &i_master_system ) )
                continue;

            *((int64_t *)p_data) = hton64( i_date );
            *(((int64_t *)p_data)+1) = hton64( i_master_system );

            /* Reply to the sender */
            sendto( i_socket, p_data, 2 * sizeof(int64_t), 0,
                    (struct sockaddr *)&from, i_struct_size );

#if 0
            msg_Dbg( p_intf, "Master clockref: %"PRId64" -> %"PRId64", from %s "
                     "(date: %"PRId64")", i_client_system, i_master_system,
                     (from.ss_family == AF_INET) ? inet_ntoa(((struct sockaddr_in *)&from)->sin_addr)
                     : "non-IPv4", i_date );
#endif
        }
        else
        {
            mtime_t i_master_system;
            mtime_t i_client_system;
            mtime_t i_system = 0;
            mtime_t i_send_date, i_receive_date;
            mtime_t i_diff_date, i_master_date;
            int i_sent, i_read, i_ret;

            if( input_GetPcrSystem( p_input, &i_system ) )
            {
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            /* Send clock request to the master */
            i_send_date = mdate();
            *((int64_t *)p_data) = hton64( i_system );

            i_sent = send( i_socket, p_data, sizeof(int64_t), 0 );
            if( i_sent <= 0 )
            {
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            /* Don't block */
            i_ret = poll( &ufd, 1, i_timeout );
            if( i_ret == 0 ) continue;
            if( i_ret < 0 )
            {
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            i_receive_date = mdate();
            i_read = recv( i_socket, p_data, MAX_MSG_LENGTH, 0 );
            if( i_read <= 0 )
            {
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            i_master_date = ntoh64(*(int64_t *)p_data);
            i_master_system = ntoh64(*(((int64_t *)p_data)+1)); /* system date */

            i_diff_date = i_receive_date -
                          ((i_receive_date - i_send_date) / 2 + i_master_date);

            if( p_input && i_master_system > 0 )
            {
                mtime_t i_diff_system;

                if( input_GetPcrSystem( p_input, &i_client_system ) )
                {
                    msleep( INTF_IDLE_SLEEP );
                    continue;
                }

                i_diff_system = i_client_system - i_master_system - i_diff_date;
                if( i_diff_system != 0 )
                {
                    input_ModifyPcrSystem( p_input, true, i_master_system - i_diff_date );
#if 0
                    msg_Dbg( p_intf, "Slave clockref: %"PRId64" -> %"PRId64" -> %"PRId64","
                             " clock diff: %"PRId64", diff: %"PRId64"",
                             i_system, i_master_system, i_client_system,
                             i_diff_system, i_diff_date );
#endif
                }
            }
            msleep( INTF_IDLE_SLEEP );
        }
    }

    if( p_input ) vlc_object_release( p_input );
    pl_Release( p_intf );
    vlc_restorecancel( canc );
}

