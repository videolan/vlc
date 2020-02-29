/*****************************************************************************
 * rtsp.c: RTSP support for RTP stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004, 2010 VLC authors and VideoLAN
 * Copyright © 2007 Rémi Denis-Courmont
 *
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Pierre Ynard
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_sout.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_rand.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
# include <locale.h>
#endif
#ifdef HAVE_XLOCALE_H
# include <xlocale.h>
#endif

#include "rtp.h"

typedef struct rtsp_session_t rtsp_session_t;

struct rtsp_stream_t
{
    vlc_mutex_t     lock;
    vlc_object_t   *owner;
    httpd_host_t   *host;
    httpd_url_t    *url;
    char           *psz_path;
    unsigned        track_id;

    int             sessionc;
    rtsp_session_t **sessionv;

    vlc_tick_t      timeout;
    vlc_timer_t     timer;
};


static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl, httpd_message_t *answer,
                          const httpd_message_t *query );
static int  RtspCallbackId( httpd_callback_sys_t *p_args,
                            httpd_client_t *cl, httpd_message_t *answer,
                            const httpd_message_t *query );
static void RtspClientDel( rtsp_stream_t *rtsp, rtsp_session_t *session );

static void RtspTimeOut( void *data );

rtsp_stream_t *RtspSetup( vlc_object_t *owner, const char *path )
{
    rtsp_stream_t *rtsp = calloc( 1, sizeof( *rtsp ) );

    if( unlikely(rtsp == NULL) )
        return NULL;

    rtsp->owner = owner;
    vlc_mutex_init( &rtsp->lock );

    rtsp->timeout = vlc_tick_from_sec(__MAX(0,var_InheritInteger(owner, "rtsp-timeout")));
    if (rtsp->timeout != 0)
    {
        if (vlc_timer_create(&rtsp->timer, RtspTimeOut, rtsp))
            goto error;
    }

    rtsp->psz_path = strdup( (path != NULL) ? path : "/" );
    if( rtsp->psz_path == NULL )
        goto error;

    msg_Dbg( owner, "RTSP stream at %s", rtsp->psz_path );

    rtsp->host = vlc_rtsp_HostNew( VLC_OBJECT(owner) );
    if( rtsp->host == NULL )
        goto error;

    char *user = var_InheritString(owner, "sout-rtsp-user");
    char *pwd = var_InheritString(owner, "sout-rtsp-pwd");

    rtsp->url = httpd_UrlNew( rtsp->host, rtsp->psz_path, user, pwd );
    free(user);
    free(pwd);
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
    if( rtsp->url )
        httpd_UrlDelete( rtsp->url );

    if( rtsp->host )
        httpd_HostDelete( rtsp->host );

    while( rtsp->sessionc > 0 )
        RtspClientDel( rtsp, rtsp->sessionv[0] );

    if (rtsp->timeout != 0)
        vlc_timer_destroy(rtsp->timer);

    free( rtsp->psz_path );
    free( rtsp );
}


struct rtsp_stream_id_t
{
    rtsp_stream_t    *stream;
    sout_stream_id_sys_t *sout_id;
    httpd_url_t      *url;
    unsigned          track_id;
    uint32_t          ssrc;
    unsigned          clock_rate; /* needed to compute rtptime in RTP-Info */
    int               mcast_fd;
};


typedef struct rtsp_strack_t rtsp_strack_t;

/* For unicast streaming */
struct rtsp_session_t
{
    rtsp_stream_t *stream;
    uint64_t       id;
    vlc_tick_t     last_seen; /* for timeouts */

    /* output (id-access) */
    int            trackc;
    rtsp_strack_t *trackv;
};


/* Unicast session track */
struct rtsp_strack_t
{
    rtsp_stream_id_t  *id;
    sout_stream_id_sys_t  *sout_id;
    int          setup_fd;  /* socket created by the SETUP request */
    int          rtp_fd;    /* socket used by the RTP output, when playing */
    uint32_t     ssrc;
    uint16_t     seq_init;
};

static void RtspTrackClose( rtsp_strack_t *tr );

#define TRACK_PATH_SIZE (sizeof("/trackID=999") - 1)

char *RtspAppendTrackPath( rtsp_stream_id_t *id, const char *base )
{
    const char *sep = strlen( base ) > 0 && base[strlen( base ) - 1] == '/' ?
                      "" : "/";
    char *url;

    if( asprintf( &url, "%s%strackID=%u", base, sep, id->track_id ) == -1 )
        url = NULL;
    return url;
}


rtsp_stream_id_t *RtspAddId( rtsp_stream_t *rtsp, sout_stream_id_sys_t *sid,
                             uint32_t ssrc, unsigned clock_rate,
                             int mcast_fd)
{
    if (rtsp->track_id > 999)
    {
        msg_Err(rtsp->owner, "RTSP: too many IDs!");
        return NULL;
    }

    char *urlbuf;
    rtsp_stream_id_t *id = malloc( sizeof( *id ) );
    httpd_url_t *url;

    if( id == NULL )
        return NULL;

    id->stream = rtsp;
    id->sout_id = sid;
    id->track_id = rtsp->track_id;
    id->ssrc = ssrc;
    id->clock_rate = clock_rate;
    id->mcast_fd = mcast_fd;

    urlbuf = RtspAppendTrackPath( id, rtsp->psz_path );
    if( urlbuf == NULL )
    {
        free( id );
        return NULL;
    }

    msg_Dbg( rtsp->owner, "RTSP: adding %s", urlbuf );

    char *user = var_InheritString(rtsp->owner, "sout-rtsp-user");
    char *pwd = var_InheritString(rtsp->owner, "sout-rtsp-pwd");

    url = id->url = httpd_UrlNew( rtsp->host, urlbuf, user, pwd );
    free( user );
    free( pwd );
    free( urlbuf );

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

    rtsp->track_id++;

    return id;
}


void RtspDelId( rtsp_stream_t *rtsp, rtsp_stream_id_t *id )
{
    httpd_UrlDelete( id->url );

    vlc_mutex_lock( &rtsp->lock );
    for( int i = 0; i < rtsp->sessionc; i++ )
    {
        rtsp_session_t *ses = rtsp->sessionv[i];

        for( int j = 0; j < ses->trackc; j++ )
        {
            if( ses->trackv[j].id == id )
            {
                rtsp_strack_t *tr = ses->trackv + j;
                RtspTrackClose( tr );
                TAB_ERASE(ses->trackc, ses->trackv, j);
            }
        }
    }

    vlc_mutex_unlock( &rtsp->lock );
    free( id );
}


/** rtsp must be locked */
static void RtspUpdateTimer( rtsp_stream_t *rtsp )
{
    if (rtsp->timeout == 0)
        return;

    vlc_tick_t timeout = 0;
    for (int i = 0; i < rtsp->sessionc; i++)
    {
        if (timeout == 0 || rtsp->sessionv[i]->last_seen < timeout)
            timeout = rtsp->sessionv[i]->last_seen;
    }
    if (timeout != 0)
    {
        timeout += rtsp->timeout;
        vlc_timer_schedule(rtsp->timer, true, timeout, VLC_TIMER_FIRE_ONCE);
    }
    else
    {
        vlc_timer_disarm(rtsp->timer);
    }
}


static void RtspTimeOut( void *data )
{
    rtsp_stream_t *rtsp = data;

    vlc_mutex_lock(&rtsp->lock);
    vlc_tick_t now = vlc_tick_now();
    for (int i = rtsp->sessionc - 1; i >= 0; i--)
        if (rtsp->sessionv[i]->last_seen + rtsp->timeout < now)
            RtspClientDel(rtsp, rtsp->sessionv[i]);
    RtspUpdateTimer(rtsp);
    vlc_mutex_unlock(&rtsp->lock);
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
        RtspTrackClose( &session->trackv[i] );

    free( session->trackv );
    free( session );
}


/** rtsp must be locked */
static void RtspClientAlive( rtsp_session_t *session )
{
    if (session->stream->timeout == 0)
        return;

    session->last_seen = vlc_tick_now();
    RtspUpdateTimer(session->stream);
}

static int dup_socket(int oldfd)
{
    int newfd;
#ifndef _WIN32
    newfd = vlc_dup(oldfd);
#else
    WSAPROTOCOL_INFO info;
    WSADuplicateSocket (oldfd, GetCurrentProcessId (), &info);
    newfd = WSASocket (info.iAddressFamily, info.iSocketType,
                       info.iProtocol, &info, 0, 0);
#endif
    return newfd;
}

/** rtsp must be locked */
static void RtspTrackClose( rtsp_strack_t *tr )
{
    if (tr->setup_fd != -1)
    {
        if (tr->rtp_fd != -1)
        {
            rtp_del_sink(tr->sout_id, tr->rtp_fd);
            tr->rtp_fd = -1;
        }
        net_Close(tr->setup_fd);
        tr->setup_fd = -1;
    }
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


static vlc_tick_t ParseNPT (const char *str)
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    unsigned hour, min;
    float sec;

    if (sscanf (str, "%u:%u:%f", &hour, &min, &sec) == 3)
        sec += ((hour * 60) + min) * 60;
    else
    if (sscanf (str, "%f", &sec) != 1)
        sec = -1;

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return sec < 0 ? -1 : vlc_tick_from_sec( sec );
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
    vlc_object_t *owner = rtsp->owner;
    char psz_sesbuf[17];
    const char *psz_session = NULL, *psz;
    char control[sizeof("rtsp://[]:12345") + NI_MAXNUMERICHOST
                  + strlen( rtsp->psz_path )];
    time_t now;

    time (&now);

    if( answer == NULL || query == NULL || cl == NULL )
        return VLC_SUCCESS;
    else
    {
        /* Build self-referential control URL */
        char ip[NI_MAXNUMERICHOST], *ptr;
        int port;

        httpd_ServerIP( cl, ip, &port );
        ptr = strchr( ip, '%' );
        if( ptr != NULL )
            *ptr = '\0';

        if( strchr( ip, ':' ) != NULL )
            sprintf( control, "rtsp://[%s]:%d%s", ip, port, rtsp->psz_path );
        else
            sprintf( control, "rtsp://%s:%d%s", ip, port, rtsp->psz_path );
    }

    /* */
    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= 0;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    httpd_MsgAdd( answer, "Server", "VLC/%s", VERSION );

    /* Date: is always allowed, and sometimes mandatory with RTSP/2.0. */
    struct tm ut;
    if (gmtime_r (&now, &ut) != NULL)
    {   /* RFC1123 format, GMT is mandatory */
        static const char wdays[7][4] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static const char mons[12][4] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        httpd_MsgAdd (answer, "Date", "%s, %02u %s %04u %02u:%02u:%02u GMT",
                      wdays[ut.tm_wday], ut.tm_mday, mons[ut.tm_mon],
                      1900 + ut.tm_year, ut.tm_hour, ut.tm_min, ut.tm_sec);
    }

    if( query->i_proto != HTTPD_PROTO_RTSP )
    {
        answer->i_status = 505;
    }
    else
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

            answer->i_status = 200;
            httpd_MsgAdd( answer, "Content-Type",  "%s", "application/sdp" );
            httpd_MsgAdd( answer, "Content-Base",  "%s", control );

            answer->p_body = (uint8_t *)
                SDPGenerate( (sout_stream_t *)owner, control );
            if( answer->p_body != NULL )
                answer->i_body = strlen( (char *)answer->p_body );
            else
                answer->i_status = 500;
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
                bool b_multicast = true, b_unsupp = false;
                bool b_multicast_port_set = false;
                unsigned loport = 5004, hiport; /* from RFC3551 */
                unsigned mloport = 5004, mhiport = mloport + 1;

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
                        b_multicast = true;
                    else
                    if( strncmp( opt, "unicast", 7 ) == 0 )
                        b_multicast = false;
                    else
                    if( sscanf( opt, "client_port=%u-%u", &loport, &hiport )
                                == 2 )
                        ;
                    else
                    if( sscanf( opt, "port=%u-%u", &mloport, &mhiport )
                                == 2 )
                        b_multicast_port_set = true;
                    else
                    if( strncmp( opt, "mode=", 5 ) == 0 )
                    {
                        if( strncasecmp( opt + 5, "play", 4 )
                         && strncasecmp( opt + 5, "\"PLAY\"", 6 ) )
                        {
                            /* Not playing?! */
                            b_unsupp = true;
                            break;
                        }
                    }
                    else
                    if( strncmp( opt,"destination=", 12 ) == 0 )
                    {
                        answer->i_status = 403;
                        b_unsupp = true;
                    }
                    else
                    {
                    /*
                     * Every other option is unsupported:
                     *
                     * "source" and "append" are invalid (server-only);
                     * "ssrc" also (as clarified per RFC2326bis).
                     *
                     * For multicast, "layers", "ttl" are set by the
                     * stream output configuration.
                     *
                     * For unicast, we want to decide "server_port" values.
                     *
                     * "interleaved" is not implemented.
                     */
                        b_unsupp = true;
                        break;
                    }
                }

                if( b_unsupp )
                    continue;

                if( b_multicast )
                {
                    char dst[NI_MAXNUMERICHOST];
                    int dport, ttl;
                    if( id->mcast_fd == -1 )
                        continue;

                    net_GetPeerAddress(id->mcast_fd, dst, &dport);

                    /* Checking for multicast port override */
                    if( b_multicast_port_set
                     && ((unsigned)dport != mloport
                      || (unsigned)dport + 1 != mhiport))
                    {
                        answer->i_status = 551;
                        continue;
                    }

                    ttl = var_InheritInteger(owner, "ttl");
                    if (ttl <= 0)
                    /* FIXME: the TTL is left to the OS default, we can
                     * only guess that it's 1. */
                        ttl = 1;

                    if( psz_session == NULL )
                    {
                        /* Create a dummy session ID */
                        snprintf( psz_sesbuf, sizeof( psz_sesbuf ), "%lu",
                                  vlc_mrand48() );
                        psz_session = psz_sesbuf;
                    }
                    answer->i_status = 200;

                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;destination=%s;port=%u-%u;"
                                  "ttl=%d;mode=play",
                                  dst, dport, dport + 1, ttl );
                     /* FIXME: this doesn't work with RTP + RTCP mux */
                }
                else
                {
                    char ip[NI_MAXNUMERICHOST], src[NI_MAXNUMERICHOST];
                    rtsp_session_t *ses = NULL;
                    int fd, sport;
                    uint32_t ssrc;

                    if( httpd_ClientIP( cl, ip, NULL ) == NULL )
                    {
                        answer->i_status = 500;
                        continue;
                    }

                    fd = net_ConnectDgram( owner, ip, loport, -1,
                                           IPPROTO_UDP );
                    if( fd == -1 )
                    {
                        msg_Err( owner,
                                 "cannot create RTP socket for %s port %u",
                                 ip, loport );
                        answer->i_status = 500;
                        continue;
                    }

                    /* Ignore any unexpected incoming packet */
                    setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &(int){ 0 },
                                sizeof (int));
                    net_GetSockAddress( fd, src, &sport );

                    vlc_mutex_lock( &rtsp->lock );
                    if( psz_session == NULL )
                    {
                        ses = RtspClientNew( rtsp );
                        snprintf( psz_sesbuf, sizeof( psz_sesbuf ), "%"PRIx64,
                                  ses->id );
                        psz_session = psz_sesbuf;
                    }
                    else
                    {
                        ses = RtspClientGet( rtsp, psz_session );
                        if( ses == NULL )
                        {
                            answer->i_status = 454;
                            vlc_mutex_unlock( &rtsp->lock );
                            net_Close( fd );
                            continue;
                        }
                    }
                    RtspClientAlive(ses);

                    rtsp_strack_t *tr = NULL;
                    for (int i = 0; i < ses->trackc; i++)
                    {
                        if (ses->trackv[i].id == id)
                        {
                            tr = ses->trackv + i;
                            break;
                        }
                    }

                    if (tr == NULL)
                    {
                        /* Set up a new track */
                        rtsp_strack_t track = { .id = id,
                                                .sout_id = id->sout_id,
                                                .setup_fd = fd,
                                                .rtp_fd = -1 };

                        ssrc = id->ssrc;
                        TAB_APPEND(ses->trackc, ses->trackv, track);
                    }
                    else if (tr->setup_fd == -1)
                    {
                        /* The track was not SETUP, but it exists
                         * because there is a sout_id running for it */
                        tr->setup_fd = fd;
                        ssrc = tr->ssrc;
                    }
                    else
                    {
                        /* The track is already set up, and we don't
                         * support changing the transport parameters on
                         * the fly */
                        vlc_mutex_unlock( &rtsp->lock );
                        answer->i_status = 455;
                        net_Close( fd );
                        break;
                    }
                    vlc_mutex_unlock( &rtsp->lock );

                    httpd_ServerIP( cl, ip, NULL );

                    /* Specify source IP only if it is different from the
                     * RTSP control connection server address */
                    if( strcmp( src, ip ) )
                    {
                        char *ptr = strchr( src, '%' );
                        if( ptr != NULL ) *ptr = '\0'; /* remove scope ID */
                    }
                    else
                        src[0] = '\0';

                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;unicast%s%s;"
                                  "client_port=%u-%u;server_port=%u-%u;"
                                  "ssrc=%08X;mode=play",
                                  src[0] ? ";source=" : "", src,
                                  loport, loport + 1, sport, sport + 1, ssrc );

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
            vlc_tick_t start = -1, end = -1, npt;
            const char *range = httpd_MsgGet (query, "Range");
            if (range != NULL)
            {
                if (strncmp (range, "npt=", 4))
                {
                    answer->i_status = 501;
                    break;
                }

                start = ParseNPT (range + 4);
                range = strchr(range, '-');
                if (range != NULL && *(range + 1))
                    end = ParseNPT (range + 1);

                if (end >= 0 && end < start)
                {
                    answer->i_status = 457;
                    break;
                }

                /* We accept start times of 0 even for broadcast streams
                 * that already started */
                else if (start > 0 || end >= 0)
                {
                    answer->i_status = 456;
                    break;
                }
            }
            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
            {
                char info[ses->trackc * ( strlen( control ) + TRACK_PATH_SIZE
                          + sizeof("url=;seq=65535;rtptime=4294967295, ")
                                          - 1 ) + 1];
                size_t infolen = 0;
                RtspClientAlive(ses);

                sout_stream_id_sys_t *sout_id = NULL;
                vlc_tick_t ts = rtp_get_ts((sout_stream_t *)owner,
                                           sout_id, &npt);

                for( int i = 0; i < ses->trackc; i++ )
                {
                    rtsp_strack_t *tr = ses->trackv + i;
                    if( ( id == NULL ) || ( tr->id == id ) )
                    {
                        if (tr->setup_fd == -1)
                            /* Track not SETUP */
                            continue;

                        uint16_t seq;
                        if( tr->rtp_fd == -1 )
                        {
                            /* Track not PLAYing yet */
                            if (tr->sout_id == NULL)
                                /* Instance not running yet (VoD) */
                                seq = tr->seq_init;
                            else
                            {
                                /* Instance running, add a sink to it */
                                tr->rtp_fd = dup_socket(tr->setup_fd);
                                if (tr->rtp_fd == -1)
                                    continue;

                                rtp_add_sink( tr->sout_id, tr->rtp_fd,
                                              false, &seq );
                            }
                        }
                        else
                        {
                            /* Track already playing */
                            assert( tr->sout_id != NULL );
                            seq = rtp_get_seq( tr->sout_id );
                        }
                        char *url = RtspAppendTrackPath( tr->id, control );
                        infolen += sprintf( info + infolen,
                                    "url=%s;seq=%u;rtptime=%u, ",
                                    url != NULL ? url : "", seq,
                                    rtp_compute_ts( tr->id->clock_rate, ts ) );
                        free( url );
                    }
                }
                if( infolen > 0 )
                {
                    info[infolen - 2] = '\0'; /* remove trailing ", " */
                    httpd_MsgAdd( answer, "RTP-Info", "%s", info );
                }
            }
            vlc_mutex_unlock( &rtsp->lock );

            if (ses != NULL)
            {
                double f_npt = secf_from_vlc_tick(npt);
                httpd_MsgAdd( answer, "Range", "npt=%f-", f_npt );
            }

            if( httpd_MsgGet( query, "Scale" ) != NULL )
                httpd_MsgAdd( answer, "Scale", "1." );
            break;
        }

        case HTTPD_MSG_PAUSE:
        {
            if (id == NULL)
            {
                answer->i_status = 405;
                httpd_MsgAdd( answer, "Allow",
                              "DESCRIBE, TEARDOWN, PLAY, GET_PARAMETER" );
                break;
            }

            rtsp_session_t *ses;
            answer->i_status = 200;
            psz_session = httpd_MsgGet( query, "Session" );
            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if (ses != NULL)
            {
                if (id != NULL) /* "Mute" the selected track */
                {
                    bool found = false;
                    for (int i = 0; i < ses->trackc; i++)
                    {
                        rtsp_strack_t *tr = ses->trackv + i;;
                        if (tr->id == id)
                        {
                            if (tr->setup_fd == -1)
                                break;

                            found = true;
                            if (tr->rtp_fd != -1)
                            {
                                rtp_del_sink(tr->sout_id, tr->rtp_fd);
                                tr->rtp_fd = -1;
                            }
                            break;
                        }
                    }
                    if (!found)
                        answer->i_status = 455;
                }
                RtspClientAlive(ses);
            }
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        case HTTPD_MSG_GETPARAMETER:
        {
            if( query->i_body > 0 )
            {
                answer->i_status = 451;
                break;
            }

            psz_session = httpd_MsgGet( query, "Session" );
            answer->i_status = 200;
            vlc_mutex_lock( &rtsp->lock );
            rtsp_session_t *ses = RtspClientGet( rtsp, psz_session );
            if (ses != NULL)
                RtspClientAlive(ses);
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

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
                {
                    RtspClientDel( rtsp, ses );
                    RtspUpdateTimer(rtsp);
                }
                else /* Delete one track from the session */
                {
                    for( int i = 0; i < ses->trackc; i++ )
                    {
                        if( ses->trackv[i].id == id )
                        {
                            RtspTrackClose( &ses->trackv[i] );
                            TAB_ERASE(ses->trackc, ses->trackv, i);
                        }
                    }
                    RtspClientAlive(ses);
                }
            }
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        default:
            return VLC_EGENERIC;
    }

    if( psz_session )
    {
        if (rtsp->timeout != 0)
            httpd_MsgAdd( answer, "Session", "%s;timeout=%" PRIu64, psz_session,
                                                              SEC_FROM_VLC_TICK(rtsp->timeout) );
        else
            httpd_MsgAdd( answer, "Session", "%s", psz_session );
    }

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
