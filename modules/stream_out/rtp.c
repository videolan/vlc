/*****************************************************************************
 * rtp.c
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: rtp.c,v 1.2 2003/11/01 00:15:49 fenrir Exp $
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("RTP stream") );
    set_capability( "sout stream", 0 );
    add_shortcut( "rtp" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );


struct sout_stream_sys_t
{
    /* sdp */
    int64_t i_sdp_id;
    int     i_sdp_version;

    /* */
    char *psz_destination;
    int  i_port;
    int  i_ttl;

    /* when need to use a private one */
    int i_payload_type;

    /* in case we do TS/PS over rtp */
    sout_mux_t   *p_mux;

    /* */
    int              i_es;
    sout_stream_id_t **es;
    char             *psz_sdp;
};

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
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
    sout_buffer_t *packet;

    /* for sending the packets */
    sout_access_out_t *p_access;
};


/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys;

    char *val;


    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->psz_destination = sout_cfg_find_value( p_stream->p_cfg, "dst" );
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "port" ) ) )
    {
        p_sys->i_port = atoi( val );
    }
    if( !p_sys->psz_destination || *p_sys->psz_destination == '\0' || p_sys->i_port <= 0 )
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
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "mux" ) ) )
    {
        /* p_sout->i_preheader = __MAX( p_sout->i_preheader, p_mux->i_preheader ); */
        msg_Err( p_stream, "muxer not yet supported" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_mux  = NULL;
    p_sys->i_payload_type = 96;
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->i_sdp_id = mdate();
    p_sys->i_sdp_version = 1;


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
    p += sprintf( p, "o=- "I64Fd" %d IN IP4 127.0.0.1\n", p_sys->i_sdp_id, p_sys->i_sdp_version );
    p += sprintf( p, "s=NONE\n" );
    p += sprintf( p, "c=IN IP4 %s/%d\n", p_sys->psz_destination, p_sys->i_ttl );

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
            p += sprintf( p, "a=rtpmap:%d %s\n", id->i_payload_type, id->psz_rtpmap );
        }
        if( id->psz_fmtp )
        {
            p += sprintf( p, "a=fmtp:%d %s\n", id->i_payload_type, id->psz_fmtp );
        }
    }

    p = p_sys->psz_sdp;
    p_sys->psz_sdp = psz_sdp;   /* I hope this avoid a lock (when we export it by http) */

    free( p );

    fprintf( stderr, "sdp=%s", p_sys->psz_sdp );
}

/*****************************************************************************
 *
 *****************************************************************************/

static int rtp_packetize_mpa  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_ac3  ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_split( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );
static int rtp_packetize_mp4a ( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );

static void sprintf_hexa( char *s, uint8_t *p_data, int i_data )
{
    static const char hex[16] = "0123456789ABCDEF";
    int i;

    for( i = 0; i < i_data; i++ )
    {
        s[2*i+0] = hex[(p_data[i]>>8)&0xf];
        s[2*i+1] = hex[(p_data[i]   )&0xf];
    }
    s[2*i_data] = '\0';
}

static sout_stream_id_t * Add      ( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_instance_t   *p_sout = p_stream->p_sout;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;

    sout_access_out_t *p_access;
    char access[100];
    char url[strlen( p_sys->psz_destination ) + 1 + 12 + 1];

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
        msg_Err( p_stream, "cannot create the access out for %s:%s",
                 access, url );
        return NULL;
    }

    /* not create the rtp specific stuff */
    id = malloc( sizeof( sout_stream_id_t ) );
    id->p_access   = p_access;
    id->psz_rtpmap = NULL;
    id->psz_fmtp   = NULL;
    id->psz_destination = strdup( p_sys->psz_destination );
    id->i_port = p_sys->i_port;

    switch( p_fmt->i_fourcc )
    {
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            id->i_payload_type = 14;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "MPA/90000" );
            id->pf_packetize = rtp_packetize_mpa;
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "ac3/90000" );
            id->pf_packetize = rtp_packetize_ac3;
            break;
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        {
            char hexa[2*p_fmt->i_extra_data +1];

            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = 90000;
            id->psz_rtpmap = strdup( "MP4V-ES/90000" );
            id->pf_packetize = rtp_packetize_split;
            if( p_fmt->i_extra_data > 0 )
            {
                id->psz_fmtp = malloc( 100 + 2 * p_fmt->i_extra_data );
                sprintf_hexa( hexa, p_fmt->p_extra_data, p_fmt->i_extra_data );
                sprintf( id->psz_fmtp,
                         "profile-level-id=3; config=%s", hexa );
            }
            break;
        }
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        {
            char hexa[2*p_fmt->i_extra_data +1];

            id->i_payload_type = p_sys->i_payload_type++;
            id->i_clock_rate = p_fmt->i_sample_rate;
            id->psz_rtpmap = malloc( strlen( "mpeg4-generic/" ) + 12 );
            sprintf( id->psz_rtpmap, "mpeg4-generic/%d", p_fmt->i_sample_rate );
            id->pf_packetize = rtp_packetize_mp4a;
            id->psz_fmtp = malloc( 200 + 2 * p_fmt->i_extra_data );
            sprintf_hexa( hexa, p_fmt->p_extra_data, p_fmt->i_extra_data );
            sprintf( id->psz_fmtp,
                     "streamtype=5; profile-level-id=15; mode=AAC-hbr; config=%s; "
                     "SizeLength=13; IndexLength=3; IndexDeltaLength=3; Profile=1;", hexa );
            break;
        }

        default:
            msg_Err( p_stream, "cannot add this stream (unsupported codec:%4.4s)", (char*)&p_fmt->i_fourcc );
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
    id->packet   = NULL;

    msg_Dbg( p_stream, "access out %s:%s mtu=%d", access, url, id->i_mtu );

    /* update port used (2 -> 1 rtp, 1 rtcp )*/
    p_sys->i_port += 2;
    TAB_APPEND( p_sys->i_es, p_sys->es, id );

    SDPGenerate( p_stream );
    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    TAB_REMOVE( p_sys->i_es, p_sys->es, id );

    if( id->packet )
    {
        sout_BufferDelete( p_stream->p_sout, id->packet );
    }
    if( id->psz_rtpmap )
    {
        free( id->psz_rtpmap );
    }
    if( id->psz_fmtp )
    {
        free( id->psz_fmtp );
    }
    sout_AccessOutDelete( id->p_access );
    free( id );
    return VLC_SUCCESS;
}

static int     Send     ( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *p_buffer )
{
    sout_buffer_t *p_next;

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
    return VLC_SUCCESS;
}

static void rtp_packetize_common( sout_stream_id_t *id, sout_buffer_t *out, int b_marker, int64_t i_pts )
{
    uint32_t i_timestamp = i_pts * (int64_t)id->i_clock_rate / (int64_t)1000000;

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

static int rtp_packetize_mpa( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *in )
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

static int rtp_packetize_ac3( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *in )
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

static int rtp_packetize_split( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *in )
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
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0), (in->i_pts > 0 ? in->i_pts : in->i_dts) );
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

static int rtp_packetize_mp4a( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *in )
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
        rtp_packetize_common( id, out, ((i == i_count - 1)?1:0), (in->i_pts > 0 ? in->i_pts : in->i_dts) );
        /* AU headers */
        /* AU headers length (bits) */
        out->p_buffer[12] = 0;
        out->p_buffer[13] = 2*8;
        /* for each AU length 13 bits + idx 3bits, */
        out->p_buffer[14] = ( in->i_size >> 5 )&&0xff;
        out->p_buffer[15] = ( (in->i_size&&0xff)<<3 )|0;

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

