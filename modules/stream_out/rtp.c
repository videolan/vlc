/*****************************************************************************
 * rtp.c: rtp stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_charset.h>
#include <vlc_strings.h>

#include "rtp.h"

#ifdef HAVE_UNISTD_H
#   include <sys/types.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <sys/stat.h>
#endif

#include <errno.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define DST_TEXT N_("Destination")
#define DST_LONGTEXT N_( \
    "This is the output URL that will be used." )
#define SDP_TEXT N_("SDP")
#define SDP_LONGTEXT N_( \
    "This allows you to specify how the SDP (Session Descriptor) for this RTP "\
    "session will be made available. You must use an url: http://location to " \
    "access the SDP via HTTP, rtsp://location for RTSP access, and sap:// " \
    "for the SDP to be announced via SAP." )
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_( \
    "This allows you to specify the muxer used for the streaming output. " \
    "Default is to use no muxer (standard RTP stream)." )

#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "This is the name of the session that will be announced in the SDP " \
    "(Session Descriptor)." )
#define DESC_TEXT N_("Session descriptipn")
#define DESC_LONGTEXT N_( \
    "This allows you to give a short description with details about the stream, " \
    "that will be announced in the SDP (Session Descriptor)." )
#define URL_TEXT N_("Session URL")
#define URL_LONGTEXT N_( \
    "This allows you to give an URL with more details about the stream " \
    "(often the website of the streaming organization), that will " \
    "be announced in the SDP (Session Descriptor)." )
#define EMAIL_TEXT N_("Session email")
#define EMAIL_LONGTEXT N_( \
    "This allows you to give a contact mail address for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )
#define PHONE_TEXT N_("Session phone number")
#define PHONE_LONGTEXT N_( \
    "This allows you to give a contact telephone number for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )

#define PORT_TEXT N_("Port")
#define PORT_LONGTEXT N_( \
    "This allows you to specify the base port for the RTP streaming." )
#define PORT_AUDIO_TEXT N_("Audio port")
#define PORT_AUDIO_LONGTEXT N_( \
    "This allows you to specify the default audio port for the RTP streaming." )
#define PORT_VIDEO_TEXT N_("Video port")
#define PORT_VIDEO_LONGTEXT N_( \
    "This allows you to specify the default video port for the RTP streaming." )

#define TTL_TEXT N_("Hop limit (TTL)")
#define TTL_LONGTEXT N_( \
    "This is the hop limit (also known as \"Time-To-Live\" or TTL) of " \
    "the multicast packets sent by the stream output (0 = use operating " \
    "system built-in default).")

#define DCCP_TEXT N_("DCCP transport")
#define DCCP_LONGTEXT N_( \
    "This enables DCCP instead of UDP as a transport for RTP." )
#define TCP_TEXT N_("TCP transport")
#define TCP_LONGTEXT N_( \
    "This enables TCP instead of UDP as a transport for RTP." )
#define UDP_LITE_TEXT N_("UDP-Lite transport")
#define UDP_LITE_LONGTEXT N_( \
    "This enables UDP-Lite instead of UDP as a transport for RTP." )

#define RFC3016_TEXT N_("MP4A LATM")
#define RFC3016_LONGTEXT N_( \
    "This allows you to stream MPEG4 LATM audio streams (see RFC3016)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-rtp-"
#define MAX_EMPTY_BLOCKS 200

vlc_module_begin();
    set_shortname( _("RTP"));
    set_description( _("RTP stream output") );
    set_capability( "sout stream", 0 );
    add_shortcut( "rtp" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );

    add_string( SOUT_CFG_PREFIX "dst", "", NULL, DST_TEXT,
                DST_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "sdp", "", NULL, SDP_TEXT,
                SDP_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "mux", "", NULL, MUX_TEXT,
                MUX_LONGTEXT, VLC_TRUE );

    add_string( SOUT_CFG_PREFIX "name", "", NULL, NAME_TEXT,
                NAME_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "description", "", NULL, DESC_TEXT,
                DESC_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "url", "", NULL, URL_TEXT,
                URL_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "email", "", NULL, EMAIL_TEXT,
                EMAIL_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "phone", "", NULL, PHONE_TEXT,
                PHONE_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "port", 1234, NULL, PORT_TEXT,
                 PORT_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "port-audio", 1230, NULL, PORT_AUDIO_TEXT,
                 PORT_AUDIO_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "port-video", 1232, NULL, PORT_VIDEO_TEXT,
                 PORT_VIDEO_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "ttl", 0, NULL, TTL_TEXT,
                 TTL_LONGTEXT, VLC_TRUE );

    add_bool( SOUT_CFG_PREFIX "dccp", 0, NULL,
              DCCP_TEXT, DCCP_LONGTEXT, VLC_FALSE );
    add_bool( SOUT_CFG_PREFIX "tcp", 0, NULL,
              TCP_TEXT, TCP_LONGTEXT, VLC_FALSE );
    add_bool( SOUT_CFG_PREFIX "udplite", 0, NULL,
              UDP_LITE_TEXT, UDP_LITE_LONGTEXT, VLC_FALSE );

    add_bool( SOUT_CFG_PREFIX "mp4a-latm", 0, NULL, RFC3016_TEXT,
                 RFC3016_LONGTEXT, VLC_FALSE );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "dst", "name", "port", "port-audio", "port-video", "*sdp", "ttl", "mux",
    "description", "url", "email", "phone",
    "dccp", "tcp", "udplite",
    "mp4a-latm", NULL
};

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *,
                               block_t* );
static sout_stream_id_t *MuxAdd ( sout_stream_t *, es_format_t * );
static int               MuxDel ( sout_stream_t *, sout_stream_id_t * );
static int               MuxSend( sout_stream_t *, sout_stream_id_t *,
                                  block_t* );

static sout_access_out_t *GrabberCreate( sout_stream_t *p_sout );
static void ThreadSend( vlc_object_t *p_this );

static void SDPHandleUrl( sout_stream_t *, char * );

static int SapSetup( sout_stream_t *p_stream );
static int FileSetup( sout_stream_t *p_stream );
static int HttpSetup( sout_stream_t *p_stream, vlc_url_t * );

struct sout_stream_sys_t
{
    /* SDP */
    int64_t i_sdp_id;
    int     i_sdp_version;
    char    *psz_sdp;
    vlc_mutex_t  lock_sdp;

    char        *psz_session_name;
    char        *psz_session_description;
    char        *psz_session_url;
    char        *psz_session_email;

    /* SDP to disk */
    vlc_bool_t b_export_sdp_file;
    char *psz_sdp_file;

    /* SDP via SAP */
    vlc_bool_t b_export_sap;
    session_descriptor_t *p_session;

    /* SDP via HTTP */
    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    /* RTSP */
    rtsp_stream_t *rtsp;

    /* */
    char     *psz_destination;
    uint8_t   proto;
    uint8_t   i_ttl;
    uint16_t  i_port;
    uint16_t  i_port_audio;
    uint16_t  i_port_video;
    vlc_bool_t b_latm;

    /* when need to use a private one or when using muxer */
    int i_payload_type;

    /* in case we do TS/PS over rtp */
    sout_mux_t        *p_mux;
    sout_access_out_t *p_grab;
    block_t           *packet;

    /* */
    vlc_mutex_t      lock_es;
    int              i_es;
    sout_stream_id_t **es;
};

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *,
                                    block_t * );

typedef struct rtp_sink_t
{
    int rtp_fd;
    rtcp_sender_t *rtcp;
} rtp_sink_t;

struct sout_stream_id_t
{
    VLC_COMMON_MEMBERS

    sout_stream_t *p_stream;
    /* rtp field */
    uint32_t    i_timestamp_start;
    uint16_t    i_sequence;
    uint8_t     i_payload_type;
    uint8_t     ssrc[4];

    /* for sdp */
    char        *psz_rtpmap;
    char        *psz_fmtp;
    int          i_clock_rate;
    int          i_port;
    int          i_cat;
    int          i_bitrate;

    /* Packetizer specific fields */
    pf_rtp_packetizer_t pf_packetize;
    int          i_mtu;

    /* Packets sinks */
    vlc_mutex_t       lock_sink;
    int               sinkc;
    rtp_sink_t       *sinkv;
    rtsp_stream_id_t *rtsp_id;

    block_fifo_t     *p_fifo;
    int64_t           i_caching;
};


/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    sout_stream_sys_t   *p_sys = NULL;
    config_chain_t      *p_cfg = NULL;
    char                *psz;
    vlc_bool_t          b_rtsp = VLC_FALSE;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX,
                       ppsz_sout_options, p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->psz_destination = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "dst" );
    p_sys->psz_session_name = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "name" );
    p_sys->psz_session_description = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "description" );
    p_sys->psz_session_url = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "url" );
    p_sys->psz_session_email = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "email" );

    p_sys->i_port       = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port" );
    p_sys->i_port_audio = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-audio" );
    p_sys->i_port_video = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-video" );

    p_sys->psz_sdp_file = NULL;

    if( p_sys->i_port_audio == p_sys->i_port_video )
    {
        msg_Err( p_stream, "audio and video port cannot be the same" );
        p_sys->i_port_audio = 0;
        p_sys->i_port_video = 0;
    }

    if( !p_sys->psz_session_name )
    {
        if( p_sys->psz_destination )
            p_sys->psz_session_name = strdup( p_sys->psz_destination );
        else
           p_sys->psz_session_name = strdup( "NONE" );
    }

    for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
    {
        if( !strcmp( p_cfg->psz_name, "sdp" )
         && ( p_cfg->psz_value != NULL )
         && !strncasecmp( p_cfg->psz_value, "rtsp:", 5 ) )
        {
            b_rtsp = VLC_TRUE;
            break;
        }
    }
    if( !b_rtsp )
    {
        psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "sdp" );
        if( psz != NULL )
        {
            if( !strncasecmp( psz, "rtsp:", 5 ) )
                b_rtsp = VLC_TRUE;
            free( psz );
        }
    }

    /* Transport protocol */
    p_sys->proto = IPPROTO_UDP;

#if 0
    if( var_GetBool( p_stream, SOUT_CFG_PREFIX "dccp" ) )
    {
        p_sys->sotype = SOCK_DCCP;
        p_sys->proto = 33 /*IPPROTO_DCCP*/;
    }
    else
    if( var_GetBool( p_stream, SOUT_CFG_PREFIX "tcp" ) )
    {
        p_sys->sotype = SOCK_STREAM;
        p_sys->proto = IPPROTO_TCP;
    }
    else
#endif
    if( var_GetBool( p_stream, SOUT_CFG_PREFIX "udplite" ) )
        p_sys->proto = 136 /*IPPROTO_UDPLITE*/;

    if( ( p_sys->psz_destination == NULL ) && !b_rtsp )
    {
        msg_Err( p_stream, "missing destination and not in RTSP mode" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_ttl = var_GetInteger( p_stream, SOUT_CFG_PREFIX "ttl" );
    if( p_sys->i_ttl == 0 )
    {
        /* Normally, we should let the default hop limit up to the core,
         * but we have to know it to build our SDP properly, which is why
         * we ask the core. FIXME: broken when neither sout-rtp-ttl nor
         * ttl are set. */
        p_sys->i_ttl = config_GetInt( p_stream, "ttl" );
    }

    p_sys->b_latm = var_GetBool( p_stream, SOUT_CFG_PREFIX "mp4a-latm" );

    p_sys->i_payload_type = 96;
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->rtsp = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->i_sdp_id = mdate();
    p_sys->i_sdp_version = 1;
    p_sys->psz_sdp = NULL;

    p_sys->b_export_sap = VLC_FALSE;
    p_sys->b_export_sdp_file = VLC_FALSE;
    p_sys->p_session = NULL;

    p_sys->p_httpd_host = NULL;
    p_sys->p_httpd_file = NULL;

    p_stream->p_sys     = p_sys;

    vlc_mutex_init( p_stream, &p_sys->lock_sdp );
    vlc_mutex_init( p_stream, &p_sys->lock_es );

    psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );
    if( psz != NULL )
    {
        sout_stream_id_t *id;

        /* Check muxer type */
        if( strncasecmp( psz, "ps", 2 )
         && strncasecmp( psz, "mpeg1", 5 )
         && strncasecmp( psz, "ts", 2 ) )
        {
            msg_Err( p_stream, "unsupported muxer type for RTP (only TS/PS)" );
            free( psz );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_sys->p_grab = GrabberCreate( p_stream );
        p_sys->p_mux = sout_MuxNew( p_sout, psz, p_sys->p_grab );
        free( psz );

        if( p_sys->p_mux == NULL )
        {
            msg_Err( p_stream, "cannot create muxer" );
            sout_AccessOutDelete( p_sys->p_grab );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys );
            return VLC_EGENERIC;
        }

        id = Add( p_stream, NULL );
        if( id == NULL )
        {
            sout_MuxDelete( p_sys->p_mux );
            sout_AccessOutDelete( p_sys->p_grab );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_sys->packet = NULL;

        p_stream->pf_add  = MuxAdd;
        p_stream->pf_del  = MuxDel;
        p_stream->pf_send = MuxSend;
    }
    else
    {
        p_sys->p_mux    = NULL;
        p_sys->p_grab   = NULL;

        p_stream->pf_add    = Add;
        p_stream->pf_del    = Del;
        p_stream->pf_send   = Send;
    }

    psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "sdp" );
    if( psz != NULL )
    {
        config_chain_t *p_cfg;

        SDPHandleUrl( p_stream, psz );

        for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
        {
            if( !strcmp( p_cfg->psz_name, "sdp" ) )
            {
                if( p_cfg->psz_value == NULL || *p_cfg->psz_value == '\0' )
                    continue;

                /* needed both :sout-rtp-sdp= and rtp{sdp=} can be used */
                if( !strcmp( p_cfg->psz_value, psz ) )
                    continue;

                SDPHandleUrl( p_stream, p_cfg->psz_value );
            }
        }
        free( psz );
    }

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol--;

    if( p_sys->p_mux )
    {
        assert( p_sys->i_es == 1 );
        Del( p_stream, p_sys->es[0] );

        sout_MuxDelete( p_sys->p_mux );
        sout_AccessOutDelete( p_sys->p_grab );
        if( p_sys->packet )
        {
            block_Release( p_sys->packet );
        }
        if( p_sys->b_export_sap )
        {
            p_sys->p_mux = NULL;
            SapSetup( p_stream );
        }
    }

    if( p_sys->rtsp != NULL )
        RtspUnsetup( p_sys->rtsp );

    vlc_mutex_destroy( &p_sys->lock_sdp );
    vlc_mutex_destroy( &p_sys->lock_es );

    if( p_sys->p_httpd_file )
        httpd_FileDelete( p_sys->p_httpd_file );

    if( p_sys->p_httpd_host )
        httpd_HostDelete( p_sys->p_httpd_host );

    free( p_sys->psz_session_name );
    free( p_sys->psz_session_description );
    free( p_sys->psz_session_url );
    free( p_sys->psz_session_email );
    free( p_sys->psz_sdp );

    if( p_sys->b_export_sdp_file )
    {
#ifdef HAVE_UNISTD_H
        unlink( p_sys->psz_sdp_file );
#endif
        free( p_sys->psz_sdp_file );
    }
    free( p_sys->psz_destination );
    free( p_sys );
}

/*****************************************************************************
 * SDPHandleUrl:
 *****************************************************************************/
static void SDPHandleUrl( sout_stream_t *p_stream, char *psz_url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_url_t url;

    vlc_UrlParse( &url, psz_url, 0 );
    if( url.psz_protocol && !strcasecmp( url.psz_protocol, "http" ) )
    {
        if( p_sys->p_httpd_file )
        {
            msg_Err( p_stream, "you can use sdp=http:// only once" );
            goto out;
        }

        if( HttpSetup( p_stream, &url ) )
        {
            msg_Err( p_stream, "cannot export SDP as HTTP" );
        }
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "rtsp" ) )
    {
        if( p_sys->rtsp != NULL )
        {
            msg_Err( p_stream, "you can use sdp=rtsp:// only once" );
            goto out;
        }

        /* FIXME test if destination is multicast or no destination at all */
        p_sys->rtsp = RtspSetup( p_stream, &url );
        if( p_sys->rtsp == NULL )
        {
            msg_Err( p_stream, "cannot export SDP as RTSP" );
        }

        if( p_sys->p_mux != NULL )
        {
            sout_stream_id_t *id = p_sys->es[0];
            id->rtsp_id = RtspAddId( p_sys->rtsp, id, 0,
                                     p_sys->psz_destination, p_sys->i_ttl,
                                     id->i_port, id->i_port + 1 );
        }
    }
    else if( ( url.psz_protocol && !strcasecmp( url.psz_protocol, "sap" ) ) ||
             ( url.psz_host && !strcasecmp( url.psz_host, "sap" ) ) )
    {
        p_sys->b_export_sap = VLC_TRUE;
        SapSetup( p_stream );
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "file" ) )
    {
        if( p_sys->b_export_sdp_file )
        {
            msg_Err( p_stream, "you can use sdp=file:// only once" );
            goto out;
        }
        p_sys->b_export_sdp_file = VLC_TRUE;
        psz_url = &psz_url[5];
        if( psz_url[0] == '/' && psz_url[1] == '/' )
            psz_url += 2;
        p_sys->psz_sdp_file = strdup( psz_url );
    }
    else
    {
        msg_Warn( p_stream, "unknown protocol for SDP (%s)",
                  url.psz_protocol );
    }

out:
    vlc_UrlClean( &url );
}

/*****************************************************************************
 * SDPGenerate
 *****************************************************************************/
/*static*/
char *SDPGenerate( const sout_stream_t *p_stream, const char *rtsp_url )
{
    const sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_sdp;
    struct sockaddr_storage dst;
    socklen_t dstlen;
    int i;
    /*
     * When we have a fixed destination (typically when we do multicast),
     * we need to put the actual port numbers in the SDP.
     * When there is no fixed destination, we only support RTSP unicast
     * on-demand setup, so we should rather let the clients decide which ports
     * to use.
     * When there is both a fixed destination and RTSP unicast, we need to
     * put port numbers used by the fixed destination, otherwise the SDP would
     * become totally incorrect for multicast use. It should be noted that
     * port numbers from SDP with RTSP are only "recommendation" from the
     * server to the clients (per RFC2326), so only broken clients will fail
     * to handle this properly. There is no solution but to use two differents
     * output chain with two different RTSP URLs if you need to handle this
     * scenario.
     */
    int inclport;

    if( p_sys->psz_destination != NULL )
    {
        inclport = 1;

        /* Oh boy, this is really ugly! (+ race condition on lock_es) */
        dstlen = sizeof( dst );
        getpeername( p_sys->es[0]->sinkv[0].rtp_fd, (struct sockaddr *)&dst,
                     &dstlen );
    }
    else
    {
        inclport = 0;

        /* Dummy destination address for RTSP */
        memset (&dst, 0, sizeof( struct sockaddr_in ) );
        dst.ss_family = AF_INET;
#ifdef HAVE_SA_LEN
        dst.ss_len =
#endif
        dstlen = sizeof( struct sockaddr_in );
    }

    psz_sdp = vlc_sdp_Start( VLC_OBJECT( p_stream ), SOUT_CFG_PREFIX,
                             NULL, 0, (struct sockaddr *)&dst, dstlen );
    if( psz_sdp == NULL )
        return NULL;

    /* TODO: a=source-filter */

    if( rtsp_url != NULL )
        sdp_AddAttribute ( &psz_sdp, "control", "%s", rtsp_url );

    /* FIXME: locking?! */
    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];
        const char *mime_major; /* major MIME type */

        switch( id->i_cat )
        {
            case VIDEO_ES:
                mime_major = "video";
                break;
            case AUDIO_ES:
                mime_major = "audio";
                break;
            case SPU_ES:
                mime_major = "text";
                break;
            default:
                continue;
        }

        sdp_AddMedia( &psz_sdp, mime_major, "RTP/AVP", inclport * id->i_port,
                      id->i_payload_type, VLC_FALSE, id->i_bitrate,
                      id->psz_rtpmap, id->psz_fmtp);

        if( rtsp_url != NULL )
            sdp_AddAttribute ( &psz_sdp, "control", "%s/trackID=%d",
                               rtsp_url, i );
    }

    return psz_sdp;
}

/*****************************************************************************
 * RTP mux
 *****************************************************************************/
static int rtp_packetize_l16  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_l8   ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mpa  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mpv  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_ac3  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_split( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mp4a ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mp4a_latm ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_h263 ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_h264 ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_amr  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_t140 ( sout_stream_t *, sout_stream_id_t *, block_t * );

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


/** Add an ES as a new RTP stream */
static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    /* NOTE: As a special case, if we use a non-RTP
     * mux (TS/PS), then p_fmt is NULL. */
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;
    int               i_port, cscov = -1;
    char              *psz_sdp;

    id = vlc_object_create( p_stream, sizeof( sout_stream_id_t ) );
    if( id == NULL )
        return NULL;

    /* Choose the port */
    i_port = 0;
    if( p_fmt == NULL )
        ;
    else
    if( p_fmt->i_cat == AUDIO_ES && p_sys->i_port_audio > 0 )
    {
        i_port = p_sys->i_port_audio;
        p_sys->i_port_audio = 0;
    }
    else
    if( p_fmt->i_cat == VIDEO_ES && p_sys->i_port_video > 0 )
    {
        i_port = p_sys->i_port_video;
        p_sys->i_port_video = 0;
    }

    while( i_port == 0 )
    {
        if( p_sys->i_port != p_sys->i_port_audio
         && p_sys->i_port != p_sys->i_port_video )
        {
            i_port = p_sys->i_port;
            p_sys->i_port += 2;
            break;
        }
        p_sys->i_port += 2;
    }

    id->p_stream   = p_stream;

    id->i_timestamp_start = rand()&0xffffffff;
    id->i_sequence = rand()&0xffff;
    id->i_payload_type = p_sys->i_payload_type;
    id->ssrc[0] = rand()&0xff;
    id->ssrc[1] = rand()&0xff;
    id->ssrc[2] = rand()&0xff;
    id->ssrc[3] = rand()&0xff;

    id->psz_rtpmap = NULL;
    id->psz_fmtp   = NULL;
    id->i_clock_rate = 90000; /* most common case */
    id->i_port     = i_port;
    if( p_fmt != NULL )
    {
        id->i_cat  = p_fmt->i_cat;
        id->i_bitrate = p_fmt->i_bitrate/1000; /* Stream bitrate in kbps */
    }
    else
    {
        id->i_cat  = VIDEO_ES;
        id->i_bitrate = 0;
    }

    id->pf_packetize = NULL;
    id->i_mtu = config_GetInt( p_stream, "mtu" );
    if( id->i_mtu <= 12 + 16 )
        id->i_mtu = 576 - 20 - 8; /* pessimistic */

    msg_Dbg( p_stream, "maximum RTP packet size: %d bytes", id->i_mtu );

    vlc_mutex_init( p_stream, &id->lock_sink );
    id->sinkc = 0;
    id->sinkv = NULL;
    id->rtsp_id = NULL;

    id->i_caching =
        (int64_t)1000 * var_GetInteger( p_stream, SOUT_CFG_PREFIX "caching");
    id->p_fifo = block_FifoNew( p_stream );

    if( vlc_thread_create( id, "RTP send thread", ThreadSend,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        vlc_mutex_destroy( &id->lock_sink );
        vlc_object_destroy( id );
        return NULL;
    }

    if( p_sys->psz_destination != NULL )
    {
        int ttl = (p_sys->i_ttl > 0) ? p_sys->i_ttl : -1;
        int fd = net_ConnectDgram( p_stream, p_sys->psz_destination,
                                   i_port, ttl, p_sys->proto );

        if( fd == -1 )
        {
            msg_Err( p_stream, "cannot create RTP socket" );
            vlc_thread_join( id );
            vlc_mutex_destroy( &id->lock_sink );
            vlc_object_destroy( id );
            return NULL;
        }
        rtp_add_sink( id, fd );
    }

    if( p_fmt == NULL )
    {
        char *psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );

        if( psz == NULL ) /* Uho! */
            ;
        else
        if( strncmp( psz, "ts", 2 ) == 0 )
        {
            id->i_payload_type = 33;
            id->psz_rtpmap = strdup( "MP2T/90000" );
        }
        else
        {
            id->psz_rtpmap = strdup( "MP2P/90000" );
        }
    }
    else
    switch( p_fmt->i_codec )
    {
        case VLC_FOURCC( 's', '1', '6', 'b' ):
            if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 44100 )
            {
                id->i_payload_type = 11;
            }
            else if( p_fmt->audio.i_channels == 2 &&
                     p_fmt->audio.i_rate == 44100 )
            {
                id->i_payload_type = 10;
            }
            if( asprintf( &id->psz_rtpmap, "L16/%d/%d", p_fmt->audio.i_rate,
                          p_fmt->audio.i_channels ) == -1 )
                id->psz_rtpmap = NULL;
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_l16;
            break;
        case VLC_FOURCC( 'u', '8', ' ', ' ' ):
            if( asprintf( &id->psz_rtpmap, "L8/%d/%d", p_fmt->audio.i_rate,
                          p_fmt->audio.i_channels ) == -1 )
                id->psz_rtpmap = NULL;
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_l8;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            id->i_payload_type = 14;
            id->psz_rtpmap = strdup( "MPA/90000" );
            id->pf_packetize = rtp_packetize_mpa;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            id->i_payload_type = 32;
            id->psz_rtpmap = strdup( "MPV/90000" );
            id->pf_packetize = rtp_packetize_mpv;
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            id->psz_rtpmap = strdup( "ac3/90000" );
            id->pf_packetize = rtp_packetize_ac3;
            break;
        case VLC_FOURCC( 'H', '2', '6', '3' ):
            id->psz_rtpmap = strdup( "H263-1998/90000" );
            id->pf_packetize = rtp_packetize_h263;
            break;
        case VLC_FOURCC( 'h', '2', '6', '4' ):
            id->psz_rtpmap = strdup( "H264/90000" );
            id->pf_packetize = rtp_packetize_h264;
            id->psz_fmtp = NULL;

            if( p_fmt->i_extra > 0 )
            {
                uint8_t *p_buffer = p_fmt->p_extra;
                int     i_buffer = p_fmt->i_extra;
                char    *p_64_sps = NULL;
                char    *p_64_pps = NULL;
                char    hexa[6+1];

                while( i_buffer > 4 &&
                       p_buffer[0] == 0 && p_buffer[1] == 0 &&
                       p_buffer[2] == 0 && p_buffer[3] == 1 )
                {
                    const int i_nal_type = p_buffer[4]&0x1f;
                    int i_offset;
                    int i_size      = 0;

                    msg_Dbg( p_stream, "we found a startcode for NAL with TYPE:%d", i_nal_type );

                    i_size = i_buffer;
                    for( i_offset = 4; i_offset+3 < i_buffer ; i_offset++)
                    {
                        if( !memcmp (p_buffer + i_offset, "\x00\x00\x00\x01", 4 ) )
                        {
                            /* we found another startcode */
                            i_size = i_offset;
                            break;
                        }
                    }
                    if( i_nal_type == 7 )
                    {
                        p_64_sps = vlc_b64_encode_binary( &p_buffer[4], i_size - 4 );
                        sprintf_hexa( hexa, &p_buffer[5], 3 );
                    }
                    else if( i_nal_type == 8 )
                    {
                        p_64_pps = vlc_b64_encode_binary( &p_buffer[4], i_size - 4 );
                    }
                    i_buffer -= i_size;
                    p_buffer += i_size;
                }
                /* */
                if( p_64_sps && p_64_pps &&
                    ( asprintf( &id->psz_fmtp,
                                "packetization-mode=1;profile-level-id=%s;"
                                "sprop-parameter-sets=%s,%s;", hexa, p_64_sps,
                                p_64_pps ) == -1 ) )
                    id->psz_fmtp = NULL;
                free( p_64_sps );
                free( p_64_pps );
            }
            if( !id->psz_fmtp )
                id->psz_fmtp = strdup( "packetization-mode=1" );
            break;

        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        {
            char hexa[2*p_fmt->i_extra +1];

            id->psz_rtpmap = strdup( "MP4V-ES/90000" );
            id->pf_packetize = rtp_packetize_split;
            if( p_fmt->i_extra > 0 )
            {
                sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &id->psz_fmtp,
                              "profile-level-id=3; config=%s;", hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            break;
        }
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        {
            id->i_clock_rate = p_fmt->audio.i_rate;

            if(!p_sys->b_latm)
            {
                char hexa[2*p_fmt->i_extra +1];

                if( asprintf( &id->psz_rtpmap, "mpeg4-generic/%d",
                              p_fmt->audio.i_rate ) == -1 )
                    id->psz_rtpmap = NULL;
                id->pf_packetize = rtp_packetize_mp4a;
                sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &id->psz_fmtp,
                              "streamtype=5; profile-level-id=15; "
                              "mode=AAC-hbr; config=%s; SizeLength=13; "
                              "IndexLength=3; IndexDeltaLength=3; Profile=1;",
                              hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            else
            {
                char hexa[13];
                int i;
                unsigned char config[6];
                unsigned int aacsrates[15] = {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                    16000, 12000, 11025, 8000, 7350, 0, 0 };

                for( i = 0; i < 15; i++ )
                    if( p_fmt->audio.i_rate == aacsrates[i] )
                        break;

                config[0]=0x40;
                config[1]=0;
                config[2]=0x20|i;
                config[3]=p_fmt->audio.i_channels<<4;
                config[4]=0x3f;
                config[5]=0xc0;

                if( asprintf( &id->psz_rtpmap, "MP4A-LATM/%d/%d",
                              p_fmt->audio.i_rate,
                              p_fmt->audio.i_channels ) == -1)
                    id->psz_rtpmap = NULL;
                id->pf_packetize = rtp_packetize_mp4a_latm;
                sprintf_hexa( hexa, config, 6 );
                if( asprintf( &id->psz_fmtp, "profile-level-id=15; "
                              "object=2; cpresent=0; config=%s", hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            break;
        }
        case VLC_FOURCC( 's', 'a', 'm', 'r' ):
            id->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                     "AMR/8000/2" : "AMR/8000" );
            id->psz_fmtp = strdup( "octet-align=1" );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_amr;
            break;
        case VLC_FOURCC( 's', 'a', 'w', 'b' ):
            id->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                     "AMR-WB/16000/2" : "AMR-WB/16000" );
            id->psz_fmtp = strdup( "octet-align=1" );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_amr;
            break;
        case VLC_FOURCC( 't', '1', '4', '0' ):
            id->psz_rtpmap = strdup( "t140/1000" );
            id->i_clock_rate = 1000;
            id->pf_packetize = rtp_packetize_t140;
            break;

        default:
            msg_Err( p_stream, "cannot add this stream (unsupported "
                     "codec:%4.4s)", (char*)&p_fmt->i_codec );
            if( id->sinkc > 0 )
                rtp_del_sink( id, id->sinkv[0].rtp_fd );
            vlc_thread_join( id );
            vlc_mutex_destroy( &id->lock_sink );
            vlc_object_destroy( id );
            return NULL;
    }

    if( cscov != -1 )
        cscov += 8 /* UDP */ + 12 /* RTP */;
    if( id->sinkc > 0 )
        net_SetCSCov( id->sinkv[0].rtp_fd, cscov, -1 );

    if( id->i_payload_type == p_sys->i_payload_type )
        p_sys->i_payload_type++;

    if( p_sys->rtsp != NULL )
        id->rtsp_id = RtspAddId( p_sys->rtsp, id, p_sys->i_es,
                                 p_sys->psz_destination,
                                 p_sys->i_ttl, id->i_port, id->i_port + 1 );

    /* Update p_sys context */
    vlc_mutex_lock( &p_sys->lock_es );
    TAB_APPEND( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    psz_sdp = SDPGenerate( p_stream, NULL );

    vlc_mutex_lock( &p_sys->lock_sdp );
    free( p_sys->psz_sdp );
    p_sys->psz_sdp = psz_sdp;
    vlc_mutex_unlock( &p_sys->lock_sdp );

    p_sys->i_sdp_version++;

    msg_Dbg( p_stream, "sdp=\n%s", p_sys->psz_sdp );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    vlc_object_attach( id, p_stream );
    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_object_kill( id );
    block_FifoWake( id->p_fifo );

    vlc_mutex_lock( &p_sys->lock_es );
    TAB_REMOVE( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    /* Release port */
    if( id->i_port > 0 )
    {
        if( id->i_cat == AUDIO_ES && p_sys->i_port_audio == 0 )
            p_sys->i_port_audio = id->i_port;
        else if( id->i_cat == VIDEO_ES && p_sys->i_port_video == 0 )
            p_sys->i_port_video = id->i_port;
    }

    free( id->psz_rtpmap );
    free( id->psz_fmtp );

    if( id->rtsp_id )
        RtspDelId( p_sys->rtsp, id->rtsp_id );
    if( id->sinkc > 0 )
        rtp_del_sink( id, id->sinkv[0].rtp_fd ); /* sink for explicit dst= */

    vlc_thread_join( id );
    vlc_mutex_destroy( &id->lock_sink );
    block_FifoRelease( id->p_fifo );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap && !p_sys->p_mux ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    vlc_object_detach( id );
    vlc_object_destroy( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    block_t *p_next;

    assert( p_stream->p_sys->p_mux == NULL );

    while( p_buffer != NULL )
    {
        p_next = p_buffer->p_next;
        if( id->pf_packetize( p_stream, id, p_buffer ) )
            break;

        block_Release( p_buffer );
        p_buffer = p_next;
    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * SAP:
 ****************************************************************************/
static int SapSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t   *p_sout = p_stream->p_sout;

    /* Remove the previous session */
    if( p_sys->p_session != NULL)
    {
        sout_AnnounceUnRegister( p_sout, p_sys->p_session);
        p_sys->p_session = NULL;
    }

    if( ( p_sys->i_es > 0 || p_sys->p_mux ) && p_sys->psz_sdp && *p_sys->psz_sdp )
    {
        announce_method_t *p_method = sout_SAPMethod();
        p_sys->p_session = sout_AnnounceRegisterSDP( p_sout, SOUT_CFG_PREFIX,
                                                     p_sys->psz_sdp,
                                                     p_sys->psz_destination,
                                                     p_method );
        sout_MethodRelease( p_method );
    }

    return VLC_SUCCESS;
}

/****************************************************************************
* File:
****************************************************************************/
static int FileSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    FILE            *f;

    if( ( f = utf8_fopen( p_sys->psz_sdp_file, "wt" ) ) == NULL )
    {
        msg_Err( p_stream, "cannot open file '%s' (%s)",
                 p_sys->psz_sdp_file, strerror(errno) );
        return VLC_EGENERIC;
    }

    fputs( p_sys->psz_sdp, f );
    fclose( f );

    return VLC_SUCCESS;
}

/****************************************************************************
 * HTTP:
 ****************************************************************************/
static int  HttpCallback( httpd_file_sys_t *p_args,
                          httpd_file_t *, uint8_t *p_request,
                          uint8_t **pp_data, int *pi_data );

static int HttpSetup( sout_stream_t *p_stream, vlc_url_t *url)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_sys->p_httpd_host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host,
                                         url->i_port > 0 ? url->i_port : 80 );
    if( p_sys->p_httpd_host )
    {
        p_sys->p_httpd_file = httpd_FileNew( p_sys->p_httpd_host,
                                             url->psz_path ? url->psz_path : "/",
                                             "application/sdp",
                                             NULL, NULL, NULL,
                                             HttpCallback, (void*)p_sys );
    }
    if( p_sys->p_httpd_file == NULL )
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int  HttpCallback( httpd_file_sys_t *p_args,
                          httpd_file_t *f, uint8_t *p_request,
                          uint8_t **pp_data, int *pi_data )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t*)p_args;

    vlc_mutex_lock( &p_sys->lock_sdp );
    if( p_sys->psz_sdp && *p_sys->psz_sdp )
    {
        *pi_data = strlen( p_sys->psz_sdp );
        *pp_data = malloc( *pi_data );
        memcpy( *pp_data, p_sys->psz_sdp, *pi_data );
    }
    else
    {
        *pp_data = NULL;
        *pi_data = 0;
    }
    vlc_mutex_unlock( &p_sys->lock_sdp );

    return VLC_SUCCESS;
}

/****************************************************************************
 * RTP send
 ****************************************************************************/
static void ThreadSend( vlc_object_t *p_this )
{
    sout_stream_id_t *id = (sout_stream_id_t *)p_this;
    unsigned i_caching = id->i_caching;
#ifdef HAVE_TEE
    int fd[5] = { -1, -1, -1, -1, -1 };

    if( pipe( fd ) )
        fd[0] = fd[1] = -1;
    else
    if( pipe( fd ) )
        fd[2] = fd[3] = -1;
    else
        fd[4] = open( "/dev/null", O_WRONLY );
#endif

    while( !id->b_die )
    {
        block_t *out = block_FifoGet( id->p_fifo );
        if( out == NULL )
            continue; /* Forced wakeup */

        mtime_t  i_date = out->i_dts + i_caching;
        ssize_t  len = out->i_buffer;

#ifdef HAVE_TEE
        if( fd[4] != -1 )
            len = write( fd[1], out->p_buffer, len);
        if( len == -1 )
            continue; /* Uho - should not happen */
#endif
        mwait( i_date );

        vlc_mutex_lock( &id->lock_sink );
        for( int i = 0; i < id->sinkc; i++ )
        {
            SendRTCP( id->sinkv[i].rtcp, out );

#ifdef HAVE_TEE
            tee( fd[0], fd[3], len, 0 );
            if( splice( fd[2], NULL, id->sinkv[i].rtp_fd, NULL, len,
                        SPLICE_F_NONBLOCK ) >= 0 )
                continue;

            /* splice failed */
            splice( fd[2], NULL, fd[4], NULL, len, 0 );
#endif
            send( id->sinkv[i].rtp_fd, out->p_buffer, len, 0 );
        }
        vlc_mutex_unlock( &id->lock_sink );

        block_Release( out );
#ifdef HAVE_TEE
        splice( fd[0], NULL, fd[4], NULL, len, 0 );
#endif
    }

#ifdef HAVE_TEE
    for( unsigned i = 0; i < 5; i++ )
        close( fd[i] );
#endif
}

static inline void rtp_packetize_send( sout_stream_id_t *id, block_t *out )
{
    block_FifoPut( id->p_fifo, out );
}

int rtp_add_sink( sout_stream_id_t *id, int fd )
{
    rtp_sink_t sink = { fd, NULL };
    sink.rtcp = OpenRTCP( VLC_OBJECT( id->p_stream ), fd, IPPROTO_UDP );
    if( sink.rtcp == NULL )
        msg_Err( id, "RTCP failed!" );

    vlc_mutex_lock( &id->lock_sink );
    INSERT_ELEM( id->sinkv, id->sinkc, id->sinkc, sink );
    vlc_mutex_unlock( &id->lock_sink );
    return VLC_SUCCESS;
}

void rtp_del_sink( sout_stream_id_t *id, int fd )
{
    rtp_sink_t sink = { fd, NULL };

    /* NOTE: must be safe to use if fd is not included */
    vlc_mutex_lock( &id->lock_sink );
    for( int i = 0; i < id->sinkc; i++ )
    {
        if (id->sinkv[i].rtp_fd == fd)
        {
            sink = id->sinkv[i];
            REMOVE_ELEM( id->sinkv, id->sinkc, i );
            break;
        }
    }
    vlc_mutex_unlock( &id->lock_sink );

    CloseRTCP( sink.rtcp );
    net_Close( sink.rtp_fd );
}

/****************************************************************************
 * rtp_packetize_*:
 ****************************************************************************/
static void rtp_packetize_common( sout_stream_id_t *id, block_t *out,
                                  int b_marker, int64_t i_pts )
{
    uint32_t i_timestamp = i_pts * (int64_t)id->i_clock_rate / I64C(1000000);

    out->p_buffer[0] = 0x80;
    out->p_buffer[1] = (b_marker?0x80:0x00)|id->i_payload_type;
    out->p_buffer[2] = ( id->i_sequence >> 8)&0xff;
    out->p_buffer[3] = ( id->i_sequence     )&0xff;
    out->p_buffer[4] = ( i_timestamp >> 24 )&0xff;
    out->p_buffer[5] = ( i_timestamp >> 16 )&0xff;
    out->p_buffer[6] = ( i_timestamp >>  8 )&0xff;
    out->p_buffer[7] = ( i_timestamp       )&0xff;

    memcpy( out->p_buffer + 8, id->ssrc, 4 );

    out->i_buffer = 12;
    id->i_sequence++;
}

static int rtp_packetize_mpa( sout_stream_t *p_stream, sout_stream_id_t *id,
                              block_t *in )
{
    int     i_max   = id->i_mtu - 12 - 4; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* mbz set to 0 */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 0;
        /* fragment offset in the current frame */
        out->p_buffer[14] = ( (i*i_max) >> 8 )&0xff;
        out->p_buffer[15] = ( (i*i_max)      )&0xff;
        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc2250 */
static int rtp_packetize_mpv( sout_stream_t *p_stream, sout_stream_id_t *id,
                              block_t *in )
{
    int     i_max   = id->i_mtu - 12 - 4; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;
    int     b_sequence_start = 0;
    int     i_temporal_ref = 0;
    int     i_picture_coding_type = 0;
    int     i_fbv = 0, i_bfc = 0, i_ffv = 0, i_ffc = 0;
    int     b_start_slice = 0;

    /* preparse this packet to get some info */
    if( in->i_buffer > 4 )
    {
        uint8_t *p = p_data;
        int      i_rest = in->i_buffer;

        for( ;; )
        {
            while( i_rest > 4 &&
                   ( p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x01 ) )
            {
                p++;
                i_rest--;
            }
            if( i_rest <= 4 )
            {
                break;
            }
            p += 3;
            i_rest -= 4;

            if( *p == 0xb3 )
            {
                /* sequence start code */
                b_sequence_start = 1;
            }
            else if( *p == 0x00 && i_rest >= 4 )
            {
                /* picture */
                i_temporal_ref = ( p[1] << 2) |((p[2]>>6)&0x03);
                i_picture_coding_type = (p[2] >> 3)&0x07;

                if( i_rest >= 4 && ( i_picture_coding_type == 2 ||
                                    i_picture_coding_type == 3 ) )
                {
                    i_ffv = (p[3] >> 2)&0x01;
                    i_ffc = ((p[3]&0x03) << 1)|((p[4]>>7)&0x01);
                    if( i_rest > 4 && i_picture_coding_type == 3 )
                    {
                        i_fbv = (p[4]>>6)&0x01;
                        i_bfc = (p[4]>>3)&0x07;
                    }
                }
            }
            else if( *p <= 0xaf )
            {
                b_start_slice = 1;
            }
        }
    }

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream,
                                             16 + i_payload );
        uint32_t      h = ( i_temporal_ref << 16 )|
                          ( b_sequence_start << 13 )|
                          ( b_start_slice << 12 )|
                          ( i == i_count - 1 ? 1 << 11 : 0 )|
                          ( i_picture_coding_type << 8 )|
                          ( i_fbv << 7 )|( i_bfc << 4 )|( i_ffv << 3 )|i_ffc;

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0,
                              in->i_pts > 0 ? in->i_pts : in->i_dts );

        /* MBZ:5 T:1 TR:10 AN:1 N:1 S:1 B:1 E:1 P:3 FBV:1 BFC:3 FFV:1 FFC:3 */
        out->p_buffer[12] = ( h >> 24 )&0xff;
        out->p_buffer[13] = ( h >> 16 )&0xff;
        out->p_buffer[14] = ( h >>  8 )&0xff;
        out->p_buffer[15] = ( h       )&0xff;

        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_ac3( sout_stream_t *p_stream, sout_stream_id_t *id,
                              block_t *in )
{
    int     i_max   = id->i_mtu - 12 - 2; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 14 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* unit count */
        out->p_buffer[12] = 1;
        /* unit header */
        out->p_buffer[13] = 0x00;
        /* data */
        memcpy( &out->p_buffer[14], p_data, i_payload );

        out->i_buffer   = 14 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_split( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_buffer   = 12 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc3016 */
static int rtp_packetize_mp4a_latm( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *in )
{
    int     i_max   = id->i_mtu - 14;              /* payload max in one packet */
    int     latmhdrsize = in->i_buffer / 0xff + 1;
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer, *p_header = NULL;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int     i_payload = __MIN( i_max, i_data );
        block_t *out;

        if( i != 0 )
            latmhdrsize = 0;
        out = block_New( p_stream, 12 + latmhdrsize + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1) ? 1 : 0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );

        if( i == 0 )
        {
            int tmp = in->i_buffer;

            p_header=out->p_buffer+12;
            while( tmp > 0xfe )
            {
                *p_header = 0xff;
                p_header++;
                tmp -= 0xff;
            }
            *p_header = tmp;
        }

        memcpy( &out->p_buffer[12+latmhdrsize], p_data, i_payload );

        out->i_buffer   = 12 + latmhdrsize + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_l16( sout_stream_t *p_stream, sout_stream_id_t *id,
                              block_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i_packet = 0;

    while( i_data > 0 )
    {
        int           i_payload = (__MIN( i_max, i_data )/4)*4;
        block_t *out = block_New( p_stream, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, 0,
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_buffer   = 12 + i_payload;
        out->i_dts    = in->i_dts + i_packet * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
        i_packet++;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_l8( sout_stream_t *p_stream, sout_stream_id_t *id,
                             block_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i_packet = 0;

    while( i_data > 0 )
    {
        int           i_payload = (__MIN( i_max, i_data )/2)*2;
        block_t *out = block_New( p_stream, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, 0,
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_buffer   = 12 + i_payload;
        out->i_dts    = in->i_dts + i_packet * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
        i_packet++;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_mp4a( sout_stream_t *p_stream, sout_stream_id_t *id,
                               block_t *in )
{
    int     i_max   = id->i_mtu - 16; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* AU headers */
        /* AU headers length (bits) */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 2*8;
        /* for each AU length 13 bits + idx 3bits, */
        out->p_buffer[14] = ( in->i_buffer >> 5 )&0xff;
        out->p_buffer[15] = ( (in->i_buffer&0xff)<<3 )|0;

        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_buffer   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}


/* rfc2429 */
#define RTP_H263_HEADER_SIZE (2)  // plen = 0
#define RTP_H263_PAYLOAD_START (14)  // plen = 0
static int rtp_packetize_h263( sout_stream_t *p_stream, sout_stream_id_t *id,
                               block_t *in )
{
    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;
    int     i_max   = id->i_mtu - 12 - RTP_H263_HEADER_SIZE; /* payload max in one packet */
    int     i_count;
    int     b_p_bit;
    int     b_v_bit = 0; // no pesky error resilience
    int     i_plen = 0; // normally plen=0 for PSC packet
    int     i_pebit = 0; // because plen=0
    uint16_t h;

    if( i_data < 2 )
    {
        return VLC_EGENERIC;
    }
    if( p_data[0] || p_data[1] )
    {
        return VLC_EGENERIC;
    }
    /* remove 2 leading 0 bytes */
    p_data += 2;
    i_data -= 2;
    i_count = ( i_data + i_max - 1 ) / i_max;

    for( i = 0; i < i_count; i++ )
    {
        int      i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream,
                                  RTP_H263_PAYLOAD_START + i_payload );
        b_p_bit = (i == 0) ? 1 : 0;
        h = ( b_p_bit << 10 )|
            ( b_v_bit << 9  )|
            ( i_plen  << 3  )|
              i_pebit;

        /* rtp common header */
        //b_m_bit = 1; // always contains end of frame
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0,
                              in->i_pts > 0 ? in->i_pts : in->i_dts );

        /* h263 header */
        out->p_buffer[12] = ( h >>  8 )&0xff;
        out->p_buffer[13] = ( h       )&0xff;
        memcpy( &out->p_buffer[RTP_H263_PAYLOAD_START], p_data, i_payload );

        out->i_buffer = RTP_H263_PAYLOAD_START + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc3984 */
static int
rtp_packetize_h264_nal( sout_stream_t *p_stream, sout_stream_id_t *id,
                        const uint8_t *p_data, int i_data, int64_t i_pts,
                        int64_t i_dts, vlc_bool_t b_last, int64_t i_length )
{
    const int i_max = id->i_mtu - 12; /* payload max in one packet */
    int i_nal_hdr;
    int i_nal_type;

    if( i_data < 5 )
        return VLC_SUCCESS;

    i_nal_hdr = p_data[3];
    i_nal_type = i_nal_hdr&0x1f;

    /* Skip start code */
    p_data += 3;
    i_data -= 3;

    /* */
    if( i_data <= i_max )
    {
        /* Single NAL unit packet */
        block_t *out = block_New( p_stream, 12 + i_data );
        out->i_dts    = i_dts;
        out->i_length = i_length;

        /* */
        rtp_packetize_common( id, out, b_last, i_pts );
        out->i_buffer = 12 + i_data;

        memcpy( &out->p_buffer[12], p_data, i_data );

        rtp_packetize_send( id, out );
    }
    else
    {
        /* FU-A Fragmentation Unit without interleaving */
        const int i_count = ( i_data-1 + i_max-2 - 1 ) / (i_max-2);
        int i;

        p_data++;
        i_data--;

        for( i = 0; i < i_count; i++ )
        {
            const int i_payload = __MIN( i_data, i_max-2 );
            block_t *out = block_New( p_stream, 12 + 2 + i_payload );
            out->i_dts    = i_dts + i * i_length / i_count;
            out->i_length = i_length / i_count;

            /* */
            rtp_packetize_common( id, out, (b_last && i_payload == i_data), i_pts );
            out->i_buffer = 14 + i_payload;

            /* FU indicator */
            out->p_buffer[12] = 0x00 | (i_nal_hdr & 0x60) | 28;
            /* FU header */
            out->p_buffer[13] = ( i == 0 ? 0x80 : 0x00 ) | ( (i == i_count-1) ? 0x40 : 0x00 )  | i_nal_type;
            memcpy( &out->p_buffer[14], p_data, i_payload );

            rtp_packetize_send( id, out );

            i_data -= i_payload;
            p_data += i_payload;
        }
    }
    return VLC_SUCCESS;
}

static int rtp_packetize_h264( sout_stream_t *p_stream, sout_stream_id_t *id,
                               block_t *in )
{
    const uint8_t *p_buffer = in->p_buffer;
    int i_buffer = in->i_buffer;

    while( i_buffer > 4 && ( p_buffer[0] != 0 || p_buffer[1] != 0 || p_buffer[2] != 1 ) )
    {
        i_buffer--;
        p_buffer++;
    }

    /* Split nal units */
    while( i_buffer > 4 )
    {
        int i_offset;
        int i_size = i_buffer;
        int i_skip = i_buffer;

        /* search nal end */
        for( i_offset = 4; i_offset+2 < i_buffer ; i_offset++)
        {
            if( p_buffer[i_offset] == 0 && p_buffer[i_offset+1] == 0 && p_buffer[i_offset+2] == 1 )
            {
                /* we found another startcode */
                i_size = i_offset - ( p_buffer[i_offset-1] == 0 ? 1 : 0);
                i_skip = i_offset;
                break;
            }
        }
        /* TODO add STAP-A to remove a lot of overhead with small slice/sei/... */
        rtp_packetize_h264_nal( p_stream, id, p_buffer, i_size,
                                (in->i_pts > 0 ? in->i_pts : in->i_dts), in->i_dts,
                                (i_size >= i_buffer), in->i_length * i_size / in->i_buffer );

        i_buffer -= i_skip;
        p_buffer += i_skip;
    }
    return VLC_SUCCESS;
}

static int rtp_packetize_amr( sout_stream_t *p_stream, sout_stream_id_t *id,
                              block_t *in )
{
    int     i_max   = id->i_mtu - 14; /* payload max in one packet */
    int     i_count = ( in->i_buffer + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_buffer;
    int     i;

    /* Only supports octet-aligned mode */
    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        block_t *out = block_New( p_stream, 14 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* Payload header */
        out->p_buffer[12] = 0xF0; /* CMR */
        out->p_buffer[13] = p_data[0]&0x7C; /* ToC */ /* FIXME: frame type */

        /* FIXME: are we fed multiple frames ? */
        memcpy( &out->p_buffer[14], p_data+1, i_payload-1 );

        out->i_buffer   = 14 + i_payload-1;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_t140( sout_stream_t *p_stream, sout_stream_id_t *id,
                               block_t *in )
{
    const size_t   i_max  = id->i_mtu - 12;
    const uint8_t *p_data = in->p_buffer;
    size_t         i_data = in->i_buffer;

    for( unsigned i_packet = 0; i_data > 0; i_packet++ )
    {
        size_t i_payload = i_data;

        /* Make sure we stop on an UTF-8 character boundary
         * (assuming the input is valid UTF-8) */
        if( i_data > i_max )
        {
            i_payload = i_max;

            while( ( p_data[i_payload] & 0xC0 ) == 0x80 )
            {
                if( i_payload == 0 )
                    return VLC_SUCCESS; /* fishy input! */

                i_payload--;
            }
        }

        block_t *out = block_New( p_stream, 12 + i_payload );
        if( out == NULL )
            return VLC_SUCCESS;

        rtp_packetize_common( id, out, 0, in->i_pts + i_packet );
        memcpy( out->p_buffer + 12, p_data, i_payload );

        out->i_buffer = 12 + i_payload;
        out->i_dts    = out->i_pts;
        out->i_length = 0;

        rtp_packetize_send( id, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Non-RTP mux
 *****************************************************************************/

/** Add an ES to a non-RTP muxed stream */
static sout_stream_id_t *MuxAdd( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_input_t      *p_input;
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    p_input = sout_MuxAddStream( p_mux, p_fmt );
    if( p_input == NULL )
    {
        msg_Err( p_stream, "cannot add this stream to the muxer" );
        return NULL;
    }

    return (sout_stream_id_t *)p_input;
}


static int MuxSend( sout_stream_t *p_stream, sout_stream_id_t *id,
                    block_t *p_buffer )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    sout_MuxSendBuffer( p_mux, (sout_input_t *)id, p_buffer );
    return VLC_SUCCESS;
}


/** Remove an ES from a non-RTP muxed stream */
static int MuxDel( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    sout_MuxDeleteStream( p_mux, (sout_input_t *)id );
    return VLC_SUCCESS;
}


static int AccessOutGrabberWriteBuffer( sout_stream_t *p_stream,
                                        const block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id = p_sys->es[0];

    int64_t  i_dts = p_buffer->i_dts;

    uint8_t         *p_data = p_buffer->p_buffer;
    unsigned int    i_data  = p_buffer->i_buffer;
    unsigned int    i_max   = id->i_mtu - 12;

    unsigned i_packet = ( p_buffer->i_buffer + i_max - 1 ) / i_max;

    while( i_data > 0 )
    {
        unsigned int i_size;

        /* output complete packet */
        if( p_sys->packet &&
            p_sys->packet->i_buffer + i_data > i_max )
        {
            rtp_packetize_send( id, p_sys->packet );
            p_sys->packet = NULL;
        }

        if( p_sys->packet == NULL )
        {
            /* allocate a new packet */
            p_sys->packet = block_New( p_stream, id->i_mtu );
            rtp_packetize_common( id, p_sys->packet, 1, i_dts );
            p_sys->packet->i_dts = i_dts;
            p_sys->packet->i_length = p_buffer->i_length / i_packet;
            i_dts += p_sys->packet->i_length;
        }

        i_size = __MIN( i_data,
                        (unsigned)(id->i_mtu - p_sys->packet->i_buffer) );

        memcpy( &p_sys->packet->p_buffer[p_sys->packet->i_buffer],
                p_data, i_size );

        p_sys->packet->i_buffer += i_size;
        p_data += i_size;
        i_data -= i_size;
    }

    return VLC_SUCCESS;
}


static int AccessOutGrabberWrite( sout_access_out_t *p_access,
                                  block_t *p_buffer )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_access->p_sys;

    while( p_buffer )
    {
        block_t *p_next;

        AccessOutGrabberWriteBuffer( p_stream, p_buffer );

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}


static sout_access_out_t *GrabberCreate( sout_stream_t *p_stream )
{
    sout_access_out_t *p_grab;

    p_grab = vlc_object_create( p_stream->p_sout, sizeof( *p_grab ) );
    if( p_grab == NULL )
        return NULL;

    p_grab->p_module    = NULL;
    p_grab->p_sout      = p_stream->p_sout;
    p_grab->psz_access  = strdup( "grab" );
    p_grab->p_cfg       = NULL;
    p_grab->psz_path    = strdup( "" );
    p_grab->p_sys       = (sout_access_out_sys_t *)p_stream;
    p_grab->pf_seek     = NULL;
    p_grab->pf_write    = AccessOutGrabberWrite;
    return p_grab;
}
