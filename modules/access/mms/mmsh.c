/*****************************************************************************
 * mmsh.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mmsh.c,v 1.3 2003/05/08 19:06:45 titer Exp $
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
 *  * http_proxy
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif

#include "network.h"
#include "asf.h"
#include "buffer.h"

#include "mms.h"
#include "mmsh.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_( MMSHOpen )  ( input_thread_t * );
void E_( MMSHClose ) ( input_thread_t * );

static ssize_t Read        ( input_thread_t * p_input, byte_t * p_buffer,
                             size_t i_len );
static void    Seek        ( input_thread_t *, off_t );

/****************************************************************************
 ****************************************************************************
 *******************                                      *******************
 *******************       Main functions                 *******************
 *******************                                      *******************
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
int  E_( MMSHOpen )  ( input_thread_t *p_input )
{
    access_sys_t    *p_sys;

    uint8_t         *p;
    http_answer_t   *p_ans;
    http_field_t    *p_field;
    chunk_t         ck;

    /* init p_sys */
    p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->i_proto = MMS_PROTO_HTTP;

    p_sys->p_socket = NULL;
    p_sys->i_request_context = 1;
    p_sys->i_buffer = 0;
    p_sys->i_buffer_pos = 0;
    p_sys->b_broadcast = VLC_TRUE;
    p_sys->p_packet = NULL;
    p_sys->i_packet_sequence = 0;
    p_sys->i_packet_used = 0;
    p_sys->i_packet_length = 0;
    p_sys->i_pos = 0;
    E_( GenerateGuid )( &p_sys->guid );

    /* open a tcp connection */
    p_sys->p_url = E_( url_new )( p_input->psz_name );

    if( *p_sys->p_url->psz_host == '\0' )
    {
        msg_Err( p_input, "invalid server addresse" );
        goto exit_error;
    }
    if( p_sys->p_url->i_port <= 0 )
    {
        p_sys->p_url->i_port = 80;
    }

    p_sys->p_socket = NetOpenTCP( p_input, p_sys->p_url );
    if( !p_sys->p_socket )
    {
        msg_Err( p_input, "cannot connect" );
        goto exit_error;
    }
    p_sys->i_request_context = 1;

    /* *** send first request *** */
    p = &p_sys->buffer[0];
    p += sprintf( p, "GET %s HTTP/1.0\r\n", p_sys->p_url->psz_path );
    p += sprintf( p,"Accept: */*\r\n" );
    p += sprintf( p, "User-Agent: NSPlayer/4.1.0.3856\r\n" );
    p += sprintf( p, "Host: %s:%d\r\n", p_sys->p_url->psz_host, p_sys->p_url->i_port );
    p += sprintf( p, "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=%d,max-duration=0\r\n", p_sys->i_request_context++ );
    //p += sprintf( p, "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}\r\n" );
    p += sprintf( p, "Pragma: xClientGUID={"GUID_FMT"}\r\n", GUID_PRINT( p_sys->guid ) );
    p += sprintf( p, "Connection: Close\r\n\r\n" );
    NetWrite( p_input, p_sys->p_socket, p_sys->buffer,  p - p_sys->buffer );


    if( NetFill ( p_input, p_sys, BUFFER_SIZE ) <= 0 )
    {
        msg_Err( p_input, "cannot read answer" );
        goto exit_error;
    }
    NetClose( p_input, p_sys->p_socket );

    p_ans = http_answer_parse( p_sys->buffer, p_sys->i_buffer );
    if( !p_ans )
    {
        msg_Err( p_input, "cannot parse answer" );
        goto exit_error;
    }

    if( p_ans->i_error >= 400 )
    {
        msg_Err( p_input, "error %d (server return=`%s')", p_ans->i_error, p_ans->psz_answer );
        http_answer_free( p_ans );
        goto exit_error;
    }
    else if( p_ans->i_error >= 300 )
    {
        msg_Err( p_input, "FIXME redirec unsuported %d (server return=`%s')", p_ans->i_error, p_ans->psz_answer );
        http_answer_free( p_ans );
        goto exit_error;
    }
    else if( p_ans->i_body <= 0 )
    {
        msg_Err( p_input, "empty answer" );
        http_answer_free( p_ans );
        goto exit_error;
    }

    /* now get features */
    /* FIXME FIXME test Content-Type to see if it's a plain stream or an asx FIXME */
    for( p_field = p_ans->p_fields; p_field != NULL; p_field = http_field_find( p_field->p_next, "Pragma" ) )
    {
        if( !strncasecmp( p_field->psz_value, "features", 8 ) )
        {
            if( strstr( p_field->psz_value, "broadcast" ) )
            {
                msg_Dbg( p_input, "stream type = broadcast" );
                p_sys->b_broadcast = VLC_TRUE;
            }
            else if( strstr( p_field->psz_value, "seekable" ) )
            {
                msg_Dbg( p_input, "stream type = seekable" );
                p_sys->b_broadcast = VLC_FALSE;
            }
            else
            {
                msg_Warn( p_input, "unknow stream types (%s)", p_field->psz_value );
                p_sys->b_broadcast = VLC_FALSE;
            }
        }
    }

    /* gather header */
    p_sys->i_header = 0;
    p_sys->p_header = malloc( p_ans->i_body );
    do
    {
        if( chunk_parse( &ck, p_ans->p_body, p_ans->i_body ) )
        {
            msg_Err( p_input, "invalid chunk answer" );
            goto exit_error;
        }
        if( ck.i_type != 0x4824 )
        {
            msg_Err( p_input, "invalid chunk (0x%x)", ck.i_type );
            break;
        }
        if( ck.i_data > 0 )
        {
            memcpy( &p_sys->p_header[p_sys->i_header],
                    ck.p_data,
                    ck.i_data );

            p_sys->i_header += ck.i_data;
        }

        /* BEURK */
        p_ans->p_body   += 12 + ck.i_data;
        p_ans->i_body   -= 12 + ck.i_data;

    } while( p_ans->i_body > 12 );

    http_answer_free( p_ans );

    msg_Dbg( p_input, "complete header size=%d", p_sys->i_header );
    if( p_sys->i_header <= 0 )
    {
        msg_Err( p_input, "header size == 0" );
        goto exit_error;
    }
    /* *** parse header and get stream and their id *** */
    /* get all streams properties,
     *
     * TODO : stream bitrates properties(optional)
     *        and bitrate mutual exclusion(optional) */
    E_( asf_HeaderParse )( &p_sys->asfh,
                           p_sys->p_header, p_sys->i_header );
    msg_Dbg( p_input, "packet count=%lld packet size=%d",p_sys->asfh.i_data_packets_count, p_sys->asfh.i_min_data_packet_size );

    E_( asf_StreamSelect)( &p_sys->asfh,
                           config_GetInt( p_input, "mms-maxbitrate" ),
                           config_GetInt( p_input, "mms-all" ),
                           config_GetInt( p_input, "audio" ),
                           config_GetInt( p_input, "video" ) );

    if( mmsh_start( p_input, p_sys, 0 ) )
    {
        msg_Err( p_input, "cannot start stream" );
        goto exit_error;
    }

    /* *** set exported functions *** */
    p_input->pf_read = Read;
    p_input->pf_seek = Seek;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;

    p_input->p_private = NULL;
    p_input->i_mtu = 3 * p_sys->asfh.i_min_data_packet_size;

    /* *** finished to set some variable *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    if( p_sys->b_broadcast )
    {
        p_input->stream.p_selected_area->i_size = 0;
        p_input->stream.b_seekable = 0;
    }
    else
    {
        p_input->stream.p_selected_area->i_size = p_sys->asfh.i_file_size;
        p_input->stream.b_seekable = 1;
    }
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for ftp access */
    p_input->i_pts_delay = config_GetInt( p_input, "mms-caching" ) * 1000;

    p_input->p_access_data = p_sys;

    return( VLC_SUCCESS );

exit_error:
    E_( url_free )( p_sys->p_url );

    if( p_sys->p_socket )
    {
        NetClose( p_input, p_sys->p_socket );
    }
    free( p_sys );
    return( VLC_EGENERIC );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
void E_( MMSHClose ) ( input_thread_t *p_input )
{
    access_sys_t    *p_sys   = p_input->p_access_data;

    msg_Dbg( p_input, "stopping stream" );

    mmsh_stop( p_input, p_sys );

    free( p_sys );
}

static int mmsh_get_packet( input_thread_t * p_input, access_sys_t *p_sys, chunk_t *p_ck )
{
    int i_mov = p_sys->i_buffer - p_sys->i_buffer_pos;

    if( p_sys->i_buffer_pos > BUFFER_SIZE / 2 )
    {
        if( i_mov > 0 )
        {
            memmove( &p_sys->buffer[0], 
                     &p_sys->buffer[p_sys->i_buffer_pos],
                     i_mov );
        }

        p_sys->i_buffer     = i_mov;
        p_sys->i_buffer_pos = 0;
    }

    if( NetFill( p_input, p_sys, 12 ) < 12 )
    {
        msg_Warn( p_input, "cannot fill buffer" );
        return VLC_EGENERIC;
    }

    chunk_parse( p_ck, &p_sys->buffer[p_sys->i_buffer_pos], p_sys->i_buffer - p_sys->i_buffer_pos );

    if( p_ck->i_type == 0x4524 )   // Transfer complete
    {
        msg_Warn( p_input, "EOF" );
        return VLC_EGENERIC;
    }
    else if( p_ck->i_type != 0x4824 && p_ck->i_type != 0x4424 )
    {
        msg_Err( p_input, "invalid chunk FATAL" );
        return VLC_EGENERIC;
    }

    if( p_ck->i_data < p_ck->i_size2 - 8 )
    {
        if( NetFill( p_input, p_sys, p_ck->i_size2 - 8 - p_ck->i_data ) <= 0 )
        {
            msg_Warn( p_input, "cannot fill buffer" );
            return VLC_EGENERIC;
        }
        chunk_parse( p_ck, &p_sys->buffer[p_sys->i_buffer_pos], p_sys->i_buffer - p_sys->i_buffer_pos );
    }

    if( p_sys->i_packet_sequence != 0 && p_ck->i_sequence != p_sys->i_packet_sequence )
    {
        msg_Warn( p_input, "packet lost ?" );
    }

    p_sys->i_packet_sequence = p_ck->i_sequence + 1;
    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = p_ck->i_data;
    p_sys->p_packet        = p_ck->p_data;

    p_sys->i_buffer_pos += 12 + p_ck->i_data;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    access_sys_t *p_sys = p_input->p_access_data;
    chunk_t      ck;
    off_t        i_offset;
    off_t        i_packet;

    i_packet = ( i_pos - p_sys->i_header ) / p_sys->asfh.i_min_data_packet_size;
    i_offset = ( i_pos - p_sys->i_header ) % p_sys->asfh.i_min_data_packet_size;

    msg_Err( p_input, "seeking to "I64Fd, i_pos );

    vlc_mutex_lock( &p_input->stream.stream_lock );

    mmsh_stop( p_input, p_sys );
    mmsh_start( p_input, p_sys, i_packet * p_sys->asfh.i_min_data_packet_size );

    for( ;; )
    {
        if( mmsh_get_packet( p_input, p_sys, &ck ) )
        {
            break;
        }

        /* skip headers */
        if( ck.i_type != 0x4824 )
        {
            break;
        }
        msg_Warn( p_input, "skipping header" );
    }

    p_sys->i_pos = i_pos;
    p_sys->i_packet_used += i_offset;


    p_input->stream.p_selected_area->i_tell = i_pos;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read        ( input_thread_t * p_input, byte_t * p_buffer,
                             size_t i_len )
{
    access_sys_t *p_sys = p_input->p_access_data;
    size_t       i_copy;
    size_t       i_data = 0;

    while( i_data < i_len )
    {
        if( p_sys->i_packet_used < p_sys->i_packet_length )
        {
            i_copy = __MIN( p_sys->i_packet_length - p_sys->i_packet_used, i_len - i_data );

            memcpy( &p_buffer[i_data],
                    &p_sys->p_packet[p_sys->i_packet_used],
                    i_copy );

            i_data += i_copy;
            p_sys->i_packet_used += i_copy;
        }
        else if( p_sys->i_pos + i_data > p_sys->i_header &&
                 (int)p_sys->i_packet_used < p_sys->asfh.i_min_data_packet_size )
        {
            i_copy = __MIN( p_sys->asfh.i_min_data_packet_size - p_sys->i_packet_used, i_len - i_data );

            memset( &p_buffer[i_data], 0, i_copy );

            i_data += i_copy;
            p_sys->i_packet_used += i_copy;
        }
        else
        {
            chunk_t ck;
            /* get a new packet */
            /* fill enought data (>12) */
            msg_Dbg( p_input, "waiting data (buffer = %d bytes", p_sys->i_buffer );

            if( mmsh_get_packet( p_input, p_sys, &ck ) )
            {
                return 0;
            }

            //fprintf( stderr, "type=0x%x size=%d sequence=%d unknown=%d size2=%d data=%d\n",
            //         ck.i_type, ck.i_size, ck.i_sequence, ck.i_unknown, ck.i_size2, ck.i_data );
        }
    }

    p_sys->i_pos += i_data;


    return( i_data );
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int mmsh_start( input_thread_t *p_input, access_sys_t *p_sys, off_t i_pos )
{
    uint8_t *p;
    int i_streams = 0;
    int i;
    http_answer_t *p_ans;

    msg_Dbg( p_input, "starting stream" );

    p_sys->p_socket = NetOpenTCP( p_input, p_sys->p_url );

    for( i = 1; i < 128; i++ )
    {
        if( p_sys->asfh.stream[i].i_selected )
        {
            i_streams++;
        }
    }

    if( i_streams <= 0 )
    {
        msg_Err( p_input, "no stream selected" );
        return VLC_EGENERIC;
    }

    p = &p_sys->buffer[0];
    p += sprintf( p, "GET %s HTTP/1.0\r\n", p_sys->p_url->psz_path );
    p += sprintf( p,"Accept: */*\r\n" );
    p += sprintf( p, "User-Agent: NSPlayer/4.1.0.3856\r\n" );
    p += sprintf( p, "Host: %s:%d\r\n", p_sys->p_url->psz_host, p_sys->p_url->i_port );
    if( p_sys->b_broadcast )
    {
        p += sprintf( p, "Pragma: no-cache,rate=1.000000,request-context=%d\r\n", p_sys->i_request_context++ );
    }
    else
    {
        p += sprintf( p, "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=0\r\n",
                         (uint32_t)((i_pos >> 32)&0xffffffff), (uint32_t)(i_pos&0xffffffff), p_sys->i_request_context++ );
    }
    p += sprintf( p, "Pragma: xPlayStrm=1\r\n" );
    //p += sprintf( p, "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}\r\n" );
    p += sprintf( p, "Pragma: xClientGUID={"GUID_FMT"}\r\n", GUID_PRINT( p_sys->guid ) );
    p += sprintf( p, "Pragma: stream-switch-count=%d\r\n", i_streams );
    p += sprintf( p, "Pragma: stream-switch-entry=" );
    for( i = 0; i < i_streams; i++ )
    {
        if( p_sys->asfh.stream[i].i_selected )
        {
            p += sprintf( p, "ffff:%d:0 ", p_sys->asfh.stream[i].i_id );
        }
        else
        {
            p += sprintf( p, "ffff:%d:2 ", p_sys->asfh.stream[i].i_id );
        }
    }
    p += sprintf( p, "\r\n" );
    p += sprintf( p, "Connection: Close\r\n\r\n" );


    NetWrite( p_input, p_sys->p_socket, p_sys->buffer,  p - p_sys->buffer );

    msg_Dbg( p_input, "filling buffer" );
    /* we read until we found a \r\n\r\n or \n\n */
    p_sys->i_buffer = 0;
    p_sys->i_buffer_pos = 0;
    for( ;; )
    {
        int     i_try = 0;
        int     i_read;
        uint8_t *p;

        p = &p_sys->buffer[p_sys->i_buffer];
        i_read =
            NetRead( p_input, p_sys->p_socket,
                     &p_sys->buffer[p_sys->i_buffer],
                      1024 );

        if( i_read == 0 )
        {
            if( i_try++ > 12 )
            {
                break;
            }
            msg_Dbg( p_input, "another try (%d/12)", i_try );
            continue;
        }

        if( i_read <= 0 || p_input->b_die || p_input->b_error )
        {
            break;
        }
        p_sys->i_buffer += i_read;
        p_sys->buffer[p_sys->i_buffer] = '\0';

        if( strstr( p, "\r\n\r\n" ) || strstr( p, "\n\n" ) )
        {
            msg_Dbg( p_input, "body found" );
            break;
        }
        if( p_sys->i_buffer >= BUFFER_SIZE - 1024 )
        {
            msg_Dbg( p_input, "buffer size exeded" );
            break;
        }
    }

    p_ans = http_answer_parse( p_sys->buffer, p_sys->i_buffer );
    if( !p_ans )
    {
        msg_Err( p_input, "cannot parse answer" );
        return VLC_EGENERIC;
    }

    if( p_ans->i_error < 200 || p_ans->i_error >= 300 )
    {
        msg_Err( p_input, "error %d (server return=`%s')", p_ans->i_error, p_ans->psz_answer );
        http_answer_free( p_ans );
        return VLC_EGENERIC;
    }

    if( !p_ans->p_body )
    {
        p_sys->i_buffer_pos = 0;
        p_sys->i_buffer = 0;
    }
    else
    {
        p_sys->i_buffer_pos = p_ans->p_body - p_sys->buffer;
    }
    http_answer_free( p_ans );

    return VLC_SUCCESS;
}

static void mmsh_stop( input_thread_t *p_input, access_sys_t *p_sys )
{
    msg_Dbg( p_input, "closing stream" );
    NetClose( p_input, p_sys->p_socket );
}

static ssize_t NetFill( input_thread_t *p_input,
                        access_sys_t   *p_sys, int i_size )
{
    int i_try   = 0;
    int i_total = 0;

    i_size = __MIN( i_size, BUFFER_SIZE - p_sys->i_buffer );
    if( i_size <= 0 )
    {
        return 0;
    }
    

    for( ;; )
    {
        int i_read;

        i_read =
            NetRead( p_input, p_sys->p_socket, &p_sys->buffer[p_sys->i_buffer], i_size );

        if( i_read == 0 )
        {
            if( i_try++ > 2 )
            {
                break;
            }
            msg_Dbg( p_input, "another try %d/2", i_try );
            continue;
        }

        if( i_read < 0 || p_input->b_die || p_input->b_error )
        {
            break;
        }
        i_total += i_read;

        p_sys->i_buffer += i_read;
        if( i_total >= i_size )
        {
            break;
        }
    }

    p_sys->buffer[p_sys->i_buffer] = '\0';

    return i_total;
}

/****************************************************************************
 * NetOpenTCP:
 ****************************************************************************/
static input_socket_t * NetOpenTCP( input_thread_t *p_input, url_t *p_url )
{
    input_socket_t   *p_socket;
    char             *psz_network;
    module_t         *p_network;
    network_socket_t socket_desc;


    p_socket = malloc( sizeof( input_socket_t ) );
    memset( p_socket, 0, sizeof( input_socket_t ) );

    psz_network = "";
    if( config_GetInt( p_input, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    else if( config_GetInt( p_input, "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    msg_Dbg( p_input, "waiting for connection..." );

    socket_desc.i_type = NETWORK_TCP;
    socket_desc.psz_server_addr = p_url->psz_host;
    socket_desc.i_server_port   = p_url->i_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    p_input->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_input, "network", psz_network ) ) )
    {
        msg_Err( p_input, "failed to connect with server" );
        return NULL;
    }
    module_Unneed( p_input, p_network );
    p_socket->i_handle = socket_desc.i_handle;
    p_input->i_mtu     = socket_desc.i_mtu;

    msg_Dbg( p_input,
             "connection with \"%s:%d\" successful",
             p_url->psz_host,
             p_url->i_port );

    return p_socket;
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t NetRead( input_thread_t *p_input,
                        input_socket_t *p_socket,
                        byte_t *p_buffer, size_t i_len )
{
    struct timeval  timeout;
    fd_set          fds;
    ssize_t         i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_socket->i_handle, &fds );

    /* We'll wait 1 second if nothing happens */
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    /* Find if some data is available */
    while( ( i_ret = select( p_socket->i_handle + 1, &fds,
                             NULL, NULL, &timeout )) == 0 ||
#ifdef HAVE_ERRNO_H
           ( i_ret < 0 && errno == EINTR )
#endif
         )
    {
        FD_ZERO( &fds );
        FD_SET( p_socket->i_handle, &fds );
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        if( p_input->b_die || p_input->b_error )
        {
            return 0;
        }
    }

    if( i_ret < 0 )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
        return -1;
    }

    i_recv = recv( p_socket->i_handle, p_buffer, i_len, 0 );

    if( i_recv < 0 )
    {
        msg_Err( p_input, "recv failed (%s)", strerror(errno) );
    }

    return i_recv;
}

static ssize_t NetWrite( input_thread_t *p_input,
                         input_socket_t *p_socket,
                         byte_t *p_buffer, size_t i_len )
{
    struct timeval  timeout;
    fd_set          fds;
    ssize_t         i_send;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_socket->i_handle, &fds );

    /* We'll wait 1 second if nothing happens */
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    /* Find if some data is available */
    while( ( i_ret = select( p_socket->i_handle + 1, NULL, &fds, NULL, &timeout ) ) == 0 ||
#ifdef HAVE_ERRNO_H
           ( i_ret < 0 && errno == EINTR )
#endif
         )
    {
        FD_ZERO( &fds );
        FD_SET( p_socket->i_handle, &fds );
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        if( p_input->b_die || p_input->b_error )
        {
            return 0;
        }
    }

    if( i_ret < 0 )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
        return -1;
    }

    i_send = send( p_socket->i_handle, p_buffer, i_len, 0 );

    if( i_send < 0 )
    {
        msg_Err( p_input, "send failed (%s)", strerror(errno) );
    }

    return i_send;
}

static void NetClose( input_thread_t *p_input, input_socket_t *p_socket )
{
#if defined( WIN32 ) || defined( UNDER_CE )
    closesocket( p_socket->i_handle );
#else
    close( p_socket->i_handle );
#endif

    free( p_socket );
}

static int http_next_line( uint8_t **pp_data, int *pi_data )
{
    char *p, *p_end = *pp_data + *pi_data;

    for( p = *pp_data; p < p_end; p++ )
    {
        if( p + 1 < p_end && *p == '\n' )
        {
            *pi_data = p_end - p - 1;
            *pp_data = p + 1;
            return VLC_SUCCESS;
        }
        if( p + 2 < p_end && p[0] == '\r' && p[1] == '\n' )
        {
            *pi_data = p_end - p - 2;
            *pp_data = p + 2;
            return VLC_SUCCESS;
        }
    }
    *pi_data = 0;
    *pp_data = p_end;
    return VLC_EGENERIC;
}

static http_answer_t *http_answer_parse( uint8_t *p_data, int i_data )
{
    http_answer_t *ans = malloc( sizeof( http_answer_t ) );
    http_field_t  **pp_last;
    char          buffer[1024];

    if( sscanf( p_data, "HTTP/1.%d %d %s", &ans-> i_version, &ans->i_error, buffer ) < 3 )
    {
        free( ans );
        return NULL;
    }
    ans->psz_answer = strdup( buffer );
    fprintf( stderr, "version=%d error=%d answer=%s\n", ans-> i_version, ans->i_error, ans->psz_answer );
    ans->p_fields = NULL;
    ans->i_body   = 0;
    ans->p_body   = 0;

    pp_last = &ans->p_fields;

    for( ;; )
    {
        http_field_t  *p_field;
        uint8_t       *colon;

        if( http_next_line( &p_data, &i_data ) )
        {
            return ans;
        }
        if( !strncmp( p_data, "\r\n", 2 ) || !strncmp( p_data, "\n", 1 ) )
        {
            break;
        }

        colon = strstr( p_data, ": " );
        if( colon )
        {
            uint8_t *end;

            end = strstr( colon, "\n" ) - 1;
            if( *end != '\r' )
            {
                end++;
            }

            p_field             = malloc( sizeof( http_field_t ) );
            p_field->psz_name   = strndup( p_data, colon - p_data );
            p_field->psz_value  = strndup( colon + 2, end - colon - 2 );
            p_field->p_next     = NULL;

            *pp_last = p_field;
            pp_last = &p_field->p_next;

            fprintf( stderr, "field name=`%s' value=`%s'\n", p_field->psz_name, p_field->psz_value );
        }
    }

    if( http_next_line( &p_data, &i_data ) )
    {
        return ans;
    }

    ans->p_body = p_data;
    ans->i_body = i_data;
    fprintf( stderr, "body size=%d\n", i_data );

    return ans;
}

static void http_answer_free( http_answer_t *ans )
{
    http_field_t  *p_field = ans->p_fields;

    while( p_field )
    {
        http_field_t *p_next;

        p_next = p_field->p_next;
        free( p_field->psz_name );
        free( p_field->psz_value );
        free( p_field );

        p_field = p_next;
    }

    free( ans->psz_answer );
    free( ans );
}

static http_field_t *http_field_find( http_field_t *p_field, char *psz_name )
{

    while( p_field )
    {
        if( !strcasecmp( p_field->psz_name, psz_name ) )
        {
            return p_field;
        }

        p_field = p_field->p_next;
    }

    return NULL;
}
static char *http_field_get_value( http_answer_t *ans, char *psz_name )
{
    http_field_t  *p_field = ans->p_fields;

    while( p_field )
    {
        if( !strcasecmp( p_field->psz_name, psz_name ) )
        {
            return p_field->psz_value;
        }

        p_field = p_field->p_next;
    }

    return NULL;
}



static int chunk_parse( chunk_t *ck, uint8_t *p_data, int i_data )
{
    if( i_data < 12 )
    {
        return VLC_EGENERIC;
    }

    ck->i_type      = GetWLE( p_data );
    ck->i_size      = GetWLE( p_data + 2);
    ck->i_sequence  = GetDWLE( p_data  + 4);
    ck->i_unknown   = GetWLE( p_data + 8);
    ck->i_size2     = GetWLE( p_data + 10);

    ck->p_data      = p_data + 12;
    ck->i_data      = __MIN( i_data - 12, ck->i_size2 - 8 );

#if 0
    fprintf( stderr, "type=0x%x size=%d sequence=%d unknown=%d size2=%d data=%d\n",
             ck->i_type, ck->i_size, ck->i_sequence, ck->i_unknown, ck->i_size2, ck->i_data );
#endif
    return VLC_SUCCESS;
}

