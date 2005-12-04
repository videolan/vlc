/*****************************************************************************
 * rtsp.c: rtsp VoD server module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
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
    "You can set the address, port and path the rtsp interface will bind to." \
    "\nSyntax is address:port/path. Default is to bind to any address "\
    "on port 554, with no path." )
vlc_module_begin();
    set_shortname( _("RTSP VoD" ) );
    set_description( _("RTSP VoD server") );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_VOD );
    set_capability( "vod server", 1 );
    set_callbacks( Open, Close );
    add_shortcut( "rtsp" );
    add_string ( "rtsp-host", NULL, NULL, HOST_TEXT, HOST_LONGTEXT, VLC_TRUE );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

typedef struct media_es_t media_es_t;

typedef struct
{
    media_es_t *p_media_es;
    char *psz_ip;
    int i_port;

} rtsp_client_es_t;

typedef struct
{
    char *psz_session;
    int64_t i_last; /* for timeout */

    vlc_bool_t b_playing; /* is it in "play" state */
    vlc_bool_t b_paused; /* is it in "pause" state */

    int i_es;
    rtsp_client_es_t **es;

} rtsp_client_t;

struct media_es_t
{
    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    httpd_url_t *p_rtsp_url;

    vod_media_t *p_media;

    es_format_t fmt;
    int         i_port;
    uint8_t     i_payload_type;
    char        *psz_rtpmap;
    char        *psz_fmtp;

};

struct vod_media_t
{
    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    httpd_url_t  *p_rtsp_url;
    char         *psz_rtsp_control;
    char         *psz_rtsp_path;

    int  i_port;
    int  i_port_audio;
    int  i_port_video;
    int  i_ttl;
    int  i_payload_type;

    int64_t i_sdp_id;
    int     i_sdp_version;

    vlc_bool_t b_multicast;

    vlc_mutex_t lock;

    /* ES list */
    int        i_es;
    media_es_t **es;
    char       *psz_mux;

    /* RTSP client */
    int           i_rtsp;
    rtsp_client_t **rtsp;

    /* Infos */
    char *psz_session_name;
    char *psz_session_description;
    char *psz_session_url;
    char *psz_session_email;
    mtime_t i_length;
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
static int RtspCallbackES( httpd_callback_sys_t *, httpd_client_t *,
                           httpd_message_t *, httpd_message_t * );

static char *SDPGenerate( const vod_media_t *, httpd_client_t *cl );

static void sprintf_hexa( char *s, uint8_t *p_data, int i_data )
{
    static const char hex[16] = "0123456789abcdef";
    int i;

    for( i = 0; i < i_data; i++ )
    {
        s[2*i+0] = hex[(p_data[i]>>4)&0xf];
        s[2*i+1] = hex[(p_data[i]   )&0xf];
    }
    s[2*i_data] = '\0';
}

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

    p_sys->psz_host = strdup( url.psz_host ? url.psz_host : "0.0.0.0" );
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

    free( p_sys->psz_host );
    free( p_sys->psz_path );
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
    p_media->es = 0;
    p_media->psz_mux = 0;
    p_media->rtsp = 0;

    asprintf( &p_media->psz_rtsp_path, "%s%s", p_sys->psz_path, psz_name );
    p_media->p_rtsp_url =
        httpd_UrlNewUnique( p_sys->p_rtsp_host, p_media->psz_rtsp_path, NULL,
                            NULL, NULL );

    if( !p_media->p_rtsp_url )
    {
        msg_Err( p_vod, "cannot create http url (%s)", p_media->psz_rtsp_path);
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

    vlc_mutex_init( p_vod, &p_media->lock );
    p_media->psz_session_name = strdup("");
    p_media->psz_session_description = strdup("");
    p_media->psz_session_url = strdup("");
    p_media->psz_session_email = strdup("");

    p_media->i_port_audio = 1234;
    p_media->i_port_video = 1236;
    p_media->i_port       = 1238;
    p_media->i_payload_type = 96;

    p_media->i_sdp_id = mdate();
    p_media->i_sdp_version = 1;
    p_media->i_length = p_item->i_duration;

    vlc_mutex_lock( &p_item->lock );
    msg_Dbg( p_vod, "media has %i declared ES", p_item->i_es );
    for( i = 0; i < p_item->i_es; i++ )
    {
        MediaAddES( p_vod, p_media, p_item->es[i] );
    }
    vlc_mutex_unlock( &p_item->lock );

    return p_media;
}

static void MediaDel( vod_t *p_vod, vod_media_t *p_media )
{
    vod_sys_t *p_sys = p_vod->p_sys;

    msg_Dbg( p_vod, "deleting media: %s", p_media->psz_rtsp_path );

    while( p_media->i_rtsp > 0 ) RtspClientDel( p_media, p_media->rtsp[0] );
    httpd_UrlDelete( p_media->p_rtsp_url );
    if( p_media->psz_rtsp_path ) free( p_media->psz_rtsp_path );
    if( p_media->psz_rtsp_control ) free( p_media->psz_rtsp_control );

    TAB_REMOVE( p_sys->i_media, p_sys->media, p_media );

    while( p_media->i_es ) MediaDelES( p_vod, p_media, &p_media->es[0]->fmt );

    vlc_mutex_destroy( &p_media->lock );
    free( p_media->psz_session_name );
    free( p_media->psz_session_description );
    free( p_media->psz_session_url );
    free( p_media->psz_session_email );
    free( p_media );
}

static int MediaAddES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt )
{
    media_es_t *p_es = malloc( sizeof(media_es_t) );
    char *psz_urlc;

    memset( p_es, 0, sizeof(media_es_t) );
    p_media->psz_mux = NULL;

    /* TODO: update SDP, etc... */
    asprintf( &psz_urlc, "%s/trackid=%d",
              p_media->psz_rtsp_path, p_media->i_es );
    msg_Dbg( p_vod, "  - ES %4.4s (%s)", (char *)&p_fmt->i_codec, psz_urlc );

    switch( p_fmt->i_codec )
    {
    case VLC_FOURCC( 's', '1', '6', 'b' ):
        if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 44100 )
        {
            p_es->i_payload_type = 11;
        }
        else if( p_fmt->audio.i_channels == 2 && p_fmt->audio.i_rate == 44100 )
        {
            p_es->i_payload_type = 10;
        }
        else
        {
            p_es->i_payload_type = p_media->i_payload_type++;
        }

        p_es->psz_rtpmap = malloc( strlen( "L16/*/*" ) + 20+1 );
        sprintf( p_es->psz_rtpmap, "L16/%d/%d", p_fmt->audio.i_rate,
                 p_fmt->audio.i_channels );
        break;
    case VLC_FOURCC( 'u', '8', ' ', ' ' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = malloc( strlen( "L8/*/*" ) + 20+1 );
        sprintf( p_es->psz_rtpmap, "L8/%d/%d", p_fmt->audio.i_rate,
                 p_fmt->audio.i_channels );
        break;
    case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
        p_es->i_payload_type = 14;
        p_es->psz_rtpmap = strdup( "MPA/90000" );
        break;
    case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
        p_es->i_payload_type = 32;
        p_es->psz_rtpmap = strdup( "MPV/90000" );
        break;
    case VLC_FOURCC( 'a', '5', '2', ' ' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( "ac3/90000" );
        break;
    case VLC_FOURCC( 'H', '2', '6', '3' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( "H263-1998/90000" );
        break;
    case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( "MP4V-ES/90000" );
        if( p_fmt->i_extra > 0 )
        {
            char *p_hexa = malloc( 2 * p_fmt->i_extra + 1 );
            p_es->psz_fmtp = malloc( 100 + 2 * p_fmt->i_extra );
            sprintf_hexa( p_hexa, p_fmt->p_extra, p_fmt->i_extra );
            sprintf( p_es->psz_fmtp,
                     "profile-level-id=3; config=%s;", p_hexa );
            free( p_hexa );
        }
        break;
    case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = malloc( strlen( "mpeg4-generic/" ) + 12 );
        sprintf( p_es->psz_rtpmap, "mpeg4-generic/%d", p_fmt->audio.i_rate );
        if( p_fmt->i_extra > 0 )
        {
            char *p_hexa = malloc( 2 * p_fmt->i_extra + 1 );
            p_es->psz_fmtp = malloc( 200 + 2 * p_fmt->i_extra );
            sprintf_hexa( p_hexa, p_fmt->p_extra, p_fmt->i_extra );
            sprintf( p_es->psz_fmtp,
                     "streamtype=5; profile-level-id=15; mode=AAC-hbr; "
                     "config=%s; SizeLength=13;IndexLength=3; "
                     "IndexDeltaLength=3; Profile=1;", p_hexa );
            free( p_hexa );
        }
        break;
    case VLC_FOURCC( 'm', 'p', '2', 't' ):
        p_media->psz_mux = "ts";
        p_es->i_payload_type = 33;
        p_es->psz_rtpmap = strdup( "MP2T/90000" );
        break;
    case VLC_FOURCC( 'm', 'p', '2', 'p' ):
        p_media->psz_mux = "ps";
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( "MP2P/90000" );
        break;
    case VLC_FOURCC( 's', 'a', 'm', 'r' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                   "AMR/8000/2" : "AMR/8000" );
        p_es->psz_fmtp = strdup( "octet-align=1" );
        break; 
    case VLC_FOURCC( 's', 'a', 'w', 'b' ):
        p_es->i_payload_type = p_media->i_payload_type++;
        p_es->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                   "AMR-WB/16000/2" : "AMR-WB/16000" );
        p_es->psz_fmtp = strdup( "octet-align=1" );
        break; 

    default:
        msg_Err( p_vod, "cannot add this stream (unsupported "
                 "codec: %4.4s)", (char*)&p_fmt->i_codec );
        free( p_es );
        return VLC_EGENERIC;
    }

    p_es->p_rtsp_url =
        httpd_UrlNewUnique( p_vod->p_sys->p_rtsp_host, psz_urlc, NULL, NULL,
                            NULL );

    if( !p_es->p_rtsp_url )
    {
        msg_Err( p_vod, "cannot create http url (%s)", psz_urlc );
        free( psz_urlc );
        free( p_es );
        return VLC_EGENERIC;
    }
    free( psz_urlc );

    httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_SETUP,
                    RtspCallbackES, (void*)p_es );
    httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_TEARDOWN,
                    RtspCallbackES, (void*)p_es );
    httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_PLAY,
                    RtspCallbackES, (void*)p_es );
    httpd_UrlCatch( p_es->p_rtsp_url, HTTPD_MSG_PAUSE,
                    RtspCallbackES, (void*)p_es );

    es_format_Copy( &p_es->fmt, p_fmt );
    p_es->p_vod = p_vod;
    p_es->p_media = p_media;

#if 0
    /* Choose the port */
    if( p_fmt->i_cat == AUDIO_ES && p_media->i_port_audio > 0 )
    {
        p_es->i_port = p_media->i_port_audio;
        p_media->i_port_audio = 0;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_media->i_port_video > 0 )
    {
        p_es->i_port = p_media->i_port_video;
        p_media->i_port_video = 0;
    }
    while( !p_es->i_port )
    {
        if( p_media->i_port != p_media->i_port_audio &&
            p_media->i_port != p_media->i_port_video )
        {
            p_es->i_port = p_media->i_port;
            p_media->i_port += 2;
            break;
        }
        p_media->i_port += 2;
    }
#else

    p_es->i_port = 0;
#endif

    vlc_mutex_lock( &p_media->lock );
    TAB_APPEND( p_media->i_es, p_media->es, p_es );
    vlc_mutex_unlock( &p_media->lock );

    p_media->i_sdp_version++;

    return VLC_SUCCESS;
}

static void MediaDelES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt)
{
    media_es_t *p_es = 0;
    int i;

    /* Find the ES */
    for( i = 0; i < p_media->i_es; i++ )
    {
        if( p_media->es[i]->fmt.i_cat == p_fmt->i_cat &&
            p_media->es[i]->fmt.i_codec == p_fmt->i_codec &&
            p_media->es[i]->fmt.i_id == p_fmt->i_id )
        {
            p_es = p_media->es[i];
        }
    }
    if( !p_es ) return;

    msg_Dbg( p_vod, "  - Removing ES %4.4s", (char *)&p_fmt->i_codec );

    vlc_mutex_lock( &p_media->lock );
    TAB_REMOVE( p_media->i_es, p_media->es, p_es );
    vlc_mutex_unlock( &p_media->lock );

    if( p_es->psz_rtpmap ) free( p_es->psz_rtpmap );
    if( p_es->psz_fmtp ) free( p_es->psz_fmtp );
    p_media->i_sdp_version++;

    if( p_es->p_rtsp_url ) httpd_UrlDelete( p_es->p_rtsp_url );
    es_format_Clean( &p_es->fmt );
}

/****************************************************************************
 * RTSP server implementation
 ****************************************************************************/
static rtsp_client_t *RtspClientNew( vod_media_t *p_media, char *psz_session )
{
    rtsp_client_t *p_rtsp = malloc( sizeof(rtsp_client_t) );
    memset( p_rtsp, 0, sizeof(rtsp_client_t) );
    p_rtsp->es = 0;

    p_rtsp->psz_session = psz_session;
    TAB_APPEND( p_media->i_rtsp, p_media->rtsp, p_rtsp );

    msg_Dbg( p_media->p_vod, "new session: %s", psz_session );

    return p_rtsp;
}

static rtsp_client_t *RtspClientGet( vod_media_t *p_media, char *psz_session )
{
    int i;

    for( i = 0; psz_session && i < p_media->i_rtsp; i++ )
    {
        if( !strcmp( p_media->rtsp[i]->psz_session, psz_session ) )
        {
            return p_media->rtsp[i];
        }
    }

    return NULL;
}

static void RtspClientDel( vod_media_t *p_media, rtsp_client_t *p_rtsp )
{
    msg_Dbg( p_media->p_vod, "closing session: %s", p_rtsp->psz_session );

    while( p_rtsp->i_es-- )
    {
        if( p_rtsp->es[p_rtsp->i_es]->psz_ip )
            free( p_rtsp->es[p_rtsp->i_es]->psz_ip );
        free( p_rtsp->es[p_rtsp->i_es] );
        if( !p_rtsp->i_es ) free( p_rtsp->es );
    }

    TAB_REMOVE( p_media->i_rtsp, p_media->rtsp, p_rtsp );

    free( p_rtsp->psz_session );
    free( p_rtsp );
}

static int RtspCallback( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                         httpd_message_t *answer, httpd_message_t *query )
{
    vod_media_t *p_media = (vod_media_t*)p_args;
    vod_t *p_vod = p_media->p_vod;
    char *psz_session = NULL;
    rtsp_client_t *p_rtsp;

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
                SDPGenerate( p_media, cl );

            if( psz_sdp != NULL )
            {
                answer->i_status = 200;
                answer->psz_status = strdup( "OK" );
                httpd_MsgAdd( answer, "Content-type",  "%s", "application/sdp" );
    
                answer->p_body = (uint8_t *)psz_sdp;
                answer->i_body = strlen( psz_sdp );
            }
            else
            {
                answer->i_status = 500;
                answer->psz_status = strdup( "Internal server error" );
                answer->p_body = NULL;
                answer->i_body = 0;
            }
            break;
        }

        case HTTPD_MSG_PLAY:
        {
            char *psz_output, ip[NI_MAXNUMERICHOST];
            int i, i_port_audio = 0, i_port_video = 0;

            /* for now only multicast so easy */
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PLAY for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            if( p_rtsp->b_playing && p_rtsp->b_paused )
            {
                vod_MediaControl( p_vod, p_media, psz_session,
                                  VOD_MEDIA_PAUSE );
                p_rtsp->b_paused = VLC_FALSE;
                break;
            }
            else if( p_rtsp->b_playing ) break;

            if( httpd_ClientIP( cl, ip ) == NULL ) break;

            p_rtsp->b_playing = VLC_TRUE;

            /* FIXME for != 1 video and 1 audio */
            for( i = 0; i < p_rtsp->i_es; i++ )
            {
                if( p_rtsp->es[i]->p_media_es->fmt.i_cat == AUDIO_ES )
                    i_port_audio = p_rtsp->es[i]->i_port;
                if( p_rtsp->es[i]->p_media_es->fmt.i_cat == VIDEO_ES )
                    i_port_video = p_rtsp->es[i]->i_port;
            }

            if( p_media->psz_mux )
            {
                asprintf( &psz_output, "rtp{dst=%s,port=%i,mux=%s}",
                          ip, i_port_video, p_media->psz_mux );
            }
            else
            {
                asprintf( &psz_output, "rtp{dst=%s,port-video=%i,"
                          "port-audio=%i}", ip, i_port_video, i_port_audio );
            }

            vod_MediaControl( p_vod, p_media, psz_session, VOD_MEDIA_PLAY,
                              psz_output );
            free( psz_output );
            break;
        }

        case HTTPD_MSG_PAUSE:
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PAUSE for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            vod_MediaControl( p_vod, p_media, psz_session, VOD_MEDIA_PAUSE );
            p_rtsp->b_paused = VLC_TRUE;

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;
            break;

        case HTTPD_MSG_TEARDOWN:
            /* for now only multicast so easy again */
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_TEARDOWN for session: %s", psz_session);

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            vod_MediaControl( p_vod, p_media, psz_session, VOD_MEDIA_STOP );
            RtspClientDel( p_media, p_rtsp );
            break;

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

static int RtspCallbackES( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                           httpd_message_t *answer, httpd_message_t *query )
{
    media_es_t *p_es = (media_es_t*)p_args;
    vod_media_t *p_media = p_es->p_media;
    vod_t *p_vod = p_media->p_vod;
    rtsp_client_t *p_rtsp = NULL;
    char *psz_session = NULL;
    char *psz_transport = NULL;
    char *psz_position = NULL;
    int i;

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

        if( strstr( psz_transport, "unicast" ) &&
            strstr( psz_transport, "client_port=" ) )
        {
            rtsp_client_t *p_rtsp;
            rtsp_client_es_t *p_rtsp_es;
            char ip[NI_MAXNUMERICHOST];
            int i_port = atoi( strstr( psz_transport, "client_port=" ) +
                               strlen("client_port=") );

            if( httpd_ClientIP( cl, ip ) == NULL )
            {
                answer->i_status = 500;
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
                asprintf( &psz_session, "%d", rand() );
                p_rtsp = RtspClientNew( p_media, psz_session );
            }
            else
            {
                p_rtsp = RtspClientGet( p_media, psz_session );
                if( !p_rtsp )
                {
                    /* FIXME right error code */
                    answer->i_status = 454;
                    answer->psz_status = strdup( "Unknown session id" );
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    free( ip );
                    break;
                }
            }

            p_rtsp_es = malloc( sizeof(rtsp_client_es_t) );
            p_rtsp_es->i_port = i_port;
            p_rtsp_es->psz_ip = strdup( ip );
            p_rtsp_es->p_media_es = p_es;
            TAB_APPEND( p_rtsp->i_es, p_rtsp->es, p_rtsp_es );

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            httpd_MsgAdd( answer, "Transport", "RTP/AVP/UDP;client_port=%d-%d",
                          i_port, i_port + 1 );
        }
        else /* TODO  strstr( psz_transport, "interleaved" ) ) */
        {
            answer->i_status = 461;
            answer->psz_status = strdup( "Unsupported Transport" );
            answer->i_body = 0;
            answer->p_body = NULL;
        }
        break;

        case HTTPD_MSG_TEARDOWN:
            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_TEARDOWN for session: %s", psz_session);

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            for( i = 0; i < p_rtsp->i_es; i++ )
            {
                if( p_rtsp->es[i]->p_media_es == p_es )
                {
                    if( p_rtsp->es[i]->psz_ip ) free( p_rtsp->es[i]->psz_ip );
                    TAB_REMOVE( p_rtsp->i_es, p_rtsp->es, p_rtsp->es[i] );
                    break;
                }
            }

            if( !p_rtsp->i_es )
            {
                vod_MediaControl( p_vod, p_media, psz_session,
                                  VOD_MEDIA_STOP );
                RtspClientDel( p_media, p_rtsp );
            }
            break;

        case HTTPD_MSG_PLAY:
            /* This is kind of a kludge. Should we only support Aggregate
             * Operations ? */
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PLAY for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );

            psz_position = httpd_MsgGet( query, "Range" );
            if( psz_position ) psz_position = strstr( psz_position, "npt=" );
            if( psz_position )
            {
                float f_pos;

                msg_Dbg( p_vod, "seeking request: %s", psz_position );

                psz_position += 4;
                if( sscanf( psz_position, "%f", &f_pos ) == 1 )
                {
                    f_pos /= ((float)(p_media->i_length/1000))/1000 / 100;
                    vod_MediaControl( p_vod, p_media, psz_session,
                                      VOD_MEDIA_SEEK, (double)f_pos );
                }
            }

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;
            break;

        case HTTPD_MSG_PAUSE:
            /* This is kind of a kludge. Should we only support Aggregate
             * Operations ? */
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PAUSE for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            vod_MediaControl( p_vod, p_media, psz_session, VOD_MEDIA_PAUSE );
            p_rtsp->b_paused = VLC_TRUE;

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            answer->i_body = 0;
            answer->p_body = NULL;
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
static char *SDPGenerate( const vod_media_t *p_media, httpd_client_t *cl )
{
    int i, i_size;
    char *p, *psz_sdp, ip[NI_MAXNUMERICHOST], ipv;

    if( httpd_ServerIP( cl, ip ) == NULL )
        return NULL;

    p = strchr( ip, '%' );
    if( p != NULL )
        *p = '\0'; /* remove scope if present */

    ipv = ( strchr( ip, ':' ) != NULL ) ? '6' : '4';

    /* Calculate size */
    i_size = sizeof( "v=0\r\n" ) +
        sizeof( "o=- * * IN IP4 \r\n" ) + 10 + NI_MAXNUMERICHOST +
        sizeof( "s=*\r\n" ) + strlen( p_media->psz_session_name ) +
        sizeof( "i=*\r\n" ) + strlen( p_media->psz_session_description ) +
        sizeof( "u=*\r\n" ) + strlen( p_media->psz_session_url ) +
        sizeof( "e=*\r\n" ) + strlen( p_media->psz_session_email ) +
        sizeof( "t=0 0\r\n" ) + /* FIXME */
        sizeof( "a=tool:"PACKAGE_STRING"\r\n" ) +
        sizeof( "c=IN IP4 0.0.0.0\r\n" ) + 20 + 10 +
        sizeof( "a=range:npt=0-1000000000.000\r\n" );

    for( i = 0; i < p_media->i_es; i++ )
    {
        media_es_t *p_es = p_media->es[i];

        i_size += sizeof( "m=**d*o * RTP/AVP *\r\n" ) + 19;
        if( p_es->psz_rtpmap )
        {
            i_size += sizeof( "a=rtpmap:* *\r\n" ) +
                strlen( p_es->psz_rtpmap ) + 9;
        }
        if( p_es->psz_fmtp )
        {
            i_size += sizeof( "a=fmtp:* *\r\n" ) +
                strlen( p_es->psz_fmtp ) + 9;
        }

        i_size += sizeof( "a=control:*/trackid=*\r\n" ) +
            strlen( p_media->psz_rtsp_control ) + 9;
    }

    p = psz_sdp = malloc( i_size );
    p += sprintf( p, "v=0\r\n" );
    p += sprintf( p, "o=- "I64Fd" %d IN IP%c %s\r\n",
                  p_media->i_sdp_id, p_media->i_sdp_version, ipv, ip );
    if( *p_media->psz_session_name )
        p += sprintf( p, "s=%s\r\n", p_media->psz_session_name );
    if( *p_media->psz_session_description )
        p += sprintf( p, "i=%s\r\n", p_media->psz_session_description );
    if( *p_media->psz_session_url )
        p += sprintf( p, "u=%s\r\n", p_media->psz_session_url );
    if( *p_media->psz_session_email )
        p += sprintf( p, "e=%s\r\n", p_media->psz_session_email );

    p += sprintf( p, "t=0 0\r\n" ); /* FIXME */
    p += sprintf( p, "a=tool:"PACKAGE_STRING"\r\n" );

    p += sprintf( p, "c=IN IP%c %s\r\n", ipv, ipv == '6' ? "::" : "0.0.0.0" );

    if( p_media->i_length > 0 )
    p += sprintf( p, "a=range:npt=0-%.3f\r\n",
                  ((float)(p_media->i_length/1000))/1000 );

    for( i = 0; i < p_media->i_es; i++ )
    {
        media_es_t *p_es = p_media->es[i];

        if( p_es->fmt.i_cat == AUDIO_ES )
        {
            p += sprintf( p, "m=audio %d RTP/AVP %d\r\n",
                          p_es->i_port, p_es->i_payload_type );
        }
        else if( p_es->fmt.i_cat == VIDEO_ES )
        {
            p += sprintf( p, "m=video %d RTP/AVP %d\r\n",
                          p_es->i_port, p_es->i_payload_type );
        }
        else
        {
            continue;
        }

        if( p_es->psz_rtpmap )
        {
            p += sprintf( p, "a=rtpmap:%d %s\r\n", p_es->i_payload_type,
                          p_es->psz_rtpmap );
        }
        if( p_es->psz_fmtp )
        {
            p += sprintf( p, "a=fmtp:%d %s\r\n", p_es->i_payload_type,
                          p_es->psz_fmtp );
        }

        p += sprintf( p, "a=control:%s/trackid=%d\r\n",
                      p_media->psz_rtsp_control, i );
    }

    return psz_sdp;
}
