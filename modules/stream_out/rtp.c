/*****************************************************************************
 * rtp.c: rtp stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004, 2010 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_fs.h>
#include <vlc_rand.h>
#include <vlc_memstream.h>
#ifdef HAVE_SRTP
# include <srtp.h>
# include <gcrypt.h>
# include <vlc_gcrypt.h>
#endif

#include "rtp.h"

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_ARPA_INET_H
#   include <arpa/inet.h>
#endif
#ifdef HAVE_LINUX_DCCP_H
#   include <linux/dccp.h>
#endif
#ifndef IPPROTO_DCCP
# define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136
#endif

#include <ctype.h>
#include <errno.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define DEST_TEXT N_("Destination")
#define DEST_LONGTEXT N_( \
    "This is the output URL that will be used." )
#define SDP_TEXT N_("SDP")
#define SDP_LONGTEXT N_( \
    "This allows you to specify how the SDP (Session Descriptor) for this RTP "\
    "session will be made available. You must use a url: http://location to " \
    "access the SDP via HTTP, rtsp://location for RTSP access, and sap:// " \
    "for the SDP to be announced via SAP." )
#define SAP_TEXT N_("SAP announcing")
#define SAP_LONGTEXT N_("Announce this session with SAP.")
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_( \
    "This allows you to specify the muxer used for the streaming output. " \
    "Default is to use no muxer (standard RTP stream)." )

#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "This is the name of the session that will be announced in the SDP " \
    "(Session Descriptor)." )
#define CAT_TEXT N_("Session category")
#define CAT_LONGTEXT N_( \
  "This allows you to specify a category for the session, " \
  "that will be announced if you choose to use SAP." )
#define DESC_TEXT N_("Session description")
#define DESC_LONGTEXT N_( \
    "This allows you to give a short description with details about the stream, " \
    "that will be announced in the SDP (Session Descriptor)." )
#define URL_TEXT N_("Session URL")
#define URL_LONGTEXT N_( \
    "This allows you to give a URL with more details about the stream " \
    "(often the website of the streaming organization), that will " \
    "be announced in the SDP (Session Descriptor)." )
#define EMAIL_TEXT N_("Session email")
#define EMAIL_LONGTEXT N_( \
    "This allows you to give a contact mail address for the stream, that will " \
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
    "the multicast packets sent by the stream output (-1 = use operating " \
    "system built-in default).")

#define RTCP_MUX_TEXT N_("RTP/RTCP multiplexing")
#define RTCP_MUX_LONGTEXT N_( \
    "This sends and receives RTCP packet multiplexed over the same port " \
    "as RTP packets." )

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Default caching value for outbound RTP streams. This " \
    "value should be set in milliseconds." )

#define PROTO_TEXT N_("Transport protocol")
#define PROTO_LONGTEXT N_( \
    "This selects which transport protocol to use for RTP." )

#define SRTP_KEY_TEXT N_("SRTP key (hexadecimal)")
#define SRTP_KEY_LONGTEXT N_( \
    "RTP packets will be integrity-protected and ciphered "\
    "with this Secure RTP master shared secret key. "\
    "This must be a 32-character-long hexadecimal string.")

#define SRTP_SALT_TEXT N_("SRTP salt (hexadecimal)")
#define SRTP_SALT_LONGTEXT N_( \
    "Secure RTP requires a (non-secret) master salt value. " \
    "This must be a 28-character-long hexadecimal string.")

static const char *const ppsz_protos[] = {
    "dccp", "sctp", "tcp", "udp", "udplite",
};

static const char *const ppsz_protocols[] = {
    "DCCP", "SCTP", "TCP", "UDP", "UDP-Lite",
};

#define RFC3016_TEXT N_("MP4A LATM")
#define RFC3016_LONGTEXT N_( \
    "This allows you to stream MPEG4 LATM audio streams (see RFC3016)." )

#define RTSP_TIMEOUT_TEXT N_( "RTSP session timeout (s)" )
#define RTSP_TIMEOUT_LONGTEXT N_( "RTSP sessions will be closed after " \
    "not receiving any RTSP request for this long. Setting it to a " \
    "negative value or zero disables timeouts. The default is 60 (one " \
    "minute)." )

#define RTSP_USER_TEXT N_("Username")
#define RTSP_USER_LONGTEXT N_("Username that will be " \
                              "requested to access the stream." )
#define RTSP_PASS_TEXT N_("Password")
#define RTSP_PASS_LONGTEXT N_("Password that will be " \
                              "requested to access the stream." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-rtp-"
#define MAX_EMPTY_BLOCKS 200

vlc_module_begin ()
    set_shortname( N_("RTP"))
    set_description( N_("RTP stream output") )
    set_capability( "sout stream", 0 )
    add_shortcut( "rtp", "vod" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )

    add_string( SOUT_CFG_PREFIX "dst", "", DEST_TEXT,
                DEST_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "sdp", "", SDP_TEXT,
                SDP_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "mux", "", MUX_TEXT,
                MUX_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "sap", false, SAP_TEXT, SAP_LONGTEXT,
              true )

    add_string( SOUT_CFG_PREFIX "name", "", NAME_TEXT,
                NAME_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "cat", "", CAT_TEXT, CAT_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "description", "", DESC_TEXT,
                DESC_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "url", "", URL_TEXT,
                URL_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "email", "", EMAIL_TEXT,
                EMAIL_LONGTEXT, true )
    add_obsolete_string( SOUT_CFG_PREFIX "phone" ) /* since 3.0.0 */

    add_string( SOUT_CFG_PREFIX "proto", "udp", PROTO_TEXT,
                PROTO_LONGTEXT, false )
        change_string_list( ppsz_protos, ppsz_protocols )
    add_integer( SOUT_CFG_PREFIX "port", 5004, PORT_TEXT,
                 PORT_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "port-audio", 0, PORT_AUDIO_TEXT,
                 PORT_AUDIO_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "port-video", 0, PORT_VIDEO_TEXT,
                 PORT_VIDEO_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "ttl", -1, TTL_TEXT,
                 TTL_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "rtcp-mux", false,
              RTCP_MUX_TEXT, RTCP_MUX_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000,
                 CACHING_TEXT, CACHING_LONGTEXT, true )

#ifdef HAVE_SRTP
    add_string( SOUT_CFG_PREFIX "key", "",
                SRTP_KEY_TEXT, SRTP_KEY_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "salt", "",
                SRTP_SALT_TEXT, SRTP_SALT_LONGTEXT, false )
#endif

    add_bool( SOUT_CFG_PREFIX "mp4a-latm", false, RFC3016_TEXT,
                 RFC3016_LONGTEXT, false )

    set_callbacks( Open, Close )

    add_submodule ()
    set_shortname( N_("RTSP VoD" ) )
    set_description( N_("RTSP VoD server") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_VOD )
    set_capability( "vod server", 10 )
    set_callbacks( OpenVoD, CloseVoD )
    add_shortcut( "rtsp" )
    add_integer( "rtsp-timeout", 60, RTSP_TIMEOUT_TEXT,
                 RTSP_TIMEOUT_LONGTEXT, true )
    add_string( "sout-rtsp-user", "",
                RTSP_USER_TEXT, RTSP_USER_LONGTEXT, true )
    add_password( "sout-rtsp-pwd", "",
                  RTSP_PASS_TEXT, RTSP_PASS_LONGTEXT, true )

vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "dst", "name", "cat", "port", "port-audio", "port-video", "*sdp", "ttl",
    "mux", "sap", "description", "url", "email",
    "proto", "rtcp-mux", "caching",
#ifdef HAVE_SRTP
    "key", "salt",
#endif
    "mp4a-latm", NULL
};

static sout_stream_id_sys_t *Add( sout_stream_t *, const es_format_t * );
static void              Del ( sout_stream_t *, sout_stream_id_sys_t * );
static int               Send( sout_stream_t *, sout_stream_id_sys_t *,
                               block_t* );
static sout_stream_id_sys_t *MuxAdd( sout_stream_t *, const es_format_t * );
static void              MuxDel ( sout_stream_t *, sout_stream_id_sys_t * );
static int               MuxSend( sout_stream_t *, sout_stream_id_sys_t *,
                                  block_t* );

static sout_access_out_t *GrabberCreate( sout_stream_t *p_sout );
static void* ThreadSend( void * );
static void *rtp_listen_thread( void * );

static void SDPHandleUrl( sout_stream_t *, const char * );

static int SapSetup( sout_stream_t *p_stream );
static int FileSetup( sout_stream_t *p_stream );
static int HttpSetup( sout_stream_t *p_stream, const vlc_url_t * );

static int64_t rtp_init_ts( const vod_media_t *p_media,
                            const char *psz_vod_session );

struct sout_stream_sys_t
{
    /* SDP */
    char    *psz_sdp;
    vlc_mutex_t  lock_sdp;

    /* SDP to disk */
    char *psz_sdp_file;

    /* SDP via SAP */
    bool b_export_sap;
    session_descriptor_t *p_session;

    /* SDP via HTTP */
    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    /* RTSP */
    rtsp_stream_t *rtsp;

    /* RTSP NPT and timestamp computations */
    mtime_t      i_npt_zero;    /* when NPT=0 packet is sent */
    int64_t      i_pts_zero;    /* predicts PTS of NPT=0 packet */
    int64_t      i_pts_offset;  /* matches actual PTS to prediction */
    vlc_mutex_t  lock_ts;

    /* */
    char     *psz_destination;
    uint16_t  i_port;
    uint16_t  i_port_audio;
    uint16_t  i_port_video;
    uint8_t   proto;
    bool      rtcp_mux;
    bool      b_latm;

    /* VoD */
    vod_media_t *p_vod_media;
    char     *psz_vod_session;

    /* in case we do TS/PS over rtp */
    sout_mux_t        *p_mux;
    sout_access_out_t *p_grab;
    block_t           *packet;

    /* */
    vlc_mutex_t      lock_es;
    int              i_es;
    sout_stream_id_sys_t **es;
};

typedef struct rtp_sink_t
{
    int rtp_fd;
    rtcp_sender_t *rtcp;
} rtp_sink_t;

struct sout_stream_id_sys_t
{
    sout_stream_t *p_stream;
    /* rtp field */
    /* For RFC 4175, seqnum is extended to 32-bits */
    uint32_t    i_sequence;
    bool        b_first_packet;
    bool        b_ts_init;
    uint32_t    i_ts_offset;
    uint8_t     ssrc[4];

    /* for rtsp */
    uint16_t    i_seq_sent_next;

    /* for sdp */
    rtp_format_t rtp_fmt;
    int          i_port;

    /* Packetizer specific fields */
    int                 i_mtu;
#ifdef HAVE_SRTP
    srtp_session_t     *srtp;
#endif

    /* Packets sinks */
    vlc_thread_t      thread;
    vlc_mutex_t       lock_sink;
    int               sinkc;
    rtp_sink_t       *sinkv;
    rtsp_stream_id_t *rtsp_id;
    struct {
        int          *fd;
        vlc_thread_t  thread;
    } listen;

    block_fifo_t     *p_fifo;
    int64_t           i_caching;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys = NULL;
    char                *psz;
    bool          b_rtsp = false;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX,
                       ppsz_sout_options, p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->psz_destination = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "dst" );

    p_sys->i_port       = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port" );
    p_sys->i_port_audio = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-audio" );
    p_sys->i_port_video = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-video" );
    p_sys->rtcp_mux     = var_GetBool( p_stream, SOUT_CFG_PREFIX "rtcp-mux" );

    if( p_sys->i_port_audio && p_sys->i_port_video == p_sys->i_port_audio )
    {
        msg_Err( p_stream, "audio and video RTP port must be distinct" );
        free( p_sys->psz_destination );
        free( p_sys );
        return VLC_EGENERIC;
    }

    for( config_chain_t *p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
    {
        if( !strcmp( p_cfg->psz_name, "sdp" )
         && ( p_cfg->psz_value != NULL )
         && !strncasecmp( p_cfg->psz_value, "rtsp:", 5 ) )
        {
            b_rtsp = true;
            break;
        }
    }
    if( !b_rtsp )
    {
        psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "sdp" );
        if( psz != NULL )
        {
            if( !strncasecmp( psz, "rtsp:", 5 ) )
                b_rtsp = true;
            free( psz );
        }
    }

    /* Transport protocol */
    p_sys->proto = IPPROTO_UDP;
    psz = var_GetNonEmptyString (p_stream, SOUT_CFG_PREFIX"proto");

    if ((psz == NULL) || !strcasecmp (psz, "udp"))
        (void)0; /* default */
    else
    if (!strcasecmp (psz, "dccp"))
    {
        p_sys->proto = IPPROTO_DCCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#if 0
    else
    if (!strcasecmp (psz, "sctp"))
    {
        p_sys->proto = IPPROTO_TCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#endif
#if 0
    else
    if (!strcasecmp (psz, "tcp"))
    {
        p_sys->proto = IPPROTO_TCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#endif
    else
    if (!strcasecmp (psz, "udplite") || !strcasecmp (psz, "udp-lite"))
        p_sys->proto = IPPROTO_UDPLITE;
    else
        msg_Warn (p_this, "unknown or unsupported transport protocol \"%s\"",
                  psz);
    free (psz);
    var_Create (p_this, "dccp-service", VLC_VAR_STRING);

    p_sys->p_vod_media = NULL;
    p_sys->psz_vod_session = NULL;

    if (! strcmp(p_stream->psz_name, "vod"))
    {
        /* The VLM stops all instances before deleting a media, so this
         * reference will remain valid during the lifetime of the rtp
         * stream output. */
        p_sys->p_vod_media = var_InheritAddress(p_stream, "vod-media");

        if (p_sys->p_vod_media != NULL)
        {
            p_sys->psz_vod_session = var_InheritString(p_stream, "vod-session");
            if (p_sys->psz_vod_session == NULL)
            {
                msg_Err(p_stream, "missing VoD session");
                free(p_sys);
                return VLC_EGENERIC;
            }

            const char *mux = vod_get_mux(p_sys->p_vod_media);
            var_SetString(p_stream, SOUT_CFG_PREFIX "mux", mux);
        }
    }

    if( p_sys->psz_destination == NULL && !b_rtsp
        && p_sys->p_vod_media == NULL )
    {
        msg_Err( p_stream, "missing destination and not in RTSP mode" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    int i_ttl = var_GetInteger( p_stream, SOUT_CFG_PREFIX "ttl" );
    if( i_ttl != -1 )
    {
        var_Create( p_stream, "ttl", VLC_VAR_INTEGER );
        var_SetInteger( p_stream, "ttl", i_ttl );
    }

    p_sys->b_latm = var_GetBool( p_stream, SOUT_CFG_PREFIX "mp4a-latm" );

    /* NPT=0 time will be determined when we packetize the first packet
     * (of any ES). But we want to be able to report rtptime in RTSP
     * without waiting (and already did in the VoD case). So until then,
     * we use an arbitrary reference PTS for timestamp computations, and
     * then actual PTS will catch up using offsets. */
    p_sys->i_npt_zero = VLC_TS_INVALID;
    p_sys->i_pts_zero = rtp_init_ts(p_sys->p_vod_media,
                                    p_sys->psz_vod_session);
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->rtsp = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->b_export_sap = false;
    p_sys->p_session = NULL;
    p_sys->psz_sdp_file = NULL;

    p_sys->p_httpd_host = NULL;
    p_sys->p_httpd_file = NULL;

    p_stream->p_sys     = p_sys;

    vlc_mutex_init( &p_sys->lock_sdp );
    vlc_mutex_init( &p_sys->lock_ts );
    vlc_mutex_init( &p_sys->lock_es );

    psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );
    if( psz != NULL )
    {
        /* Check muxer type */
        if( strncasecmp( psz, "ps", 2 )
         && strncasecmp( psz, "mpeg1", 5 )
         && strncasecmp( psz, "ts", 2 ) )
        {
            msg_Err( p_stream, "unsupported muxer type for RTP (only TS/PS)" );
            free( psz );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_ts );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys->psz_vod_session );
            free( p_sys->psz_destination );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_sys->p_grab = GrabberCreate( p_stream );
        p_sys->p_mux = sout_MuxNew( p_stream->p_sout, psz, p_sys->p_grab );
        free( psz );

        if( p_sys->p_mux == NULL )
        {
            msg_Err( p_stream, "cannot create muxer" );
            sout_AccessOutDelete( p_sys->p_grab );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_ts );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys->psz_vod_session );
            free( p_sys->psz_destination );
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
    p_stream->pace_nocontrol = true;

    if( var_GetBool( p_stream, SOUT_CFG_PREFIX"sap" ) )
        SDPHandleUrl( p_stream, "sap" );

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

    if( p_sys->p_mux != NULL )
    {
        sout_stream_id_sys_t *id = Add( p_stream, NULL );
        if( id == NULL )
        {
            Close( p_this );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->p_mux )
    {
        assert( p_sys->i_es <= 1 );

        sout_MuxDelete( p_sys->p_mux );
        if ( p_sys->i_es > 0 )
            Del( p_stream, p_sys->es[0] );
        sout_AccessOutDelete( p_sys->p_grab );

        if( p_sys->packet )
        {
            block_Release( p_sys->packet );
        }
    }

    if( p_sys->rtsp != NULL )
        RtspUnsetup( p_sys->rtsp );

    vlc_mutex_destroy( &p_sys->lock_sdp );
    vlc_mutex_destroy( &p_sys->lock_ts );
    vlc_mutex_destroy( &p_sys->lock_es );

    if( p_sys->p_httpd_file )
        httpd_FileDelete( p_sys->p_httpd_file );

    if( p_sys->p_httpd_host )
        httpd_HostDelete( p_sys->p_httpd_host );

    free( p_sys->psz_sdp );

    if( p_sys->psz_sdp_file != NULL )
    {
        unlink( p_sys->psz_sdp_file );
        free( p_sys->psz_sdp_file );
    }
    free( p_sys->psz_vod_session );
    free( p_sys->psz_destination );
    free( p_sys );
}

/*****************************************************************************
 * SDPHandleUrl:
 *****************************************************************************/
static void SDPHandleUrl( sout_stream_t *p_stream, const char *psz_url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_url_t url;

    vlc_UrlParse( &url, psz_url );
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

        if( url.psz_host != NULL && *url.psz_host )
        {
            msg_Warn( p_stream, "\"%s\" RTSP host might be ignored in "
                      "multiple-host configurations, use at your own risks.",
                      url.psz_host );
            msg_Info( p_stream, "Consider passing --rtsp-host=IP on the "
                                "command line instead." );

            var_Create( p_stream, "rtsp-host", VLC_VAR_STRING );
            var_SetString( p_stream, "rtsp-host", url.psz_host );
        }
        if( url.i_port != 0 )
        {
            /* msg_Info( p_stream, "Consider passing --rtsp-port=%u on "
                      "the command line instead.", url.i_port ); */

            var_Create( p_stream, "rtsp-port", VLC_VAR_INTEGER );
            var_SetInteger( p_stream, "rtsp-port", url.i_port );
        }

        p_sys->rtsp = RtspSetup( VLC_OBJECT(p_stream), NULL, url.psz_path );
        if( p_sys->rtsp == NULL )
            msg_Err( p_stream, "cannot export SDP as RTSP" );
    }
    else if( ( url.psz_protocol && !strcasecmp( url.psz_protocol, "sap" ) ) ||
             ( url.psz_host && !strcasecmp( url.psz_host, "sap" ) ) )
    {
        p_sys->b_export_sap = true;
        SapSetup( p_stream );
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "file" ) )
    {
        if( p_sys->psz_sdp_file != NULL )
        {
            msg_Err( p_stream, "you can use sdp=file:// only once" );
            goto out;
        }
        p_sys->psz_sdp_file = vlc_uri2path( psz_url );
        if( p_sys->psz_sdp_file == NULL )
            goto out;
        FileSetup( p_stream );
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
char *SDPGenerate( sout_stream_t *p_stream, const char *rtsp_url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    struct vlc_memstream sdp;
    struct sockaddr_storage dst;
    char *psz_sdp = NULL;
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

    vlc_mutex_lock( &p_sys->lock_es );
    if( unlikely(p_sys->i_es == 0 || (rtsp_url != NULL && !p_sys->es[0]->rtsp_id)) )
        goto out; /* hmm... */

    if( p_sys->psz_destination != NULL )
    {
        inclport = 1;

        /* Oh boy, this is really ugly! */
        dstlen = sizeof( dst );
        if( p_sys->es[0]->listen.fd != NULL )
            getsockname( p_sys->es[0]->listen.fd[0],
                         (struct sockaddr *)&dst, &dstlen );
        else
            getpeername( p_sys->es[0]->sinkv[0].rtp_fd,
                         (struct sockaddr *)&dst, &dstlen );
    }
    else
    {
        inclport = 0;

        /* Check against URL format rtsp://[<ipv6>]:<port>/<path> */
        bool ipv6 = rtsp_url != NULL && strlen( rtsp_url ) > 7
                    && rtsp_url[7] == '[';

        /* Dummy destination address for RTSP */
        dstlen = ipv6 ? sizeof( struct sockaddr_in6 )
                      : sizeof( struct sockaddr_in );
        memset (&dst, 0, dstlen);
        dst.ss_family = ipv6 ? AF_INET6 : AF_INET;
#ifdef HAVE_SA_LEN
        dst.ss_len = dstlen;
#endif
    }

    if( vlc_sdp_Start( &sdp, VLC_OBJECT( p_stream ), SOUT_CFG_PREFIX,
                       NULL, 0, (struct sockaddr *)&dst, dstlen ) )
        goto out;

    /* TODO: a=source-filter */
    if( p_sys->rtcp_mux )
        sdp_AddAttribute( &sdp, "rtcp-mux", NULL );

    if( rtsp_url != NULL )
        sdp_AddAttribute ( &sdp, "control", "%s", rtsp_url );

    const char *proto = "RTP/AVP"; /* protocol */
    if( rtsp_url == NULL )
    {
        switch( p_sys->proto )
        {
            case IPPROTO_UDP:
                break;
            case IPPROTO_TCP:
                proto = "TCP/RTP/AVP";
                break;
            case IPPROTO_DCCP:
                proto = "DCCP/RTP/AVP";
                break;
            case IPPROTO_UDPLITE:
                return psz_sdp;
        }
    }

    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_sys_t *id = p_sys->es[i];
        rtp_format_t *rtp_fmt = &id->rtp_fmt;
        const char *mime_major; /* major MIME type */

        switch( rtp_fmt->cat )
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

        sdp_AddMedia( &sdp, mime_major, proto, inclport * id->i_port,
                      rtp_fmt->payload_type, false, rtp_fmt->bitrate,
                      rtp_fmt->ptname, rtp_fmt->clock_rate, rtp_fmt->channels,
                      rtp_fmt->fmtp);

        /* cf RFC4566 §5.14 */
        if( inclport && !p_sys->rtcp_mux && (id->i_port & 1) )
            sdp_AddAttribute( &sdp, "rtcp", "%u", id->i_port + 1 );

        if( rtsp_url != NULL )
        {
            char *track_url = RtspAppendTrackPath( id->rtsp_id, rtsp_url );
            if( track_url != NULL )
            {
                sdp_AddAttribute( &sdp, "control", "%s", track_url );
                free( track_url );
            }
        }
        else
        {
            if( id->listen.fd != NULL )
                sdp_AddAttribute( &sdp, "setup", "passive" );
            if( p_sys->proto == IPPROTO_DCCP )
                sdp_AddAttribute( &sdp, "dccp-service-code", "SC:RTP%c",
                                  toupper( (unsigned char)mime_major[0] ) );
        }
    }

    if( vlc_memstream_close( &sdp ) == 0 )
        psz_sdp = sdp.ptr;
out:
    vlc_mutex_unlock( &p_sys->lock_es );
    return psz_sdp;
}

/*****************************************************************************
 * RTP mux
 *****************************************************************************/

/**
 * Shrink the MTU down to a fixed packetization time (for audio).
 */
static void
rtp_set_ptime (sout_stream_id_sys_t *id, unsigned ptime_ms, size_t bytes)
{
    /* Samples per second */
    size_t spl = (id->rtp_fmt.clock_rate - 1) * ptime_ms / 1000 + 1;
    bytes *= id->rtp_fmt.channels;
    spl *= bytes;

    if (spl < rtp_mtu (id)) /* MTU is big enough for ptime */
        id->i_mtu = 12 + spl;
    else /* MTU is too small for ptime, align to a sample boundary */
        id->i_mtu = 12 + (((id->i_mtu - 12) / bytes) * bytes);
}

uint32_t rtp_compute_ts( unsigned i_clock_rate, int64_t i_pts )
{
    /* This is an overflow-proof way of doing:
     * return i_pts * (int64_t)i_clock_rate / CLOCK_FREQ;
     *
     * NOTE: this plays nice with offsets because the (equivalent)
     * calculations are linear. */
    lldiv_t q = lldiv(i_pts, CLOCK_FREQ);
    return q.quot * (int64_t)i_clock_rate
          + q.rem * (int64_t)i_clock_rate / CLOCK_FREQ;
}

/** Add an ES as a new RTP stream */
static sout_stream_id_sys_t *Add( sout_stream_t *p_stream,
                                  const es_format_t *p_fmt )
{
    /* NOTE: As a special case, if we use a non-RTP
     * mux (TS/PS), then p_fmt is NULL. */
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char              *psz_sdp;

    sout_stream_id_sys_t *id = malloc( sizeof( *id ) );
    if( unlikely(id == NULL) )
        return NULL;
    id->p_stream   = p_stream;

    id->i_mtu = var_InheritInteger( p_stream, "mtu" );
    if( id->i_mtu <= 12 + 16 )
        id->i_mtu = 576 - 20 - 8; /* pessimistic */
    msg_Dbg( p_stream, "maximum RTP packet size: %d bytes", id->i_mtu );

#ifdef HAVE_SRTP
    id->srtp = NULL;
#endif
    vlc_mutex_init( &id->lock_sink );
    id->sinkc = 0;
    id->sinkv = NULL;
    id->rtsp_id = NULL;
    id->p_fifo = NULL;
    id->listen.fd = NULL;

    id->b_first_packet = true;
    id->i_caching =
        (int64_t)1000 * var_GetInteger( p_stream, SOUT_CFG_PREFIX "caching");

    vlc_rand_bytes (&id->i_sequence, sizeof (id->i_sequence));
    vlc_rand_bytes (id->ssrc, sizeof (id->ssrc));

    bool format = false;

    if (p_sys->p_vod_media != NULL)
    {
        id->rtp_fmt.ptname = NULL;
        uint32_t ssrc;
        int val = vod_init_id(p_sys->p_vod_media, p_sys->psz_vod_session,
                              p_fmt ? p_fmt->i_id : 0, id, &id->rtp_fmt,
                              &ssrc, &id->i_seq_sent_next);
        if (val == VLC_SUCCESS)
        {
            memcpy(id->ssrc, &ssrc, sizeof(id->ssrc));
            /* This is ugly, but id->i_seq_sent_next needs to be
             * initialized inside vod_init_id() to avoid race
             * conditions. */
            id->i_sequence = id->i_seq_sent_next;
        }
        /* vod_init_id() may fail either because the ES wasn't found in
         * the VoD media, or because the RTSP session is gone. In the
         * former case, id->rtp_fmt was left untouched. */
        format = (id->rtp_fmt.ptname != NULL);
    }

    if (!format)
    {
        id->rtp_fmt.fmtp = NULL; /* don't free() garbage on error */
        char *psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );
        if (p_fmt == NULL && psz == NULL)
            goto error;
        int val = rtp_get_fmt(VLC_OBJECT(p_stream), p_fmt, psz, &id->rtp_fmt);
        free( psz );
        if (val != VLC_SUCCESS)
            goto error;
    }

#ifdef HAVE_SRTP
    char *key = var_GetNonEmptyString (p_stream, SOUT_CFG_PREFIX"key");
    if (key)
    {
        vlc_gcrypt_init ();
        id->srtp = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 10,
                                   SRTP_PRF_AES_CM, SRTP_RCC_MODE1);
        if (id->srtp == NULL)
        {
            free (key);
            goto error;
        }

        char *salt = var_GetNonEmptyString (p_stream, SOUT_CFG_PREFIX"salt");
        int val = srtp_setkeystring (id->srtp, key, salt ? salt : "");
        free (salt);
        free (key);
        if (val)
        {
            msg_Err (p_stream, "bad SRTP key/salt combination (%s)",
                     vlc_strerror_c(val));
            goto error;
        }
        id->i_sequence = 0; /* FIXME: awful hack for libvlc_srtp */
    }
#endif

    id->i_seq_sent_next = id->i_sequence;

    int mcast_fd = -1;
    if( p_sys->psz_destination != NULL )
    {
        /* Choose the port */
        uint16_t i_port = 0;
        if( p_fmt == NULL )
            ;
        else
        if( p_fmt->i_cat == AUDIO_ES && p_sys->i_port_audio > 0 )
            i_port = p_sys->i_port_audio;
        else
        if( p_fmt->i_cat == VIDEO_ES && p_sys->i_port_video > 0 )
            i_port = p_sys->i_port_video;

        /* We do not need the ES lock (p_sys->lock_es) here, because
         * this is the only one thread that can *modify* the ES table.
         * The ES lock protects the other threads from our modifications
         * (TAB_APPEND, TAB_REMOVE). */
        for (int i = 0; i_port && (i < p_sys->i_es); i++)
             if (i_port == p_sys->es[i]->i_port)
                 i_port = 0; /* Port already in use! */
        for (uint16_t p = p_sys->i_port; i_port == 0; p += 2)
        {
            if (p == 0)
            {
                msg_Err (p_stream, "too many RTP elementary streams");
                goto error;
            }
            i_port = p;
            for (int i = 0; i_port && (i < p_sys->i_es); i++)
                 if (p == p_sys->es[i]->i_port)
                     i_port = 0;
        }

        id->i_port = i_port;

        int type = SOCK_STREAM;

        switch( p_sys->proto )
        {
#ifdef SOCK_DCCP
            case IPPROTO_DCCP:
            {
                const char *code;
                switch (id->rtp_fmt.cat)
                {
                    case VIDEO_ES: code = "RTPV";     break;
                    case AUDIO_ES: code = "RTPARTPV"; break;
                    case SPU_ES:   code = "RTPTRTPV"; break;
                    default:       code = "RTPORTPV"; break;
                }
                var_SetString (p_stream, "dccp-service", code);
                type = SOCK_DCCP;
            }
#endif
            /* fall through */
            case IPPROTO_TCP:
                id->listen.fd = net_Listen( VLC_OBJECT(p_stream),
                                            p_sys->psz_destination, i_port,
                                            type, p_sys->proto );
                if( id->listen.fd == NULL )
                {
                    msg_Err( p_stream, "passive COMEDIA RTP socket failed" );
                    goto error;
                }
                if( vlc_clone( &id->listen.thread, rtp_listen_thread, id,
                               VLC_THREAD_PRIORITY_LOW ) )
                {
                    net_ListenClose( id->listen.fd );
                    id->listen.fd = NULL;
                    goto error;
                }
                break;

            default:
            {
                int fd = net_ConnectDgram( p_stream, p_sys->psz_destination,
                                           i_port, -1, p_sys->proto );
                if( fd == -1 )
                {
                    msg_Err( p_stream, "cannot create RTP socket" );
                    goto error;
                }
                /* Ignore any unexpected incoming packet (including RTCP-RR
                 * packets in case of rtcp-mux) */
                setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &(int){ 0 },
                            sizeof (int));
                rtp_add_sink( id, fd, p_sys->rtcp_mux, NULL );
                /* FIXME: test if this is multicast  */
                mcast_fd = fd;
            }
        }
    }

    if( p_fmt != NULL )
    switch( p_fmt->i_codec )
    {
        case VLC_CODEC_MULAW:
        case VLC_CODEC_ALAW:
        case VLC_CODEC_U8:
            rtp_set_ptime (id, 20, 1);
            break;
        case VLC_CODEC_S16B:
        case VLC_CODEC_S16L:
            rtp_set_ptime (id, 20, 2);
            break;
        case VLC_CODEC_S24B:
            rtp_set_ptime (id, 20, 3);
            break;
        default:
            break;
    }

#if 0 /* No payload formats sets this at the moment */
    int cscov = -1;
    if( cscov != -1 )
        cscov += 8 /* UDP */ + 12 /* RTP */;
    if( id->sinkc > 0 )
        net_SetCSCov( id->sinkv[0].rtp_fd, cscov, -1 );
#endif

    vlc_mutex_lock( &p_sys->lock_ts );
    id->b_ts_init = ( p_sys->i_npt_zero != VLC_TS_INVALID );
    vlc_mutex_unlock( &p_sys->lock_ts );
    if( id->b_ts_init )
        id->i_ts_offset = rtp_compute_ts( id->rtp_fmt.clock_rate,
                                          p_sys->i_pts_offset );

    if( p_sys->rtsp != NULL )
        id->rtsp_id = RtspAddId( p_sys->rtsp, id, GetDWBE( id->ssrc ),
                                 id->rtp_fmt.clock_rate, mcast_fd );

    id->p_fifo = block_FifoNew();
    if( unlikely(id->p_fifo == NULL) )
        goto error;
    if( vlc_clone( &id->thread, ThreadSend, id, VLC_THREAD_PRIORITY_HIGHEST ) )
    {
        block_FifoRelease( id->p_fifo );
        id->p_fifo = NULL;
        goto error;
    }

    /* Update p_sys context */
    vlc_mutex_lock( &p_sys->lock_es );
    TAB_APPEND( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    psz_sdp = SDPGenerate( p_stream, NULL );

    vlc_mutex_lock( &p_sys->lock_sdp );
    free( p_sys->psz_sdp );
    p_sys->psz_sdp = psz_sdp;
    vlc_mutex_unlock( &p_sys->lock_sdp );

    msg_Dbg( p_stream, "sdp=\n%s", p_sys->psz_sdp );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap ) SapSetup( p_stream );
    if( p_sys->psz_sdp_file != NULL ) FileSetup( p_stream );

    return id;

error:
    Del( p_stream, id );
    return NULL;
}

static void Del( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_mutex_lock( &p_sys->lock_es );
    TAB_REMOVE( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    if( likely(id->p_fifo != NULL) )
    {
        vlc_cancel( id->thread );
        vlc_join( id->thread, NULL );
        block_FifoRelease( id->p_fifo );
    }

    free( id->rtp_fmt.fmtp );

    if (p_sys->p_vod_media != NULL)
        vod_detach_id(p_sys->p_vod_media, p_sys->psz_vod_session, id);
    if( id->rtsp_id )
        RtspDelId( p_sys->rtsp, id->rtsp_id );
    if( id->listen.fd != NULL )
    {
        vlc_cancel( id->listen.thread );
        vlc_join( id->listen.thread, NULL );
        net_ListenClose( id->listen.fd );
    }
    /* Delete remaining sinks (incoming connections or explicit
     * outgoing dst=) */
    while( id->sinkc > 0 )
        rtp_del_sink( id, id->sinkv[0].rtp_fd );
#ifdef HAVE_SRTP
    if( id->srtp != NULL )
        srtp_destroy( id->srtp );
#endif

    vlc_mutex_destroy( &id->lock_sink );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap ) SapSetup( p_stream );
    if( p_sys->psz_sdp_file != NULL ) FileSetup( p_stream );

    free( id );
}

static int Send( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                 block_t *p_buffer )
{
    assert( p_stream->p_sys->p_mux == NULL );
    (void)p_stream;

    while( p_buffer != NULL )
    {
        block_t *p_next = p_buffer->p_next;
        p_buffer->p_next = NULL;

        /* Send a Vorbis/Theora Packed Configuration packet (RFC 5215 §3.1)
         * as the first packet of the stream */
        if (id->b_first_packet)
        {
            id->b_first_packet = false;
            if (!strcmp(id->rtp_fmt.ptname, "vorbis") ||
                !strcmp(id->rtp_fmt.ptname, "theora"))
                rtp_packetize_xiph_config(id, id->rtp_fmt.fmtp,
                                          p_buffer->i_pts);
        }

        if( id->rtp_fmt.pf_packetize( id, p_buffer ) )
            break;

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

    /* Remove the previous session */
    if( p_sys->p_session != NULL)
    {
        sout_AnnounceUnRegister( p_stream, p_sys->p_session);
        p_sys->p_session = NULL;
    }

    if( p_sys->i_es > 0 && p_sys->psz_sdp && *p_sys->psz_sdp )
        p_sys->p_session = sout_AnnounceRegisterSDP( p_stream,
                                                     p_sys->psz_sdp,
                                                     p_sys->psz_destination );

    return VLC_SUCCESS;
}

/****************************************************************************
* File:
****************************************************************************/
static int FileSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    FILE            *f;

    if( p_sys->psz_sdp == NULL )
        return VLC_EGENERIC; /* too early */

    if( ( f = vlc_fopen( p_sys->psz_sdp_file, "wt" ) ) == NULL )
    {
        msg_Err( p_stream, "cannot open file '%s' (%s)",
                 p_sys->psz_sdp_file, vlc_strerror_c(errno) );
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

static int HttpSetup( sout_stream_t *p_stream, const vlc_url_t *url)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_sys->p_httpd_host = vlc_http_HostNew( VLC_OBJECT(p_stream) );
    if( p_sys->p_httpd_host )
    {
        p_sys->p_httpd_file = httpd_FileNew( p_sys->p_httpd_host,
                                             url->psz_path ? url->psz_path : "/",
                                             "application/sdp",
                                             NULL, NULL,
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
    VLC_UNUSED(f); VLC_UNUSED(p_request);
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
static void* ThreadSend( void *data )
{
#ifdef _WIN32
# define ENOBUFS      WSAENOBUFS
# define EAGAIN       WSAEWOULDBLOCK
# define EWOULDBLOCK  WSAEWOULDBLOCK
#endif
    sout_stream_id_sys_t *id = data;
    unsigned i_caching = id->i_caching;

    for (;;)
    {
        block_t *out = block_FifoGet( id->p_fifo );
        block_cleanup_push (out);

#ifdef HAVE_SRTP
        if( id->srtp )
        {   /* FIXME: this is awfully inefficient */
            size_t len = out->i_buffer;
            out = block_Realloc( out, 0, len + 10 );
            out->i_buffer = len;

            int canc = vlc_savecancel ();
            int val = srtp_send( id->srtp, out->p_buffer, &len, len + 10 );
            vlc_restorecancel (canc);
            if( val )
            {
                msg_Dbg( id->p_stream, "SRTP sending error: %s",
                         vlc_strerror_c(val) );
                block_Release( out );
                out = NULL;
            }
            else
                out->i_buffer = len;
        }
        if (out)
            mwait (out->i_dts + i_caching);
        vlc_cleanup_pop ();
        if (out == NULL)
            continue;
#else
        mwait (out->i_dts + i_caching);
        vlc_cleanup_pop ();
#endif

        ssize_t len = out->i_buffer;
        int canc = vlc_savecancel ();

        vlc_mutex_lock( &id->lock_sink );
        unsigned deadc = 0; /* How many dead sockets? */
        int deadv[id->sinkc ? id->sinkc : 1]; /* Dead sockets list */

        for( int i = 0; i < id->sinkc; i++ )
        {
#ifdef HAVE_SRTP
            if( !id->srtp ) /* FIXME: SRTCP support */
#endif
                SendRTCP( id->sinkv[i].rtcp, out );

            if( send( id->sinkv[i].rtp_fd, out->p_buffer, len, 0 ) == -1
             && net_errno != EAGAIN && net_errno != EWOULDBLOCK
             && net_errno != ENOBUFS && net_errno != ENOMEM )
            {
                int type;
                getsockopt( id->sinkv[i].rtp_fd, SOL_SOCKET, SO_TYPE,
                            &type, &(socklen_t){ sizeof(type) });
                if( type == SOCK_DGRAM )
                    /* ICMP soft error: ignore and retry */
                    send( id->sinkv[i].rtp_fd, out->p_buffer, len, 0 );
                else
                    /* Broken connection */
                    deadv[deadc++] = id->sinkv[i].rtp_fd;
            }
        }
        id->i_seq_sent_next = ntohs(((uint16_t *) out->p_buffer)[1]) + 1;
        vlc_mutex_unlock( &id->lock_sink );
        block_Release( out );

        for( unsigned i = 0; i < deadc; i++ )
        {
            msg_Dbg( id->p_stream, "removing socket %d", deadv[i] );
            rtp_del_sink( id, deadv[i] );
        }
        vlc_restorecancel (canc);
    }
    return NULL;
}


/* This thread dequeues incoming connections (DCCP streaming) */
static void *rtp_listen_thread( void *data )
{
    sout_stream_id_sys_t *id = data;

    assert( id->listen.fd != NULL );

    for( ;; )
    {
        int fd = net_Accept( id->p_stream, id->listen.fd );
        if( fd == -1 )
            continue;
        int canc = vlc_savecancel( );
        rtp_add_sink( id, fd, true, NULL );
        vlc_restorecancel( canc );
    }

    vlc_assert_unreachable();
}


int rtp_add_sink( sout_stream_id_sys_t *id, int fd, bool rtcp_mux, uint16_t *seq )
{
    rtp_sink_t sink = { fd, NULL };
    sink.rtcp = OpenRTCP( VLC_OBJECT( id->p_stream ), fd, IPPROTO_UDP,
                          rtcp_mux );
    if( sink.rtcp == NULL )
        msg_Err( id->p_stream, "RTCP failed!" );

    vlc_mutex_lock( &id->lock_sink );
    TAB_APPEND(id->sinkc, id->sinkv, sink);
    if( seq != NULL )
        *seq = id->i_seq_sent_next;
    vlc_mutex_unlock( &id->lock_sink );
    return VLC_SUCCESS;
}

void rtp_del_sink( sout_stream_id_sys_t *id, int fd )
{
    rtp_sink_t sink = { fd, NULL };

    /* NOTE: must be safe to use if fd is not included */
    vlc_mutex_lock( &id->lock_sink );
    for( int i = 0; i < id->sinkc; i++ )
    {
        if (id->sinkv[i].rtp_fd == fd)
        {
            sink = id->sinkv[i];
            TAB_ERASE(id->sinkc, id->sinkv, i);
            break;
        }
    }
    vlc_mutex_unlock( &id->lock_sink );

    CloseRTCP( sink.rtcp );
    net_Close( sink.rtp_fd );
}

uint16_t rtp_get_seq( sout_stream_id_sys_t *id )
{
    /* This will return values for the next packet. */
    uint16_t seq;

    vlc_mutex_lock( &id->lock_sink );
    seq = id->i_seq_sent_next;
    vlc_mutex_unlock( &id->lock_sink );

    return seq;
}

/* Return an arbitrary initial timestamp for RTP timestamp computations.
 * RFC 3550 states that the resulting initial RTP timestamps SHOULD be
 * random (although we use the same reference for all the ES as a
 * feature). In the VoD case, this function is called independently
 * from several parts of the code, so we need to always return the same
 * value. */
static int64_t rtp_init_ts( const vod_media_t *p_media,
                            const char *psz_vod_session )
{
    if (p_media == NULL || psz_vod_session == NULL)
        return mdate();

    uint64_t i_ts_init;
    /* As per RFC 2326, session identifiers are at least 8 bytes long */
    strncpy((char *)&i_ts_init, psz_vod_session, sizeof(uint64_t));
    i_ts_init ^= (uintptr_t)p_media;
    /* Limit the timestamp to 48 bits, this is enough and allows us
     * to stay away from overflows */
    i_ts_init &= 0xFFFFFFFFFFFF;
    return i_ts_init;
}

/* Return a timestamp corresponding to packets being sent now, and that
 * can be passed to rtp_compute_ts() to get rtptime values for each ES.
 * Also return the NPT corresponding to this timestamp. If the stream
 * output is not started, the initial timestamp that will be used with
 * the first packets for NPT=0 is returned instead. */
int64_t rtp_get_ts( const sout_stream_t *p_stream, const sout_stream_id_sys_t *id,
                    const vod_media_t *p_media, const char *psz_vod_session,
                    int64_t *p_npt )
{
    if (p_npt != NULL)
        *p_npt = 0;

    if (id != NULL)
        p_stream = id->p_stream;

    if (p_stream == NULL)
        return rtp_init_ts(p_media, psz_vod_session);

    sout_stream_sys_t *p_sys = p_stream->p_sys;
    mtime_t i_npt_zero;
    vlc_mutex_lock( &p_sys->lock_ts );
    i_npt_zero = p_sys->i_npt_zero;
    vlc_mutex_unlock( &p_sys->lock_ts );

    if( i_npt_zero == VLC_TS_INVALID )
        return p_sys->i_pts_zero;

    mtime_t now = mdate();
    if( now < i_npt_zero )
        return p_sys->i_pts_zero;

    int64_t npt = now - i_npt_zero;
    if (p_npt != NULL)
        *p_npt = npt;

    return p_sys->i_pts_zero + npt;
}

void rtp_packetize_common( sout_stream_id_sys_t *id, block_t *out,
                           bool b_m_bit, int64_t i_pts )
{
    if( !id->b_ts_init )
    {
        sout_stream_sys_t *p_sys = id->p_stream->p_sys;
        vlc_mutex_lock( &p_sys->lock_ts );
        if( p_sys->i_npt_zero == VLC_TS_INVALID )
        {
            /* This is the first packet of any ES. We initialize the
             * NPT=0 time reference, and the offset to match the
             * arbitrary PTS reference. */
            p_sys->i_npt_zero = i_pts + id->i_caching;
            p_sys->i_pts_offset = p_sys->i_pts_zero - i_pts;
        }
        vlc_mutex_unlock( &p_sys->lock_ts );

        /* And in any case this is the first packet of this ES, so we
         * initialize the offset for this ES. */
        id->i_ts_offset = rtp_compute_ts( id->rtp_fmt.clock_rate,
                                          p_sys->i_pts_offset );
        id->b_ts_init = true;
    }

    uint32_t i_timestamp = rtp_compute_ts( id->rtp_fmt.clock_rate, i_pts )
                           + id->i_ts_offset;

    out->p_buffer[0] = 0x80;
    out->p_buffer[1] = (b_m_bit?0x80:0x00)|id->rtp_fmt.payload_type;
    out->p_buffer[2] = ( id->i_sequence >> 8)&0xff;
    out->p_buffer[3] = ( id->i_sequence     )&0xff;
    out->p_buffer[4] = ( i_timestamp >> 24 )&0xff;
    out->p_buffer[5] = ( i_timestamp >> 16 )&0xff;
    out->p_buffer[6] = ( i_timestamp >>  8 )&0xff;
    out->p_buffer[7] = ( i_timestamp       )&0xff;

    memcpy( out->p_buffer + 8, id->ssrc, 4 );

    id->i_sequence++;
}

uint16_t rtp_get_extended_sequence( sout_stream_id_sys_t *id )
{
    return id->i_sequence >> 16;
}

void rtp_packetize_send( sout_stream_id_sys_t *id, block_t *out )
{
    block_FifoPut( id->p_fifo, out );
}

/**
 * @return configured max RTP payload size (including payload type-specific
 * headers, excluding RTP and transport headers)
 */
size_t rtp_mtu (const sout_stream_id_sys_t *id)
{
    return id->i_mtu - 12;
}

/*****************************************************************************
 * Non-RTP mux
 *****************************************************************************/

/** Add an ES to a non-RTP muxed stream */
static sout_stream_id_sys_t *MuxAdd( sout_stream_t *p_stream,
                                     const es_format_t *p_fmt )
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

    return (sout_stream_id_sys_t *)p_input;
}


static int MuxSend( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                    block_t *p_buffer )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    return sout_MuxSendBuffer( p_mux, (sout_input_t *)id, p_buffer );
}


/** Remove an ES from a non-RTP muxed stream */
static void MuxDel( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    sout_MuxDeleteStream( p_mux, (sout_input_t *)id );
}


static ssize_t AccessOutGrabberWriteBuffer( sout_stream_t *p_stream,
                                            const block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = p_sys->es[0];

    int64_t  i_dts = p_buffer->i_dts;

    uint8_t         *p_data = p_buffer->p_buffer;
    size_t          i_data  = p_buffer->i_buffer;
    size_t          i_max   = id->i_mtu - 12;
    bool            b_dis   = (p_buffer->i_flags & BLOCK_FLAG_DISCONTINUITY);

    size_t i_packet = ( p_buffer->i_buffer + i_max - 1 ) / i_max;

    while( i_data > 0 )
    {
        size_t i_size;

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
            p_sys->packet = block_Alloc( id->i_mtu );
            /* m-bit is discontinuity for MPEG1/2 PS and TS, RFC2250 2.1 */
            rtp_packetize_common( id, p_sys->packet, b_dis, i_dts );
            p_sys->packet->i_buffer = 12;
            p_sys->packet->i_dts = i_dts;
            p_sys->packet->i_length = p_buffer->i_length / i_packet;
            i_dts += p_sys->packet->i_length;
            b_dis = false;
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


static ssize_t AccessOutGrabberWrite( sout_access_out_t *p_access,
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

    p_grab = vlc_object_create( p_stream, sizeof( *p_grab ) );
    if( p_grab == NULL )
        return NULL;

    p_grab->p_module    = NULL;
    p_grab->psz_access  = strdup( "grab" );
    p_grab->p_cfg       = NULL;
    p_grab->psz_path    = strdup( "" );
    p_grab->p_sys       = (sout_access_out_sys_t *)p_stream;
    p_grab->pf_seek     = NULL;
    p_grab->pf_write    = AccessOutGrabberWrite;
    return p_grab;
}

void rtp_get_video_geometry( sout_stream_id_sys_t *id, int *width, int *height )
{
    int ret = sscanf( id->rtp_fmt.fmtp, "%*s width=%d; height=%d; ", width, height );
    assert( ret == 2 );
}
