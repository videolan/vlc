/*****************************************************************************
 * udp.c: raw UDP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 * Copyright (C) 2007 Remi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Remi Denis-Courmont
 *
 * Reviewed: 23 October 2003, Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

#include <errno.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_network.h>
#include <vlc_block.h>

#define MTU 65535

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

#define BUFFER_TEXT N_("Receive buffer")
#define BUFFER_LONGTEXT N_("UDP receive buffer size (bytes)" )

vlc_module_begin ()
    set_shortname( N_("UDP" ) )
    set_description( N_("UDP input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_obsolete_integer( "server-port" ) /* since 2.0.0 */
    add_integer( "udp-buffer", 0x400000, BUFFER_TEXT, BUFFER_LONGTEXT, true )

    set_capability( "access", 0 )
    add_shortcut( "udp", "udpstream", "udp4", "udp6" )

    set_callbacks( Open, Close )
vlc_module_end ()

struct access_sys_t
{
    int fd;
    size_t fifo_size;
    block_fifo_t *fifo;
    vlc_thread_t thread;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *BlockUDP( access_t * );
static int Control( access_t *, int, va_list );
static void* ThreadRead( void *data );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;

    char *psz_name = strdup( p_access->psz_location );
    char *psz_parser;
    const char *psz_server_addr, *psz_bind_addr = "";
    int  i_bind_port = 1234, i_server_port = 0;

    access_sys_t *sys = malloc( sizeof( *sys ) );
    if( unlikely( sys == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = sys;

    /* Set up p_access */
    access_InitFields( p_access );
    ACCESS_SET_CALLBACKS( NULL, BlockUDP, Control, NULL );

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */
    psz_parser = strchr( psz_name, '@' );
    if( psz_parser != NULL )
    {
        /* Found bind address and/or bind port */
        *psz_parser++ = '\0';
        psz_bind_addr = psz_parser;

        if( psz_bind_addr[0] == '[' )
            /* skips bracket'd IPv6 address */
            psz_parser = strchr( psz_parser, ']' );

        if( psz_parser != NULL )
        {
            psz_parser = strchr( psz_parser, ':' );
            if( psz_parser != NULL )
            {
                *psz_parser++ = '\0';
                i_bind_port = atoi( psz_parser );
            }
        }
    }

    psz_server_addr = psz_name;
    psz_parser = ( psz_server_addr[0] == '[' )
        ? strchr( psz_name, ']' ) /* skips bracket'd IPv6 address */
        : psz_name;

    if( psz_parser != NULL )
    {
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser != NULL )
        {
            *psz_parser++ = '\0';
            i_server_port = atoi( psz_parser );
        }
    }

    msg_Dbg( p_access, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    sys->fd = net_OpenDgram( p_access, psz_bind_addr, i_bind_port,
                             psz_server_addr, i_server_port, IPPROTO_UDP );
    free( psz_name );
    if( sys->fd == -1 )
    {
        msg_Err( p_access, "cannot open socket" );
        goto error;
    }

    sys->fifo = block_FifoNew();
    if( unlikely( sys->fifo == NULL ) )
    {
        net_Close( sys->fd );
        goto error;
    }

    sys->fifo_size = var_InheritInteger( p_access, "udp-buffer");

    if( vlc_clone( &sys->thread, ThreadRead, p_access,
                   VLC_THREAD_PRIORITY_INPUT ) )
    {
        block_FifoRelease( sys->fifo );
        net_Close( sys->fd );
error:
        free( sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *sys = p_access->p_sys;

    vlc_cancel( sys->thread );
    vlc_join( sys->thread, NULL );
    block_FifoRelease( sys->fifo );
    net_Close( sys->fd );
    free( sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool    *pb_bool;
    int64_t *pi_64;

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger(p_access, "network-caching");
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * BlockUDP:
 *****************************************************************************/
static block_t *BlockUDP( access_t *p_access )
{
    access_sys_t *sys = p_access->p_sys;

    return block_FifoGet( sys->fifo );
}

/*****************************************************************************
 * ThreadRead: Pull packets from socket as soon as possible.
 *****************************************************************************/
static void* ThreadRead( void *data )
{
    access_t *access = data;
    access_sys_t *sys = access->p_sys;

    for( ;; )
    {
        block_t *pkt;
        ssize_t len;

        block_FifoPace( sys->fifo, SIZE_MAX, sys->fifo_size );

        pkt = block_Alloc( MTU );
        if( unlikely( pkt == NULL ) )
            break;

        block_cleanup_push( pkt );
        len = net_Read( access, sys->fd, NULL, pkt->p_buffer, MTU, false );
        vlc_cleanup_pop();

        if( len == -1 )
        {
            block_Release( pkt );

            if( errno == EINTR )
                break;
            continue;
        }

        pkt = block_Realloc( pkt, 0, len );
        block_FifoPut( sys->fifo, pkt );
    }

    block_FifoWake( sys->fifo );
    return NULL;
}
