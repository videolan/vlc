/*****************************************************************************
 * httpd.c
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdlib.h>
#include <vlc/vlc.h>

#ifdef ENABLE_HTTPD

#include <assert.h>

#include "vlc_httpd.h"
#include "network.h"
#include "vlc_tls.h"
#include "vlc_acl.h"

#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

#if defined( WIN32 )
/* We need HUGE buffer otherwise TCP throughput is very limited */
#define HTTPD_CL_BUFSIZE 1000000
#else
#define HTTPD_CL_BUFSIZE 10000
#endif

static void httpd_ClientClean( httpd_client_t *cl );

struct httpd_url_t
{
    httpd_host_t *host;

    vlc_mutex_t lock;

    char      *psz_url;
    char      *psz_user;
    char      *psz_password;
    vlc_acl_t *p_acl;

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
    HTTPD_CLIENT_BIDIR,     /* check for reading and get data from cb */
};

struct httpd_client_t
{
    httpd_url_t *url;

    int     i_ref;

    struct  sockaddr_storage sock;
    int     i_sock_size;
    int     fd;

    int     i_mode;
    int     i_state;
    int     b_read_waiting; /* stop as soon as possible sending */

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
    tls_session_t *p_tls;
};


/*****************************************************************************
 * Various functions
 *****************************************************************************/
/*char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";*/
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

static struct
{
    const char *psz_ext;
    const char *psz_mime;
} http_mime[] =
{
    { ".htm",   "text/html" },
    { ".html",  "text/html" },
    { ".txt",   "text/plain" },
    { ".xml",   "text/xml" },
    { ".dtd",   "text/dtd" },

    { ".css",   "text/css" },

    /* image mime */
    { ".gif",   "image/gif" },
    { ".jpe",   "image/jpeg" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".png",   "image/png" },
    { ".mpjpeg","multipart/x-mixed-replace; boundary=This Random String" },

    /* media mime */
    { ".avi",   "video/avi" },
    { ".asf",   "video/x-ms-asf" },
    { ".m1a",   "audio/mpeg" },
    { ".m2a",   "audio/mpeg" },
    { ".m1v",   "video/mpeg" },
    { ".m2v",   "video/mpeg" },
    { ".mp2",   "audio/mpeg" },
    { ".mp3",   "audio/mpeg" },
    { ".mpa",   "audio/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".mpeg",  "video/mpeg" },
    { ".mpe",   "video/mpeg" },
    { ".mov",   "video/quicktime" },
    { ".moov",  "video/quicktime" },
    { ".ogg",   "application/ogg" },
    { ".ogm",   "application/ogg" },
    { ".wav",   "audio/wav" },
    { ".wma",   "audio/x-ms-wma" },
    { ".wmv",   "video/x-ms-wmv" },


    /* end */
    { NULL,     NULL }
};

static const char *httpd_MimeFromUrl( const char *psz_url )
{

    char *psz_ext;

    psz_ext = strrchr( psz_url, '.' );
    if( psz_ext )
    {
        int i;

        for( i = 0; http_mime[i].psz_ext != NULL ; i++ )
        {
            if( !strcasecmp( http_mime[i].psz_ext, psz_ext ) )
            {
                return http_mime[i].psz_mime;
            }
        }
    }
    return "application/octet-stream";
}

#if 0
typedef struct
{
    int i_code;
    const char *psz_reason;
} http_status_info;

static const http_status_info http_reason[] =
{
  /*{ 100, "Continue" },
    { 101, "Switching Protocols" },*/
    { 200, "OK" }/*,
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 250, "Low on Storage Space" },
    { 300, "Multiple Choices" }*/,
    { 301, "Moved Permanently" }/*,
    { 302, "Moved Temporarily" }, - aka "Found"
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 307, "Temporary Redirect" },
    { 400, "Bad Request" }*/,
    { 401, "Unauthorized" }/*,
    { 402, "Payment Required" }*/,
    { 403, "Forbidden" },
    { 404, "Not Found" }/*,
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Time-out" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Request Entity Too Large" },
    { 414, "Request-URI Too Large" },
    { 415, "Unsupported Media Type" },
    { 416, "Requested range not satisfiable" },
    { 417, "Expectation Failed" },
    { 451, "Parameter Not Understood" },
    { 452, "Conference Not Found" },
    { 453, "Not Enough Bandwidth" }*/,
    { 454, "Session Not Found" }/*,
    { 455, "Method Not Valid in This State" },
    { 456, "Header Field Not Valid for Resource" },
    { 457, "Invalid Range" },
    { 458, "Parameter Is Read-Only" },
    { 459, "Aggregate operation not allowed" },
    { 460, "Only aggregate operation allowed" }*/,
    { 461, "Unsupported transport" }/*,
    { 462, "Destination unreachable" }*/,
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" }/*,
    { 502, "Bad Gateway" }*/,
    { 503, "Service Unavailable" }/*,
    { 504, "Gateway Time-out" },
    { 505, "Protocol version not supported" }*/,
    {   0, NULL }
};

static const char *psz_fallback_reason[] =
{ "Continue", "OK", "Found", "Client Error", "Server Error" };

static const char *httpd_ReasonFromCode( int i_code )
{
    const http_status_info *p;

    for (p = http_reason; p->i_code < i_code; p++);

    if( p->i_code == i_code )
        return p->psz_reason;

    assert( ( i_code >= 100 ) && ( i_code <= 599 ) );
    return psz_fallback_reason[(i_code / 100) - 1];
}
#endif

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

static int httpd_FileCallBack( httpd_callback_sys_t *p_sys, httpd_client_t *cl, httpd_message_t *answer, httpd_message_t *query )
{
    httpd_file_t *file = (httpd_file_t*)p_sys;
    uint8_t *psz_args = query->psz_args;
    uint8_t **pp_body, *p_body;
    char *psz_connection = NULL;
    int *pi_body, i_body;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;

    answer->i_status = 200;
    answer->psz_status = strdup( "OK" );

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

    file->pf_fill( file->p_sys, file, psz_args, pp_body, pi_body );

    if( query->i_type == HTTPD_MSG_HEAD && p_body != NULL )
    {
        free( p_body );
    }

    /* We respect client request */
    psz_connection = httpd_MsgGet( &cl->query, "Connection" );
    if( psz_connection != NULL )
    {
        httpd_MsgAdd( answer, "Connection", psz_connection );
    }

    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

    return VLC_SUCCESS;
}

httpd_file_t *httpd_FileNew( httpd_host_t *host,
                             const char *psz_url, const char *psz_mime,
                             const char *psz_user, const char *psz_password,
                             const vlc_acl_t *p_acl, httpd_file_callback_t pf_fill,
                             httpd_file_sys_t *p_sys )
{
    httpd_file_t *file = malloc( sizeof( httpd_file_t ) );

    if( ( file->url = httpd_UrlNewUnique( host, psz_url, psz_user,
                                          psz_password, p_acl )
        ) == NULL )
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
        file->psz_mime = strdup( httpd_MimeFromUrl( psz_url ) );
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

void httpd_FileDelete( httpd_file_t *file )
{
    httpd_UrlDelete( file->url );

    free( file->psz_url );
    free( file->psz_mime );

    free( file );
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

static int httpd_HandlerCallBack( httpd_callback_sys_t *p_sys, httpd_client_t *cl, httpd_message_t *answer, httpd_message_t *query )
{
    httpd_handler_t *handler = (httpd_handler_t*)p_sys;
    uint8_t *psz_args = query->psz_args;
    char psz_remote_addr[NI_MAXNUMERICHOST];

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = HTTPD_PROTO_NONE;
    answer->i_type   = HTTPD_MSG_ANSWER;

    /* We do it ourselves, thanks */
    answer->i_status = 0;
    answer->psz_status = NULL;

    if( httpd_ClientIP( cl, psz_remote_addr ) == NULL )
        *psz_remote_addr = '\0';

    handler->pf_fill( handler->p_sys, handler, query->psz_url, psz_args,
                      query->i_type, query->p_body, query->i_body,
                      psz_remote_addr, NULL,
                      &answer->p_body, &answer->i_body );

    if( query->i_type == HTTPD_MSG_HEAD )
    {
        char *p = (char *)answer->p_body;
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
            answer->p_body = realloc( answer->p_body, answer->i_body );
        }
    }

    if( strncmp( (char *)answer->p_body, "HTTP/1.", 7 ) )
    {
        int i_status, i_headers;
        char *psz_headers, *psz_new, *psz_status;

        if( !strncmp( (char *)answer->p_body, "Status: ", 8 ) )
        {
            /* Apache-style */
            i_status = strtol( (char *)&answer->p_body[8], &psz_headers, 0 );
            if( *psz_headers ) psz_headers++;
            if( *psz_headers ) psz_headers++;
            i_headers = answer->i_body - (psz_headers - (char *)answer->p_body);
        }
        else
        {
            i_status = 200;
            psz_headers = (char *)answer->p_body;
            i_headers = answer->i_body;
        }
        switch( i_status )
        {
        case 200:
            psz_status = "OK";
            break;
        case 401:
            psz_status = "Unauthorized";
            break;
        default:
            if( (i_status < 0) || (i_status > 999) )
                i_status = 500;
            psz_status = "Undefined";
            break;
        }
        answer->i_body = sizeof("HTTP/1.0 xxx \r\n")
                        + strlen(psz_status) + i_headers - 1;
        psz_new = (char *)malloc( answer->i_body + 1);
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
                                   const vlc_acl_t *p_acl,
                                   httpd_handler_callback_t pf_fill,
                                   httpd_handler_sys_t *p_sys )
{
    httpd_handler_t *handler = malloc( sizeof( httpd_handler_t ) );

    if( ( handler->url = httpd_UrlNewUnique( host, psz_url, psz_user,
                                             psz_password, p_acl )
        ) == NULL )
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

void httpd_HandlerDelete( httpd_handler_t *handler )
{
    httpd_UrlDelete( handler->url );
    free( handler );
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
                                   httpd_message_t *query )
{
    httpd_redirect_t *rdir = (httpd_redirect_t*)p_sys;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    answer->i_proto  = query->i_proto;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_status = 301;
    answer->psz_status = strdup( "Moved Permanently" );

    answer->i_body = asprintf( (char **)&answer->p_body,
        "<?xml version=\"1.0\" encoding=\"ascii\" ?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD  XHTML 1.0 Strict//EN\" "
        "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
        "<html>\n"
        "<head>\n"
        "<title>Redirection</title>\n"
        "</head>\n"
        "<body>\n"
        "<h1>You should be "
        "<a href=\"%s\">redirected</a></h1>\n"
        "<hr />\n"
        "<p><a href=\"http://www.videolan.org\">VideoLAN</a>\n</p>"
        "<hr />\n"
        "</body>\n"
        "</html>\n", rdir->psz_dst );

    /* XXX check if it's ok or we need to set an absolute url */
    httpd_MsgAdd( answer, "Location",  "%s", rdir->psz_dst );

    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

    return VLC_SUCCESS;
}

httpd_redirect_t *httpd_RedirectNew( httpd_host_t *host, const char *psz_url_dst,
                                     const char *psz_url_src )
{
    httpd_redirect_t *rdir = malloc( sizeof( httpd_redirect_t ) );

    if( !( rdir->url = httpd_UrlNewUnique( host, psz_url_src, NULL, NULL, NULL ) ) )
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
                                 httpd_message_t *query )
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
        answer->p_body = malloc( i_write );
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
        answer->psz_status = strdup( "OK" );

        if( query->i_type != HTTPD_MSG_HEAD )
        {
            httpd_ClientModeStream( cl );
            vlc_mutex_lock( &stream->lock );
            /* Send the header */
            if( stream->i_header > 0 )
            {
                answer->i_body = stream->i_header;
                answer->p_body = malloc( stream->i_header );
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
            vlc_bool_t b_xplaystream = VLC_FALSE;
            int i;

            httpd_MsgAdd( answer, "Content-type", "%s",
                          "application/octet-stream" );
            httpd_MsgAdd( answer, "Server", "Cougar 4.1.0.3921" );
            httpd_MsgAdd( answer, "Pragma", "no-cache" );
            httpd_MsgAdd( answer, "Pragma", "client-id=%d", rand()&0x7fff );
            httpd_MsgAdd( answer, "Pragma", "features=\"broadcast\"" );

            /* Check if there is a xPlayStrm=1 */
            for( i = 0; i < query->i_name; i++ )
            {
                if( !strcasecmp( query->name[i],  "Pragma" ) &&
                    strstr( query->value[i], "xPlayStrm=1" ) )
                {
                    b_xplaystream = VLC_TRUE;
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
                                 const char *psz_user, const char *psz_password,
                                 const vlc_acl_t *p_acl )
{
    httpd_stream_t *stream = malloc( sizeof( httpd_stream_t ) );

    if( ( stream->url = httpd_UrlNewUnique( host, psz_url, psz_user,
                                            psz_password, p_acl )
        ) == NULL )
    {
        free( stream );
        return NULL;
    }
    vlc_mutex_init( host, &stream->lock );
    if( psz_mime && *psz_mime )
    {
        stream->psz_mime = strdup( psz_mime );
    }
    else
    {
        stream->psz_mime = strdup( httpd_MimeFromUrl( psz_url ) );
    }
    stream->i_header = 0;
    stream->p_header = NULL;
    stream->i_buffer_size = 5000000;    /* 5 Mo per stream */
    stream->p_buffer = malloc( stream->i_buffer_size );
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
    if( stream->p_header )
    {
        free( stream->p_header );
        stream->p_header = NULL;
    }
    stream->i_header = i_data;
    if( i_data > 0 )
    {
        stream->p_header = malloc( i_data );
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
    if( stream->psz_mime ) free( stream->psz_mime );
    if( stream->p_header ) free( stream->p_header );
    if( stream->p_buffer ) free( stream->p_buffer );
    free( stream );
}

/*****************************************************************************
 * Low level
 *****************************************************************************/
static void httpd_HostThread( httpd_host_t * );

/* create a new host */
httpd_host_t *httpd_HostNew( vlc_object_t *p_this, const char *psz_host,
                             int i_port )
{
    return httpd_TLSHostNew( p_this, psz_host, i_port, NULL, NULL, NULL, NULL
                           );
}

httpd_host_t *httpd_TLSHostNew( vlc_object_t *p_this, const char *psz_hostname,
                                int i_port,
                                const char *psz_cert, const char *psz_key,
                                const char *psz_ca, const char *psz_crl )
{
    httpd_t      *httpd;
    httpd_host_t *host;
    tls_server_t *p_tls;
    char *psz_host;
    vlc_value_t  lockval;
    int i;

    if( psz_hostname == NULL )
        psz_hostname = "";

    psz_host = strdup( psz_hostname );
    if( psz_host == NULL )
    {
        msg_Err( p_this, "memory error" );
        return NULL;
    }

    /* to be sure to avoid multiple creation */
    var_Create( p_this->p_libvlc, "httpd_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "httpd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    if( !(httpd = vlc_object_find( p_this, VLC_OBJECT_HTTPD, FIND_ANYWHERE )) )
    {
        msg_Info( p_this, "creating httpd" );
        if( ( httpd = vlc_object_create( p_this, VLC_OBJECT_HTTPD ) ) == NULL )
        {
            vlc_mutex_unlock( lockval.p_address );
            free( psz_host );
            return NULL;
        }

        httpd->i_host = 0;
        httpd->host   = NULL;

        vlc_object_yield( httpd );
        vlc_object_attach( httpd, p_this->p_vlc );
    }

    /* verify if it already exist */
    for( i = httpd->i_host - 1; i >= 0; i-- )
    {
        host = httpd->host[i];

        /* cannot mix TLS and non-TLS hosts */
        if( ( ( httpd->host[i]->p_tls != NULL ) != ( psz_cert != NULL ) )
         || ( host->i_port != i_port )
         || strcmp( host->psz_hostname, psz_hostname ) )
            continue;

        /* yep found */
        host->i_ref++;

        vlc_mutex_unlock( lockval.p_address );
        return host;
    }

    host = NULL;

    /* determine TLS configuration */
    if ( psz_cert != NULL )
    {
        p_tls = tls_ServerCreate( p_this, psz_cert, psz_key );
        if ( p_tls == NULL )
        {
            msg_Err( p_this, "TLS initialization error" );
            goto error;
        }

        if ( ( psz_ca != NULL) && tls_ServerAddCA( p_tls, psz_ca ) )
        {
            msg_Err( p_this, "TLS CA error" );
            goto error;
        }

        if ( ( psz_crl != NULL) && tls_ServerAddCRL( p_tls, psz_crl ) )
        {
            msg_Err( p_this, "TLS CRL error" );
            goto error;
        }
    }
    else
        p_tls = NULL;

    /* create the new host */
    host = vlc_object_create( p_this, VLC_OBJECT_HTTPD_HOST );
    host->httpd = httpd;
    vlc_mutex_init( httpd, &host->lock );
    host->i_ref = 1;

    host->fd = net_ListenTCP( p_this, psz_host, i_port );
    if( host->fd == NULL )
    {
        msg_Err( p_this, "cannot create socket(s) for HTTP host" );
        goto error;
    }

    host->i_port = i_port;
    host->psz_hostname = psz_host;

    host->i_url     = 0;
    host->url       = NULL;
    host->i_client  = 0;
    host->client    = NULL;

    host->p_tls = p_tls;

    /* create the thread */
    if( vlc_thread_create( host, "httpd host thread", httpd_HostThread,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_this, "cannot spawn http host thread" );
        goto error;
    }

    /* now add it to httpd */
    TAB_APPEND( httpd->i_host, httpd->host, host );
    vlc_mutex_unlock( lockval.p_address );

    return host;

error:
    free( psz_host );
    if( httpd->i_host <= 0 )
    {
        vlc_object_release( httpd );
        vlc_object_detach( httpd );
        vlc_object_destroy( httpd );
    }
    vlc_mutex_unlock( lockval.p_address );

    if( host != NULL )
    {
        net_ListenClose( host->fd );
        vlc_mutex_destroy( &host->lock );
        vlc_object_destroy( host );
    }

    if( p_tls != NULL )
        tls_ServerDelete( p_tls );

    return NULL;
}

/* delete a host */
void httpd_HostDelete( httpd_host_t *host )
{
    httpd_t *httpd = host->httpd;
    vlc_value_t lockval;
    int i;

    var_Get( httpd->p_libvlc, "httpd_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    host->i_ref--;
    if( host->i_ref > 0 )
    {
        /* still used */
        vlc_mutex_unlock( lockval.p_address );
        msg_Dbg( host, "httpd_HostDelete: host still used" );
        return;
    }
    TAB_REMOVE( httpd->i_host, httpd->host, host );

    host->b_die = 1;
    vlc_thread_join( host );

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

    if( host->p_tls != NULL)
        tls_ServerDelete( host->p_tls );

    net_ListenClose( host->fd );
    free( host->psz_hostname );

    vlc_mutex_destroy( &host->lock );
    vlc_object_destroy( host );

    vlc_object_release( httpd );
    if( httpd->i_host <= 0 )
    {
        msg_Dbg( httpd, "no host left, stopping httpd" );
        vlc_object_detach( httpd );
        vlc_object_destroy( httpd );
    }
    vlc_mutex_unlock( lockval.p_address );
}

/* register a new url */
static httpd_url_t *httpd_UrlNewPrivate( httpd_host_t *host, const char *psz_url,
                                         const char *psz_user, const char *psz_password,
                                         const vlc_acl_t *p_acl, vlc_bool_t b_check )
{
    httpd_url_t *url;
    int         i;

    vlc_mutex_lock( &host->lock );
    if( b_check )
    {
        for( i = 0; i < host->i_url; i++ )
        {
            if( !strcmp( psz_url, host->url[i]->psz_url ) )
            {
                msg_Warn( host->httpd,
                          "cannot add '%s' (url already defined)", psz_url );
                vlc_mutex_unlock( &host->lock );
                return NULL;
            }
        }
    }

    url = malloc( sizeof( httpd_url_t ) );
    url->host = host;

    vlc_mutex_init( host->httpd, &url->lock );
    url->psz_url = strdup( psz_url );
    url->psz_user = strdup( psz_user ? psz_user : "" );
    url->psz_password = strdup( psz_password ? psz_password : "" );
    url->p_acl = ACL_Duplicate( host, p_acl );
    for( i = 0; i < HTTPD_MSG_MAX; i++ )
    {
        url->catch[i].cb = NULL;
        url->catch[i].p_sys = NULL;
    }

    TAB_APPEND( host->i_url, host->url, url );
    vlc_mutex_unlock( &host->lock );

    return url;
}

httpd_url_t *httpd_UrlNew( httpd_host_t *host, const char *psz_url,
                           const char *psz_user, const char *psz_password,
                           const vlc_acl_t *p_acl )
{
    return httpd_UrlNewPrivate( host, psz_url, psz_user,
                                psz_password, p_acl, VLC_FALSE );
}

httpd_url_t *httpd_UrlNewUnique( httpd_host_t *host, const char *psz_url,
                                 const char *psz_user, const char *psz_password,
                                 const vlc_acl_t *p_acl )
{
    return httpd_UrlNewPrivate( host, psz_url, psz_user,
                                psz_password, p_acl, VLC_TRUE );
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

/* delete an url */
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
    ACL_Destroy( url->p_acl );

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

void httpd_MsgInit( httpd_message_t *msg )
{
    msg->cl         = NULL;
    msg->i_type     = HTTPD_MSG_NONE;
    msg->i_proto    = HTTPD_PROTO_NONE;
    msg->i_version  = -1;

    msg->i_status   = 0;
    msg->psz_status = NULL;

    msg->psz_url    = NULL;
    msg->psz_args   = NULL;

    msg->i_channel  = -1;

    msg->i_name     = 0;
    msg->name       = NULL;
    msg->i_value    = 0;
    msg->value      = NULL;

    msg->i_body_offset = 0;
    msg->i_body        = 0;
    msg->p_body        = 0;
}

void httpd_MsgClean( httpd_message_t *msg )
{
    int i;

    if( msg->psz_status )
    {
        free( msg->psz_status );
    }
    if( msg->psz_url )
    {
        free( msg->psz_url );
    }
    if( msg->psz_args )
    {
        free( msg->psz_args );
    }
    for( i = 0; i < msg->i_name; i++ )
    {
        free( msg->name[i] );
        free( msg->value[i] );
    }
    if( msg->name )
    {
        free( msg->name );
    }
    if( msg->value )
    {
        free( msg->value );
    }
    if( msg->p_body )
    {
        free( msg->p_body );
    }
    httpd_MsgInit( msg );
}

char *httpd_MsgGet( httpd_message_t *msg, char *name )
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

void httpd_MsgAdd( httpd_message_t *msg, char *name, char *psz_value, ... )
{
    va_list args;
    char *value = NULL;

    va_start( args, psz_value );
#if defined(HAVE_VASPRINTF) && !defined(__APPLE__) && !defined(SYS_BEOS)
    vasprintf( &value, psz_value, args );
#else
    {
        int i_size = strlen( psz_value ) + 4096;    /* FIXME stupid system */
        value = calloc( i_size, sizeof( char ) );
        vsnprintf( value, i_size, psz_value, args );
        value[i_size - 1] = 0;
    }
#endif
    va_end( args );

    name = strdup( name );

    TAB_APPEND( msg->i_name,  msg->name,  name );
    TAB_APPEND( msg->i_value, msg->value, value );
}

static void httpd_ClientInit( httpd_client_t *cl )
{
    cl->i_state = HTTPD_CLIENT_RECEIVING;
    cl->i_activity_date = mdate();
    cl->i_activity_timeout = I64C(10000000);
    cl->i_buffer_size = HTTPD_CL_BUFSIZE;
    cl->i_buffer = 0;
    cl->p_buffer = malloc( cl->i_buffer_size );
    cl->i_mode   = HTTPD_CLIENT_FILE;
    cl->b_read_waiting = VLC_FALSE;

    httpd_MsgInit( &cl->query );
    httpd_MsgInit( &cl->answer );
}

void httpd_ClientModeStream( httpd_client_t *cl )
{
    cl->i_mode   = HTTPD_CLIENT_STREAM;
}

void httpd_ClientModeBidir( httpd_client_t *cl )
{
    cl->i_mode   = HTTPD_CLIENT_BIDIR;
}

char* httpd_ClientIP( const httpd_client_t *cl, char *psz_ip )
{
    return net_GetPeerAddress( cl->fd, psz_ip, NULL ) ? NULL : psz_ip;
}

char* httpd_ServerIP( const httpd_client_t *cl, char *psz_ip )
{
    return net_GetSockAddress( cl->fd, psz_ip, NULL ) ? NULL : psz_ip;
}

static void httpd_ClientClean( httpd_client_t *cl )
{
    if( cl->fd >= 0 )
    {
        if( cl->p_tls != NULL )
            tls_ServerSessionClose( cl->p_tls );
        net_Close( cl->fd );
        cl->fd = -1;
    }

    httpd_MsgClean( &cl->answer );
    httpd_MsgClean( &cl->query );

    if( cl->p_buffer )
    {
        free( cl->p_buffer );
        cl->p_buffer = NULL;
    }
}

static httpd_client_t *httpd_ClientNew( int fd, struct sockaddr_storage *sock,
                                        int i_sock_size,
                                        tls_session_t *p_tls )
{
    httpd_client_t *cl = malloc( sizeof( httpd_client_t ) );

    if( !cl ) return NULL;

    cl->i_ref   = 0;
    cl->fd      = fd;
    memcpy( &cl->sock, sock, sizeof( cl->sock ) );
    cl->i_sock_size = i_sock_size;
    cl->url     = NULL;
    cl->p_tls = p_tls;

    httpd_ClientInit( cl );

    return cl;
}

static int httpd_NetRecv( httpd_client_t *cl, uint8_t *p, int i_len )
{
    tls_session_t *p_tls;

    p_tls = cl->p_tls;
    if( p_tls != NULL)
        return tls_Recv( p_tls, p, i_len );

    return recv( cl->fd, p, i_len, 0 );
}

static int httpd_NetSend( httpd_client_t *cl, const uint8_t *p, int i_len )
{
    tls_session_t *p_tls;

    p_tls = cl->p_tls;
    if( p_tls != NULL)
        return tls_Send( p_tls, p, i_len );

    return send( cl->fd, p, i_len, 0 );
}

static void httpd_ClientRecv( httpd_client_t *cl )
{
    int i_len;

    if( cl->query.i_proto == HTTPD_PROTO_NONE )
    {
        /* enough to see if it's rtp over rtsp or RTSP/HTTP */
        i_len = httpd_NetRecv( cl, &cl->p_buffer[cl->i_buffer],
                               4 - cl->i_buffer );
        if( i_len > 0 )
        {
            cl->i_buffer += i_len;
        }

        if( cl->i_buffer >= 4 )
        {
            /*fprintf( stderr, "peek=%4.4s\n", cl->p_buffer );*/
            /* detect type */
            if( cl->p_buffer[0] == '$' )
            {
                /* RTSP (rtp over rtsp) */
                cl->query.i_proto = HTTPD_PROTO_RTSP;
                cl->query.i_type  = HTTPD_MSG_CHANNEL;
                cl->query.i_channel = cl->p_buffer[1];
                cl->query.i_body  = (cl->p_buffer[2] << 8)|cl->p_buffer[3];
                cl->query.p_body  = malloc( cl->query.i_body );

                cl->i_buffer      = 0;
            }
            else if( !memcmp( cl->p_buffer, "HTTP", 4 ) )
            {
                cl->query.i_proto = HTTPD_PROTO_HTTP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            }
            else if( !memcmp( cl->p_buffer, "RTSP", 4 ) )
            {
                cl->query.i_proto = HTTPD_PROTO_RTSP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            }
            else if( !memcmp( cl->p_buffer, "GET", 3 ) ||
                     !memcmp( cl->p_buffer, "HEAD", 4 ) ||
                     !memcmp( cl->p_buffer, "POST", 4 ) )
            {
                cl->query.i_proto = HTTPD_PROTO_HTTP;
                cl->query.i_type  = HTTPD_MSG_NONE;
            }
            else
            {
                cl->query.i_proto = HTTPD_PROTO_RTSP;
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
            i_len = httpd_NetRecv (cl, &cl->p_buffer[cl->i_buffer], 1 );
            if( i_len <= 0 )
            {
                break;
            }
            cl->i_buffer++;

            if( cl->i_buffer + 1 >= cl->i_buffer_size )
            {
                cl->i_buffer_size += 1024;
                cl->p_buffer = realloc( cl->p_buffer, cl->i_buffer_size );
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
                    {
                        p++;
                    }
                    cl->query.psz_status = strdup( p );
                }
                else
                {
                    static const struct
                    {
                        char *name;
                        int  i_type;
                        int  i_proto;
                    }
                    msg_type[] =
                    {
                        { "GET",        HTTPD_MSG_GET,  HTTPD_PROTO_HTTP },
                        { "HEAD",       HTTPD_MSG_HEAD, HTTPD_PROTO_HTTP },
                        { "POST",       HTTPD_MSG_POST, HTTPD_PROTO_HTTP },

                        { "OPTIONS",    HTTPD_MSG_OPTIONS,  HTTPD_PROTO_RTSP },
                        { "DESCRIBE",   HTTPD_MSG_DESCRIBE, HTTPD_PROTO_RTSP },
                        { "SETUP",      HTTPD_MSG_SETUP,    HTTPD_PROTO_RTSP },
                        { "PLAY",       HTTPD_MSG_PLAY,     HTTPD_PROTO_RTSP },
                        { "PAUSE",      HTTPD_MSG_PAUSE,    HTTPD_PROTO_RTSP },
                        { "TEARDOWN",   HTTPD_MSG_TEARDOWN, HTTPD_PROTO_RTSP },

                        { NULL,         HTTPD_MSG_NONE,     HTTPD_PROTO_NONE }
                    };
                    int  i;

                    p = NULL;
                    cl->query.i_type = HTTPD_MSG_NONE;

                    /*fprintf( stderr, "received new request=%s\n", cl->p_buffer);*/

                    for( i = 0; msg_type[i].name != NULL; i++ )
                    {
                        if( !strncmp( (char *)cl->p_buffer, msg_type[i].name,
                                      strlen( msg_type[i].name ) ) )
                        {
                            p = (char *)&cl->p_buffer[strlen((char *)msg_type[i].name) + 1 ];
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
                        if( !strncasecmp( p, "rtsp:", 5 ) )
                        {
                            /* for rtsp url, you have rtsp://localhost:port/path */
                            p += 5;
                            while( *p == '/' ) p++;
                            while( *p && *p != '/' ) p++;
                        }
                        cl->query.psz_url = strdup( p );
                        if( ( p3 = strchr( cl->query.psz_url, '?' ) )  )
                        {
                            *p3++ = '\0';
                            cl->query.psz_args = (uint8_t *)strdup( p3 );
                        }
                        if( p2 )
                        {
                            while( *p2 == ' ' )
                            {
                                p2++;
                            }
                            if( !strncasecmp( p2, "HTTP/1.", 7 ) )
                            {
                                cl->query.i_proto = HTTPD_PROTO_HTTP;
                                cl->query.i_version = atoi( p2+7 );
                            }
                            else if( !strncasecmp( p2, "RTSP/1.", 7 ) )
                            {
                                cl->query.i_proto = HTTPD_PROTO_RTSP;
                                cl->query.i_version = atoi( p2+7 );
                            }
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
                    /* TODO Mhh, handle the case client will only send a request and close the connection
                     * to mark and of body (probably only RTSP) */
                    cl->query.p_body = malloc( cl->query.i_body );
                    cl->i_buffer = 0;
                }
                else
                {
                    cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
                }
            }
        }
    }

    /* check if the client is to be set to dead */
#if defined( WIN32 ) || defined( UNDER_CE )
    if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
    if( ( i_len < 0 && errno != EAGAIN && errno != EINTR ) || ( i_len == 0 ) )
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
    cl->i_activity_date = mdate();

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

        i_size = strlen( "HTTP/1.") + 10 + 10 +
                 strlen( cl->answer.psz_status ? cl->answer.psz_status : "" ) + 5;
        for( i = 0; i < cl->answer.i_name; i++ )
        {
            i_size += strlen( cl->answer.name[i] ) + 2 +
                      strlen( cl->answer.value[i] ) + 2;
        }

        if( cl->i_buffer_size < i_size )
        {
            cl->i_buffer_size = i_size;
            free( cl->p_buffer );
            cl->p_buffer = malloc( i_size );
        }
        p = (char *)cl->p_buffer;

        p += sprintf( p, "%s/1.%d %d %s\r\n",
                      cl->answer.i_proto ==  HTTPD_PROTO_HTTP ? "HTTP" : "RTSP",
                      cl->answer.i_version,
                      cl->answer.i_status, cl->answer.psz_status );
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
        cl->i_activity_date = mdate();
        cl->i_buffer += i_len;

        if( cl->i_buffer >= cl->i_buffer_size )
        {
            if( cl->answer.i_body == 0  && cl->answer.i_body_offset > 0 &&
                !cl->b_read_waiting )
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
#if defined( WIN32 ) || defined( UNDER_CE )
        if( ( i_len < 0 && WSAGetLastError() != WSAEWOULDBLOCK ) || ( i_len == 0 ) )
#else
        if( ( i_len < 0 && errno != EAGAIN && errno != EINTR ) || ( i_len == 0 ) )
#endif
        {
            /* error */
            cl->i_state = HTTPD_CLIENT_DEAD;
        }
    }
}

static void httpd_ClientTlsHsIn( httpd_client_t *cl )
{
    switch( tls_SessionContinueHandshake( cl->p_tls ) )
    {
        case 0:
            cl->i_state = HTTPD_CLIENT_RECEIVING;
            break;

        case -1:
            cl->i_state = HTTPD_CLIENT_DEAD;
            cl->p_tls = NULL;
            break;

        case 2:
            cl->i_state = HTTPD_CLIENT_TLS_HS_OUT;
    }
}

static void httpd_ClientTlsHsOut( httpd_client_t *cl )
{
    switch( tls_SessionContinueHandshake( cl->p_tls ) )
    {
        case 0:
            cl->i_state = HTTPD_CLIENT_RECEIVING;
            break;

        case -1:
            cl->i_state = HTTPD_CLIENT_DEAD;
            cl->p_tls = NULL;
            break;

        case 1:
            cl->i_state = HTTPD_CLIENT_TLS_HS_IN;
            break;
    }
}

static void httpd_HostThread( httpd_host_t *host )
{
    tls_session_t *p_tls = NULL;

    stats_Create( host, "client_connections", STATS_CLIENT_CONNECTIONS,
                  VLC_VAR_INTEGER, STATS_COUNTER );
    stats_Create( host, "active_connections", STATS_ACTIVE_CONNECTIONS,
                  VLC_VAR_INTEGER, STATS_COUNTER );

    while( !host->b_die )
    {
        struct timeval  timeout;
        fd_set          fds_read;
        fd_set          fds_write;
        /* FIXME: (too) many int variables */
        int             fd, i_fd;
        int             i_handle_max = 0;
        int             i_ret;
        int             i_client;
        int             b_low_delay = 0;

        if( host->i_url <= 0 )
        {
            /* 0.2s */
            msleep( 200000 );
            continue;
        }

        /* built a set of handle to select */
        FD_ZERO( &fds_read );
        FD_ZERO( &fds_write );

        i_handle_max = -1;

        for( i_fd = 0; (fd = host->fd[i_fd]) != -1; i_fd++ )
        {
            FD_SET( fd, &fds_read );
            if( fd > i_handle_max )
                i_handle_max = fd;
        }

        /* prepare a new TLS session */
        if( ( p_tls == NULL ) && ( host->p_tls != NULL ) )
            p_tls = tls_ServerSessionPrepare( host->p_tls );

        /* add all socket that should be read/write and close dead connection */
        vlc_mutex_lock( &host->lock );
        for( i_client = 0; i_client < host->i_client; i_client++ )
        {
            httpd_client_t *cl = host->client[i_client];

            if( cl->i_ref < 0 || ( cl->i_ref == 0 &&
                ( cl->i_state == HTTPD_CLIENT_DEAD ||
                  ( cl->i_activity_timeout > 0 &&
                    cl->i_activity_date+cl->i_activity_timeout < mdate()) ) ) )
            {
                httpd_ClientClean( cl );
                stats_UpdateInteger( host, STATS_ACTIVE_CONNECTIONS, -1, NULL );
                TAB_REMOVE( host->i_client, host->client, cl );
                free( cl );
                i_client--;
                continue;
            }
            else if( ( cl->i_state == HTTPD_CLIENT_RECEIVING )
                  || ( cl->i_state == HTTPD_CLIENT_TLS_HS_IN ) )
            {
                FD_SET( cl->fd, &fds_read );
                i_handle_max = __MAX( i_handle_max, cl->fd );
            }
            else if( ( cl->i_state == HTTPD_CLIENT_SENDING )
                  || ( cl->i_state == HTTPD_CLIENT_TLS_HS_OUT ) )
            {
                FD_SET( cl->fd, &fds_write );
                i_handle_max = __MAX( i_handle_max, cl->fd );
            }
            else if( cl->i_state == HTTPD_CLIENT_RECEIVE_DONE )
            {
                httpd_message_t *answer = &cl->answer;
                httpd_message_t *query  = &cl->query;
                int i_msg = query->i_type;

                httpd_MsgInit( answer );

                /* Handle what we received */
                if( (cl->i_mode != HTTPD_CLIENT_BIDIR) &&
                    (i_msg == HTTPD_MSG_ANSWER || i_msg == HTTPD_MSG_CHANNEL) )
                {
                    /* we can only receive request from client when not
                     * in BIDIR mode */
                    cl->url     = NULL;
                    cl->i_state = HTTPD_CLIENT_DEAD;
                }
                else if( i_msg == HTTPD_MSG_ANSWER )
                {
                    /* We are in BIDIR mode, trigger the callback and then
                     * check for new data */
                    if( cl->url && cl->url->catch[i_msg].cb )
                    {
                        cl->url->catch[i_msg].cb( cl->url->catch[i_msg].p_sys,
                                                  cl, NULL, query );
                    }
                    cl->i_state = HTTPD_CLIENT_WAITING;
                }
                else if( i_msg == HTTPD_MSG_CHANNEL )
                {
                    /* We are in BIDIR mode, trigger the callback and then
                     * check for new data */
                    if( cl->url && cl->url->catch[i_msg].cb )
                    {
                        cl->url->catch[i_msg].cb( cl->url->catch[i_msg].p_sys,
                                                  cl, NULL, query );
                    }
                    cl->i_state = HTTPD_CLIENT_WAITING;
                }
                else if( i_msg == HTTPD_MSG_OPTIONS )
                {
                    char *psz_cseq = NULL;
                    int i_cseq;

                    /* unimplemented */
                    answer->i_proto  = query->i_proto ;
                    answer->i_type   = HTTPD_MSG_ANSWER;
                    answer->i_version= 0;
                    answer->i_status = 200;
                    answer->psz_status = strdup( "Ok" );

                    answer->i_body = 0;
                    answer->p_body = NULL;

                    psz_cseq = httpd_MsgGet( query, "Cseq" );
                    if( psz_cseq )
                        i_cseq = atoi( psz_cseq );
                    else
                        i_cseq = 0;
                    httpd_MsgAdd( answer, "Cseq", "%d", i_cseq );
                    httpd_MsgAdd( answer, "Server", "VLC Server" );
                    httpd_MsgAdd( answer, "Public", "DESCRIBE, SETUP, "
                                 "TEARDOWN, PLAY, PAUSE" );
                    httpd_MsgAdd( answer, "Content-Length", "%d",
                                  answer->i_body );

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
                        uint8_t *p;

                        /* unimplemented */
                        answer->i_proto  = query->i_proto ;
                        answer->i_type   = HTTPD_MSG_ANSWER;
                        answer->i_version= 0;
                        answer->i_status = 501;
                        answer->psz_status = strdup( "Unimplemented" );

                        p = answer->p_body = malloc( 1000 );

                        p += sprintf( (char *)p,
                            "<?xml version=\"1.0\" encoding=\"ascii\" ?>"
                            "<!DOCTYPE html PUBLIC \"-//W3C//DTD  XHTML 1.0 Strict//EN\" "
                            "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
                            "<html>\n"
                            "<head>\n"
                            "<title>Error 501</title>\n"
                            "</head>\n"
                            "<body>\n"
                            "<h1>501 Unimplemented</h1>\n"
                            "<hr />\n"
                            "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
                            "</body>\n"
                            "</html>\n" );

                        answer->i_body = p - answer->p_body;
                        httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );

                        cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                        cl->i_state = HTTPD_CLIENT_SENDING;
                    }
                }
                else
                {
                    vlc_bool_t b_auth_failed = VLC_FALSE;
                    vlc_bool_t b_hosts_failed = VLC_FALSE;
                    int i;

                    /* Search the url and trigger callbacks */
                    for( i = 0; i < host->i_url; i++ )
                    {
                        httpd_url_t *url = host->url[i];

                        if( !strcmp( url->psz_url, query->psz_url ) )
                        {
                            if( url->catch[i_msg].cb )
                            {
                                if( answer && ( url->p_acl != NULL ) )
                                {
                                    char ip[NI_MAXNUMERICHOST];

                                    if( httpd_ClientIP( cl, ip ) != NULL )
                                    {
                                        if( ACL_Check( url->p_acl, ip ) )
                                        {
                                            b_hosts_failed = VLC_TRUE;
                                            break;
                                        }
                                    }
                                    else
                                        b_hosts_failed = VLC_TRUE;
                                }

                                if( answer && ( *url->psz_user || *url->psz_password ) )
                                {
                                    /* create the headers */
                                    char *b64 = httpd_MsgGet( query, "Authorization" ); /* BASIC id */
                                    char *auth;
                                    char *id;

                                    asprintf( &id, "%s:%s", url->psz_user, url->psz_password );
                                    if( b64 ) auth = malloc( strlen(b64) + 1 );
                                    else auth = malloc( strlen("") + 1 );

                                    if( b64 != NULL
                                         && !strncasecmp( b64, "BASIC", 5 ) )
                                    {
                                        b64 += 5;
                                        while( *b64 == ' ' )
                                        {
                                            b64++;
                                        }
                                        b64_decode( auth, b64 );
                                    }
                                    else
                                    {
                                        strcpy( auth, "" );
                                    }

                                    if( strcmp( id, auth ) )
                                    {
                                        httpd_MsgAdd( answer, "WWW-Authenticate", "Basic realm=\"%s\"", url->psz_user );
                                        /* We fail for all url */
                                        b_auth_failed = VLC_TRUE;
                                        free( id );
                                        free( auth );
                                        break;
                                    }

                                    free( id );
                                    free( auth );
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
                        uint8_t *p;

                        answer->i_proto  = query->i_proto;
                        answer->i_type   = HTTPD_MSG_ANSWER;
                        answer->i_version= 0;
                        p = answer->p_body = malloc( 1000 + strlen(query->psz_url) );

                        if( b_hosts_failed )
                        {
                            answer->i_status = 403;
                            answer->psz_status = strdup( "Forbidden" );

                            /* FIXME: lots of code duplication */
                            p += sprintf( (char *)p,
                                "<?xml version=\"1.0\" encoding=\"ascii\" ?>"
                                "<!DOCTYPE html PUBLIC \"-//W3C//DTD  XHTML 1.0 Strict//EN\" "
                                "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
                                "<html>\n"
                                "<head>\n"
                                "<title>Error 403</title>\n"
                                "</head>\n"
                                "<body>\n"
                                "<h1>403 Forbidden (%s)</h1>\n"
                                "<hr />\n"
                                "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
                                "</body>\n"
                                "</html>\n", query->psz_url );
                        }
                        else if( b_auth_failed )
                        {
                            answer->i_status = 401;
                            answer->psz_status = strdup( "Authorization Required" );

                            p += sprintf( (char *)p,
                                "<?xml version=\"1.0\" encoding=\"ascii\" ?>"
                                "<!DOCTYPE html PUBLIC \"-//W3C//DTD  XHTML 1.0 Strict//EN\" "
                                "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
                                "<html>\n"
                                "<head>\n"
                                "<title>Error 401</title>\n"
                                "</head>\n"
                                "<body>\n"
                                "<h1>401 Authorization Required (%s)</h1>\n"
                                "<hr />\n"
                                "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
                                "</body>\n"
                                "</html>\n", query->psz_url );
                        }
                        else
                        {
                            /* no url registered */
                            answer->i_status = 404;
                            answer->psz_status = strdup( "Not found" );

                            p += sprintf( (char *)p,
                                "<?xml version=\"1.0\" encoding=\"ascii\" ?>"
                                "<!DOCTYPE html PUBLIC \"-//W3C//DTD  XHTML 1.0 Strict//EN\" "
                                "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
                                "<html>\n"
                                "<head>\n"
                                "<title>Error 404</title>\n"
                                "</head>\n"
                                "<body>\n"
                                "<h1>404 Resource not found(%s)</h1>\n"
                                "<hr />\n"
                                "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
                                "</body>\n"
                                "</html>\n", query->psz_url );
                        }

                        answer->i_body = p - answer->p_body;
                        cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                        httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
                    }

                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
            }
            else if( cl->i_state == HTTPD_CLIENT_SEND_DONE )
            {
                if( cl->i_mode == HTTPD_CLIENT_FILE || cl->answer.i_body_offset == 0 )
                {
                    char *psz_connection = httpd_MsgGet( &cl->answer, "Connection" );
                    char *psz_query = httpd_MsgGet( &cl->query, "Connection" );
                    vlc_bool_t b_connection = VLC_FALSE;
                    vlc_bool_t b_keepalive = VLC_FALSE;
                    vlc_bool_t b_query = VLC_FALSE;

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
                          ( ( cl->answer.i_version == 0 && b_keepalive ) ||
                            ( cl->answer.i_version == 1 && !b_connection ) ) ) ||
                        ( ( cl->query.i_proto == HTTPD_PROTO_RTSP ) &&
                          !b_query && !b_connection ) )
                    {
                        httpd_MsgClean( &cl->query );
                        httpd_MsgInit( &cl->query );

                        cl->i_buffer = 0;
                        cl->i_buffer_size = 1000;
                        free( cl->p_buffer );
                        cl->p_buffer = malloc( cl->i_buffer_size );
                        cl->i_state = HTTPD_CLIENT_RECEIVING;
                    }
                    else
                    {
                        cl->i_state = HTTPD_CLIENT_DEAD;
                    }
                    httpd_MsgClean( &cl->answer );
                }
                else if( cl->b_read_waiting )
                {
                    /* we have a message waiting for us to read it */
                    httpd_MsgClean( &cl->answer );
                    httpd_MsgClean( &cl->query );

                    cl->i_buffer = 0;
                    cl->i_buffer_size = 1000;
                    free( cl->p_buffer );
                    cl->p_buffer = malloc( cl->i_buffer_size );
                    cl->i_state = HTTPD_CLIENT_RECEIVING;
                    cl->b_read_waiting = VLC_FALSE;
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
                    /* we have new data, so reenter send mode */
                    cl->i_buffer      = 0;
                    cl->p_buffer      = cl->answer.p_body;
                    cl->i_buffer_size = cl->answer.i_body;
                    cl->answer.p_body = NULL;
                    cl->answer.i_body = 0;
                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
                else
                {
                    /* we shouldn't wait too long */
                    b_low_delay = VLC_TRUE;
                }
            }

            /* Special for BIDIR mode we also check reading */
            if( cl->i_mode == HTTPD_CLIENT_BIDIR &&
                cl->i_state == HTTPD_CLIENT_SENDING )
            {
                FD_SET( cl->fd, &fds_read );
                i_handle_max = __MAX( i_handle_max, cl->fd );
            }
        }
        vlc_mutex_unlock( &host->lock );

        /* we will wait 100ms or 20ms (not too big 'cause HTTPD_CLIENT_WAITING) */
        timeout.tv_sec = 0;
        timeout.tv_usec = b_low_delay ? 20000 : 100000;

        i_ret = select( i_handle_max + 1,
                        &fds_read, &fds_write, NULL, &timeout );

        if( (i_ret == -1) && (errno != EINTR) )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Warn( host, "cannot select sockets (%d)", WSAGetLastError( ) );
#else
            msg_Warn( host, "cannot select sockets : %s", strerror( errno ) );
#endif
            msleep( 1000 );
            continue;
        }
        else if( i_ret <= 0 )
        {
            continue;
        }

        /* accept new connections */
        for( i_fd = 0; (fd = host->fd[i_fd]) != -1; i_fd++ )
        {
            if( FD_ISSET( fd, &fds_read ) )
            {
                socklen_t i_sock_size = sizeof( struct sockaddr_storage );
                struct  sockaddr_storage sock;

                fd = accept( fd, (struct sockaddr *)&sock, &i_sock_size );
                if( fd >= 0 )
                {
                    int i_state = 0;

                    /* set this new socket non-block */
#if defined( WIN32 ) || defined( UNDER_CE )
                    {
                        unsigned long i_dummy = 1;
                        ioctlsocket( fd, FIONBIO, &i_dummy );
                    }
#else
                    fcntl( fd, F_SETFL, O_NONBLOCK );
#endif

                    if( p_tls != NULL)
                    {
                        switch ( tls_ServerSessionHandshake( p_tls, fd ) )
                        {
                            case -1:
                                msg_Err( host, "Rejecting TLS connection" );
                                net_Close( fd );
                                fd = -1;
                                p_tls = NULL;
                                break;

                            case 1: /* missing input - most likely */
                                i_state = HTTPD_CLIENT_TLS_HS_IN;
                                break;

                            case 2: /* missing output */
                                i_state = HTTPD_CLIENT_TLS_HS_OUT;
                                break;
                        }
                    }

                    if( fd >= 0 )
                    {
                        httpd_client_t *cl;
                        char ip[NI_MAXNUMERICHOST];
                        stats_UpdateInteger( host, STATS_CLIENT_CONNECTIONS,
                                             1, NULL );
                        stats_UpdateInteger( host, STATS_ACTIVE_CONNECTIONS, 1,
                                             NULL );
                        cl = httpd_ClientNew( fd, &sock, i_sock_size, p_tls );
                        httpd_ClientIP( cl, ip );
                        msg_Dbg( host, "Connection from %s", ip );
                        p_tls = NULL;
                        vlc_mutex_lock( &host->lock );
                        TAB_APPEND( host->i_client, host->client, cl );
                        vlc_mutex_unlock( &host->lock );

                        if( i_state != 0 )
                            cl->i_state = i_state; // override state for TLS
                    }
                }
            }
        }

        /* now try all others socket */
        vlc_mutex_lock( &host->lock );
        for( i_client = 0; i_client < host->i_client; i_client++ )
        {
            httpd_client_t *cl = host->client[i_client];
            if( cl->i_state == HTTPD_CLIENT_RECEIVING )
            {
                httpd_ClientRecv( cl );
            }
            else if( cl->i_state == HTTPD_CLIENT_SENDING )
            {
                httpd_ClientSend( cl );
            }
            else if( cl->i_state == HTTPD_CLIENT_TLS_HS_IN )
            {
                httpd_ClientTlsHsIn( cl );
            }
            else if( cl->i_state == HTTPD_CLIENT_TLS_HS_OUT )
            {
                httpd_ClientTlsHsOut( cl );
            }

            if( cl->i_mode == HTTPD_CLIENT_BIDIR &&
                cl->i_state == HTTPD_CLIENT_SENDING &&
                FD_ISSET( cl->fd, &fds_read ) )
            {
                cl->b_read_waiting = VLC_TRUE;
            }
        }
        vlc_mutex_unlock( &host->lock );
    }

    if( p_tls != NULL )
        tls_ServerSessionClose( p_tls );
}

#else /* ENABLE_HTTPD */

/* We just define an empty wrapper */
httpd_host_t *httpd_TLSHostNew( vlc_object_t *a, char *b, int c,
                                tls_server_t *d )
{
    msg_Err( a, "HTTP daemon support is disabled" );
    return NULL;
}

httpd_host_t *httpd_HostNew( vlc_object_t *a, char *b, int c )
{
    msg_Err( a, "HTTP daemon support is disabled" );
    return NULL;
}

void httpd_HostDelete( httpd_host_t *a )
{
}

httpd_url_t *httpd_UrlNew( httpd_host_t *host, char *psz_url,
                           char *psz_user, char *psz_password,
                           const vlc_acl_t *p_acl )
{
    return NULL;
}

httpd_url_t *httpd_UrlNewUnique( httpd_host_t *host, char *psz_url,
                                 char *psz_user, char *psz_password,
                                 const vlc_acl_t *p_acl )
{
    return NULL;
}

int httpd_UrlCatch( httpd_url_t *a, int b, httpd_callback_t c,
                    httpd_callback_sys_t *d )
{
    return 0;
}

void httpd_UrlDelete( httpd_url_t *a )
{
}

char* httpd_ClientIP( httpd_client_t *cl, char *psz_ip )
{
    return NULL;
}

char* httpd_ServerIP( httpd_client_t *cl, char *psz_ip )
{
    return NULL;
}

void httpd_ClientModeStream( httpd_client_t *a )
{
}

void httpd_ClientModeBidir( httpd_client_t *a )
{
}

void httpd_FileDelete( httpd_file_t *a )
{
}

httpd_file_t *httpd_FileNew( httpd_host_t *a, char *b, char *c, char *d,
                             char *e, httpd_file_callback_t f,
                             httpd_file_sys_t *g )
{
    return NULL;
}

httpd_handler_t *httpd_HandlerNew( httpd_host_t *host, const char *psz_url,
                                   const char *psz_user,
                                   const char *psz_password,
                                   const vlc_acl_t *p_acl,
                                   httpd_handler_callback_t pf_fill,
                                   httpd_handler_sys_t *p_sys )
{
    return NULL;
}

void httpd_HandlerDelete( httpd_handler_t *handler )
{
}

void httpd_RedirectDelete( httpd_redirect_t *a )
{
}

httpd_redirect_t *httpd_RedirectNew( httpd_host_t *a,
                                     char *b, char *c )
{
    return NULL;
}

void httpd_StreamDelete( httpd_stream_t *a )
{
}

int httpd_StreamHeader( httpd_stream_t *a, uint8_t *b, int c )
{
    return 0;
}

int httpd_StreamSend ( httpd_stream_t *a, uint8_t *b, int c )
{
    return 0;
}

httpd_stream_t *httpd_StreamNew( httpd_host_t *a, char *b, char *c,
                                 char *d, char *e )
{
    return NULL;
}

void httpd_MsgInit ( httpd_message_t *a )
{
}

void httpd_MsgAdd  ( httpd_message_t *a, char *b, char *c, ... )
{
}

char *httpd_MsgGet ( httpd_message_t *a, char *b )
{
    return NULL;
}

void httpd_MsgClean( httpd_message_t *a )
{
}

#endif /* ENABLE_HTTPD */
