/*****************************************************************************
 * tcp.c: TCP access plug-in
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: tcp.c,v 1.2 2003/12/04 16:49:43 sam Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for udp streams. This " \
    "value should be set in miliseconds units." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("TCP input") );
    add_category_hint( N_("TCP"), NULL , VLC_TRUE );
    add_integer( "tcp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access", 0 );
    add_shortcut( "tcp" );
    add_shortcut( "tcp4" );
    add_shortcut( "tcp6" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int fd;
};

static ssize_t  Read ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;

    char           *psz_network;
    char           *psz_dup = strdup(p_input->psz_name);
    char           *psz_parser = psz_dup;

    network_socket_t sock;
    module_t         *p_network;

    vlc_value_t     val;

    /* Select ip version */
    psz_network = "";
    if( config_GetInt( p_input, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    if( config_GetInt( p_input, "ipv6" ) )
    {
        psz_network = "ipv6";
    }
    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "tcp6", 5 ) )
        {
            psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "tcp4", 5 ) )
        {
            psz_network = "ipv4";
        }
    }

    /* Parse server:port */
    while( *psz_parser && *psz_parser != ':' )
    {
        if( *psz_parser == '[' )
        {
            /* IPV6 */
            while( *psz_parser && *psz_parser  != ']' )
            {
                psz_parser++;
            }
        }
        psz_parser++;
    }

    if( *psz_parser != ':' || psz_parser == psz_dup )
    {
        msg_Err( p_input, "you have to provide server:port addresse" );
        free( psz_dup );
        return VLC_EGENERIC;
    }

    *psz_parser++ = '\0';

    /* Prepare the network_socket_t structure */
    sock.i_type = NETWORK_TCP;
    sock.psz_bind_addr = "";
    sock.i_bind_port = 0;
    sock.psz_server_addr = psz_dup;
    sock.i_server_port = atoi( psz_parser );
    sock.i_ttl           = 0;

    if( sock.i_server_port <= 0 )
    {
        msg_Err( p_input, "invalid port number (%d)", sock.i_server_port );
        free( psz_dup );
        return VLC_EGENERIC;
    }

    /* connecting */
    msg_Dbg( p_input, "opening server=%s:%d",
             sock.psz_server_addr, sock.i_server_port );
    p_input->p_private = (void*)&sock;
    p_network = module_Need( p_input, "network", psz_network );
    free( psz_dup );
    if( p_network == NULL )
    {
        return VLC_EGENERIC;
    }
    module_Unneed( p_input, p_network );

    p_input->p_access_data = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->fd = sock.i_handle;

    p_input->pf_read = Read;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;  /* FIXME ? */
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    p_input->i_mtu = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_input, "tcp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "tcp-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;

    msg_Info( p_input, "closing TCP target `%s'", p_input->psz_source );

#ifdef UNDER_CE
    CloseHandle( (HANDLE)p_sys->fd );
#elif defined( WIN32 )
    closesocket( p_sys->fd );
#else
    close( p_sys->fd );
#endif

    free( p_sys );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
#ifdef UNDER_CE
    return -1;
#else
    access_sys_t   *p_sys = p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    ssize_t         i_recv;
    int             i_ret;

    do
    {
        if( p_input->b_die || p_input->b_error )
        {
            return 0;
        }

        /* Initialize file descriptor set */
        FD_ZERO( &fds );
        FD_SET( p_sys->fd, &fds );

        /* We'll wait 0.5 second if nothing happens */
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
    } while( ( i_ret = select( p_sys->fd + 1, &fds, NULL, NULL, &timeout )) == 0 ||
             ( i_ret < 0 && errno == EINTR ) );

    if( i_ret < 0 )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
        return -1;
    }

    if( ( i_recv = recv( p_sys->fd, p_buffer, i_len, 0 ) ) < 0 )
    {
        msg_Err( p_input, "recv failed (%s)", strerror(errno) );
    }
    return i_recv;
#endif
}

