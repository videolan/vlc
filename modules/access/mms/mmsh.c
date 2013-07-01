/*****************************************************************************
 * mmsh.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_strings.h>
#include <vlc_input.h>

#include <vlc_network.h>
#include <vlc_url.h>
#include "asf.h"
#include "buffer.h"

#include "mms.h"
#include "mmsh.h"

/* TODO:
 *  - authentication
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  MMSHOpen  ( access_t * );
void MMSHClose ( access_t * );

static block_t *Block( access_t *p_access );
static ssize_t ReadRedirect( access_t *, uint8_t *, size_t );
static int  Seek( access_t *, uint64_t );
static int  Control( access_t *, int, va_list );

static int  Describe( access_t  *, char **ppsz_location );
static int  Start( access_t *, uint64_t );
static void Stop( access_t * );

static int  GetPacket( access_t *, chunk_t * );
static void GetHeader( access_t *p_access, int i_content_length );

static int Restart( access_t * );
static int Reset( access_t * );

//#define MMSH_USER_AGENT "NSPlayer/4.1.0.3856"
#define MMSH_USER_AGENT "NSPlayer/7.10.0.3059"

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
int MMSHOpen( access_t *p_access )
{
    access_sys_t    *p_sys;
    char            *psz_location = NULL;
    char            *psz_proxy;

    STANDARD_BLOCK_ACCESS_INIT

    p_sys->i_proto= MMS_PROTO_HTTP;
    p_sys->fd     = -1;

    /* Handle proxy */
    p_sys->b_proxy = false;
    memset( &p_sys->proxy, 0, sizeof(p_sys->proxy) );

    /* Check proxy */
    /* TODO reuse instead http-proxy from http access ? */
    psz_proxy = var_CreateGetNonEmptyString( p_access, "mmsh-proxy" );
    if( !psz_proxy )
    {
        char *psz_http_proxy = var_InheritString( p_access, "http-proxy" );
        if( psz_http_proxy )
        {
            psz_proxy = psz_http_proxy;
            var_SetString( p_access, "mmsh-proxy", psz_proxy );
        }
    }

    if( psz_proxy )
    {
        p_sys->b_proxy = true;
        vlc_UrlParse( &p_sys->proxy, psz_proxy, 0 );
        free( psz_proxy );
    }
    else
    {
        const char *http_proxy = getenv( "http_proxy" );
        if( http_proxy )
        {
            p_sys->b_proxy = true;
            vlc_UrlParse( &p_sys->proxy, http_proxy, 0 );
        }
    }

    if( p_sys->b_proxy )
    {
        if( ( p_sys->proxy.psz_host == NULL ) ||
            ( *p_sys->proxy.psz_host == '\0' ) )
        {
            msg_Warn( p_access, "invalid proxy host" );
            vlc_UrlClean( &p_sys->proxy );
            free( p_sys );
            return VLC_EGENERIC;
        }

        if( p_sys->proxy.i_port <= 0 )
            p_sys->proxy.i_port = 80;
        msg_Dbg( p_access, "Using http proxy %s:%d",
                 p_sys->proxy.psz_host, p_sys->proxy.i_port );
    }

    /* open a tcp connection */
    vlc_UrlParse( &p_sys->url, p_access->psz_location, 0 );
    if( ( p_sys->url.psz_host == NULL ) ||
        ( *p_sys->url.psz_host == '\0' ) )
    {
        msg_Err( p_access, "invalid host" );
        goto error;
    }
    if( p_sys->url.i_port <= 0 )
        p_sys->url.i_port = 80;

    if( Describe( p_access, &psz_location ) )
        goto error;

    /* Handle redirection */
    if( psz_location && *psz_location )
    {
        msg_Dbg( p_access, "redirection to %s", psz_location );

        input_thread_t * p_input = access_GetParentInput( p_access );
        input_item_t * p_new_loc;

        if( !p_input )
        {
            free( psz_location );
            goto error;
        }
        /** \bug we do not autodelete here */
        p_new_loc = input_item_New( psz_location, psz_location );
        input_item_t *p_item = input_GetItem( p_input );
        input_item_PostSubItem( p_item, p_new_loc );

        vlc_gc_decref( p_new_loc );
        vlc_object_release( p_input );

        free( psz_location );

        p_access->pf_block = NULL;
        p_access->pf_read = ReadRedirect;
        return VLC_SUCCESS;
    }
    free( psz_location );

    /* Start playing */
    if( Start( p_access, 0 ) )
    {
        msg_Err( p_access, "cannot start stream" );
        free( p_sys->p_header );
        goto error;
    }

    if( !p_sys->b_broadcast )
    {
        p_access->info.i_size = p_sys->asfh.i_file_size;
    }

    return VLC_SUCCESS;

error:
    vlc_UrlClean( &p_sys->proxy );
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
void  MMSHClose ( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    Stop( p_access );

    free( p_sys->p_header );

    vlc_UrlClean( &p_sys->proxy );
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool   *pb_bool;
    bool    b_bool;
    int64_t      *pi_64;
    int          i_int;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = !p_sys->b_broadcast;
            break;

        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger( p_access, "network-caching" );
            break;

        case ACCESS_GET_PRIVATE_ID_STATE:
            i_int = (int)va_arg( args, int );
            pb_bool = (bool *)va_arg( args, bool * );

            if( (i_int < 0) || (i_int > 127) )
                return VLC_EGENERIC;
            *pb_bool =  p_sys->asfh.stream[i_int].i_selected ? true : false;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            b_bool = (bool)va_arg( args, int );
            if( b_bool )
                Stop( p_access );
            else
                Seek( p_access, p_access->info.i_pos );
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
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
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    chunk_t      ck;
    uint64_t     i_offset;
    uint64_t     i_packet;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    i_packet = ( i_pos - p_sys->i_header ) / p_sys->asfh.i_min_data_packet_size;
    i_offset = ( i_pos - p_sys->i_header ) % p_sys->asfh.i_min_data_packet_size;

    Stop( p_access );
    Start( p_access, i_packet * p_sys->asfh.i_min_data_packet_size );

    while( vlc_object_alive (p_access) )
    {
        if( GetPacket( p_access, &ck ) )
            break;

        /* skip headers */
        if( ck.i_type != 0x4824 )
            break;

        msg_Warn( p_access, "skipping header" );
    }

    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;
    p_sys->i_packet_used += i_offset;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ReadRedirect:
 *****************************************************************************/
static ssize_t ReadRedirect( access_t *p_access, uint8_t *p, size_t i_len )
{
    VLC_UNUSED(p_access); VLC_UNUSED(p); VLC_UNUSED(i_len);
    return 0;
}

/*****************************************************************************
 * Block:
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    const unsigned i_packet_min = p_sys->asfh.i_min_data_packet_size;

    if( p_access->info.i_pos < p_sys->i_start + p_sys->i_header )
    {
        const size_t i_offset = p_access->info.i_pos - p_sys->i_start;
        const size_t i_copy = p_sys->i_header - i_offset;

        block_t *p_block = block_Alloc( i_copy );
        if( !p_block )
            return NULL;

        memcpy( p_block->p_buffer, &p_sys->p_header[i_offset], i_copy );
        p_access->info.i_pos += i_copy;
        return p_block;
    }
    else if( p_sys->i_packet_length > 0 &&
             p_sys->i_packet_used < __MAX( p_sys->i_packet_length, i_packet_min ) )
    {
        size_t i_copy = 0;
        size_t i_padding = 0;

        if( p_sys->i_packet_used < p_sys->i_packet_length )
            i_copy = p_sys->i_packet_length - p_sys->i_packet_used;
        if( __MAX( p_sys->i_packet_used, p_sys->i_packet_length ) < i_packet_min )
            i_padding = i_packet_min - __MAX( p_sys->i_packet_used, p_sys->i_packet_length );

        block_t *p_block = block_Alloc( i_copy + i_padding );
        if( !p_block )
            return NULL;

        if( i_copy > 0 )
            memcpy( &p_block->p_buffer[0], &p_sys->p_packet[p_sys->i_packet_used], i_copy );
        if( i_padding > 0 )
            memset( &p_block->p_buffer[i_copy], 0, i_padding );

        p_sys->i_packet_used += i_copy + i_padding;
        p_access->info.i_pos += i_copy + i_padding;
        return p_block;

    }

    chunk_t ck;
    if( GetPacket( p_access, &ck ) )
    {
        int i_ret = -1;
        if( p_sys->b_broadcast )
        {
            if( (ck.i_type == 0x4524) && (ck.i_sequence != 0) )
                i_ret = Restart( p_access );
            else if( ck.i_type == 0x4324 )
                i_ret = Reset( p_access );
        }
        if( i_ret )
        {
            p_access->info.b_eof = true;
            return 0;
        }
    }
    if( ck.i_type != 0x4424 )
    {
        p_sys->i_packet_used = 0;
        p_sys->i_packet_length = 0;
    }

    return NULL;
}

/* */
static int Restart( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char *psz_location = NULL;

    msg_Dbg( p_access, "Restart the stream" );
    p_sys->i_start = p_access->info.i_pos;

    /* */
    msg_Dbg( p_access, "stoping the stream" );
    Stop( p_access );

    /* */
    msg_Dbg( p_access, "describe the stream" );
    if( Describe( p_access, &psz_location ) )
    {
        msg_Err( p_access, "describe failed" );
        return VLC_EGENERIC;
    }
    free( psz_location );

    /* */
    if( Start( p_access, 0 ) )
    {
        msg_Err( p_access, "Start failed" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
static int Reset( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    asf_header_t old_asfh = p_sys->asfh;
    int i;

    msg_Dbg( p_access, "Reset the stream" );
    p_sys->i_start = p_access->info.i_pos;

    /* */
    p_sys->i_packet_sequence = 0;
    p_sys->i_packet_used = 0;
    p_sys->i_packet_length = 0;
    p_sys->p_packet = NULL;

    /* Get the next header FIXME memory loss ? */
    GetHeader( p_access, -1 );
    if( p_sys->i_header <= 0 )
        return VLC_EGENERIC;

    asf_HeaderParse ( &p_sys->asfh,
                       p_sys->p_header, p_sys->i_header );
    msg_Dbg( p_access, "packet count=%"PRId64" packet size=%d",
             p_sys->asfh.i_data_packets_count,
             p_sys->asfh.i_min_data_packet_size );

    asf_StreamSelect( &p_sys->asfh,
                       var_InheritInteger( p_access, "mms-maxbitrate" ),
                       var_InheritBool( p_access, "mms-all" ),
                       var_InheritBool( p_access, "audio" ),
                       var_InheritBool( p_access, "video" ) );

    /* Check we have comptible asfh */
    for( i = 1; i < 128; i++ )
    {
        asf_stream_t *p_old = &old_asfh.stream[i];
        asf_stream_t *p_new = &p_sys->asfh.stream[i];

        if( p_old->i_cat != p_new->i_cat || p_old->i_selected != p_new->i_selected )
            break;
    }
    if( i < 128 )
    {
        msg_Warn( p_access, "incompatible asf header, restart" );
        return Restart( p_access );
    }

    /* */
    p_sys->i_packet_used = 0;
    p_sys->i_packet_length = 0;
    return VLC_SUCCESS;
}

static int OpenConnection( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_url_t    srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;

    if( ( p_sys->fd = net_ConnectTCP( p_access,
                                      srv.psz_host, srv.i_port ) ) < 0 )
    {
        msg_Err( p_access, "cannot connect to %s:%d",
                 srv.psz_host, srv.i_port );
        return VLC_EGENERIC;
    }

    if( p_sys->b_proxy )
    {
        net_Printf( p_access, p_sys->fd, NULL,
                    "GET http://%s:%d%s HTTP/1.0\r\n",
                    p_sys->url.psz_host, p_sys->url.i_port,
                    ( (p_sys->url.psz_path == NULL) ||
                      (*p_sys->url.psz_path == '\0') ) ?
                         "/" : p_sys->url.psz_path );

        /* Proxy Authentication */
        if( p_sys->proxy.psz_username && *p_sys->proxy.psz_username )
        {
            char *buf;
            char *b64;

            if( asprintf( &buf, "%s:%s", p_sys->proxy.psz_username,
                       p_sys->proxy.psz_password ? p_sys->proxy.psz_password : "" ) == -1 )
                return VLC_ENOMEM;

            b64 = vlc_b64_encode( buf );
            free( buf );

            net_Printf( p_access, p_sys->fd, NULL,
                        "Proxy-Authorization: Basic %s\r\n", b64 );
            free( b64 );
        }
    }
    else
    {
        net_Printf( p_access, p_sys->fd, NULL,
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s:%d\r\n",
                    ( (p_sys->url.psz_path == NULL) ||
                      (*p_sys->url.psz_path == '\0') ) ?
                            "/" : p_sys->url.psz_path,
                    p_sys->url.psz_host, p_sys->url.i_port );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Describe:
 *****************************************************************************/
static int Describe( access_t  *p_access, char **ppsz_location )
{
    access_sys_t *p_sys = p_access->p_sys;
    char         *psz_location = NULL;
    int          i_content_length = -1;
    bool         b_keepalive = false;
    char         *psz;
    int          i_code;

    /* Reinit context */
    p_sys->b_broadcast = true;
    p_sys->i_request_context = 1;
    p_sys->i_packet_sequence = 0;
    p_sys->i_packet_used = 0;
    p_sys->i_packet_length = 0;
    p_sys->p_packet = NULL;

    GenerateGuid ( &p_sys->guid );

    if( OpenConnection( p_access ) )
        return VLC_EGENERIC;

    net_Printf( p_access, p_sys->fd, NULL,
                "Accept: */*\r\n"
                "User-Agent: "MMSH_USER_AGENT"\r\n"
                "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=%d,max-duration=0\r\n"
                "Pragma: xClientGUID={"GUID_FMT"}\r\n"
                "Connection: Close\r\n",
                p_sys->i_request_context++,
                GUID_PRINT( p_sys->guid ) );

    if( net_Printf( p_access, p_sys->fd, NULL, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        goto error;
    }

    /* Receive the http header */
    if( ( psz = net_Gets( p_access, p_sys->fd, NULL ) ) == NULL )
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
                    p_sys->b_broadcast = true;
                }
                else if( strstr( p, "seekable" ) )
                {
                    msg_Dbg( p_access, "stream type = seekable" );
                    p_sys->b_broadcast = false;
                }
                else
                {
                    msg_Warn( p_access, "unknow stream types (%s)", p );
                    p_sys->b_broadcast = false;
                }
            }
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            psz_location = strdup( p );
        }
        else if( !strcasecmp( psz, "Content-Length" ) )
        {
            i_content_length = atoi( p );
            msg_Dbg( p_access, "content-length = %d", i_content_length );
        }
        else if( !strcasecmp( psz, "Connection" ) )
        {
            if( strcasestr( p, "Keep-Alive" ) )
            {
                msg_Dbg( p_access, "Keep-Alive header found" );
                b_keepalive = true;
            }
        }


        free( psz );
    }

    /* Handle the redirection */
    if( ( (i_code == 301) || (i_code == 302) ||
          (i_code == 303) || (i_code == 307) ) &&
        psz_location && *psz_location )
    {
        msg_Dbg( p_access, "redirection to %s", psz_location );
        net_Close( p_sys->fd ); p_sys->fd = -1;

        *ppsz_location = psz_location;
        return VLC_SUCCESS;
    }
    free( psz_location );

    /* Read the asf header */
    GetHeader( p_access, b_keepalive ? i_content_length : -1);
    if( p_sys->i_header <= 0 )
    {
        msg_Err( p_access, "header size == 0" );
        goto error;
    }
    /* close this connection */
    net_Close( p_sys->fd );
    p_sys->fd = -1;

    /* *** parse header and get stream and their id *** */
    /* get all streams properties,
     *
     * TODO : stream bitrates properties(optional)
     *        and bitrate mutual exclusion(optional) */
    asf_HeaderParse ( &p_sys->asfh,
                       p_sys->p_header, p_sys->i_header );
    msg_Dbg( p_access, "packet count=%"PRId64" packet size=%d",
             p_sys->asfh.i_data_packets_count,
             p_sys->asfh.i_min_data_packet_size );

    if( p_sys->asfh.i_min_data_packet_size <= 0 )
        goto error;

    asf_StreamSelect( &p_sys->asfh,
                       var_InheritInteger( p_access, "mms-maxbitrate" ),
                       var_InheritBool( p_access, "mms-all" ),
                       var_InheritBool( p_access, "audio" ),
                       var_InheritBool( p_access, "video" ) );
    return VLC_SUCCESS;

error:
    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd  );
        p_sys->fd = -1;
    }
    return VLC_EGENERIC;
}

static void GetHeader( access_t *p_access, int i_content_length )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read_content = 0;

    /* Read the asf header */
    p_sys->i_header = 0;
    free( p_sys->p_header  );
    p_sys->p_header = NULL;
    for( ;; )
    {
        chunk_t ck;
        if( (i_content_length >= 0 && i_read_content >= i_content_length) || GetPacket( p_access, &ck ) || ck.i_type != 0x4824 )
            break;

        i_read_content += (4+ck.i_size);

        if( ck.i_data > 0 )
        {
            p_sys->i_header += ck.i_data;
            p_sys->p_header = xrealloc( p_sys->p_header, p_sys->i_header );
            memcpy( &p_sys->p_header[p_sys->i_header - ck.i_data],
                    ck.p_data, ck.i_data );
        }
    }
    msg_Dbg( p_access, "complete header size=%d", p_sys->i_header );
}


/*****************************************************************************
 * Start stream
 ****************************************************************************/
static int Start( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int  i_streams = 0;
    int  i_streams_selected = 0;
    int  i;
    char *psz = NULL;

    msg_Dbg( p_access, "starting stream" );

    for( i = 1; i < 128; i++ )
    {
        if( p_sys->asfh.stream[i].i_cat == ASF_CODEC_TYPE_UNKNOWN )
            continue;
        i_streams++;
        if( p_sys->asfh.stream[i].i_selected )
            i_streams_selected++;
    }
    if( i_streams_selected <= 0 )
    {
        msg_Err( p_access, "no stream selected" );
        return VLC_EGENERIC;
    }

    if( OpenConnection( p_access ) )
        return VLC_EGENERIC;

    net_Printf( p_access, p_sys->fd, NULL,
                "Accept: */*\r\n"
                "User-Agent: "MMSH_USER_AGENT"\r\n" );
    if( p_sys->b_broadcast )
    {
        net_Printf( p_access, p_sys->fd, NULL,
                    "Pragma: no-cache,rate=1.000000,request-context=%d\r\n",
                    p_sys->i_request_context++ );
    }
    else
    {
        net_Printf( p_access, p_sys->fd, NULL,
                    "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=0\r\n",
                    (uint32_t)((i_pos >> 32)&0xffffffff),
                    (uint32_t)(i_pos&0xffffffff),
                    p_sys->i_request_context++ );
    }
    net_Printf( p_access, p_sys->fd, NULL,
                "Pragma: xPlayStrm=1\r\n"
                "Pragma: xClientGUID={"GUID_FMT"}\r\n"
                "Pragma: stream-switch-count=%d\r\n"
                "Pragma: stream-switch-entry=",
                GUID_PRINT( p_sys->guid ),
                i_streams);

    for( i = 1; i < 128; i++ )
    {
        if( p_sys->asfh.stream[i].i_cat != ASF_CODEC_TYPE_UNKNOWN )
        {
            int i_select = 2;
            if( p_sys->asfh.stream[i].i_selected )
            {
                i_select = 0;
            }
            net_Printf( p_access, p_sys->fd, NULL,
                        "ffff:%x:%d ", i, i_select );
        }
    }
    net_Printf( p_access, p_sys->fd, NULL, "\r\n" );
    net_Printf( p_access, p_sys->fd, NULL,
                "Connection: Close\r\n" );

    if( net_Printf( p_access, p_sys->fd, NULL, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        return VLC_EGENERIC;
    }

    psz = net_Gets( p_access, p_sys->fd, NULL );
    if( psz == NULL )
    {
        msg_Err( p_access, "cannot read data 0" );
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
            msg_Err( p_access, "cannot read data 1" );
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
 * closing stream
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
 * get packet
 *****************************************************************************/
static int GetPacket( access_t * p_access, chunk_t *p_ck )
{
    access_sys_t *p_sys = p_access->p_sys;
    int restsize;

    /* chunk_t */
    memset( p_ck, 0, sizeof( chunk_t ) );

    /* Read the chunk header */
    /* Some headers are short, like 0x4324. Reading 12 bytes will cause us
     * to lose synchronization with the stream. Just read to the length
     * (4 bytes), decode and then read up to 8 additional bytes to get the
     * entire header.
     */
    if( net_Read( p_access, p_sys->fd, NULL, p_sys->buffer, 4, true ) < 4 )
    {
       msg_Err( p_access, "cannot read data 2" );
       return VLC_EGENERIC;
    }

    p_ck->i_type = GetWLE( p_sys->buffer);
    p_ck->i_size = GetWLE( p_sys->buffer + 2);

    restsize = p_ck->i_size;
    if( restsize > 8 )
        restsize = 8;

    if( net_Read( p_access, p_sys->fd, NULL, p_sys->buffer + 4, restsize, true ) < restsize )
    {
        msg_Err( p_access, "cannot read data 3" );
        return VLC_EGENERIC;
    }
    p_ck->i_sequence  = GetDWLE( p_sys->buffer + 4);
    p_ck->i_unknown   = GetWLE( p_sys->buffer + 8);

    /* Set i_size2 to 8 if this header was short, since a real value won't be
     * present in the buffer. Using 8 avoid reading additional data for the
     * packet.
     */
    if( restsize < 8 )
        p_ck->i_size2 = 8;
    else
        p_ck->i_size2 = GetWLE( p_sys->buffer + 10);

    p_ck->p_data      = p_sys->buffer + 12;
    p_ck->i_data      = p_ck->i_size2 - 8;

    if( p_ck->i_type == 0x4524 )   // $E (End-of-Stream Notification) Packet
    {
        if( p_ck->i_sequence == 0 )
        {
            msg_Warn( p_access, "EOF" );
            return VLC_EGENERIC;
        }
        else
        {
            msg_Warn( p_access, "next stream following" );
            return VLC_EGENERIC;
        }
    }
    else if( p_ck->i_type == 0x4324 ) // $C (Stream Change Notification) Packet
    {
        /* 0x4324 is CHUNK_TYPE_RESET: a new stream will follow with a sequence of 0 */
        msg_Warn( p_access, "next stream following (reset) seq=%d", p_ck->i_sequence  );
        return VLC_EGENERIC;
    }
    else if( (p_ck->i_type != 0x4824) && (p_ck->i_type != 0x4424) )
    {
        /* Unsupported so far:
         * $M (Metadata) Packet               0x4D24
         * $P (Packet-Pair) Packet            0x5024
         * $T (Test Data Notification) Packet 0x5424
         */
        msg_Err( p_access, "unrecognized chunk FATAL (0x%x)", p_ck->i_type );
        return VLC_EGENERIC;
    }

    if( (p_ck->i_data > 0) &&
        (net_Read( p_access, p_sys->fd, NULL, &p_sys->buffer[12],
                   p_ck->i_data, true ) < p_ck->i_data) )
    {
        msg_Err( p_access, "cannot read data 4" );
        return VLC_EGENERIC;
    }

#if 0
    if( (p_sys->i_packet_sequence != 0) &&
        (p_ck->i_sequence != p_sys->i_packet_sequence) )
    {
        msg_Warn( p_access, "packet lost ? (%d != %d)", p_ck->i_sequence, p_sys->i_packet_sequence );
    }
#endif

    p_sys->i_packet_sequence = p_ck->i_sequence + 1;
    p_sys->i_packet_used   = 0;
    p_sys->i_packet_length = p_ck->i_data;
    p_sys->p_packet        = p_ck->p_data;

    return VLC_SUCCESS;
}
