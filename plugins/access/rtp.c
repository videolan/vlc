/*****************************************************************************
 * rtp.c: RTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: rtp.c,v 1.1.2.1 2002/10/03 22:14:58 massiot Exp $
 *
 * Authors: Tristan Leteurtre <tooney@via.ecp.fr>
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

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "network.h"

#define RTP_HEADER_LEN 12

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * );
static int  Open       ( struct input_thread_s * );
static ssize_t RTPNetworkRead( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("RTP access module") )
    ADD_CAPABILITY( ACCESS, 0 )
    ADD_SHORTCUT( "rtp" )
    ADD_SHORTCUT( "rtpstream" )
    ADD_SHORTCUT( "rtp4" )
    ADD_SHORTCUT( "rtp6" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->access );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.access
    input.pf_open             = Open;
    input.pf_read             = RTPNetworkRead;
    input.pf_close            = input_FDNetworkClose;
    input.pf_set_program      = input_SetProgram;
    input.pf_set_area         = NULL;
    input.pf_seek             = NULL;
#undef input
}

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( struct input_thread_s *p_this )
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

    if( config_GetIntVariable(  "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    if( config_GetIntVariable( "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "rtp6", 5 ) )
        {
            psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "rtp4", 5 ) )
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
            intf_ErrMsg( "cannot parse server port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    if( *psz_bind_port )
    {
        i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            intf_ErrMsg( "cannot parse bind port near %s", psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( *psz_server_addr || i_server_port )
    {
        intf_ErrMsg( "this RTP syntax is deprecated; the server argument will be");
        intf_ErrMsg( "ignored (%s:%d). If you wanted to enter a multicast address",
                          psz_server_addr, i_server_port);
        intf_ErrMsg( "or local port, type : %s:@%s:%d",
                          *p_input->psz_access ? p_input->psz_access : "udp",
                          psz_server_addr, i_server_port );

        i_server_port = 0;
        psz_server_addr = "";
    }
 
    intf_WarnMsg( 2, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = psz_bind_addr;
    socket_desc.i_bind_port = i_bind_port;
    socket_desc.psz_server_addr = psz_server_addr;
    socket_desc.i_server_port = i_server_port;

    /* Find an appropriate network module */
    p_network = module_Need( MODULE_CAPABILITY_NETWORK, psz_network,
                             &socket_desc );
    free(psz_name);
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_network );
    
    p_access_data = malloc( sizeof(input_socket_t) );
    p_input->p_access_data = (void *)p_access_data;

    if( p_access_data == NULL )
    {
        intf_ErrMsg( "out of memory" );
        return( -1 );
    }

    p_access_data->i_handle = socket_desc.i_handle;
    p_input->i_mtu = socket_desc.i_mtu;

    p_input->psz_demux = "ts";
    
    return( 0 );
}

/*****************************************************************************
 * RTPNetworkRead : Read for the network, and parses the RTP header
 *****************************************************************************/
static ssize_t RTPNetworkRead( input_thread_t * p_input, byte_t * p_buffer,
	                   size_t i_len	)
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;
    
    byte_t *    p_tmp_buffer = alloca( p_input->i_mtu );

    /* Get the raw data from the socket.
     * We first assume that RTP header size is the classic RTP_HEADER_LEN. */
    ssize_t i_ret = input_FDNetworkRead( p_input, p_tmp_buffer,
		                         p_input->i_mtu );
    
    if (!i_ret) return 0;
	     
    /* Parse the header and make some verifications.
     * See RFC 1889 & RFC 2250. */
  
    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F ); 
  
    if ( i_rtp_version != 2 ) 
        intf_WarnMsg( 1, "RTP version is %u, should be 2", i_rtp_version );
  
    if ( i_payload_type != 33 )
        intf_WarnMsg( 1, "RTP payload type is %u, only 33 (Mpeg2-TS) " \
                 "is supported", i_payload_type );
 
    /* Return the packet without the RTP header. */
    i_ret -= ( RTP_HEADER_LEN + 4 * i_CSRC_count );

    if ( i_ret > i_len )
    {
        /* This should NOT happen. */
        intf_WarnMsg( 1, "RTP input trashing %d bytes", i_ret - i_len );
        i_ret = i_len;
    }

    FAST_MEMCPY( p_buffer, 
                       p_tmp_buffer + RTP_HEADER_LEN + 4 * i_CSRC_count,
                       i_ret );
    
    return i_ret;
}
