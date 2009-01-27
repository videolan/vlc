/*****************************************************************************
 * netsync.c: synchronisation between several network clients.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_es_out.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include <vlc_network.h>

#define NETSYNC_PORT 9875

/* FIXME: UGLY UGLY !! Netsync should be totally reworked */
#include "../../src/input/input_internal.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Activate( vlc_object_t * );
static void Close   ( vlc_object_t * );

static mtime_t GetClockRef( intf_thread_t *, mtime_t );

/// \bug [String] This string is BAD.
#define NETSYNC_TEXT N_( "Act as master" )
#define NETSYNC_LONGTEXT N_( "Should " \
  "act as the master client for the network synchronisation?" )

/// \bug [String] This string is BAD.
#define MIP_TEXT N_( "Master client ip address" )
#define MIP_LONGTEXT N_( "IP address of " \
  "the master client used for the network synchronisation." )

vlc_module_begin ()
    set_shortname( N_("Network Sync"))
    set_description( N_("Network synchronisation") )
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

    add_bool( "netsync-master", 0, NULL,
              NETSYNC_TEXT, NETSYNC_LONGTEXT, true )
    add_string( "netsync-master-ip", NULL, NULL, MIP_TEXT, MIP_LONGTEXT,
                true )

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

    if( config_GetInt( p_intf, "netsync-master" ) <= 0 )
    {
        char *psz_master = config_GetPsz( p_intf, "netsync-master-ip" );
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

    input_thread_t *p_input = NULL;
    char p_data[MAX_MSG_LENGTH];
    int i_socket;
    int canc = vlc_savecancel();

    /* High priority thread */
    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_INPUT );

    while( vlc_object_alive( p_intf ) )
    {
        /* Update the input */
        if( p_input == NULL )
            p_input =
                (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                   FIND_ANYWHERE );
        else if( p_input->b_dead )
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
            mtime_t i_date, i_clockref, i_master_clockref;
            int i_struct_size, i_read, i_ret;

            /* Don't block */
            i_ret = poll( &ufd, 1, 500 );
            if( i_ret == 0 ) continue;
            if( i_ret < 0 )
            {
                /* Wait a bit */
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            /* We received something */
            i_struct_size = sizeof( from );
            i_read = recvfrom( i_socket, p_data, MAX_MSG_LENGTH, 0,
                               (struct sockaddr*)&from,
                               (unsigned int *)&i_struct_size );

            i_clockref = ntoh64(*(int64_t *)p_data);

            i_date = mdate();
            *(int64_t *)p_data = hton64( i_date );

            i_master_clockref = GetClockRef( p_intf, i_clockref );
            *(((int64_t *)p_data)+1) = hton64( i_master_clockref );

            /* Reply to the sender */
            sendto( i_socket, p_data, 2 * sizeof(int64_t), 0,
                    (struct sockaddr *)&from, i_struct_size );

#if 0
            msg_Dbg( p_intf, "Master clockref: %"PRId64" -> %"PRId64", from %s "
                     "(date: %"PRId64")", i_clockref, i_master_clockref,
                     from.ss_family == AF_INET
                     ? inet_ntoa(((struct sockaddr_in *)&from)->sin_addr)
                     : "non-IPv4", i_date );
#endif
        }
        else
        {
            mtime_t i_send_date, i_receive_date, i_master_date, i_diff_date;
            mtime_t i_master_clockref, i_client_clockref, i_drift;
            mtime_t i_clockref = 0;
            int i_sent, i_read, i_ret;

            /* Send clock request to the master */
            *(int64_t *)p_data = hton64( i_clockref );
            i_send_date = mdate();

            i_sent = send( i_socket, p_data, sizeof(int64_t), 0 );
            if( i_sent <= 0 )
            {
                /* Wait a bit */
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            /* Don't block */
            i_ret = poll( &ufd, 1, 500 );
            if( i_ret == 0 ) continue;
            if( i_ret < 0 )
            {
                /* Wait a bit */
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            i_receive_date = mdate();

            i_read = recv( i_socket, p_data, MAX_MSG_LENGTH, 0 );
            if( i_read <= 0 )
            {
                /* Wait a bit */
                msleep( INTF_IDLE_SLEEP );
                continue;
            }

            i_master_date = ntoh64(*(int64_t *)p_data);
            i_master_clockref = ntoh64(*(((int64_t *)p_data)+1));

            i_diff_date = i_receive_date -
                          ((i_receive_date - i_send_date) / 2 + i_master_date);

            i_client_clockref = i_drift = 0;
            if( p_input && i_master_clockref )
            {
                i_client_clockref = GetClockRef( p_intf, i_clockref );
                i_drift = i_client_clockref - i_master_clockref - i_diff_date;

                /* Update our clock to match the master's one */
                if( i_client_clockref )
                    p_input->i_pts_delay -= i_drift;
            }

#if 0
            msg_Dbg( p_intf, "Slave clockref: %"PRId64" -> %"PRId64" -> %"PRId64", "
                     "clock diff: %"PRId64" drift: %"PRId64,
                     i_clockref, i_master_clockref,
                     i_client_clockref, i_diff_date, i_drift );
#endif

            /* Wait a bit */
            msleep( INTF_IDLE_SLEEP );
        }
    }

    if( p_input ) vlc_object_release( p_input );
    vlc_restorecancel( canc );
}

static mtime_t GetClockRef( intf_thread_t *p_intf, mtime_t i_pts )
{
    input_thread_t *p_input = p_intf->p_sys->p_input;
    mtime_t i_ts;

    if( !p_input || !p_input->p->p_es_out ) return 0;

    if( es_out_Control( p_input->p->p_es_out, ES_OUT_GET_TS, i_pts, &i_ts ) ==
        VLC_SUCCESS )
    {
        return i_ts;
    }

    return 0;
}
