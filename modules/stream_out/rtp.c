/*****************************************************************************
 * rtp.c: rtp stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id: rtp.c,v 1.12 2004/02/02 14:43:50 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "httpd.h"
#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("RTP stream output") );
    set_capability( "sout stream", 0 );
    add_shortcut( "rtp" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *,
                               sout_buffer_t* );

struct sout_stream_sys_t
{
    /* sdp */
    int64_t i_sdp_id;
    int     i_sdp_version;
    char    *psz_sdp;

    vlc_mutex_t  lock_sdp;

    httpd_t      *p_httpd;
    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    /* */
    char *psz_destination;
    int  i_port;
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
    sout_buffer_t     *packet;

    /* */
    int              i_es;
    sout_stream_id_t **es;
};

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *,
                                    sout_buffer_t * );

struct sout_stream_id_t
{
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
    sout_input_t      *p_input;
};
static int AccessOutGrabberWrite( sout_access_out_t *, sout_buffer_t * );

static int  HttpCallback( httpd_file_callback_args_t *p_args,
                          uint8_t *p_request, int i_request,
                          uint8_t **pp_data, int *pi_data );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    sout_stream_sys_t   *p_sys;

    char *val;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->psz_destination = sout_cfg_find_value( p_stream->p_cfg, "dst" );
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "port" ) ) )
    {
        p_sys->i_port = atoi( val );
    }
    if( !p_sys->psz_destination || *p_sys->psz_destination == '\0' ||
        p_sys->i_port <= 0 )
    {
        msg_Err( p_stream, "invalid/missing dst or port" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "ttl" ) ) )
    {
        p_sys->i_ttl = atoi( val );
    }
    else
    {
        p_sys->i_ttl = config_GetInt( p_stream, "ttl" );
    }

    p_sys->i_payload_type = 96;
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->i_sdp_id = mdate();
    p_sys->i_sdp_version = 1;
    p_sys->psz_sdp = NULL;
    p_sys->p_httpd = NULL;
    p_sys->p_httpd_host = NULL;
    p_sys->p_httpd_file = NULL;

    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "mux" ) ) )
    {
        sout_access_out_t *p_grab;

        char *psz_rtpmap;
        char access[100];
        char url[strlen( p_sys->psz_destination ) + 1 + 12 + 1];

        /* Check muxer type */
        if( !strncasecmp( val, "ps", 2 ) || !strncasecmp( val, "mpeg1", 5 ) )
        {
            psz_rtpmap = "MP2P/90000";
        }
        else if( !strncasecmp( val, "ts", 2 ) )
        {
            psz_rtpmap = "MP2T/90000";
            p_sys->i_payload_type = 33;
        }
        else
        {
            msg_Err( p_stream, "unsupported muxer type with rtp (only ts/ps)" );
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
        sprintf( url, "%s:%d", p_sys->psz_destination, p_sys->i_port );
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
        if( !( p_sys->p_mux = sout_MuxNew( p_sout, val, p_sys->p_grab ) ) )
        {
            msg_Err( p_stream, "cannot create the muxer (%s)", val );
            sout_AccessOutDelete( p_sys->p_grab );
            sout_AccessOutDelete( p_sys->p_access );
            free( p_sys );
            return VLC_EGENERIC;
        }
        p_sout->i_preheader = __MAX( p_sout->i_preheader,
                                     p_sys->p_mux->i_preheader );

        /* create the SDP only once */
        p_sys->psz_sdp =
            malloc( 200 + 20 + 10 + strlen( p_sys->psz_destination ) +
                    10 + 10 + 10 + 10 + strlen( psz_rtpmap ) );
        sprintf( p_sys->psz_sdp,
                 "v=0\n"
                 "o=- "I64Fd" %d IN IP4 127.0.0.1\n"
                 "s=NONE\n"
                 "c=IN IP4 %s/%d\n"
                 "m=video %d RTP/AVP %d\n"
                 "a=rtpmap:%d %s\n",
                 p_sys->i_sdp_id, p_sys->i_sdp_version,
                 p_sys->psz_destination, p_sys->i_ttl,
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

    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "sdp" ) ) )
    {
        vlc_url_t url;

        vlc_UrlParse( &url, val, 0 );
        if( url.psz_protocol && !strcmp( url.psz_protocol, "http" ) )
        {
            if( ( p_sys->p_httpd =
                  httpd_Find( VLC_OBJECT(p_stream), VLC_TRUE ) ) )
            {
                p_sys->p_httpd_host =
                    p_sys->p_httpd->pf_register_host( p_sys->p_httpd,
                                                      url.psz_host,
                                                      url.i_port );
                if( p_sys->p_httpd_host )
                {
                    p_sys->p_httpd_file =
                        p_sys->p_httpd->pf_register_file( p_sys->p_httpd,
                            url.psz_path ? url.psz_path : "/",
                            "application/sdp", NULL, NULL,
                            HttpCallback, HttpCallback, (void*)p_sys );
                }
            }
            if( p_sys->p_httpd_file == NULL )
            {
                msg_Err( p_stream, "cannot export sdp as http" );
            }
        }
        else
        {
            msg_Warn( p_stream, "unknow protocol for SDP (%s)",
                      url.psz_protocol );
        }
        vlc_UrlClean( &url );
    }

    vlc_mutex_init( p_stream, &p_sys->lock_sdp );

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

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
        sout_MuxDelete( p_sys->p_mux );
        sout_AccessOutDelete( p_sys->p_access );
        sout_AccessOutDelete( p_sys->p_grab );
        if( p_sys->packet )
        {
            sout_BufferDelete( p_stream->p_sout, p_sys->packet );
        }
    }

    vlc_mutex_destroy( &p_sys->lock_sdp );

    if( p_sys->p_httpd_file )
    {
        p_sys->p_httpd->pf_unregister_file( p_sys->p_httpd,
                                            p_sys->p_httpd_file );
    }
    if( p_sys->p_httpd_host )
    {
        p_sys->p_httpd->pf_unregister_host( p_sys->p_httpd,
                                            p_sys->p_httpd_host );
    }
    if( p_sys->p_httpd )
    {
        httpd_Release( p_sys->p_httpd );
    }

    if( p_sys->psz_sdp )
    {
        free( p_sys->psz_sdp );
    }
    free( p_sys );
}

/*****************************************************************************
 * SDPGenerate
 *****************************************************************************/
static void SDPGenerate( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_size;
    char *psz_sdp, *p;
    int i;

    i_size = strlen( "v=0\n" ) +
             strlen( "o=- * * IN IP4 127.0.0.1\n" ) +
             strlen( "s=NONE\n" ) +
             strlen( "c=IN IP4 */*\n" ) +
             strlen( p_sys->psz_destination ) +
             20 + 10 + 10 + 1;
    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];

        i_size += strlen( "m=**d*o * RTP/AVP *\n" ) + 10 + 10;

        if( id->psz_rtpmap )
        {
            i_size += strlen( "a=rtpmap:* *\n" ) + strlen( id->psz_rtpmap )+10;
        }
        if( id->psz_fmtp )
        {
            i_size += strlen( "a=fmtp:* *\n" ) + strlen( id->psz_fmtp ) + 10;
        }
    }

    p = psz_sdp = malloc( i_size );
    p += sprintf( p, "v=0\n" );
    p += sprintf( p, "o=- "I64Fd" %d IN IP4 127.0.0.1\n",
                  p_sys->i_sdp_id, p_sys->i_sdp_version );
    p += sprintf( p, "s=NONE\n" );
    p += sprintf( p, "c=IN IP4 %s/%d\n", p_sys->psz_destination,
                  p_sys->i_ttl );

    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];

        if( id->i_cat == AUDIO_ES )
        {
            p += sprintf( p, "m=audio %d RTP/AVP %d\n",
                          id->i_port, id->i_payload_type );
        }
        else if( id->i_cat == VIDEO_ES )
        {
            p += sprintf( p, "m=video %d RTP/AVP %d\n",
                          id->i_port, id->i_payload_type );
        }
        else
        {
            continue;
        }
        if( id->psz_rtpmap )
        {
            p += sprintf( p, "a=rtpmap:%d %s\n", id->i_payload_type,
                          id->psz_rtpmap );
        }
        if( id->psz_fmtp )
        {
            p += sprintf( p, "a=fmtp:%d %s\n", id->i_payload_type,
                          id->psz_fmtp );
        }
    }

    vlc_mutex_lock( &p_sys->lock_sdp );
    free( p_sys->psz_sdp );
    p_sys->psz_sdp = psz_sdp;
    vlc_mutex_unlock( &p_sys->lock_sdp );

    p_sys->i_sdp_version++;

    fprintf( stderr, "sdp=%s", p_sys->psz_sdp );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int rtp_packetize_l16  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_l8   ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_mpa  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_mpv  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_ac3  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_split( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_mp4a ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );

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

    char access[100];
    char url[strlen( p_sys->psz_destination ) + 1 + 12 + 1];

    if( p_sys->p_mux != NULL )
    {
        sout_input_t      *p_input  = NULL;
        if( ( p_input = sout_MuxAddStream( p_sys->p_mux, p_fmt ) ) == NULL )
        {
            msg_Err( p_stream, "cannot add this stream to the muxer" );
            return NULL;
        }

        id = malloc( sizeof( sout_stream_id_t ) );
        id->p_access    = NULL;
        id->p_input     = p_input;
        id->pf_packetize= NULL;

        return id;
    }

    /* first try to create the access out */
    if( p_sys->i_ttl > 0 )
    {
        sprintf( access, "udp{raw,ttl=%d}", p_sys->i_ttl );
    }
    else
    {
        sprintf( access, "udp{raw}" );
    }
    sprintf( url, "%s:%d", p_sys->psz_destination, p_sys->i_port );
    if( ( p_access = sout_AccessOutNew( p_sout, access, url ) ) == NULL )
    {
        msg_Err( p_stream, "cannot create the access out for %s://%s",
                 access, url );
        return NULL;
    }

    /* not create the rtp specific stuff */
    id = malloc( sizeof( sout_stream_id_t ) );
    id->p_access   = p_access;
    id->p_input    = NULL;
    id->psz_rtpmap = NULL;
    id->psz_fmtp   = NULL;
    id->psz_destination = strdup( p_sys->psz_destination );
    id->i_port = p_sys->i_port;

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

        default:
            msg_Err( p_stream, "cannot add this stream (unsupported "
                     "codec:%4.4s)", (char*)&p_fmt->i_codec );
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

    msg_Dbg( p_stream, "access out %s:%s mtu=%d", access, url, id->i_mtu );

    /* Update p_sys context */
    /* update port used (2 -> 1 rtp, 1 rtcp )*/
    TAB_APPEND( p_sys->i_es, p_sys->es, id );
    if( p_sys->p_mux == NULL )
    {
        p_sys->i_port += 2;
        SDPGenerate( p_stream );
    }

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    TAB_REMOVE( p_sys->i_es, p_sys->es, id );

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
        free( id->psz_destination );
        sout_AccessOutDelete( id->p_access );
    }
    else if( id->p_input )
    {
        sout_MuxDeleteStream( p_sys->p_mux, id->p_input );
    }
    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 sout_buffer_t *p_buffer )
{
    sout_buffer_t *p_next;

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
            sout_BufferDelete( p_stream->p_sout, p_buffer );
            p_buffer = p_next;
        }
    }
    return VLC_SUCCESS;
}

static int  HttpCallback( httpd_file_callback_args_t *p_args,
                          uint8_t *p_request, int i_request,
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

static int AccessOutGrabberWriteBuffer( sout_stream_t *p_stream,
                                        sout_buffer_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    int64_t  i_dts = p_buffer->i_dts;
    uint32_t i_timestamp = i_dts * 9 / 100;

    uint8_t         *p_data = p_buffer->p_buffer;
    unsigned int    i_data  = p_buffer->i_size;
    unsigned int    i_max   = p_sys->i_mtu - 12;

    int i_packet = ( p_buffer->i_size + i_max - 1 ) / i_max;

    while( i_data > 0 )
    {
        unsigned int i_size;

        /* output complete packet */
        if( p_sys->packet &&
            p_sys->packet->i_size + i_data > i_max )
        {
            sout_AccessOutWrite( p_sys->p_access, p_sys->packet );
            p_sys->packet = NULL;
        }

        if( p_sys->packet == NULL )
        {
            /* allocate a new packet */
            p_sys->packet = sout_BufferNew( p_stream->p_sout, p_sys->i_mtu );
            p_sys->packet->p_buffer[ 0] = 0x80;
            p_sys->packet->p_buffer[ 1] = p_sys->i_payload_type;
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
            p_sys->packet->i_size = 12;

            p_sys->packet->i_dts = i_dts;
            p_sys->packet->i_length = p_buffer->i_length / i_packet;
            i_dts += p_sys->packet->i_length;

            p_sys->i_sequence++;
        }

        i_size = __MIN( i_data, p_sys->i_mtu - p_sys->packet->i_size );

        memcpy( &p_sys->packet->p_buffer[p_sys->packet->i_size],
                p_data,
                i_size );

        p_sys->packet->i_size += i_size;
        p_data += i_size;
        i_data -= i_size;
    }

    return VLC_SUCCESS;
}

static int AccessOutGrabberWrite( sout_access_out_t *p_access,
                                  sout_buffer_t *p_buffer )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_access->p_sys;

    //fprintf( stderr, "received buffer size=%d\n", p_buffer->i_size );
    //
    while( p_buffer )
    {
        sout_buffer_t *p_next;

        AccessOutGrabberWriteBuffer( p_stream, p_buffer );

        p_next = p_buffer->p_next;
        sout_BufferDelete( p_access->p_sout, p_buffer );
        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}


static void rtp_packetize_common( sout_stream_id_t *id, sout_buffer_t *out,
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

    out->i_size = 12;
    id->i_sequence++;
}

static int rtp_packetize_mpa( sout_stream_t *p_stream, sout_stream_id_t *id,
                              sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12 - 4; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* mbz set to 0 */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 0;
        /* fragment offset in the current frame */
        out->p_buffer[14] = ( (i*i_max) >> 8 )&0xff;
        out->p_buffer[15] = ( (i*i_max)      )&0xff;
        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_size   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

/* rfc2250 */
static int rtp_packetize_mpv( sout_stream_t *p_stream, sout_stream_id_t *id,
                              sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12 - 4; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i;
    int     b_sequence_start = 0;
    int     i_temporal_ref = 0;
    int     i_picture_coding_type = 0;
    int     i_fbv = 0, i_bfc = 0, i_ffv = 0, i_ffc = 0;
    int     b_start_slice = 0;

    /* preparse this packet to get some info */
    if( in->i_size > 4 )
    {
        uint8_t *p = p_data;
        int      i_rest = in->i_size;

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
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout,
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

        out->i_size   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}
static int rtp_packetize_ac3( sout_stream_t *p_stream, sout_stream_id_t *id,
                              sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12 - 2; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 14 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, (i == i_count - 1)?1:0, in->i_pts );
        /* unit count */
        out->p_buffer[12] = 1;
        /* unit header */
        out->p_buffer[13] = 0x00;
        /* data */
        memcpy( &out->p_buffer[14], p_data, i_payload );

        out->i_size   = 14 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_split( sout_stream_t *p_stream, sout_stream_id_t *id,
                                sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_size   = 12 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_l16( sout_stream_t *p_stream, sout_stream_id_t *id,
                              sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i_packet = 0;

    while( i_data > 0 )
    {
        int           i_payload = (__MIN( i_max, i_data )/4)*4;
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, 0,
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_size   = 12 + i_payload;
        out->i_dts    = in->i_dts + i_packet * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
        i_packet++;
    }

    return VLC_SUCCESS;
}

static int rtp_packetize_l8( sout_stream_t *p_stream, sout_stream_id_t *id,
                             sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 12; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i_packet = 0;

    while( i_data > 0 )
    {
        int           i_payload = (__MIN( i_max, i_data )/2)*2;
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 12 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, 0,
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        memcpy( &out->p_buffer[12], p_data, i_payload );

        out->i_size   = 12 + i_payload;
        out->i_dts    = in->i_dts + i_packet * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
        i_packet++;
    }

    return VLC_SUCCESS;
}
static int rtp_packetize_mp4a( sout_stream_t *p_stream, sout_stream_id_t *id,
                               sout_buffer_t *in )
{
    int     i_max   = id->i_mtu - 16; /* payload max in one packet */
    int     i_count = ( in->i_size + i_max - 1 ) / i_max;

    uint8_t *p_data = in->p_buffer;
    int     i_data  = in->i_size;
    int     i;

    for( i = 0; i < i_count; i++ )
    {
        int           i_payload = __MIN( i_max, i_data );
        sout_buffer_t *out = sout_BufferNew( p_stream->p_sout, 16 + i_payload );

        /* rtp common header */
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0),
                              (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* AU headers */
        /* AU headers length (bits) */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 2*8;
        /* for each AU length 13 bits + idx 3bits, */
        out->p_buffer[14] = ( in->i_size >> 5 )&0xff;
        out->p_buffer[15] = ( (in->i_size&0xff)<<3 )|0;

        memcpy( &out->p_buffer[16], p_data, i_payload );

        out->i_size   = 16 + i_payload;
        out->i_dts    = in->i_dts + i * in->i_length / i_count;
        out->i_length = in->i_length / i_count;

        sout_AccessOutWrite( id->p_access, out );

        p_data += i_payload;
        i_data -= i_payload;
    }

    return VLC_SUCCESS;
}
