/*****************************************************************************
 * rtsp.c: RTSP support for RTP stream output module
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
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

#include "rtp.h"

/* For unicast/interleaved streaming */
struct rtsp_client_t
{
    char    *psz_session;
    int64_t i_last; /* for timeout */

    /* is it in "play" state */
    vlc_bool_t b_playing;

    /* output (id-access) */
    int               i_id;
    sout_stream_id_t  **id;
    int               i_access;
    sout_access_out_t **access;
};

static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl,
                          httpd_message_t *answer, httpd_message_t *query );
static int  RtspCallbackId( httpd_callback_sys_t *p_args,
                            httpd_client_t *cl,
                            httpd_message_t *answer, httpd_message_t *query );
static void RtspClientDel( sout_stream_t *p_stream, rtsp_client_t *rtsp );

int RtspSetup( sout_stream_t *p_stream, const vlc_url_t *url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_stream, "rtsp setup: %s : %d / %s\n", url->psz_host, url->i_port, url->psz_path );

    p_sys->p_rtsp_host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host, url->i_port > 0 ? url->i_port : 554 );
    if( p_sys->p_rtsp_host == NULL )
    {
        return VLC_EGENERIC;
    }

    p_sys->psz_rtsp_path = strdup( url->psz_path ? url->psz_path : "/" );
    p_sys->psz_rtsp_control = malloc (strlen( url->psz_host ) + 20 + strlen( p_sys->psz_rtsp_path ) + 1 );
    sprintf( p_sys->psz_rtsp_control, "rtsp://%s:%d%s",
             url->psz_host,  url->i_port > 0 ? url->i_port : 554, p_sys->psz_rtsp_path );

    p_sys->p_rtsp_url = httpd_UrlNewUnique( p_sys->p_rtsp_host, p_sys->psz_rtsp_path, NULL, NULL, NULL );
    if( p_sys->p_rtsp_url == NULL )
    {
        return VLC_EGENERIC;
    }
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_DESCRIBE, RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_SETUP,    RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_PLAY,     RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_PAUSE,    RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_TEARDOWN, RtspCallback, (void*)p_stream );

    return VLC_SUCCESS;
}


void RtspUnsetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    while( p_sys->i_rtsp > 0 )
        RtspClientDel( p_stream, p_sys->rtsp[0] );

    if( p_sys->p_rtsp_url )
        httpd_UrlDelete( p_sys->p_rtsp_url );

    if( p_sys->p_rtsp_host )
        httpd_HostDelete( p_sys->p_rtsp_host );
}


int RtspAddId( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char psz_urlc[strlen( p_sys->psz_rtsp_control ) + 1 + 10];

    sprintf( psz_urlc, "%s/trackID=%d", p_sys->psz_rtsp_path, p_sys->i_es );
    msg_Dbg( p_stream, "rtsp: adding %s\n", psz_urlc );
    id->p_rtsp_url = httpd_UrlNewUnique( p_sys->p_rtsp_host, psz_urlc, NULL, NULL, NULL );

    if( id->p_rtsp_url )
    {
        httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_DESCRIBE, RtspCallbackId, (void*)id );
        httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_SETUP,    RtspCallbackId, (void*)id );
        httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_PLAY,     RtspCallbackId, (void*)id );
        httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_PAUSE,    RtspCallbackId, (void*)id );
        httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_TEARDOWN, RtspCallbackId, (void*)id );
    }

    return VLC_SUCCESS;
}


void RtspDelId( sout_stream_t *p_stream, sout_stream_id_t *id )
{
   httpd_UrlDelete( id->p_rtsp_url );
}


static rtsp_client_t *RtspClientNew( sout_stream_t *p_stream, const char *psz_session )
{
    rtsp_client_t *rtsp = malloc( sizeof( rtsp_client_t ));

    rtsp->psz_session = strdup( psz_session );
    rtsp->i_last = 0;
    rtsp->b_playing = VLC_FALSE;
    rtsp->i_id = 0;
    rtsp->id = NULL;
    rtsp->i_access = 0;
    rtsp->access = NULL;

    TAB_APPEND( p_stream->p_sys->i_rtsp, p_stream->p_sys->rtsp, rtsp );

    return rtsp;
}


static rtsp_client_t *RtspClientGet( sout_stream_t *p_stream, const char *psz_session )
{
    int i;

    if( !psz_session ) return NULL;

    for( i = 0; i < p_stream->p_sys->i_rtsp; i++ )
    {
        if( !strcmp( p_stream->p_sys->rtsp[i]->psz_session, psz_session ) )
        {
            return p_stream->p_sys->rtsp[i];
        }
    }
    return NULL;
}


static void RtspClientDel( sout_stream_t *p_stream, rtsp_client_t *rtsp )
{
    int i;
    TAB_REMOVE( p_stream->p_sys->i_rtsp, p_stream->p_sys->rtsp, rtsp );

    for( i = 0; i < rtsp->i_access; i++ )
    {
        sout_AccessOutDelete( rtsp->access[i] );
    }
    if( rtsp->id )     free( rtsp->id );
    if( rtsp->access ) free( rtsp->access );

    free( rtsp->psz_session );
    free( rtsp );
}


/** Aggregate RTSP callback */
static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl,
                          httpd_message_t *answer, httpd_message_t *query )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_args;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_destination = p_sys->psz_destination;
    const char *psz_session = NULL;
    const char *psz_cseq = NULL;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    //fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    if( httpd_MsgGet( query, "Require" ) != NULL )
    {
        answer->i_status = 551;
        httpd_MsgAdd( query, "Unsupported", "%s",
                      httpd_MsgGet( query, "Require" ) );
    }
    else
    switch( query->i_type )
    {
        case HTTPD_MSG_DESCRIBE:
        {
            char *psz_sdp = SDPGenerate( p_stream, psz_destination, VLC_TRUE );

            answer->i_status = 200;
            httpd_MsgAdd( answer, "Content-Type",  "%s", "application/sdp" );
            httpd_MsgAdd( answer, "Content-Base",  "%s", p_sys->psz_rtsp_control );
            answer->p_body = (uint8_t *)psz_sdp;
            answer->i_body = strlen( psz_sdp );
            break;
        }

        case HTTPD_MSG_SETUP:
            answer->i_status = 459;
            break;

        case HTTPD_MSG_PLAY:
        {
            rtsp_client_t *rtsp;
            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );
            rtsp = RtspClientGet( p_stream, psz_session );
            if( rtsp && !rtsp->b_playing )
            {
                int i_id;
                /* FIXME */
                rtsp->b_playing = VLC_TRUE;

                vlc_mutex_lock( &p_sys->lock_es );
                for( i_id = 0; i_id < rtsp->i_id; i_id++ )
                {
                    sout_stream_id_t *id = rtsp->id[i_id];
                    int i;

                    for( i = 0; i < p_sys->i_es; i++ )
                    {
                        if( id == p_sys->es[i] )
                            break;
                    }
                    if( i >= p_sys->i_es ) continue;

                    rtp_add_sink( id, rtsp->access[i_id] );
                }
                vlc_mutex_unlock( &p_sys->lock_es );
            }
            break;
        }

        case HTTPD_MSG_PAUSE:
            answer->i_status = 405;
            httpd_MsgAdd( answer, "Allow", "DESCRIBE, PLAY, TEARDOWN" );
            break;

        case HTTPD_MSG_TEARDOWN:
        {
            rtsp_client_t *rtsp;

            /* for now only multicast so easy again */
            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );
            rtsp = RtspClientGet( p_stream, psz_session );
            if( rtsp )
            {
                int i_id;

                vlc_mutex_lock( &p_sys->lock_es );
                for( i_id = 0; i_id < rtsp->i_id; i_id++ )
                {
                    sout_stream_id_t *id = rtsp->id[i_id];
                    int i;

                    for( i = 0; i < p_sys->i_es; i++ )
                    {
                        if( id == p_sys->es[i] )
                            break;
                    }
                    if( i >= p_sys->i_es ) continue;

                    rtp_del_sink( id, rtsp->access[i_id] );
                }
                vlc_mutex_unlock( &p_sys->lock_es );

                RtspClientDel( p_stream, rtsp );
            }
            break;
        }

        default:
            return VLC_EGENERIC;
    }

    httpd_MsgAdd( answer, "Server", "%s", PACKAGE_STRING );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    psz_cseq = httpd_MsgGet( query, "Cseq" );
    if( psz_cseq )
        httpd_MsgAdd( answer, "Cseq", "%s", psz_cseq );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s;timeout=5", psz_session );
    return VLC_SUCCESS;
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


/** Non-aggregate RTSP callback */
/*static*/ int RtspCallbackId( httpd_callback_sys_t *p_args,
                               httpd_client_t *cl,
                               httpd_message_t *answer, httpd_message_t *query )
{
    sout_stream_id_t *id = (sout_stream_id_t*)p_args;
    sout_stream_t    *p_stream = id->p_stream;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char psz_session_init[21];
    const char *psz_session;
    const char *psz_cseq;

    if( answer == NULL || query == NULL )
        return VLC_SUCCESS;
    //fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    /* */
    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    /* Create new session ID if needed */
    psz_session = httpd_MsgGet( query, "Session" );
    if( psz_session == NULL )
    {
        /* FIXME: should be somewhat secure randomness */
        snprintf( psz_session_init, sizeof(psz_session_init), I64Fd,
                  NTPtime64() + rand() );
    }

    if( httpd_MsgGet( query, "Require" ) != NULL )
    {
        answer->i_status = 551;
        httpd_MsgAdd( query, "Unsupported", "%s",
                      httpd_MsgGet( query, "Require" ) );
    }
    else
    switch( query->i_type )
    {
        case HTTPD_MSG_SETUP:
        {
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
                    if( sscanf( opt, "client_port=%u-%u", &loport, &hiport ) == 2 )
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
                    {
                    /*
                     * Every other option is unsupported:
                     *
                     * "source" and "append" are invalid.
                     *
                     * For multicast, "port", "layers", "ttl" are set by the
                     * stream output configuration.
                     *
                     * For unicast, we do not allow "destination" as it
                     * carries a DoS risk, and we decide on "server_port".
                     *
                     * "interleaved" and "ssrc" are not implemented.
                     */
                        b_unsupp = VLC_TRUE;
                        break;
                    }
                }

                if( b_unsupp )
                    continue;

                if( b_multicast )
                {
                    if( p_sys->psz_destination == NULL )
                        continue;

                    answer->i_status = 200;

                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;destination=%s;port=%d-%d;"
                                  "ttl=%d;mode=play",
                                  p_sys->psz_destination, id->i_port, id->i_port+1,
                                  ( p_sys->i_ttl > 0 ) ? p_sys->i_ttl : 1 );
                }
                else
                {
                    char ip[NI_MAXNUMERICHOST], psz_access[22],
                         url[NI_MAXNUMERICHOST + 8];
                    sout_access_out_t *p_access;
                    rtsp_client_t *rtsp = NULL;

                    if( ( hiport - loport ) > 1 )
                        continue;

                    if( psz_session == NULL )
                    {
                        psz_session = psz_session_init;
                        rtsp = RtspClientNew( p_stream, psz_session );
                    }
                    else
                    {
                        /* FIXME: we probably need to remove an access out,
                         * if there is already one for the same ID */
                        rtsp = RtspClientGet( p_stream, psz_session );
                        if( rtsp == NULL )
                        {
                            answer->i_status = 454;
                            continue;
                        }
                    }

                    if( httpd_ClientIP( cl, ip ) == NULL )
                    {
                        answer->i_status = 500;
                        continue;
                    }

                    if( p_sys->i_ttl > 0 )
                        snprintf( psz_access, sizeof( psz_access ),
                                  "udp{raw,rtcp,ttl=%d}", p_sys->i_ttl );
                    else
                        strcpy( psz_access, "udp{raw,rtcp}" );

                    snprintf( url, sizeof( url ),
                              ( strchr( ip, ':' ) != NULL ) ? "[%s]:%d" : "%s:%d",
                              ip, loport );

                    p_access = sout_AccessOutNew( p_stream->p_sout,
                                                  psz_access, url );
                    if( p_access == NULL )
                    {
                        msg_Err( p_stream,
                                 "cannot create access output for %s://%s",
                                 psz_access, url );
                        answer->i_status = 500;
                        break;
                    }

                    TAB_APPEND( rtsp->i_id, rtsp->id, id );
                    TAB_APPEND( rtsp->i_access, rtsp->access, p_access );

                    char *src = var_GetNonEmptyString (p_access, "src-addr");
                    int sport = var_GetInteger (p_access, "src-port");

                    httpd_ServerIP( cl, ip );

                    if( ( src != NULL ) && strcmp( src, ip ) )
                    {
                        /* Specify source IP if it is different from the RTSP
                         * control connection server address */
                        char *ptr = strchr( src, '%' );
                        if( ptr != NULL ) *ptr = '\0'; /* remove scope ID */

                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;source=%s;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      src, loport, hiport, sport, sport + 1 );
                    }
                    else
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      loport, hiport, sport, sport + 1 );
                    }

                    answer->i_status = 200;
                    free( src );
                }
                break;
            }
            break;
        }

        default:
            answer->i_status = 460;
            break;
    }

    psz_cseq = httpd_MsgGet( query, "Cseq" );
    if( psz_cseq )
        httpd_MsgAdd( answer, "Cseq", "%s", psz_cseq );
    httpd_MsgAdd( answer, "Server", "%s", PACKAGE_STRING );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );
    return VLC_SUCCESS;
}
