/*****************************************************************************
 * httpd.c
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: httpd.c,v 1.21 2003/07/02 18:44:27 zorglub Exp $
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

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include "httpd.h"

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
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#include "network.h"

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

#define LISTEN_BACKLOG          100
#define HTTPD_MAX_CONNECTION    512
#define HTTPD_CONNECTION_MAX_UNUSED 10000000

#define FREE( p ) if( p ) { free( p); (p) = NULL; }

#if defined( WIN32 ) || defined( UNDER_CE )
#define SOCKET_CLOSE(a)    closesocket(a)
#else
#define SOCKET_CLOSE(a)    close(a)
#endif

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int              Open   ( vlc_object_t * );
static void             Close  ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HTTP 1.0 daemon") );
    set_capability( "httpd", 42 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static httpd_host_t     *RegisterHost   ( httpd_t *, char *, int );
static void             UnregisterHost  ( httpd_t *, httpd_host_t * );

static httpd_file_t     *RegisterFile   ( httpd_t *,
                                          char *psz_file, char *psz_mime,
                                          char *psz_user, char *psz_password,
                                          httpd_file_callback pf_get,
                                          httpd_file_callback pf_post,
                                          httpd_file_callback_args_t *p_args );
static void             UnregisterFile  ( httpd_t *, httpd_file_t * );

//#define httpd_stream_t              httpd_file_t
static httpd_stream_t   *RegisterStream ( httpd_t *,
                                         char *psz_file, char *psz_mime,
                                         char *psz_user, char *psz_password );
static int              SendStream      ( httpd_t *, httpd_stream_t *, uint8_t *, int );
static int              HeaderStream    ( httpd_t *, httpd_stream_t *, uint8_t *, int );
static void             UnregisterStream( httpd_t *, httpd_stream_t* );

/*****************************************************************************
 * Internal definitions
 *****************************************************************************/
struct httpd_host_t
{
    int    i_ref;

    char   *psz_host_addr;
    int    i_port;

    struct sockaddr_in sock;
    int    fd;

};

#define HTTPD_AUTHENTICATE_NONE     0
#define HTTPD_AUTHENTICATE_BASIC    1

//typedef httpd_file_t httpd_stream_t;

struct httpd_file_t
{
    int         i_ref;

    char        *psz_file;
    char        *psz_mime;


    int         i_authenticate_method;
        char        *psz_user;          /* NULL if no auth */
        char        *psz_password;      /* NULL if no auth */

    vlc_bool_t  b_stream;               /* if false: httpd will retreive data by a callback
                                              true:  it's up to the program to give data to httpd */
    void                *p_sys;         /* provided for user */
    httpd_file_callback pf_get;         /* it should allocate and fill *pp_data and *pi_data */
    httpd_file_callback pf_post;        /* it should allocate and fill *pp_data and *pi_data */

    /* private */

    /* circular buffer for stream only */
    int         i_buffer_size;      /* buffer size, can't be reallocated smaller */
    uint8_t     *p_buffer;          /* buffer */
    int64_t     i_buffer_pos;       /* absolute position from begining */
    int         i_buffer_last_pos;  /* a new connection will start with that */

    /* data to be send at connection time (if any) */
    int         i_header_size;
    uint8_t     *p_header;
};


#define HTTPD_CONNECTION_RECEIVING_REQUEST      1
#define HTTPD_CONNECTION_SENDING_HEADER         2
#define HTTPD_CONNECTION_SENDING_FILE           3
#define HTTPD_CONNECTION_SENDING_STREAM         4
#define HTTPD_CONNECTION_TO_BE_CLOSED           5

#define HTTPD_CONNECTION_METHOD_GET             1
#define HTTPD_CONNECTION_METHOD_POST            2
typedef struct httpd_connection_s
{
    struct httpd_connection_s *p_next;
    struct httpd_connection_s *p_prev;

    struct  sockaddr_in sock;
    int     fd;
    mtime_t i_last_activity_date;

    int    i_state;
    int    i_method;       /* get/post */

    char    *psz_file;      // file to be send
    int     i_http_error;   // error to be send with the file
    char    *psz_user;      // if Authorization in the request header
    char    *psz_password;

    uint8_t *p_request;     // whith get: ?<*>, with post: main data
    int      i_request_size;

    httpd_file_t    *p_file;

    /* used while sending header and file */
    int     i_buffer_size;
    uint8_t *p_buffer;
    int     i_buffer;            /* private */

    /* used for stream */
    int64_t i_stream_pos;   /* absolute pos in stream */
} httpd_connection_t;

/* Linked List of banned IP */
typedef struct httpd_banned_ip_s
{
    struct httpd_banned_ip_s *p_next;
    struct httpd_banned_ip_s *p_prev;

    char *psz_ip;

} httpd_banned_ip_t;
/*
 * The httpd thread
 */
struct httpd_sys_t
{
    VLC_COMMON_MEMBERS

    vlc_mutex_t             host_lock;
    volatile int            i_host_count;
    httpd_host_t            **host;

    vlc_mutex_t             file_lock;
    int                     i_file_count;
    httpd_file_t            **file;

    vlc_mutex_t             connection_lock;
    int                     i_connection_count;
    httpd_connection_t      *p_first_connection;

    vlc_mutex_t             ban_lock;
    int                     i_banned_ip_count;
    httpd_banned_ip_t       *p_first_banned_ip;
};

static void httpd_Thread( httpd_sys_t *p_httpt );
static void httpd_ConnnectionNew( httpd_sys_t *, int , struct sockaddr_in * );
static void httpd_ConnnectionClose( httpd_sys_t *, httpd_connection_t * );
static int httpd_UnbanIP( httpd_sys_t *, httpd_banned_ip_t *);
static int httpd_BanIP( httpd_sys_t *, char *);
static httpd_banned_ip_t *httpd_GetbannedIP( httpd_sys_t *, char * );
/*****************************************************************************
 * Open:
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{
    httpd_t     *p_httpd = (httpd_t*)p_this;
    httpd_sys_t *p_httpt;

    /* Launch httpt thread */
    if( !( p_httpt = vlc_object_create( p_this, sizeof( httpd_sys_t ) ) ) )
    {
        msg_Err( p_this, "out of memory" );
        return( VLC_EGENERIC );
    }

    p_httpt->b_die  = 0;
    p_httpt->b_error= 0;

    /* init httpt_t structure */
    vlc_mutex_init( p_httpd, &p_httpt->host_lock );
    p_httpt->i_host_count = 0;
    p_httpt->host = NULL;

    vlc_mutex_init( p_httpd, &p_httpt->file_lock );
    p_httpt->i_file_count = 0;
    p_httpt->file = NULL;

    vlc_mutex_init( p_httpd, &p_httpt->connection_lock );
    p_httpt->i_connection_count = 0;
    p_httpt->p_first_connection = NULL;

    vlc_mutex_init( p_httpd, &p_httpt->ban_lock );
    p_httpt->i_banned_ip_count = 0;
    p_httpt->p_first_banned_ip = NULL;

    /* start the thread */
    if( vlc_thread_create( p_httpt, "httpd thread",
                           httpd_Thread, VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_this, "cannot spawn http thread" );

        vlc_mutex_destroy( &p_httpt->host_lock );
        vlc_mutex_destroy( &p_httpt->file_lock );
        vlc_mutex_destroy( &p_httpt->connection_lock );
        vlc_mutex_destroy( &p_httpt->ban_lock );

        vlc_object_destroy( p_httpt );
        return( VLC_EGENERIC );
    }

    msg_Info( p_httpd, "http thread launched" );

    p_httpd->p_sys = p_httpt;
    p_httpd->pf_register_host   = RegisterHost;
    p_httpd->pf_unregister_host = UnregisterHost;
    p_httpd->pf_register_file   = RegisterFile;
    p_httpd->pf_unregister_file = UnregisterFile;
    p_httpd->pf_register_stream = RegisterStream;
    p_httpd->pf_header_stream   = HeaderStream;
    p_httpd->pf_send_stream     = SendStream;
    p_httpd->pf_unregister_stream=UnregisterStream;

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    httpd_t     *p_httpd = (httpd_t*)p_this;
    httpd_sys_t *p_httpt = p_httpd->p_sys;

    httpd_connection_t *p_con;
    httpd_banned_ip_t *p_banned_ip;

    int i;

    p_httpt->b_die = 1;
    vlc_thread_join( p_httpt );

    /* first close all host */
    vlc_mutex_destroy( &p_httpt->host_lock );
    if( p_httpt->i_host_count )
    {
        msg_Err( p_httpd, "still have %d hosts registered !", p_httpt->i_host_count );
    }
    for( i = 0; i < p_httpt->i_host_count; i++ )
    {
#define p_host p_httpt->host[i]
        FREE( p_host->psz_host_addr );
        SOCKET_CLOSE( p_host->fd );

        FREE( p_host );
#undef p_host
    }
    FREE( p_httpt->host );

    /* now all file */
    vlc_mutex_destroy( &p_httpt->file_lock );
    if( p_httpt->i_file_count )
    {
        msg_Err( p_httpd, "still have %d files registered !", p_httpt->i_file_count );
    }
    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
#define p_file p_httpt->file[i]
        FREE( p_file->psz_file );
        FREE( p_file->psz_mime );
        if( p_file->i_authenticate_method != HTTPD_AUTHENTICATE_NONE )
        {
            FREE( p_file->psz_user );
            FREE( p_file->psz_password );
        }
        FREE( p_file->p_buffer );

        FREE( p_file );
#undef p_file
    }
    FREE( p_httpt->file );

    /* andd close all connection */
    vlc_mutex_destroy( &p_httpt->connection_lock );
    if( p_httpt->i_connection_count )
    {
        msg_Warn( p_httpd, "%d connections still in use", p_httpt->i_connection_count );
    }
    while( ( p_con = p_httpt->p_first_connection ) )
    {
        httpd_ConnnectionClose( p_httpt, p_con );
    }

    /* Free all banned IP */
    vlc_mutex_destroy( &p_httpt->ban_lock );
    while( ( p_banned_ip = p_httpt->p_first_banned_ip))
    {
        httpd_UnbanIP(p_httpt,p_banned_ip);
    }

    msg_Info( p_httpd, "httpd instance closed" );
    vlc_object_destroy( p_httpt );
}


/****************************************************************************
 ****************************************************************************
 ***
 ***
 ****************************************************************************
 ****************************************************************************/
static int BuildAddr( struct sockaddr_in * p_socket,
                      const char * psz_address, int i_port )
{
    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                /* family */
    p_socket->sin_port = htons( (uint16_t)i_port );
    if( !*psz_address )
    {
        p_socket->sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        struct hostent    * p_hostent;

        /* Try to convert address directly from in_addr - this will work if
         * psz_address is dotted decimal. */
#ifdef HAVE_ARPA_INET_H
        if( !inet_aton( psz_address, &p_socket->sin_addr ) )
#else
        p_socket->sin_addr.s_addr = inet_addr( psz_address );
        if( p_socket->sin_addr.s_addr == INADDR_NONE )
#endif
        {
            /* We have a fqdn, try to find its address */
            if ( (p_hostent = gethostbyname( psz_address )) == NULL )
            {
                return( -1 );
            }

            /* Copy the first address of the host in the socket address */
            memcpy( &p_socket->sin_addr, p_hostent->h_addr_list[0],
                     p_hostent->h_length );
        }
    }
    return( 0 );
}


/*
 * listen on a host for a httpd instance
 */

static httpd_host_t *_RegisterHost( httpd_sys_t *p_httpt, char *psz_host_addr, int i_port )
{
    httpd_host_t    *p_host;
    struct sockaddr_in  sock;
    int i;
    int fd = -1;
    int i_opt;
#if !defined( WIN32 ) && !defined( UNDER_CE )
    int i_flags;
#endif

    if( BuildAddr( &sock, psz_host_addr, i_port ) )
    {
        msg_Err( p_httpt, "cannot build address for %s:%d", psz_host_addr, i_port );
        return NULL;
    }

    /* is it already declared ? */
    vlc_mutex_lock( &p_httpt->host_lock );
    for( i = 0; i < p_httpt->i_host_count; i++ )
    {
        if( p_httpt->host[i]->sock.sin_port == sock.sin_port &&
            ( p_httpt->host[i]->sock.sin_addr.s_addr == INADDR_ANY ||
            p_httpt->host[i]->sock.sin_addr.s_addr == sock.sin_addr.s_addr ) )
        {
            break;
        }
    }

    if( i < p_httpt->i_host_count )
    {
        /* yes, increment ref count and succed */
        p_httpt->host[i]->i_ref++;
        vlc_mutex_unlock( &p_httpt->host_lock );
        return( p_httpt->host[i] );
    }

    /* need to add a new listening socket */

    /* open socket */
    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 )
    {
        msg_Err( p_httpt, "cannot open socket" );
        goto socket_failed;
    }
    /* reuse socket */
    i_opt = 1;
    if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) < 0 )
    {
        msg_Warn( p_httpt, "cannot configure socket (SO_REUSEADDR)" );
    }
    /* bind it */
    if( bind( fd, (struct sockaddr *)&sock, sizeof( struct sockaddr_in ) ) < 0 )
    {
        msg_Err( p_httpt, "cannot bind socket" );
        goto socket_failed;
    }
    /* set to non-blocking */
#if defined( WIN32 ) || defined( UNDER_CE )
    {
        unsigned long i_dummy = 1;
        if( ioctlsocket( fd, FIONBIO, &i_dummy ) != 0 )
        {
            msg_Err( p_httpt, "cannot set socket to non-blocking mode" );
            goto socket_failed;
        }
    }
#else
    if( ( i_flags = fcntl( fd, F_GETFL, 0 ) ) < 0 )
    {
        msg_Err( p_httpt, "cannot F_GETFL socket" );
        goto socket_failed;
    }
    if( fcntl( fd, F_SETFL, i_flags | O_NONBLOCK ) < 0 )
    {
        msg_Err( p_httpt, "cannot F_SETFL O_NONBLOCK" );
        goto socket_failed;
    }
#endif
    /* listen */
    if( listen( fd, LISTEN_BACKLOG ) < 0 )
    {
        msg_Err( p_httpt, "cannot listen socket" );
        goto socket_failed;
    }

    if( p_httpt->host )
    {
        p_httpt->host = realloc( p_httpt->host, sizeof( httpd_host_t *)  * ( p_httpt->i_host_count + 1 ) );
    }
    else
    {
        p_httpt->host = malloc( sizeof( httpd_host_t *) );
    }
    p_host                = malloc( sizeof( httpd_host_t ) );
    p_host->i_ref         = 1;
    p_host->psz_host_addr = strdup( psz_host_addr );
    p_host->i_port        = i_port;
    p_host->sock          = sock;
    p_host->fd            = fd;

    p_httpt->host[p_httpt->i_host_count++] = p_host;
    vlc_mutex_unlock( &p_httpt->host_lock );

    return p_host;

socket_failed:
    vlc_mutex_unlock( &p_httpt->host_lock );
    if( fd >= 0 )
    {
        SOCKET_CLOSE( fd );
    }
    return NULL;
}
static httpd_host_t     *RegisterHost( httpd_t *p_httpd, char *psz_host_addr, int i_port )
{
    return( _RegisterHost( p_httpd->p_sys, psz_host_addr, i_port ) );
}

/*
 * remove a listening host for an httpd instance
 */
static void            _UnregisterHost( httpd_sys_t *p_httpt, httpd_host_t *p_host )
{
    int i;

    vlc_mutex_lock( &p_httpt->host_lock );
    for( i = 0; i < p_httpt->i_host_count; i++ )
    {
        if( p_httpt->host[i] == p_host )
        {
            break;
        }
    }
    if( i >= p_httpt->i_host_count )
    {
        vlc_mutex_unlock( &p_httpt->host_lock );
        msg_Err( p_httpt, "cannot unregister host" );
        return;
    }

    p_host->i_ref--;

    if( p_host->i_ref > 0 )
    {
        /* still in use */
        vlc_mutex_unlock( &p_httpt->host_lock );
        return;
    }

    /* no more used */
    FREE( p_host->psz_host_addr );
    SOCKET_CLOSE( p_host->fd );

    FREE( p_host );

    if( p_httpt->i_host_count <= 1 )
    {
        FREE( p_httpt->host );
        p_httpt->i_host_count = 0;
    }
    else
    {
        int i_move;

        i_move = p_httpt->i_host_count - i - 1;

        if( i_move > 0 )
        {
            memmove( &p_httpt->host[i],
                     &p_httpt->host[i+1],
                     i_move * sizeof( httpd_host_t * ) );
        }

        p_httpt->i_host_count--;
        p_httpt->host = realloc( p_httpt->host,
                                 p_httpt->i_host_count * sizeof( httpd_host_t * ) );
    }

    vlc_mutex_unlock( &p_httpt->host_lock );
}
static void             UnregisterHost( httpd_t *p_httpd, httpd_host_t *p_host )
{
    _UnregisterHost( p_httpd->p_sys, p_host );
}


static void __RegisterFile( httpd_sys_t *p_httpt, httpd_file_t *p_file )
{
    /* add a new file */
    if( p_httpt->i_file_count )
    {
        p_httpt->file = realloc( p_httpt->file, sizeof( httpd_file_t *)  * ( p_httpt->i_file_count + 1 ) );
    }
    else
    {
        p_httpt->file = malloc( sizeof( httpd_file_t *) );
    }

    p_httpt->file[p_httpt->i_file_count++] = p_file;
}

static httpd_file_t    *_RegisterFile( httpd_sys_t *p_httpt,
                                       char *psz_file, char *psz_mime,
                                       char *psz_user, char *psz_password,
                                       httpd_file_callback pf_get,
                                       httpd_file_callback pf_post,
                                       httpd_file_callback_args_t *p_args )
{
    httpd_file_t    *p_file;
    int i;

    vlc_mutex_lock( &p_httpt->file_lock );
    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( !strcmp( psz_file, p_httpt->file[i]->psz_file ) )
        {
            break;
        }
    }
    if( i < p_httpt->i_file_count )
    {
        vlc_mutex_unlock( &p_httpt->file_lock );
        msg_Err( p_httpt, "%s already registered", psz_file );
        return NULL;
    }

    p_file              = malloc( sizeof( httpd_file_t ) );
    p_file->i_ref       = 0;
    p_file->psz_file    = strdup( psz_file );
    p_file->psz_mime    = strdup( psz_mime );
    if( psz_user && *psz_user )
    {
        p_file->i_authenticate_method = HTTPD_AUTHENTICATE_BASIC;
        p_file->psz_user              = strdup( psz_user );
        p_file->psz_password          = strdup( psz_password );
    }
    else
    {
        p_file->i_authenticate_method = HTTPD_AUTHENTICATE_NONE;
        p_file->psz_user              = NULL;
        p_file->psz_password          = NULL;
    }

    p_file->b_stream          = VLC_FALSE;
    p_file->p_sys             = p_args;
    p_file->pf_get            = pf_get;
    p_file->pf_post           = pf_post;

    p_file->i_buffer_size     = 0;
    p_file->i_buffer_last_pos = 0;
    p_file->i_buffer_pos      = 0;
    p_file->p_buffer          = NULL;

    p_file->i_header_size     = 0;
    p_file->p_header          = NULL;

    __RegisterFile( p_httpt, p_file );

    vlc_mutex_unlock( &p_httpt->file_lock );

    return p_file;
}
static httpd_file_t     *RegisterFile( httpd_t *p_httpd,
                                       char *psz_file, char *psz_mime,
                                       char *psz_user, char *psz_password,
                                       httpd_file_callback pf_get,
                                       httpd_file_callback pf_post,
                                       httpd_file_callback_args_t *p_args )
{
    return( _RegisterFile( p_httpd->p_sys,
                           psz_file, psz_mime, psz_user, psz_password,
                           pf_get, pf_post, p_args ) );
}

static httpd_stream_t  *_RegisterStream( httpd_sys_t *p_httpt,
                                         char *psz_file, char *psz_mime,
                                         char *psz_user, char *psz_password )
{
    httpd_stream_t    *p_stream;
    int i;

    vlc_mutex_lock( &p_httpt->file_lock );
    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( !strcmp( psz_file, p_httpt->file[i]->psz_file ) )
        {
            break;
        }
    }
    if( i < p_httpt->i_file_count )
    {
        vlc_mutex_unlock( &p_httpt->file_lock );
        msg_Err( p_httpt, "%s already registered", psz_file );
        return NULL;
    }

    p_stream              = malloc( sizeof( httpd_stream_t ) );
    p_stream->i_ref       = 0;
    p_stream->psz_file    = strdup( psz_file );
    p_stream->psz_mime    = strdup( psz_mime );
    if( psz_user && *psz_user )
    {
        p_stream->i_authenticate_method = HTTPD_AUTHENTICATE_BASIC;
        p_stream->psz_user              = strdup( psz_user );
        p_stream->psz_password          = strdup( psz_password );
    }
    else
    {
        p_stream->i_authenticate_method = HTTPD_AUTHENTICATE_NONE;
        p_stream->psz_user              = NULL;
        p_stream->psz_password          = NULL;
    }

    p_stream->b_stream        = VLC_TRUE;
    p_stream->p_sys           = NULL;
    p_stream->pf_get          = NULL;
    p_stream->pf_post         = NULL;

    p_stream->i_buffer_size   = 5*1024*1024;
    p_stream->i_buffer_pos      = 0;
    p_stream->i_buffer_last_pos = 0;
    p_stream->p_buffer        = malloc( p_stream->i_buffer_size );

    p_stream->i_header_size   = 0;
    p_stream->p_header        = NULL;

    __RegisterFile( p_httpt, p_stream );

    vlc_mutex_unlock( &p_httpt->file_lock );

    return p_stream;
}
static httpd_stream_t   *RegisterStream( httpd_t *p_httpd,
                                         char *psz_file, char *psz_mime,
                                         char *psz_user, char *psz_password )
{
    return( _RegisterStream( p_httpd->p_sys,
                             psz_file, psz_mime, psz_user, psz_password ) );
}

static void            _UnregisterFile( httpd_sys_t *p_httpt, httpd_file_t *p_file )
{
    int i;

    vlc_mutex_lock( &p_httpt->file_lock );
    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( !strcmp( p_file->psz_file, p_httpt->file[i]->psz_file ) )
        {
            break;
        }
    }
    if( i >= p_httpt->i_file_count )
    {
        vlc_mutex_unlock( &p_httpt->file_lock );
        msg_Err( p_httpt, "cannot unregister file" );
        return;
    }

    if( p_file->i_ref > 0 )
    {
        httpd_connection_t *p_con;
        /* force closing all connection for this file */
        msg_Err( p_httpt, "closing all client connection" );

        vlc_mutex_lock( &p_httpt->connection_lock );
        for( p_con = p_httpt->p_first_connection; p_con != NULL; )
        {
            httpd_connection_t *p_next;

            p_next = p_con->p_next;
            if( p_con->p_file == p_file )
            {
                httpd_ConnnectionClose( p_httpt, p_con );
            }
            p_con = p_next;
        }
        vlc_mutex_unlock( &p_httpt->connection_lock );
    }

    FREE( p_file->psz_file );
    FREE( p_file->psz_mime );
    if( p_file->i_authenticate_method != HTTPD_AUTHENTICATE_NONE )
    {
        FREE( p_file->psz_user );
        FREE( p_file->psz_password );
    }
    FREE( p_file->p_buffer );
    FREE( p_file->p_header );

    FREE( p_file );


    if( p_httpt->i_file_count == 1 )
    {
        FREE( p_httpt->file );
        p_httpt->i_file_count = 0;
    }
    else
    {
        int i_move;

        i_move = p_httpt->i_file_count - i - 1;
        if( i_move > 0  )
        {
            memmove( &p_httpt->file[i], &p_httpt->file[i + 1], sizeof( httpd_file_t *) * i_move );
        }
        p_httpt->i_file_count--;
        p_httpt->file = realloc( p_httpt->file, sizeof( httpd_file_t *) * p_httpt->i_file_count );
    }

    vlc_mutex_unlock( &p_httpt->file_lock );
}
static void            UnregisterFile( httpd_t *p_httpd, httpd_file_t *p_file )
{
    _UnregisterFile( p_httpd->p_sys, p_file );
}

static void             UnregisterStream( httpd_t *p_httpd, httpd_stream_t *p_stream )
{
    _UnregisterFile( p_httpd->p_sys, p_stream );
}



static int             _SendStream( httpd_sys_t *p_httpt, httpd_stream_t *p_stream, uint8_t *p_data, int i_data )
{
    int i_count;
    int i_pos;

    if( i_data <= 0 || p_data == NULL )
    {
        return( VLC_SUCCESS );
    }
    //fprintf( stderr, "## i_data=%d pos=%lld\n", i_data, p_stream->i_buffer_pos );

    vlc_mutex_lock( &p_httpt->file_lock );

    /* save this pointer (to be used by new connection) */
    p_stream->i_buffer_last_pos = p_stream->i_buffer_pos;

    i_pos = p_stream->i_buffer_pos % p_stream->i_buffer_size;
    i_count = i_data;
    while( i_count > 0)
    {
        int i_copy;

        i_copy = __MIN( i_count, p_stream->i_buffer_size - i_pos );

        memcpy( &p_stream->p_buffer[i_pos],
                p_data,
                i_copy );

        i_pos = ( i_pos + i_copy ) % p_stream->i_buffer_size;
        i_count -= i_copy;
        p_data += i_copy;
    }

    p_stream->i_buffer_pos += i_data;
    vlc_mutex_unlock( &p_httpt->file_lock );

    return( VLC_SUCCESS );
}
static int             SendStream( httpd_t *p_httpd, httpd_stream_t *p_stream, uint8_t *p_data, int i_data )
{
    return( _SendStream( p_httpd->p_sys, p_stream, p_data, i_data ) );
}

static int             HeaderStream( httpd_t *p_httpd, httpd_stream_t *p_stream, uint8_t *p_data, int i_data )
{
    httpd_sys_t *p_httpt = p_httpd->p_sys;

    vlc_mutex_lock( &p_httpt->file_lock );

    FREE( p_stream->p_header );
    if( p_data == NULL || i_data <= 0 )
    {
        p_stream->i_header_size = 0;
    }
    else
    {
        p_stream->i_header_size = i_data;
        p_stream->p_header = malloc( i_data );
        memcpy( p_stream->p_header,
                p_data,
                i_data );
    }
    vlc_mutex_unlock( &p_httpt->file_lock );

    return( VLC_SUCCESS );
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int  httpd_page_401_get( httpd_file_callback_args_t *p_args,
                                uint8_t *p_request, int i_request,
                                uint8_t **pp_data, int *pi_data )
{
    char *p;

    p = *pp_data = malloc( 1024 );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>Error 401</title>\n" );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center> 401 authentification needed</center></h1>\n" );
    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    return VLC_SUCCESS;
}
static int  httpd_page_404_get( httpd_file_callback_args_t *p_args,
                                uint8_t *p_request, int i_request,
                                uint8_t **pp_data, int *pi_data )
{
    char *p;

    p = *pp_data = malloc( 1024 );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>Error 404</title>\n" );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center> 404 Ressource not found</center></h1>\n" );
    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    return VLC_SUCCESS;
}


static int _httpd_page_admin_get_status( httpd_file_callback_args_t *p_args,
                                         uint8_t **pp_data, int *pi_data )
{
    httpd_sys_t *p_httpt = (httpd_sys_t*)p_args;
    httpd_connection_t *p_con;
    httpd_banned_ip_t *p_ip;

    int i;
    char *p;

    /* FIXME FIXME do not use static size FIXME FIXME*/
    p = *pp_data = malloc( 8096 );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>VideoLAN Client Stream Output</title>\n" );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center>VideoLAN Client Stream Output</center></h1>\n" );
    p += sprintf( p, "<h2><center>Admin page</center></h2>\n" );

    /* general */
    p += sprintf( p, "<h3>General state</h3>\n" );
    p += sprintf( p, "<ul>\n" );
    p += sprintf( p, "<li>Connection count: %d</li>\n", p_httpt->i_connection_count );
    //p += sprintf( p, "<li>Total bandwith: %d</li>\n", -1 );
    /*p += sprintf( p, "<li></li>\n" );*/
    p += sprintf( p, "<li>Ban count: %d</li>\n", p_httpt->i_banned_ip_count );
    p += sprintf( p, "</ul>\n" );

    /* ban list */
    /* XXX do not lock on ban_lock */
    p += sprintf( p, "<h3>Ban list</h3>\n" );
    p += sprintf( p, "<table border=\"1\" cellspacing=\"0\" >\n" );
    p += sprintf( p, "<tr>\n<th>IP</th>\n<th>Action</th></tr>\n" );
    for( p_ip = p_httpt->p_first_banned_ip;p_ip != NULL; p_ip = p_ip->p_next )
    {
        p += sprintf( p, "<tr>\n" );
        p += sprintf( p, "<td>%s</td>\n", p_ip->psz_ip );
        p += sprintf( p, "<td><form method=\"get\" action=\"\">"
                         "<select name=\"action\">"
                         "<option selected>unban_ip</option>"
                         "</select>"
                         "<input type=\"hidden\" name=\"id\" value=\"%s\"/>"
                         "<input type=\"submit\" value=\"Do it\" />"
                         "</form></td>\n", p_ip->psz_ip);
        p += sprintf( p, "</tr>\n" );
    }
    p += sprintf( p, "</table>\n" );



    /* host list */
    vlc_mutex_lock( &p_httpt->host_lock );
    p += sprintf( p, "<h3>Host list</h3>\n" );
    p += sprintf( p, "<table border=\"1\" cellspacing=\"0\" >\n" );
    p += sprintf( p, "<tr>\n<th>Host</th><th>Port</th><th>IP</th>\n</tr>\n" );

    for( i = 0; i < p_httpt->i_host_count; i++ )
    {
        p += sprintf( p, "<tr>\n" );
        p += sprintf( p, "<td>%s</td>\n", p_httpt->host[i]->psz_host_addr );
        p += sprintf( p, "<td>%d</td>\n", p_httpt->host[i]->i_port );
        p += sprintf( p, "<td>%s</td>\n", inet_ntoa( p_httpt->host[i]->sock.sin_addr ) );
        p += sprintf( p, "</tr>\n" );
    }
    p += sprintf( p, "</table>\n" );
    vlc_mutex_unlock( &p_httpt->host_lock );

    /* file list */
    /* XXX do not take lock on file_lock */
    p += sprintf( p, "<h3>File list</h3>\n" );
    p += sprintf( p, "<table border=\"1\" cellspacing=\"0\" >\n" );
    p += sprintf( p, "<tr>\n<th>Name</th><th>Mime</th><th>Protected</th><th>Used</th>\n</tr>\n" );

    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( !p_httpt->file[i]->b_stream )
        {
            p += sprintf( p, "<tr>\n" );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_file );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_mime );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_user ? "Yes" : "No" );
            p += sprintf( p, "<td>%d</td>\n", p_httpt->file[i]->i_ref);
            p += sprintf( p, "</tr>\n" );
        }
    }
    p += sprintf( p, "</table>\n" );

    /* stream list */
    /* XXX do not take lock on file_lock */
    p += sprintf( p, "<h3>Stream list</h3>\n" );
    p += sprintf( p, "<table border=\"1\" cellspacing=\"0\" >\n" );
    p += sprintf( p, "<tr>\n<th>Name</th><th>Mime</th><th>Protected</th><th>Used</th>\n</tr>\n" );

    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( p_httpt->file[i]->b_stream )
        {
            p += sprintf( p, "<tr>\n" );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_file );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_mime );
            p += sprintf( p, "<td>%s</td>\n", p_httpt->file[i]->psz_user ? "Yes" : "No" );
            p += sprintf( p, "<td>%d</td>\n", p_httpt->file[i]->i_ref);
            p += sprintf( p, "</tr>\n" );
        }
    }
    p += sprintf( p, "</table>\n" );

    /* connection list */
    /* XXX do not take lock on connection_lock */
    p += sprintf( p, "<h3>Connection list</h3>\n" );
    p += sprintf( p, "<table border=\"1\" cellspacing=\"0\" >\n" );
    p += sprintf( p, "<tr>\n<th>IP</th><th>Requested File</th><th>Status</th><th>Action</th>\n</tr>\n" );

    for( p_con = p_httpt->p_first_connection;p_con != NULL; p_con = p_con->p_next )
    {
        p += sprintf( p, "<tr>\n" );
        p += sprintf( p, "<td>%s</td>\n", inet_ntoa( p_con->sock.sin_addr ) );
        p += sprintf( p, "<td>%s</td>\n", p_con->psz_file );
        p += sprintf( p, "<td>%d</td>\n", p_con->i_http_error );
        p += sprintf( p, "<td><form method=\"get\" action=\"\">"
                         "<select name=\"action\">"
                         "<option selected>close_connection</option>"
                         "<option>ban_ip</option>"
                         "<option>close_connection_and_ban_ip</option>"
                         "</select>"
                         "<input type=\"hidden\" name=\"id\" value=\"%p\"/>"
                         "<input type=\"submit\" value=\"Do it\" />"
                              "</form></td>\n", p_con);
        p += sprintf( p, "</tr>\n" );
    }
    p += sprintf( p, "</table>\n" );


    /* www.videolan.org */
    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    return( VLC_SUCCESS );
}

static int _httpd_page_admin_get_success( httpd_file_callback_args_t *p_args,
                                          uint8_t **pp_data, int *pi_data,
                                          char *psz_msg )
{
    char *p;

    p = *pp_data = malloc( 8096 );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>VideoLAN Client Stream Output</title>\n" );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center>VideoLAN Client Stream Output</center></h1>\n" );

    p += sprintf( p, "<p>Success=`%s'</p>", psz_msg );
    p += sprintf( p, "<a href=\"admin.html\">Back to admin page</a>\n" );

    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    return( VLC_SUCCESS );
}

static int _httpd_page_admin_get_error( httpd_file_callback_args_t *p_args,
                                        uint8_t **pp_data, int *pi_data,
                                        char *psz_error )
{
    char *p;

    p = *pp_data = malloc( 8096 );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>VideoLAN Client Stream Output</title>\n" );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center>VideoLAN Client Stream Output</center></h1>\n" );

    p += sprintf( p, "<p>Error=`%s'</p>", psz_error );
    p += sprintf( p, "<a href=\"admin.html\">Back to admin page</a>\n" );

    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    return( VLC_SUCCESS );
}

static void _httpd_uri_extract_value( char *psz_uri, char *psz_name, char *psz_value, int i_value_max )
{
    char *p;

    p = strstr( psz_uri, psz_name );
    if( p )
    {
        int i_len;

        p += strlen( psz_name );
        if( *p == '=' ) p++;

        if( strchr( p, '&' ) )
        {
            i_len = strchr( p, '&' ) - p;
        }
        else
        {
            i_len = strlen( p );
        }
        i_len = __MIN( i_value_max - 1, i_len );
        if( i_len > 0 )
        {
            strncpy( psz_value, p, i_len );
            psz_value[i_len] = '\0';
        }
        else
        {
            strncpy( psz_value, "", i_value_max );
        }
    }
    else
    {
        strncpy( psz_value, "", i_value_max );
    }
}


static int  httpd_page_admin_get( httpd_file_callback_args_t *p_args,
                                  uint8_t *p_request, int i_request,
                                  uint8_t **pp_data, int *pi_data )
{
    httpd_sys_t *p_httpt = (httpd_sys_t*)p_args;
    httpd_connection_t *p_con;

    if( i_request > 0)
    {
        char action[512];

        _httpd_uri_extract_value( p_request, "action", action, 512 );

        if( !strcmp( action, "close_connection" ) )
        {
            char id[128];
            void *i_id;

            _httpd_uri_extract_value( p_request, "id", id, 512 );
            i_id = (void*)strtol( id, NULL, 0 );
            msg_Dbg( p_httpt, "requested closing connection id=%s %p", id, i_id );
            for( p_con = p_httpt->p_first_connection;p_con != NULL; p_con = p_con->p_next )
            {
                if( (void*)p_con == i_id )
                {
                    /* XXX don't free p_con as it could be the one that it is sending ... */
                    p_con->i_state = HTTPD_CONNECTION_TO_BE_CLOSED;
                    return( _httpd_page_admin_get_success( p_args, pp_data, pi_data, "connection closed" ) );
                }
            }
            return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, "invalid id" ) );
        }
        else if( !strcmp( action, "ban_ip" ) )
        {
            char id[128];
            void *i_id;

            _httpd_uri_extract_value( p_request, "id", id, 512 );
            i_id = (void*)strtol( id, NULL, 0 );

            msg_Dbg( p_httpt, "requested banning ip id=%s %p", id, i_id );

            for( p_con = p_httpt->p_first_connection;p_con != NULL; p_con = p_con->p_next )
            {
                if( (void*)p_con == i_id )
                {
                    if( httpd_BanIP( p_httpt,inet_ntoa( p_con->sock.sin_addr ) ) == 0)
                        return( _httpd_page_admin_get_success( p_args, pp_data, pi_data, "IP banned" ) );
                    else
                        break;
                }
            }

            return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, action ) );
        }
        else if( !strcmp( action, "unban_ip" ) )
        {
            char id[128];

            _httpd_uri_extract_value( p_request, "id", id, 512 );
            msg_Dbg( p_httpt, "requested unbanning ip %s", id);

            if( httpd_UnbanIP( p_httpt, httpd_GetbannedIP ( p_httpt, id ) ) == 0)
                return( _httpd_page_admin_get_success( p_args, pp_data, pi_data, "IP Unbanned" ) );
            else
                return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, action ) );
        }
        else if( !strcmp( action, "close_connection_and_ban_ip" ) )
        {
            char id[128];
            void *i_id;

            _httpd_uri_extract_value( p_request, "id", id, 512 );
            i_id = (void*)strtol( id, NULL, 0 );
            msg_Dbg( p_httpt, "requested closing connection and banning ip id=%s %p", id, i_id );
            for( p_con = p_httpt->p_first_connection;p_con != NULL; p_con = p_con->p_next )
            {
                if( (void*)p_con == i_id )
                {
                    /* XXX don't free p_con as it could be the one that it is sending ... */
                    p_con->i_state = HTTPD_CONNECTION_TO_BE_CLOSED;

                    if( httpd_BanIP( p_httpt,inet_ntoa( p_con->sock.sin_addr ) ) == 0)
                        return( _httpd_page_admin_get_success( p_args, pp_data, pi_data, "Connection closed and IP banned" ) );
                    else
                        break;
                }

            }
            return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, "invalid id" ) );


            return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, action ) );
        }
        else
        {
            return( _httpd_page_admin_get_error( p_args, pp_data, pi_data, action ) );
        }
    }
    else
    {
        return( _httpd_page_admin_get_status( p_args, pp_data, pi_data ) );
    }

    return VLC_SUCCESS;
}

static int httpd_BanIP( httpd_sys_t *p_httpt, char * psz_new_banned_ip)
{
    httpd_banned_ip_t *p_new_banned_ip ;

    p_new_banned_ip = malloc( sizeof( httpd_banned_ip_t ) );
    if( !p_new_banned_ip )
    {
        return -1;
    }
    p_new_banned_ip->p_next=NULL;
    p_new_banned_ip->psz_ip = malloc( strlen( psz_new_banned_ip ) + 1 );
    if( !p_new_banned_ip->psz_ip )
    {
        return -2;
    }

    strcpy( p_new_banned_ip->psz_ip, psz_new_banned_ip );

    msg_Dbg( p_httpt, "Banning IP %s", psz_new_banned_ip );

    if( p_httpt->p_first_banned_ip )
    {
        httpd_banned_ip_t *p_last;

        p_last = p_httpt->p_first_banned_ip;
        while( p_last->p_next )
        {
            p_last = p_last->p_next;
        }

        p_last->p_next = p_new_banned_ip;
        p_new_banned_ip->p_prev = p_last;
    }
    else
    {
        p_new_banned_ip->p_prev = NULL;

        p_httpt->p_first_banned_ip = p_new_banned_ip;
    }

    p_httpt->i_banned_ip_count++;
    return 0;
}

static httpd_banned_ip_t *httpd_GetbannedIP( httpd_sys_t *p_httpt, char *psz_ip )
{
    httpd_banned_ip_t *p_ip;

    p_ip = p_httpt->p_first_banned_ip;

    while( p_ip)
    {
        if( strcmp( psz_ip, p_ip->psz_ip ) == 0 )
        {
            return p_ip;
        }
        p_ip = p_ip->p_next;
    }

    return NULL;
}

static int httpd_UnbanIP( httpd_sys_t *p_httpt, httpd_banned_ip_t *p_banned_ip )
{
    if(!p_banned_ip)
    {
        return -1;
    }

    msg_Dbg( p_httpt, "Unbanning IP %s",p_banned_ip->psz_ip);

    /* first cut out from list */
    if( p_banned_ip->p_prev )
    {
        p_banned_ip->p_prev->p_next = p_banned_ip->p_next;
    }
    else
    {
        p_httpt->p_first_banned_ip = p_banned_ip->p_next;
    }

    if( p_banned_ip->p_next )
    {
        p_banned_ip->p_next->p_prev = p_banned_ip->p_prev;
    }

    FREE( p_banned_ip->psz_ip );
    FREE( p_banned_ip );

    p_httpt->i_banned_ip_count--;

    return 0;
}

static void httpd_ConnnectionNew( httpd_sys_t *p_httpt, int fd, struct sockaddr_in *p_sock )
{
    httpd_connection_t *p_con;

    msg_Dbg( p_httpt, "new connection from %s", inet_ntoa( p_sock->sin_addr ) );

    /* verify if it's a banned ip */
    if(httpd_GetbannedIP( p_httpt,inet_ntoa( p_sock->sin_addr ) ) )
    {
        msg_Dbg( p_httpt, "Ip %s banned : closing connection", inet_ntoa( p_sock->sin_addr ) );
        close(fd);
        return;
    }

    /* create a new connection and link it */
    p_con = malloc( sizeof( httpd_connection_t ) );
    p_con->i_state  = HTTPD_CONNECTION_RECEIVING_REQUEST;
    p_con->fd       = fd;
    p_con->i_last_activity_date = mdate();

    p_con->sock     = *p_sock;
    p_con->psz_file = NULL;
    p_con->i_http_error = 0;
    p_con->psz_user = NULL;
    p_con->psz_password = NULL;
    p_con->p_file   = NULL;

    p_con->i_request_size = 0;
    p_con->p_request = NULL;

    p_con->i_buffer = 0;
    p_con->i_buffer_size = 8096;
    p_con->p_buffer = malloc( p_con->i_buffer_size );

    p_con->i_stream_pos = 0; // updated by httpd_thread */
    p_con->p_next = NULL;

    if( p_httpt->p_first_connection )
    {
        httpd_connection_t *p_last;

        p_last = p_httpt->p_first_connection;
        while( p_last->p_next )
        {
            p_last = p_last->p_next;
        }

        p_last->p_next = p_con;
        p_con->p_prev = p_last;
    }
    else
    {
        p_con->p_prev = NULL;

        p_httpt->p_first_connection = p_con;
    }

    p_httpt->i_connection_count++;
}

static void httpd_ConnnectionClose( httpd_sys_t *p_httpt, httpd_connection_t *p_con )
{
    msg_Dbg( p_httpt, "close connection from %s", inet_ntoa( p_con->sock.sin_addr ) );

    p_httpt->i_connection_count--;
    /* first cut out from list */
    if( p_con->p_prev )
    {
        p_con->p_prev->p_next = p_con->p_next;
    }
    else
    {
        p_httpt->p_first_connection = p_con->p_next;
    }

    if( p_con->p_next )
    {
        p_con->p_next->p_prev = p_con->p_prev;
    }

    if( p_con->p_file ) p_con->p_file->i_ref--;
    FREE( p_con->psz_file );

    FREE( p_con->p_buffer );
    SOCKET_CLOSE( p_con->fd );

    FREE( p_con->psz_user );
    FREE( p_con->psz_password );

    FREE( p_con->p_request );
    free( p_con );
}

static void httpd_RequestGetWord( char *word, int i_word_max, char **pp_buffer, char *p_end )
{
    char *p = *pp_buffer;
    int i;

    while( p < p_end && *p && ( *p == ' ' || *p == '\t' ) )
    {
        p++;
    }

    i = 0;
    for( i = 0; i < i_word_max && p < p_end && *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'; i++,p++)
    {
        word[i] = *p;
    }

    word[__MIN( i, i_word_max -1 )] = '\0';

    *pp_buffer = p;

}
static int httpd_RequestNextLine( char **pp_buffer, char *p_end )
{
    char *p;

    for( p = *pp_buffer; p < p_end; p++ )
    {
        if( p + 1 < p_end && *p == '\n' )
        {
            *pp_buffer = p + 1;
            return VLC_SUCCESS;
        }
        if( p + 2 < p_end && p[0] == '\r' && p[1] == '\n' )
        {
            *pp_buffer = p + 2;
            return VLC_SUCCESS;
        }
    }
    *pp_buffer = p_end;
    return VLC_EGENERIC;
}

//char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void b64_decode( char *dest, char *src )
{
    int  i_level;
    int  last = 0;
    int  b64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
        };

    for( i_level = 0; *src != '\0'; src++ )
    {
        int  c;

        c = b64[(unsigned int)*src];
        if( c == -1 )
        {
            src++;
            continue;
        }

        switch( i_level )
        {
            case 0:
                i_level++;
                break;
            case 1:
                *dest++ = ( last << 2 ) | ( ( c >> 4)&0x03 );
                i_level++;
                break;
            case 2:
                *dest++ = ( ( last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                i_level++;
                break;
            case 3:
                *dest++ = ( ( last &0x03 ) << 6 ) | c;
                i_level = 0;
        }
        last = c;
    }

    *dest = '\0';
}

static void httpd_ConnectionParseRequest( httpd_sys_t *p_httpt, httpd_connection_t *p_con )
{
    char *psz_status;
    char *p, *p_end;

    int  i;
    char command[32];
    char url[1024];
    char version[32];
    char user[512] = "";
    char password[512] = "";

    //msg_Dbg( p_httpt, "new request=\n%s", p_con->p_buffer );


    p = p_con->p_buffer;
    p_end = p + strlen( p ) + 1;

    httpd_RequestGetWord( command, 32, &p, p_end );
    httpd_RequestGetWord( url, 1024, &p, p_end );
    httpd_RequestGetWord( version, 32, &p, p_end );
    //msg_Dbg( p_httpt, "ask =%s= =%s= =%s=", command, url, version );

    p_con->p_request      = NULL;
    p_con->i_request_size = 0;
    if( !strcmp( command, "GET" ) )
    {
        p_con->i_method = HTTPD_CONNECTION_METHOD_GET;
    }
    else if( !strcmp( command, "POST" ))
    {
        p_con->i_method = HTTPD_CONNECTION_METHOD_POST;
    }
    else
    {
        /* unimplemented */
        p_con->psz_file = strdup( "/501.html" );
        p_con->i_method = HTTPD_CONNECTION_METHOD_GET;
        p_con->i_http_error = 501;
        goto create_header;
    }

    if( strcmp( version, "HTTP/1.0" ) && strcmp( version, "HTTP/1.1" ) )
    {
        p_con->psz_file = strdup( "/505.html" );
        p_con->i_http_error = 505;

        goto create_header;
    }

    /* parse headers */
    for( ;; )
    {
        char header[1024];

        if( httpd_RequestNextLine( &p, p_end ) )
        {
            //msg_Dbg( p_httpt, "failled new line" );
            break;;
        }
        //msg_Dbg( p_httpt, "new line=%s", p );

        httpd_RequestGetWord( header, 1024, &p, p_end );
        if( !strcmp( header, "\r\n" ) || !strcmp( header, "\n" ) )
        {
            break;
        }

        if( !strcmp( header, "Authorization:" ) )
        {
            char method[32];

            httpd_RequestGetWord( method, 32, &p, p_end );
            if( !strcasecmp( method, "BASIC" ) )
            {
                char basic[1024];
                char decoded[1024];

                httpd_RequestGetWord( basic, 1024, &p, p_end );
                //msg_Dbg( p_httpt, "Authorization: basic:%s", basic );
                b64_decode( decoded, basic );

                //msg_Dbg( p_httpt, "Authorization: decoded:%s", decoded );
                if( strchr( decoded, ':' ) )
                {
                    char *p = strchr( decoded, ':' );

                    p[0] = '\0'; p++;
                    strcpy( user, decoded );
                    strcpy( password, p );
                }
            }
        }
    }

    if( strchr( url, '?' ) )
    {
        char *p_request = strchr( url, '?' );
        *p_request++ = '\0';
        p_con->psz_file = strdup( url );
        p_con->p_request = strdup( p_request );
        p_con->i_request_size = strlen( p_con->p_request );
    }
    else
    {
        p_con->psz_file = strdup( url );
    }

    /* fix p_request */
    if( p_con->i_method == HTTPD_CONNECTION_METHOD_POST )
    {
        char *p_request;
        if( strstr( p_con->p_buffer, "\r\n\r\n" ) )
        {
            p_request = strstr( p_con->p_buffer, "\r\n\r\n" ) + 4;
        }
        else if( strstr( p_con->p_buffer, "\n\n" ) )
        {
            p_request = strstr( p_con->p_buffer, "\n\n" ) + 2;
        }
        else
        {
            p_request = NULL;
        }
        if( p_request && p_request < p_end )
        {
            p_con->i_request_size = p_end - p_request;
            p_con->p_request = malloc( p_con->i_request_size + 1);

            memcpy( p_con->p_request,
                    p_request,
                    p_con->i_request_size );

            p_con->p_request[p_con->i_request_size] = '\0';
        }
    }
    p_con->i_http_error = 200;

create_header:
    //msg_Dbg( p_httpt, "ask %s %s %d", command, p_con->psz_file, p_con->i_http_error );
    FREE( p_con->p_buffer );
    p_con->i_buffer = 0;
    p_con->i_buffer_size = 0;

    //vlc_mutex_lock( &p_httpt->file_lock );
search_file:
    /* search file */
    p_con->p_file = NULL;
    for( i = 0; i < p_httpt->i_file_count; i++ )
    {
        if( !strcmp( p_httpt->file[i]->psz_file, p_con->psz_file ) )
        {
            if( p_httpt->file[i]->b_stream ||
                ( p_con->i_method == HTTPD_CONNECTION_METHOD_GET  && p_httpt->file[i]->pf_get ) ||
                ( p_con->i_method == HTTPD_CONNECTION_METHOD_POST && p_httpt->file[i]->pf_post ) )
            {
                p_con->p_file = p_httpt->file[i];
                break;
            }
        }
    }

    if( !p_con->p_file )
    {
        p_con->psz_file = strdup( "/404.html" );
        p_con->i_http_error = 404;

        /* XXX be sure that "/404.html" exist else ... */
        goto search_file;
    }

    if( p_con->p_file->i_authenticate_method == HTTPD_AUTHENTICATE_BASIC )
    {
        if( strcmp( user, p_con->p_file->psz_user ) || strcmp( password, p_con->p_file->psz_password ) )
        {
            p_con->psz_file = strdup( "/401.html" );
            strcpy( user, p_con->p_file->psz_user );
            p_con->i_http_error = 401;

            /* XXX do not put password on 404 else ... */
            goto search_file;
        }
    }

    p_con->p_file->i_ref++;
//    vlc_mutex_unlock( &p_httpt->file_lock );

    switch( p_con->i_http_error )
    {
        case 200:
            psz_status = "OK";
            break;

        case 401:
            psz_status = "Authorization Required";
            break;
        default:
            psz_status = "Unknown";
            break;
    }

    p_con->i_state = HTTPD_CONNECTION_SENDING_HEADER;

    p_con->i_buffer_size = 4096;
    p_con->i_buffer = 0;

    /* we send stream header with this one */
    if( p_con->i_http_error == 200 && p_con->p_file->b_stream )
    {
        p_con->i_buffer_size += p_con->p_file->i_header_size;
    }

    p = p_con->p_buffer = malloc( p_con->i_buffer_size );

    p += sprintf( p, "HTTP/1.0 %d %s\r\n", p_con->i_http_error, psz_status );
    p += sprintf( p, "Content-type: %s\r\n", p_con->p_file->psz_mime );
    if( p_con->i_http_error == 401 )
    {
        p += sprintf( p, "WWW-Authenticate: Basic realm=\"%s\"\r\n", user );
    }
    p += sprintf( p, "Cache-Control: no-cache\r\n" );
    p += sprintf( p, "\r\n" );

    p_con->i_buffer_size = strlen( p_con->p_buffer );// + 1;

    if( p_con->i_http_error == 200 && p_con->p_file->b_stream && p_con->p_file->i_header_size > 0 )
    {
        /* add stream header */
        memcpy( &p_con->p_buffer[p_con->i_buffer_size],
                p_con->p_file->p_header,
                p_con->p_file->i_header_size );
        p_con->i_buffer_size += p_con->p_file->i_header_size;
    }

    //msg_Dbg( p_httpt, "answer=\n%s", p_con->p_buffer );
}
#define HTTPD_STREAM_PACKET 10000
static void httpd_Thread( httpd_sys_t *p_httpt )
{
    httpd_file_t    *p_page_admin;
    httpd_file_t    *p_page_401;
    httpd_file_t    *p_page_404;

    httpd_connection_t *p_con;

    msg_Info( p_httpt, "httpd started" );

    p_page_401 = _RegisterFile( p_httpt,
                                "/401.html", "text/html",
                                NULL, NULL,
                                httpd_page_401_get,
                                NULL,
                                (httpd_file_callback_args_t*)NULL );
    p_page_404 = _RegisterFile( p_httpt,
                                "/404.html", "text/html",
                                NULL, NULL,
                                httpd_page_404_get,
                                NULL,
                                (httpd_file_callback_args_t*)NULL );
    p_page_admin = _RegisterFile( p_httpt,
                                  "/admin.html", "text/html",
                                  "admin", "salut",
                                  httpd_page_admin_get,
                                  NULL,
                                  (httpd_file_callback_args_t*)p_httpt );

    while( !p_httpt->b_die )
    {
        struct timeval  timeout;
        fd_set          fds_read;
        fd_set          fds_write;
        int             i_handle_max = 0;
        int             i_ret;
        int i;
        if( p_httpt->i_host_count <= 0 )
        {
            msleep( 100 * 1000 );
            continue;
        }

        /* we will create a socket set with host and connection */
        FD_ZERO( &fds_read );
        FD_ZERO( &fds_write );

        vlc_mutex_lock( &p_httpt->host_lock );
        vlc_mutex_lock( &p_httpt->connection_lock );
        for( i = 0; i < p_httpt->i_host_count; i++ )
        {
            FD_SET( p_httpt->host[i]->fd, &fds_read );
            i_handle_max = __MAX( i_handle_max, p_httpt->host[i]->fd );
        }
        for( p_con = p_httpt->p_first_connection; p_con != NULL; )
        {
            /* no more than 10s of inactivity */
            if( p_con->i_last_activity_date + (mtime_t)HTTPD_CONNECTION_MAX_UNUSED < mdate() ||
                p_con->i_state == HTTPD_CONNECTION_TO_BE_CLOSED)
            {
                httpd_connection_t *p_next = p_con->p_next;

                msg_Dbg( p_httpt,  "close unused connection" );
                httpd_ConnnectionClose( p_httpt, p_con );
                p_con = p_next;
                continue;
            }

            if( p_con->i_state == HTTPD_CONNECTION_SENDING_STREAM && p_con->i_stream_pos + HTTPD_STREAM_PACKET >= p_con->p_file->i_buffer_pos )
            {
                p_con = p_con->p_next;
                continue;
            }

            if( p_con->i_state == HTTPD_CONNECTION_RECEIVING_REQUEST )
            {
                FD_SET( p_con->fd, &fds_read );
            }
            else
            {
                FD_SET( p_con->fd, &fds_write );
            }
            i_handle_max = __MAX( i_handle_max, p_con->fd );

            p_con = p_con->p_next;
        }
        vlc_mutex_unlock( &p_httpt->host_lock );
        vlc_mutex_unlock( &p_httpt->connection_lock );

        /* we will wait 0.5s */
        timeout.tv_sec = 0;
        timeout.tv_usec = 500*1000;

        i_ret = select( i_handle_max + 1,
                        &fds_read,
                        &fds_write,
                        NULL,
                        &timeout );
        if( i_ret == -1 && errno != EINTR )
        {
            msg_Warn( p_httpt, "cannot select sockets" );
            msleep( 1000 );
            continue;
        }
        if( i_ret <= 0 )
        {
//            msg_Dbg( p_httpt, "waiting..." );
            continue;
        }

        vlc_mutex_lock( &p_httpt->host_lock );
        /* accept/refuse new connection */
        for( i = 0; i < p_httpt->i_host_count; i++ )
        {
            int     i_sock_size = sizeof( struct sockaddr_in );
            struct  sockaddr_in sock;
            int     fd;

            fd = accept( p_httpt->host[i]->fd, (struct sockaddr *)&sock,
                         &i_sock_size );
            if( fd > 0 )
            {
#if defined( WIN32 ) || defined( UNDER_CE )
                {
                    unsigned long i_dummy = 1;
                    ioctlsocket( fd, FIONBIO, &i_dummy );
                }
#else
                fcntl( fd, F_SETFL, O_NONBLOCK );
#endif

                if( p_httpt->i_connection_count >= HTTPD_MAX_CONNECTION )
                {
                    msg_Warn( p_httpt, "max connection reached" );
                    SOCKET_CLOSE( fd );
                    continue;
                }
                /* create a new connection and link it */
                httpd_ConnnectionNew( p_httpt, fd, &sock );

            }
        }
        vlc_mutex_unlock( &p_httpt->host_lock );

        vlc_mutex_lock( &p_httpt->file_lock );
        /* now do work for all connections */
        for( p_con = p_httpt->p_first_connection; p_con != NULL; )
        {
            if( p_con->i_state == HTTPD_CONNECTION_RECEIVING_REQUEST )
            {
                int i_len;
                /* read data */
                i_len = recv( p_con->fd,
                              p_con->p_buffer + p_con->i_buffer,
                              p_con->i_buffer_size - p_con->i_buffer, 0 );


#if defined( WIN32 ) || defined( UNDER_CE )
                if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
                if( ( i_len < 0 && errno != EAGAIN && errno != EINTR ) || ( i_len == 0 ) )
#endif
                {
                    httpd_connection_t *p_next = p_con->p_next;

                    httpd_ConnnectionClose( p_httpt, p_con );
                    p_con = p_next;
                }
                else if( i_len > 0 )
                {
                    uint8_t *ptr;
                    p_con->i_last_activity_date = mdate();
                    p_con->i_buffer += i_len;

                    ptr = p_con->p_buffer + p_con->i_buffer;

                    if( ( p_con->i_buffer >= 2 && !strncmp( ptr - 2, "\n\n", 2 ) )||
                        ( p_con->i_buffer >= 4 && !strncmp( ptr - 4, "\r\n\r\n", 4 ) ) ||
                        p_con->i_buffer >= p_con->i_buffer_size )
                    {
                        p_con->p_buffer[__MIN( p_con->i_buffer, p_con->i_buffer_size - 1 )] = '\0';
                        httpd_ConnectionParseRequest( p_httpt, p_con );
                    }

                    p_con = p_con->p_next;
                }
                else
                {
                    p_con = p_con->p_next;
                }
                continue;   /* just for clarity */
            }
            else if( p_con->i_state == HTTPD_CONNECTION_SENDING_HEADER || p_con->i_state == HTTPD_CONNECTION_SENDING_FILE )
            {
                int i_len;

                /* write data */
                if( p_con->i_buffer_size - p_con->i_buffer > 0 )
                {
                    i_len = send( p_con->fd, p_con->p_buffer + p_con->i_buffer, p_con->i_buffer_size - p_con->i_buffer, 0 );
                }
                else
                {
                    i_len = 0;
                }
//                msg_Warn( p_httpt, "on %d send %d bytes %s", p_con->i_buffer_size, i_len, p_con->p_buffer + p_con->i_buffer );

#if defined( WIN32 ) || defined( UNDER_CE )
                if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
                if( ( i_len < 0 && errno != EAGAIN && errno != EINTR ) || ( i_len == 0 ) )
#endif
                {
                    httpd_connection_t *p_next = p_con->p_next;

                    httpd_ConnnectionClose( p_httpt, p_con );
                    p_con = p_next;
                }
                else if( i_len > 0 )
                {
                    p_con->i_last_activity_date = mdate();
                    p_con->i_buffer += i_len;

                    if( p_con->i_buffer >= p_con->i_buffer_size )
                    {
                        if( p_con->i_state == HTTPD_CONNECTION_SENDING_HEADER )
                        {
                            p_con->i_buffer_size = 0;
                            p_con->i_buffer = 0;
                            FREE( p_con->p_buffer );

                            if( !p_con->p_file->b_stream )
                            {
                                p_con->i_state = HTTPD_CONNECTION_SENDING_FILE; // be sure to out from HTTPD_CONNECTION_SENDING_HEADER
                                if( p_con->i_method == HTTPD_CONNECTION_METHOD_GET )
                                {
                                    p_con->p_file->pf_get( p_con->p_file->p_sys,
                                                           p_con->p_request, p_con->i_request_size,
                                                           &p_con->p_buffer, &p_con->i_buffer_size );
                                }
                                else if( p_con->i_method == HTTPD_CONNECTION_METHOD_POST )
                                {
                                    p_con->p_file->pf_post( p_con->p_file->p_sys,
                                                            p_con->p_request, p_con->i_request_size,
                                                            &p_con->p_buffer, &p_con->i_buffer_size );
                                }
                                else
                                {
                                    p_con->p_buffer = NULL;
                                    p_con->i_buffer_size = 0;
                                }
                            }
                            else
                            {
                                p_con->i_state = HTTPD_CONNECTION_SENDING_STREAM;
                                p_con->i_stream_pos = p_con->p_file->i_buffer_last_pos;
                            }
                            p_con = p_con->p_next;
                        }
                        else
                        {
                            httpd_connection_t *p_next = p_con->p_next;

                            httpd_ConnnectionClose( p_httpt, p_con );
                            p_con = p_next;
                        }
                    }
                    else
                    {
                        p_con = p_con->p_next;
                    }
                }
                else
                {
                    p_con = p_con->p_next;
                }
                continue;   /* just for clarity */
            }
            else if( p_con->i_state == HTTPD_CONNECTION_SENDING_STREAM )
            {
                httpd_stream_t *p_stream = p_con->p_file;
                int i_send;
                int i_write;

                if( p_con->i_stream_pos < p_stream->i_buffer_pos )
                {
                    int i_pos;
                    /* check if this p_con aren't to late */
                    if( p_con->i_stream_pos + p_stream->i_buffer_size < p_stream->i_buffer_pos )
                    {
                        fprintf(stderr, "fixing i_stream_pos (old=%lld i_buffer_pos=%lld\n", p_con->i_stream_pos, p_stream->i_buffer_pos  );
                        p_con->i_stream_pos = p_stream->i_buffer_last_pos;
                    }

                    i_pos = p_con->i_stream_pos % p_stream->i_buffer_size;
                    /* size until end of buffer */
                    i_write = p_stream->i_buffer_size - i_pos;
                    /* is it more than valid data */
                    if( i_write >= p_stream->i_buffer_pos - p_con->i_stream_pos )
                    {
                        i_write = p_stream->i_buffer_pos - p_con->i_stream_pos;
                    }
                    /* limit to HTTPD_STREAM_PACKET */
                    if( i_write > HTTPD_STREAM_PACKET )
                    {
                        i_write = HTTPD_STREAM_PACKET;
                    }
                    i_send = send( p_con->fd, &p_stream->p_buffer[i_pos], i_write, 0 );

#if defined( WIN32 ) || defined( UNDER_CE )
                    if( ( i_send < 0 && WSAGetLastError() != WSAEWOULDBLOCK )|| ( i_send == 0 ) )
#else
                    if( ( i_send < 0 && errno != EAGAIN && errno != EINTR )|| ( i_send == 0 ) )
#endif
                    {
                        httpd_connection_t *p_next = p_con->p_next;

                        httpd_ConnnectionClose( p_httpt, p_con );
                        p_con = p_next;
                        continue;
                    }
                    else if( i_send > 0 )
                    {
                        p_con->i_last_activity_date = mdate();
                        p_con->i_stream_pos += i_send;
                    }
                }
                p_con = p_con->p_next;
                continue;   /* just for clarity */
            }
            else if( p_con->i_state != HTTPD_CONNECTION_TO_BE_CLOSED )
            {
                msg_Warn( p_httpt, "cannot occur (Invalid p_con->i_state)" );
                p_con = p_con->p_next;
            }
        }   /* for over connection */

        vlc_mutex_unlock( &p_httpt->file_lock );
    }
    msg_Info( p_httpt, "httpd stopped" );

    _UnregisterFile( p_httpt, p_page_401 );
    _UnregisterFile( p_httpt, p_page_404 );
    _UnregisterFile( p_httpt, p_page_admin );
}
