/*****************************************************************************
 * udp.c: raw UDP & RTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: udp.c,v 1.16 2003/03/24 17:15:29 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif

#include "network.h"

#define RTP_HEADER_LEN 12

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open       ( vlc_object_t * );
static void Close      ( vlc_object_t * );
static ssize_t Read    ( input_thread_t *, byte_t *, size_t );
static ssize_t RTPRead ( input_thread_t *, byte_t *, size_t );
static ssize_t RTPChoose( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for udp streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("raw UDP access module") );
    add_category_hint( N_("udp"), NULL , VLC_TRUE );
    add_integer( "udp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access", 0 );
    add_shortcut( "udp" );
    add_shortcut( "udpstream" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );
    add_shortcut( "rtp" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    input_socket_t *    p_access_data;
    module_t *          p_network;
    char *              psz_network = "";
    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_bind_addr = "";
    char *              psz_bind_port = "";
    int                 i_bind_port = 0, i_server_port = 0;
    network_socket_t    socket_desc;

    if( config_GetInt( p_input, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    if( config_GetInt( p_input, "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "udp6", 5 ) )
        {
            psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "udp4", 5 ) )
        {
            psz_network = "ipv4";
        }
    }

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */

    if( *psz_parser && *psz_parser != '@' )
    {
        /* Found server */
        psz_server_addr = psz_parser;

        while( *psz_parser && *psz_parser != ':' && *psz_parser != '@' )
        {
            if( *psz_parser == '[' )
            {
                /* IPv6 address */
                while( *psz_parser && *psz_parser != ']' )
                {
                    psz_parser++;
                }
            }
            psz_parser++;
        }

        if( *psz_parser == ':' )
        {
            /* Found server port */
            *psz_parser = '\0'; /* Terminate server name */
            psz_parser++;
            psz_server_port = psz_parser;

            while( *psz_parser && *psz_parser != '@' )
            {
                psz_parser++;
            }
        }
    }

    if( *psz_parser == '@' )
    {
        /* Found bind address or bind port */
        *psz_parser = '\0'; /* Terminate server port or name if necessary */
        psz_parser++;

        if( *psz_parser && *psz_parser != ':' )
        {
            /* Found bind address */
            psz_bind_addr = psz_parser;

            while( *psz_parser && *psz_parser != ':' )
            {
                if( *psz_parser == '[' )
                {
                    /* IPv6 address */
                    while( *psz_parser && *psz_parser != ']' )
                    {
                        psz_parser++;
                    }
                }
                psz_parser++;
            }
        }

        if( *psz_parser == ':' )
        {
            /* Found bind port */
            *psz_parser = '\0'; /* Terminate bind address if necessary */
            psz_parser++;

            psz_bind_port = psz_parser;
        }
    }

    /* Convert ports format */
    if( *psz_server_port )
    {
        i_server_port = strtol( psz_server_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            msg_Err( p_input, "cannot parse server port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    if( *psz_bind_port )
    {
        i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            msg_Err( p_input, "cannot parse bind port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    if( i_bind_port == 0 )
    {
        i_bind_port = config_GetInt( p_this, "server-port" );
    }

    p_input->pf_read = RTPChoose;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( *psz_server_addr || i_server_port )
    {
        msg_Err( p_input, "this UDP syntax is deprecated; the server argument will be");
        msg_Err( p_input, "ignored (%s:%d). If you wanted to enter a multicast address",
                          psz_server_addr, i_server_port);
        msg_Err( p_input, "or local port, type : %s:@%s:%d",
                          *p_input->psz_access ? p_input->psz_access : "udp",
                          psz_server_addr, i_server_port );

        i_server_port = 0;
        psz_server_addr = "";
    }
 
    msg_Dbg( p_input, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = psz_bind_addr;
    socket_desc.i_bind_port = i_bind_port;
    socket_desc.psz_server_addr = psz_server_addr;
    socket_desc.i_server_port = i_server_port;

    /* Find an appropriate network module */
    p_input->p_private = (void*) &socket_desc;
    p_network = module_Need( p_input, "network", psz_network );
    free(psz_name);
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_input, p_network );
    
    p_access_data = malloc( sizeof(input_socket_t) );
    p_input->p_access_data = (access_sys_t *)p_access_data;

    if( p_access_data == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }

    p_access_data->i_handle = socket_desc.i_handle;
    p_input->i_mtu = socket_desc.i_mtu;

    /* Update default_pts to a suitable value for udp access */
    p_input->i_pts_delay = config_GetInt( p_input, "udp-caching" ) * 1000;

    return( 0 );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;

    msg_Info( p_input, "closing UDP target `%s'", p_input->psz_source );

#ifdef UNDER_CE
    CloseHandle( (HANDLE)p_access_data->i_handle );
#elif defined( WIN32 )
    closesocket( p_access_data->i_handle );
#else
    close( p_access_data->i_handle );
#endif

    free( p_access_data );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
#ifdef UNDER_CE
    return -1;

#else
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    ssize_t         i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_access_data->i_handle, &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    while( (i_ret = select( p_access_data->i_handle + 1, &fds,
                            NULL, NULL, &timeout )) == 0
           || (i_ret < 0 && errno == EINTR) )
    {
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

    i_recv = recv( p_access_data->i_handle, p_buffer, i_len, 0 );

    if( i_recv < 0 )
    {
#ifdef WIN32
        /* On win32 recv() will fail if the datagram doesn't fit inside
	 * the passed buffer, even though the buffer will be filled with
	 * the first part of the datagram. */
        if( WSAGetLastError() == WSAEMSGSIZE )
	{
	    msg_Err( p_input, "recv() failed. "
		     "Increase the mtu size (--mtu option)" );
	    i_recv = i_len;
	}
	else
#endif
	    msg_Err( p_input, "recv failed (%s)", strerror(errno) );
    }

    return i_recv;

#endif
}

/*****************************************************************************
 * RTPRead : read from the network, and parse the RTP header
 *****************************************************************************/
static ssize_t RTPRead( input_thread_t * p_input, byte_t * p_buffer,
                        size_t i_len )
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;

    byte_t *    p_tmp_buffer = alloca( p_input->i_mtu );

    /* Get the raw data from the socket.
     * We first assume that RTP header size is the classic RTP_HEADER_LEN. */
    ssize_t i_ret = Read( p_input, p_tmp_buffer, p_input->i_mtu );

    if ( !i_ret ) return 0;

    /* Parse the header and make some verifications.
     * See RFC 1889 & RFC 2250. */

    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F );

    if ( i_rtp_version != 2 )
        msg_Dbg( p_input, "RTP version is %u, should be 2", i_rtp_version );

    if ( i_payload_type != 33 && i_payload_type != 14
          && i_payload_type != 32 )
        msg_Dbg( p_input, "unsupported RTP payload type (%u)", i_payload_type );

    /* Return the packet without the RTP header. */
    i_ret -= ( RTP_HEADER_LEN + 4 * i_CSRC_count );

    if ( (size_t)i_ret > i_len )
    {
        /* This should NOT happen. */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        i_ret = i_len;
    }

    p_input->p_vlc->pf_memcpy( p_buffer,
                       p_tmp_buffer + RTP_HEADER_LEN + 4 * i_CSRC_count,
                       i_ret );

    return i_ret;
}

/*****************************************************************************
 * RTPChoose : read from the network, and decide whether it's UDP or RTP
 *****************************************************************************/
static ssize_t RTPChoose( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len )
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;

    byte_t *    p_tmp_buffer = alloca( p_input->i_mtu );

    /* Get the raw data from the socket.
     * We first assume that RTP header size is the classic RTP_HEADER_LEN. */
    ssize_t i_ret = Read( p_input, p_tmp_buffer, p_input->i_mtu );

    if ( !i_ret ) return 0;
    
    /* Check that it's not TS. */
    if ( p_tmp_buffer[0] == 0x47 )
    {
        msg_Dbg( p_input, "detected TS over raw UDP" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    /* Parse the header and make some verifications.
     * See RFC 1889 & RFC 2250. */

    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F );

    if ( i_rtp_version != 2 )
    {
        msg_Dbg( p_input, "no RTP header detected" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    switch ( i_payload_type )
    {
    case 33:
        msg_Dbg( p_input, "detected TS over RTP" );
        break;

    case 14:
        msg_Dbg( p_input, "detected MPEG audio over RTP" );
        break;

    case 32:
        msg_Dbg( p_input, "detected MPEG video over RTP" );
        break;

    default:
        msg_Dbg( p_input, "no RTP header detected" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    /* Return the packet without the RTP header. */
    p_input->pf_read = RTPRead;
    i_ret -= ( RTP_HEADER_LEN + 4 * i_CSRC_count );

    if ( (size_t)i_ret > i_len )
    {
        /* This should NOT happen. */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        i_ret = i_len;
    }

    p_input->p_vlc->pf_memcpy( p_buffer,
                       p_tmp_buffer + RTP_HEADER_LEN + 4 * i_CSRC_count,
                       i_ret );

    return i_ret;
}
