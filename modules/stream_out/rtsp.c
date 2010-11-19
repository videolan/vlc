/*****************************************************************************
 * rtsp.c: RTSP support for RTP stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004, 2010 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
 *
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Pierre Ynard
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

#ifndef WIN32
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
    vod_media_t    *vod_media;
    httpd_host_t   *host;
    httpd_url_t    *url;
    char           *psz_path;
    unsigned        track_id;
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

rtsp_stream_t *RtspSetup( vlc_object_t *owner, vod_media_t *media,
                          const vlc_url_t *url )
{
    rtsp_stream_t *rtsp = malloc( sizeof( *rtsp ) );

    if( rtsp == NULL || ( url->i_port > 99999 ) )
    {
        free( rtsp );
        return NULL;
    }

    rtsp->owner = owner;
    rtsp->vod_media = media;
    rtsp->sessionc = 0;
    rtsp->sessionv = NULL;
    rtsp->host = NULL;
    rtsp->url = NULL;
    rtsp->psz_path = NULL;
    rtsp->track_id = 0;
    vlc_mutex_init( &rtsp->lock );

    rtsp->port = (url->i_port > 0) ? url->i_port : 554;
    rtsp->psz_path = strdup( ( url->psz_path != NULL ) ? url->psz_path : "/" );
    if( rtsp->psz_path == NULL )
        goto error;

    msg_Dbg( owner, "RTSP stream: host %s port %d at %s",
             url->psz_host, rtsp->port, rtsp->psz_path );

    rtsp->host = httpd_HostNew( VLC_OBJECT(owner), url->psz_host,
                                rtsp->port );
    if( rtsp->host == NULL )
        goto error;

    rtsp->url = httpd_UrlNewUnique( rtsp->host, rtsp->psz_path,
                                    NULL, NULL, NULL );
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

    while( rtsp->sessionc > 0 )
        RtspClientDel( rtsp, rtsp->sessionv[0] );

    if( rtsp->host )
        httpd_HostDelete( rtsp->host );

    free( rtsp->psz_path );
    vlc_mutex_destroy( &rtsp->lock );

    free( rtsp );
}


struct rtsp_stream_id_t
{
    rtsp_stream_t    *stream;
    sout_stream_id_t *sout_id;
    unsigned          clock_rate; /* needed to compute rtptime in RTP-Info */
    httpd_url_t      *url;
    const char       *dst;
    int               ttl;
    unsigned          track_id;
    uint32_t          ssrc;
    uint16_t          loport, hiport;
};


typedef struct rtsp_strack_t rtsp_strack_t;

/* For unicast streaming */
struct rtsp_session_t
{
    rtsp_stream_t *stream;
    uint64_t       id;
    bool           vod_started; /* true if the VoD media instance was created */

    /* output (id-access) */
    int            trackc;
    rtsp_strack_t *trackv;
};


/* Unicast session track */
struct rtsp_strack_t
{
    rtsp_stream_id_t  *id;
    sout_stream_id_t  *sout_id;
    int          setup_fd;       /* socket created by the SETUP request */
    int          rtp_fd;         /* socket used by the RTP output */
    uint32_t     ssrc;
    uint16_t     seq_init;
    bool         playing;
};

static void RtspTrackClose( rtsp_strack_t *tr );

char *RtspAppendTrackPath( rtsp_stream_id_t *id, const char *base )
{
    const char *sep = strlen( base ) > 0 && base[strlen( base ) - 1] == '/' ?
                      "" : "/";
    char *url;

    if( asprintf( &url, "%s%strackID=%u", base, sep, id->track_id ) == -1 )
        url = NULL;
    return url;
}


rtsp_stream_id_t *RtspAddId( rtsp_stream_t *rtsp, sout_stream_id_t *sid,
                             uint32_t ssrc, unsigned clock_rate,
                             /* Multicast stuff - TODO: cleanup */
                             const char *dst, int ttl,
                             unsigned loport, unsigned hiport )
{
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
    /* TODO: can we assume that this need not be strdup'd? */
    id->dst = dst;
    if( id->dst != NULL )
    {
        id->ttl = ttl;
        id->loport = loport;
        id->hiport = hiport;
    }

    urlbuf = RtspAppendTrackPath( id, rtsp->psz_path );
    if( urlbuf == NULL )
    {
        free( id );
        return NULL;
    }

    msg_Dbg( rtsp->owner, "RTSP: adding %s", urlbuf );
    url = id->url = httpd_UrlNewUnique( rtsp->host, urlbuf, NULL, NULL, NULL );
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
                REMOVE_ELEM( ses->trackv, ses->trackc, j );
            }
        }
    }

    vlc_mutex_unlock( &rtsp->lock );
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
    s->vod_started = false;
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


/* Attach a starting VoD RTP id to its RTSP track, and let it
 * initialize with the parameters of the SETUP request */
int RtspTrackAttach( rtsp_stream_t *rtsp, const char *name,
                     rtsp_stream_id_t *id, sout_stream_id_t *sout_id,
                     uint32_t *ssrc, uint16_t *seq_init )
{
    int val = VLC_EGENERIC;
    rtsp_session_t *session;

    vlc_mutex_lock(&rtsp->lock);
    session = RtspClientGet(rtsp, name);

    if (session == NULL)
        goto out;

    for (int i = 0; session->trackc; i++)
    {
        rtsp_strack_t *tr = session->trackv + i;
        if (tr->id == id)
        {
            int rtp_fd;
#if !defined(WIN32) || defined(UNDER_CE)
            rtp_fd = vlc_dup(tr->setup_fd);
#else
            WSAPROTOCOL_INFO info;
            WSADuplicateSocket (tr->setup_fd, GetCurrentProcessId (), &info);
            rtp_fd = WSASocket (info.iAddressFamily, info.iSocketType,
                                info.iProtocol, &info, 0, 0);
#endif
            if (rtp_fd == -1)
                break;

            /* Ignore any unexpected incoming packet */
            /* XXX: is this needed again? */
            setsockopt (rtp_fd, SOL_SOCKET, SO_RCVBUF, &(int){ 0 },
                        sizeof (int));

            uint16_t seq;
            *ssrc = ntohl(tr->ssrc);
            *seq_init = tr->seq_init;
            rtp_add_sink(sout_id, rtp_fd, false, &seq);
            /* To avoid race conditions, sout_id->i_seq_sent_next must
             * be set here and now. Make sure the caller did its job
             * properly when passing seq_init. */
            assert(tr->seq_init == seq);

            tr->rtp_fd = rtp_fd;
            tr->sout_id = sout_id;
            tr->playing = true;

            val = VLC_SUCCESS;
            break;
        }
    }

out:
    vlc_mutex_unlock(&rtsp->lock);
    return val;
}


/* Remove references to the RTP id when it is stopped */
void RtspTrackDetach( rtsp_stream_t *rtsp, const char *name,
                      sout_stream_id_t *sout_id )
{
    rtsp_session_t *session;

    vlc_mutex_lock(&rtsp->lock);
    session = RtspClientGet(rtsp, name);

    if (session == NULL)
        goto out;

    for (int i = 0; session->trackc; i++)
    {
        rtsp_strack_t *tr = session->trackv + i;
        if (tr->sout_id == sout_id)
        {
            tr->sout_id = NULL;
            tr->playing = false;
            rtp_del_sink(sout_id, tr->rtp_fd);
            break;
        }
    }

out:
    vlc_mutex_unlock(&rtsp->lock);
}


/** rtsp must be locked */
static void RtspTrackClose( rtsp_strack_t *tr )
{
    if (tr->sout_id != NULL)
        rtp_del_sink(tr->sout_id, tr->rtp_fd);
    /* rtp_fd is duplicated from setup_fd only in VoD mode. */
    if (tr->id->stream->vod_media != NULL)
        net_Close(tr->setup_fd);
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


static int64_t ParseNPT (const char *str)
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    unsigned hour, min;
    float sec;

    if (sscanf (str, "%u:%u:%f", &hour, &min, &sec) == 3)
        sec += ((hour * 60) + min) * 60;
    else
    if (sscanf (str, "%f", &sec) != 1)
        sec = 0.;

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return sec * CLOCK_FREQ;
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
    bool vod = rtsp->vod_media != NULL;
    time_t now;

    time (&now);

    if( answer == NULL || query == NULL || cl == NULL )
        return VLC_SUCCESS;
    else
    {
        /* Build self-referential control URL */
        char ip[NI_MAXNUMERICHOST], *ptr;

        httpd_ServerIP( cl, ip );
        ptr = strchr( ip, '%' );
        if( ptr != NULL )
            *ptr = '\0';

        if( strchr( ip, ':' ) != NULL )
            sprintf( control, "rtsp://[%s]:%u%s", ip, rtsp->port,
                     rtsp->psz_path );
        else
            sprintf( control, "rtsp://%s:%u%s", ip, rtsp->port,
                     rtsp->psz_path );
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

            answer->p_body = (uint8_t *) ( vod ?
                SDPGenerateVoD( rtsp->vod_media, control ) :
                SDPGenerate( (sout_stream_t *)owner, control ) );
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
                        b_multicast = true;
                    else
                    if( strncmp( opt, "unicast", 7 ) == 0 )
                        b_multicast = false;
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
                     * For multicast, "port", "layers", "ttl" are set by the
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
                    const char *dst = id->dst;
                    if( dst == NULL )
                        continue;

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
                                  dst, id->loport, id->hiport,
                                  ( id->ttl > 0 ) ? id->ttl : 1 );
                }
                else
                {
                    char ip[NI_MAXNUMERICHOST], src[NI_MAXNUMERICHOST];
                    rtsp_session_t *ses = NULL;
                    int fd, sport;
                    uint32_t ssrc;

                    if( httpd_ClientIP( cl, ip ) == NULL )
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

                    rtsp_strack_t track = { .id = id, .sout_id = id->sout_id,
                                            .setup_fd = fd, .playing = false };

                    if (vod)
                    {
                        vlc_rand_bytes (&track.seq_init, sizeof (track.seq_init));
                        vlc_rand_bytes (&track.ssrc, sizeof (track.ssrc));
                        ssrc = track.ssrc;
                    }
                    else
                    {
                        track.rtp_fd = track.setup_fd;
                        ssrc = id->ssrc;
                    }

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
                        /* FIXME: we probably need to remove an access out,
                         * if there is already one for the same ID */
                        ses = RtspClientGet( rtsp, psz_session );
                        if( ses == NULL )
                        {
                            answer->i_status = 454;
                            vlc_mutex_unlock( &rtsp->lock );
                            net_Close( fd );
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
                                      "ssrc=%08X;mode=play",
                                      src, loport, loport + 1, sport,
                                      sport + 1, ssrc );
                    }
                    else
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "ssrc=%08X;mode=play",
                                      loport, loport + 1, sport, sport + 1,
                                      ssrc );
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
            const char *range = httpd_MsgGet (query, "Range");
            if (range != NULL && strncmp (range, "npt=", 4))
            {
                answer->i_status = 501;
                break;
            }

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
            {
                /* The "trackID" part must match what is done in
                 * RtspAppendTrackPath() */
                /* FIXME: we really need to limit the number of tracks... */
                char info[ses->trackc * ( strlen( control )
                              + sizeof("url=/trackID=123;seq=65535;"
                                       "rtptime=4294967295, ") ) + 1];
                size_t infolen = 0;

                sout_stream_id_t *sout_id = NULL;
                if (vod)
                {
                    /* We don't keep a reference to the sout_stream_t,
                     * so we check if a sout_id is available instead.
                     * FIXME: this is broken if the stream is still
                     * running but with no track set up; but this case
                     * is already broken anyway (see below). */
                    for (int i = 0; i < ses->trackc; i++)
                    {
                        sout_id = ses->trackv[i].sout_id;
                        if (sout_id != NULL)
                            break;
                    }
                }
                int64_t ts = rtp_get_ts(vod ? NULL : (sout_stream_t *)owner,
                                        sout_id, rtsp->vod_media, psz_session);

                for( int i = 0; i < ses->trackc; i++ )
                {
                    rtsp_strack_t *tr = ses->trackv + i;
                    if( ( id == NULL ) || ( tr->id == id ) )
                    {
                        uint16_t seq;
                        if( !tr->playing )
                        {
                            if (vod)
                                /* TODO: if the RTP stream output is already
                                 * started, it won't pick up newly set-up
                                 * tracks, so we need to call rtp_add_sink()
                                 * or something. */
                                seq = tr->seq_init;
                            else
                            {
                                tr->playing = true;
                                rtp_add_sink( tr->sout_id, tr->rtp_fd,
                                              false, &seq );
                            }
                        }
                        else
                        {
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
                if (vod)
                {
                    /* TODO: fix that crap, this is barely RTSP */
                    if (!ses->vod_started)
                    {
                        vod_start(rtsp->vod_media, psz_session);
                        ses->vod_started = true;
                    }
                    else
                    {
                        if (range != NULL)
                        {
                            int64_t time = ParseNPT (range + 4);
                            vod_seek(rtsp->vod_media, psz_session, time);
                        }
                        /* This is the thing to do to unpause... */
                        vod_start(rtsp->vod_media, psz_session);
                    }
                }
            }
            vlc_mutex_unlock( &rtsp->lock );

            if( httpd_MsgGet( query, "Scale" ) != NULL )
                httpd_MsgAdd( answer, "Scale", "1." );
            break;
        }

        case HTTPD_MSG_PAUSE:
        {
            if (!vod)
            {
                answer->i_status = 405;
                httpd_MsgAdd( answer, "Allow",
                              "%s, TEARDOWN, PLAY, GET_PARAMETER",
                              ( id != NULL ) ? "SETUP" : "DESCRIBE" );
                break;
            }

            rtsp_session_t *ses;
            answer->i_status = 200;
            psz_session = httpd_MsgGet( query, "Session" );
            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if (ses != NULL)
                vod_pause(rtsp->vod_media, psz_session);
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

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
                {
                    RtspClientDel( rtsp, ses );
                    if (vod)
                        vod_stop(rtsp->vod_media, psz_session);
                }
                else /* Delete one track from the session */
                for( int i = 0; i < ses->trackc; i++ )
                {
                    if( ses->trackv[i].id == id )
                    {
                        RtspTrackClose( &ses->trackv[i] );
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
