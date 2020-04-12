/*****************************************************************************
 * udp.c
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <vlc_queue.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#elif defined (HAVE_SYS_SOCKET_H)
#   include <sys/socket.h>
#endif

#include <vlc_network.h>

#define MAX_EMPTY_BLOCKS 200

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-udp-"

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Default caching value for outbound UDP streams. This " \
    "value should be set in milliseconds." )

#define GROUP_TEXT N_("Group packets")
#define GROUP_LONGTEXT N_("Packets can be sent one by one at the right time " \
                          "or by groups. You can choose the number " \
                          "of packets that will be sent at a time. It " \
                          "helps reducing the scheduling load on " \
                          "heavily-loaded systems." )

vlc_module_begin ()
    set_description( N_("UDP stream output") )
    set_shortname( "UDP" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_integer( SOUT_CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000, CACHING_TEXT, CACHING_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "group", 1, GROUP_TEXT, GROUP_LONGTEXT,
                                 true )

    set_capability( "sout access", 0 )
    add_shortcut( "udp" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

static const char *const ppsz_sout_options[] = {
    "caching",
    "group",
    NULL
};

/* Options handled by the libvlc network core */
static const char *const ppsz_core_options[] = {
    "dscp",
    "ttl",
    "miface",
    NULL
};

static ssize_t Write   ( sout_access_out_t *, block_t * );
static int Control( sout_access_out_t *, int, va_list );

static void* ThreadWrite( void * );

typedef struct
{
    vlc_tick_t    i_caching;
    int           i_handle;
    bool          b_mtu_warning;
    bool          dead;
    size_t        i_mtu;

    vlc_queue_t   queue;
    block_t      *p_buffer;

    vlc_thread_t  thread;
} sout_access_out_sys_t;

#define DEFAULT_PORT 1234

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_dst_addr = NULL;
    int                 i_dst_port;

    int                 i_handle;

    config_ChainParse( p_access, SOUT_CFG_PREFIX,
                       ppsz_sout_options, p_access->p_cfg );
    config_ChainParse( p_access, "",
                       ppsz_core_options, p_access->p_cfg );

    if (var_Create (p_access, "dst-port", VLC_VAR_INTEGER)
     || var_Create (p_access, "src-port", VLC_VAR_INTEGER)
     || var_Create (p_access, "dst-addr", VLC_VAR_STRING)
     || var_Create (p_access, "src-addr", VLC_VAR_STRING))
    {
        return VLC_ENOMEM;
    }

    if( !( p_sys = malloc ( sizeof( *p_sys ) ) ) )
        return VLC_ENOMEM;
    p_access->p_sys = p_sys;

    i_dst_port = DEFAULT_PORT;
    char *psz_parser = psz_dst_addr = strdup( p_access->psz_path );
    if( !psz_dst_addr )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    if (psz_parser[0] == '[')
        psz_parser = strchr (psz_parser, ']');

    psz_parser = strchr (psz_parser ? psz_parser : psz_dst_addr, ':');
    if (psz_parser != NULL)
    {
        *psz_parser++ = '\0';
        i_dst_port = atoi (psz_parser);
    }

    i_handle = net_ConnectDgram( p_this, psz_dst_addr, i_dst_port, -1,
                                 IPPROTO_UDP );
    free (psz_dst_addr);

    if( i_handle == -1 )
    {
         msg_Err( p_access, "failed to create raw UDP socket" );
         free (p_sys);
         return VLC_EGENERIC;
    }
    else
    {
        char addr[NI_MAXNUMERICHOST];
        int port;

        if (net_GetSockAddress (i_handle, addr, &port) == 0)
        {
            msg_Dbg (p_access, "source: %s port %d", addr, port);
            var_SetString (p_access, "src-addr", addr);
            var_SetInteger (p_access, "src-port", port);
        }

        if (net_GetPeerAddress (i_handle, addr, &port) == 0)
        {
            msg_Dbg (p_access, "destination: %s port %d", addr, port);
            var_SetString (p_access, "dst-addr", addr);
            var_SetInteger (p_access, "dst-port", port);
        }
    }
    shutdown( i_handle, SHUT_RD );

    p_sys->i_caching = VLC_TICK_FROM_MS(
                     var_GetInteger( p_access, SOUT_CFG_PREFIX "caching") );
    p_sys->i_handle = i_handle;
    p_sys->i_mtu = var_CreateGetInteger( p_this, "mtu" );
    p_sys->b_mtu_warning = false;
    p_sys->dead = false;
    vlc_queue_Init(&p_sys->queue, offsetof (block_t, p_next));
    p_sys->p_buffer = NULL;

    if( vlc_clone( &p_sys->thread, ThreadWrite, p_access,
                           VLC_THREAD_PRIORITY_HIGHEST ) )
    {
        msg_Err( p_access, "cannot spawn sout access thread" );
        net_Close (i_handle);
        free (p_sys);
        return VLC_EGENERIC;
    }

    p_access->pf_write = Write;
    p_access->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    vlc_queue_Kill(&p_sys->queue, &p_sys->dead);
    vlc_join( p_sys->thread, NULL );

    if( p_sys->p_buffer ) block_Release( p_sys->p_buffer );

    net_Close( p_sys->i_handle );
    free( p_sys );
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    (void)p_access;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg( args, bool * ) = false;
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;

    while( p_buffer )
    {
        block_t *p_next;
        int i_packets = 0;
        vlc_tick_t now = vlc_tick_now();

        if( !p_sys->b_mtu_warning && p_buffer->i_buffer > p_sys->i_mtu )
        {
            msg_Warn( p_access, "packet size > MTU, you should probably "
                      "increase the MTU" );
            p_sys->b_mtu_warning = true;
        }

        /* Check if there is enough space in the buffer */
        if( p_sys->p_buffer &&
            p_sys->p_buffer->i_buffer + p_buffer->i_buffer > p_sys->i_mtu )
        {
            if( p_sys->p_buffer->i_dts + p_sys->i_caching < now )
            {
                msg_Dbg( p_access, "late packet for UDP input (%"PRId64 ")",
                         now - p_sys->p_buffer->i_dts
                          - p_sys->i_caching );
            }
            vlc_queue_Enqueue(&p_sys->queue, p_sys->p_buffer);
            p_sys->p_buffer = NULL;
        }

        i_len += p_buffer->i_buffer;
        while( p_buffer->i_buffer )
        {
            size_t i_payload_size = p_sys->i_mtu;
            size_t i_write = __MIN( p_buffer->i_buffer, i_payload_size );

            i_packets++;

            if( !p_sys->p_buffer )
            {
                p_sys->p_buffer = block_Alloc( p_sys->i_mtu );
                if( !p_sys->p_buffer ) break;
                p_sys->p_buffer->i_dts = p_buffer->i_dts;
                p_sys->p_buffer->i_buffer = 0;
            }

            memcpy( p_sys->p_buffer->p_buffer + p_sys->p_buffer->i_buffer,
                    p_buffer->p_buffer, i_write );

            p_sys->p_buffer->i_buffer += i_write;
            p_buffer->p_buffer += i_write;
            p_buffer->i_buffer -= i_write;
            if ( p_buffer->i_flags & BLOCK_FLAG_CLOCK )
            {
                if ( p_sys->p_buffer->i_flags & BLOCK_FLAG_CLOCK )
                    msg_Warn( p_access, "putting two PCRs at once" );
                p_sys->p_buffer->i_flags |= BLOCK_FLAG_CLOCK;
            }

            if( p_sys->p_buffer->i_buffer == p_sys->i_mtu || i_packets > 1 )
            {
                /* Flush */
                if( p_sys->p_buffer->i_dts + p_sys->i_caching < now )
                {
                    msg_Dbg( p_access, "late packet for udp input (%"PRId64 ")",
                             vlc_tick_now() - p_sys->p_buffer->i_dts
                              - p_sys->i_caching );
                }
                vlc_queue_Enqueue(&p_sys->queue, p_sys->p_buffer);
                p_sys->p_buffer = NULL;
            }
        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

    return i_len;
}

/*****************************************************************************
 * ThreadWrite: Write a packet on the network at the good time.
 *****************************************************************************/
static void* ThreadWrite( void *data )
{
    sout_access_out_t *p_access = data;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    vlc_tick_t i_date_last = -1;
    const unsigned i_group = var_GetInteger( p_access,
                                             SOUT_CFG_PREFIX "group" );
    int i_to_send = i_group;
    unsigned i_dropped_packets = 0;
    block_t *p_pk;

    while ((p_pk = vlc_queue_DequeueKillable(&p_sys->queue,
                                             &p_sys->dead)) != NULL)
    {
        vlc_tick_t    i_date;

        i_date = p_sys->i_caching + p_pk->i_dts;
        if( i_date_last > 0 )
        {
            if( i_date - i_date_last > VLC_TICK_FROM_SEC(2) )
            {
                if( !i_dropped_packets )
                    msg_Dbg( p_access, "mmh, hole (%"PRId64" > 2s) -> drop",
                             i_date - i_date_last );

                block_Release( p_pk );

                i_date_last = i_date;
                i_dropped_packets++;
                continue;
            }
            else if( i_date - i_date_last < VLC_TICK_FROM_MS(-1) )
            {
                if( !i_dropped_packets )
                    msg_Dbg( p_access, "mmh, packets in the past (%"PRId64")",
                             i_date_last - i_date );
            }
        }

        i_to_send--;
        if( !i_to_send || (p_pk->i_flags & BLOCK_FLAG_CLOCK) )
        {
            vlc_tick_wait( i_date );
            i_to_send = i_group;
        }
        if ( send( p_sys->i_handle, p_pk->p_buffer, p_pk->i_buffer, 0 ) == -1 )
            msg_Warn( p_access, "send error: %s", vlc_strerror_c(errno) );

        if( i_dropped_packets )
        {
            msg_Dbg( p_access, "dropped %i packets", i_dropped_packets );
            i_dropped_packets = 0;
        }

        i_date_last = i_date;

#if 1
        i_date = vlc_tick_now() - i_date;
        if ( i_date > VLC_TICK_FROM_MS(20) )
        {
            msg_Dbg( p_access, "packet has been sent too late (%"PRId64 ")",
                     i_date );
        }
#endif

        block_Release( p_pk );

    }
    return NULL;
}
