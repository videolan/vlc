/*****************************************************************************
 * httpd.c
 *****************************************************************************
 * Copyright (C) 2004-2006 VLC authors and VideoLAN
 * Copyright © 2004-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_httpd.h>

#include <assert.h>

#include <vlc_network.h>
#include <vlc_tls.h>
#include <vlc_strings.h>
#include <vlc_rand.h>
#include <vlc_charset.h>
#include <vlc_url.h>
#include <vlc_mime.h>
#include "../libvlc.h"

#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif

#if defined( _WIN32 )
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

#if defined( _WIN32 )
/* We need HUGE buffer otherwise TCP throughput is very limited */
#define HTTPD_CL_BUFSIZE 1000000
#else
#define HTTPD_CL_BUFSIZE 10000
#endif

static void httpd_ClientClean( httpd_client_t *cl );

/* each host run in his own thread */
struct httpd_host_t
{
    VLC_COMMON_MEMBERS

    /* ref count */
    unsigned    i_ref;

    /* address/port and socket for listening at connections */
    int         *fds;
    unsigned     nfd;
    unsigned     port;

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    /* all registered url (becarefull that 2 httpd_url_t could point at the same url)
     * This will slow down the url research but make my live easier
     * All url will have their cb trigger, but only the first one can answer
     * */
    int         i_url;
    httpd_url_t **url;

    int            i_client;
    httpd_client_t **client;

    /* TLS data */
    vlc_tls_creds_t *p_tls;
};


struct httpd_url_t
{
    httpd_host_t *host;

    vlc_mutex_t lock;

    char      *psz_url;
    char      *psz_user;
    char      *psz_password;

    struct
    {
        httpd_callback_t     cb;
        httpd_callback_sys_t *p_sys;
    } catch[HTTPD_MSG_MAX];
};

/* status */
enum
{
    HTTPD_CLIENT_RECEIVING,
    HTTPD_CLIENT_RECEIVE_DONE,

    HTTPD_CLIENT_SENDING,
    HTTPD_CLIENT_SEND_DONE,

    HTTPD_CLIENT_WAITING,

    HTTPD_CLIENT_DEAD,

    HTTPD_CLIENT_TLS_HS_IN,
    HTTPD_CLIENT_TLS_HS_OUT
};

/* mode */
enum
{
    HTTPD_CLIENT_FILE,      /* default */
    HTTPD_CLIENT_STREAM,    /* regulary get data from cb */
};

struct httpd_client_t
{
    httpd_url_t *url;

    int     i_ref;

    int     fd;

    bool    b_stream_mode;
    uint8_t i_state;

    mtime_t i_activity_date;
    mtime_t i_activity_timeout;

    /* buffer for reading header */
    int     i_buffer_size;
    int     i_buffer;
    uint8_t *p_buffer;

    /* */
    httpd_message_t query;  /* client -> httpd */
    httpd_message_t answer; /* httpd -> client */

    /* TLS data */
    vlc_tls_t *p_tls;
};


/*****************************************************************************
 * Various functions
 *****************************************************************************/
typedef struct
{
    unsigned   i_code;
    const char psz_reason[36];
} http_status_info;

static const http_status_info http_reason[] =
{
  /*{ 100, "Continue" },
    { 101, "Switching Protocols" },*/
    { 200, "OK" },
  /*{ 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-authoritative information" },
    { 204, "No content" },
    { 205, "Reset content" },
    { 206, "Partial content" },
    { 250, "Low on storage space" },
    { 300, "Multiple choices" },*/
    { 301, "Moved permanently" },
  /*{ 302, "Moved temporarily" },
    { 303, "See other" },
    { 304, "Not modified" },
    { 305, "Use proxy" },
    { 307, "Temporary redirect" },
    { 400, "Bad request" },*/
    { 401, "Unauthorized" },
  /*{ 402, "Payment Required" },*/
    { 403, "Forbidden" },
    { 404, "Not found" },
    { 405, "Method not allowed" },
  /*{ 406, "Not acceptable" },
    { 407, "Proxy authentication required" },
    { 408, "Request time-out" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length required" },
    { 412, "Precondition failed" },
    { 413, "Request entity too large" },
    { 414, "Request-URI too large" },
    { 415, "Unsupported media Type" },
    { 416, "Requested range not satisfiable" },
    { 417, "Expectation failed" },
    { 451, "Parameter not understood" },
    { 452, "Conference not found" },
    { 453, "Not enough bandwidth" },*/
    { 454, "Session not found" },
    { 455, "Method not valid in this State" },
    { 456, "Header field not valid for resource" },
    { 457, "Invalid range" },
  /*{ 458, "Read-only parameter" },*/
    { 459, "Aggregate operation not allowed" },
    { 460, "Non-aggregate operation not allowed" },
    { 461, "Unsupported transport" },
  /*{ 462, "Destination unreachable" },*/
    { 500, "Internal server error" },
    { 501, "Not implemented" },
  /*{ 502, "Bad gateway" },*/
    { 503, "Service unavailable" },
  /*{ 504, "Gateway time-out" },*/
    { 505, "Protocol version not supported" },
    { 551, "Option not supported" },
    { 999, "" }
};

static const char psz_fallback_reason[5][16] =
{ "Continue", "OK", "Found", "Client error", "Server error" };

static const char *httpd_ReasonFromCode( unsigned i_code )
{
    const http_status_info *p;

    assert( ( i_code >= 100 ) && ( i_code <= 599 ) );

    for (p = http_reason; i_code > p->i_code; p++);

    if( p->i_code == i_code )
        return p->psz_reason;

    return psz_fallback_reason[(i_code / 100) - 1];
}


static size_t httpd_HtmlError (char **body, int code, const char *url)
{
    const char *errname = httpd_ReasonFromCode (code);
    assert (errname != NULL);

    int res = asprintf (body,
        "<?xml version=\"1.0\" encoding=\"ascii\" ?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\""
        " \"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<title>%s</title>\n"
        "</head>\n"
        "<body>\n"
        "<h1>%d %s%s%s%s</h1>\n"
        "<hr />\n"
        "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
        "</body>\n"
        "</html>\n", errname, code, errname,
        (url ? " (" : ""), (url ? url : ""), (url ? ")" : ""));

    if (res == -1)
    {
        *body = NULL;
        return 0;
    }

    return (size_t)res;
}


/*****************************************************************************
 * High Level Functions: httpd_file_t
 *****************************************************************************/
struct httpd_file_t
{
    httpd_url_t *url;

    char *psz_url;
    char *psz_mime;

    httpd_file_callback_t pf_fill;
    httpd_file_sys_t      *p_sys;

};

static int
httpd_FileCallBack( httpd_callback_sys_t *p_sys, httpd_client_t *cl,
                    httpd_message_t *answer, const httpd_message_t *query )
{
    httpd_file_t *file = (httpd_file_t*)p_sys;
    uint8_t **pp_body, *p_body;
    const char *psz_connection;
    int *pi_body, i_body;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= 1;
    answer->i_type   = HTTPD_MSG_ANSWER;

    answer->i_status = 200;

    httpd_MsgAdd( answer, "Content-type",  "%s", file->psz_mime );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( query->i_type != HTTPD_MSG_HEAD )
    {
        pp_body = &answer->p_body;
        pi_body = &answer->i_body;
    }
    else
    {
        /* The file still needs to be executed. */
        p_body = NULL;
        i_body = 0;
        pp_body = &p_body;
        pi_body = &i_body;
    }

    if( query->i_type == HTTPD_MSG_POST )
    {
        /* msg_Warn not supported */
    }

    uint8_t *psz_args = query->psz_args;
    file->pf_fill( file->p_sys, file, psz_args, pp_body, pi_body );

    if( query->i_type == HTTPD_MSG_HEAD && p_body != NULL )
    {
        free( p_body );
    }

    /* We respect client request */
    psz_connection = httpd_MsgGet( &cl->query, "Connection" );
    if( psz_connection != NULL )
    {
        httpd_MsgAdd( answer, "Connection", "%s", psz_connection );
    }

    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

    return VLC_SUCCESS;
}

httpd_file_t *httpd_FileNew( httpd_host_t *host,
                             const char *psz_url, const char *psz_mime,
                             const char *psz_user, const char *psz_password,
                             httpd_file_callback_t pf_fill,
                             httpd_file_sys_t *p_sys )
{
    httpd_file_t *file = xmalloc( sizeof( httpd_file_t ) );

    file->url = httpd_UrlNew( host, psz_url, psz_user, psz_password );
    if( file->url == NULL )
    {
        free( file );
        return NULL;
    }

    file->psz_url  = strdup( psz_url );
    if( psz_mime && *psz_mime )
    {
        file->psz_mime = strdup( psz_mime );
    }
    else
    {
        file->psz_mime = strdup( vlc_mime_Ext2Mime( psz_url ) );
    }

    file->pf_fill = pf_fill;
    file->p_sys   = p_sys;

    httpd_UrlCatch( file->url, HTTPD_MSG_HEAD, httpd_FileCallBack,
                    (httpd_callback_sys_t*)file );
    httpd_UrlCatch( file->url, HTTPD_MSG_GET,  httpd_FileCallBack,
                    (httpd_callback_sys_t*)file );
    httpd_UrlCatch( file->url, HTTPD_MSG_POST, httpd_FileCallBack,
                    (httpd_callback_sys_t*)file );

    return file;
}

httpd_file_sys_t *httpd_FileDelete( httpd_file_t *file )
{
    httpd_file_sys_t *p_sys = file->p_sys;

    httpd_UrlDelete( file->url );

    free( file->psz_url );
    free( file->psz_mime );

    free( file );

    return p_sys;
}

/*****************************************************************************
 * High Level Functions: httpd_handler_t (for CGIs)
 *****************************************************************************/
struct httpd_handler_t
{
    httpd_url_t *url;

    httpd_handler_callback_t pf_fill;
    httpd_handler_sys_t      *p_sys;

};

static int
httpd_HandlerCallBack( httpd_callback_sys_t *p_sys, httpd_client_t *cl,
                       httpd_message_t *answer, const httpd_message_t *query )
{
    httpd_handler_t *handler = (httpd_handler_t*)p_sys;
    char psz_remote_addr[NI_MAXNUMERICHOST];

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = HTTPD_PROTO_NONE;
    answer->i_type   = HTTPD_MSG_ANSWER;

    /* We do it ourselves, thanks */
    answer->i_status = 0;

    if( httpd_ClientIP( cl, psz_remote_addr, NULL ) == NULL )
        *psz_remote_addr = '\0';

    uint8_t *psz_args = query->psz_args;
    handler->pf_fill( handler->p_sys, handler, query->psz_url, psz_args,
                      query->i_type, query->p_body, query->i_body,
                      psz_remote_addr, NULL,
                      &answer->p_body, &answer->i_body );

    if( query->i_type == HTTPD_MSG_HEAD )
    {
        char *p = (char *)answer->p_body;

        /* Looks for end of header (i.e. one empty line) */
        while ( (p = strchr( p, '\r' )) != NULL )
        {
            if( p[1] && p[1] == '\n' && p[2] && p[2] == '\r'
                 && p[3] && p[3] == '\n' )
            {
                break;
            }
        }

        if( p != NULL )
        {
            p[4] = '\0';
            answer->i_body = strlen((char*)answer->p_body) + 1;
            answer->p_body = xrealloc( answer->p_body, answer->i_body );
        }
    }

    if( strncmp( (char *)answer->p_body, "HTTP/1.", 7 ) )
    {
        int i_status, i_headers;
        char *psz_headers, *psz_new;
        const char *psz_status;

        if( !strncmp( (char *)answer->p_body, "Status: ", 8 ) )
        {
            /* Apache-style */
            i_status = strtol( (char *)&answer->p_body[8], &psz_headers, 0 );
            if( *psz_headers == '\r' || *psz_headers == '\n' ) psz_headers++;
            if( *psz_headers == '\n' ) psz_headers++;
            i_headers = answer->i_body - (psz_headers - (char *)answer->p_body);
        }
        else
        {
            i_status = 200;
            psz_headers = (char *)answer->p_body;
            i_headers = answer->i_body;
        }

        psz_status = httpd_ReasonFromCode( i_status );
        answer->i_body = sizeof("HTTP/1.0 xxx \r\n")
                        + strlen(psz_status) + i_headers - 1;
        psz_new = (char *)xmalloc( answer->i_body + 1);
        sprintf( psz_new, "HTTP/1.0 %03d %s\r\n", i_status, psz_status );
        memcpy( &psz_new[strlen(psz_new)], psz_headers, i_headers );
        free( answer->p_body );
        answer->p_body = (uint8_t *)psz_new;
    }

    return VLC_SUCCESS;
}

httpd_handler_t *httpd_HandlerNew( httpd_host_t *host, const char *psz_url,
                                   const char *psz_user,
                                   const char *psz_password,
                                   httpd_handler_callback_t pf_fill,
                                   httpd_handler_sys_t *p_sys )
{
    httpd_handler_t *handler = xmalloc( sizeof( httpd_handler_t ) );

    handler->url = httpd_UrlNew( host, psz_url, psz_user, psz_password );
    if( handler->url == NULL )
    {
        free( handler );
        return NULL;
    }

    handler->pf_fill = pf_fill;
    handler->p_sys   = p_sys;

    httpd_UrlCatch( handler->url, HTTPD_MSG_HEAD, httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler );
    httpd_UrlCatch( handler->url, HTTPD_MSG_GET,  httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler );
    httpd_UrlCatch( handler->url, HTTPD_MSG_POST, httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler );

    return handler;
}

httpd_handler_sys_t *httpd_HandlerDelete( httpd_handler_t *handler )
{
    httpd_handler_sys_t *p_sys = handler->p_sys;
    httpd_UrlDelete( handler->url );
    free( handler );
    return p_sys;
}

/*****************************************************************************
 * High Level Functions: httpd_redirect_t
 *****************************************************************************/
struct httpd_redirect_t
{
    httpd_url_t *url;
    char        *psz_dst;
};

static int httpd_RedirectCallBack( httpd_callback_sys_t *p_sys,
                                   httpd_client_t *cl, httpd_message_t *answer,
                                   const httpd_message_t *query )
{
    httpd_redirect_t *rdir = (httpd_redirect_t*)p_sys;
    char *p_body;
    (void)cl;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= 1;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_status = 301;

    answer->i_body = httpd_HtmlError (&p_body, 301, rdir->psz_dst);
    answer->p_body = (unsigned char *)p_body;

    /* XXX check if it's ok or we need to set an absolute url */
    httpd_MsgAdd( answer, "Location",  "%s", rdir->psz_dst );

    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

    return VLC_SUCCESS;
}

httpd_redirect_t *httpd_RedirectNew( httpd_host_t *host, const char *psz_url_dst,
                                     const char *psz_url_src )
{
    httpd_redirect_t *rdir = xmalloc( sizeof( httpd_redirect_t ) );

    rdir->url = httpd_UrlNew( host, psz_url_src, NULL, NULL );
    if( rdir->url == NULL )
    {
        free( rdir );
        return NULL;
    }
    rdir->psz_dst = strdup( psz_url_dst );

    /* Redirect apply for all HTTP request and RTSP DESCRIBE resquest */
    httpd_UrlCatch( rdir->url, HTTPD_MSG_HEAD, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir );
    httpd_UrlCatch( rdir->url, HTTPD_MSG_GET, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir );
    httpd_UrlCatch( rdir->url, HTTPD_MSG_POST, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir );
    httpd_UrlCatch( rdir->url, HTTPD_MSG_DESCRIBE, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir );

    return rdir;
}
void httpd_RedirectDelete( httpd_redirect_t *rdir )
{
    httpd_UrlDelete( rdir->url );
    free( rdir->psz_dst );
    free( rdir );
}

/*****************************************************************************
 * High Level Funtions: httpd_stream_t
 *****************************************************************************/
struct httpd_stream_t
{
    vlc_mutex_t lock;
    httpd_url_t *url;

    char    *psz_mime;

    /* Header to send as first packet */
    uint8_t *p_header;
    int     i_header;

    /* circular buffer */
    int         i_buffer_size;      /* buffer size, can't be reallocated smaller */
    uint8_t     *p_buffer;          /* buffer */
    int64_t     i_buffer_pos;       /* absolute position from begining */
    int64_t     i_buffer_last_pos;  /* a new connection will start with that */
};

static int httpd_StreamCallBack( httpd_callback_sys_t *p_sys,
                                 httpd_client_t *cl, httpd_message_t *answer,
                                 const httpd_message_t *query )
{
    httpd_stream_t *stream = (httpd_stream_t*)p_sys;

    if( answer == NULL || query == NULL || cl == NULL )
    {
        return VLC_SUCCESS;
    }

    if( answer->i_body_offset > 0 )
    {
        int64_t i_write;
        int     i_pos;

#if 0
        fprintf( stderr, "httpd_StreamCallBack i_body_offset=%lld\n",
                 answer->i_body_offset );
#endif

        if( answer->i_body_offset >= stream->i_buffer_pos )
        {
            /* fprintf( stderr, "httpd_StreamCallBack: no data\n" ); */
            return VLC_EGENERIC;    /* wait, no data available */
        }
        if( answer->i_body_offset + stream->i_buffer_size <
            stream->i_buffer_pos )
        {
            /* this client isn't fast enough */
#if 0
            fprintf( stderr, "fixing i_body_offset (old=%lld new=%lld)\n",
                     answer->i_body_offset, stream->i_buffer_last_pos );
#endif
            answer->i_body_offset = stream->i_buffer_last_pos;
        }

        i_pos   = answer->i_body_offset % stream->i_buffer_size;
        i_write = stream->i_buffer_pos - answer->i_body_offset;
        if( i_write > HTTPD_CL_BUFSIZE )
        {
            i_write = HTTPD_CL_BUFSIZE;
        }
        else if( i_write <= 0 )
        {
            return VLC_EGENERIC;    /* wait, no data available */
        }

        /* Don't go past the end of the circular buffer */
        i_write = __MIN( i_write, stream->i_buffer_size - i_pos );

        /* using HTTPD_MSG_ANSWER -> data available */
        answer->i_proto  = HTTPD_PROTO_HTTP;
        answer->i_version= 0;
        answer->i_type   = HTTPD_MSG_ANSWER;

        answer->i_body = i_write;
        answer->p_body = xmalloc( i_write );
        memcpy( answer->p_body, &stream->p_buffer[i_pos], i_write );

        answer->i_body_offset += i_write;

        return VLC_SUCCESS;
    }
    else
    {
        answer->i_proto  = HTTPD_PROTO_HTTP;
        answer->i_version= 0;
        answer->i_type   = HTTPD_MSG_ANSWER;

        answer->i_status = 200;

        if( query->i_type != HTTPD_MSG_HEAD )
        {
            cl->b_stream_mode = true;
            vlc_mutex_lock( &stream->lock );
            /* Send the header */
            if( stream->i_header > 0 )
            {
                answer->i_body = stream->i_header;
                answer->p_body = xmalloc( stream->i_header );
                memcpy( answer->p_body, stream->p_header, stream->i_header );
            }
            answer->i_body_offset = stream->i_buffer_last_pos;
            vlc_mutex_unlock( &stream->lock );
        }
        else
        {
            httpd_MsgAdd( answer, "Content-Length", "%d", 0 );
            answer->i_body_offset = 0;
        }

        if( !strcmp( stream->psz_mime, "video/x-ms-asf-stream" ) )
        {
            bool b_xplaystream = false;
            int i;

            httpd_MsgAdd( answer, "Content-type", "%s",
                          "application/octet-stream" );
            httpd_MsgAdd( answer, "Server", "Cougar 4.1.0.3921" );
            httpd_MsgAdd( answer, "Pragma", "no-cache" );
            httpd_MsgAdd( answer, "Pragma", "client-id=%lu",
                          vlc_mrand48()&0x7fff );
            httpd_MsgAdd( answer, "Pragma", "features=\"broadcast\"" );

            /* Check if there is a xPlayStrm=1 */
            for( i = 0; i < query->i_name; i++ )
            {
                if( !strcasecmp( query->name[i],  "Pragma" ) &&
                    strstr( query->value[i], "xPlayStrm=1" ) )
                {
                    b_xplaystream = true;
                }
            }

            if( !b_xplaystream )
            {
                answer->i_body_offset = 0;
            }
        }
        else
        {
            httpd_MsgAdd( answer, "Content-type",  "%s", stream->psz_mime );
        }
        httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );
        return VLC_SUCCESS;
    }
}

httpd_stream_t *httpd_StreamNew( httpd_host_t *host,
                                 const char *psz_url, const char *psz_mime,
                                 const char *psz_user, const char *psz_password )
{
    httpd_stream_t *stream = xmalloc( sizeof( httpd_stream_t ) );

    stream->url = httpd_UrlNew( host, psz_url, psz_user, psz_password );
    if( stream->url == NULL )
    {
        free( stream );
        return NULL;
    }
    vlc_mutex_init( &stream->lock );
    if( psz_mime && *psz_mime )
    {
        stream->psz_mime = strdup( psz_mime );
    }
    else
    {
        stream->psz_mime = strdup( vlc_mime_Ext2Mime( psz_url ) );
    }
    stream->i_header = 0;
    stream->p_header = NULL;
    stream->i_buffer_size = 5000000;    /* 5 Mo per stream */
    stream->p_buffer = xmalloc( stream->i_buffer_size );
    /* We set to 1 to make life simpler
     * (this way i_body_offset can never be 0) */
    stream->i_buffer_pos = 1;
    stream->i_buffer_last_pos = 1;

    httpd_UrlCatch( stream->url, HTTPD_MSG_HEAD, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream );
    httpd_UrlCatch( stream->url, HTTPD_MSG_GET, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream );
    httpd_UrlCatch( stream->url, HTTPD_MSG_POST, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream );

    return stream;
}

int httpd_StreamHeader( httpd_stream_t *stream, uint8_t *p_data, int i_data )
{
    vlc_mutex_lock( &stream->lock );
    free( stream->p_header );
    stream->p_header = NULL;

    stream->i_header = i_data;
    if( i_data > 0 )
    {
        stream->p_header = xmalloc( i_data );
        memcpy( stream->p_header, p_data, i_data );
    }
    vlc_mutex_unlock( &stream->lock );

    return VLC_SUCCESS;
}

int httpd_StreamSend( httpd_stream_t *stream, uint8_t *p_data, int i_data )
{
    int i_count;
    int i_pos;

    if( i_data < 0 || p_data == NULL )
    {
        return VLC_SUCCESS;
    }
    vlc_mutex_lock( &stream->lock );

    /* save this pointer (to be used by new connection) */
    stream->i_buffer_last_pos = stream->i_buffer_pos;

    i_pos = stream->i_buffer_pos % stream->i_buffer_size;
    i_count = i_data;
    while( i_count > 0)
    {
        int i_copy;

        i_copy = __MIN( i_count, stream->i_buffer_size - i_pos );

        /* Ok, we can't go past the end of our buffer */
        memcpy( &stream->p_buffer[i_pos], p_data, i_copy );

        i_pos = ( i_pos + i_copy ) % stream->i_buffer_size;
        i_count -= i_copy;
        p_data  += i_copy;
    }

    stream->i_buffer_pos += i_data;

    vlc_mutex_unlock( &stream->lock );
    return VLC_SUCCESS;
}

void httpd_StreamDelete( httpd_stream_t *stream )
{
    httpd_UrlDelete( stream->url );
    vlc_mutex_destroy( &stream->lock );
    free( stream->psz_mime );
    free( stream->p_header );
    free( stream->p_buffer );
    free( stream );
}

/*****************************************************************************
 * Low level
 *****************************************************************************/
static void* httpd_HostThread( void * );
static httpd_host_t *httpd_HostCreate( vlc_object_t *, const char *,
                                       const char *, vlc_tls_creds_t * );

/* create a new host */
httpd_host_t *vlc_http_HostNew( vlc_object_t *p_this )
{
    return httpd_HostCreate( p_this, "http-host", "http-port", NULL );
}

httpd_host_t *vlc_https_HostNew( vlc_object_t *obj )
{
    char *cert = var_InheritString( obj, "http-cert" );
    if( cert == NULL )
    {
        msg_Err( obj, "HTTP/TLS certificate not specified!" );
        return NULL;
    }

    char *key = var_InheritString( obj, "http-key" );
    vlc_tls_creds_t *tls = vlc_tls_ServerCreate( obj, cert, key );

    if( tls == NULL )
    {
        msg_Err( obj, "HTTP/TLS certificate error (%s and %s)",
                 cert, (key != NULL) ? key : cert );
        free( key );
        free( cert );
        return NULL;
    }
    free( key );
    free( cert );

    char *ca = var_InheritString( obj, "http-ca" );
    if( ca != NULL )
    {
        if( vlc_tls_ServerAddCA( tls, ca ) )
        {
            msg_Err( obj, "HTTP/TLS CA error (%s)", ca );
            free( ca );
            goto error;
        }
        free( ca );
    }

    char *crl = var_InheritString( obj, "http-crl" );
    if( crl != NULL )
    {
        if( vlc_tls_ServerAddCRL( tls, crl ) )
        {
            msg_Err( obj, "TLS CRL error (%s)", crl );
            free( crl );
            goto error;
        }
        free( crl );
    }

    return httpd_HostCreate( obj, "http-host", "https-port", tls );

error:
    vlc_tls_Delete( tls );
    return NULL;
}

httpd_host_t *vlc_rtsp_HostNew( vlc_object_t *p_this )
{
    return httpd_HostCreate( p_this, "rtsp-host", "rtsp-port", NULL );
}

static struct httpd
{
    vlc_mutex_t  mutex;

    httpd_host_t **host;
    int          i_host;
} httpd = { VLC_STATIC_MUTEX, NULL, 0 };

static httpd_host_t *httpd_HostCreate( vlc_object_t *p_this,
                                       const char *hostvar,
                                       const char *portvar,
                                       vlc_tls_creds_t *p_tls )
{
    httpd_host_t *host;
    char *hostname = var_InheritString( p_this, hostvar );
    unsigned port = var_InheritInteger( p_this, portvar );

    vlc_url_t url;
    vlc_UrlParse( &url, hostname, 0 );
    free( hostname );
    if( url.i_port != 0 )
    {
        msg_Err( p_this, "Ignoring port %d (using %d)", url.i_port, port );
        msg_Info( p_this, "Specify port %d separately with the "
                          "%s option instead.", url.i_port, portvar );
    }

    /* to be sure to avoid multiple creation */
    vlc_mutex_lock( &httpd.mutex );

    /* verify if it already exist */
    for( int i = 0; i < httpd.i_host; i++ )
    {
        host = httpd.host[i];

        /* cannot mix TLS and non-TLS hosts */
        if( host->port != port
         || (host->p_tls != NULL) != (p_tls != NULL) )
            continue;

        /* Increase existing matching host reference count.
         * The reference count is written under both the global httpd and the
         * host lock. It is read with either or both locks held. The global
         * lock is always acquired first. */
        vlc_mutex_lock( &host->lock );
        host->i_ref++;
        vlc_mutex_unlock( &host->lock );

        vlc_mutex_unlock( &httpd.mutex );
        vlc_UrlClean( &url );
        vlc_tls_Delete( p_tls );
        return host;
    }

    /* create the new host */
    host = (httpd_host_t *)vlc_custom_create( p_this, sizeof (*host),
                                              "http host" );
    if (host == NULL)
        goto error;

    vlc_mutex_init( &host->lock );
    vlc_cond_init( &host->wait );
    host->i_ref = 1;

    host->fds = net_ListenTCP( p_this, url.psz_host, port );
    if( host->fds == NULL )
    {
        msg_Err( p_this, "cannot create socket(s) for HTTP host" );
        goto error;
    }
    for (host->nfd = 0; host->fds[host->nfd] != -1; host->nfd++);

    if( vlc_object_waitpipe( VLC_OBJECT( host ) ) == -1 )
    {
        msg_Err( host, "signaling pipe error: %m" );
        goto error;
    }

    host->port     = port;
    host->i_url    = 0;
    host->url      = NULL;
    host->i_client = 0;
    host->client   = NULL;
    host->p_tls    = p_tls;

    /* create the thread */
    if( vlc_clone( &host->thread, httpd_HostThread, host,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_this, "cannot spawn http host thread" );
        goto error;
    }

    /* now add it to httpd */
    TAB_APPEND( httpd.i_host, httpd.host, host );
    vlc_mutex_unlock( &httpd.mutex );

    vlc_UrlClean( &url );

    return host;

error:
    vlc_mutex_unlock( &httpd.mutex );

    if( host != NULL )
    {
        net_ListenClose( host->fds );
        vlc_cond_destroy( &host->wait );
        vlc_mutex_destroy( &host->lock );
        vlc_object_release( host );
    }

    vlc_UrlClean( &url );
    vlc_tls_Delete( p_tls );
    return NULL;
}

/* delete a host */
void httpd_HostDelete( httpd_host_t *host )
{
    int i;
    bool delete = false;

    vlc_mutex_lock( &httpd.mutex );

    vlc_mutex_lock( &host->lock );
    host->i_ref--;
    if( host->i_ref == 0 )
        delete = true;
    vlc_mutex_unlock( &host->lock );
    if( !delete )
    {
        /* still used */
        vlc_mutex_unlock( &httpd.mutex );
        msg_Dbg( host, "httpd_HostDelete: host still in use" );
        return;
    }
    TAB_REMOVE( httpd.i_host, httpd.host, host );

    vlc_cancel( host->thread );
    vlc_join( host->thread, NULL );

    msg_Dbg( host, "HTTP host removed" );

    for( i = 0; i < host->i_url; i++ )
    {
        msg_Err( host, "url still registered: %s", host->url[i]->psz_url );
    }
    for( i = 0; i < host->i_client; i++ )
    {
        httpd_client_t *cl = host->client[i];
        msg_Warn( host, "client still connected" );
        httpd_ClientClean( cl );
        TAB_REMOVE( host->i_client, host->client, cl );
        free( cl );
        i--;
        /* TODO */
    }

    vlc_tls_Delete( host->p_tls );
    net_ListenClose( host->fds );
    vlc_cond_destroy( &host->wait );
    vlc_mutex_destroy( &host->lock );
    vlc_object_release( host );
    vlc_mutex_unlock( &httpd.mutex );
}

/* register a new url */
httpd_url_t *httpd_UrlNew( httpd_host_t *host, const char *psz_url,
                           const char *psz_user, const char *psz_password )
{
    httpd_url_t *url;

    assert( psz_url != NULL );

    vlc_mutex_lock( &host->lock );
    for( int i = 0; i < host->i_url; i++ )
    {
        if( !strcmp( psz_url, host->url[i]->psz_url ) )
        {
            msg_Warn( host,
                      "cannot add '%s' (url already defined)", psz_url );
            vlc_mutex_unlock( &host->lock );
            return NULL;
        }
    }

    url = xmalloc( sizeof( httpd_url_t ) );
    url->host = host;

    vlc_mutex_init( &url->lock );
    url->psz_url = strdup( psz_url );
    url->psz_user = strdup( psz_user ? psz_user : "" );
    url->psz_password = strdup( psz_password ? psz_password : "" );
    for( int i = 0; i < HTTPD_MSG_MAX; i++ )
    {
        url->catch[i].cb = NULL;
        url->catch[i].p_sys = NULL;
    }

    TAB_APPEND( host->i_url, host->url, url );
    vlc_cond_signal( &host->wait );
    vlc_mutex_unlock( &host->lock );

    return url;
}

/* register callback on a url */
int httpd_UrlCatch( httpd_url_t *url, int i_msg, httpd_callback_t cb,
                    httpd_callback_sys_t *p_sys )
{
    vlc_mutex_lock( &url->lock );
    url->catch[i_msg].cb   = cb;
    url->catch[i_msg].p_sys= p_sys;
    vlc_mutex_unlock( &url->lock );

    return VLC_SUCCESS;
}

/* delete a url */
void httpd_UrlDelete( httpd_url_t *url )
{
    httpd_host_t *host = url->host;
    int          i;

    vlc_mutex_lock( &host->lock );
    TAB_REMOVE( host->i_url, host->url, url );

    vlc_mutex_destroy( &url->lock );
    free( url->psz_url );
    free( url->psz_user );
    free( url->psz_password );

    for( i = 0; i < host->i_client; i++ )
    {
        httpd_client_t *client = host->client[i];

        if( client->url == url )
        {
            /* TODO complete it */
            msg_Warn( host, "force closing connections" );
            httpd_ClientClean( client );
            TAB_REMOVE( host->i_client, host->client, client );
            free( client );
            i--;
        }
    }
    free( url );
    vlc_mutex_unlock( &host->lock );
}

static void httpd_MsgInit( httpd_message_t *msg )
{
    msg->cl         = NULL;
    msg->i_type     = HTTPD_MSG_NONE;
    msg->i_proto    = HTTPD_PROTO_NONE;
    msg->i_version  = -1; /* FIXME */

    msg->i_status   = 0;

    msg->psz_url    = NULL;
    msg->psz_args   = NULL;

    msg->i_name     = 0;
    msg->name       = NULL;
    msg->i_value    = 0;
    msg->value      = NULL;

    msg->i_body_offset = 0;
    msg->i_body        = 0;
    msg->p_body        = NULL;
}

static void httpd_MsgClean( httpd_message_t *msg )
{
    int i;

    free( msg->psz_url );
    free( msg->psz_args );
    for( i = 0; i < msg->i_name; i++ )
    {
        free( msg->name[i] );
        free( msg->value[i] );
    }
    free( msg->name );
    free( msg->value );
    free( msg->p_body );
    httpd_MsgInit( msg );
}

const char *httpd_MsgGet( const httpd_message_t *msg, const char *name )
{
    int i;

    for( i = 0; i < msg->i_name; i++ )
    {
        if( !strcasecmp( msg->name[i], name ))
        {
            return msg->value[i];
        }
    }
    return NULL;
}

void httpd_MsgAdd( httpd_message_t *msg, const char *name, const char *psz_value, ... )
{
    va_list args;
    char *value = NULL;

    va_start( args, psz_value );
    if( us_vasprintf( &value, psz_value, args ) == -1 )
        value = NULL;
    va_end( args );

    if( value == NULL )
        return;

    name = strdup( name );
    if( name == NULL )
    {
        free( value );
        return;
    }

    TAB_APPEND( msg->i_name,  msg->name,  (char*)name );
    TAB_APPEND( msg->i_value, msg->value, value );
}

static void httpd_ClientInit( httpd_client_t *cl, mtime_t now )
{
    cl->i_state = HTTPD_CLIENT_RECEIVING;
    cl->i_activity_date = now;
    cl->i_activity_timeout = INT64_C(10000000);
    cl->i_buffer_size = HTTPD_CL_BUFSIZE;
    cl->i_buffer = 0;
    cl->p_buffer = xmalloc( cl->i_buffer_size );
    cl->b_stream_mode = false;

    httpd_MsgInit( &cl->query );
    httpd_MsgInit( &cl->answer );
}

char* httpd_ClientIP( const httpd_client_t *cl, char *ip, int *port )
{
    return net_GetPeerAddress( cl->fd, ip, port ) ? NULL : ip;
}

char* httpd_ServerIP( const httpd_client_t *cl, char *ip, int *port )
{
    return net_GetSockAddress( cl->fd, ip, port ) ? NULL : ip;
}

static void httpd_ClientClean( httpd_client_t *cl )
{
    if( cl->fd >= 0 )
    {
        if( cl->p_tls != NULL )
            vlc_tls_SessionDelete( cl->p_tls );
        net_Close( cl->fd );
        cl->fd = -1;
    }

    httpd_MsgClean( &cl->answer );
    httpd_MsgClean( &cl->query );

    free( cl->p_buffer );
    cl->p_buffer = NULL;
}

static httpd_client_t *httpd_ClientNew( int fd, vlc_tls_t *p_tls, mtime_t now )
{
    httpd_client_t *cl = malloc( sizeof( httpd_client_t ) );

    if( !cl ) return NULL;

    cl->i_ref   = 0;
    cl->fd      = fd;
    cl->url     = NULL;
    cl->p_tls = p_tls;

    httpd_ClientInit( cl, now );
    if( p_tls != NULL )
        cl->i_state = HTTPD_CLIENT_TLS_HS_OUT;

    return cl;
}

static
ssize_t httpd_NetRecv (httpd_client_t *cl, uint8_t *p, size_t i_len)
{
    vlc_tls_t *p_tls;
    ssize_t val;

    p_tls = cl->p_tls;
    do
        val = p_tls ? tls_Recv (p_tls, p, i_len)
                    : recv (cl->fd, p, i_len, 0);
    while (val == -1 && errno == EINTR);
    return val;
}

static
ssize_t httpd_NetSend (httpd_client_t *cl, const uint8_t *p, size_t i_len)
{
    vlc_tls_t *p_tls;
    ssize_t val;

    p_tls = cl->p_tls;
    do
        val = p_tls ? tls_Send( p_tls, p, i_len )
                    : send (cl->fd, p, i_len, 0);
    while (val == -1 && errno == EINTR);
    return val;
}


static const struct
{
    const char name[16];
    int  i_type;
    int  i_proto;
}
msg_type[] =
{
    { "OPTIONS",       HTTPD_MSG_OPTIONS,      HTTPD_PROTO_RTSP },
    { "DESCRIBE",      HTTPD_MSG_DESCRIBE,     HTTPD_PROTO_RTSP },
    { "SETUP",         HTTPD_MSG_SETUP,        HTTPD_PROTO_RTSP },
    { "PLAY",          HTTPD_MSG_PLAY,         HTTPD_PROTO_RTSP },
    { "PAUSE",         HTTPD_MSG_PAUSE,        HTTPD_PROTO_RTSP },
    { "GET_PARAMETER", HTTPD_MSG_GETPARAMETER, HTTPD_PROTO_RTSP },
    { "TEARDOWN",      HTTPD_MSG_TEARDOWN,     HTTPD_PROTO_RTSP },
    { "GET",           HTTPD_MSG_GET,          HTTPD_PROTO_HTTP },
    { "HEAD",          HTTPD_MSG_HEAD,         HTTPD_PROTO_HTTP },
    { "POST",          HTTPD_MSG_POST,         HTTPD_PROTO_HTTP },
    { "",              HTTPD_MSG_NONE,         HTTPD_PROTO_NONE }
};


static void httpd_ClientRecv( httpd_client_t *cl )
{
    int i_len;

    /* ignore leading whites */
    if( ( cl->query.i_proto == HTTPD_PROTO_NONE ) &&
        ( cl->i_buffer == 0 ) )
    {
        unsigned char c;

        i_len = httpd_NetRecv( cl, &c, 1 );

        if( ( i_len > 0 ) && ( strchr( "\r\n\t ", c ) == NULL ) )
        {
            cl->p_buffer[0] = c;
            cl->i_buffer++;
        }
    }
    else
    if( cl->query.i_proto == HTTPD_PROTO_NONE )
    {
        /* enough to see if it's Interleaved RTP over RTSP or RTSP/HTTP */
        i_len = httpd_NetRecv( cl, &cl->p_buffer[cl->i_buffer],
                               7 - cl->i_buffer );
        if( i_len > 0 )
        {
            cl->i_buffer += i_len;
        }

        /* The smallest legal request is 7 bytes ("GET /\r\n"),
         * this is the maximum we can ask at this point. */
        if( cl->i_buffer >= 7 )
        {
            if( !memcmp( cl->p_buffer, "HTTP/1.", 7 ) )
            {
                cl->query.i_proto = HTTPD_PROTO_HTTP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            }
            else if( !memcmp( cl->p_buffer, "RTSP/1.", 7 ) )
            {
                cl->query.i_proto = HTTPD_PROTO_RTSP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            }
            else
            {
                /* We need the full request line to determine the protocol. */
                cl->query.i_proto = HTTPD_PROTO_HTTP0;
                cl->query.i_type  = HTTPD_MSG_NONE;
            }
        }
    }
    else if( cl->query.i_body > 0 )
    {
        /* we are reading the body of a request or a channel */
        i_len = httpd_NetRecv( cl, &cl->query.p_body[cl->i_buffer],
                               cl->query.i_body - cl->i_buffer );
        if( i_len > 0 )
        {
            cl->i_buffer += i_len;
        }
        if( cl->i_buffer >= cl->query.i_body )
        {
            cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
        }
    }
    else
    {
        /* we are reading a header -> char by char */
        for( ;; )
        {
            if( cl->i_buffer == cl->i_buffer_size )
            {
                uint8_t *newbuf = realloc( cl->p_buffer, cl->i_buffer_size + 1024 );
                if( newbuf == NULL )
                {
                    i_len = 0;
                    break;
                }

                cl->p_buffer = newbuf;
                cl->i_buffer_size += 1024;
            }

            i_len = httpd_NetRecv (cl, &cl->p_buffer[cl->i_buffer], 1 );
            if( i_len <= 0 )
            {
                break;
            }
            cl->i_buffer++;

            if( ( cl->query.i_proto == HTTPD_PROTO_HTTP0 )
             && ( cl->p_buffer[cl->i_buffer - 1] == '\n' ) )
            {
                /* Request line is now complete */
                const char *p = memchr( cl->p_buffer, ' ', cl->i_buffer );
                size_t len;

                assert( cl->query.i_type == HTTPD_MSG_NONE );

                if( p == NULL ) /* no URI: evil guy */
                {
                    i_len = 0; /* drop connection */
                    break;
                }

                do
                    p++; /* skips extra spaces */
                while( *p == ' ' );

                p = memchr( p, ' ', ((char *)cl->p_buffer) + cl->i_buffer - p );
                if( p == NULL ) /* no explicit protocol: HTTP/0.9 */
                {
                    i_len = 0; /* not supported currently -> drop */
                    break;
                }

                do
                    p++; /* skips extra spaces ever again */
                while( *p == ' ' );

                len = ((char *)cl->p_buffer) + cl->i_buffer - p;
                if( len < 7 ) /* foreign protocol */
                    i_len = 0; /* I don't understand -> drop */
                else
                if( memcmp( p, "HTTP/1.", 7 ) == 0 )
                {
                    cl->query.i_proto = HTTPD_PROTO_HTTP;
                    cl->query.i_version = atoi( p + 7 );
                }
                else
                if( memcmp( p, "RTSP/1.", 7 ) == 0 )
                {
                    cl->query.i_proto = HTTPD_PROTO_RTSP;
                    cl->query.i_version = atoi( p + 7 );
                }
                else
                if( memcmp( p, "HTTP/", 5 ) == 0 )
                {
                    const uint8_t sorry[] =
                       "HTTP/1.1 505 Unknown HTTP version\r\n\r\n";
                    httpd_NetSend( cl, sorry, sizeof( sorry ) - 1 );
                    i_len = 0; /* drop */
                }
                else
                if( memcmp( p, "RTSP/", 5 ) == 0 )
                {
                    const uint8_t sorry[] =
                        "RTSP/1.0 505 Unknown RTSP version\r\n\r\n";
                    httpd_NetSend( cl, sorry, sizeof( sorry ) - 1 );
                    i_len = 0; /* drop */
                }
                else /* yet another foreign protocol */
                    i_len = 0;

                if( i_len == 0 )
                    break;
            }

            if( ( cl->i_buffer >= 2 && !memcmp( &cl->p_buffer[cl->i_buffer-2], "\n\n", 2 ) )||
                ( cl->i_buffer >= 4 && !memcmp( &cl->p_buffer[cl->i_buffer-4], "\r\n\r\n", 4 ) ) )
            {
                char *p;

                /* we have finished the header so parse it and set i_body */
                cl->p_buffer[cl->i_buffer] = '\0';

                if( cl->query.i_type == HTTPD_MSG_ANSWER )
                {
                    /* FIXME:
                     * assume strlen( "HTTP/1.x" ) = 8
                     */
                    cl->query.i_status =
                        strtol( (char *)&cl->p_buffer[8],
                                &p, 0 );
                    while( *p == ' ' )
                        p++;
                }
                else
                {
                    unsigned i;

                    p = NULL;
                    cl->query.i_type = HTTPD_MSG_NONE;

                    /*fprintf( stderr, "received new request=%s\n", cl->p_buffer);*/

                    for( i = 0; msg_type[i].name[0]; i++ )
                    {
                        if( !strncmp( (char *)cl->p_buffer, msg_type[i].name,
                                      strlen( msg_type[i].name ) ) )
                        {
                            p = (char *)&cl->p_buffer[strlen(msg_type[i].name) + 1 ];
                            cl->query.i_type = msg_type[i].i_type;
                            if( cl->query.i_proto != msg_type[i].i_proto )
                            {
                                p = NULL;
                                cl->query.i_proto = HTTPD_PROTO_NONE;
                                cl->query.i_type = HTTPD_MSG_NONE;
                            }
                            break;
                        }
                    }
                    if( p == NULL )
                    {
                        if( strstr( (char *)cl->p_buffer, "HTTP/1." ) )
                        {
                            cl->query.i_proto = HTTPD_PROTO_HTTP;
                        }
                        else if( strstr( (char *)cl->p_buffer, "RTSP/1." ) )
                        {
                            cl->query.i_proto = HTTPD_PROTO_RTSP;
                        }
                    }
                    else
                    {
                        char *p2;
                        char *p3;

                        while( *p == ' ' )
                        {
                            p++;
                        }
                        p2 = strchr( p, ' ' );
                        if( p2 )
                        {
                            *p2++ = '\0';
                        }
                        if( !strncasecmp( p, ( cl->query.i_proto
                             == HTTPD_PROTO_HTTP ) ? "http:" : "rtsp:", 5 ) )
                        {   /* Skip hier-part of URL (if present) */
                            p += 5;
                            if( !strncmp( p, "//", 2 ) ) /* skip authority */
                            {   /* see RFC3986 §3.2 */
                                p += 2;
                                p += strcspn( p, "/?#" );
                            }
                        }
                        else
                        if( !strncasecmp( p, ( cl->query.i_proto
                             == HTTPD_PROTO_HTTP ) ? "https:" : "rtsps:", 6 ) )
                        {   /* Skip hier-part of URL (if present) */
                            p += 6;
                            if( !strncmp( p, "//", 2 ) ) /* skip authority */
                            {   /* see RFC3986 §3.2 */
                                p += 2;
                                p += strcspn( p, "/?#" );
                            }
                        }

                        cl->query.psz_url = strdup( p );
                        if( ( p3 = strchr( cl->query.psz_url, '?' ) )  )
                        {
                            *p3++ = '\0';
                            cl->query.psz_args = (uint8_t *)strdup( p3 );
                        }
                        p = p2;
                    }
                }
                if( p )
                {
                    p = strchr( p, '\n' );
                }
                if( p )
                {
                    while( *p == '\n' || *p == '\r' )
                    {
                        p++;
                    }
                    while( p && *p != '\0' )
                    {
                        char *line = p;
                        char *eol = p = strchr( p, '\n' );
                        char *colon;

                        while( eol && eol >= line && ( *eol == '\n' || *eol == '\r' ) )
                        {
                            *eol-- = '\0';
                        }

                        if( ( colon = strchr( line, ':' ) ) )
                        {
                            char *name;
                            char *value;

                            *colon++ = '\0';
                            while( *colon == ' ' )
                            {
                                colon++;
                            }
                            name = strdup( line );
                            value = strdup( colon );

                            TAB_APPEND( cl->query.i_name, cl->query.name, name );
                            TAB_APPEND( cl->query.i_value,cl->query.value,value);

                            if( !strcasecmp( name, "Content-Length" ) )
                            {
                                cl->query.i_body = atol( value );
                            }
                        }

                        if( p )
                        {
                            p++;
                            while( *p == '\n' || *p == '\r' )
                            {
                                p++;
                            }
                        }
                    }
                }
                if( cl->query.i_body > 0 )
                {
                    /* TODO Mhh, handle the case where the client only
                     * sends a request and closes the connection to
                     * mark the end of the body (probably only RTSP) */
                    cl->query.p_body = malloc( cl->query.i_body );
                    cl->i_buffer = 0;
                    if ( cl->query.p_body == NULL )
                    {
                        switch (cl->query.i_proto)
                        {
                            case HTTPD_PROTO_HTTP:
                            {
                                const uint8_t sorry[] =
                            "HTTP/1.1 413 Request Entity Too Large\r\n\r\n";
                                httpd_NetSend( cl, sorry, sizeof( sorry ) - 1 );
                                break;
                            }
                            case HTTPD_PROTO_RTSP:
                            {
                                const uint8_t sorry[] =
                            "RTSP/1.0 413 Request Entity Too Large\r\n\r\n";
                                httpd_NetSend( cl, sorry, sizeof( sorry ) - 1 );
                                break;
                            }
                            default:
                                assert( 0 );
                        }
                        i_len = 0; /* drop */
                    }
                    break;
                }
                else
                {
                    cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
                }
            }
        }
    }

    /* check if the client is to be set to dead */
#if defined( _WIN32 )
    if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
    if( ( i_len < 0 && errno != EAGAIN ) || ( i_len == 0 ) )
#endif
    {
        if( cl->query.i_proto != HTTPD_PROTO_NONE && cl->query.i_type != HTTPD_MSG_NONE )
        {
            /* connection closed -> end of data */
            if( cl->query.i_body > 0 )
            {
                cl->query.i_body = cl->i_buffer;
            }
            cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
        }
        else
        {
            cl->i_state = HTTPD_CLIENT_DEAD;
        }
    }

    /* XXX: for QT I have to disable timeout. Try to find why */
    if( cl->query.i_proto == HTTPD_PROTO_RTSP )
        cl->i_activity_timeout = 0;

#if 0 /* Debugging only */
    if( cl->i_state == HTTPD_CLIENT_RECEIVE_DONE )
    {
        int i;

        fprintf( stderr, "received new request\n" );
        fprintf( stderr, "  - proto=%s\n",
                 cl->query.i_proto == HTTPD_PROTO_HTTP ? "HTTP" : "RTSP" );
        fprintf( stderr, "  - version=%d\n", cl->query.i_version );
        fprintf( stderr, "  - msg=%d\n", cl->query.i_type );
        if( cl->query.i_type == HTTPD_MSG_ANSWER )
        {
            fprintf( stderr, "  - answer=%d '%s'\n", cl->query.i_status,
                     cl->query.psz_status );
        }
        else if( cl->query.i_type != HTTPD_MSG_NONE )
        {
            fprintf( stderr, "  - url=%s\n", cl->query.psz_url );
        }
        for( i = 0; i < cl->query.i_name; i++ )
        {
            fprintf( stderr, "  - option name='%s' value='%s'\n",
                     cl->query.name[i], cl->query.value[i] );
        }
    }
#endif
}

static void httpd_ClientSend( httpd_client_t *cl )
{
    int i;
    int i_len;

    if( cl->i_buffer < 0 )
    {
        /* We need to create the header */
        int i_size = 0;
        char *p;
        const char *psz_status = httpd_ReasonFromCode( cl->answer.i_status );

        i_size = strlen( "HTTP/1.") + 10 + 10 + strlen( psz_status ) + 5;
        for( i = 0; i < cl->answer.i_name; i++ )
        {
            i_size += strlen( cl->answer.name[i] ) + 2 +
                      strlen( cl->answer.value[i] ) + 2;
        }

        if( cl->i_buffer_size < i_size )
        {
            cl->i_buffer_size = i_size;
            free( cl->p_buffer );
            cl->p_buffer = xmalloc( i_size );
        }
        p = (char *)cl->p_buffer;

        p += sprintf( p, "%s.%u %d %s\r\n",
                      cl->answer.i_proto ==  HTTPD_PROTO_HTTP ? "HTTP/1" : "RTSP/1",
                      cl->answer.i_version,
                      cl->answer.i_status, psz_status );
        for( i = 0; i < cl->answer.i_name; i++ )
        {
            p += sprintf( p, "%s: %s\r\n", cl->answer.name[i],
                          cl->answer.value[i] );
        }
        p += sprintf( p, "\r\n" );

        cl->i_buffer = 0;
        cl->i_buffer_size = (uint8_t*)p - cl->p_buffer;

        /*fprintf( stderr, "sending answer\n" );
        fprintf( stderr, "%s",  cl->p_buffer );*/
    }

    i_len = httpd_NetSend( cl, &cl->p_buffer[cl->i_buffer],
                           cl->i_buffer_size - cl->i_buffer );
    if( i_len >= 0 )
    {
        cl->i_buffer += i_len;

        if( cl->i_buffer >= cl->i_buffer_size )
        {
            if( cl->answer.i_body == 0  && cl->answer.i_body_offset > 0 )
            {
                /* catch more body data */
                int     i_msg = cl->query.i_type;
                int64_t i_offset = cl->answer.i_body_offset;

                httpd_MsgClean( &cl->answer );
                cl->answer.i_body_offset = i_offset;

                cl->url->catch[i_msg].cb( cl->url->catch[i_msg].p_sys, cl,
                                          &cl->answer, &cl->query );
            }

            if( cl->answer.i_body > 0 )
            {
                /* send the body data */
                free( cl->p_buffer );
                cl->p_buffer = cl->answer.p_body;
                cl->i_buffer_size = cl->answer.i_body;
                cl->i_buffer = 0;

                cl->answer.i_body = 0;
                cl->answer.p_body = NULL;
            }
            else
            {
                /* send finished */
                cl->i_state = HTTPD_CLIENT_SEND_DONE;
            }
        }
    }
    else
    {
#if defined( _WIN32 )
        if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
        if( ( i_len < 0 && errno != EAGAIN ) || ( i_len == 0 ) )
#endif
        {
            /* error */
            cl->i_state = HTTPD_CLIENT_DEAD;
        }
    }
}

static void httpd_ClientTlsHandshake( httpd_client_t *cl )
{
    switch( vlc_tls_SessionHandshake( cl->p_tls, NULL, NULL ) )
    {
        case 0:
            cl->i_state = HTTPD_CLIENT_RECEIVING;
            break;

        case -1:
            cl->i_state = HTTPD_CLIENT_DEAD;
            break;

        case 1:
            cl->i_state = HTTPD_CLIENT_TLS_HS_IN;
            break;

        case 2:
            cl->i_state = HTTPD_CLIENT_TLS_HS_OUT;
            break;
    }
}

static void* httpd_HostThread( void *data )
{
    httpd_host_t *host = data;
    int canc = vlc_savecancel();

    vlc_mutex_lock( &host->lock );
    while( host->i_ref > 0 )
    {
        struct pollfd ufd[host->nfd + host->i_client];
        unsigned nfd;
        for( nfd = 0; nfd < host->nfd; nfd++ )
        {
            ufd[nfd].fd = host->fds[nfd];
            ufd[nfd].events = POLLIN;
            ufd[nfd].revents = 0;
        }

        /* add all socket that should be read/write and close dead connection */
        while( host->i_url <= 0 )
        {
            mutex_cleanup_push( &host->lock );
            vlc_restorecancel( canc );
            vlc_cond_wait( &host->wait, &host->lock );
            canc = vlc_savecancel();
            vlc_cleanup_pop();
        }

        mtime_t now = mdate();
        bool b_low_delay = false;

        for(int i_client = 0; i_client < host->i_client; i_client++ )
        {
            httpd_client_t *cl = host->client[i_client];
            if( cl->i_ref < 0 || ( cl->i_ref == 0 &&
                ( cl->i_state == HTTPD_CLIENT_DEAD ||
                  ( cl->i_activity_timeout > 0 &&
                    cl->i_activity_date+cl->i_activity_timeout < now) ) ) )
            {
                httpd_ClientClean( cl );
                TAB_REMOVE( host->i_client, host->client, cl );
                free( cl );
                i_client--;
                continue;
            }

            struct pollfd *pufd = ufd + nfd;
            assert (pufd < ufd + (sizeof (ufd) / sizeof (ufd[0])));

            pufd->fd = cl->fd;
            pufd->events = pufd->revents = 0;

            if( ( cl->i_state == HTTPD_CLIENT_RECEIVING )
                  || ( cl->i_state == HTTPD_CLIENT_TLS_HS_IN ) )
            {
                pufd->events = POLLIN;
            }
            else if( ( cl->i_state == HTTPD_CLIENT_SENDING )
                  || ( cl->i_state == HTTPD_CLIENT_TLS_HS_OUT ) )
            {
                pufd->events = POLLOUT;
            }
            else if( cl->i_state == HTTPD_CLIENT_RECEIVE_DONE )
            {
                httpd_message_t *answer = &cl->answer;
                httpd_message_t *query  = &cl->query;
                int i_msg = query->i_type;

                httpd_MsgInit( answer );

                /* Handle what we received */
                if( i_msg == HTTPD_MSG_ANSWER )
                {
                    cl->url     = NULL;
                    cl->i_state = HTTPD_CLIENT_DEAD;
                }
                else if( i_msg == HTTPD_MSG_OPTIONS )
                {

                    answer->i_type   = HTTPD_MSG_ANSWER;
                    answer->i_proto  = query->i_proto;
                    answer->i_status = 200;
                    answer->i_body = 0;
                    answer->p_body = NULL;

                    httpd_MsgAdd( answer, "Server", "VLC/%s", VERSION );
                    httpd_MsgAdd( answer, "Content-Length", "0" );

                    switch( query->i_proto )
                    {
                        case HTTPD_PROTO_HTTP:
                            answer->i_version = 1;
                            httpd_MsgAdd( answer, "Allow",
                                          "GET,HEAD,POST,OPTIONS" );
                            break;

                        case HTTPD_PROTO_RTSP:
                        {
                            const char *p;
                            answer->i_version = 0;

                            p = httpd_MsgGet( query, "Cseq" );
                            if( p != NULL )
                                httpd_MsgAdd( answer, "Cseq", "%s", p );
                            p = httpd_MsgGet( query, "Timestamp" );
                            if( p != NULL )
                                httpd_MsgAdd( answer, "Timestamp", "%s", p );

                            p = httpd_MsgGet( query, "Require" );
                            if( p != NULL )
                            {
                                answer->i_status = 551;
                                httpd_MsgAdd( query, "Unsupported", "%s", p );
                            }

                            httpd_MsgAdd( answer, "Public", "DESCRIBE,SETUP,"
                                          "TEARDOWN,PLAY,PAUSE,GET_PARAMETER" );
                            break;
                        }
                    }

                    cl->i_buffer = -1;  /* Force the creation of the answer in
                                         * httpd_ClientSend */
                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
                else if( i_msg == HTTPD_MSG_NONE )
                {
                    if( query->i_proto == HTTPD_PROTO_NONE )
                    {
                        cl->url = NULL;
                        cl->i_state = HTTPD_CLIENT_DEAD;
                    }
                    else
                    {
                        char *p;

                        /* unimplemented */
                        answer->i_proto  = query->i_proto ;
                        answer->i_type   = HTTPD_MSG_ANSWER;
                        answer->i_version= 0;
                        answer->i_status = 501;

                        answer->i_body = httpd_HtmlError (&p, 501, NULL);
                        answer->p_body = (uint8_t *)p;
                        httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

                        cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                        cl->i_state = HTTPD_CLIENT_SENDING;
                    }
                }
                else
                {
                    bool b_auth_failed = false;

                    /* Search the url and trigger callbacks */
                    for(int i = 0; i < host->i_url; i++ )
                    {
                        httpd_url_t *url = host->url[i];

                        if( !strcmp( url->psz_url, query->psz_url ) )
                        {
                            if( url->catch[i_msg].cb )
                            {
                                if( answer && ( *url->psz_user || *url->psz_password ) )
                                {
                                    /* create the headers */
                                    const char *b64 = httpd_MsgGet( query, "Authorization" ); /* BASIC id */
                                    char *user = NULL, *pass = NULL;

                                    if( b64 != NULL
                                     && !strncasecmp( b64, "BASIC", 5 ) )
                                    {
                                        b64 += 5;
                                        while( *b64 == ' ' )
                                            b64++;

                                        user = vlc_b64_decode( b64 );
                                        if (user != NULL)
                                        {
                                            pass = strchr (user, ':');
                                            if (pass != NULL)
                                                *pass++ = '\0';
                                        }
                                    }

                                    if ((user == NULL) || (pass == NULL)
                                     || strcmp (user, url->psz_user)
                                     || strcmp (pass, url->psz_password))
                                    {
                                        httpd_MsgAdd( answer,
                                                      "WWW-Authenticate",
                                                      "Basic realm=\"VLC stream\"" );
                                        /* We fail for all url */
                                        b_auth_failed = true;
                                        free( user );
                                        break;
                                    }

                                    free( user );
                                }

                                if( !url->catch[i_msg].cb( url->catch[i_msg].p_sys, cl, answer, query ) )
                                {
                                    if( answer->i_proto == HTTPD_PROTO_NONE )
                                    {
                                        /* Raw answer from a CGI */
                                        cl->i_buffer = cl->i_buffer_size;
                                    }
                                    else
                                        cl->i_buffer = -1;

                                    /* only one url can answer */
                                    answer = NULL;
                                    if( cl->url == NULL )
                                    {
                                        cl->url = url;
                                    }
                                }
                            }
                        }
                    }

                    if( answer )
                    {
                        char *p;

                        answer->i_proto  = query->i_proto;
                        answer->i_type   = HTTPD_MSG_ANSWER;
                        answer->i_version= 0;

                        if( b_auth_failed )
                        {
                            answer->i_status = 401;
                        }
                        else
                        {
                            /* no url registered */
                            answer->i_status = 404;
                        }

                        answer->i_body = httpd_HtmlError (&p,
                                                          answer->i_status,
                                                          query->psz_url);
                        answer->p_body = (uint8_t *)p;

                        cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                        httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
                        httpd_MsgAdd( answer, "Content-Type", "%s", "text/html" );
                    }

                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
            }
            else if( cl->i_state == HTTPD_CLIENT_SEND_DONE )
            {
                if( !cl->b_stream_mode || cl->answer.i_body_offset == 0 )
                {
                    const char *psz_connection = httpd_MsgGet( &cl->answer, "Connection" );
                    const char *psz_query = httpd_MsgGet( &cl->query, "Connection" );
                    bool b_connection = false;
                    bool b_keepalive = false;
                    bool b_query = false;

                    cl->url = NULL;
                    if( psz_connection )
                    {
                        b_connection = ( strcasecmp( psz_connection, "Close" ) == 0 );
                        b_keepalive = ( strcasecmp( psz_connection, "Keep-Alive" ) == 0 );
                    }

                    if( psz_query )
                    {
                        b_query = ( strcasecmp( psz_query, "Close" ) == 0 );
                    }

                    if( ( ( cl->query.i_proto == HTTPD_PROTO_HTTP ) &&
                          ( ( cl->query.i_version == 0 && b_keepalive ) ||
                            ( cl->query.i_version == 1 && !b_connection ) ) ) ||
                        ( ( cl->query.i_proto == HTTPD_PROTO_RTSP ) &&
                          !b_query && !b_connection ) )
                    {
                        httpd_MsgClean( &cl->query );
                        httpd_MsgInit( &cl->query );

                        cl->i_buffer = 0;
                        cl->i_buffer_size = 1000;
                        free( cl->p_buffer );
                        cl->p_buffer = xmalloc( cl->i_buffer_size );
                        cl->i_state = HTTPD_CLIENT_RECEIVING;
                    }
                    else
                    {
                        cl->i_state = HTTPD_CLIENT_DEAD;
                    }
                    httpd_MsgClean( &cl->answer );
                }
                else
                {
                    int64_t i_offset = cl->answer.i_body_offset;
                    httpd_MsgClean( &cl->answer );

                    cl->answer.i_body_offset = i_offset;
                    free( cl->p_buffer );
                    cl->p_buffer = NULL;
                    cl->i_buffer = 0;
                    cl->i_buffer_size = 0;

                    cl->i_state = HTTPD_CLIENT_WAITING;
                }
            }
            else if( cl->i_state == HTTPD_CLIENT_WAITING )
            {
                int64_t i_offset = cl->answer.i_body_offset;
                int     i_msg = cl->query.i_type;

                httpd_MsgInit( &cl->answer );
                cl->answer.i_body_offset = i_offset;

                cl->url->catch[i_msg].cb( cl->url->catch[i_msg].p_sys, cl,
                                          &cl->answer, &cl->query );
                if( cl->answer.i_type != HTTPD_MSG_NONE )
                {
                    /* we have new data, so re-enter send mode */
                    cl->i_buffer      = 0;
                    cl->p_buffer      = cl->answer.p_body;
                    cl->i_buffer_size = cl->answer.i_body;
                    cl->answer.p_body = NULL;
                    cl->answer.i_body = 0;
                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
            }

            if (pufd->events != 0)
                nfd++;
            else
                b_low_delay = true;
        }
        vlc_mutex_unlock( &host->lock );
        vlc_restorecancel( canc );

        /* we will wait 20ms (not too big) if HTTPD_CLIENT_WAITING */
        int ret = poll( ufd, nfd, b_low_delay ? 20 : -1 );

        canc = vlc_savecancel();
        vlc_mutex_lock( &host->lock );
        switch( ret )
        {
            case -1:
                if (errno != EINTR)
                {
                    /* Kernel on low memory or a bug: pace */
                    msg_Err( host, "polling error: %m" );
                    msleep( 100000 );
                }
            case 0:
                continue;
        }

        /* Handle client sockets */
        now = mdate();
        nfd = host->nfd;

        for( int i_client = 0; i_client < host->i_client; i_client++ )
        {
            httpd_client_t *cl = host->client[i_client];
            const struct pollfd *pufd = &ufd[nfd];

            assert( pufd < &ufd[sizeof(ufd) / sizeof(ufd[0])] );

            if( cl->fd != pufd->fd )
                continue; // we were not waiting for this client
            ++nfd;
            if( pufd->revents == 0 )
                continue; // no event received

            cl->i_activity_date = now;

            if( cl->i_state == HTTPD_CLIENT_RECEIVING )
            {
                httpd_ClientRecv( cl );
            }
            else if( cl->i_state == HTTPD_CLIENT_SENDING )
            {
                httpd_ClientSend( cl );
            }
            else if( cl->i_state == HTTPD_CLIENT_TLS_HS_IN
                  || cl->i_state == HTTPD_CLIENT_TLS_HS_OUT )
            {
                httpd_ClientTlsHandshake( cl );
            }
        }

        /* Handle server sockets (accept new connections) */
        for( nfd = 0; nfd < host->nfd; nfd++ )
        {
            httpd_client_t *cl;
            int fd = ufd[nfd].fd;

            assert (fd == host->fds[nfd]);

            if( ufd[nfd].revents == 0 )
                continue;

            /* */
            fd = vlc_accept (fd, NULL, NULL, true);
            if (fd == -1)
                continue;
            setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
                        &(int){ 1 }, sizeof(int));

            vlc_tls_t *p_tls;

            if( host->p_tls != NULL )
                p_tls = vlc_tls_SessionCreate( host->p_tls, fd, NULL );
            else
                p_tls = NULL;

            cl = httpd_ClientNew( fd, p_tls, now );

            TAB_APPEND( host->i_client, host->client, cl );
        }
    }
    vlc_mutex_unlock( &host->lock );
    return NULL;
}
