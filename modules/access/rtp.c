/*****************************************************************************
 * rtp.c: RTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: rtp.c,v 1.3 2002/10/03 20:49:31 jpsaman Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include "network.h"

#define RTP_HEADER_LEN 12
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open       ( vlc_object_t * );
static int  RTPNetworkRead( input_thread_t *, byte_t *, size_t );
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("RTP access module") );
    set_capability( "access", 0 );
    add_shortcut( "rtp" );
    add_shortcut( "rtpstream" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );
    set_callbacks( Open, __input_FDNetworkClose );
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

    p_input->pf_read = RTPNetworkRead;
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
        msg_Err( p_input, "this RTP syntax is deprecated; the server argument will be");
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

    p_input->psz_demux = "ts";
    
    return( 0 );
}

/*****************************************************************************
 * RTPNetworkRead : Read for the network, and parses the RTP header
 *****************************************************************************/
static int RTPNetworkRead( input_thread_t * p_input, byte_t * p_buffer,
	                   size_t i_len	)
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;
    int         i;
    
    byte_t 	p_tmp_buffer[1500];
    
    // Get the Raw data from the socket
    // We first assume that RTP header size is the classic RTP_HEADER_LEN
    ssize_t i_ret = input_FDNetworkRead(p_input, p_tmp_buffer,
		                        i_len + RTP_HEADER_LEN);
    
    if (!i_ret) return 0;
	     
    // Parse the header and make some verifications
    // See RFC 1889 & RFC 2250
  
    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F ); 
  
    if ( i_rtp_version != 2 ) 
        msg_Dbg( p_input, "RTP version is %u, should be 2", i_rtp_version );
  
    if ( i_payload_type != 33 )
        msg_Dbg( p_input, "RTP payload type is %u, only 33 (Mpeg2-TS) \
is supported", i_payload_type );
 
    // If both bytes are wrong, maybe a synchro error occurred...
    if (( i_rtp_version != 2 ) && ( i_payload_type != 33 ))
    {
         msg_Dbg( p_input, "Too many RTP errors, trying to re-synchronize" );
 
        //Trying to re-synchronize
	for ( i=0 ; (i<i_len) ||
                    ((( p_tmp_buffer[0] & 0xC0 ) >> 6 == 2 )
                     && ( p_tmp_buffer[1] & 0x7F ) == 33 ) ; i++);

	if (i!=i_len)
	{
	   input_FDNetworkRead(p_input, p_tmp_buffer,i);
	   return 0;
	}
		
    }

/*    
    // if i_CSRC_count != 0, the header is in fact longer than RTP_HEADER_LEN
    // so we have to read some extra bytes 
    // This case is supposed to be very rare (vls does not handle that),
    //  so in practical this second input_FDNetworkRead is never done...
    if (i_CSRC_count)
    {    i_ret += input_FDNetworkRead(p_input,
                                      p_tmp_buffer + i_len + RTP_HEADER_LEN,
                                      4 * i_CSRC_count ); 
    }
*/
    
    // Return the packet without the RTP header
    i_ret -= ( RTP_HEADER_LEN + 4 * i_CSRC_count );

    p_input->p_vlc->pf_memcpy( p_buffer, 
                       p_tmp_buffer + RTP_HEADER_LEN + 4 * i_CSRC_count,
                       i_ret );
    
    return (i_ret);
}
