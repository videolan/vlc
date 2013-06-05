/*****************************************************************************
 * rtsp.c: rtsp VoD server module
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_httpd.h>
#include <vlc_vod.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_rand.h>

#ifndef _WIN32
# include <locale.h>
#endif

#ifdef HAVE_XLOCALE_H
# include <xlocale.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define THROTTLE_TEXT N_( "Maximum number of connections" )
#define THROTTLE_LONGTEXT N_( "This limits the maximum number of clients " \
    "that can connect to the RTSP VOD. 0 means no limit."  )

#define RAWMUX_TEXT N_( "MUX for RAW RTSP transport" )

#define SESSION_TIMEOUT_TEXT N_( "Sets the timeout option in the RTSP " \
    "session string" )
#define SESSION_TIMEOUT_LONGTEXT N_( "Defines what timeout option to add " \
    "to the RTSP session ID string. Setting it to a negative number removes " \
    "the timeout option entirely. This is needed by some IPTV STBs (such as " \
    "those made by HansunTech) which get confused by it. The default is 5." )

vlc_module_begin ()
    set_shortname( N_("RTSP VoD" ) )
    set_description( N_("RTSP VoD server") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_VOD )
    set_capability( "vod server", 1 )
    set_callbacks( Open, Close )
    add_shortcut( "rtsp" )
    add_string( "rtsp-raw-mux", "ts", RAWMUX_TEXT,
                RAWMUX_TEXT, true )
    add_integer( "rtsp-throttle-users", 0, THROTTLE_TEXT,
                 THROTTLE_LONGTEXT, true )
    add_integer( "rtsp-session-timeout", 5, SESSION_TIMEOUT_TEXT,
                 SESSION_TIMEOUT_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

typedef struct media_es_t media_es_t;

typedef struct
{
    media_es_t *p_media_es;
    int i_port;

} rtsp_client_es_t;

typedef struct
{
    char *psz_session;

    bool b_playing; /* is it in "play" state */
    int i_port_raw;

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
    uint8_t     i_payload_type;
    const char  *psz_ptname;
    unsigned    i_clock_rate;
    unsigned    i_channels;
    char        *psz_fmtp;

};

struct vod_media_t
{
    int id;

    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    httpd_url_t  *p_rtsp_url;
    char         *psz_rtsp_control_v4;
    char         *psz_rtsp_control_v6;
    char         *psz_rtsp_path;

    vlc_mutex_t lock;

    /* ES list */
    int        i_es;
    media_es_t **es;
    const char *psz_mux;
    bool  b_raw;

    /* RTSP client */
    int           i_rtsp;
    rtsp_client_t **rtsp;

    /* Infos */
    mtime_t i_length;
};

struct vod_sys_t
{
    /* RTSP server */
    httpd_host_t *p_rtsp_host;
    char *psz_path;
    int i_throttle_users;
    int i_connections;

    char *psz_raw_mux;

    int i_session_timeout;

    /* List of media */
    int i_media_id;
    int i_media;
    vod_media_t **media;

    /* */
    vlc_thread_t thread;
    block_fifo_t *p_fifo_cmd;
};

/* rtsp delayed command (to avoid deadlock between vlm/httpd) */
typedef enum
{
    RTSP_CMD_TYPE_NONE,  /* Exit requested */

    RTSP_CMD_TYPE_PLAY,
    RTSP_CMD_TYPE_PAUSE,
    RTSP_CMD_TYPE_STOP,
    RTSP_CMD_TYPE_SEEK,
    RTSP_CMD_TYPE_REWIND,
    RTSP_CMD_TYPE_FORWARD,

    RTSP_CMD_TYPE_ADD,
    RTSP_CMD_TYPE_DEL,
} rtsp_cmd_type_t;

/* */
typedef struct
{
    int i_type;
    int i_media_id;
    vod_media_t *p_media;
    char *psz_session;
    char *psz_arg;
    int64_t i_arg;
    double f_arg;
} rtsp_cmd_t;

static vod_media_t *MediaNew( vod_t *, const char *, input_item_t * );
static void         MediaDel( vod_t *, vod_media_t * );
static void         MediaAskDel ( vod_t *, vod_media_t * );
static int          MediaAddES( vod_t *, vod_media_t *, es_format_t * );
static void         MediaDelES( vod_t *, vod_media_t *, es_format_t * );

static void* CommandThread( void * );
static void  CommandPush( vod_t *, rtsp_cmd_type_t, vod_media_t *,
                          const char *psz_session, int64_t i_arg,
                          double f_arg, const char *psz_arg );

static rtsp_client_t *RtspClientNew( vod_media_t *, char * );
static rtsp_client_t *RtspClientGet( vod_media_t *, const char * );
static void           RtspClientDel( vod_media_t *, rtsp_client_t * );

static int RtspCallback( httpd_callback_sys_t *, httpd_client_t *,
                         httpd_message_t *, const httpd_message_t * );
static int RtspCallbackES( httpd_callback_sys_t *, httpd_client_t *,
                           httpd_message_t *, const httpd_message_t * );

static char *SDPGenerate( const vod_media_t *, httpd_client_t *cl );

static void sprintf_hexa( char *s, uint8_t *p_data, int i_data )
{
    static const char hex[16] = "0123456789abcdef";

    for( int i = 0; i < i_data; i++ )
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
    vod_sys_t *p_sys = NULL;
    char *psz_url = NULL;
    vlc_url_t url;

    psz_url = var_InheritString( p_vod, "rtsp-host" );
    vlc_UrlParse( &url, psz_url, 0 );
    free( psz_url );

    p_vod->p_sys = p_sys = malloc( sizeof( vod_sys_t ) );
    if( !p_sys ) goto error;
    p_sys->p_rtsp_host = 0;

    p_sys->i_session_timeout = var_CreateGetInteger( p_this, "rtsp-session-timeout" );

    p_sys->i_throttle_users = var_CreateGetInteger( p_this, "rtsp-throttle-users" );
    msg_Dbg( p_this, "allowing up to %d connections", p_sys->i_throttle_users );
    p_sys->i_connections = 0;

    p_sys->psz_raw_mux = var_CreateGetString( p_this, "rtsp-raw-mux" );

    p_sys->p_rtsp_host = vlc_rtsp_HostNew( VLC_OBJECT(p_vod) );
    if( !p_sys->p_rtsp_host )
    {
        msg_Err( p_vod, "cannot create RTSP server" );
        goto error;
    }

    p_sys->psz_path = strdup( url.psz_path ? url.psz_path : "/" );

    vlc_UrlClean( &url );

    TAB_INIT( p_sys->i_media, p_sys->media );
    p_sys->i_media_id = 0;

    p_vod->pf_media_new = MediaNew;
    p_vod->pf_media_del = MediaAskDel;

    p_sys->p_fifo_cmd = block_FifoNew();
    if( vlc_clone( &p_sys->thread, CommandThread, p_vod, VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_vod, "cannot spawn rtsp vod thread" );
        block_FifoRelease( p_sys->p_fifo_cmd );
        free( p_sys->psz_path );
        goto error;
    }

    return VLC_SUCCESS;

error:
    if( p_sys )
    {
        if( p_sys->p_rtsp_host ) httpd_HostDelete( p_sys->p_rtsp_host );
        free( p_sys->psz_raw_mux );
        free( p_sys );
    }
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

    /* Stop command thread */
    CommandPush( p_vod, RTSP_CMD_TYPE_NONE, NULL, NULL, 0, 0.0, NULL );
    vlc_join( p_sys->thread, NULL );

    while( block_FifoCount( p_sys->p_fifo_cmd ) > 0 )
    {
        rtsp_cmd_t cmd;
        block_t *p_block_cmd = block_FifoGet( p_sys->p_fifo_cmd );
        memcpy( &cmd, p_block_cmd->p_buffer, sizeof(cmd) );
        block_Release( p_block_cmd );
        if ( cmd.i_type == RTSP_CMD_TYPE_DEL )
            MediaDel(p_vod, cmd.p_media);
        free( cmd.psz_session );
        free( cmd.psz_arg );
    }
    block_FifoRelease( p_sys->p_fifo_cmd );

    httpd_HostDelete( p_sys->p_rtsp_host );
    var_Destroy( p_this, "rtsp-session-timeout" );
    var_Destroy( p_this, "rtsp-throttle-users" );
    var_Destroy( p_this, "rtsp-raw-mux" );

    /* Check VLM is not buggy */
    if( p_sys->i_media > 0 )
        msg_Err( p_vod, "rtsp vod leaking %d medias", p_sys->i_media );
    TAB_CLEAN( p_sys->i_media, p_sys->media );

    free( p_sys->psz_path );
    free( p_sys->psz_raw_mux );
    free( p_sys );
}

/*****************************************************************************
 * Media handling
 *****************************************************************************/
static vod_media_t *MediaNew( vod_t *p_vod, const char *psz_name,
                              input_item_t *p_item )
{
    vod_sys_t *p_sys = p_vod->p_sys;

    vod_media_t *p_media = calloc( 1, sizeof(vod_media_t) );
    if( !p_media )
        return NULL;

    p_media->id = p_sys->i_media_id++;
    TAB_INIT( p_media->i_es, p_media->es );
    p_media->psz_mux = NULL;
    TAB_INIT( p_media->i_rtsp, p_media->rtsp );
    p_media->b_raw = false;

    if( asprintf( &p_media->psz_rtsp_path, "%s%s",
                  p_sys->psz_path, psz_name ) <0 )
        return NULL;
    p_media->p_rtsp_url =
        httpd_UrlNew( p_sys->p_rtsp_host, p_media->psz_rtsp_path, NULL, NULL );

    if( !p_media->p_rtsp_url )
    {
        msg_Err( p_vod, "cannot create RTSP url (%s)", p_media->psz_rtsp_path);
        free( p_media->psz_rtsp_path );
        free( p_media );
        return NULL;
    }

    msg_Dbg( p_vod, "created RTSP url: %s", p_media->psz_rtsp_path );

    if( asprintf( &p_media->psz_rtsp_control_v4,
                  "rtsp://%%s:%%d%s/trackID=%%d",
                  p_media->psz_rtsp_path ) < 0 )
    {
        httpd_UrlDelete( p_media->p_rtsp_url );
        free( p_media->psz_rtsp_path );
        free( p_media );
        return NULL;
    }
    if( asprintf( &p_media->psz_rtsp_control_v6,
                  "rtsp://[%%s]:%%d%s/trackID=%%d",
                  p_media->psz_rtsp_path ) < 0 )
    {
        httpd_UrlDelete( p_media->p_rtsp_url );
        free( p_media->psz_rtsp_path );
        free( p_media );
        return NULL;
    }

    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_SETUP,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_DESCRIBE,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_PLAY,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_PAUSE,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_GETPARAMETER,
                    RtspCallback, (void*)p_media );
    httpd_UrlCatch( p_media->p_rtsp_url, HTTPD_MSG_TEARDOWN,
                    RtspCallback, (void*)p_media );

    p_media->p_vod = p_vod;

    vlc_mutex_init( &p_media->lock );

    p_media->i_length = input_item_GetDuration( p_item );

    vlc_mutex_lock( &p_item->lock );
    msg_Dbg( p_vod, "media has %i declared ES", p_item->i_es );
    for( int i = 0; i < p_item->i_es; i++ )
    {
        MediaAddES( p_vod, p_media, p_item->es[i] );
    }
    vlc_mutex_unlock( &p_item->lock );

    CommandPush( p_vod, RTSP_CMD_TYPE_ADD, p_media, NULL, 0, 0.0, NULL );
    return p_media;
}

static void MediaAskDel ( vod_t *p_vod, vod_media_t *p_media )
{
    CommandPush( p_vod, RTSP_CMD_TYPE_DEL, p_media, NULL, 0, 0.0, NULL );
}

static void MediaDel( vod_t *p_vod, vod_media_t *p_media )
{
    vod_sys_t *p_sys = p_vod->p_sys;

    msg_Dbg( p_vod, "deleting media: %s", p_media->psz_rtsp_path );

    TAB_REMOVE( p_sys->i_media, p_sys->media, p_media );

    httpd_UrlDelete( p_media->p_rtsp_url );

    while( p_media->i_rtsp > 0 )
        RtspClientDel( p_media, p_media->rtsp[0] );
    TAB_CLEAN( p_media->i_rtsp, p_media->rtsp );

    free( p_media->psz_rtsp_path );
    free( p_media->psz_rtsp_control_v6 );
    free( p_media->psz_rtsp_control_v4 );

    while( p_media->i_es )
        MediaDelES( p_vod, p_media, &p_media->es[0]->fmt );
    TAB_CLEAN( p_media->i_es, p_media->es );

    vlc_mutex_destroy( &p_media->lock );

    free( p_media );
}

static int MediaAddES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt )
{
    char *psz_urlc;

    media_es_t *p_es = calloc( 1, sizeof(media_es_t) );
    if( !p_es )
        return VLC_ENOMEM;

    p_media->psz_mux = NULL;

    /* TODO: update SDP, etc... */
    if( asprintf( &psz_urlc, "%s/trackID=%d",
              p_media->psz_rtsp_path, p_media->i_es ) < 0 )
    {
        free( p_es );
        return VLC_ENOMEM;
    }
    msg_Dbg( p_vod, "  - ES %4.4s (%s)", (char *)&p_fmt->i_codec, psz_urlc );

    /* Dynamic payload. No conflict since we put each ES in its own
     * RTP session */
    p_es->i_payload_type = 96;
    p_es->i_clock_rate = 90000;
    p_es->i_channels = 1;

    switch( p_fmt->i_codec )
    {
        case VLC_CODEC_S16B:
            if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 44100 )
            {
                p_es->i_payload_type = 11;
            }
            else if( p_fmt->audio.i_channels == 2 &&
                     p_fmt->audio.i_rate == 44100 )
            {
                p_es->i_payload_type = 10;
            }
            p_es->psz_ptname = "L16";
            p_es->i_clock_rate = p_fmt->audio.i_rate;
            p_es->i_channels = p_fmt->audio.i_channels;
            break;
        case VLC_CODEC_U8:
            p_es->psz_ptname = "L8";
            p_es->i_clock_rate = p_fmt->audio.i_rate;
            p_es->i_channels = p_fmt->audio.i_channels;
            break;
        case VLC_CODEC_MPGA:
            p_es->i_payload_type = 14;
            p_es->psz_ptname = "MPA";
            break;
        case VLC_CODEC_MPGV:
            p_es->i_payload_type = 32;
            p_es->psz_ptname = "MPV";
            break;
        case VLC_CODEC_A52:
            p_es->psz_ptname = "ac3";
            p_es->i_clock_rate = p_fmt->audio.i_rate;
            break;
        case VLC_CODEC_H263:
            p_es->psz_ptname = "H263-1998";
            break;
        case VLC_CODEC_H264:
            p_es->psz_ptname = "H264";
            p_es->psz_fmtp = NULL;
            /* FIXME AAAAAAAAAAAARRRRRRRRGGGG copied from stream_out/rtp.c */
            if( p_fmt->i_extra > 0 )
            {
                uint8_t *p_buffer = p_fmt->p_extra;
                int     i_buffer = p_fmt->i_extra;
                char    *p_64_sps = NULL;
                char    *p_64_pps = NULL;
                char    hexa[6+1];

                while( i_buffer > 4 )
                {
                    int i_offset    = 0;
                    int i_size      = 0;

                    while( p_buffer[0] != 0 || p_buffer[1] != 0 ||
                           p_buffer[2] != 1 )
                    {
                        p_buffer++;
                        i_buffer--;
                        if( i_buffer == 0 ) break;
                    }

                    if( i_buffer < 4 || memcmp(p_buffer, "\x00\x00\x01", 3 ) )
                    {
                        /* No startcode found.. */
                        break;
                    }
                    p_buffer += 3;
                    i_buffer -= 3;

                    const int i_nal_type = p_buffer[0]&0x1f;

                    i_size = i_buffer;
                    for( i_offset = 0; i_offset+2 < i_buffer ; i_offset++)
                    {
                        if( !memcmp(p_buffer + i_offset, "\x00\x00\x01", 3 ) )
                        {
                            /* we found another startcode */
                            while( i_offset > 0 && 0 == p_buffer[ i_offset - 1 ] )
                                i_offset--;
                            i_size = i_offset;
                            break;
                        }
                    }

                    if( i_size == 0 )
                    {
                        /* No-info found in nal */
                        continue;
                    }

                    if( i_nal_type == 7 )
                    {
                        free( p_64_sps );
                        p_64_sps = vlc_b64_encode_binary( p_buffer, i_size );
                        /* XXX: nothing ensures that i_size >= 4 ?? */
                        sprintf_hexa( hexa, &p_buffer[1], 3 );
                    }
                    else if( i_nal_type == 8 )
                    {
                        free( p_64_pps );
                        p_64_pps = vlc_b64_encode_binary( p_buffer, i_size );
                    }
                    i_buffer -= i_size;
                    p_buffer += i_size;
                }
                /* */
                if( p_64_sps && p_64_pps )
                {
                    if( asprintf( &p_es->psz_fmtp,
                                  "packetization-mode=1;profile-level-id=%s;"
                                  "sprop-parameter-sets=%s,%s;", hexa, p_64_sps,
                                  p_64_pps ) < 0 )
                    {
                        free( p_64_sps );
                        free( p_64_pps );
                        free( psz_urlc );
                        free( p_es );
                        return VLC_ENOMEM;
                    }
                }
                free( p_64_sps );
                free( p_64_pps );
            }
            if( !p_es->psz_fmtp )
                p_es->psz_fmtp = strdup( "packetization-mode=1" );
            break;
        case VLC_CODEC_MP4V:
            p_es->psz_ptname = "MP4V-ES";
            if( p_fmt->i_extra > 0 )
            {
                char *p_hexa = malloc( 2 * p_fmt->i_extra + 1 );
                sprintf_hexa( p_hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &p_es->psz_fmtp,
                              "profile-level-id=3; config=%s;", p_hexa ) == -1 )
                    p_es->psz_fmtp = NULL;
                free( p_hexa );
            }
            break;
        case VLC_CODEC_MP4A:
            p_es->psz_ptname = "mpeg4-generic";
            p_es->i_clock_rate = p_fmt->audio.i_rate;
            if( p_fmt->i_extra > 0 )
            {
                char *p_hexa = malloc( 2 * p_fmt->i_extra + 1 );
                sprintf_hexa( p_hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &p_es->psz_fmtp,
                              "streamtype=5; profile-level-id=15; mode=AAC-hbr; "
                              "config=%s; SizeLength=13;IndexLength=3; "
                              "IndexDeltaLength=3; Profile=1;", p_hexa ) == -1 )
                    p_es->psz_fmtp = NULL;
                free( p_hexa );
            }
            break;
        case VLC_FOURCC( 'm', 'p', '2', 't' ):
            p_media->psz_mux = "ts";
            p_es->i_payload_type = 33;
            p_es->psz_ptname = "MP2T";
            break;
        case VLC_FOURCC( 'm', 'p', '2', 'p' ):
            p_media->psz_mux = "ps";
            p_es->psz_ptname = "MP2P";
            break;
        case VLC_CODEC_AMR_NB:
            p_es->psz_ptname = "AMR";
            p_es->i_clock_rate = 8000;
            if(p_fmt->audio.i_channels == 2 )
                p_es->i_channels = 2;
            p_es->psz_fmtp = strdup( "octet-align=1" );
            break;
        case VLC_CODEC_AMR_WB:
            p_es->psz_ptname = "AMR-WB";
            p_es->i_clock_rate = 16000;
            if(p_fmt->audio.i_channels == 2 )
                p_es->i_channels = 2;
            p_es->psz_fmtp = strdup( "octet-align=1" );
            break;

        default:
            msg_Err( p_vod, "cannot add this stream (unsupported "
                    "codec: %4.4s)", (char*)&p_fmt->i_codec );
            free( psz_urlc );
            free( p_es );
            return VLC_EGENERIC;
    }

    p_es->p_rtsp_url =
        httpd_UrlNew( p_vod->p_sys->p_rtsp_host, psz_urlc, NULL, NULL );

    if( !p_es->p_rtsp_url )
    {
        msg_Err( p_vod, "cannot create RTSP url (%s)", psz_urlc );
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

    vlc_mutex_lock( &p_media->lock );
    TAB_APPEND( p_media->i_es, p_media->es, p_es );
    vlc_mutex_unlock( &p_media->lock );

    return VLC_SUCCESS;
}

static void MediaDelES( vod_t *p_vod, vod_media_t *p_media, es_format_t *p_fmt)
{
    media_es_t *p_es = NULL;

    /* Find the ES */
    for( int i = 0; i < p_media->i_es; i++ )
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

    free( p_es->psz_fmtp );

    if( p_es->p_rtsp_url ) httpd_UrlDelete( p_es->p_rtsp_url );
    es_format_Clean( &p_es->fmt );
    free( p_es );
}

static void CommandPush( vod_t *p_vod, rtsp_cmd_type_t i_type, vod_media_t *p_media, const char *psz_session, int64_t i_arg,
                         double f_arg, const char *psz_arg )
{
    rtsp_cmd_t cmd;
    block_t *p_cmd;

    memset( &cmd, 0, sizeof(cmd) );
    cmd.i_type = i_type;
    cmd.p_media = p_media;
    if( p_media )
        cmd.i_media_id = p_media->id;
    if( psz_session )
        cmd.psz_session = strdup(psz_session);
    cmd.i_arg = i_arg;
    cmd.f_arg = f_arg;
    if( psz_arg )
        cmd.psz_arg = strdup(psz_arg);

    p_cmd = block_Alloc( sizeof(rtsp_cmd_t) );
    memcpy( p_cmd->p_buffer, &cmd, sizeof(cmd) );

    block_FifoPut( p_vod->p_sys->p_fifo_cmd, p_cmd );
}

static void* CommandThread( void *obj )
{
    vod_t *p_vod = (vod_t*)obj;
    vod_sys_t *p_sys = p_vod->p_sys;
    int canc = vlc_savecancel ();

    for( ;; )
    {
        block_t *p_block_cmd = block_FifoGet( p_sys->p_fifo_cmd );
        rtsp_cmd_t cmd;
        vod_media_t *p_media = NULL;
        int i;

        if( !p_block_cmd )
            break;

        memcpy( &cmd, p_block_cmd->p_buffer, sizeof(cmd) );
        block_Release( p_block_cmd );

        if( cmd.i_type == RTSP_CMD_TYPE_NONE )
            break;

        if ( cmd.i_type == RTSP_CMD_TYPE_ADD )
        {
            TAB_APPEND( p_sys->i_media, p_sys->media, cmd.p_media );
            goto next;
        }

        if ( cmd.i_type == RTSP_CMD_TYPE_DEL )
        {
            MediaDel(p_vod, cmd.p_media);
            goto next;
        }

        /* */
        for( i = 0; i < p_sys->i_media; i++ )
        {
            if( p_sys->media[i]->id == cmd.i_media_id )
                break;
        }
        if( i >= p_sys->i_media )
        {
            goto next;
        }
        p_media = p_sys->media[i];

        switch( cmd.i_type )
        {
        case RTSP_CMD_TYPE_PLAY:
            cmd.i_arg = -1;
            vod_MediaControl( p_vod, p_media, cmd.psz_session,
                              VOD_MEDIA_PLAY, cmd.psz_arg, &cmd.i_arg );
            break;
        case RTSP_CMD_TYPE_PAUSE:
            cmd.i_arg = -1;
            vod_MediaControl( p_vod, p_media, cmd.psz_session,
                              VOD_MEDIA_PAUSE, &cmd.i_arg );
            break;

        case RTSP_CMD_TYPE_STOP:
            vod_MediaControl( p_vod, p_media, cmd.psz_session, VOD_MEDIA_STOP );
            break;

        case RTSP_CMD_TYPE_SEEK:
            vod_MediaControl( p_vod, p_media, cmd.psz_session,
                              VOD_MEDIA_SEEK, cmd.i_arg );
            break;

        case RTSP_CMD_TYPE_REWIND:
            vod_MediaControl( p_vod, p_media, cmd.psz_session,
                              VOD_MEDIA_REWIND, cmd.f_arg );
            break;

        case RTSP_CMD_TYPE_FORWARD:
            vod_MediaControl( p_vod, p_media, cmd.psz_session,
                              VOD_MEDIA_FORWARD, cmd.f_arg );
            break;

        default:
            break;
        }

    next:
        free( cmd.psz_session );
        free( cmd.psz_arg );
    }

    vlc_restorecancel (canc);
    return NULL;
}

/****************************************************************************
 * RTSP server implementation
 ****************************************************************************/
static rtsp_client_t *RtspClientNew( vod_media_t *p_media, char *psz_session )
{
    rtsp_client_t *p_rtsp = calloc( 1, sizeof(rtsp_client_t) );

    if( !p_rtsp )
        return NULL;
    p_rtsp->es = 0;

    p_rtsp->psz_session = psz_session;
    TAB_APPEND( p_media->i_rtsp, p_media->rtsp, p_rtsp );

    p_media->p_vod->p_sys->i_connections++;
    msg_Dbg( p_media->p_vod, "new session: %s, connections: %d",
             psz_session, p_media->p_vod->p_sys->i_throttle_users );

    return p_rtsp;
}

static rtsp_client_t *RtspClientGet( vod_media_t *p_media, const char *psz_session )
{
    for( int i = 0; psz_session && i < p_media->i_rtsp; i++ )
    {
        if( !strcmp( p_media->rtsp[i]->psz_session, psz_session ) )
            return p_media->rtsp[i];
    }

    return NULL;
}

static void RtspClientDel( vod_media_t *p_media, rtsp_client_t *p_rtsp )
{
    p_media->p_vod->p_sys->i_connections--;
    msg_Dbg( p_media->p_vod, "closing session: %s, connections: %d",
             p_rtsp->psz_session, p_media->p_vod->p_sys->i_throttle_users );

    while( p_rtsp->i_es )
    {
        p_rtsp->i_es--;
        free( p_rtsp->es[p_rtsp->i_es] );
    }
    free( p_rtsp->es );

    TAB_REMOVE( p_media->i_rtsp, p_media->rtsp, p_rtsp );

    free( p_rtsp->psz_session );
    free( p_rtsp );
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


static int RtspCallback( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                         httpd_message_t *answer, const httpd_message_t *query )
{
    vod_media_t *p_media = (vod_media_t*)p_args;
    vod_t *p_vod = p_media->p_vod;
    const char *psz_transport = NULL;
    const char *psz_playnow = NULL; /* support option: x-playNow */
    const char *psz_session = NULL;
    const char *psz_cseq = NULL;
    rtsp_client_t *p_rtsp;
    int i_cseq = 0;

    if( answer == NULL || query == NULL ) return VLC_SUCCESS;

    msg_Dbg( p_vod, "RtspCallback query: type=%d", query->i_type );

    answer->i_proto   = HTTPD_PROTO_RTSP;
    answer->i_version = query->i_version;
    answer->i_type    = HTTPD_MSG_ANSWER;
    answer->i_body    = 0;
    answer->p_body    = NULL;

    switch( query->i_type )
    {
        case HTTPD_MSG_SETUP:
        {
            psz_playnow = httpd_MsgGet( query, "x-playNow" );
            psz_transport = httpd_MsgGet( query, "Transport" );
            if( psz_transport == NULL )
            {
                answer->i_status = 400;
                break;
            }
            msg_Dbg( p_vod, "HTTPD_MSG_SETUP: transport=%s", psz_transport );

            if( strstr( psz_transport, "unicast" ) &&
                strstr( psz_transport, "client_port=" ) )
            {
                rtsp_client_t *p_rtsp = NULL;
                char ip[NI_MAXNUMERICHOST];
                int i_port = atoi( strstr( psz_transport, "client_port=" ) +
                                   strlen("client_port=") );

                if( strstr( psz_transport, "MP2T/H2221/UDP" ) ||
                    strstr( psz_transport, "RAW/RAW/UDP" ) )
                {
                    p_media->psz_mux = p_vod->p_sys->psz_raw_mux;
                    p_media->b_raw = true;
                }

                if( httpd_ClientIP( cl, ip, NULL ) == NULL )
                {
                    answer->i_status = 500;
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    break;
                }

                msg_Dbg( p_vod, "HTTPD_MSG_SETUP: unicast ip=%s port=%d",
                         ip, i_port );

                psz_session = httpd_MsgGet( query, "Session" );
                if( !psz_session || !*psz_session )
                {
                    char *psz_new;
                    if( ( p_vod->p_sys->i_throttle_users > 0 ) &&
                        ( p_vod->p_sys->i_connections >= p_vod->p_sys->i_throttle_users ) )
                    {
                        answer->i_status = 503;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
#warning Should use secure randomness here! (spoofing risk)
                    if( asprintf( &psz_new, "%lu", vlc_mrand48() ) < 0 )
                        return VLC_ENOMEM;
                    psz_session = psz_new;

                    p_rtsp = RtspClientNew( p_media, psz_new );
                    if( !p_rtsp )
                    {
                        answer->i_status = 454;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
                }
                else
                {
                    p_rtsp = RtspClientGet( p_media, psz_session );
                    if( !p_rtsp )
                    {
                        answer->i_status = 454;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
                }

                answer->i_status = 200;
                answer->i_body = 0;
                answer->p_body = NULL;

                if( p_media->b_raw )
                {
                    p_rtsp->i_port_raw = i_port;

                    if( strstr( psz_transport, "MP2T/H2221/UDP" ) )
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "MP2T/H2221/UDP;unicast;client_port=%d-%d",
                                      i_port, i_port + 1 );
                    }
                    else if( strstr( psz_transport, "RAW/RAW/UDP" ) )
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "RAW/RAW/UDP;unicast;client_port=%d-%d",
                                      i_port, i_port + 1 );
                    }
                }
                else
                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;unicast;client_port=%d-%d",
                                  i_port, i_port + 1 );
            }
            else /* TODO  strstr( psz_transport, "interleaved" ) ) */
            {
                answer->i_status = 461;
                answer->i_body = 0;
                answer->p_body = NULL;
            }

            /* Intentional fall-through on x-playNow option in RTSP request */
            if( !psz_playnow )
                break;
        }

        case HTTPD_MSG_PLAY:
        {
            char *psz_output, ip[NI_MAXNUMERICHOST];
            int i_port_audio = 0, i_port_video = 0;

            /* for now only multicast so easy */
            if( !psz_playnow )
            {
                answer->i_status = 200;
                answer->i_body = 0;
                answer->p_body = NULL;
            }

            if( !psz_session )
                psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PLAY for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp )
            {
                answer->i_status = 500;
                answer->i_body = 0;
                answer->p_body = NULL;
                break;
            }

            if( p_rtsp->b_playing )
            {
                const char *psz_position = httpd_MsgGet( query, "Range" );
                const char *psz_scale = httpd_MsgGet( query, "Scale" );
                if( psz_position )
                    psz_position = strstr( psz_position, "npt=" );
                if( psz_position && !psz_scale )
                {
                    int64_t i_time = ParseNPT (psz_position + 4);
                    msg_Dbg( p_vod, "seeking request: %s", psz_position );
                    CommandPush( p_vod, RTSP_CMD_TYPE_SEEK, p_media,
                                 psz_session, i_time, 0.0, NULL );
                }
                else if( psz_scale )
                {
                    double f_scale = 0.0;
                    char *end;

                    f_scale = us_strtod( psz_scale, &end );
                    if( end > psz_scale )
                    {
                        f_scale = (f_scale * 30.0);
                        if( psz_scale[0] == '-' ) /* rewind */
                        {
                            msg_Dbg( p_vod, "rewind request: %s", psz_scale );
                            CommandPush( p_vod, RTSP_CMD_TYPE_REWIND, p_media,
                                         psz_session, 0, f_scale, NULL );
                        }
                        else if(psz_scale[0] != '1' ) /* fast-forward */
                        {
                            msg_Dbg( p_vod, "fastforward request: %s",
                                     psz_scale );
                            CommandPush( p_vod, RTSP_CMD_TYPE_FORWARD, p_media,
                                         psz_session, 0, f_scale, NULL );
                        }
                    }
                }
                /* unpause, in case it's paused */
                CommandPush( p_vod, RTSP_CMD_TYPE_PLAY, p_media, psz_session,
                             0, 0.0, "" );
                break;
            }

            if( httpd_ClientIP( cl, ip, NULL ) == NULL ) break;

            p_rtsp->b_playing = true;

            /* FIXME for != 1 video and 1 audio */
            for( int i = 0; i < p_rtsp->i_es; i++ )
            {
                if( p_rtsp->es[i]->p_media_es->fmt.i_cat == AUDIO_ES )
                    i_port_audio = p_rtsp->es[i]->i_port;
                if( p_rtsp->es[i]->p_media_es->fmt.i_cat == VIDEO_ES )
                    i_port_video = p_rtsp->es[i]->i_port;
            }

            if( p_media->psz_mux )
            {
                if( p_media->b_raw )
                {
                    if( asprintf( &psz_output,
                              "std{access=udp,dst=%s:%i,mux=%s}",
                              ip, p_rtsp->i_port_raw, p_media->psz_mux ) < 0 )
                        return VLC_ENOMEM;
                }
                else
                {
                    if( asprintf( &psz_output,
                              "rtp{dst=%s,port=%i,mux=%s}",
                              ip, i_port_video, p_media->psz_mux ) < 0 )
                        return VLC_ENOMEM;
                }
            }
            else
            {
                if( asprintf( &psz_output,
                              "rtp{dst=%s,port-video=%i,port-audio=%i}",
                              ip, i_port_video, i_port_audio ) < 0 )
                    return VLC_ENOMEM;
            }

            CommandPush( p_vod, RTSP_CMD_TYPE_PLAY, p_media, psz_session,
                         0, 0.0, psz_output );
            free( psz_output );
            break;
        }

        case HTTPD_MSG_DESCRIBE:
        {
            char *psz_sdp =
                SDPGenerate( p_media, cl );

            if( psz_sdp != NULL )
            {
                answer->i_status = 200;
                httpd_MsgAdd( answer, "Content-type",  "%s",
                              "application/sdp" );

                answer->p_body = (uint8_t *)psz_sdp;
                answer->i_body = strlen( psz_sdp );
            }
            else
            {
                answer->i_status = 500;
                answer->p_body = NULL;
                answer->i_body = 0;
            }
            break;
        }

        case HTTPD_MSG_PAUSE:
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PAUSE for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            CommandPush( p_vod, RTSP_CMD_TYPE_PAUSE, p_media, psz_session,
                         0, 0.0, NULL );

            answer->i_status = 200;
            answer->i_body = 0;
            answer->p_body = NULL;
            break;

        case HTTPD_MSG_TEARDOWN:
            /* for now only multicast so easy again */
            answer->i_status = 200;
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_TEARDOWN for session: %s", psz_session);

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            CommandPush( p_vod, RTSP_CMD_TYPE_STOP, p_media, psz_session,
                         0, 0.0, NULL );
            RtspClientDel( p_media, p_rtsp );
            break;

        case HTTPD_MSG_GETPARAMETER:
            answer->i_status = 200;
            answer->i_body = 0;
            answer->p_body = NULL;
            break;

        default:
            return VLC_EGENERIC;
    }

    httpd_MsgAdd( answer, "Server", "VLC/%s", VERSION );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    psz_cseq = httpd_MsgGet( query, "Cseq" );
    psz_cseq ? i_cseq = atoi( psz_cseq ) : 0;
    httpd_MsgAdd( answer, "CSeq", "%d", i_cseq );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
    {
         if( p_media->p_vod->p_sys->i_session_timeout >= 0 )
             httpd_MsgAdd( answer, "Session", "%s;timeout=%i", psz_session,
               p_media->p_vod->p_sys->i_session_timeout );
         else
              httpd_MsgAdd( answer, "Session", "%s", psz_session );
    }

    return VLC_SUCCESS;
}

static int RtspCallbackES( httpd_callback_sys_t *p_args, httpd_client_t *cl,
                           httpd_message_t *answer,
                           const httpd_message_t *query )
{
    media_es_t *p_es = (media_es_t*)p_args;
    vod_media_t *p_media = p_es->p_media;
    vod_t *p_vod = p_media->p_vod;
    rtsp_client_t *p_rtsp = NULL;
    const char *psz_transport = NULL;
    const char *psz_playnow = NULL; /* support option: x-playNow */
    const char *psz_session = NULL;
    const char *psz_position = NULL;
    const char *psz_cseq = NULL;
    int i_cseq = 0;

    if( answer == NULL || query == NULL ) return VLC_SUCCESS;

    msg_Dbg( p_vod, "RtspCallback query: type=%d", query->i_type );

    answer->i_proto   = HTTPD_PROTO_RTSP;
    answer->i_version = query->i_version;
    answer->i_type    = HTTPD_MSG_ANSWER;
    answer->i_body    = 0;
    answer->p_body      = NULL;

    switch( query->i_type )
    {
        case HTTPD_MSG_SETUP:
            psz_playnow = httpd_MsgGet( query, "x-playNow" );
            psz_transport = httpd_MsgGet( query, "Transport" );

            msg_Dbg( p_vod, "HTTPD_MSG_SETUP: transport=%s", psz_transport );

            if( strstr( psz_transport, "unicast" ) &&
                strstr( psz_transport, "client_port=" ) )
            {
                rtsp_client_t *p_rtsp = NULL;
                rtsp_client_es_t *p_rtsp_es = NULL;
                char ip[NI_MAXNUMERICHOST];
                int i_port = atoi( strstr( psz_transport, "client_port=" ) +
                                   strlen("client_port=") );

                if( httpd_ClientIP( cl, ip, NULL ) == NULL )
                {
                    answer->i_status = 500;
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    break;
                }

                msg_Dbg( p_vod, "HTTPD_MSG_SETUP: unicast ip=%s port=%d",
                        ip, i_port );

                psz_session = httpd_MsgGet( query, "Session" );
                if( !psz_session || !*psz_session )
                {
                    char *psz_new;
                    if( ( p_vod->p_sys->i_throttle_users > 0 ) &&
                        ( p_vod->p_sys->i_connections >= p_vod->p_sys->i_throttle_users ) )
                    {
                        answer->i_status = 503;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
#warning Session ID should be securely random (spoofing risk)
                    if( asprintf( &psz_new, "%lu", vlc_mrand48() ) < 0 )
                        return VLC_ENOMEM;
                    psz_session = psz_new;

                    p_rtsp = RtspClientNew( p_media, psz_new );
                    if( !p_rtsp )
                    {
                        answer->i_status = 454;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
                }
                else
                {
                    p_rtsp = RtspClientGet( p_media, psz_session );
                    if( !p_rtsp )
                    {
                        answer->i_status = 454;
                        answer->i_body = 0;
                        answer->p_body = NULL;
                        break;
                    }
                }

                p_rtsp_es = malloc( sizeof(rtsp_client_es_t) );
                if( !p_rtsp_es )
                {
                    answer->i_status = 500;
                    answer->i_body = 0;
                    answer->p_body = NULL;
                    break;
                }
                p_rtsp_es->i_port = i_port;
                p_rtsp_es->p_media_es = p_es;
                TAB_APPEND( p_rtsp->i_es, p_rtsp->es, p_rtsp_es );

                answer->i_status = 200;
                answer->i_body = 0;
                answer->p_body = NULL;

                if( p_media->b_raw )
                {
                    if( strstr( psz_transport, "MP2T/H2221/UDP" ) )
                    {
                        httpd_MsgAdd( answer, "Transport",
                                     "MP2T/H2221/UDP;unicast;client_port=%d-%d",
                                     p_rtsp_es->i_port, p_rtsp_es->i_port + 1 );
                    }
                    else if( strstr( psz_transport, "RAW/RAW/UDP" ) )
                    {
                        httpd_MsgAdd( answer, "Transport",
                                     "RAW/RAW/UDP;unicast;client_port=%d-%d",
                                     p_rtsp_es->i_port, p_rtsp_es->i_port + 1 );
                    }
                }
                else
                {
                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;unicast;client_port=%d-%d",
                                  p_rtsp_es->i_port, p_rtsp_es->i_port + 1 );
                }
            }
            else /* TODO  strstr( psz_transport, "interleaved" ) ) */
            {
                answer->i_status = 461;
                answer->i_body = 0;
                answer->p_body = NULL;
            }

            /* Intentional fall-through on x-playNow option in RTSP request */
            if( !psz_playnow )
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
                int64_t i_time = ParseNPT (psz_position + 4);
                msg_Dbg( p_vod, "seeking request: %s", psz_position );
                CommandPush( p_vod, RTSP_CMD_TYPE_SEEK, p_media,
                             psz_session, i_time, 0.0, NULL );
            }

            if( !psz_playnow )
            {
                answer->i_status = 200;
                answer->i_body = 0;
                answer->p_body = NULL;
            }
            break;

        case HTTPD_MSG_TEARDOWN:
            answer->i_status = 200;
            answer->i_body = 0;
            answer->p_body = NULL;

            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_TEARDOWN for session: %s", psz_session);

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            for( int i = 0; i < p_rtsp->i_es; i++ )
            {
                rtsp_client_es_t *es = p_rtsp->es[i];
                if( es->p_media_es == p_es )
                {
                    TAB_REMOVE( p_rtsp->i_es, p_rtsp->es, es );
                    break;
                }
            }

            if( !p_rtsp->i_es )
            {
                CommandPush( p_vod, RTSP_CMD_TYPE_STOP, p_media, psz_session,
                             0, 0.0, NULL );
                RtspClientDel( p_media, p_rtsp );
            }
            break;

        case HTTPD_MSG_PAUSE:
            /* This is kind of a kludge. Should we only support Aggregate
             * Operations ? */
            psz_session = httpd_MsgGet( query, "Session" );
            msg_Dbg( p_vod, "HTTPD_MSG_PAUSE for session: %s", psz_session );

            p_rtsp = RtspClientGet( p_media, psz_session );
            if( !p_rtsp ) break;

            CommandPush( p_vod, RTSP_CMD_TYPE_PAUSE, p_media, psz_session,
                         0, 0.0, NULL );

            answer->i_status = 200;
            answer->i_body = 0;
            answer->p_body = NULL;
            break;

        default:
            return VLC_EGENERIC;
            break;
    }

    httpd_MsgAdd( answer, "Server", "VLC/%s", VERSION );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    psz_cseq = httpd_MsgGet( query, "Cseq" );
    if (psz_cseq)
        i_cseq = atoi( psz_cseq );
    else
        i_cseq = 0;
    httpd_MsgAdd( answer, "Cseq", "%d", i_cseq );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SDPGenerate: TODO
 * FIXME: need to be moved to a common place ?
 *****************************************************************************/
static char *SDPGenerate( const vod_media_t *p_media, httpd_client_t *cl )
{
    char *psz_sdp, ip[NI_MAXNUMERICHOST];
    const char *psz_control;
    int port;

    if( httpd_ServerIP( cl, ip, &port ) == NULL )
        return NULL;

    bool ipv6 = ( strchr( ip, ':' ) != NULL );

    psz_control = ipv6 ? p_media->psz_rtsp_control_v6
                       : p_media->psz_rtsp_control_v4;

    /* Dummy destination address for RTSP */
    struct sockaddr_storage dst;
    socklen_t dstlen = ipv6 ? sizeof( struct sockaddr_in6 )
                            : sizeof( struct sockaddr_in );
    memset (&dst, 0, dstlen);
    dst.ss_family = ipv6 ? AF_INET6 : AF_INET;
#ifdef HAVE_SA_LEN
    dst.ss_len = dstlen;
#endif

    psz_sdp = vlc_sdp_Start( VLC_OBJECT( p_media->p_vod ), "sout-rtp-",
                             NULL, 0, (struct sockaddr *)&dst, dstlen );
    if( psz_sdp == NULL )
        return NULL;

    if( p_media->i_length > 0 )
    {
        lldiv_t d = lldiv( p_media->i_length / 1000, 1000 );
        sdp_AddAttribute( &psz_sdp, "range","npt=0-%lld.%03u", d.quot,
                          (unsigned)d.rem );
    }

    for( int i = 0; i < p_media->i_es; i++ )
    {
        media_es_t *p_es = p_media->es[i];
        const char *mime_major; /* major MIME type */

        switch( p_es->fmt.i_cat )
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

        sdp_AddMedia( &psz_sdp, mime_major, "RTP/AVP", 0 /* p_es->i_port */,
                      p_es->i_payload_type, false, 0,
                      p_es->psz_ptname, p_es->i_clock_rate, p_es->i_channels,
                      p_es->psz_fmtp );

        sdp_AddAttribute( &psz_sdp, "control", psz_control, ip, port, i );
    }

    return psz_sdp;
}
