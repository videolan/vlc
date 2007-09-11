/*****************************************************************************
 * rtsp.c: RTSP support for RTP stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
 *
 * $Id: rtp.c 21407 2007-08-22 20:10:41Z courmisch $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc_sout.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_rand.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "rtp.h"

typedef struct rtsp_session_t rtsp_session_t;

struct rtsp_stream_t
{
    vlc_mutex_t     lock;
    sout_stream_t  *owner;
    httpd_host_t   *host;
    httpd_url_t    *url;
    char           *psz_path;
    unsigned        port;

    int             sessionc;
    rtsp_session_t **sessionv;
};


static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl, httpd_message_t *answer,
                          const httpd_message_t *query );
static int  RtspCallbackId( httpd_callback_sys_t *p_args,
                            httpd_client_t *cl, httpd_message_t *answer,
                            const httpd_message_t *query );
static void RtspClientDel( rtsp_stream_t *rtsp, rtsp_session_t *session );

rtsp_stream_t *RtspSetup( sout_stream_t *p_stream, const vlc_url_t *url )
{
    rtsp_stream_t *rtsp = malloc( sizeof( *rtsp ) );

    if( rtsp == NULL || ( url->i_port > 99999 ) )
        return NULL;

    rtsp->owner = p_stream;
    rtsp->sessionc = 0;
    rtsp->sessionv = NULL;
    vlc_mutex_init( p_stream, &rtsp->lock );

    msg_Dbg( p_stream, "rtsp setup: %s : %d / %s\n",
             url->psz_host, url->i_port, url->psz_path );

    rtsp->port = (url->i_port > 0) ? url->i_port : 554;
    if( url->psz_path != NULL )
        rtsp->psz_path = strdup( url->psz_path + 1 );
    else
        rtsp->psz_path = NULL;

    rtsp->host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host,
                                rtsp->port );
    if( rtsp->host == NULL )
        goto error;

    rtsp->url = httpd_UrlNewUnique( rtsp->host,
                                    url->psz_path ? url->psz_path : "/", NULL,
                                    NULL, NULL );
    if( rtsp->url == NULL )
        goto error;

    httpd_UrlCatch( rtsp->url, HTTPD_MSG_DESCRIBE, RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_SETUP,    RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_PLAY,     RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_PAUSE,    RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_GETPARAMETER, RtspCallback,
                    (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_TEARDOWN, RtspCallback, (void*)rtsp );
    return rtsp;

error:
    RtspUnsetup( rtsp );
    return NULL;
}


void RtspUnsetup( rtsp_stream_t *rtsp )
{
    while( rtsp->sessionc > 0 )
        RtspClientDel( rtsp, rtsp->sessionv[0] );

    if( rtsp->url )
        httpd_UrlDelete( rtsp->url );

    if( rtsp->host )
        httpd_HostDelete( rtsp->host );

    vlc_mutex_destroy( &rtsp->lock );
}


struct rtsp_stream_id_t
{
    rtsp_stream_t    *stream;
    sout_stream_id_t *sout_id;
    httpd_url_t      *url;
    const char       *dst;
    int               ttl;
    unsigned          loport, hiport;
};


typedef struct rtsp_strack_t rtsp_strack_t;

/* For unicast streaming */
struct rtsp_session_t
{
    rtsp_stream_t *stream;
    uint64_t       id;

    /* output (id-access) */
    int            trackc;
    rtsp_strack_t *trackv;
};


/* Unicast session track */
struct rtsp_strack_t
{
    sout_stream_id_t  *id;
    int                fd;
    vlc_bool_t         playing;
};


rtsp_stream_id_t *RtspAddId( rtsp_stream_t *rtsp, sout_stream_id_t *sid,
                             unsigned num,
                             /* Multicast stuff - TODO: cleanup */
                             const char *dst, int ttl,
                             unsigned loport, unsigned hiport )
{
    char urlbuf[sizeof( "//trackID=123" ) + strlen( rtsp->psz_path )];
    rtsp_stream_id_t *id = malloc( sizeof( *id ) );
    httpd_url_t *url;

    if( id == NULL )
        return NULL;

    id->stream = rtsp;
    id->sout_id = sid;
    /* TODO: can we assume that this need not be strdup'd? */
    id->dst = dst;
    if( id->dst != NULL )
    {
        id->ttl = ttl;
        id->loport = loport;
        id->hiport = hiport;
    }

    snprintf( urlbuf, sizeof( urlbuf ), "/%s/trackID=%u", rtsp->psz_path,
              num );
    msg_Dbg( rtsp->owner, "RTSP: adding %s", urlbuf );
    url = id->url = httpd_UrlNewUnique( rtsp->host, urlbuf, NULL, NULL, NULL );

    if( url == NULL )
    {
        free( id );
        return NULL;
    }

    httpd_UrlCatch( url, HTTPD_MSG_DESCRIBE, RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_SETUP,    RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_PLAY,     RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_PAUSE,    RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_GETPARAMETER, RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_TEARDOWN, RtspCallbackId, (void *)id );

    return id;
}


void RtspDelId( rtsp_stream_t *rtsp, rtsp_stream_id_t *id )
{
    vlc_mutex_lock( &rtsp->lock );
    for( int i = 0; i < rtsp->sessionc; i++ )
    {
        rtsp_session_t *ses = rtsp->sessionv[i];

        for( int j = 0; j < ses->trackc; j++ )
        {
            if( ses->trackv[j].id == id->sout_id )
            {
                rtsp_strack_t *tr = ses->trackv + j;
                net_Close( tr->fd );
                REMOVE_ELEM( ses->trackv, ses->trackc, j );
            }
        }
    }

    vlc_mutex_unlock( &rtsp->lock );
    httpd_UrlDelete( id->url );
    free( id );
}


/** rtsp must be locked */
static
rtsp_session_t *RtspClientNew( rtsp_stream_t *rtsp )
{
    rtsp_session_t *s = malloc( sizeof( *s ) );
    if( s == NULL )
        return NULL;

    s->stream = rtsp;
    vlc_rand_bytes (&s->id, sizeof (s->id));
    s->trackc = 0;
    s->trackv = NULL;

    TAB_APPEND( rtsp->sessionc, rtsp->sessionv, s );

    return s;
}


/** rtsp must be locked */
static
rtsp_session_t *RtspClientGet( rtsp_stream_t *rtsp, const char *name )
{
    char *end;
    uint64_t id;
    int i;

    if( name == NULL )
        return NULL;

    errno = 0;
    id = strtoull( name, &end, 0x10 );
    if( errno || *end )
        return NULL;

    /* FIXME: use a hash/dictionary */
    for( i = 0; i < rtsp->sessionc; i++ )
    {
        if( rtsp->sessionv[i]->id == id )
            return rtsp->sessionv[i];
    }
    return NULL;
}


/** rtsp must be locked */
static
void RtspClientDel( rtsp_stream_t *rtsp, rtsp_session_t *session )
{
    int i;
    TAB_REMOVE( rtsp->sessionc, rtsp->sessionv, session );

    for( i = 0; i < session->trackc; i++ )
        rtp_del_sink( session->trackv[i].id, session->trackv[i].fd );

    free( session->trackv );
    free( session );
}


/** Finds the next transport choice */
static inline const char *transport_next( const char *str )
{
    /* Looks for comma */
    str = strchr( str, ',' );
    if( str == NULL )
        return NULL; /* No more transport options */

    str++; /* skips comma */
    while( strchr( "\r\n\t ", *str ) )
        str++;

    return (*str) ? str : NULL;
}


/** Finds the next transport parameter */
static inline const char *parameter_next( const char *str )
{
    while( strchr( ",;", *str ) == NULL )
        str++;

    return (*str == ';') ? (str + 1) : NULL;
}


/** RTSP requests handler
 * @param id selected track for non-aggregate URLs,
 *           NULL for aggregate URLs
 */
static int RtspHandler( rtsp_stream_t *rtsp, rtsp_stream_id_t *id,
                        httpd_client_t *cl,
                        httpd_message_t *answer,
                        const httpd_message_t *query )
{
    sout_stream_t *p_stream = rtsp->owner;
    char psz_sesbuf[17];
    const char *psz_session = NULL, *psz;

    if( answer == NULL || query == NULL || cl == NULL )
        return VLC_SUCCESS;

    /* */
    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    if( httpd_MsgGet( query, "Require" ) != NULL )
    {
        answer->i_status = 551;
        httpd_MsgAdd( answer, "Unsupported", "%s",
                      httpd_MsgGet( query, "Require" ) );
    }
    else
    switch( query->i_type )
    {
        case HTTPD_MSG_DESCRIBE:
        {   /* Aggregate-only */
            if( id != NULL )
            {
                answer->i_status = 460;
                break;
            }

            char ip[NI_MAXNUMERICHOST], *ptr;
            char control[sizeof("rtsp://[]:12345/") + sizeof( ip )
                            + strlen( rtsp->psz_path )];

            /* Build self-referential URL */
            httpd_ServerIP( cl, ip );
            ptr = strchr( ip, '%' );
            if( ptr != NULL )
                *ptr = '\0';

            if( strchr( ip, ':' ) != NULL )
                sprintf( control, "rtsp://[%s]:%u/%s", ip, rtsp->port,
                         ( rtsp->psz_path != NULL ) ? rtsp->psz_path : "" );
            else
                sprintf( control, "rtsp://%s:%u/%s", ip, rtsp->port,
                         ( rtsp->psz_path != NULL ) ? rtsp->psz_path : "" );

            ptr = SDPGenerate( rtsp->owner, control );

            answer->i_status = 200;
            httpd_MsgAdd( answer, "Content-Type",  "%s", "application/sdp" );
            httpd_MsgAdd( answer, "Content-Base",  "%s", control );
            answer->p_body = (uint8_t *)ptr;
            answer->i_body = strlen( ptr );
            break;
        }

        case HTTPD_MSG_SETUP:
            /* Non-aggregate-only */
            if( id == NULL )
            {
                answer->i_status = 459;
                break;
            }

            psz_session = httpd_MsgGet( query, "Session" );
            answer->i_status = 461;

            for( const char *tpt = httpd_MsgGet( query, "Transport" );
                 tpt != NULL;
                 tpt = transport_next( tpt ) )
            {
                vlc_bool_t b_multicast = VLC_TRUE, b_unsupp = VLC_FALSE;
                unsigned loport = 5004, hiport = 5005; /* from RFC3551 */

                /* Check transport protocol. */
                /* Currently, we only support RTP/AVP over UDP */
                if( strncmp( tpt, "RTP/AVP", 7 ) )
                    continue;
                tpt += 7;
                if( strncmp( tpt, "/UDP", 4 ) == 0 )
                    tpt += 4;
                if( strchr( ";,", *tpt ) == NULL )
                    continue;

                /* Parse transport options */
                for( const char *opt = parameter_next( tpt );
                     opt != NULL;
                     opt = parameter_next( opt ) )
                {
                    if( strncmp( opt, "multicast", 9 ) == 0)
                        b_multicast = VLC_TRUE;
                    else
                    if( strncmp( opt, "unicast", 7 ) == 0 )
                        b_multicast = VLC_FALSE;
                    else
                    if( sscanf( opt, "client_port=%u-%u", &loport, &hiport )
                                == 2 )
                        ;
                    else
                    if( strncmp( opt, "mode=", 5 ) == 0 )
                    {
                        if( strncasecmp( opt + 5, "play", 4 )
                         && strncasecmp( opt + 5, "\"PLAY\"", 6 ) )
                        {
                            /* Not playing?! */
                            b_unsupp = VLC_TRUE;
                            break;
                        }
                    }
                    else
                    if( strncmp( opt,"destination=", 12 ) == 0 )
                    {
                        answer->i_status = 403;
                        b_unsupp = VLC_TRUE;
                    }
                    else
                    {
                    /*
                     * Every other option is unsupported:
                     *
                     * "source" and "append" are invalid (server-only);
                     * "ssrc" also (as clarified per RFC2326bis).
                     *
                     * For multicast, "port", "layers", "ttl" are set by the
                     * stream output configuration.
                     *
                     * For unicast, we want to decide "server_port" values.
                     *
                     * "interleaved" is not implemented.
                     */
                        b_unsupp = VLC_TRUE;
                        break;
                    }
                }

                if( b_unsupp )
                    continue;

                if( b_multicast )
                {
                    const char *dst = id->dst;
                    if( dst == NULL )
                        continue;

                    if( psz_session == NULL )
                    {
                        /* Create a dummy session ID */
                        snprintf( psz_sesbuf, sizeof( psz_sesbuf ), "%d",
                                  rand() );
                        psz_session = psz_sesbuf;
                    }
                    answer->i_status = 200;

                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;destination=%s;port=%u-%u;"
                                  "ttl=%d;mode=play",
                                  dst, id->loport, id->hiport,
                                  ( id->ttl > 0 ) ? id->ttl : 1 );
                }
                else
                {
                    char ip[NI_MAXNUMERICHOST], src[NI_MAXNUMERICHOST];
                    rtsp_session_t *ses = NULL;
                    rtsp_strack_t track = { id->sout_id, -1, VLC_FALSE };
                    int sport;

                    if( httpd_ClientIP( cl, ip ) == NULL )
                    {
                        answer->i_status = 500;
                        continue;
                    }

                    track.fd = net_ConnectDgram( p_stream, ip, loport, -1,
                                                 IPPROTO_UDP );
                    if( track.fd == -1 )
                    {
                        msg_Err( p_stream,
                                 "cannot create RTP socket for %s port %u",
                                 ip, loport );
                        answer->i_status = 500;
                        continue;
                    }

                    net_GetSockAddress( track.fd, src, &sport );

                    vlc_mutex_lock( &rtsp->lock );
                    if( psz_session == NULL )
                    {
                        ses = RtspClientNew( rtsp );
                        snprintf( psz_sesbuf, sizeof( psz_sesbuf ), I64Fx,
                                  ses->id );
                        psz_session = psz_sesbuf;
                    }
                    else
                    {
                        /* FIXME: we probably need to remove an access out,
                         * if there is already one for the same ID */
                        ses = RtspClientGet( rtsp, psz_session );
                        if( ses == NULL )
                        {
                            answer->i_status = 454;
                            vlc_mutex_unlock( &rtsp->lock );
                            continue;
                        }
                    }

                    INSERT_ELEM( ses->trackv, ses->trackc, ses->trackc,
                                 track );
                    vlc_mutex_unlock( &rtsp->lock );

                    httpd_ServerIP( cl, ip );

                    if( strcmp( src, ip ) )
                    {
                        /* Specify source IP if it is different from the RTSP
                         * control connection server address */
                        char *ptr = strchr( src, '%' );
                        if( ptr != NULL ) *ptr = '\0'; /* remove scope ID */

                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;source=%s;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      src, loport, loport + 1, sport,
                                      sport + 1 );
                    }
                    else
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      loport, loport + 1, sport, sport + 1 );
                    }

                    answer->i_status = 200;
                }
                break;
            }
            break;

        case HTTPD_MSG_PLAY:
        {
            rtsp_session_t *ses;
            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );
            if( httpd_MsgGet( query, "Range" ) != NULL )
            {
                answer->i_status = 456; /* cannot seek */
                break;
            }

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
            {
                for( int i = 0; i < ses->trackc; i++ )
                {
                    rtsp_strack_t *tr = ses->trackv + i;
                    if( !tr->playing
                     && ( ( id == NULL ) || ( tr->id == id->sout_id ) ) )
                    {
                        tr->playing = VLC_TRUE;
                        rtp_add_sink( tr->id, tr->fd );
                    }
                }
            }
            vlc_mutex_unlock( &rtsp->lock );

            if( httpd_MsgGet( query, "Scale" ) != NULL )
                httpd_MsgAdd( answer, "Scale", "1." );
            break;
        }

        case HTTPD_MSG_PAUSE:
            answer->i_status = 405;
            httpd_MsgAdd( answer, "Allow",
                          "%s, TEARDOWN, PLAY, GET_PARAMETER",
                          ( id != NULL ) ? "SETUP" : "DESCRIBE" );
            break;

        case HTTPD_MSG_GETPARAMETER:
            if( query->i_body > 0 )
            {
                answer->i_status = 451;
                break;
            }

            psz_session = httpd_MsgGet( query, "Session" );
            answer->i_status = 200;
            break;

        case HTTPD_MSG_TEARDOWN:
        {
            rtsp_session_t *ses;

            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
            {
                if( id == NULL ) /* Delete the entire session */
                    RtspClientDel( rtsp, ses );
                else /* Delete one track from the session */
                for( int i = 0; i < ses->trackc; i++ )
                {
                    if( ses->trackv[i].id == id->sout_id )
                    {
                        rtp_del_sink( id->sout_id, ses->trackv[i].fd );
                        REMOVE_ELEM( ses->trackv, ses->trackc, i );
                    }
                }
            }
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        default:
            return VLC_EGENERIC;
    }

    httpd_MsgAdd( answer, "Server", "%s", PACKAGE_STRING );
    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );

    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cache-Control", "no-cache" );

    psz = httpd_MsgGet( query, "Cseq" );
    if( psz != NULL )
        httpd_MsgAdd( answer, "Cseq", "%s", psz );
    psz = httpd_MsgGet( query, "Timestamp" );
    if( psz != NULL )
        httpd_MsgAdd( answer, "Timestamp", "%s", psz );

    return VLC_SUCCESS;
}


/** Aggregate RTSP callback */
static int RtspCallback( httpd_callback_sys_t *p_args,
                         httpd_client_t *cl,
                         httpd_message_t *answer,
                         const httpd_message_t *query )
{
    return RtspHandler( (rtsp_stream_t *)p_args, NULL, cl, answer, query );
}


/** Non-aggregate RTSP callback */
static int RtspCallbackId( httpd_callback_sys_t *p_args,
                           httpd_client_t *cl,
                           httpd_message_t *answer,
                           const httpd_message_t *query )
{
    rtsp_stream_id_t *id = (rtsp_stream_id_t *)p_args;
    return RtspHandler( id->stream, id, cl, answer, query );
}
