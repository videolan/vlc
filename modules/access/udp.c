/*****************************************************************************
 * udp.c: raw UDP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: udp.c,v 1.5 2002/12/11 20:13:50 fenrir Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
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
 * Local prototypes
 *****************************************************************************/
static int  Open       ( vlc_object_t * );
static void Close      ( vlc_object_t * );
static ssize_t Read    ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("raw UDP access module") );
    set_capability( "access", 0 );
    add_shortcut( "udp" );
    add_shortcut( "udpstream" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    input_socket_t *    p_access_data;
    module_t *          p_network;
    char *              psz_network = "";
    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_bind_addr = "";
    char *              psz_bind_port = "";
    int                 i_bind_port = 0, i_server_port = 0;
    network_socket_t    socket_desc;

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
        if( !strncmp( p_input->psz_access, "udp6", 5 ) )
        {
            psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "udp4", 5 ) )
        {
            psz_network = "ipv4";
        }
    }

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */

    if( *psz_parser && *psz_parser != '@' )
    {
        /* Found server */
        psz_server_addr = psz_parser;

        while( *psz_parser && *psz_parser != ':' && *psz_parser != '@' )
        {
            if( *psz_parser == '[' )
            {
                /* IPv6 address */
                while( *psz_parser && *psz_parser != ']' )
                {
                    psz_parser++;
                }
            }
            psz_parser++;
        }

        if( *psz_parser == ':' )
        {
            /* Found server port */
            *psz_parser = '\0'; /* Terminate server name */
            psz_parser++;
            psz_server_port = psz_parser;

            while( *psz_parser && *psz_parser != '@' )
            {
                psz_parser++;
            }
        }
    }

    if( *psz_parser == '@' )
    {
        /* Found bind address or bind port */
        *psz_parser = '\0'; /* Terminate server port or name if necessary */
        psz_parser++;

        if( *psz_parser && *psz_parser != ':' )
        {
            /* Found bind address */
            psz_bind_addr = psz_parser;

            while( *psz_parser && *psz_parser != ':' )
            {
                if( *psz_parser == '[' )
                {
                    /* IPv6 address */
                    while( *psz_parser && *psz_parser != ']' )
                    {
                        psz_parser++;
                    }
                }
                psz_parser++;
            }
        }

        if( *psz_parser == ':' )
        {
            /* Found bind port */
            *psz_parser = '\0'; /* Terminate bind address if necessary */
            psz_parser++;

            psz_bind_port = psz_parser;
        }
    }

    /* Convert ports format */
    if( *psz_server_port )
    {
        i_server_port = strtol( psz_server_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            msg_Err( p_input, "cannot parse server port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    if( *psz_bind_port )
    {
        i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            msg_Err( p_input, "cannot parse bind port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    p_input->pf_read = Read;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( *psz_server_addr || i_server_port )
    {
        msg_Err( p_input, "this UDP syntax is deprecated; the server argument will be");
        msg_Err( p_input, "ignored (%s:%d). If you wanted to enter a multicast address",
                          psz_server_addr, i_server_port);
        msg_Err( p_input, "or local port, type : %s:@%s:%d",
                          *p_input->psz_access ? p_input->psz_access : "udp",
                          psz_server_addr, i_server_port );

        i_server_port = 0;
        psz_server_addr = "";
    }
 
    msg_Dbg( p_input, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = psz_bind_addr;
    socket_desc.i_bind_port = i_bind_port;
    socket_desc.psz_server_addr = psz_server_addr;
    socket_desc.i_server_port = i_server_port;

    /* Find an appropriate network module */
    p_input->p_private = (void*) &socket_desc;
    p_network = module_Need( p_input, "network", psz_network );
    free(psz_name);
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_input, p_network );
    
    p_access_data = malloc( sizeof(input_socket_t) );
    p_input->p_access_data = (access_sys_t *)p_access_data;

    if( p_access_data == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }

    p_access_data->i_handle = socket_desc.i_handle;
    p_input->i_mtu = socket_desc.i_mtu;

    return( 0 );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;

    msg_Info( p_input, "closing UDP target `%s'", p_input->psz_source );

#ifdef UNDER_CE
    CloseHandle( (HANDLE)p_access_data->i_handle );
#elif defined( WIN32 )
    closesocket( p_access_data->i_handle );
#else
    close( p_access_data->i_handle );
#endif

    free( p_access_data );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
#ifdef UNDER_CE
    return -1;

#else
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_access_data->i_handle, &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    i_ret = select( p_access_data->i_handle + 1, &fds,
                    NULL, NULL, &timeout );

    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
    }
    else if( i_ret > 0 )
    {
        ssize_t i_recv = recv( p_access_data->i_handle, p_buffer, i_len, 0 );

        if( i_recv < 0 )
        {
            msg_Err( p_input, "recv failed (%s)", strerror(errno) );
        }

        return i_recv;
    }

    return 0;

#endif
}

