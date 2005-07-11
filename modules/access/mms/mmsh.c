/*****************************************************************************
 * mmsh.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
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
int  E_(MMSHOpen)  ( access_t * );
void E_(MMSHClose) ( access_t * );

static int  Read( access_t *, uint8_t *, int );
static int  ReadRedirect( access_t *, uint8_t *, int );
static int  Seek( access_t *, int64_t );
static int  Control( access_t *, int, va_list );

static int  Describe( access_t  *, char **ppsz_location );
static int  Start( access_t *, int64_t );
static void Stop( access_t * );
static int  GetPacket( access_t *, chunk_t * );

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
int E_(MMSHOpen)( access_t *p_access )
{
    access_sys_t    *p_sys;
    char            *psz_location = NULL;

    /* init p_sys */

    /* Set up p_access */
    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->i_proto= MMS_PROTO_HTTP;
    p_sys->fd     = -1;
    p_sys->i_start= 0;

    /* open a tcp connection */
    vlc_UrlParse( &p_sys->url, p_access->psz_path, 0 );
    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Err( p_access, "invalid host" );
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( p_sys->url.i_port <= 0 )
        p_sys->url.i_port = 80;

    if( Describe( p_access, &psz_location ) )
    {
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }
    /* Handle redirection */
    if( psz_location && *psz_location )
    {
        playlist_t * p_playlist = vlc_object_find( p_access, VLC_OBJECT_PLAYLIST, FIND_PARENT );

        msg_Dbg( p_access, "redirection to %s", psz_location );

        if( !p_playlist )
        {
            msg_Err( p_access, "redirection failed: can't find playlist" );
            free( psz_location );
            return VLC_EGENERIC;
        }
        p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
        playlist_Add( p_playlist, psz_location, psz_location,
                      PLAYLIST_INSERT | PLAYLIST_GO,
                      p_playlist->i_index + 1 );
        vlc_object_release( p_playlist );

        free( psz_location );

        p_access->pf_read = ReadRedirect;
        return VLC_SUCCESS;
    }

    /* Start playing */
    if( Start( p_access, 0 ) )
    {
        msg_Err( p_access, "cannot start stream" );
        free( p_sys->p_header );
        vlc_UrlClean( &p_sys->url );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( !p_sys->b_broadcast )
    {
        p_access->info.i_size = p_sys->asfh.i_file_size;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
void E_( MMSHClose )( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    Stop( p_access );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    int          i_int;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = !p_sys->b_broadcast;
            break;

        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;

        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );

#if 0       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb_bool = VLC_FALSE;
#endif
            *pb_bool = VLC_TRUE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 3 * p_sys->asfh.i_min_data_packet_size;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "mms-caching" ) * I64C(1000);
            break;

        case ACCESS_GET_PRIVATE_ID_STATE:
            i_int = (int)va_arg( args, int );
            pb_bool = (vlc_bool_t *)va_arg( args, vlc_bool_t * );

            if( i_int < 0 || i_int > 127 )
                return VLC_EGENERIC;
            *pb_bool =  p_sys->asfh.stream[i_int].i_selected ? VLC_TRUE : VLC_FALSE;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    chunk_t      ck;
    off_t        i_offset;
    off_t        i_packet;

    msg_Dbg( p_access, "seeking to "I64Fd, i_pos );

    i_packet = ( i_pos - p_sys->i_header ) / p_sys->asfh.i_min_data_packet_size;
    i_offset = ( i_pos - p_sys->i_header ) % p_sys->asfh.i_min_data_packet_size;

    Stop( p_access );
    Start( p_access, i_packet * p_sys->asfh.i_min_data_packet_size );

    while( !p_access->b_die )
    {
        if( GetPacket( p_access, &ck ) )
            break;

        /* skip headers */
        if( ck.i_type != 0x4824 )
            break;

        msg_Warn( p_access, "skipping header" );
    }

    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = VLC_FALSE;
    p_sys->i_packet_used += i_offset;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static int ReadRedirect( access_t *p_access, uint8_t *p, int i_len )
{
    return 0;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    size_t       i_copy;
    size_t       i_data = 0;

    if( p_access->info.b_eof )
        return 0;

    while( i_data < i_len )
    {
        if( p_access->info.i_pos < p_sys->i_start + p_sys->i_header )
        {
            int i_offset = p_access->info.i_pos - p_sys->i_start;
            i_copy = __MIN( p_sys->i_header - i_offset, i_len - i_data );
            memcpy( &p_buffer[i_data], &p_sys->p_header[i_offset], i_copy );

            i_data += i_copy;
            p_access->info.i_pos += i_copy;
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
            p_access->info.i_pos += i_copy;
        }
        else if( p_sys->i_packet_length > 0 &&
                 (int)p_sys->i_packet_used < p_sys->asfh.i_min_data_packet_size )
        {
            i_copy = __MIN( p_sys->asfh.i_min_data_packet_size - p_sys->i_packet_used,
                            i_len - i_data );

            memset( &p_buffer[i_data], 0, i_copy );

            i_data += i_copy;
            p_sys->i_packet_used += i_copy;
            p_access->info.i_pos += i_copy;
        }
        else
        {
            chunk_t ck;
            if( GetPacket( p_access, &ck ) )
            {
                if( ck.i_type == 0x4524 && ck.i_sequence != 0 && p_sys->b_broadcast )
                {
                    char *psz_location = NULL;

                    p_sys->i_start = p_access->info.i_pos;

                    msg_Dbg( p_access, "stoping the stream" );
                    Stop( p_access );

                    msg_Dbg( p_access, "describe the stream" );
                    if( Describe( p_access, &psz_location ) )
                    {
                        msg_Err( p_access, "describe failed" );
                        p_access->info.b_eof = VLC_TRUE;
                        return 0;
                    }
                    if( Start( p_access, 0 ) )
                    {
                        msg_Err( p_access, "Start failed" );
                        p_access->info.b_eof = VLC_TRUE;
                        return 0;
                    }
                }
                else
                {
                    p_access->info.b_eof = VLC_TRUE;
                    return 0;
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
static int Describe( access_t  *p_access, char **ppsz_location )
{
    access_sys_t *p_sys = p_access->p_sys;
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

    if( ( p_sys->fd = net_OpenTCP( p_access, p_sys->url.psz_host,
                                            p_sys->url.i_port ) ) < 0 )
    {
        msg_Err( p_access, "cannot connect to %s:%d", p_sys->url.psz_host, p_sys->url.i_port );
        goto error;
    }

    /* send first request */
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
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

    if( net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        goto error;
    }

    /* Receive the http header */
    if( ( psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL ) ) == NULL )
    {
        msg_Err( p_access, "failed to read answer" );
        goto error;
    }
    if( strncmp( psz, "HTTP/1.", 7 ) )
    {
        msg_Err( p_access, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
    }
    i_code = atoi( &psz[9] );
    if( i_code >= 400 )
    {
        msg_Err( p_access, "error: %s", psz );
        free( psz );
        goto error;
    }

    msg_Dbg( p_access, "HTTP reply '%s'", psz );
    free( psz );
    for( ;; )
    {
        char *psz = net_Gets( p_access, p_sys->fd, NULL );
        char *p;

        if( psz == NULL )
        {
            msg_Err( p_access, "failed to read answer" );
            goto error;
        }

        if( *psz == '\0' )
        {
            free( psz );
            break;
        }

        if( ( p = strchr( psz, ':' ) ) == NULL )
        {
            msg_Err( p_access, "malformed header line: %s", psz );
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
                    msg_Dbg( p_access, "stream type = broadcast" );
                    p_sys->b_broadcast = VLC_TRUE;
                }
                else if( strstr( p, "seekable" ) )
                {
                    msg_Dbg( p_access, "stream type = seekable" );
                    p_sys->b_broadcast = VLC_FALSE;
                }
                else
                {
                    msg_Warn( p_access, "unknow stream types (%s)", p );
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
        msg_Dbg( p_access, "redirection to %s", psz_location );
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
        if( GetPacket( p_access, &ck ) ||
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
    msg_Dbg( p_access, "complete header size=%d", p_sys->i_header );
    if( p_sys->i_header <= 0 )
    {
        msg_Err( p_access, "header size == 0" );
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
    msg_Dbg( p_access, "packet count="I64Fd" packet size=%d",
             p_sys->asfh.i_data_packets_count,
             p_sys->asfh.i_min_data_packet_size );

    E_( asf_StreamSelect)( &p_sys->asfh,
                           var_CreateGetInteger( p_access, "mms-maxbitrate" ),
                           var_CreateGetInteger( p_access, "mms-all" ),
                           var_CreateGetInteger( p_access, "audio" ),
                           var_CreateGetInteger( p_access, "video" ) );

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
static int Start( access_t *p_access, off_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int  i_streams = 0;
    int  i;
    char *psz;

    msg_Dbg( p_access, "starting stream" );

    if( ( p_sys->fd = net_OpenTCP( p_access, p_sys->url.psz_host,
                                            p_sys->url.i_port ) ) < 0 )
    {
        /* should not occur */
        msg_Err( p_access, "cannot connect to the server" );
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
        msg_Err( p_access, "no stream selected" );
        return VLC_EGENERIC;
    }
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                "GET %s HTTP/1.0\r\n"
                "Accept: */*\r\n"
                "User-Agent: NSPlayer/4.1.0.3856\r\n"
                "Host: %s:%d\r\n",
                ( p_sys->url.psz_path == NULL || *p_sys->url.psz_path == '\0' ) ? "/" : p_sys->url.psz_path,
                p_sys->url.psz_host, p_sys->url.i_port );
    if( p_sys->b_broadcast )
    {
        net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                    "Pragma: no-cache,rate=1.000000,request-context=%d\r\n",
                    p_sys->i_request_context++ );
    }
    else
    {
        net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                    "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=0\r\n",
                    (uint32_t)((i_pos >> 32)&0xffffffff),
                    (uint32_t)(i_pos&0xffffffff),
                    p_sys->i_request_context++ );
    }
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
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

            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "ffff:%d:%d ", i, i_select );
        }
    }
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL, "\r\n" );
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                "Connection: Close\r\n" );

    if( net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        return VLC_EGENERIC;
    }

    if( ( psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL ) ) == NULL )
    {
        msg_Err( p_access, "cannot read data" );
        return VLC_EGENERIC;
    }
    if( atoi( &psz[9] ) >= 400 )
    {
        msg_Err( p_access, "error: %s", psz );
        free( psz );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "HTTP reply '%s'", psz );
    free( psz );

    /* FIXME check HTTP code */
    for( ;; )
    {
        char *psz = net_Gets( p_access, p_sys->fd, NULL );
        if( psz == NULL )
        {
            msg_Err( p_access, "cannot read data" );
            return VLC_EGENERIC;
        }
        if( *psz == '\0' )
        {
            free( psz );
            break;
        }
        msg_Dbg( p_access, "%s", psz );
        free( psz );
    }

    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Stop( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "closing stream" );
    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd );
        p_sys->fd = -1;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static int GetPacket( access_t * p_access, chunk_t *p_ck )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* chunk_t */
    memset( p_ck, 0, sizeof( chunk_t ) );

    /* Read the chunk header */
    if( net_Read( p_access, p_sys->fd, NULL, p_sys->buffer, 12, VLC_TRUE ) < 12 )
    {
        /* msg_Err( p_access, "cannot read data" ); */
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
            msg_Warn( p_access, "EOF" );
            return VLC_EGENERIC;
        }
        else
        {
            msg_Warn( p_access, "Next stream follow but not supported" );
            return VLC_EGENERIC;
        }
    }
    else if( p_ck->i_type != 0x4824 && p_ck->i_type != 0x4424 )
    {
        msg_Err( p_access, "invalid chunk FATAL (0x%x)", p_ck->i_type );
        return VLC_EGENERIC;
    }

    if( p_ck->i_data > 0 &&
        net_Read( p_access, p_sys->fd, NULL, &p_sys->buffer[12], p_ck->i_data, VLC_TRUE ) < p_ck->i_data )
    {
        msg_Err( p_access, "cannot read data" );
        return VLC_EGENERIC;
    }

    if( p_sys->i_packet_sequence != 0 &&
        p_ck->i_sequence != p_sys->i_packet_sequence )
    {
        msg_Warn( p_access, "packet lost ? (%d != %d)", p_ck->i_sequence, p_sys->i_packet_sequence );
    }

    p_sys->i_packet_sequence = p_ck->i_sequence + 1;
    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = p_ck->i_data;
    p_sys->p_packet        = p_ck->p_data;

    return VLC_SUCCESS;
}
