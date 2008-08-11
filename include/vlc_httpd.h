/*****************************************************************************
 * vlc_httpd.h: builtin HTTP/RTSP server.
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_HTTPD_H
#define VLC_HTTPD_H 1

/**
 * \file
 * This file defines functions, structures, enums and macros for httpd functionality in vlc.
 */

enum
{
    HTTPD_MSG_NONE,

    /* answer */
    HTTPD_MSG_ANSWER,

    /* channel communication */
    HTTPD_MSG_CHANNEL,

    /* http request */
    HTTPD_MSG_GET,
    HTTPD_MSG_HEAD,
    HTTPD_MSG_POST,

    /* rtsp request */
    HTTPD_MSG_OPTIONS,
    HTTPD_MSG_DESCRIBE,
    HTTPD_MSG_SETUP,
    HTTPD_MSG_PLAY,
    HTTPD_MSG_PAUSE,
    HTTPD_MSG_GETPARAMETER,
    HTTPD_MSG_TEARDOWN,

    /* just to track the count of MSG */
    HTTPD_MSG_MAX
};

enum
{
    HTTPD_PROTO_NONE,
    HTTPD_PROTO_HTTP,  /* HTTP/1.x */
    HTTPD_PROTO_RTSP,  /* RTSP/1.x */
    HTTPD_PROTO_HTTP0, /* HTTP/0.x */
};

struct httpd_message_t
{
    httpd_client_t *cl; /* NULL if not throught a connection e vlc internal */

    uint8_t i_type;
    uint8_t i_proto;
    uint8_t i_version;

    /* for an answer */
    int     i_status;

    /* for a query */
    char    *psz_url;
    /* FIXME find a clean way to handle GET(psz_args)
       and POST(body) through the same code */
    uint8_t *psz_args;

    /* for rtp over rtsp */
    int     i_channel;

    /* options */
    int     i_name;
    char    **name;
    int     i_value;
    char    **value;

    /* body */
    int64_t i_body_offset;
    int     i_body;
    uint8_t *p_body;

};

/* create a new host */
VLC_EXPORT( httpd_host_t *, httpd_HostNew, ( vlc_object_t *, const char *psz_host, int i_port ) );
VLC_EXPORT( httpd_host_t *, httpd_TLSHostNew, ( vlc_object_t *, const char *, int, const char *, const char *, const char *, const char * ) );

/* delete a host */
VLC_EXPORT( void,           httpd_HostDelete, ( httpd_host_t * ) );

/* register a new url */
VLC_EXPORT( httpd_url_t *,  httpd_UrlNew, ( httpd_host_t *, const char *psz_url, const char *psz_user, const char *psz_password, const vlc_acl_t *p_acl ) );
VLC_EXPORT( httpd_url_t *,  httpd_UrlNewUnique, ( httpd_host_t *, const char *psz_url, const char *psz_user, const char *psz_password, const vlc_acl_t *p_acl ) );
/* register callback on a url */
VLC_EXPORT( int,            httpd_UrlCatch, ( httpd_url_t *, int i_msg, httpd_callback_t, httpd_callback_sys_t * ) );
/* delete an url */
VLC_EXPORT( void,           httpd_UrlDelete, ( httpd_url_t * ) );

/* Default client mode is FILE, use these to change it */
VLC_EXPORT( void,           httpd_ClientModeStream, ( httpd_client_t *cl ) );
VLC_EXPORT( void,           httpd_ClientModeBidir, ( httpd_client_t *cl ) );
VLC_EXPORT( char*,          httpd_ClientIP, ( const httpd_client_t *cl, char *psz_ip ) );
VLC_EXPORT( char*,          httpd_ServerIP, ( const httpd_client_t *cl, char *psz_ip ) );

/* High level */

VLC_EXPORT( httpd_file_t *, httpd_FileNew, ( httpd_host_t *, const char *psz_url, const char *psz_mime, const char *psz_user, const char *psz_password, const vlc_acl_t *p_acl, httpd_file_callback_t pf_fill, httpd_file_sys_t * ) );
VLC_EXPORT( httpd_file_sys_t *, httpd_FileDelete, ( httpd_file_t * ) );


VLC_EXPORT( httpd_handler_t *, httpd_HandlerNew, ( httpd_host_t *, const char *psz_url, const char *psz_user, const char *psz_password, const vlc_acl_t *p_acl, httpd_handler_callback_t pf_fill, httpd_handler_sys_t * ) );
VLC_EXPORT( httpd_handler_sys_t *, httpd_HandlerDelete, ( httpd_handler_t * ) );


VLC_EXPORT( httpd_redirect_t *, httpd_RedirectNew, ( httpd_host_t *, const char *psz_url_dst, const char *psz_url_src ) );
VLC_EXPORT( void,               httpd_RedirectDelete, ( httpd_redirect_t * ) );


VLC_EXPORT( httpd_stream_t *, httpd_StreamNew,    ( httpd_host_t *, const char *psz_url, const char *psz_mime, const char *psz_user, const char *psz_password, const vlc_acl_t *p_acl ) );
VLC_EXPORT( void,             httpd_StreamDelete, ( httpd_stream_t * ) );
VLC_EXPORT( int,              httpd_StreamHeader, ( httpd_stream_t *, uint8_t *p_data, int i_data ) );
VLC_EXPORT( int,              httpd_StreamSend,   ( httpd_stream_t *, uint8_t *p_data, int i_data ) );


/* Msg functions facilities */
VLC_EXPORT( void,         httpd_MsgInit, ( httpd_message_t * )  );
VLC_EXPORT( void,         httpd_MsgAdd, ( httpd_message_t *, const char *psz_name, const char *psz_value, ... ) LIBVLC_FORMAT( 3, 4 ) );
/* return "" if not found. The string is not allocated */
VLC_EXPORT( const char *, httpd_MsgGet, ( const httpd_message_t *, const char *psz_name ) );
VLC_EXPORT( void,         httpd_MsgClean, ( httpd_message_t * ) );

#endif /* _VLC_HTTPD_H */
