/*****************************************************************************
 * mmsh.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

#include "network.h"
#include "asf.h"
#include "buffer.h"

#include "mms.h"
#include "mmsh.h"

/* TODO:
 *  - http_proxy
 *  - authentication
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_(MMSHOpen)  ( input_thread_t * );
void E_(MMSHClose) ( input_thread_t * );

static ssize_t  Read( input_thread_t *, byte_t *, size_t );
static ssize_t  ReadRedirect( input_thread_t *, byte_t *, size_t );
static void     Seek( input_thread_t *, off_t );

static int      Describe( input_thread_t  *, char **ppsz_location );
static int      Start( input_thread_t *, off_t );
static void     Stop( input_thread_t * );
static int      GetPacket( input_thread_t *, chunk_t * );

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
int  E_( MMSHOpen )  ( input_thread_t *p_input )
{
    access_sys_t    *p_sys;

    char            *psz_location = NULL;

    vlc_value_t     val;

    /* init p_sys */
    p_input->p_access_data = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->i_proto = MMS_PROTO_HTTP;

    p_sys->fd       = -1;
    p_sys->i_pos  = 0;
    p_sys->i_start= 0;

    /* open a tcp connection */
    vlc_UrlParse( &p_sys->url, p_input->psz_name, 0 );
    if( p_sys->url.psz_host == NULL && *p_sys->url.psz_host == '\0' )
    {
        msg_Err( p_input, "invalid host" );
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( p_sys->url.i_port <= 0 )
    {
        p_sys->url.i_port = 80;
    }

    if( Describe( p_input, &psz_location ) )
    {
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }
    /* Handle redirection */
    if( psz_location && *psz_location )
    {
        playlist_t * p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );

        msg_Dbg( p_input, "redirection to %s", psz_location );

        if( !p_playlist )
        {
            msg_Err( p_input, "redirection failed: can't find playlist" );
            free( psz_location );
            return VLC_EGENERIC;
        }
        p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
        playlist_Add( p_playlist, psz_location, psz_location,
                      PLAYLIST_INSERT | PLAYLIST_GO,
                      p_playlist->i_index + 1 );
        vlc_object_release( p_playlist );

        free( psz_location );

        p_input->pf_read = ReadRedirect;
        p_input->pf_seek = NULL;
        p_input->pf_set_program = input_SetProgram;
        p_input->pf_set_area = NULL;

        /* *** finished to set some variable *** */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.b_pace_control = 0;
        p_input->stream.p_selected_area->i_size = 0;
        p_input->stream.b_seekable = 0;
        p_input->stream.p_selected_area->i_tell = 0;
        p_input->stream.i_method = INPUT_METHOD_NETWORK;
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        return VLC_SUCCESS;
    }

    /* Start playing */
    if( Start( p_input, 0 ) )
    {
        msg_Err( p_input, "cannot start stream" );
        free( p_sys->p_header );
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
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

    /* Update default_pts to a suitable value for mms access */
    var_Get( p_input, "mms-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
void E_( MMSHClose )( input_thread_t *p_input )
{
    access_sys_t    *p_sys   = p_input->p_access_data;

    msg_Dbg( p_input, "stopping stream" );

    Stop( p_input );


    free( p_sys );
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

    msg_Dbg( p_input, "seeking to "I64Fd, i_pos );

    vlc_mutex_lock( &p_input->stream.stream_lock );

    Stop( p_input );
    Start( p_input, i_packet * p_sys->asfh.i_min_data_packet_size );

    for( ;; )
    {
        if( GetPacket( p_input, &ck ) )
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
static ssize_t ReadRedirect( input_thread_t *p_input, byte_t *p, size_t i )
{
    return 0;
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
        if( p_sys->i_pos < p_sys->i_start + p_sys->i_header )
        {
            int i_offset = p_sys->i_pos - p_sys->i_start;
            i_copy = __MIN( p_sys->i_header - i_offset, i_len - i_data );
            memcpy( &p_buffer[i_data], &p_sys->p_header[i_offset], i_copy );

            i_data += i_copy;
            p_sys->i_pos += i_copy;
        }
        else if( p_sys->i_packet_used < p_sys->i_packet_length )
        {
            i_copy = __MIN( p_sys->i_packet_length - p_sys->i_packet_used,
                            i_len - i_data );

            memcpy( &p_buffer[i_data],
                    &p_sys->p_packet[p_sys->i_packet_used],
                    i_copy );

            i_data += i_copy;
            p_sys->i_packet_used += i_copy;
            p_sys->i_pos += i_copy;
        }
        else if( p_sys->i_packet_length > 0 &&
                 (int)p_sys->i_packet_used < p_sys->asfh.i_min_data_packet_size )
        {
            i_copy = __MIN( p_sys->asfh.i_min_data_packet_size - p_sys->i_packet_used,
                            i_len - i_data );

            memset( &p_buffer[i_data], 0, i_copy );

            i_data += i_copy;
            p_sys->i_packet_used += i_copy;
            p_sys->i_pos += i_copy;
        }
        else
        {
            chunk_t ck;
            if( GetPacket( p_input, &ck ) )
            {
                if( ck.i_type == 0x4524 && ck.i_sequence != 0 && p_sys->b_broadcast )
                {
                    char *psz_location = NULL;

                    p_sys->i_start = p_sys->i_pos;

                    msg_Dbg( p_input, "stoping the stream" );
                    Stop( p_input );

                    msg_Dbg( p_input, "describe the stream" );
                    if( Describe( p_input, &psz_location ) )
                    {
                        msg_Err( p_input, "describe failed" );
                        return -1;
                    }
                    if( Start( p_input, 0 ) )
                    {
                        msg_Err( p_input, "Start failed" );
                        return -1;
                    }
                }
                else
                {
                    return -1;
                }
            }
            if( ck.i_type != 0x4424 )
            {
                p_sys->i_packet_used = 0;
                p_sys->i_packet_length = 0;
            }
        }
    }

    return( i_data );
}

/*****************************************************************************
 * Describe:
 *****************************************************************************/
static int Describe( input_thread_t  *p_input, char **ppsz_location )
{
    access_sys_t *p_sys = p_input->p_access_data;
    char         *psz_location = NULL;
    char         *psz;
    int          i_code;

    /* Reinit context */
    p_sys->b_broadcast = VLC_TRUE;
    p_sys->i_request_context = 1;
    p_sys->i_packet_sequence = 0;
    p_sys->i_packet_used = 0;
    p_sys->i_packet_length = 0;
    p_sys->p_packet = NULL;
    E_( GenerateGuid )( &p_sys->guid );

    if( ( p_sys->fd = net_OpenTCP( p_input, p_sys->url.psz_host,
                                            p_sys->url.i_port ) ) < 0 )
    {
        msg_Err( p_input, "cannot connect to%s:%d", p_sys->url.psz_host, p_sys->url.i_port );
        goto error;
    }

    /* send first request */
    net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                "GET %s HTTP/1.0\r\n"
                "Accept: */*\r\n"
                "User-Agent: NSPlayer/4.1.0.3856\r\n"
                "Host: %s:%d\r\n"
                "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=%d,max-duration=0\r\n"
                "Pragma: xClientGUID={"GUID_FMT"}\r\n"
                "Connection: Close\r\n",
                ( p_sys->url.psz_path == NULL || *p_sys->url.psz_path == '\0' ) ? "/" : p_sys->url.psz_path,
                p_sys->url.psz_host, p_sys->url.i_port,
                p_sys->i_request_context++,
                GUID_PRINT( p_sys->guid ) );

    if( net_Printf( VLC_OBJECT(p_input), p_sys->fd, "\r\n" ) < 0 )
    {
        msg_Err( p_input, "failed to send request" );
        goto error;
    }

    /* Receive the http header */
    if( ( psz = net_Gets( VLC_OBJECT(p_input), p_sys->fd ) ) == NULL )
    {
        msg_Err( p_input, "failed to read answer" );
        goto error;
    }
    if( strncmp( psz, "HTTP/1.", 7 ) )
    {
        msg_Err( p_input, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
    }
    i_code = atoi( &psz[9] );
    if( i_code >= 400 )
    {
        msg_Err( p_input, "error: %s", psz );
        free( psz );
        goto error;
    }

    msg_Dbg( p_input, "HTTP reply '%s'", psz );
    free( psz );
    for( ;; )
    {
        char *psz = net_Gets( p_input, p_sys->fd );
        char *p;

        if( psz == NULL )
        {
            msg_Err( p_input, "failed to read answer" );
            goto error;
        }

        if( *psz == '\0' )
        {
            free( psz );
            break;
        }

        if( ( p = strchr( psz, ':' ) ) == NULL )
        {
            msg_Err( p_input, "malformed header line: %s", psz );
            free( psz );
            goto error;
        }
        *p++ = '\0';
        while( *p == ' ' ) p++;

        /* FIXME FIXME test Content-Type to see if it's a plain stream or an
         * asx FIXME */
        if( !strcasecmp( psz, "Pragma" ) )
        {
            if( strstr( p, "features" ) )
            {
                /* FIXME, it is a bit badly done here ..... */
                if( strstr( p, "broadcast" ) )
                {
                    msg_Dbg( p_input, "stream type = broadcast" );
                    p_sys->b_broadcast = VLC_TRUE;
                }
                else if( strstr( p, "seekable" ) )
                {
                    msg_Dbg( p_input, "stream type = seekable" );
                    p_sys->b_broadcast = VLC_FALSE;
                }
                else
                {
                    msg_Warn( p_input, "unknow stream types (%s)", p );
                    p_sys->b_broadcast = VLC_FALSE;
                }
            }
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            psz_location = strdup( p );
        }

        free( psz );
    }

    /* Handle the redirection */
    if( ( i_code == 301 || i_code == 302 ||
          i_code == 303 || i_code == 307 ) &&
        psz_location && *psz_location )
    {
        msg_Dbg( p_input, "redirection to %s", psz_location );
        net_Close( p_sys->fd ); p_sys->fd = -1;

        *ppsz_location = psz_location;
        return VLC_SUCCESS;
    }

    /* Read the asf header */
    p_sys->i_header = 0;
    p_sys->p_header = NULL;
    for( ;; )
    {
        chunk_t ck;
        if( GetPacket( p_input, &ck ) ||
            ck.i_type != 0x4824 )
        {
            break;
        }

        if( ck.i_data > 0 )
        {
            p_sys->i_header += ck.i_data;
            p_sys->p_header = realloc( p_sys->p_header, p_sys->i_header );
            memcpy( &p_sys->p_header[p_sys->i_header - ck.i_data],
                    ck.p_data, ck.i_data );
        }
    }
    msg_Dbg( p_input, "complete header size=%d", p_sys->i_header );
    if( p_sys->i_header <= 0 )
    {
        msg_Err( p_input, "header size == 0" );
        goto error;
    }
    /* close this connection */
    net_Close( p_sys->fd ); p_sys->fd = -1;

    /* *** parse header and get stream and their id *** */
    /* get all streams properties,
     *
     * TODO : stream bitrates properties(optional)
     *        and bitrate mutual exclusion(optional) */
    E_( asf_HeaderParse )( &p_sys->asfh,
                           p_sys->p_header, p_sys->i_header );
    msg_Dbg( p_input, "packet count=%lld packet size=%d",
             p_sys->asfh.i_data_packets_count,
             p_sys->asfh.i_min_data_packet_size );

    E_( asf_StreamSelect)( &p_sys->asfh,
                           config_GetInt( p_input, "mms-maxbitrate" ),
                           config_GetInt( p_input, "mms-all" ),
                           config_GetInt( p_input, "audio" ),
                           config_GetInt( p_input, "video" ) );

    return VLC_SUCCESS;

error:
    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd  );
        p_sys->fd = -1;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Start( input_thread_t *p_input, off_t i_pos )
{
    access_sys_t *p_sys = p_input->p_access_data;
    int  i_streams = 0;
    int  i;
    char *psz;

    msg_Dbg( p_input, "starting stream" );

    if( ( p_sys->fd = net_OpenTCP( p_input, p_sys->url.psz_host,
                                            p_sys->url.i_port ) ) < 0 )
    {
        /* should not occur */
        msg_Err( p_input, "cannot connect to the server" );
        return VLC_EGENERIC;
    }

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
    net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                "GET %s HTTP/1.0\r\n"
                "Accept: */*\r\n"
                "User-Agent: NSPlayer/4.1.0.3856\r\n"
                "Host: %s:%d\r\n",
                ( p_sys->url.psz_path == NULL || *p_sys->url.psz_path == '\0' ) ? "/" : p_sys->url.psz_path,
                p_sys->url.psz_host, p_sys->url.i_port );
    if( p_sys->b_broadcast )
    {
        net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                    "Pragma: no-cache,rate=1.000000,request-context=%d\r\n",
                    p_sys->i_request_context++ );
    }
    else
    {
        net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                    "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=0\r\n",
                    (uint32_t)((i_pos >> 32)&0xffffffff),
                    (uint32_t)(i_pos&0xffffffff),
                    p_sys->i_request_context++ );
    }
    net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                "Pragma: xPlayStrm=1\r\n"
                "Pragma: xClientGUID={"GUID_FMT"}\r\n"
                "Pragma: stream-switch-count=%d\r\n"
                "Pragma: stream-switch-entry=",
                GUID_PRINT( p_sys->guid ),
                i_streams);

    for( i = 1; i < 128; i++ )
    {
        if( p_sys->asfh.stream[i].i_cat != ASF_STREAM_UNKNOWN )
        {
            int i_select = 2;
            if( p_sys->asfh.stream[i].i_selected )
            {
                i_select = 0;
            }

            net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                        "ffff:%d:%d ", i, i_select );
        }
    }
    net_Printf( VLC_OBJECT(p_input), p_sys->fd, "\r\n" );
    net_Printf( VLC_OBJECT(p_input), p_sys->fd, "Connection: Close\r\n" );

    if( net_Printf( VLC_OBJECT(p_input), p_sys->fd, "\r\n" ) < 0 )
    {
        msg_Err( p_input, "failed to send request" );
        return VLC_EGENERIC;
    }

    if( ( psz = net_Gets( VLC_OBJECT(p_input), p_sys->fd ) ) == NULL )
    {
        msg_Err( p_input, "cannot read data" );
        return VLC_EGENERIC;
    }
    if( atoi( &psz[9] ) >= 400 )
    {
        msg_Err( p_input, "error: %s", psz );
        free( psz );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_input, "HTTP reply '%s'", psz );
    free( psz );

    /* FIXME check HTTP code */
    for( ;; )
    {
        char *psz = net_Gets( p_input, p_sys->fd );
        if( psz == NULL )
        {
            msg_Err( p_input, "cannot read data" );
            return VLC_EGENERIC;
        }
        if( *psz == '\0' )
        {
            free( psz );
            break;
        }
        msg_Dbg( p_input, "%s", psz );
        free( psz );
    }

    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Stop( input_thread_t *p_input )
{
    access_sys_t *p_sys = p_input->p_access_data;

    msg_Dbg( p_input, "closing stream" );
    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd );
        p_sys->fd = -1;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static int GetPacket( input_thread_t * p_input, chunk_t *p_ck )
{
    access_sys_t *p_sys = p_input->p_access_data;

    /* chunk_t */
    memset( p_ck, 0, sizeof( chunk_t ) );

    /* Read the chunk header */
    if( net_Read( p_input, p_sys->fd, p_sys->buffer, 12, VLC_TRUE ) < 12 )
    {
        /* msg_Err( p_input, "cannot read data" ); */
        return VLC_EGENERIC;
    }

    p_ck->i_type      = GetWLE( p_sys->buffer);
    p_ck->i_size      = GetWLE( p_sys->buffer + 2);
    p_ck->i_sequence  = GetDWLE( p_sys->buffer + 4);
    p_ck->i_unknown   = GetWLE( p_sys->buffer + 8);
    p_ck->i_size2     = GetWLE( p_sys->buffer + 10);
    p_ck->p_data      = p_sys->buffer + 12;
    p_ck->i_data      = p_ck->i_size2 - 8;

    if( p_ck->i_type == 0x4524 )   // Transfer complete
    {
        if( p_ck->i_sequence == 0 )
        {
            msg_Warn( p_input, "EOF" );
            return VLC_EGENERIC;
        }
        else
        {
            msg_Warn( p_input, "Next stream follow but not supported" );
            return VLC_EGENERIC;
        }
    }
    else if( p_ck->i_type != 0x4824 && p_ck->i_type != 0x4424 )
    {
        msg_Err( p_input, "invalid chunk FATAL (0x%x)", p_ck->i_type );
        return VLC_EGENERIC;
    }

    if( p_ck->i_data > 0 &&
        net_Read( p_input, p_sys->fd, &p_sys->buffer[12], p_ck->i_data, VLC_TRUE ) < p_ck->i_data )
    {
        msg_Err( p_input, "cannot read data" );
        return VLC_EGENERIC;
    }

    if( p_sys->i_packet_sequence != 0 &&
        p_ck->i_sequence != p_sys->i_packet_sequence )
    {
        msg_Warn( p_input, "packet lost ? (%d != %d)", p_ck->i_sequence, p_sys->i_packet_sequence );
    }

    p_sys->i_packet_sequence = p_ck->i_sequence + 1;
    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = p_ck->i_data;
    p_sys->p_packet        = p_ck->p_data;

    return VLC_SUCCESS;
}
