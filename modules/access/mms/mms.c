/*****************************************************************************
 * mms.c: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mms.c,v 1.16 2002/12/31 01:54:35 massiot Exp $
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


/*
 * TODO:
 *  - clean code, break huge code block
 *  - fix memory leak
 *  - begin udp support...
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   if HAVE_ARPA_INET_H
#      include <arpa/inet.h>
#   elif defined( SYS_BEOS )
#      include <net/netdb.h>
#   endif
#endif

#include "network.h"
#include "asf.h"
#include "buffer.h"
#include "mms.h"

/****************************************************************************
 * NOTES:
 *  MMSProtocole documentation found at http://get.to/sdp
 ****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

static int  Read        ( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len );
static void Seek        ( input_thread_t *, off_t );
static int  SetProgram  ( input_thread_t *, pgrm_descriptor_t * );


static int MMSOpen( input_thread_t  *, url_t *, int, char * );

static int MMSStart  ( input_thread_t  *, uint32_t );
static int MMSStop  ( input_thread_t  *p_input );

static int MMSClose  ( input_thread_t  * );


static int  mms_CommandRead( input_thread_t *p_input, int i_command1, int i_command2 );
static int  mms_CommandSend( input_thread_t *, int, uint32_t, uint32_t, uint8_t *, int );

static int  mms_HeaderMediaRead( input_thread_t *, int );

static int  mms_ReceivePacket( input_thread_t * );

static void mms_ParseURL( url_t *p_url, char *psz_url );



/*
 * XXX DON'T FREE MY MEMORY !!! XXX
 * non mais :P
 */

/*
 * Ok, ok, j'le ferai plus...
 */
/*
 * Merci :))
 */
/*
 * Vous pourriez signer vos commentaires (même si on voit bien qui peut
 * écrire ce genre de trucs :p), et écrire en anglais, bordel de
 * merde :p.
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for mms streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("MMS access module") );
    set_capability( "access", 0 );
    add_category_hint( "stream", NULL );
        add_integer( "mms-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                     CACHING_TEXT, CACHING_LONGTEXT );

        add_bool( "mms-all", 0, NULL,
                  "force selection of all streams",
                  "force selection of all streams" );

        add_string( "mms-stream", NULL, NULL,
                    "streams selection",
                    "force this stream selection" );
        add_integer( "mms-maxbitrate", 0, NULL,
                     "max bitrate",
                     "set max bitrate for auto streams selections" );
    add_shortcut( "mms" );
    add_shortcut( "mmsu" );
    add_shortcut( "mmst" );
    set_callbacks( Open, Close );
vlc_module_end();

#define BUF_SIZE 200000

static int Open( vlc_object_t *p_this )
{
    access_t    *p_access;
    int         i_proto;
    char        *psz_network;
    int         i_status;

    input_thread_t  *p_input = (input_thread_t*)p_this;

    /* *** allocate p_access_data *** */
    p_input->p_access_data =
        (void*)p_access = malloc( sizeof( access_t ) );
    memset( p_access, 0, sizeof( access_t ) );


    /* *** Parse URL and get server addr/port and path *** */
    mms_ParseURL( &p_access->url, p_input->psz_name );
    if( p_access->url.psz_server_addr == NULL ||
        !( *p_access->url.psz_server_addr ) )
    {
        FREE( p_access->url.psz_private );
        msg_Err( p_input, "invalid server name" );
        return( -1 );
    }
    if( p_access->url.i_server_port == 0 )
    {
        p_access->url.i_server_port = 1755; /* default port */
    }
    if( p_access->url.i_bind_port == 0 )
    {
        p_access->url.i_bind_port = 7000;   /* default port */
    }


    /* *** connect to this server *** */
    /* 1: look at  requested protocol (udp/tcp) */
    i_proto = MMS_PROTO_AUTO;
    if( *p_input->psz_access )
    {
        if( !strncmp( p_input->psz_access, "mmsu", 4 ) )
        {
            i_proto = MMS_PROTO_UDP;
        }
        else if( !strncmp( p_input->psz_access, "mmst", 4 ) )
        {
            i_proto = MMS_PROTO_TCP;
        }
    }
    /* 2: look at ip version ipv4/ipv6 */
    psz_network = "";
    if( config_GetInt( p_input, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    else if( config_GetInt( p_input, "ipv6" ) )
    {
        psz_network = "ipv6";
    }
    /* 3: connect */
    if( i_proto == MMS_PROTO_AUTO )
    {   /* first try with TCP */
        i_status =
            MMSOpen( p_input, &p_access->url, MMS_PROTO_TCP, psz_network );
        if( i_status < 0 )
        {   /* then with UDP */
            i_status =
             MMSOpen( p_input, &p_access->url, MMS_PROTO_UDP, psz_network );
        }
    }
    else
    {

        i_status =
            MMSOpen( p_input, &p_access->url, i_proto, psz_network );
    }

    if( i_status < 0 )
    {
        msg_Err( p_input, "cannot connect to server" );
        FREE( p_access->url.psz_private );
        return( -1 );
    }
    msg_Dbg( p_input, "connected to %s", p_access->url.psz_server_addr );


    /* *** set exported functions *** */
    p_input->pf_read = Read;
    p_input->pf_seek = Seek;
    p_input->pf_set_program = SetProgram;
    p_input->pf_set_area = NULL;

    p_input->p_private = NULL;

    /* *** finished to set some variable *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    if( p_access->i_proto == MMS_PROTO_UDP )
    {
        p_input->stream.b_connected = 0;
    }
    else
    {
        p_input->stream.b_connected = 1;
    }
    p_input->stream.p_selected_area->i_tell = 0;
    if( p_access->i_packet_count <= 0 )
    {
        p_input->stream.b_seekable = 0;
        p_input->stream.p_selected_area->i_size = 0;
    }
    else
    {
        p_input->stream.b_seekable = 1;
        p_input->stream.p_selected_area->i_size =
            p_access->i_header +
            p_access->i_packet_count * p_access->i_packet_length;
    }

    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* *** Start stream *** */
    if( MMSStart( p_input, 0xffffffff ) < 0 )
    {
        msg_Err( p_input, "cannot start stream" );
        MMSClose( p_input );
        FREE( p_access->url.psz_private );
        return( -1 );
    }

    /* Update default_pts to a suitable value for mms access */
    p_input->i_pts_delay = config_GetInt( p_input, "mms-caching" ) * 1000;

    return( 0 );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    access_t    *p_access = (access_t*)p_input->p_access_data;

    /* close connection with server */
    MMSClose( p_input );

    /* free memory */
    FREE( p_access->url.psz_private );
}

/*****************************************************************************
 * SetProgram: do nothing
 *****************************************************************************/
static int SetProgram( input_thread_t * p_input,
                       pgrm_descriptor_t * p_program )
{
    return( 0 );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    /*
     * FIXME
     * Don't work
     * Probably some bad or missing command
     *
     *
     */
#if 1

    access_t    *p_access = (access_t*)p_input->p_access_data;
    uint32_t    i_packet;
    uint32_t    i_offset;
    var_buffer_t    buffer;

    if( i_pos < 0 )
    {
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( i_pos < p_access->i_header)
    {

        if( p_access->i_pos < p_access->i_header )
        {
            /* no need to restart stream, it was already one
             * or no stream was yet read */
            p_access->i_pos = i_pos;
            return;
        }
        else
        {
            i_packet = 0xffffffff;
            i_offset = 0;
        }
    }
    else
    {
        i_packet = ( i_pos - p_access->i_header ) / p_access->i_packet_length;
        i_offset = ( i_pos - p_access->i_header ) % p_access->i_packet_length;
    }
    msg_Dbg( p_input, "seeking to "I64Fd " (packet:%d)", i_pos, i_packet );

    MMSStop( p_input );
    msg_Dbg( p_input, "stream stopped (seek)" );

    /* *** restart stream *** */
    var_buffer_initwrite( &buffer, 0 );
    var_buffer_add64( &buffer, 0 ); /* seek point in second */
    var_buffer_add32( &buffer, 0xffffffff );
    var_buffer_add32( &buffer, i_packet ); // begin from start
    var_buffer_add8( &buffer, 0xff ); // stream time limit
    var_buffer_add8( &buffer, 0xff ); //  on 3bytes ...
    var_buffer_add8( &buffer, 0xff ); //
    var_buffer_add8( &buffer, 0x00 ); // don't use limit
    var_buffer_add32( &buffer, p_access->i_media_packet_id_type );

    mms_CommandSend( p_input, 0x07, p_access->i_command_level, 0x0001ffff,
                     buffer.p_data, buffer.i_data );

    var_buffer_free( &buffer );


    for( ;; )
    {
        mms_HeaderMediaRead( p_input, MMS_PACKET_CMD );
        if( p_access->i_command == 0x1e )
        {
            msg_Dbg( p_input, "received 0x1e (seek)" );
            break;
        }
    }

    for( ;; )
    {
        mms_HeaderMediaRead( p_input, MMS_PACKET_CMD );
        if( p_access->i_command == 0x05 )
        {
            msg_Dbg( p_input, "received 0x05 (seek)" );
            break;
        }
    }

    /* get a packet */
    mms_HeaderMediaRead( p_input, MMS_PACKET_MEDIA );
    msg_Dbg( p_input, "Streaming restarted" );

    p_access->i_media_used += i_offset;
    p_access->i_pos = i_pos;
    p_input->stream.p_selected_area->i_tell = i_pos;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

#endif
}

static int  Read        ( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    size_t      i_data;
    size_t      i_copy;

    i_data = 0;

    /* *** send header if needed ** */
    if( p_access->i_pos < p_access->i_header )
    {
        i_copy = __MIN( i_len, p_access->i_header - p_access->i_pos );
        if( i_copy > 0 )
        {
            memcpy( p_buffer,
                    p_access->p_header + p_access->i_pos,
                    i_copy );
        }
        i_data += i_copy;
    }

    /* *** now send data if needed *** */
    while( i_data < i_len )
    {
        if( p_access->i_media_used < p_access->i_media )
        {
            i_copy = __MIN( i_len - i_data ,
                            p_access->i_media - p_access->i_media_used );
            memcpy( p_buffer + i_data,
                    p_access->p_media + p_access->i_media_used,
                    i_copy );
            i_data += i_copy;
            p_access->i_media_used += i_copy;
        }
        else if( p_access->p_media != NULL &&
                 p_access->i_media_used < p_access->i_packet_length )
        {
            i_copy = __MIN( i_len - i_data,
                            p_access->i_packet_length - p_access->i_media_used);
            memset( p_buffer + i_data, 0, i_copy );

            i_data += i_copy;
            p_access->i_media_used += i_copy;
        }
        else
        {
            if( p_access->i_eos
                 || mms_HeaderMediaRead( p_input, MMS_PACKET_MEDIA ) < 0 )
            {
                p_access->i_pos += i_data;
                return( i_data );
            }
        }
    }

    p_access->i_pos += i_data;
    return( i_data );
}

static void asf_HeaderParse( mms_stream_t stream[128],
                             uint8_t *p_header, int i_header )
{
    var_buffer_t buffer;
    guid_t      guid;
    uint64_t    i_size;
    int         i;

    for( i = 0; i < 128; i++ )
    {
        stream[i].i_cat = MMS_STREAM_UNKNOWN;
    }

//    fprintf( stderr, " ---------------------header:%d\n", i_header );
    var_buffer_initread( &buffer, p_header, i_header );

    var_buffer_getguid( &buffer, &guid );

    if( !CmpGuid( &guid, &asf_object_header_guid ) )
    {
//        XXX Error
//        fprintf( stderr, " ---------------------ERROR------\n" );
    }
    var_buffer_getmemory( &buffer, NULL, 30 - 16 );

    for( ;; )
    {
//    fprintf( stderr, " ---------------------data:%d\n", buffer.i_data );
        if( var_buffer_readempty( &buffer ) )
        {
            return;
        }

        var_buffer_getguid( &buffer, &guid );
        i_size = var_buffer_get64( &buffer );
        if( CmpGuid( &guid, &asf_object_stream_properties_guid ) )
        {
            int     i_stream_id;
            guid_t  stream_type;
//            msg_Dbg( p_input, "found stream_properties" );

            var_buffer_getguid( &buffer, &stream_type );
            var_buffer_getmemory( &buffer, NULL, 32 );
            i_stream_id = var_buffer_get8( &buffer ) & 0x7f;

 //           fprintf( stderr, " 1---------------------skip:%lld\n", i_size - 24 - 32 - 16 - 1 );
            var_buffer_getmemory( &buffer, NULL, i_size - 24 - 32 - 16 - 1);

            if( CmpGuid( &stream_type, &asf_object_stream_type_video ) )
            {
//                msg_Dbg( p_input, "video stream[%d] found", i_stream_id );
                stream[i_stream_id].i_cat = MMS_STREAM_VIDEO;
            }
            else if( CmpGuid( &stream_type, &asf_object_stream_type_audio ) )
            {
//                msg_Dbg( p_input, "audio stream[%d] found", i_stream_id );
                stream[i_stream_id].i_cat = MMS_STREAM_AUDIO;
            }
            else
            {
//                msg_Dbg( p_input, "unknown stream[%d] found", i_stream_id );
                stream[i_stream_id].i_cat = MMS_STREAM_UNKNOWN;
            }
        }
        else if ( CmpGuid( &guid, &asf_object_bitrate_properties_guid ) )
        {
            int     i_count;
            uint8_t i_stream_id;

            i_count = var_buffer_get16( &buffer );
            i_size -= 2;
            while( i_count > 0 )
            {
                i_stream_id = var_buffer_get16( &buffer )&0x7f;
                stream[i_stream_id].i_bitrate =  var_buffer_get32( &buffer );
                i_count--;
                i_size -= 6;
            }
//            fprintf( stderr, " 2---------------------skip:%lld\n", i_size - 24);
            var_buffer_getmemory( &buffer, NULL, i_size - 24 );
        }
        else
        {
            // skip unknown guid
            var_buffer_getmemory( &buffer, NULL, i_size - 24 );
//            fprintf( stderr, " 3---------------------skip:%lld\n", i_size - 24);
        }
    }
}

static void mms_StreamSelect( input_thread_t * p_input,
                              mms_stream_t stream[128] )
{
    /* XXX FIXME use mututal eclusion information */
    int i;
    int i_audio, i_video;
    int b_audio, b_video;
    int i_bitrate_total;
    int i_bitrate_max;
    char *psz_stream;

    i_audio = 0;
    i_video = 0;
    i_bitrate_total = 0;
    i_bitrate_max = config_GetInt( p_input, "mms-maxbitrate" );
    b_audio = config_GetInt( p_input, "audio" );
    b_video = config_GetInt( p_input, "video" );
    if( config_GetInt( p_input, "mms-all" ) )
    {
        /* select all valid stream */
        for( i = 1; i < 128; i++ )
        {
            if( stream[i].i_cat != MMS_STREAM_UNKNOWN )
            {
                stream[i].i_selected = 1;
            }
        }
        return;
    }
    else
    {
        for( i = 0; i < 128; i++ )
        {
            stream[i].i_selected = 0; /* by default, not selected */
        }
    }
    psz_stream = config_GetPsz( p_input, "mms-stream" );

    if( psz_stream && *psz_stream )
    {
        char *psz_tmp = psz_stream;
        while( *psz_tmp )
        {
            if( *psz_tmp == ',' )
            {
                psz_tmp++;
            }
            else
            {
                int i_stream;
                i_stream = atoi( psz_tmp );
                while( *psz_tmp != '\0' && *psz_tmp != ',' )
                {
                    psz_tmp++;
                }

                if( i_stream > 0 && i_stream < 128 &&
                    stream[i_stream].i_cat != MMS_STREAM_UNKNOWN )
                {
                    stream[i_stream].i_selected = 1;
                }
            }
        }
        FREE( psz_stream );
        return;
    }
    FREE( psz_stream );

    /* big test:
     * select a stream if
     *    - no audio nor video stream
     *    - or:
     *         - if i_bitrate_max not set keep the highest bitrate
     *         - if i_bitrate_max is set, keep stream that make we used best
     *           quality regarding i_bitrate_max
     *
     * XXX: little buggy:
     *        - it doesn't use mutual exclusion info..
     *        - when selecting a better stream we could select
     *        something that make i_bitrate_total> i_bitrate_max
     */
    for( i = 1; i < 128; i++ )
    {
        if( stream[i].i_cat == MMS_STREAM_UNKNOWN )
        {
            continue;
        }
        else if( stream[i].i_cat == MMS_STREAM_AUDIO && b_audio &&
                 ( i_audio <= 0 ||
                    ( ( ( stream[i].i_bitrate > stream[i_audio].i_bitrate &&
                          ( i_bitrate_total + stream[i].i_bitrate - stream[i_audio].i_bitrate
                                            < i_bitrate_max || !i_bitrate_max) ) ||
                        ( stream[i].i_bitrate < stream[i_audio].i_bitrate &&
                              i_bitrate_max != 0 && i_bitrate_total > i_bitrate_max )
                      ) )  ) )
        {
            /* unselect old stream */
            if( i_audio > 0 )
            {
                stream[i_audio].i_selected = 0;
                if( stream[i_audio].i_bitrate> 0 )
                {
                    i_bitrate_total -= stream[i_audio].i_bitrate;
                }
            }

            stream[i].i_selected = 1;
            if( stream[i].i_bitrate> 0 )
            {
                i_bitrate_total += stream[i].i_bitrate;
            }
            i_audio = i;
        }
        else if( stream[i].i_cat == MMS_STREAM_VIDEO && b_video &&
                 ( i_video <= 0 ||
                    (
                        ( ( stream[i].i_bitrate > stream[i_video].i_bitrate &&
                            ( i_bitrate_total + stream[i].i_bitrate - stream[i_video].i_bitrate
                                            < i_bitrate_max || !i_bitrate_max) ) ||
                          ( stream[i].i_bitrate < stream[i_video].i_bitrate &&
                            i_bitrate_max != 0 && i_bitrate_total > i_bitrate_max )
                        ) ) )  )
        {
            /* unselect old stream */

            stream[i_video].i_selected = 0;
            if( stream[i_video].i_bitrate> 0 )
            {
                i_bitrate_total -= stream[i_video].i_bitrate;
            }

            stream[i].i_selected = 1;
            if( stream[i].i_bitrate> 0 )
            {
                i_bitrate_total += stream[i].i_bitrate;
            }
            i_video = i;
        }

    }
    if( i_bitrate_max > 0 )
    {
        msg_Dbg( p_input,
                 "requested bitrate:%d real bitrate:%d",
                 i_bitrate_max, i_bitrate_total );
    }
    else
    {
        msg_Dbg( p_input,
                 "total bitrate:%d",
                 i_bitrate_total );
    }
}

/****************************************************************************
 * MMSOpen : Open a connection with the server over mmst or mmsu(not yet)
 ****************************************************************************/
static int MMSOpen( input_thread_t  *p_input,
                       url_t *p_url,
                       int  i_proto,
                       char *psz_network ) /* "", "ipv4", "ipv6" */
{
    module_t    *p_network;
    access_t    *p_access = (access_t*)p_input->p_access_data;

    network_socket_t    socket_desc;
    int b_udp = ( i_proto == MMS_PROTO_UDP ) ? 1 : 0;

    var_buffer_t buffer;
    char         tmp[4096];
    uint16_t     *p;
    int          i_server_version;
    int          i_tool_version;
    int          i_update_player_url;
    int          i_encryption_type;
    int          i;
    int          i_streams;
    int          i_first;


    /* *** Open a TCP connection with server *** */
    msg_Dbg( p_input, "waiting for connection..." );
    socket_desc.i_type = NETWORK_TCP;
    socket_desc.psz_server_addr = p_url->psz_server_addr;
    socket_desc.i_server_port   = p_url->i_server_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    p_input->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_input, "network", psz_network ) ) )
    {
        msg_Err( p_input, "failed to open a connection (tcp)" );
        return( -1 );
    }
    module_Unneed( p_input, p_network );
    p_access->socket_tcp.i_handle = socket_desc.i_handle;
    p_input->i_mtu    = 0; /*socket_desc.i_mtu;*/
    msg_Dbg( p_input,
             "connection(tcp) with \"%s:%d\" successful",
             p_url->psz_server_addr,
             p_url->i_server_port );

    /* *** Bind port if UDP protocol is selected *** */
    if( b_udp )
    {
        if( !p_url->psz_bind_addr || !*p_url->psz_bind_addr )
        {
            struct sockaddr_in name;
            socklen_t i_namelen = sizeof( struct sockaddr_in );

            if( getsockname( p_access->socket_tcp.i_handle,
                             (struct sockaddr*)&name, &i_namelen ) < 0 )
            {

                msg_Err( p_input, "for udp you have to provide bind address (mms://<server_addr>@<bind_addr/<path> (FIXME)" );
#if defined( UNDER_CE )
                CloseHandle( (HANDLE)p_access->socket_tcp.i_handle );
#elif defined( WIN32 )
                closesocket( p_access->socket_tcp.i_handle );
#else
                close( p_access->socket_tcp.i_handle );
#endif
                return( -1 );
            }
            p_access->psz_bind_addr = inet_ntoa( name.sin_addr );
        }
        else
        {
            p_access->psz_bind_addr = strdup( p_url->psz_bind_addr );
        }
        socket_desc.i_type = NETWORK_UDP;
        socket_desc.psz_server_addr = "";
        socket_desc.i_server_port   = 0;
        socket_desc.psz_bind_addr   = p_access->psz_bind_addr;
        socket_desc.i_bind_port     = p_url->i_bind_port;
        p_input->p_private = (void*)&socket_desc;
        if( !( p_network = module_Need( p_input, "network", psz_network ) ) )
        {
            msg_Err( p_input, "failed to open a connection (udp)" );
#if defined( UNDER_CE )
            CloseHandle( (HANDLE)p_access->socket_tcp.i_handle );
#elif defined( WIN32 )
            closesocket( p_access->socket_tcp.i_handle );
#else
            close( p_access->socket_tcp.i_handle );
#endif
            return( -1 );
        }
        module_Unneed( p_input, p_network );
        p_access->socket_udp.i_handle = socket_desc.i_handle;
        p_input->i_mtu    = 0;/*socket_desc.i_mtu;  FIXME */

    }
    else
    {
        p_access->psz_bind_addr = NULL;
    }

    /* *** Init context for mms prototcol *** */
    GenerateGuid( &p_access->guid );    /* used to identify client by server */
    msg_Dbg( p_input,
             "generated guid: "GUID_FMT,
             GUID_PRINT( p_access->guid ) );
    p_access->i_command_level = 1;          /* updated after 0x1A command */
    p_access->i_seq_num = 0;
    p_access->i_media_packet_id_type  = 0x04;
    p_access->i_header_packet_id_type = 0x02;
    p_access->i_proto = i_proto;
    p_access->i_packet_seq_num = 0;
    p_access->p_header = NULL;
    p_access->i_header = 0;
    p_access->p_media = NULL;
    p_access->i_media = 0;
    p_access->i_media_used = 0;

    p_access->i_pos = 0;
    p_access->i_buffer_tcp = 0;
    p_access->i_buffer_udp = 0;
    p_access->p_cmd = NULL;
    p_access->i_cmd = 0;
    p_access->i_eos = 0;

    /* *** send command 1 : connection request *** */
    var_buffer_initwrite( &buffer, 0 );
    var_buffer_add16( &buffer, 0x001c );
    var_buffer_add16( &buffer, 0x0003 );
    sprintf( tmp,
             "NSPlayer/7.0.0.1956; {"GUID_FMT"}; Host: %s",
             GUID_PRINT( p_access->guid ),
             p_url->psz_server_addr );
    var_buffer_addUTF16( &buffer, tmp );

    mms_CommandSend( p_input,
                     0x01,          /* connexion request */
                     0x00000000,    /* flags, FIXME */
                     0x0004000b,    /* ???? */
                     buffer.p_data,
                     buffer.i_data );

    mms_CommandRead( p_input, 0x01, 0 );
    i_server_version = GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 32 );
    i_tool_version = GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 36 );
    i_update_player_url = GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 40 );
    i_encryption_type = GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 44 );
    p = (uint16_t*)( p_access->p_cmd + MMS_CMD_HEADERSIZE + 48 );
#define GETUTF16( psz, size ) \
    { \
        int i; \
        psz = malloc( size + 1); \
        for( i = 0; i < size; i++ ) \
        { \
            psz[i] = p[i]; \
        } \
        psz[size] = '\0'; \
        p += 2 * ( size ); \
    }
    GETUTF16( p_access->psz_server_version, i_server_version );
    GETUTF16( p_access->psz_tool_version, i_tool_version );
    GETUTF16( p_access->psz_update_player_url, i_update_player_url );
    GETUTF16( p_access->psz_encryption_type, i_encryption_type );
#undef GETUTF16
    msg_Dbg( p_input,
             "0x01 --> server_version:\"%s\" tool_version:\"%s\" update_player_url:\"%s\" encryption_type:\"%s\"",
             p_access->psz_server_version,
             p_access->psz_tool_version,
             p_access->psz_update_player_url,
             p_access->psz_encryption_type );

    /* *** should make an 18 command to make data timing *** */

    /* *** send command 2 : transport protocol selection *** */
    var_buffer_reinitwrite( &buffer, 0 );
    var_buffer_add32( &buffer, 0x00000000 );
    var_buffer_add32( &buffer, 0x000a0000 );
    var_buffer_add32( &buffer, 0x00000002 );
    if( b_udp )
    {
        sprintf( tmp,
                 "\\\\%s\\UDP\\%d",
                 p_access->psz_bind_addr,
                 p_url->i_bind_port );
    }
    else
    {
        sprintf( tmp, "\\\\127.0.0.1\\TCP\\1242"  );
    }
    var_buffer_addUTF16( &buffer, tmp );
    var_buffer_add16( &buffer, '0' );

    mms_CommandSend( p_input,
                     0x02,          /* connexion request */
                     0x00000000,    /* flags, FIXME */
                     0xffffffff,    /* ???? */
                     buffer.p_data,
                     buffer.i_data );

    /* *** response from server, should be 0x02 or 0x03 *** */
    mms_CommandRead( p_input, 0x02, 0x03 );
    if( p_access->i_command == 0x03 )
    {
        msg_Err( p_input,
                 "%s protocol selection failed", b_udp ? "UDP" : "TCP" );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }
    else if( p_access->i_command != 0x02 )
    {
        msg_Warn( p_input, "received command isn't 0x02 in reponse to 0x02" );
    }

    /* *** send command 5 : media file name/path requested *** */
    var_buffer_reinitwrite( &buffer, 0 );
    var_buffer_add64( &buffer, 0 );
    var_buffer_addUTF16( &buffer, p_url->psz_path );

    mms_CommandSend( p_input,
                     0x05,
                     p_access->i_command_level,
                     0xffffffff,
                     buffer.p_data,
                     buffer.i_data );

    /* *** wait for reponse *** */
    mms_CommandRead( p_input, 0x1a, 0x06 );

    /* test if server send 0x1A answer */
    if( p_access->i_command == 0x1A )
    {
        msg_Err( p_input, "id/password requested (not yet supported)" );
        /*  FIXME */
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }
    if( p_access->i_command != 0x06 )
    {
        msg_Err( p_input,
                 "unknown answer (0x%x instead of 0x06)",
                 p_access->i_command );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }

    /*  1 for file ok, 2 for authen ok */
    switch( GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE ) )
    {
        case 0x0001:
            msg_Dbg( p_input, "Media file name/path accepted" );
            break;
        case 0x0002:
            msg_Dbg( p_input, "Authentication accepted" );
            break;
        case -1:
        default:
        msg_Err( p_input, "error while asking for file %d",
                 GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE ) );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }

    p_access->i_flags_broadcast =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 12 );
    p_access->i_media_length =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 24 );
    p_access->i_packet_length =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 44 );
    p_access->i_packet_count =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 48 );
    p_access->i_max_bit_rate =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 56 );
    p_access->i_header_size =
        GetDWLE( p_access->p_cmd + MMS_CMD_HEADERSIZE + 60 );

    msg_Dbg( p_input,
             "answer 0x06 flags:0x%8.8x media_length:%ds packet_length:%d packet_count:%d max_bit_rate:%d header_size:%d",
             p_access->i_flags_broadcast,
             p_access->i_media_length,
             p_access->i_packet_length,
             p_access->i_packet_count,
             p_access->i_max_bit_rate,
             p_access->i_header_size );

    /* XXX XXX dirty hack XXX XXX */
    p_input->i_mtu    = 3 * p_access->i_packet_length;

    /* *** send command 15 *** */

    var_buffer_reinitwrite( &buffer, 0 );
    var_buffer_add32( &buffer, 0 );
    var_buffer_add32( &buffer, 0x8000 );
    var_buffer_add32( &buffer, 0xffffffff );
    var_buffer_add32( &buffer, 0x00 );
    var_buffer_add32( &buffer, 0x00 );
    var_buffer_add32( &buffer, 0x00 );
    var_buffer_add64( &buffer, 0x40ac200000000000 );
    var_buffer_add32( &buffer, p_access->i_header_packet_id_type );
    mms_CommandSend( p_input, 0x15, p_access->i_command_level, 0x00,
                     buffer.p_data, buffer.i_data );

    /* *** wait for reponse *** */
    mms_CommandRead( p_input, 0x11, 0 );

    if( p_access->i_command != 0x11 )
    {
        msg_Err( p_input,
                 "unknown answer (0x%x instead of 0x11)",
                 p_access->i_command );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }
    /* *** now read header packet *** */
    if( mms_HeaderMediaRead( p_input, MMS_PACKET_HEADER ) < 0 )
    {
        msg_Err( p_input, "cannot receive header" );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }
    /* *** parse header and get stream and their id *** */
    /* get all streams properties,
     *
     * TODO : stream bitrates properties(optional)
     *        and bitrate mutual exclusion(optional) */
    asf_HeaderParse( p_access->stream,
                     p_access->p_header, p_access->i_header );
    mms_StreamSelect( p_input, p_access->stream );
    /* *** now select stream we want to receive *** */
    /* TODO take care of stream bitrate TODO */
    i_streams = 0;
    i_first = -1;
    var_buffer_reinitwrite( &buffer, 0 );
    /* for now, select first audio and video stream */
    for( i = 1; i < 128; i++ )
    {

        if( p_access->stream[i].i_cat != MMS_STREAM_UNKNOWN )
        {
            i_streams++;
            if( i_first != -1 )
            {
                var_buffer_add16( &buffer, 0xffff );
                var_buffer_add16( &buffer, i );
            }
            else
            {
                i_first = i;
            }
            if( p_access->stream[i].i_selected )
            {
                var_buffer_add16( &buffer, 0x0000 );
                msg_Info( p_input,
                          "selecting stream[0x%x] %s (%d kb/s)",
                          i,
                          ( p_access->stream[i].i_cat == MMS_STREAM_AUDIO  ) ?
                                                  "audio" : "video" ,
                          p_access->stream[i].i_bitrate / 1024);
            }
            else
            {
                var_buffer_add16( &buffer, 0x0002 );
                msg_Info( p_input,
                          "ignoring stream[0x%x] %s (%d kb/s)",
                          i,
                          ( p_access->stream[i].i_cat == MMS_STREAM_AUDIO  ) ?
                                    "audio" : "video" ,
                          p_access->stream[i].i_bitrate / 1024);

            }
        }
    }

    if( i_streams == 0 )
    {
        msg_Err( p_input, "cannot find any stream" );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }
    mms_CommandSend( p_input, 0x33,
                     i_streams,
                     0xffff | ( i_first << 16 ),
                     buffer.p_data, buffer.i_data );

    mms_CommandRead( p_input, 0x21, 0 );
    if( p_access->i_command != 0x21 )
    {
        msg_Err( p_input,
                 "unknown answer (0x%x instead of 0x21)",
                 p_access->i_command );
        var_buffer_free( &buffer );
        MMSClose( p_input );
        return( -1 );
    }


    var_buffer_free( &buffer );

    msg_Info( p_input, "connection sucessful" );

    return( 0 );
}

/****************************************************************************
 * MMSStart : Start streaming
 ****************************************************************************/
static int MMSStart  ( input_thread_t  *p_input, uint32_t i_packet )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;
    var_buffer_t    buffer;

    /* *** start stream from packet 0 *** */
    var_buffer_initwrite( &buffer, 0 );
    var_buffer_add64( &buffer, 0 ); /* seek point in second */
    var_buffer_add32( &buffer, 0xffffffff );
    var_buffer_add32( &buffer, i_packet ); // begin from start
    var_buffer_add8( &buffer, 0xff ); // stream time limit
    var_buffer_add8( &buffer, 0xff ); //  on 3bytes ...
    var_buffer_add8( &buffer, 0xff ); //
    var_buffer_add8( &buffer, 0x00 ); // don't use limit
    var_buffer_add32( &buffer, p_access->i_media_packet_id_type );

    mms_CommandSend( p_input, 0x07, p_access->i_command_level, 0x0001ffff,
                     buffer.p_data, buffer.i_data );

    var_buffer_free( &buffer );

    mms_CommandRead( p_input, 0x05, 0 );

    if( p_access->i_command != 0x05 )
    {
        msg_Err( p_input,
                 "unknown answer (0x%x instead of 0x05)",
                 p_access->i_command );
        return( -1 );
    }
    else
    {
        /* get a packet */
        mms_HeaderMediaRead( p_input, MMS_PACKET_MEDIA );
        msg_Dbg( p_input, "Streaming started" );
        return( 0 );
    }
}

/****************************************************************************
 * MMSStop : Stop streaming
 ****************************************************************************/
static int MMSStop  ( input_thread_t  *p_input )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;

    /* *** stop stream but keep connection alive *** */
    mms_CommandSend( p_input,
                     0x09,
                     p_access->i_command_level,
                     0x001fffff,
                     NULL, 0 );
    return( 0 );
}

/****************************************************************************
 * MMSClose : Close streaming and connection
 ****************************************************************************/
static int MMSClose  ( input_thread_t  *p_input )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;

    msg_Dbg( p_input, "Connection closed" );

    /* *** tell server that we will disconnect *** */
    mms_CommandSend( p_input,
                     0x0d,
                     p_access->i_command_level,
                     0x00000001,
                     NULL, 0 );
    /* *** close sockets *** */
#if defined( UNDER_CE )
    CloseHandle( (HANDLE)p_access->socket_tcp.i_handle );
#elif defined( WIN32 )
    closesocket( p_access->socket_tcp.i_handle );
#else
    close( p_access->socket_tcp.i_handle );
#endif

    if( p_access->i_proto == MMS_PROTO_UDP )
    {
#if defined( UNDER_CE )
        CloseHandle( (HANDLE)p_access->socket_udp.i_handle );
#elif defined( WIN32 )
        closesocket( p_access->socket_udp.i_handle );
#else
        close( p_access->socket_udp.i_handle );
#endif
    }

    FREE( p_access->p_cmd );
    FREE( p_access->p_media );
    FREE( p_access->p_header );

    FREE( p_access->psz_server_version );
    FREE( p_access->psz_tool_version );
    FREE( p_access->psz_update_player_url );
    FREE( p_access->psz_encryption_type );

    return( 0 );
}

/*****************************************************************************
 * mms_ParseURL : parse an url string and fill an url_t
 *****************************************************************************/
static void mms_ParseURL( url_t *p_url, char *psz_url )
{
    char *psz_parser;
    char *psz_server_port;

    p_url->psz_private = strdup( psz_url );

    psz_parser = p_url->psz_private;

    while( *psz_parser == '/' )
    {
        psz_parser++;
    }
    p_url->psz_server_addr = psz_parser;

    while( *psz_parser &&
           *psz_parser != ':' &&  *psz_parser != '/' && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_server_port = psz_parser;

        while( *psz_parser && *psz_parser != '/' )
        {
            psz_parser++;
        }
    }
    else
    {
        psz_server_port = "";
    }

    if( *psz_parser == '@' )
    {
        char *psz_bind_port;

        *psz_parser = '\0';
        psz_parser++;

        p_url->psz_bind_addr = psz_parser;

        while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
        {
            psz_parser++;
        }

        if( *psz_parser == ':' )
        {
            *psz_parser = '\0';
            psz_parser++;
            psz_bind_port = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                psz_parser++;
            }
        }
        else
        {
            psz_bind_port = "";
        }
        if( *psz_bind_port )
        {
            p_url->i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        }
        else
        {
            p_url->i_bind_port = 0;
        }
    }
    else
    {
        p_url->psz_bind_addr = "";
        p_url->i_bind_port = 0;
    }

    if( *psz_parser == '/' )
    {
        *psz_parser = '\0';
        psz_parser++;
        p_url->psz_path = psz_parser;
    }

    if( *psz_server_port )
    {
        p_url->i_server_port = strtol( psz_server_port, &psz_parser, 10 );
    }
    else
    {
        p_url->i_server_port = 0;
    }
}

/****************************************************************************
 *
 * MMS specific functions
 *
 ****************************************************************************/

static int mms_CommandSend( input_thread_t *p_input,
                             int i_command,
                             uint32_t i_prefix1, uint32_t i_prefix2,
                             uint8_t *p_data, int i_data )
{
    var_buffer_t buffer;

    access_t    *p_access = (access_t*)p_input->p_access_data;
    int i_data_by8;

    i_data_by8 = ( i_data + 7 ) / 8;

    /* first init uffer */
    var_buffer_initwrite( &buffer, 0 );

    var_buffer_add32( &buffer, 0x00000001 );    /* start sequence */
    var_buffer_add32( &buffer, 0xB00BFACE );
    /* size after protocol type */
    var_buffer_add32( &buffer, i_data + MMS_CMD_HEADERSIZE - 16 );
    var_buffer_add32( &buffer, 0x20534d4d );    /* protocol "MMS " */
    var_buffer_add32( &buffer, i_data_by8 + 4 );
    var_buffer_add32( &buffer, p_access->i_seq_num ); p_access->i_seq_num++;
    var_buffer_add64( &buffer, 0 );
    var_buffer_add32( &buffer, i_data_by8 + 2 );
    var_buffer_add32( &buffer, 0x00030000 | i_command ); /* dir | command */
    var_buffer_add32( &buffer, i_prefix1 );    /* command specific */
    var_buffer_add32( &buffer, i_prefix2 );    /* command specific */

    /* specific command data */
    if( p_data && i_data > 0 )
    {
        var_buffer_addmemory( &buffer, p_data, i_data );
    }

    /* send it */
    if( send( p_access->socket_tcp.i_handle,
              buffer.p_data,
              buffer.i_data,
              0 ) == -1 )
    {
        msg_Err( p_input, "failed to send command" );
        return( -1 );
    }

    var_buffer_free( &buffer );
    return( 0 );
}

static int  NetFillBuffer( input_thread_t *p_input )
{
#ifdef UNDER_CE
    return -1;
#else
    access_t    *p_access = (access_t*)p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;

    /* FIXME when using udp */
    ssize_t i_tcp, i_udp;
    ssize_t i_tcp_read, i_udp_read;
    int i_handle_max;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );

    i_tcp = MMS_BUFFER_SIZE/2 - p_access->i_buffer_tcp;

    if( p_access->i_proto == MMS_PROTO_UDP )
    {
        i_udp = MMS_BUFFER_SIZE/2 - p_access->i_buffer_udp;
    }
    else
    {
        i_udp = 0;  /* there isn't udp socket */
    }

    i_handle_max = 0;
    if( i_tcp > 0 )
    {
        FD_SET( p_access->socket_tcp.i_handle, &fds );
        i_handle_max = __MAX( i_handle_max, p_access->socket_tcp.i_handle );
    }
    if( i_udp > 0 )
    {
        FD_SET( p_access->socket_udp.i_handle, &fds );
        i_handle_max = __MAX( i_handle_max, p_access->socket_udp.i_handle );
    }

    if( i_handle_max == 0 )
    {
        msg_Warn( p_input, "nothing to read %d:%d", i_tcp, i_udp );
        return( 0 );
    }
    else
    {
//        msg_Warn( p_input, "ask for tcp:%d udp:%d", i_tcp, i_udp );
    }

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    i_ret = select( i_handle_max + 1,
                    &fds,
                    NULL, NULL, &timeout );

    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
        return -1;
    }

    if( i_tcp > 0 && FD_ISSET( p_access->socket_tcp.i_handle, &fds ) )
    {
        i_tcp_read =
            recv( p_access->socket_tcp.i_handle,
                  p_access->buffer_tcp + p_access->i_buffer_tcp,
                  i_tcp + MMS_BUFFER_SIZE/2, 0 );
    }
    else
    {
        i_tcp_read = 0;
    }

    if( i_udp > 0 && FD_ISSET( p_access->socket_udp.i_handle, &fds ) )
    {
        i_udp_read = recv( p_access->socket_udp.i_handle,
                           p_access->buffer_udp + p_access->i_buffer_udp,
                           i_udp + MMS_BUFFER_SIZE/2, 0 );
    }
    else
    {
        i_udp_read = 0;
    }

#if 1
    if( p_access->i_proto == MMS_PROTO_UDP )
    {
        msg_Dbg( p_input,
                 "filling buffer TCP:%d+%d UDP:%d+%d",
                 p_access->i_buffer_tcp,
                 i_tcp_read,
                 p_access->i_buffer_udp,
                 i_udp_read );
    }
    else
    {
        msg_Dbg( p_input,
                 "filling buffer TCP:%d+%d",
                 p_access->i_buffer_tcp,
                 i_tcp_read );
    }
#endif
    p_access->i_buffer_tcp += i_tcp_read;
    p_access->i_buffer_udp += i_udp_read;

    return( i_tcp_read + i_udp_read);
#endif
}

static int  mms_ParseCommand( input_thread_t *p_input,
                              uint8_t *p_data,
                              int i_data,
                              int *pi_used )
{
 #define GET32( i_pos ) \
    ( p_access->p_cmd[i_pos] + ( p_access->p_cmd[i_pos +1] << 8 ) + \
      ( p_access->p_cmd[i_pos + 2] << 16 ) + \
      ( p_access->p_cmd[i_pos + 3] << 24 ) )

    access_t    *p_access = (access_t*)p_input->p_access_data;
    int         i_length;
    uint32_t    i_id;

    if( p_access->p_cmd )
    {
        free( p_access->p_cmd );
    }
    p_access->i_cmd = i_data;
    p_access->p_cmd = malloc( i_data );
    memcpy( p_access->p_cmd, p_data, i_data );

    *pi_used = i_data; /* by default */

    if( i_data < MMS_CMD_HEADERSIZE )
    {
        msg_Warn( p_input, "truncated command (header incomplete)" );
        p_access->i_command = 0;
        return( -1 );
    }
    i_id =  GetDWLE( p_data + 4 );
    i_length = GetDWLE( p_data + 8 ) + 16;

    if( i_id != 0xb00bface )
    {
        msg_Err( p_input,
                 "incorrect command header (0x%x)", i_id );
        p_access->i_command = 0;
        return( -1 );
    }

    if( i_length > p_access->i_cmd )
    {
        msg_Warn( p_input,
                  "truncated command (missing %d bytes)",
                   i_length - i_data  );
        p_access->i_command = 0;
        return( -1 );
    }
    else if( i_length < p_access->i_cmd )
    {
        p_access->i_cmd = i_length;
        *pi_used = i_length;
    }

    msg_Dbg( p_input,
             "recv command start_sequence:0x%8.8x command_id:0x%8.8x length:%d len8:%d sequence 0x%8.8x len8_II:%d dir_comm:0x%8.8x",
             GET32( 0 ),
             GET32( 4 ),
             GET32( 8 ),
             /* 12: protocol type "MMS " */
             GET32( 16 ),
             GET32( 20 ),
             /* 24: unknown (0) */
             /* 28: unknown (0) */
             GET32( 32 ),
             GET32( 36 )
             /* 40: switches */
             /* 44: extra */ );

    p_access->i_command = GET32( 36 ) & 0xffff;

    return( MMS_PACKET_CMD );
}

static int  mms_ParsePacket( input_thread_t *p_input,
                             uint8_t *p_data, size_t i_data,
                             int *pi_used )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    int i_packet_seq_num;
    size_t i_packet_length;
    uint32_t i_packet_id;

    uint8_t  *p_packet;


//    *pi_used = i_data; /* default */
    *pi_used = i_data; /* default */
    if( i_data <= 8 )
    {
        msg_Warn( p_input, "truncated packet (header incomplete)" );
        return( -1 );
    }

    i_packet_id = p_data[4];
    i_packet_seq_num = GetDWLE( p_data );
    i_packet_length = GetWLE( p_data + 6 );


    if( i_packet_length > i_data || i_packet_length <= 8)
    {
        msg_Warn( p_input,
                  "truncated packet (missing %d bytes)",
                  i_packet_length - i_data  );
        *pi_used = 0;
        return( -1 );
    }
    else if( i_packet_length < i_data )
    {
        *pi_used = i_packet_length;
    }

    if( i_packet_id == 0xff )
    {
        msg_Warn( p_input,
                  "receive MMS UDP pair timing" );
        return( MMS_PACKET_UDP_TIMING );
    }

    if( i_packet_id != p_access->i_header_packet_id_type &&
        i_packet_id != p_access->i_media_packet_id_type )
    {
        msg_Warn( p_input, "incorrect Packet Id Type (0x%x)", i_packet_id );
        return( -1 );
    }

    /* we now have a media or a header packet */
    p_packet = malloc( i_packet_length - 8 ); // don't bother with preheader
    memcpy( p_packet, p_data + 8, i_packet_length - 8 );

    if( i_packet_seq_num != p_access->i_packet_seq_num )
    {
        /* FIXME for udp could be just wrong order ? */
        msg_Warn( p_input,
                  "detected packet lost (%d != %d)",
                  i_packet_seq_num,
                  p_access->i_packet_seq_num );
        p_access->i_packet_seq_num = i_packet_seq_num;
    }
    p_access->i_packet_seq_num++;

    if( i_packet_id == p_access->i_header_packet_id_type )
    {
        FREE( p_access->p_header );
        p_access->p_header = p_packet;
        p_access->i_header = i_packet_length - 8;
/*        msg_Dbg( p_input,
                 "receive header packet (%d bytes)",
                 i_packet_length - 8 ); */

        return( MMS_PACKET_HEADER );
    }
    else
    {
        FREE( p_access->p_media );
        p_access->p_media = p_packet;
        p_access->i_media = i_packet_length - 8;
        p_access->i_media_used = 0;
/*        msg_Dbg( p_input,
                 "receive media packet (%d bytes)",
                 i_packet_length - 8 ); */

        return( MMS_PACKET_MEDIA );
    }
}

static int mms_ReceivePacket( input_thread_t *p_input )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    int i_packet_tcp_type;
    int i_packet_udp_type;

    for( ;; )
    {
        if( NetFillBuffer( p_input ) < 0 )
        {
            msg_Warn( p_input, "cannot fill buffer" );
            continue;
        }
        /* TODO udp */

        i_packet_tcp_type = -1;
        i_packet_udp_type = -1;

        if( p_access->i_buffer_tcp > 0 )
        {
            int i_used;

            if( GetDWLE( p_access->buffer_tcp + 4 ) == 0xb00bface )
            {
                i_packet_tcp_type =
                    mms_ParseCommand( p_input,
                                      p_access->buffer_tcp,
                                      p_access->i_buffer_tcp,
                                      &i_used );

            }
            else
            {
                i_packet_tcp_type =
                    mms_ParsePacket( p_input,
                                     p_access->buffer_tcp,
                                     p_access->i_buffer_tcp,
                                     &i_used );
            }
            if( i_used < MMS_BUFFER_SIZE )
            {
                memmove( p_access->buffer_tcp,
                         p_access->buffer_tcp + i_used,
                         MMS_BUFFER_SIZE - i_used );
            }
            p_access->i_buffer_tcp -= i_used;
        }
        else if( p_access->i_buffer_udp > 0 )
        {
            int i_used;
#if 0
            if( GetDWLE( p_access->buffer_tcp + 4 ) == 0xb00bface )
            {
                i_packet_tcp_type =
                    mms_ParseCommand( p_input,
                                      p_access->buffer_tcp,
                                      p_access->i_buffer_tcp,
                                      &i_used );

            }
            else
#endif
            {
                i_packet_tcp_type =
                    mms_ParsePacket( p_input,
                                     p_access->buffer_udp,
                                     p_access->i_buffer_udp,
                                     &i_used );
            }
            if( i_used < MMS_BUFFER_SIZE )
            {
                memmove( p_access->buffer_udp,
                         p_access->buffer_udp + i_used,
                         MMS_BUFFER_SIZE - i_used );
            }
            p_access->i_buffer_udp -= i_used;
        }
        else
        {
            i_packet_udp_type = -1;
        }

        if( i_packet_tcp_type == MMS_PACKET_CMD &&
                p_access->i_command == 0x1b )
        {
            mms_CommandSend( p_input, 0x1b, 0, 0, NULL, 0 );
            i_packet_tcp_type = -1;
        }

        if( i_packet_tcp_type != -1 )
        {
            return( i_packet_tcp_type );
        }
        else if( i_packet_udp_type != -1 )
        {
            return( i_packet_udp_type );
        }

    }
}

static int  mms_ReceiveCommand( input_thread_t *p_input )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;

    for( ;; )
    {
        int i_used;
        int i_status;
        NetFillBuffer( p_input );

        i_status = mms_ParseCommand( p_input,
                              p_access->buffer_tcp,
                              p_access->i_buffer_tcp,
                              &i_used );
        if( i_used < MMS_BUFFER_SIZE )
        {
            memmove( p_access->buffer_tcp,
                     p_access->buffer_tcp + i_used,
                     MMS_BUFFER_SIZE - i_used );
        }
        p_access->i_buffer_tcp -= i_used;

        if( i_status < 0 )
        {
            return( -1 );
        }

        if( p_access->i_command == 0x1b )
        {
            mms_CommandSend( p_input, 0x1b, 0, 0, NULL, 0 );
        }
        else
        {
            break;
        }
    }

    return( 0 );
}

#define MMS_RETRY_MAX       10
#define MMS_RETRY_SLEEP     50000

static int mms_CommandRead( input_thread_t *p_input, int i_command1, int i_command2 )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    int i_count;
    int i_status;

    for( i_count = 0; i_count < MMS_RETRY_MAX; )
    {

        i_status = mms_ReceiveCommand( p_input );
        if( i_status < 0 || p_access->i_command == 0 )
        {
            i_count++;
            msleep( MMS_RETRY_SLEEP );
        }
        else if( i_command1 == 0 && i_command2 == 0)
        {
            return( 0 );
        }
        else if( p_access->i_command == i_command1 || p_access->i_command == i_command2 )
        {
            return( 0 );
        }
        else
        {
            switch( p_access->i_command )
            {
                case 0x03:
                    msg_Warn( p_input, "socket closed by server" );
                    p_access->i_eos = 1;
                    return( -1 );
                case 0x1e:
                    msg_Warn( p_input, "end of media stream" );
                    p_access->i_eos = 1;
                    return( -1 );
                default:
                    break;
            }
        }
    }
    msg_Warn( p_input, "failed to receive command (abording)" );

    return( -1 );
}


static int mms_HeaderMediaRead( input_thread_t *p_input, int i_type )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    int         i_count;

    for( i_count = 0; i_count < MMS_RETRY_MAX; )
    {
        int i_status;

        i_status = mms_ReceivePacket( p_input );
        if( i_status < 0 )
        {
            i_count++;
            msg_Warn( p_input,
                      "cannot receive header (%d/%d)", i_count, MMS_RETRY_MAX );
            msleep( MMS_RETRY_SLEEP );
        }
        else if( i_status == i_type || i_type == MMS_PACKET_ANY )
        {
            return( i_type );
        }
        else if( i_status == MMS_PACKET_CMD )
        {
            switch( p_access->i_command )
            {
                case 0x03:
                    msg_Warn( p_input, "socket closed by server" );
                    p_access->i_eos = 1;
                    return( -1 );
                case 0x1e:
                    msg_Warn( p_input, "end of media stream" );
                    p_access->i_eos = 1;
                    return( -1 );
                case 0x20:
                    /* XXX not too dificult to be done EXCEPT that we
                     * need to restart demuxer... and I don't see how we
                     * could do that :p */
                    msg_Err( p_input,
                             "reinitialization needed --> unsupported" );
                    p_access->i_eos = 1;
                    return( -1 );
                default:
                    break;
            }
        }
    }
    msg_Err( p_input,
             "cannot receive %s (abording)",
               ( i_type == MMS_PACKET_HEADER ) ? "header" : "media data" );
    return( -1 );
}

