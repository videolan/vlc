/*****************************************************************************
 * http.c: HTTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: http.c,v 1.43 2003/08/02 19:30:35 bigben Exp $
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

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
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

#include "vlc_playlist.h"
#include "network.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open       ( vlc_object_t * );
static void Close      ( vlc_object_t * );

static void Seek       ( input_thread_t *, off_t );
static ssize_t Read    ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define PROXY_TEXT N_("Specify an HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "Specify an HTTP proxy to use. It must be in the form " \
    "http://myproxy.mydomain:myport. If none is specified, the HTTP_PROXY " \
    "environment variable will be tried." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for http streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    add_category_hint( N_("http"), NULL, VLC_FALSE );
    add_string( "http-proxy", NULL, NULL, PROXY_TEXT, PROXY_LONGTEXT, VLC_FALSE );
    add_integer( "http-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "http-user", NULL, NULL, "HTTP user name", "HTTP user name for Basic Authentification", VLC_FALSE );
    add_string( "http-pwd", NULL , NULL, "HTTP password", "HTTP password for Basic Authentification", VLC_FALSE );
    set_description( _("HTTP input") );
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
#define MAX_ANSWER_SIZE 1024
#define MAX_QUERY_SIZE 1024

typedef struct _input_socket_s
{
    input_socket_t      _socket;

    char *              psz_network;
    network_socket_t    socket_desc;
    char                psz_buffer[MAX_QUERY_SIZE];
    char                psz_auth_string[MAX_QUERY_SIZE];
    char *              psz_name;
} _input_socket_t;

/*****************************************************************************
 * HTTPConnect: connect to the server and seek to i_tell
 *****************************************************************************/
static int HTTPConnect( input_thread_t * p_input, off_t i_tell )
{
    char psz_buffer[MAX_QUERY_SIZE];
    _input_socket_t * p_access_data;
    module_t * p_network;
    char * psz_parser, * psz_value, * psz_answer;
    byte_t * p_bytes;
    int i_code, i_ret, i, i_size;

    enum { HTTP_PROTOCOL, ICY_PROTOCOL } i_protocol;

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

    /* Build the query string */
    if ( p_input->stream.b_seekable )
    {
         snprintf( psz_buffer, MAX_QUERY_SIZE,
                   "%s"
                   "Range: bytes="I64Fd"-\r\n"
                   HTTP_USERAGENT
                   "%s"
                   HTTP_END,
                   p_access_data->psz_buffer, i_tell, p_access_data->psz_auth_string );
    }
    else
    {
         snprintf( psz_buffer, MAX_QUERY_SIZE,
                   "%s"
                   HTTP_USERAGENT
                   "%s"
                   HTTP_END,
                   p_access_data->psz_buffer, p_access_data->psz_auth_string );
    }
    psz_buffer[MAX_QUERY_SIZE - 1] = '\0';

    /* Send GET query */
    i_ret = send( p_access_data->_socket.i_handle,
                  psz_buffer, strlen( psz_buffer ), 0 );
    if( i_ret == -1 )
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

    /* Get the HTTP returncode */
    i_size = input_Peek( p_input, &p_bytes, MAX_ANSWER_SIZE );
    psz_parser = (char *)p_bytes;

    if( i_size <= 0 )
    {
        msg_Err( p_input, "not enough data" );
        Close( VLC_OBJECT(p_input) );
        return VLC_EGENERIC;
    }

    /* Guess the protocol */
    if( ( ( (size_t)i_size >= strlen("HTTP/1.x") ) &&
            !strncmp( psz_parser, "HTTP/1.", strlen("HTTP/1.") ) ) )
    {
        i_protocol = HTTP_PROTOCOL;

        psz_parser += strlen("HTTP/1.x");
        i_size -= strlen("HTTP/1.x");
    }
    else if( ( (size_t)i_size >= strlen("ICY") &&
             !strncmp( psz_parser, "ICY", strlen("ICY") ) ) )
    {
        i_protocol = ICY_PROTOCOL;
        if( !p_input->psz_demux || !*p_input->psz_demux  )
        {
            msg_Info( p_input, "ICY server found, mp3 demuxer selected" );
            p_input->psz_demux = "mp3";    // FIXME strdup ?
        }

        psz_parser += strlen("ICY");
        i_size -= strlen("ICY");
    }
    else
    {
        msg_Err( p_input, "invalid HTTP reply '%s'", psz_parser );
        return VLC_EGENERIC;
    }

    /* Check the HTTP return code */
    i_code = atoi( (char*)psz_parser );
    msg_Dbg( p_input, "%s server replied: %i",
             i_protocol == HTTP_PROTOCOL ? "HTTP" : "ICY", i_code );
    psz_parser += 4;
    i_size -= 4;

    /* Find the end of the line */
    for ( i = 0; (i < i_size -1) && ((psz_parser[i] != '\r') ||
      (psz_parser[i+1] != '\n')); i++ )
    {
        ;
    }

    /* Check we actually parsed something */
    if ( i+1 == i_size && psz_parser[i+1] != '\n' )
    {
        msg_Err( p_input, "stream not compliant with HTTP/1.x" );
        return VLC_EGENERIC;
    }

    /* Store the line we just parsed and skip it */
    psz_answer = strndup( psz_parser, i );
    if( !psz_answer )
    {
        return VLC_ENOMEM;
    }

    p_input->p_current_data = psz_parser + i + 2;

    /* Parse remaining headers */
    for ( ; ; )
    {
        char psz_line[MAX_ANSWER_SIZE];

        i_size = input_Peek( p_input, &p_bytes, MAX_ANSWER_SIZE );
        psz_parser = (char *)p_bytes;

        if( i_size <= 0 )
        {
            msg_Err( p_input, "not enough data" );
            Close( VLC_OBJECT(p_input) );
            free( psz_answer );
            return VLC_EGENERIC;
        }

        /* Copy one line to psz_line */
        i = 0;
        while( i_size && psz_parser[i] != '\r'
                      && psz_parser[i + 1] != '\n' )
        {
            psz_line[i] = psz_parser[i];
            i++;
            i_size--;
        }
        p_input->p_current_data = psz_parser + i + 2;
        if( !i )
        {
            break; /* End of headers */
        }
        psz_line[i] = '\0';
        psz_parser = strchr( psz_line, ':' );
        if ( !psz_parser )
        {
            msg_Err( p_input, "malformed header line: %s", psz_line );
            free( psz_answer );
            return VLC_EGENERIC;
        }
        psz_parser[0] = '\0';
        psz_parser++;
        while ( *psz_parser == ' ' || *psz_parser == '\t' )
        {
            psz_parser++;
        }
        psz_value = psz_parser;

        if( !strcasecmp( psz_line, "Content-Length" ) )
        {
            off_t i_size = 0;
#ifdef HAVE_ATOLL
            i_size = i_tell + atoll( psz_value );
#else
            int sign = 1;

            if( *psz_value == '-' ) sign = -1;
            while( *psz_value >= '0' && *psz_value <= '9' )
            {
                i_size = i_size * 10 + *psz_value++ - '0';
            }
            i_size = i_tell + ( i_size * sign );
#endif
            msg_Dbg( p_input, "stream size is "I64Fd, i_size );

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->stream.p_selected_area->i_size = i_size;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        /* Redirection support */
        else if( ( i_code == 301 || i_code == 302 ||
                   i_code == 303 || i_code == 307 )
                    && !strcasecmp( psz_line, "location" ) )
        {
            playlist_t * p_playlist = (playlist_t *) vlc_object_find(
                                  p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );
            if( !p_playlist )
            {
                msg_Err( p_input, "redirection failed: can't find playlist" );
                free( psz_answer );
                return VLC_EGENERIC;
            }
            msg_Dbg( p_input, "%i %s: redirected to %s",
                              i_code, psz_answer, psz_value );
            p_playlist->pp_items[p_playlist->i_index]->b_autodeletion
                                                                  = VLC_TRUE;
            playlist_Add( p_playlist, psz_value, NULL, 0,
                          PLAYLIST_INSERT | PLAYLIST_GO,
                          p_playlist->i_index + 1 );
            vlc_object_release( p_playlist );
        }

        /* TODO: parse other headers here */
    }

    /* Something went wrong */
    if ( i_code >= 400 )
    {
        msg_Err( p_input, "%i %s", i_code, psz_answer );
        p_input->p_current_data = psz_parser + i_size;
        free( psz_answer );
        return VLC_EGENERIC;
    }

    free( psz_answer );

    /* Set final stream properties */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( i_protocol == ICY_PROTOCOL )
    {
        p_input->stream.b_seekable = VLC_FALSE;
    }
    else
    {
        p_input->stream.b_seekable = VLC_TRUE;
    }

    if( p_input->stream.p_selected_area->i_size )
    {
        p_input->stream.p_selected_area->i_tell = i_tell;
    }
    else
    {
        p_input->stream.b_seekable = VLC_FALSE;
    }
    if( i_code != 206 )
    {
        p_input->stream.b_seekable = VLC_FALSE;
    }
    p_input->stream.b_changed = VLC_TRUE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Encode a string in base64
 * Code borrowed from Rafael Steil
 *****************************************************************************/
void encodeblock( unsigned char in[3], unsigned char out[4], int len )
{
    static const char cb64[]
        = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

char *str_base64_encode(char *psz_str, input_thread_t *p_input )
{
    unsigned char in[3], out[4];
    unsigned int i, len, blocksout = 0, linesize = strlen(psz_str);
    char *psz_tmp = psz_str;
    char *psz_result = (char *)malloc( linesize / 3 * 4 + 5 );

    if( !psz_result )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    while( *psz_tmp )
    {
        len = 0;

        for( i = 0; i < 3; i++ )
        {
            in[i] = (unsigned char)*psz_tmp;

            if (*psz_tmp)
                len++;
            else
                in[i] = 0;

            psz_tmp++;
        }

        if( len )
        {
            encodeblock( in, out, len );

            for( i = 0; i < 4; i++ )
            {
                psz_result[blocksout++] = out[i];
            }
        }
    }

    psz_result[blocksout] = '\0';
    return psz_result;
}

/*****************************************************************************
 * Open: parse URL and open the remote file at the beginning
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    _input_socket_t *   p_access_data;
    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name, * psz_auth_parser;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_path = "";
    char *              psz_proxy, *psz_proxy_orig;
    char *              psz_user = NULL, *psz_pwd = NULL;
    int                 i_server_port = 0;
    vlc_value_t         val;

    p_access_data = malloc( sizeof(_input_socket_t) );
    p_input->p_access_data = (access_sys_t *)p_access_data;
    if( p_access_data == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free(psz_name);
        return VLC_ENOMEM;
    }

    p_access_data->psz_name = psz_name;
    p_access_data->psz_network = "";
    memset(p_access_data->psz_auth_string, 0, MAX_QUERY_SIZE);

    var_Create( p_input, "ipv4", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ipv4", &val );
    if( val.i_int )
    {
        p_access_data->psz_network = "ipv4";
    }
    var_Create( p_input, "ipv6", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ipv6", &val );
    if( val.i_int )
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
     * //[user:password]@<hostname>[:<port>][/<path>] */


    while( *psz_parser == '/' )
    {
        psz_parser++;
    }
    psz_auth_parser = psz_parser;

    while ( *psz_auth_parser != '@' && *psz_auth_parser != '\0' )
    {
        psz_auth_parser++;
    }
    if ( *psz_auth_parser == '@' )
    {
        psz_user = psz_parser;
        while ( *psz_parser != ':' && psz_parser < psz_auth_parser )
        {
            psz_parser++;
        }
        if ( psz_parser != psz_auth_parser )
        {
            *psz_parser = '\0';
            psz_pwd = psz_parser + 1;
        }
        else
        {
            psz_pwd = "";
        }
        *psz_auth_parser = '\0';
        psz_parser = psz_auth_parser + 1;
    }

    psz_server_addr = psz_parser;

    while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
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
            return VLC_EGENERIC;
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
        return VLC_EGENERIC;
    }

    /* Handle autehtification */

   if ( !psz_user )
    {
        var_Create( p_input, "http-user", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_input, "http-user", &val );
        psz_user = val.psz_string;

        var_Create( p_input, "http-pwd", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_input, "http-pwd", &val );
        psz_pwd = val.psz_string;
    }

    if ( *psz_user )
    {
        char psz_user_pwd[MAX_QUERY_SIZE];
        msg_Dbg( p_input, "authenticating, user=%s, password=%s",
                                           psz_user, psz_pwd );
        snprintf( psz_user_pwd, MAX_QUERY_SIZE, "%s:%s", psz_user, psz_pwd );
        snprintf( p_access_data->psz_auth_string, MAX_QUERY_SIZE,
                  "Authorization: Basic %s\r\n",
                  str_base64_encode( psz_user_pwd, p_input ) );
    }

    /* Check proxy config variable */
    var_Create( p_input, "http-proxy", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_input, "http-proxy", &val );
    psz_proxy_orig = val.psz_string;
    if( psz_proxy_orig == NULL )
    {
        /* Check proxy environment variable */
        psz_proxy_orig = getenv( "http_proxy" );
        if( psz_proxy_orig != NULL )
        {
            psz_proxy_orig = strdup( psz_proxy_orig );
        }
    }

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

            msg_Dbg( p_input, "using HTTP proxy server=%s port=%d",
                     psz_proxy, i_proxy_port );
        }
        else
        {
            msg_Err( p_input, "HTTP proxy %s is invalid!", psz_proxy_orig );
            free( p_input->p_access_data );
            free( psz_name );
            if( psz_proxy_orig ) free( psz_proxy_orig );
            return VLC_EGENERIC;
        }

        if( psz_proxy_orig ) free( psz_proxy_orig );

        p_access_data->socket_desc.psz_server_addr = psz_proxy;
        p_access_data->socket_desc.i_server_port = i_proxy_port;
        p_access_data->socket_desc.i_type = NETWORK_TCP;
        p_access_data->socket_desc.i_ttl           = 0;

        snprintf( p_access_data->psz_buffer, MAX_QUERY_SIZE,
                  "GET http://%s:%d/%s HTTP/1.0\r\n",
                  psz_server_addr, i_server_port, psz_path );
    }
    else
    {
        /* No proxy, direct connection. */
        p_access_data->socket_desc.i_type = NETWORK_TCP;
        p_access_data->socket_desc.psz_server_addr = psz_server_addr;
        p_access_data->socket_desc.i_server_port = i_server_port;
        p_access_data->socket_desc.i_ttl           = 0;

        snprintf( p_access_data->psz_buffer, MAX_QUERY_SIZE,
                  "GET /%s HTTP/1.1\r\nHost: %s\r\n",
                  psz_path, psz_server_addr );
    }
    p_access_data->psz_buffer[MAX_QUERY_SIZE - 1] = '\0';

    msg_Dbg( p_input, "opening server=%s port=%d path=%s",
                      psz_server_addr, i_server_port, psz_path );

    p_input->pf_read = Read;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = Seek;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;
    p_input->stream.b_seekable = VLC_TRUE;
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

        p_input->stream.b_seekable = VLC_FALSE;
        psz_pos[7] = '0';
        if( HTTPConnect( p_input, 0 ) )
        {
            free( p_input->p_access_data );
            free( psz_name );
            return VLC_EGENERIC;
        }
    }

    /* Update default_pts to a suitable value for http access */

    var_Create( p_input, "http-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "http-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

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
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    ssize_t         i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_access_data->i_handle, &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    while( (i_ret = select( p_access_data->i_handle + 1, &fds,
                            NULL, NULL, &timeout )) == 0
#ifdef HAVE_ERRNO_H
           || (i_ret < 0 && errno == EINTR)
#endif
           )
    {
        FD_ZERO( &fds );
        FD_SET( p_access_data->i_handle, &fds );
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if( p_input->b_die || p_input->b_error )
        {
            return 0;
        }
    }

    if( i_ret < 0 )
    {
        msg_Err( p_input, "network select error" );
        return -1;
    }

    i_recv = recv( p_access_data->i_handle, p_buffer, i_len, 0 );

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
