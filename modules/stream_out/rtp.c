/*****************************************************************************
 * rtp.c: rtp stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
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
#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DST_TEXT N_("Destination")
#define DST_LONGTEXT N_( \
    "Allows you to specify the output URL used for the streaming output." )
#define SDP_TEXT N_("SDP")
#define SDP_LONGTEXT N_( \
    "Allows you to specify the SDP used for the streaming output. " \
    "You must use an url: http://location to access the SDP via HTTP, " \
    "rtsp://location for RTSP access, and sap:// for the SDP to be " \
    "announced via SAP." )
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_( \
    "Allows you to specify the muxer used for the streaming output." )

#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "Allows you to specify the session name used for the streaming output." )
#define DESC_TEXT N_("Session description")
#define DESC_LONGTEXT N_( \
    "Allows you to give a broader description of the stream." )
#define URL_TEXT N_("Session URL")
#define URL_LONGTEXT N_( \
    "Allows you to specify a URL with additional information on the stream." )
#define EMAIL_TEXT N_("Session email")
#define EMAIL_LONGTEXT N_( \
    "Allows you to specify contact e-mail address for this session." )

#define PORT_TEXT N_("Port")
#define PORT_LONGTEXT N_( \
    "Allows you to specify the base port used for the RTP streaming." )
#define PORT_AUDIO_TEXT N_("Audio port")
#define PORT_AUDIO_LONGTEXT N_( \
    "Allows you to specify the default audio port used for the RTP streaming." )
#define PORT_VIDEO_TEXT N_("Video port")
#define PORT_VIDEO_LONGTEXT N_( \
    "Allows you to specify the default video port used for the RTP streaming." )

#define TTL_TEXT N_("Time To Live")
#define TTL_LONGTEXT N_( \
    "Allows you to specify the time to live for the output stream." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-rtp-"

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

    add_string( SOUT_CFG_PREFIX "name", "NONE", NULL, NAME_TEXT,
                NAME_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "description", "", NULL, DESC_TEXT,
                DESC_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "url", "", NULL, URL_TEXT,
                URL_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "email", "", NULL, EMAIL_TEXT,
                EMAIL_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "port", 1234, NULL, PORT_TEXT,
                 PORT_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "port-audio", 1230, NULL, PORT_AUDIO_TEXT,
                 PORT_AUDIO_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "port-video", 1232, NULL, PORT_VIDEO_TEXT,
                 PORT_VIDEO_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "ttl", 0, NULL, TTL_TEXT,
                 TTL_LONGTEXT, VLC_TRUE );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "dst", "name", "port", "port-audio", "port-video", "*sdp", "ttl", "mux",
    "description", "url","email", NULL
};

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *,
                               block_t* );

/* For unicast/interleaved streaming */
typedef struct
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
} rtsp_client_t;

struct sout_stream_sys_t
{
    /* sdp */
    int64_t i_sdp_id;
    int     i_sdp_version;
    char    *psz_sdp;
    vlc_mutex_t  lock_sdp;

    char        *psz_session_name;
    char        *psz_session_description;
    char        *psz_session_url;
    char        *psz_session_email;

    /* */
    vlc_bool_t b_export_sdp_file;
    char *psz_sdp_file;
    /* sap */
    vlc_bool_t b_export_sap;
    session_descriptor_t *p_session;

    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    httpd_host_t *p_rtsp_host;
    httpd_url_t  *p_rtsp_url;
    char         *psz_rtsp_control;
    char         *psz_rtsp_path;

    /* */
    char *psz_destination;
    int  i_port;
    int  i_port_audio;
    int  i_port_video;
    int  i_ttl;

    /* when need to use a private one or when using muxer */
    int i_payload_type;

    /* in case we do TS/PS over rtp */
    sout_mux_t        *p_mux;
    sout_access_out_t *p_access;
    int               i_mtu;
    sout_access_out_t *p_grab;
    uint16_t          i_sequence;
    uint32_t          i_timestamp_start;
    uint8_t           ssrc[4];
    block_t           *packet;

    /* */
    vlc_mutex_t      lock_es;
    int              i_es;
    sout_stream_id_t **es;

    /* */
    int              i_rtsp;
    rtsp_client_t    **rtsp;
};

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *,
                                    block_t * );

struct sout_stream_id_t
{
    sout_stream_t *p_stream;
    /* rtp field */
    uint8_t     i_payload_type;
    uint16_t    i_sequence;
    uint32_t    i_timestamp_start;
    uint8_t     ssrc[4];

    /* for sdp */
    int         i_clock_rate;
    char        *psz_rtpmap;
    char        *psz_fmtp;
    char        *psz_destination;
    int         i_port;
    int         i_cat;

    /* Packetizer specific fields */
    pf_rtp_packetizer_t pf_packetize;
    int           i_mtu;

    /* for sending the packets */
    sout_access_out_t *p_access;

    vlc_mutex_t       lock_rtsp;
    int               i_rtsp_access;
    sout_access_out_t **rtsp_access;

    /* */
    sout_input_t      *p_input;

    /* RTSP url control */
    httpd_url_t  *p_rtsp_url;
};

static int AccessOutGrabberWrite( sout_access_out_t *, block_t * );

static void SDPHandleUrl( sout_stream_t *, char * );

static int SapSetup( sout_stream_t *p_stream );
static int FileSetup( sout_stream_t *p_stream );
static int HttpSetup( sout_stream_t *p_stream, vlc_url_t * );
static int RtspSetup( sout_stream_t *p_stream, vlc_url_t * );

static int  RtspCallback( httpd_callback_sys_t *, httpd_client_t *,
                          httpd_message_t *, httpd_message_t * );
static int  RtspCallbackId( httpd_callback_sys_t *, httpd_client_t *,
                            httpd_message_t *, httpd_message_t * );


static rtsp_client_t *RtspClientNew( sout_stream_t *, char *psz_session );
static rtsp_client_t *RtspClientGet( sout_stream_t *, char *psz_session );
static void           RtspClientDel( sout_stream_t *, rtsp_client_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    sout_stream_sys_t   *p_sys;
    vlc_value_t         val;

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );

    p_sys->psz_destination = var_GetString( p_stream, SOUT_CFG_PREFIX "dst" );
    if( *p_sys->psz_destination == '\0' )
    {
        free( p_sys->psz_destination );
        p_sys->psz_destination = NULL;
    }

    p_sys->psz_session_name = var_GetString( p_stream, SOUT_CFG_PREFIX "name" );
    p_sys->psz_session_description = var_GetString( p_stream, SOUT_CFG_PREFIX "description" );
    p_sys->psz_session_url = var_GetString( p_stream, SOUT_CFG_PREFIX "url" );
    p_sys->psz_session_email = var_GetString( p_stream, SOUT_CFG_PREFIX "email" );

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

    if( !p_sys->psz_destination || *p_sys->psz_destination == '\0' )
    {
        sout_cfg_t *p_cfg;
        vlc_bool_t b_ok = VLC_FALSE;

        for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
        {
            if( !strcmp( p_cfg->psz_name, "sdp" ) )
            {
                if( p_cfg->psz_value && !strncasecmp( p_cfg->psz_value, "rtsp", 4 ) )
                {
                    b_ok = VLC_TRUE;
                    break;
                }
            }
        }
        if( !b_ok )
        {
            vlc_value_t val2;
            var_Get( p_stream, SOUT_CFG_PREFIX "sdp", &val2 );
            if( !strncasecmp( val2.psz_string, "rtsp", 4 ) )
                b_ok = VLC_TRUE;
            free( val2.psz_string );
        }

        if( !b_ok )
        {
            msg_Err( p_stream, "missing destination and not in rtsp mode" );
            free( p_sys );
            return VLC_EGENERIC;
        }
        p_sys->psz_destination = NULL;
    }
    else if( p_sys->i_port <= 0 )
    {
        msg_Err( p_stream, "invalid port" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "ttl", &val );
    if( ( val.i_int > 255 ) || ( val.i_int < 0 ) )
    {
        msg_Err( p_stream, "illegal TTL %d", val.i_int );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->i_ttl = val.i_int;

    p_sys->i_payload_type = 96;
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->i_rtsp = 0;
    p_sys->rtsp   = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->i_sdp_id = mdate();
    p_sys->i_sdp_version = 1;
    p_sys->psz_sdp = NULL;

    p_sys->b_export_sap = VLC_FALSE;
    p_sys->b_export_sdp_file = VLC_FALSE;
    p_sys->p_session = NULL;

    p_sys->p_httpd_host = NULL;
    p_sys->p_httpd_file = NULL;
    p_sys->p_rtsp_host  = NULL;
    p_sys->p_rtsp_url   = NULL;
    p_sys->psz_rtsp_control = NULL;
    p_sys->psz_rtsp_path = NULL;

    vlc_mutex_init( p_stream, &p_sys->lock_sdp );
    vlc_mutex_init( p_stream, &p_sys->lock_es );

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    var_Get( p_stream, SOUT_CFG_PREFIX "mux", &val );
    if( *val.psz_string )
    {
        sout_access_out_t *p_grab;
        char *psz_rtpmap, url[NI_MAXHOST + 8], access[17], psz_ttl[5], ipv;

        if( !p_sys->psz_destination || *p_sys->psz_destination == '\0' )
        {
            msg_Err( p_stream, "rtp needs a destination when muxing" );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* Check muxer type */
        if( !strncasecmp( val.psz_string, "ps", 2 ) || !strncasecmp( val.psz_string, "mpeg1", 5 ) )
        {
            psz_rtpmap = "MP2P/90000";
        }
        else if( !strncasecmp( val.psz_string, "ts", 2 ) )
        {
            psz_rtpmap = "MP2T/90000";
            p_sys->i_payload_type = 33;
        }
        else
        {
            msg_Err( p_stream, "unsupported muxer type with rtp (only ts/ps)" );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* create the access out */
        if( p_sys->i_ttl > 0 )
        {
            sprintf( access, "udp{raw,ttl=%d}", p_sys->i_ttl );
        }
        else
        {
            sprintf( access, "udp{raw}" );
        }

        /* IPv6 needs brackets if not already present */
        snprintf( url, sizeof( url ),
                  ( ( p_sys->psz_destination[0] != '[' ) 
                 && ( strchr( p_sys->psz_destination, ':' ) != NULL ) )
                  ? "[%s]:%d" : "%s:%d", p_sys->psz_destination,
                  p_sys->i_port );
        url[sizeof( url ) - 1] = '\0';
        /* FIXME: we should check that url is a numerical address, otherwise
         * the SDP will be quite broken (regardless of the IP protocol version)
         */
        ipv = ( strchr( p_sys->psz_destination, ':' ) != NULL ) ? '6' : '4';

        if( !( p_sys->p_access = sout_AccessOutNew( p_sout, access, url ) ) )
        {
            msg_Err( p_stream, "cannot create the access out for %s://%s",
                     access, url );
            free( p_sys );
            return VLC_EGENERIC;
        }
        p_sys->i_mtu = config_GetInt( p_stream, "mtu" );  /* XXX beurk */
        if( p_sys->i_mtu <= 16 )
        {
            /* better than nothing */
            p_sys->i_mtu = 1500;
        }

        /* the access out grabber TODO export it as sout_AccessOutGrabberNew */
        p_grab = p_sys->p_grab =
            vlc_object_create( p_sout, sizeof( sout_access_out_t ) );
        p_grab->p_module    = NULL;
        p_grab->p_sout      = p_sout;
        p_grab->psz_access  = strdup( "grab" );
        p_grab->p_cfg       = NULL;
        p_grab->psz_name    = strdup( "" );
        p_grab->p_sys       = (sout_access_out_sys_t*)p_stream;
        p_grab->pf_seek     = NULL;
        p_grab->pf_write    = AccessOutGrabberWrite;

        /* the muxer */
        if( !( p_sys->p_mux = sout_MuxNew( p_sout, val.psz_string, p_sys->p_grab ) ) )
        {
            msg_Err( p_stream, "cannot create the muxer (%s)", val.psz_string );
            sout_AccessOutDelete( p_sys->p_grab );
            sout_AccessOutDelete( p_sys->p_access );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* create the SDP for a muxed stream (only once) */
        /* FIXME  http://www.faqs.org/rfcs/rfc2327.html
           All text fields should be UTF-8 encoded. Use global a:charset to announce this.
           o= - should be local username (no spaces allowed)
           o= time should be hashed with some other value to garantue uniqueness
           o= we need IP6 support?
           o= don't use the localhost address. use fully qualified domain name or IP4 address
           p= international phone number (pass via vars?)
           c= IP6 support
           a= recvonly (missing)
           a= type:broadcast (missing)
           a= charset: (normally charset should be UTF-8, this can be used to override s= and i=)
           a= x-plgroup: (missing)
           RTP packets need to get the correct src IP address  */
        if( net_AddressIsMulticast( (vlc_object_t *)p_stream, p_sys->psz_destination ) )
        {
            snprintf( psz_ttl, sizeof( psz_ttl ), "/%d", p_sys->i_ttl ? 
                p_sys->i_ttl : config_GetInt( p_sout, "ttl" ) );
            psz_ttl[sizeof( psz_ttl ) - 1] = '\0';
        }
        else
        {
            psz_ttl[0] = '\0'; 
        }

        asprintf( &p_sys->psz_sdp,
                  "v=0\r\n"
                  /* FIXME: source address not known :( */
                  "o=- "I64Fd" %d IN IP%c %s\r\n"
                  "s=%s\r\n"
                  "i=%s\r\n"
                  "u=%s\r\n"
                  "e=%s\r\n"
                  "t=0 0\r\n" /* permanent stream */ /* when scheduled from vlm, we should set this info correctly */
                  "a=tool:"PACKAGE_STRING"\r\n"
                  "c=IN IP%c %s%s\r\n"
                  "m=video %d RTP/AVP %d\r\n"
                  "a=rtpmap:%d %s\r\n",
                  p_sys->i_sdp_id, p_sys->i_sdp_version,
                  ipv, ipv == '6' ? "::1" : "127.0.0.1" /* FIXME */,
                  p_sys->psz_session_name,
                  p_sys->psz_session_description,
                  p_sys->psz_session_url,
                  p_sys->psz_session_email,
                  ipv, p_sys->psz_destination, psz_ttl,
                  p_sys->i_port, p_sys->i_payload_type,
                  p_sys->i_payload_type, psz_rtpmap );
        fprintf( stderr, "sdp=%s", p_sys->psz_sdp );

        /* create the rtp context */
        p_sys->ssrc[0] = rand()&0xff;
        p_sys->ssrc[1] = rand()&0xff;
        p_sys->ssrc[2] = rand()&0xff;
        p_sys->ssrc[3] = rand()&0xff;
        p_sys->i_sequence = rand()&0xffff;
        p_sys->i_timestamp_start = rand()&0xffffffff;
        p_sys->packet = NULL;
    }
    else
    {
        p_sys->p_mux    = NULL;
        p_sys->p_access = NULL;
        p_sys->p_grab   = NULL;
    }
    free( val.psz_string );


    var_Get( p_stream, SOUT_CFG_PREFIX "sdp", &val );
    if( *val.psz_string )
    {
        sout_cfg_t *p_cfg;

        SDPHandleUrl( p_stream, val.psz_string );

        for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
        {
            if( !strcmp( p_cfg->psz_name, "sdp" ) )
            {
                if( p_cfg->psz_value == NULL || *p_cfg->psz_value == '\0' )
                    continue;

                if( !strcmp( p_cfg->psz_value, val.psz_string ) )   /* needed both :sout-rtp-sdp= and rtp{sdp=} can be used */
                    continue;

                SDPHandleUrl( p_stream, p_cfg->psz_value );
            }
        }
    }
    free( val.psz_string );

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
        sout_MuxDelete( p_sys->p_mux );
        sout_AccessOutDelete( p_sys->p_access );
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

    while( p_sys->i_rtsp > 0 )
    {
        RtspClientDel( p_stream, p_sys->rtsp[0] );
    }

    vlc_mutex_destroy( &p_sys->lock_sdp );

    if( p_sys->p_httpd_file )
    {
        httpd_FileDelete( p_sys->p_httpd_file );
    }
    if( p_sys->p_httpd_host )
    {
        httpd_HostDelete( p_sys->p_httpd_host );
    }
    if( p_sys->p_rtsp_url )
    {
        httpd_UrlDelete( p_sys->p_rtsp_url );
    }
    if( p_sys->p_rtsp_host )
    {
        httpd_HostDelete( p_sys->p_rtsp_host );
    }
#if 0
    /* why? is this disabled? */
    if( p_sys->psz_session_name )
    {
        free( p_sys->psz_session_name );
        p_sys->psz_session_name = NULL;
    }
    if( p_sys->psz_session_description )
    {
        free( p_sys->psz_session_description );
        p_sys->psz_session_description = NULL;
    }
    if( p_sys->psz_session_url )
    {
        free( p_sys->psz_session_url );
        p_sys->psz_session_url = NULL;
    }
    if( p_sys->psz_session_email )
    {
        free( p_sys->psz_session_email );
        p_sys->psz_session_email = NULL;
    }
#endif
    if( p_sys->psz_sdp )
    {
        free( p_sys->psz_sdp );
    }
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
            msg_Err( p_stream, "You can used sdp=http:// only once" );
            return;
        }

        if( HttpSetup( p_stream, &url ) )
        {
            msg_Err( p_stream, "cannot export sdp as http" );
        }
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "rtsp" ) )
    {
        if( p_sys->p_rtsp_url )
        {
            msg_Err( p_stream, "You can used sdp=rtsp:// only once" );
            return;
        }

        /* FIXME test if destination is multicast or no destination at all FIXME */
        if( RtspSetup( p_stream, &url ) )
        {
            msg_Err( p_stream, "cannot export sdp as rtsp" );
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
            msg_Err( p_stream, "You can used sdp=file:// only once" );
            return;
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
    vlc_UrlClean( &url );
}

/*****************************************************************************
 * SDPGenerate
 *****************************************************************************/
        /* FIXME  http://www.faqs.org/rfcs/rfc2327.html
           All text fields should be UTF-8 encoded. Use global a:charset to announce this.
           o= - should be local username (no spaces allowed)
           o= time should be hashed with some other value to garantue uniqueness
           o= we need IP6 support?
           o= don't use the localhost address. use fully qualified domain name or IP4 address
           p= international phone number (pass via vars?)
           c= IP6 support
           a= recvonly (missing)
           a= type:broadcast (missing)
           a= charset: (normally charset should be UTF-8, this can be used to override s= and i=)
           a= x-plgroup: (missing)
           RTP packets need to get the correct src IP address  */
static char *SDPGenerate( const sout_stream_t *p_stream,
                          const char *psz_destination, vlc_bool_t b_rtsp )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t  *p_sout = p_stream->p_sout;
    int i_size;
    char *psz_sdp, *p, ipv;
    int i;

    /* FIXME: breaks IP version check on unknown destination */
    if( psz_destination == NULL )
        psz_destination = "0.0.0.0";

    i_size = sizeof( "v=0\r\n" ) +
             sizeof( "o=- * * IN IP4 127.0.0.1\r\n" ) + 10 + 10 +
             sizeof( "s=*\r\n" ) + strlen( p_sys->psz_session_name ) +
             sizeof( "i=*\r\n" ) + strlen( p_sys->psz_session_description ) +
             sizeof( "u=*\r\n" ) + strlen( p_sys->psz_session_url ) +
             sizeof( "e=*\r\n" ) + strlen( p_sys->psz_session_email ) +
             sizeof( "t=0 0\r\n" ) + /* permanent stream */ /* when scheduled from vlm, we should set this info correctly */
             sizeof( "a=tool:"PACKAGE_STRING"\r\n" ) +
             sizeof( "c=IN IP4 */*\r\n" ) + 20 + 10 +
             strlen( psz_destination ) ;
    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];

        i_size += strlen( "m=**d*o * RTP/AVP *\r\n" ) + 10 + 10;
        if( id->psz_rtpmap )
        {
            i_size += strlen( "a=rtpmap:* *\r\n" ) + strlen( id->psz_rtpmap )+10;
        }
        if( id->psz_fmtp )
        {
            i_size += strlen( "a=fmtp:* *\r\n" ) + strlen( id->psz_fmtp ) + 10;
        }
        if( b_rtsp )
        {
            i_size += strlen( "a=control:*/trackid=*\r\n" ) + strlen( p_sys->psz_rtsp_control ) + 10;
        }
    }

    ipv = ( strchr( psz_destination, ':' ) != NULL ) ? '6' : '4';

    p = psz_sdp = malloc( i_size );
    p += sprintf( p, "v=0\r\n" );
    p += sprintf( p, "o=- "I64Fd" %d IN IP%c %s\r\n",
                  p_sys->i_sdp_id, p_sys->i_sdp_version,
                  ipv, ipv == '6' ? "::" : "127.0.0.1" );
    if( *p_sys->psz_session_name )
        p += sprintf( p, "s=%s\r\n", p_sys->psz_session_name );
    if( *p_sys->psz_session_description )
        p += sprintf( p, "i=%s\r\n", p_sys->psz_session_description );
    if( *p_sys->psz_session_url )
        p += sprintf( p, "u=%s\r\n", p_sys->psz_session_url );
    if( *p_sys->psz_session_email )
        p += sprintf( p, "e=%s\r\n", p_sys->psz_session_email );

    p += sprintf( p, "t=0 0\r\n" ); /* permanent stream */ /* when scheduled from vlm, we should set this info correctly */
    p += sprintf( p, "a=tool:"PACKAGE_STRING"\r\n" );

    p += sprintf( p, "c=IN IP%c %s", ipv, psz_destination );

    if( net_AddressIsMulticast( (vlc_object_t *)p_stream, psz_destination ) )
    {
        /* Add the ttl if it is a multicast address */
        p += sprintf( p, "/%d\r\n", p_sys->i_ttl ? p_sys->i_ttl :
                config_GetInt( p_sout, "ttl" ) );
    }
    else
    {
        p += sprintf( p, "\r\n" );
    }

    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];

        if( id->i_cat == AUDIO_ES )
        {
            p += sprintf( p, "m=audio %d RTP/AVP %d\r\n",
                          id->i_port, id->i_payload_type );
        }
        else if( id->i_cat == VIDEO_ES )
        {
            p += sprintf( p, "m=video %d RTP/AVP %d\r\n",
                          id->i_port, id->i_payload_type );
        }
        else
        {
            continue;
        }
        if( id->psz_rtpmap )
        {
            p += sprintf( p, "a=rtpmap:%d %s\r\n", id->i_payload_type,
                          id->psz_rtpmap );
        }
        if( id->psz_fmtp )
        {
            p += sprintf( p, "a=fmtp:%d %s\r\n", id->i_payload_type,
                          id->psz_fmtp );
        }
        if( b_rtsp )
        {
            p += sprintf( p, "a=control:%s/trackid=%d\r\n", p_sys->psz_rtsp_control, i );
        }
    }

    return psz_sdp;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int rtp_packetize_l16  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_l8   ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mpa  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mpv  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_ac3  ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_split( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_mp4a ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_h263 ( sout_stream_t *, sout_stream_id_t *, block_t * );
static int rtp_packetize_amr  ( sout_stream_t *, sout_stream_id_t *, block_t * );

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

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_instance_t   *p_sout = p_stream->p_sout;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;
    sout_access_out_t *p_access = NULL;
    int               i_port;
    char              *psz_sdp;

    if( p_sys->p_mux != NULL )
    {
        sout_input_t      *p_input  = NULL;
        if( ( p_input = sout_MuxAddStream( p_sys->p_mux, p_fmt ) ) == NULL )
        {
            msg_Err( p_stream, "cannot add this stream to the muxer" );
            return NULL;
        }

        id = malloc( sizeof( sout_stream_id_t ) );
        memset( id, 0, sizeof( sout_stream_id_t ) );
        id->p_access    = NULL;
        id->p_input     = p_input;
        id->pf_packetize= NULL;
        id->p_rtsp_url  = NULL;
        id->i_port      = 0;
        return id;
    }


    /* Choose the port */
    i_port = 0;
    if( p_fmt->i_cat == AUDIO_ES && p_sys->i_port_audio > 0 )
    {
        i_port = p_sys->i_port_audio;
        p_sys->i_port_audio = 0;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->i_port_video > 0 )
    {
        i_port = p_sys->i_port_video;
        p_sys->i_port_video = 0;
    }
    while( i_port == 0 )
    {
        if( p_sys->i_port != p_sys->i_port_audio && p_sys->i_port != p_sys->i_port_video )
        {
            i_port = p_sys->i_port;
            p_sys->i_port += 2;
            break;
        }
        p_sys->i_port += 2;
    }

    if( p_sys->psz_destination )
    {
        char access[17];
        char url[NI_MAXHOST + 8];

        /* first try to create the access out */
        if( p_sys->i_ttl > 0 )
        {
            snprintf( access, sizeof( access ), "udp{raw,ttl=%d}",
                      p_sys->i_ttl );
            access[sizeof( access ) - 1] = '\0';
        }
        else
            strcpy( access, "udp{raw}" );

        snprintf( url, sizeof( url ), (( p_sys->psz_destination[0] != '[' ) &&
                 strchr( p_sys->psz_destination, ':' )) ? "[%s]:%d" : "%s:%d",
                 p_sys->psz_destination, i_port );
        url[sizeof( url ) - 1] = '\0';

        if( ( p_access = sout_AccessOutNew( p_sout, access, url ) ) == NULL )
        {
            msg_Err( p_stream, "cannot create the access out for %s://%s",
                     access, url );
            return NULL;
        }
        msg_Dbg( p_stream, "access out %s:%s", access, url );
    }

    /* not create the rtp specific stuff */
    id = malloc( sizeof( sout_stream_id_t ) );
    memset( id, 0, sizeof( sout_stream_id_t ) );
    id->p_stream   = p_stream;
    id->p_access   = p_access;
    id->p_input    = NULL;
    id->psz_rtpmap = NULL;
    id->psz_fmtp   = NULL;
    id->psz_destination = p_sys->psz_destination ? strdup( p_sys->psz_destination ) : NULL;
    id->i_port = i_port;
    id->p_rtsp_url = NULL;
    vlc_mutex_init( p_stream, &id->lock_rtsp );
    id->i_rtsp_access = 0;
    id->rtsp_access = NULL;

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
            else
            {
                id->i_payload_type = p_sys->i_payload_type++;
            }
            id->psz_rtpmap = malloc( strlen( "L16/*/*" ) + 20+1 );
            sprintf( id->psz_rtpmap, "L16/%d/%d", p_fmt->audio.i_rate,
                     p_fmt->audio.i_channels );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_l16;
            break;
        case VLC_FOURCC( 'u', '8', ' ', ' ' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->psz_rtpmap = malloc( strlen( "L8/*/*" ) + 20+1 );
            sprintf( id->psz_rtpmap, "L8/%d/%d", p_fmt->audio.i_rate,
                     p_fmt->audio.i_channels );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_l8;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            id->i_payload_type = 14;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "MPA/90000" );
            id->pf_packetize = rtp_packetize_mpa;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            id->i_payload_type = 32;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "MPV/90000" );
            id->pf_packetize = rtp_packetize_mpv;
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "ac3/90000" );
            id->pf_packetize = rtp_packetize_ac3;
            break;
        case VLC_FOURCC( 'H', '2', '6', '3' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "H263-1998/90000" );
            id->pf_packetize = rtp_packetize_h263;
            break;

        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        {
            char hexa[2*p_fmt->i_extra +1];

            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "MP4V-ES/90000" );
            id->pf_packetize = rtp_packetize_split;
            if( p_fmt->i_extra > 0 )
            {
                id->psz_fmtp = malloc( 100 + 2 * p_fmt->i_extra );
                sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
                sprintf( id->psz_fmtp,
                         "profile-level-id=3; config=%s;", hexa );
            }
            break;
        }
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        {
            char hexa[2*p_fmt->i_extra +1];

            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->psz_rtpmap = malloc( strlen( "mpeg4-generic/" ) + 12 );
            sprintf( id->psz_rtpmap, "mpeg4-generic/%d", p_fmt->audio.i_rate );
            id->pf_packetize = rtp_packetize_mp4a;
            id->psz_fmtp = malloc( 200 + 2 * p_fmt->i_extra );
            sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
            sprintf( id->psz_fmtp,
                     "streamtype=5; profile-level-id=15; mode=AAC-hbr; "
                     "config=%s; SizeLength=13;IndexLength=3; "
                     "IndexDeltaLength=3; Profile=1;", hexa );
            break;
        }
        case VLC_FOURCC( 's', 'a', 'm', 'r' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                     "AMR/8000/2" : "AMR/8000" );
            id->psz_fmtp = strdup( "octet-align=1" );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_amr;
            break; 
        case VLC_FOURCC( 's', 'a', 'w', 'b' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->psz_rtpmap = strdup( p_fmt->audio.i_channels == 2 ?
                                     "AMR-WB/16000/2" : "AMR-WB/16000" );
            id->psz_fmtp = strdup( "octet-align=1" );
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->pf_packetize = rtp_packetize_amr;
            break; 

        default:
            msg_Err( p_stream, "cannot add this stream (unsupported "
                     "codec:%4.4s)", (char*)&p_fmt->i_codec );
            if( p_access )
            {
                sout_AccessOutDelete( p_access );
            }
            free( id );
            return NULL;
    }
    id->i_cat = p_fmt->i_cat;

    id->ssrc[0] = rand()&0xff;
    id->ssrc[1] = rand()&0xff;
    id->ssrc[2] = rand()&0xff;
    id->ssrc[3] = rand()&0xff;
    id->i_sequence = rand()&0xffff;
    id->i_timestamp_start = rand()&0xffffffff;

    id->i_mtu    = config_GetInt( p_stream, "mtu" );  /* XXX beuk */
    if( id->i_mtu <= 16 )
    {
        /* better than nothing */
        id->i_mtu = 1500;
    }
    msg_Dbg( p_stream, "using mtu=%d", id->i_mtu );

    if( p_sys->p_rtsp_url )
    {
        char psz_urlc[strlen( p_sys->psz_rtsp_control ) + 1 + 10];

        sprintf( psz_urlc, "%s/trackid=%d", p_sys->psz_rtsp_path, p_sys->i_es );
        fprintf( stderr, "rtsp: adding %s\n", psz_urlc );
        id->p_rtsp_url = httpd_UrlNewUnique( p_sys->p_rtsp_host, psz_urlc, NULL, NULL, NULL );

        if( id->p_rtsp_url )
        {
            httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_SETUP,    RtspCallbackId, (void*)id );
            //httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_PLAY,     RtspCallback, (void*)p_stream );
            //httpd_UrlCatch( id->p_rtsp_url, HTTPD_MSG_PAUSE,    RtspCallback, (void*)p_stream );
        }
    }


    /* Update p_sys context */
    vlc_mutex_lock( &p_sys->lock_es );
    TAB_APPEND( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    psz_sdp = SDPGenerate( p_stream, p_sys->psz_destination, VLC_FALSE );

    vlc_mutex_lock( &p_sys->lock_sdp );
    free( p_sys->psz_sdp );
    p_sys->psz_sdp = psz_sdp;
    vlc_mutex_unlock( &p_sys->lock_sdp );

    p_sys->i_sdp_version++;

    fprintf( stderr, "sdp=%s", p_sys->psz_sdp );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

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

    if( id->p_access )
    {
        if( id->psz_rtpmap )
        {
            free( id->psz_rtpmap );
        }
        if( id->psz_fmtp )
        {
            free( id->psz_fmtp );
        }
        if( id->psz_destination )
            free( id->psz_destination );
        sout_AccessOutDelete( id->p_access );
    }
    else if( id->p_input )
    {
        sout_MuxDeleteStream( p_sys->p_mux, id->p_input );
    }
    if( id->p_rtsp_url )
    {
        httpd_UrlDelete( id->p_rtsp_url );
    }
    vlc_mutex_destroy( &id->lock_rtsp );
    if( id->rtsp_access ) free( id->rtsp_access );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap && !p_sys->p_mux ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    block_t *p_next;

    if( p_stream->p_sys->p_mux )
    {
        sout_MuxSendBuffer( p_stream->p_sys->p_mux, id->p_input, p_buffer );
    }
    else
    {
        while( p_buffer )
        {
            p_next = p_buffer->p_next;
            if( id->pf_packetize( p_stream, id, p_buffer ) )
            {
                break;
            }
            block_Release( p_buffer );
            p_buffer = p_next;
        }
    }
    return VLC_SUCCESS;
}


static int AccessOutGrabberWriteBuffer( sout_stream_t *p_stream,
                                        block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    int64_t  i_dts = p_buffer->i_dts;
    uint32_t i_timestamp = i_dts * 9 / 100;

    uint8_t         *p_data = p_buffer->p_buffer;
    unsigned int    i_data  = p_buffer->i_buffer;
    unsigned int    i_max   = p_sys->i_mtu - 12;

    unsigned i_packet = ( p_buffer->i_buffer + i_max - 1 ) / i_max;

    while( i_data > 0 )
    {
        unsigned int i_size;

        /* output complete packet */
        if( p_sys->packet &&
            p_sys->packet->i_buffer + i_data > i_max )
        {
            sout_AccessOutWrite( p_sys->p_access, p_sys->packet );
            p_sys->packet = NULL;
        }

        if( p_sys->packet == NULL )
        {
            /* allocate a new packet */
            p_sys->packet = block_New( p_stream, p_sys->i_mtu );
            p_sys->packet->p_buffer[ 0] = 0x80;
            p_sys->packet->p_buffer[ 1] = 0x80|p_sys->i_payload_type;
            p_sys->packet->p_buffer[ 2] = ( p_sys->i_sequence >> 8)&0xff;
            p_sys->packet->p_buffer[ 3] = ( p_sys->i_sequence     )&0xff;
            p_sys->packet->p_buffer[ 4] = ( i_timestamp >> 24 )&0xff;
            p_sys->packet->p_buffer[ 5] = ( i_timestamp >> 16 )&0xff;
            p_sys->packet->p_buffer[ 6] = ( i_timestamp >>  8 )&0xff;
            p_sys->packet->p_buffer[ 7] = ( i_timestamp       )&0xff;
            p_sys->packet->p_buffer[ 8] = p_sys->ssrc[0];
            p_sys->packet->p_buffer[ 9] = p_sys->ssrc[1];
            p_sys->packet->p_buffer[10] = p_sys->ssrc[2];
            p_sys->packet->p_buffer[11] = p_sys->ssrc[3];
            p_sys->packet->i_buffer = 12;

            p_sys->packet->i_dts = i_dts;
            p_sys->packet->i_length = p_buffer->i_length / i_packet;
            i_dts += p_sys->packet->i_length;

            p_sys->i_sequence++;
        }

        i_size = __MIN( i_data,
                        (unsigned)(p_sys->i_mtu - p_sys->packet->i_buffer) );

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

    //fprintf( stderr, "received buffer size=%d\n", p_buffer->i_buffer );
    //
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

/****************************************************************************
 * SAP:
 ****************************************************************************/
static int SapSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t   *p_sout = p_stream->p_sout;
    announce_method_t *p_method = sout_AnnounceMethodCreate( METHOD_TYPE_SAP );

    /* Remove the previous session */
    if( p_sys->p_session != NULL)
    {
        sout_AnnounceUnRegister( p_sout, p_sys->p_session);
        sout_AnnounceSessionDestroy( p_sys->p_session );
        p_sys->p_session = NULL;
    }

    if( ( p_sys->i_es > 0 || p_sys->p_mux ) && p_sys->psz_sdp && *p_sys->psz_sdp )
    {
        p_sys->p_session = sout_AnnounceRegisterSDP( p_sout, p_sys->psz_sdp,
                                                     p_sys->psz_destination,
                                                     p_method );
    }

    free( p_method );
    return VLC_SUCCESS;
}

/****************************************************************************
* File:
****************************************************************************/
static int FileSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    FILE            *f;

    if( ( f = fopen( p_sys->psz_sdp_file, "wt" ) ) == NULL )
    {
        msg_Err( p_stream, "cannot open file '%s' (%s)",
                 p_sys->psz_sdp_file, strerror(errno) );
        return VLC_EGENERIC;
    }

    fprintf( f, "%s", p_sys->psz_sdp );
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

    p_sys->p_httpd_host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host, url->i_port > 0 ? url->i_port : 80 );
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
 * RTSP:
 ****************************************************************************/
static rtsp_client_t *RtspClientNew( sout_stream_t *p_stream, char *psz_session )
{
    rtsp_client_t *rtsp = malloc( sizeof( rtsp_client_t ));

    rtsp->psz_session = psz_session;
    rtsp->i_last = 0;
    rtsp->b_playing = VLC_FALSE;
    rtsp->i_id = 0;
    rtsp->id = NULL;
    rtsp->i_access = 0;
    rtsp->access = NULL;

    TAB_APPEND( p_stream->p_sys->i_rtsp, p_stream->p_sys->rtsp, rtsp );

    return rtsp;
}
static rtsp_client_t *RtspClientGet( sout_stream_t *p_stream, char *psz_session )
{
    int i;
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

static int RtspSetup( sout_stream_t *p_stream, vlc_url_t *url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    fprintf( stderr, "rtsp setup: %s : %d / %s\n", url->psz_host, url->i_port, url->psz_path );

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
    if( p_sys->p_rtsp_url == 0 )
    {
        return VLC_EGENERIC;
    }
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_DESCRIBE, RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_PLAY,     RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_PAUSE,    RtspCallback, (void*)p_stream );
    httpd_UrlCatch( p_sys->p_rtsp_url, HTTPD_MSG_TEARDOWN, RtspCallback, (void*)p_stream );

    return VLC_SUCCESS;
}

static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl,
                          httpd_message_t *answer, httpd_message_t *query )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_args;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char          *psz_destination = p_sys->psz_destination;
    char          *psz_session = NULL;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;

    switch( query->i_type )
    {
        case HTTPD_MSG_DESCRIBE:
        {
            char *psz_sdp = SDPGenerate( p_stream, psz_destination ? psz_destination : "0.0.0.0", VLC_TRUE );

            answer->i_status = 200;
            answer->psz_status = strdup( "OK" );
            httpd_MsgAdd( answer, "Content-type",  "%s", "application/sdp" );

            answer->p_body = (uint8_t *)psz_sdp;
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

                    vlc_mutex_lock( &id->lock_rtsp );
                    TAB_APPEND( id->i_rtsp_access, id->rtsp_access, rtsp->access[i_id] );
                    vlc_mutex_unlock( &id->lock_rtsp );
                }
                vlc_mutex_unlock( &p_sys->lock_es );
            }
            break;
        }
        case HTTPD_MSG_PAUSE:
            /* FIXME */
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

                    vlc_mutex_lock( &id->lock_rtsp );
                    TAB_REMOVE( id->i_rtsp_access, id->rtsp_access, rtsp->access[i_id] );
                    vlc_mutex_unlock( &id->lock_rtsp );
                }
                vlc_mutex_unlock( &p_sys->lock_es );

                RtspClientDel( p_stream, rtsp );
            }
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    httpd_MsgAdd( answer, "Server", "VLC Server" );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cseq", "%d", atoi( httpd_MsgGet( query, "Cseq" ) ) );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
    {
        httpd_MsgAdd( answer, "Session", "%s;timeout=5", psz_session );
    }
    return VLC_SUCCESS;
}

static int  RtspCallbackId( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl,
                          httpd_message_t *answer, httpd_message_t *query )
{
    sout_stream_id_t *id = (sout_stream_id_t*)p_args;
    sout_stream_t    *p_stream = id->p_stream;
    sout_instance_t    *p_sout = p_stream->p_sout;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char          *psz_session = NULL;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;

    switch( query->i_type )
    {
        case HTTPD_MSG_SETUP:
        {
            char *psz_transport = httpd_MsgGet( query, "Transport" );

            fprintf( stderr, "HTTPD_MSG_SETUP: transport=%s\n", psz_transport );

            if( strstr( psz_transport, "multicast" ) && id->psz_destination )
            {
                fprintf( stderr, "HTTPD_MSG_SETUP: multicast\n" );
                answer->i_status = 200;
                answer->psz_status = strdup( "OK" );
                answer->i_body = 0;
                answer->p_body = NULL;
                psz_session = httpd_MsgGet( query, "Session" );
                if( *psz_session == 0 )
                {
                    psz_session = malloc( 100 );
                    sprintf( psz_session, "%d", rand() );
                }
                httpd_MsgAdd( answer, "Transport",
                              "RTP/AVP/UDP;destination=%s;port=%d-%d;ttl=%d",
                              id->psz_destination, id->i_port,id->i_port+1,
                              p_sys->i_ttl ? p_sys->i_ttl : 
                              config_GetInt( p_sout, "ttl" ) );
            }
            else if( strstr( psz_transport, "unicast" ) && strstr( psz_transport, "client_port=" ) )
            {
                int  i_port = atoi( strstr( psz_transport, "client_port=" ) + strlen("client_port=") );
                char ip[NI_MAXNUMERICHOST], psz_access[17], psz_url[NI_MAXNUMERICHOST + 8];

                sout_access_out_t *p_access;

                rtsp_client_t *rtsp = NULL;

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
                if( *psz_session == 0 )
                {
                    psz_session = malloc( 100 );
                    sprintf( psz_session, "%d", rand() );

                    rtsp = RtspClientNew( p_stream, psz_session );
                }
                else
                {
                    rtsp = RtspClientGet( p_stream, psz_session );
                    if( rtsp == NULL )
                    {
                        answer->i_status = 454;
                        answer->psz_status = strdup( "Unknown session id" );
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
                }

                /* first try to create the access out */
                if( p_sys->i_ttl > 0 )
                    snprintf( psz_access, sizeof( psz_access ),
                              "udp{raw,ttl=%d}", p_sys->i_ttl );
                else
                    strncpy( psz_access, "udp{raw}", sizeof( psz_access ) );
                psz_access[sizeof( psz_access ) - 1] = '\0';

                snprintf( psz_url, sizeof( psz_url ),
                         ( strchr( ip, ':' ) != NULL ) ? "[%s]:%d" : "%s:%d",
                         ip, i_port );

                if( ( p_access = sout_AccessOutNew( p_stream->p_sout, psz_access, psz_url ) ) == NULL )
                {
                    msg_Err( p_stream, "cannot create the access out for %s://%s",
                             psz_access, psz_url );
                    answer->i_status = 500;
                    answer->psz_status = strdup( "Internal server error" );
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    break;
                }

                TAB_APPEND( rtsp->i_id, rtsp->id, id );
                TAB_APPEND( rtsp->i_access, rtsp->access, p_access );

                answer->i_status = 200;
                answer->psz_status = strdup( "OK" );
                answer->i_body = 0;
                answer->p_body = NULL;

                httpd_MsgAdd( answer, "Transport",
                              "RTP/AVP/UDP;client_port=%d-%d", i_port, i_port + 1 );
            }
            else /* TODO  strstr( psz_transport, "interleaved" ) ) */
            {
                answer->i_status = 461;
                answer->psz_status = strdup( "Unsupported Transport" );
                answer->i_body = 0;
                answer->p_body = NULL;
            }
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    httpd_MsgAdd( answer, "Server", "VLC Server" );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cseq", "%d", atoi( httpd_MsgGet( query, "Cseq" ) ) );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
    {
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );
    }
    return VLC_SUCCESS;
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

    out->p_buffer[ 8] = id->ssrc[0];
    out->p_buffer[ 9] = id->ssrc[1];
    out->p_buffer[10] = id->ssrc[2];
    out->p_buffer[11] = id->ssrc[3];

    out->i_buffer = 12;
    id->i_sequence++;
}

static void rtp_packetize_send( sout_stream_id_t *id, block_t *out )
{
    int i;
    vlc_mutex_lock( &id->lock_rtsp );
    for( i = 0; i < id->i_rtsp_access; i++ )
    {
        sout_AccessOutWrite( id->rtsp_access[i], block_Duplicate( out ) );
    }
    vlc_mutex_unlock( &id->lock_rtsp );

    if( id->p_access )
    {
        sout_AccessOutWrite( id->p_access, out );
    }
    else
    {
        block_Release( out );
    }
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
        out->p_buffer[13] = 0x00; /* ToC */ /* FIXME: frame type */

        /* FIXME: are we fed multiple frames ? */
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
