/*****************************************************************************
 * http.c: HTTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: http.c,v 1.19 2002/07/31 20:56:50 sam Exp $
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

static int  SetProgram ( input_thread_t *, pgrm_descriptor_t * );  
static void Seek       ( input_thread_t *, off_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HTTP access module") );
    set_capability( "access", 0 );
    add_shortcut( "http4" );
    add_shortcut( "http6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * _input_socket_t: private access plug-in data, modified to add private
 *                  fields
 *****************************************************************************/
typedef struct _input_socket_s
{
    input_socket_t      _socket;

    char *              psz_network;
    network_socket_t    socket_desc;
    char                psz_buffer[256];
    char *              psz_name;
} _input_socket_t;

/*****************************************************************************
 * HTTPConnect: connect to the server and seek to i_tell
 *****************************************************************************/
static int HTTPConnect( input_thread_t * p_input, off_t i_tell )
{
    _input_socket_t *   p_access_data = p_input->p_access_data;
    module_t *          p_network;
    char                psz_buffer[256];
    byte_t *            psz_parser;
    int                 i_returncode, i;
    char *              psz_return_alpha;

    /* Find an appropriate network module */
    p_input->p_private = (void*) &p_access_data->socket_desc;
    p_network = module_Need( p_input, "network", p_access_data->psz_network );
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_input, p_network );

    p_access_data->_socket.i_handle = p_access_data->socket_desc.i_handle;

#   define HTTP_USERAGENT "User-Agent: " COPYRIGHT_MESSAGE "\r\n"
#   define HTTP_END       "\r\n"
 
    if ( p_input->stream.b_seekable )
    {
         snprintf( psz_buffer, sizeof(psz_buffer),
                   "%s"
                   "Range: bytes=%lld-\r\n"
                   HTTP_USERAGENT HTTP_END,
                   p_access_data->psz_buffer, i_tell );
    }
    else
    {
         snprintf( psz_buffer, sizeof(psz_buffer),
                   "%s"
                   HTTP_USERAGENT HTTP_END,
                   p_access_data->psz_buffer );
    }
    psz_buffer[sizeof(psz_buffer) - 1] = '\0';

    /* Send GET ... */
    if( send( p_access_data->_socket.i_handle, psz_buffer,
               strlen( psz_buffer ), 0 ) == (-1) )
    {
        msg_Err( p_input, "cannot send request (%s)", strerror(errno) );
        input_FDNetworkClose( p_input );
        return( -1 );
    }

    /* Prepare the input thread for reading. */ 
    p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;

    /* FIXME: we shouldn't have to do that ! It's UGLY but mandatory because
     * input_FillBuffer assumes p_input->pf_read exists */
    p_input->pf_read = input_FDNetworkRead;

    while( !input_FillBuffer( p_input ) )
    {
        if( p_input->b_die || p_input->b_error )
        {
            input_FDNetworkClose( p_input );
            return( -1 );
        }
    }

    /* Parse HTTP header. */
#define MAX_LINE 1024

    /* get the returncode */
    if( input_Peek( p_input, &psz_parser, MAX_LINE ) <= 0 )
    {
        msg_Err( p_input, "not enough data" );
        input_FDNetworkClose( p_input );
        return( -1 );
    }

    if( !strncmp( psz_parser, "HTTP/1.",
                  strlen("HTTP/1.") ) )
    {
        psz_parser += strlen("HTTP 1.") + 2;
        i_returncode = atoi( psz_parser );
        msg_Dbg( p_input, "HTTP server replied: %i", i_returncode );
        psz_parser += 4;
        for ( i = 0; psz_parser[i] != '\r' || psz_parser[i+1] != '\n'; i++ )
        {
            ;
        }
        psz_return_alpha = malloc( i + 1 );
        memcpy( psz_return_alpha, psz_parser, i );
        psz_return_alpha[i] = '\0';
    }
    else
    {
        msg_Err( p_input, "invalid http reply" );
        return -1;
    }
    
    if ( i_returncode >= 400 ) /* something is wrong */
    {
        msg_Err( p_input, "%i %s", i_returncode,
                 psz_return_alpha );
        return -1;
    }
    
    for( ; ; ) 
    {
        if( input_Peek( p_input, &psz_parser, MAX_LINE ) <= 0 )
        {
            msg_Err( p_input, "not enough data" );
            input_FDNetworkClose( p_input );
            return( -1 );
        }

        if( psz_parser[0] == '\r' && psz_parser[1] == '\n' )
        {
            /* End of header. */
            p_input->p_current_data += 2;
            break;
        }

        if( !strncmp( psz_parser, "Content-Length: ",
                      strlen("Content-Length: ") ) )
        {
            psz_parser += strlen("Content-Length: ");
            vlc_mutex_lock( &p_input->stream.stream_lock );
#ifdef HAVE_ATOLL
            p_input->stream.p_selected_area->i_size = atoll( psz_parser )
                                                        + i_tell;
#else
            /* FIXME : this won't work for 64-bit lengths */
            p_input->stream.p_selected_area->i_size = atoi( psz_parser )
                                                        + i_tell;
#endif
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        while( *psz_parser != '\r' && psz_parser < p_input->p_last_data )
        {
            psz_parser++;
        }
        p_input->p_current_data = psz_parser + 2;
    }

    if( p_input->stream.p_selected_area->i_size )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.p_selected_area->i_tell = i_tell
            + (p_input->p_last_data - p_input->p_current_data);
        p_input->stream.b_seekable = 1;
        p_input->stream.b_changed = 1;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    return( 0 );
}

/*****************************************************************************
 * Open: parse URL and open the remote file at the beginning
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    _input_socket_t *   p_access_data;
    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_path = "";
    char *              psz_proxy;
    int                 i_server_port = 0;

    p_access_data = p_input->p_access_data = malloc( sizeof(_input_socket_t) );
    if( p_access_data == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free(psz_name);
        return( -1 );
    }

    p_access_data->psz_name = psz_name;
    p_access_data->psz_network = "";
    if( config_GetInt( p_input, "ipv4" ) )
    {
        p_access_data->psz_network = "ipv4";
    }
    if( config_GetInt( p_input, "ipv6" ) )
    {
        p_access_data->psz_network = "ipv6";
    }
    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "http6", 6 ) )
        {
            p_access_data->psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "http4", 6 ) )
        {
            p_access_data->psz_network = "ipv4";
        }
    }

    /* Parse psz_name syntax :
     * //<hostname>[:<port>][/<path>] */
    while( *psz_parser == '/' )
    {
        psz_parser++;
    }
    psz_server_addr = psz_parser;

    while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
    {
        psz_parser++;
    }

    if ( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_server_port = psz_parser;

        while( *psz_parser && *psz_parser != '/' )
        {
            psz_parser++;
        }
    }

    if( *psz_parser == '/' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_path = psz_parser;
    }

    /* Convert port format */
    if( *psz_server_port )
    {
        i_server_port = strtol( psz_server_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            msg_Err( p_input, "cannot parse server port near %s", psz_parser );
            free( p_input->p_access_data );
            free( psz_name );
            return( -1 );
        }
    }

    if( i_server_port == 0 )
    {
        i_server_port = 80;
    }

    if( !*psz_server_addr )
    {
        msg_Err( p_input, "no server given" );
        free( p_input->p_access_data );
        free( psz_name );
        return( -1 );
    }

    /* Check proxy */
    if( (psz_proxy = getenv( "http_proxy" )) != NULL && *psz_proxy )
    {
        /* http://myproxy.mydomain:myport/ */
        int                 i_proxy_port = 0;
 
        /* Skip the protocol name */
        while( *psz_proxy && *psz_proxy != ':' )
        {
            psz_proxy++;
        }
 
        /* Skip the "://" part */
        while( *psz_proxy && (*psz_proxy == ':' || *psz_proxy == '/') )
        {
            psz_proxy++;
        }
 
        /* Found a proxy name */
        if( *psz_proxy )
        {
            char *psz_port = psz_proxy;
 
            /* Skip the hostname part */
            while( *psz_port && *psz_port != ':' && *psz_port != '/' )
            {
                psz_port++;
            }
 
            /* Found a port name */
            if( *psz_port )
            {
                char * psz_junk;
 
                /* Replace ':' with '\0' */
                *psz_port = '\0';
                psz_port++;
 
                psz_junk = psz_port;
                while( *psz_junk && *psz_junk != '/' )
                {
                    psz_junk++;
                }
 
                if( *psz_junk )
                {
                    *psz_junk = '\0';
                }
 
                if( *psz_port != '\0' )
                {
                    i_proxy_port = atoi( psz_port );
                }
            }
        }
        else
        {
            msg_Err( p_input, "http_proxy environment variable is invalid!" );
            free( p_input->p_access_data );
            free( psz_name );
            return( -1 );
        }

        p_access_data->socket_desc.i_type = NETWORK_TCP;
        p_access_data->socket_desc.psz_server_addr = psz_proxy;
        p_access_data->socket_desc.i_server_port = i_proxy_port;

        snprintf( p_access_data->psz_buffer, sizeof(p_access_data->psz_buffer),
                  "GET http://%s:%d/%s\r\n HTTP/1.0\r\n",
                  psz_server_addr, i_server_port, psz_path );
    }
    else
    {
        /* No proxy, direct connection. */
        p_access_data->socket_desc.i_type = NETWORK_TCP;
        p_access_data->socket_desc.psz_server_addr = psz_server_addr;
        p_access_data->socket_desc.i_server_port = i_server_port;

        snprintf( p_access_data->psz_buffer, sizeof(p_access_data->psz_buffer),
                  "GET /%s HTTP/1.1\r\nHost: %s\r\n",
                  psz_path, psz_server_addr );
    }
    p_access_data->psz_buffer[sizeof(p_access_data->psz_buffer) - 1] = '\0';

    msg_Dbg( p_input, "opening server=%s port=%d path=%s",
                      psz_server_addr, i_server_port, psz_path );

    p_input->pf_read = input_FDNetworkRead;
    p_input->pf_set_program = SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = Seek;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->i_mtu = 0;
 
    if( HTTPConnect( p_input, 0 ) )
    {
        char * psz_pos = strstr(p_access_data->psz_buffer, "HTTP/1.1");
        p_input->stream.b_seekable = 0;
        psz_pos[7] = '0';
        if( HTTPConnect( p_input, 0 ) )
        {
            free( p_input->p_access_data );
            free( psz_name );
            return( -1 );
        }
    }
    return 0;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    _input_socket_t * p_access_data = 
        (_input_socket_t *)p_input->p_access_data;

    free( p_access_data->psz_name );
    input_FDNetworkClose( p_input );
}

/*****************************************************************************
 * SetProgram: do nothing
 *****************************************************************************/
static int SetProgram( input_thread_t * p_input,
                       pgrm_descriptor_t * p_program )
{
    return( 0 );
}

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    _input_socket_t *   p_access_data = p_input->p_access_data;
    close( p_access_data->_socket.i_handle );
    msg_Dbg( p_input, "seeking to position %lld", i_pos );
    HTTPConnect( p_input, i_pos );
}

