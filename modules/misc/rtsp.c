/*****************************************************************************
 * rtsp.c: rtsp VoD server module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "vlc_httpd.h"
#include "vlc_vod.h"
#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define HOST_TEXT N_( "Host address" )
#define HOST_LONGTEXT N_( \
    "You can set the address, port and path the rtsp interface will bind to." )

vlc_module_begin();
    set_description( _("RTSP VoD server") );
    set_capability( "vod server", 1 );
    set_callbacks( Open, Close );
    add_shortcut( "rtsp" );
    add_string ( "rtsp-host", NULL, NULL, HOST_TEXT, HOST_LONGTEXT, VLC_TRUE );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

typedef struct
{
    char *psz_session;
    int64_t i_last; /* for timeout */

    vlc_bool_t b_playing; /* is it in "play" state */

} rtsp_client_t;

typedef struct
{
    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    httpd_url_t  *p_rtsp_url;

    vod_media_t *p_media;

} media_es_t;

struct vod_media_t
{
    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    httpd_url_t  *p_rtsp_url;
    char         *psz_rtsp_control;
    char         *psz_rtsp_path;

    char *psz_destination;
    int  i_port;
    int  i_port_audio;
    int  i_port_video;
    int  i_ttl;

    /* ES list */
    int        i_es;
    media_es_t **es;

    /* RTSP client */
    int           i_rtsp;
    rtsp_client_t **rtsp;
};

struct vod_sys_t
{
    /* RTSP server */
    httpd_host_t *p_rtsp_host;
    char *psz_host;
    char *psz_path;
    int i_port;

    /* List of media */
    int i_media;
    vod_media_t **media;
};

static vod_media_t *MediaNew( vod_t *, char *, input_item_t * );
static void         MediaDel( vod_t *, vod_media_t * );
static int          MediaAddES( vod_t *, vod_media_t *, es_format_t * );
static void         MediaDelES( vod_t *, vod_media_t *, es_format_t * );

static rtsp_client_t *RtspClientNew( vod_media_t *, char * );
static rtsp_client_t *RtspClientGet( vod_media_t *, char * );
static void           RtspClientDel( vod_media_t *, rtsp_client_t * );

static int RtspCallback( httpd_callback_sys_t *, httpd_client_t *,
                         httpd_message_t *, httpd_message_t * );
static int RtspCallbackId( httpd_callback_sys_t *, httpd_client_t *,
                           httpd_message_t *, httpd_message_t * );

static char *SDPGenerate( vod_media_t *, char * );

/*****************************************************************************
 * Open: Starts the RTSP server module
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vod_t *p_vod = (vod_t *)p_this;
    vod_sys_t *p_sys = 0;
    char *psz_url = 0;
    vlc_url_t url;

    psz_url = config_GetPsz( p_vod, "rtsp-host" );
    vlc_UrlParse( &url, psz_url, 0 );
    if( psz_url ) free( psz_url );

    if( !url.psz_host || !*url.psz_host )
    {
        if( url.psz_host ) free( url.psz_host );
        url.psz_host = strdup( "localhost" );
    }
    if( url.i_port <= 0 ) url.i_port = 554;

    p_vod->p_sys = p_sys = malloc( sizeof( vod_sys_t ) );
    if( !p_sys ) goto error;
    p_sys->p_rtsp_host = 0;

    p_sys->p_rtsp_host =
        httpd_HostNew( VLC_OBJECT(p_vod), url.psz_host, url.i_port );
    if( !p_sys->p_rtsp_host )
    {
        msg_Err( p_vod, "cannot create http server (%s:%i)",
                 url.psz_host, url.i_port );
        goto error;
    }

    p_sys->psz_host = strdup( url.psz_host );
    p_sys->psz_path = strdup( url.psz_path ? url.psz_path : "/" );
    p_sys->i_port = url.i_port;

    vlc_UrlClean( &url );
    p_sys->media = 0;
    p_sys->i_media = 0;

    p_vod->pf_media_new = MediaNew;
    p_vod->pf_media_del = MediaDel;
    p_vod->pf_media_add_es = MediaAddES;
    p_vod->pf_media_del_es = MediaDelES;

    return VLC_SUCCESS;

 error:

    if( p_sys && p_sys->p_rtsp_host ) httpd_HostDelete( p_sys->p_rtsp_host );
    if( p_sys ) free( p_sys );
    vlc_UrlClean( &url );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    vod_t *p_vod = (vod_t *)p_this;
    vod_sys_t *p_sys = p_vod->p_sys;

    httpd_HostDelete( p_sys->p_rtsp_host );

    /* TODO delete medias */

    free( p_sys );
}

/*****************************************************************************
 * Media handling
 *****************************************************************************/
static vod_media_t *MediaNew( vod_t *p_vod, char *psz_name,
                              input_item_t *p_item )
{
    vod_sys_t *p_sys = p_vod->p_sys;
    vod_media_t *p_media = malloc( sizeof(vod_media_t) );
    int i;

    memset( p_media, 0, sizeof(vod_media_t) );
    asprintf( &p_media->psz_rtsp_path, "%s%s", p_sys->psz_path, psz_name );

    p_media->p_rtsp_url =
        httpd_UrlNewUnique( p_sys->p_rtsp_host, p_media->psz_rtsp_path, 0, 0 );

    if( !p_media->p_rtsp_url )
    {
        msg_Err( p_vod, "cannot create http url" );
        free( p_media->psz_rtsp_path );
        free( p_media );
        return 0;
    }

    msg_Dbg( p_vod, "created rtsp url: %s", p_media->psz_rtsp_path );

    asprintf( &p_media->psz_rtsp_control, "rtsp://%s:%d%s",
              p_sys->psz_host, p_sys->i_port, p_media->psz_rtsp_path );

    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_DESCRIBE,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_PLAY,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_PAUSE,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_TEARDOWN,
                    RtspCallback, (void*)p_media );

    p_media->p_vod = p_vod;

    TAB_APPEND( p_sys->i_media, p_sys->media, p_media );

    vlc_mutex_lock( &p_item->lock );
    msg_Dbg( p_vod, "media has %i declared ES", p_item->i_es );
    for( i = 0; i < p_item->i_es; i++ )
    msg_Dbg( p_vod, "  - ES %i: %4.4s", i, (char *)&p_item->es[i]->i_codec );
    vlc_mutex_unlock( &p_item->lock );

    return p_media;
}

static void MediaDel( vod_t *p_vod, vod_media_t *p_media )
{
    vod_sys_t *p_sys = p_vod->p_sys;

    while( p_media->i_rtsp > 0 ) RtspClientDel( p_media, p_media->rtsp[0] );
    httpd_UrlDelete( p_media->p_rtsp_url );
    if( p_media->psz_rtsp_path ) free( p_media->psz_rtsp_path );
    if( p_media->psz_rtsp_control ) free( p_media->psz_rtsp_control );

    TAB_REMOVE( p_sys->i_media, p_sys->media, p_media );
    free( p_media );
}

static int MediaAddES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt )
{
    media_es_t *p_es = malloc( sizeof(media_es_t) );
    memset( p_es, 0, sizeof(media_es_t) );

    TAB_APPEND( p_media->i_es, p_media->es, p_es );

    /* TODO: update SDP, etc... */

    if( p_media->p_rtsp_url )
    {
        char psz_urlc[strlen( p_media->psz_rtsp_control ) + 1 + 10];

        sprintf( psz_urlc, "%s/trackid=%d",
                 p_media->psz_rtsp_path, p_media->i_es );
        fprintf( stderr, "rtsp: adding %s\n", psz_urlc );

        p_es->p_rtsp_url =
            httpd_UrlNewUnique( p_vod->p_sys->p_rtsp_host, psz_urlc, 0, 0 );

        if( p_es->p_rtsp_url )
        {
            httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_SETUP,
                            RtspCallbackId, (void*)p_es );
#if 0
            httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_PLAY,
                            RtspCallback, (void*)p_es );
            httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_PAUSE,
                            RtspCallback, (void*)p_es );
#endif
        }
    }

    p_es->p_vod = p_vod;
    p_es->p_media = p_media;

    return VLC_SUCCESS;
}

static void MediaDelES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt)
{
    media_es_t *p_es = 0;

    TAB_REMOVE( p_media->i_es, p_media->es, p_es );

    /* TODO do something useful */

    if( p_es->p_rtsp_url ) httpd_UrlDelete( p_es->p_rtsp_url );
}

/****************************************************************************
 * RTSP server implementation
 ****************************************************************************/
static rtsp_client_t *RtspClientNew( vod_media_t *p_media, char *psz_session )
{
    rtsp_client_t *rtsp = malloc( sizeof(rtsp_client_t) );

    rtsp->psz_session = psz_session;
    rtsp->i_last = 0;
    rtsp->b_playing = VLC_FALSE;

    TAB_APPEND( p_media->i_rtsp, p_media->rtsp, rtsp );

    msg_Dbg( p_media->p_vod, "new session: %s", psz_session );

    return rtsp;
}

static rtsp_client_t *RtspClientGet( vod_media_t *p_media, char *psz_session )
{
    int i;

    for( i = 0; i < p_media->i_rtsp; i++ )
    {
        if( !strcmp( p_media->rtsp[i]->psz_session, psz_session ) )
        {
            return p_media->rtsp[i];
        }
    }

    return NULL;
}

static void RtspClientDel( vod_media_t *p_media, rtsp_client_t *rtsp )
{
    msg_Dbg( p_media->p_vod, "closing session: %s", rtsp->psz_session );

    TAB_REMOVE( p_media->i_rtsp, p_media->rtsp, rtsp );

    free( rtsp->psz_session );
    free( rtsp );
}

static int RtspCallback( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                         httpd_message_t *answer, httpd_message_t *query )
{
    vod_media_t *p_media = (vod_media_t*)p_args;
    vod_t *p_vod = p_media->p_vod;
    char *psz_destination = p_media->psz_destination;
    char *psz_session = NULL;

    if( answer == NULL || query == NULL ) return VLC_SUCCESS;

    fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto   = HTTPD_PROTO_RTSP;
    answer->i_version = query->i_version;
    answer->i_type    = HTTPD_MSG_ANSWER;

    switch( query->i_type )
    {
        case HTTPD_MSG_DESCRIBE:
        {
            char *psz_sdp =
                SDPGenerate( p_media, psz_destination ?
                             psz_destination : "0.0.0.0" );

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            httpd_MsgAdd( answer, "Content-type",  "%s", "application/sdp" );

            answer->p_body = psz_sdp;
            answer->i_body = strlen( psz_sdp );
            break;
        }

        case HTTPD_MSG_PLAY:
        {
            rtsp_client_t *rtsp;

            /* for now only multicast so easy */
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PLAY for session: %s", psz_session );

            rtsp = RtspClientGet( p_media, psz_session );
            if( rtsp && !rtsp->b_playing )
            {
                rtsp->b_playing = VLC_TRUE;
                /* TODO: do something useful */
            }
            break;
        }

        case HTTPD_MSG_PAUSE:
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PAUSE for session: %s", psz_session );
            /* TODO: do something useful */
            return VLC_EGENERIC;

        case HTTPD_MSG_TEARDOWN:
        {
            rtsp_client_t *rtsp;

            /* for now only multicast so easy again */
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_TEARDOWN for session: %s", psz_session);

            rtsp = RtspClientGet( p_media, psz_session );
            if( rtsp )
            {
                /* TODO: do something useful */
                RtspClientDel( p_media, rtsp );
            }
            break;
        }

        default:
            return VLC_EGENERIC;
    }

    httpd_MsgAdd( answer, "Server", "VLC Server" );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cseq", "%d",
                  atoi( httpd_MsgGet( query, "Cseq" ) ) );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
    {
        httpd_MsgAdd( answer, "Session", "%s;timeout=5", psz_session );
    }

    return VLC_SUCCESS;
}

static int RtspCallbackId( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                           httpd_message_t *answer, httpd_message_t *query )
{
    vod_media_t *p_media = (vod_media_t*)p_args;
    vod_t *p_vod = p_media->p_vod;
    char *psz_session = NULL;
    char *psz_transport = NULL;

    if( answer == NULL || query == NULL ) return VLC_SUCCESS;

    fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto   = HTTPD_PROTO_RTSP;
    answer->i_version = query->i_version;
    answer->i_type    = HTTPD_MSG_ANSWER;

    switch( query->i_type )
    {
    case HTTPD_MSG_SETUP:
        psz_transport = httpd_MsgGet( query, "Transport" );
        fprintf( stderr, "HTTPD_MSG_SETUP: transport=%s\n", psz_transport );

        if( strstr( psz_transport, "multicast" ) && p_media->psz_destination )
        {
            fprintf( stderr, "HTTPD_MSG_SETUP: multicast\n" );
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            if( !psz_session || !*psz_session )
            {
                if( psz_session ) free( psz_session );
                asprintf( &psz_session, "%d", rand() );
            }

            httpd_MsgAdd( answer, "Transport",
                          "RTP/AVP/UDP;destination=%s;port=%d-%d;ttl=%d",
                          p_media->psz_destination, p_media->i_port,
                          p_media->i_port+1, p_media->i_ttl );
        }
        else if( strstr( psz_transport, "unicast" ) &&
                 strstr( psz_transport, "client_port=" ) )
        {
            rtsp_client_t *rtsp = NULL;
            char *ip = httpd_ClientIP( cl );
            int i_port = atoi( strstr( psz_transport, "client_port=" ) +
                               strlen("client_port=") );

            if( !ip )
            {
                answer->i_status = 400;
                answer->psz_status = strdup( "Internal server error" );
                answer->i_body = 0;
                answer->p_body = NULL;
                break;
            }

            fprintf( stderr, "HTTPD_MSG_SETUP: unicast ip=%s port=%d\n",
                     ip, i_port );

            psz_session = httpd_MsgGet( query, "Session" );
            if( !psz_session || !*psz_session )
            {
                if( psz_session ) free( psz_session );
                asprintf( &psz_session, "%d", rand() );
                rtsp = RtspClientNew( p_media, psz_session );
            }
            else
            {
                rtsp = RtspClientGet( p_media, psz_session );
                if( !rtsp )
                {
                    /* FIXME right error code */
                    answer->i_status = 400;
                    answer->psz_status = strdup( "Unknown session id" );
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    free( ip );
                    break;
                }
            }

            /* TODO: do something useful */

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            httpd_MsgAdd( answer, "Transport", "RTP/AVP/UDP;client_port=%d-%d",
                          i_port, i_port + 1 );
        }
        else /* TODO  strstr( psz_transport, "interleaved" ) ) */
        {
            answer->i_status = 400;
            answer->psz_status = strdup( "Bad Request" );
            answer->i_body = 0;
            answer->p_body = NULL;
        }
        break;

        default:
            return VLC_EGENERIC;
            break;
    }

    httpd_MsgAdd( answer, "Server", "VLC Server" );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cseq", "%d",
                  atoi( httpd_MsgGet( query, "Cseq" ) ) );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
    {
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SDPGenerate: TODO
 * FIXME: need to be moved to a common place ?
 *****************************************************************************/
static char *SDPGenerate( vod_media_t *p_media, char *psz_destination )
{
    return strdup( "" );
}
