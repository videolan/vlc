/*****************************************************************************
 * http.c: HTTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: http.c,v 1.14 2002/12/06 12:18:11 sigmunau Exp $
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
#include <string.h>
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
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
static ssize_t Read    ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define PROXY_TEXT N_("Use an http proxy")
#define PROXY_LONGTEXT N_( \
    "The htpp proxy must be in the form http://myproxy.mydomain:myport" )
vlc_module_begin();
    add_category_hint( N_("http"), NULL );
    add_string( "http-proxy", NULL, NULL, PROXY_TEXT, PROXY_LONGTEXT );
    set_description( _("HTTP access module") );
    set_capability( "access", 0 );
    add_shortcut( "http" );
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

#define MAX_LINE 1024

/*****************************************************************************
 * HTTPConnect: connect to the server and seek to i_tell
 *****************************************************************************/
static int HTTPConnect( input_thread_t * p_input, off_t i_tell )
{
    _input_socket_t *   p_access_data;
    module_t *          p_network;
    char                psz_buffer[1024];
    byte_t *            psz_parser;
    int                 i_pos, i_returncode, i, i_size;
    char *              psz_return_alpha;
    char *              psz_protocol;
    char                psz_line[MAX_LINE];
    char *              psz_header_name;
    char *              psz_header_value;
    playlist_t *        p_playlist;
    
    /* Find an appropriate network module */
    p_access_data = (_input_socket_t *)p_input->p_access_data;
    p_input->p_private = (void*) &p_access_data->socket_desc;
    p_network = module_Need( p_input, "network", p_access_data->psz_network );
    if( p_network == NULL )
    {
        return VLC_ENOMOD;
    }
    module_Unneed( p_input, p_network );

    p_access_data->_socket.i_handle = p_access_data->socket_desc.i_handle;

#   define HTTP_USERAGENT "User-Agent: " COPYRIGHT_MESSAGE "\r\n"
#   define HTTP_END       "\r\n"
 
    if ( p_input->stream.b_seekable )
    {
         snprintf( psz_buffer, sizeof(psz_buffer),
                   "%s"
                   "Range: bytes="I64Fd"-\r\n"
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
    printf(psz_buffer);
    /* Send GET ... */
    if( send( p_access_data->_socket.i_handle, psz_buffer,
               strlen( psz_buffer ), 0 ) == (-1) )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_input, "cannot send request (%s)", strerror(errno) );
#else
        msg_Err( p_input, "cannot send request" );
#endif
        Close( VLC_OBJECT(p_input) );
        return VLC_EGENERIC;
    }

    /* Prepare the input thread for reading. */ 
    p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;

    /* FIXME: we shouldn't have to do that ! It's UGLY but mandatory because
     * input_FillBuffer assumes p_input->pf_read exists */
    p_input->pf_read = Read;

    while( !input_FillBuffer( p_input ) )
    {
        if( p_input->b_die || p_input->b_error )
        {
            Close( VLC_OBJECT(p_input) );
            return VLC_EGENERIC;
        }
    }

    /* Parse HTTP header. */

    /* get the returncode */
    if( (i_size = input_Peek( p_input, &psz_parser, MAX_LINE )) <= 0 )
    {
        msg_Err( p_input, "not enough data" );
        Close( VLC_OBJECT(p_input) );
        return VLC_EGENERIC;
    }
    if( ( ( i_size >= sizeof("HTTP/1.") + 1 ) &&
            !strncmp( psz_parser, "HTTP/1.", strlen("HTTP/1.") ) ) )
    {
        psz_protocol = "HTTP";
    }
    else if( ( i_size >= sizeof("ICY") &&
             !strncmp( psz_parser, "ICY", sizeof("ICY") - 1 ) ) )
    {
        psz_protocol = "ICY";
        if( !p_input->psz_demux || !*p_input->psz_demux  )
        {
            msg_Info( p_input, "ICY server --> mp3 demuxer selected" );
            p_input->psz_demux = "mp3";    // FIXME strdup ?
        }
    }
    else
    {
        msg_Err( p_input, "invalid http reply" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_input, "detected %s server", psz_protocol );

    if( !strncmp( psz_protocol, "ICY", sizeof("ICY") - 1 ) )
    { // ICY server
        psz_parser += (sizeof("ICY") - 1);
        i_size -= (sizeof("ICY") - 1);
    }
    else
    { // HTTP/1.0 or HTTP/1.1
        psz_parser += sizeof("HTTP/1.") + 1;
        i_size -= sizeof("HTTP/1.") + 1;
    }

    i_returncode = atoi( (char*)psz_parser );
    msg_Dbg( p_input, "%s server replied: %i", psz_protocol, i_returncode );
    psz_parser += 4;
    i_size -= 4;
    for ( i = 0; (i < i_size -1) && ((psz_parser[i] != '\r') ||
      (psz_parser[i+1] != '\n')); i++ )
    {
        ;
    }
    /* check we actually parsed something */
    if ( (i == i_size - 1) && (psz_parser[i+1] != '\n') )
    {
        msg_Err( p_input, "stream not compliant with HTTP/1.x" );
        return VLC_EGENERIC;
    }

    psz_return_alpha = malloc( i + 1 );
    memcpy( psz_return_alpha, psz_parser, i );
    psz_return_alpha[i] = '\0';
    p_input->p_current_data = &psz_parser[i + 2];

    /* read header lines */
    for ( ; ; )
    {
        if( ( i_size = input_Peek( p_input, &psz_parser, MAX_LINE ) ) <= 0 )
        {
            msg_Err( p_input, "not enough data" );
            Close( VLC_OBJECT(p_input) );
            return VLC_EGENERIC;
        }
        
        i_pos = 0;
        while( i_size && psz_parser[i_pos] != '\r'
               && psz_parser[i_pos + 1] != '\n' )
        {
            psz_line[i_pos] = psz_parser[i_pos];
            i_pos++;
            i_size--;
        }
        p_input->p_current_data = &psz_parser[i_pos + 2];
        if (!i_pos)
        {
            break; /* end of header*/
        }
        psz_line[i_pos] = '\0';
        psz_parser = strchr( psz_line, ':' );
        if ( !psz_parser )
        {
            msg_Err( p_input, "malformed header line: %s", psz_line );
            return VLC_EGENERIC;
        }
        *psz_parser = '\0';
        psz_parser++;
        psz_header_name = psz_line;
        while ( *psz_parser == ' ' || *psz_parser == '\t' )
        {
            psz_parser++;
        }
        psz_header_value = psz_parser;
/*        msg_Dbg( p_input, "found header \"%s: %s\"",
          psz_header_name, psz_header_value );*/
        
        if( !strcasecmp( psz_header_name, "Content-Length" ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
#ifdef HAVE_ATOLL
            p_input->stream.p_selected_area->i_size = atoll( (char*)psz_header_value )
                                                        + i_tell;
#else
            /* FIXME : this won't work for 64-bit lengths */
            p_input->stream.p_selected_area->i_size = atoi( (char*)psz_header_value )
                                                        + i_tell;
#endif
            msg_Dbg( p_input, "stream size is %d", p_input->stream.p_selected_area->i_size );
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        /* redirection support */
        else if ( ( i_returncode == 301 ||
                    i_returncode == 302 ||
                    i_returncode == 303 ||
                    i_returncode == 307 )
                  && !strcasecmp( psz_header_name, "location" ) )
        {
            p_playlist = (playlist_t *) vlc_object_find( p_input,
                                                         VLC_OBJECT_PLAYLIST,
                                                         FIND_ANYWHERE );
            if( !p_playlist )
            {
                msg_Err( p_input, "redirection failed: can't find playlist" );
                return VLC_EGENERIC;
            }
            msg_Dbg( p_input, "%i %s: redircted to %s",
                       i_returncode, psz_return_alpha, psz_header_value );
            p_playlist->pp_items[p_playlist->i_index]->b_autodeletion =
                VLC_TRUE;
            playlist_Add( p_playlist, psz_header_value,
                          PLAYLIST_APPEND|PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
        /* parse other headers here */
    }
    
    if ( i_returncode >= 400 ) /* something is wrong */
    {
        msg_Err( p_input, "%i %s", i_returncode, psz_return_alpha );
        return VLC_EGENERIC;
    }


    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( !strcmp( psz_protocol, "ICY") )
    {
        p_input->stream.b_seekable = 0;
    }
    else
    {
        p_input->stream.b_seekable = 1;
    }

    if( p_input->stream.p_selected_area->i_size )
    {
        p_input->stream.p_selected_area->i_tell = i_tell
            + (p_input->p_last_data - p_input->p_current_data);
    }
    else
    {
        p_input->stream.b_seekable = 0;
    }
    p_input->stream.b_changed = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
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
    char *              psz_proxy, *psz_proxy_orig;
    int                 i_server_port = 0;

    p_access_data = malloc( sizeof(_input_socket_t) );
    p_input->p_access_data = (access_sys_t *)p_access_data;
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

    /* Check proxy config variable */
    if( (psz_proxy_orig = config_GetPsz( p_input, "http-proxy" )) == NULL )
        /* Check proxy environment variable */
        if( (psz_proxy_orig = getenv( "http_proxy" )) != NULL )
            psz_proxy_orig = strdup( psz_proxy_orig );

    psz_proxy = psz_proxy_orig;
    if( psz_proxy != NULL && *psz_proxy )
    {
        /* http://myproxy.mydomain:myport/ */
        int i_proxy_port = 0;
 
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

            psz_proxy = strdup( psz_proxy );

            msg_Dbg( p_input, "using http proxy server=%s port=%d",
                     psz_proxy, i_proxy_port );
        }
        else
        {
            msg_Err( p_input, "http proxy %s is invalid!", psz_proxy_orig );
            free( p_input->p_access_data );
            free( psz_name );
            if( psz_proxy_orig ) free( psz_proxy_orig );
            return( -1 );
        }

        if( psz_proxy_orig ) free( psz_proxy_orig );

        p_access_data->socket_desc.psz_server_addr = psz_proxy;
        p_access_data->socket_desc.i_server_port = i_proxy_port;
        p_access_data->socket_desc.i_type = NETWORK_TCP;

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

    p_input->pf_read = Read;
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
        /* Request failed, try again with HTTP/1.0 */
        char * psz_pos = strstr( p_access_data->psz_buffer, "HTTP/1.1" );

        if( !psz_pos )
        {
            return VLC_EGENERIC;
        }

        p_input->stream.b_seekable = 0;
        psz_pos[7] = '0';
        if( HTTPConnect( p_input, 0 ) )
        {
            free( p_input->p_access_data );
            free( psz_name );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    int i_handle = ((network_socket_t *)p_input->p_access_data)->i_handle;
    _input_socket_t * p_access_data = 
        (_input_socket_t *)p_input->p_access_data;

    free( p_access_data->psz_name );

    msg_Info( p_input, "closing HTTP target `%s'", p_input->psz_source );

#if defined( WIN32 ) || defined( UNDER_CE )
    closesocket( i_handle );
#else
    close( i_handle );
#endif

    free( p_access_data );
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
    _input_socket_t *p_access_data = (_input_socket_t*)p_input->p_access_data;
#if defined( WIN32 ) || defined( UNDER_CE )
    closesocket( p_access_data->_socket.i_handle );
#else
    close( p_access_data->_socket.i_handle );
#endif
    msg_Dbg( p_input, "seeking to position "I64Fd, i_pos );
    HTTPConnect( p_input, i_pos );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
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

#ifdef HAVE_ERRNO_H
    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
    }
#else
    if( i_ret == -1 )
    {
        msg_Err( p_input, "network select error" );
    }
#endif
    else if( i_ret > 0 )
    {
        ssize_t i_recv = recv( p_access_data->i_handle, p_buffer, i_len, 0 );

        if( i_recv > 0 )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->stream.p_selected_area->i_tell += i_recv;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        if( i_recv < 0 )
        {
#ifdef HAVE_ERRNO_H
            msg_Err( p_input, "recv failed (%s)", strerror(errno) );
#else
            msg_Err( p_input, "recv failed" );
#endif
        }
        return i_recv;
    }

    return 0;
}
