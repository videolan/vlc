/*****************************************************************************
 * rtsp.c: Retreive a description from an RTSP url.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: rtsp.c,v 1.2 2003/08/03 12:04:28 fenrir Exp $
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <errno.h>

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   if HAVE_ARPA_INET_H
#      include <arpa/inet.h>
#   elif defined( SYS_BEOS )
#      include <net/netdb.h>
#   endif
#endif

#include "network.h"

/*
 * TODO:
 *  - login/password management
 *  - RTSP over UDP
 *
 */
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("RTSP SDP request") );
    set_capability( "access", 0 );
    add_category_hint( "stream", NULL, VLC_FALSE );
        add_integer( "rtsp-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                     "", "", VLC_TRUE );
    add_shortcut( "rtsp" );
    add_shortcut( "rtspu" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Locales prototypes
 *****************************************************************************/
static ssize_t Read   ( input_thread_t *, byte_t *, size_t );

typedef struct
{
    char           *psz_host;
    int            i_port;
    char           *psz_path;
    char           *psz_login;
    char           *psz_password;
} url_t;

static void url_Parse( url_t *, char * );
static void url_Clean( url_t * );

static int  NetRead ( input_thread_t *, int , uint8_t *, int );
static void NetClose( input_thread_t *p_input, int i_handle );

struct access_sys_t
{
    int i_handle;

    /* description */
    int     i_data;
    uint8_t *p_data;
    uint8_t *p_actu;
};

#define RTSP_DESCRIBE_REQUEST \
    "DESCRIBE rtsp://%s:%d/%s RTSP/1.0\r\n" \
    "CSeq: 1\r\n" \
    "User-Agent: " COPYRIGHT_MESSAGE "\r\n" \
    "Accept: application/sdp\r\n" \
    "\r\n"

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;

    url_t          url;
    char           *psz_network = "";
    vlc_bool_t     b_udp = VLC_FALSE;
    module_t       *p_network;
    int            i_handle = -1;

    network_socket_t    socket_desc;
    int                 i_request;
    uint8_t             *p_request, *p;

    int                 i_code;

    vlc_value_t    val;

    /* Parse the URL [user:password@]host[:port]/path */
    url_Parse( &url, p_input->psz_name );
    if( url.i_port <= 0 )
    {
        /* Default RTSP port */
        url.i_port = 554;
    }

    /* ipv4 vs ipv6 */
    var_Create( p_input, "ipv4", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ipv4", &val );
    if( val.i_int )
    {
        psz_network = "ipv4";
    }

    var_Create( p_input, "ipv6", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ipv6", &val );
    if( val.i_int )
    {
        psz_network = "ipv6";
    }

    /* UDP vs TCP */
    if( !strcmp( p_input->psz_access, "rtspu" ) )
    {
        b_udp = VLC_TRUE;
    }

    if( b_udp )
    {
        msg_Err( p_input, "RTSP over UDP not supported" );
        goto error;
    }

    /* Open the TCP connection */
    socket_desc.i_type          = NETWORK_TCP;
    socket_desc.psz_server_addr = url.psz_host;
    socket_desc.i_server_port   = url.i_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    socket_desc.i_ttl           = 0;

    p_input->p_private = (void*)&socket_desc;
    p_network = module_Need( p_input, "network", psz_network );
    if( p_network == NULL )
    {
        goto error;
    }
    module_Unneed( p_input, p_network );
    i_handle = socket_desc.i_handle;
    p_input->i_mtu    = socket_desc.i_mtu;

    msg_Dbg( p_input, "connected with %s:%d", url.psz_host, url.i_port );

    /* Build request */
    i_request = strlen( RTSP_DESCRIBE_REQUEST ) +
                strlen( url.psz_host ) + 5 + strlen( url.psz_path ) + 1;
    p_request = malloc( i_request );
    sprintf( p_request, RTSP_DESCRIBE_REQUEST,
             url.psz_host, url.i_port, url.psz_path );

    msg_Dbg( p_input, "Request=%s", p_request );
    /* Send the request */
    for( p = p_request; p < p_request + strlen( p_request ); )
    {
        int i_send;

        i_send = send( i_handle, p, strlen( p ), 0 );
        if( i_send <= 0 )
        {
            goto error;
        }

        p += i_send;
    }
    free( p_request );

    /* Read the complete answer*/
    i_request = 1024;
    p_request = p = malloc( i_request );
    p_request[0] = '\0';
    for( ;; )
    {
        int i_recv;

        i_recv = NetRead( p_input, i_handle, p, i_request - ( p - p_request ) - 1);
        if( i_recv <= 0 )
        {
            break;
        }

        p[i_recv] = '\0';

        p += i_recv;

        if( p >= p_request + i_request - 1 )
        {
            int i_size = p - p_request;

            p_request = realloc( p_request, i_request + 1024 );

            p = p_request + i_size;
        }
    }

    if( strlen( p_request ) <= 0 ||
        ( !strstr( p_request, "\n\n" ) && !strstr( p_request, "\r\n\r\n" )  ) )
    {
        msg_Dbg( p_input, "failed to retreive description" );
        goto error;
    }

    /* Parse the header */
    msg_Dbg( p_input, "answer header=%s", p_request );
    if( strncmp( p_request, "RTSP/1.", strlen( "RTSP/1." ) ) )
    {
        msg_Err( p_input, "invalid answer" );
        free( p_request );
        goto error;
    }
    i_code = atoi( &p_request[strlen( "RTSP/1.x" )] );

    if( i_code / 100 != 2 )
    {
        msg_Err( p_input, "return code is %d (!=2xx), not yet supported", i_code );
        free( p_request );
        goto error;
    }

    p_input->p_access_data = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->i_handle = i_handle;
    p_sys->p_data   = p_request;
    p_sys->i_data   = strlen( p_request );
    if( ( p = strstr( p_request, "\r\n\r\n" ) ) )
    {
        p_sys->p_actu = p + 4;
    }
    else if( ( p = strstr( p_request, "\n\n" ) ) )
    {
        p_sys->p_actu = p + 2;
    }
    else
    {
        msg_Err( p_input, "mhhh ? cannot happens" );
        goto error;
    }

    /* Set exported functions */
    p_input->pf_read = Read;
    p_input->pf_seek = NULL;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->p_private = NULL;

    /* Finished to set some variable */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for RTSP access */
    p_input->i_pts_delay = config_GetInt( p_input, "rtsp-caching" ) * 1000;

    NetClose( p_input, i_handle );
    url_Clean( &url );
    return VLC_SUCCESS;

error:
    if( i_handle > 0 )
    {
        NetClose( p_input, i_handle );
    }
    url_Clean( &url );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys   = p_input->p_access_data;

    free( p_sys->p_data );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    access_sys_t   *p_sys   = p_input->p_access_data;

    int            i_copy = __MIN( (int)i_len, &p_sys->p_data[p_sys->i_data] - p_sys->p_actu );

    if( i_copy <= 0 )
    {
        return 0;
    }

    memcpy( p_buffer, p_sys->p_actu, i_copy );

    p_sys->p_actu += i_copy;

    return i_copy;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void url_Parse( url_t *url, char *psz_url )
{
    char *psz_dup = strdup( psz_url );
    char *psz_tmp, *p;

    p = psz_dup;

    while( *p == '/' )
    {
        p++;
    }
    if( strchr( p, '@' ) )
    {
        /* TODO: There is a login/password */
    }
    url->psz_login = strdup( "" );
    url->psz_password = strdup( "" );

    psz_tmp = p;
    /* Skip host */
    while( *p && *p != ':' &&  *p != '/' )
    {
        if( *p == '[' )
        {
            /* ipv6 address */
            while( *p && *p != ']' )
            {
                *p++;
            }
        }
        p++;
    }

    /* Get port */
    if( *p == ':' )
    {
        char *psz_port;
        *p++ = 0;
        psz_port = p;

        while( *p && *p != '/' )
        {
            p++;
        }
        url->i_port = atoi( psz_port );
    }
    else
    {
        url->i_port = 0;
    }
    /* Get host */
    *p++ = '\0';
    url->psz_host = strdup( psz_tmp );

    /* Get path */
    url->psz_path = strdup( p );

    free( psz_dup );
}

static void url_Clean( url_t *url )
{
    if( url->psz_host )
    {
        free( url->psz_host );
    }
    if( url->psz_path )
    {
        free( url->psz_path );
    }
    if( url->psz_login )
    {
        free( url->psz_login );
    }
    if( url->psz_password )
    {
        free( url->psz_password );
    }
}



static int NetRead( input_thread_t *p_input,
                    int i_handle,
                    uint8_t *p_buffer, int i_len )
{
    struct timeval  timeout;
    fd_set          fds;
    int             i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( i_handle, &fds );

    /* We'll wait 1 second if nothing happens */
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    /* Find if some data is available */
    while( (i_ret = select( i_handle + 1, &fds,
                            NULL, NULL, &timeout )) == 0
           || (i_ret < 0 && errno == EINTR) )
    {
        FD_ZERO( &fds );
        FD_SET( i_handle, &fds );
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        if( p_input->b_die || p_input->b_error )
        {
            return 0;
        }
    }

    if( i_ret < 0 )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
        return -1;
    }

    i_recv = recv( i_handle, p_buffer, i_len, 0 );

    if( i_recv < 0 )
    {
        msg_Err( p_input, "recv failed (%s)", strerror(errno) );
    }

    return i_recv;
}



static void NetClose( input_thread_t *p_input, int i_handle )
{
#if defined( UNDER_CE )
    CloseHandle( (HANDLE)i_handle );
#elif defined( WIN32 )
    closesocket( i_handle );
#else
    close( i_handle );
#endif
}


