/*****************************************************************************
 * announce.c : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
 *          Damien Lucas <nitrox@via.ecp.fr>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/sout.h>

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

#include "announce.h"
#include "network.h"

#define SAP_IPV4_ADDR "224.2.127.254" /* Standard port and address for SAP */
#define SAP_PORT 9875

#define SAP_IPV6_ADDR_1 "FF0"
#define SAP_IPV6_ADDR_2 "::2:7FFE"

#define DEFAULT_PORT "1234"

/****************************************************************************
 *  Split : split a string into two parts: the one which is before the delim
 *               and the one which is after.
 *               NULL is returned if delim is not found
 ****************************************************************************/

static char * split( char *psz_in, char *psz_out1, char *psz_out2, char delim)
{
    unsigned int i_count = 0; /* pos in input string */
    unsigned int i_pos1  = 0; /* pos in out2 string */
    unsigned int i_pos2  = 0;
    char *psz_cur; /* store the pos of the first delim found */

    /* Skip spaces at the beginning */
    while( psz_in[i_count] == ' ' )
    {
        i_count++;
    }

    if( psz_in[i_count] == '\0' )
    {
        return NULL;
    }

    /* Look for delim */
    while( psz_in[i_count] && psz_in[i_count] != delim )
    {
        psz_out1[i_pos1] = psz_in[i_count];
        i_count++;
        i_pos1++;
    }
    /* Mark the end of out1 */
    psz_out1[i_pos1] = '\0';

    if( psz_in[i_count] == '\0' )
    {
        return NULL;
    }

    /* store pos of the first delim */
    psz_cur = psz_in + i_count;

    /* skip all delim and all spaces */
    while( psz_in[i_count] == ' ' || psz_in[i_count] == delim )
    {
        i_count++;
    }

    if( psz_in[i_count] == '\0' )
    {
        return psz_cur;
    }

    /* Store the second string */
    while( psz_in[i_count] )
    {
        psz_out2[i_pos2] = psz_in[i_count];
        i_pos2++;
        i_count++;
    }
    psz_out2[i_pos2] = '\0';

    return psz_cur;
}

/*****************************************************************************
 * sout_SAPNew: Creates a SAP Session
 *****************************************************************************/
sap_session_t * sout_SAPNew ( sout_instance_t *p_sout, char * psz_url_arg,
                              char * psz_name_arg, int ip_version,
                              char * psz_v6_scope )
{
    sap_session_t       *p_sap; /* The SAP structure */
    module_t            *p_network; /* Network module */
    network_socket_t    socket_desc; /* Socket descriptor */
    char                *sap_ipv6_addr = NULL; /* IPv6 built address */
    char                *psz_eol; /* Used to parse IPv6 URIs */
    int                 i_port; /* Port in numerical format */

    /* Allocate the SAP structure */
    p_sap = (sap_session_t *) malloc( sizeof ( sap_session_t ) ) ;
    if ( !p_sap )
    {
        msg_Err( p_sout, "out of memory" );
        return NULL;
    }

    /* Fill the information in the structure */
    if( strstr( psz_url_arg, "[" ) )
    {
        /* We have an IPv6 address. Do not use ':' as the port separator */
        psz_eol = strchr( psz_url_arg, ']' );
        if( !psz_eol )
        {
            msg_Warn( p_sout, "no matching ], unable to parse URI");
            return NULL;
        }

        if( !psz_eol++ )
        {
            sprintf( p_sap->psz_url, "%s", psz_url_arg );
            sprintf( p_sap->psz_port, "%s", DEFAULT_PORT );
        }
        else
        {
            *psz_eol = '\0';
            sprintf( p_sap->psz_url, "%s", psz_url_arg );
            psz_eol++;
            if( psz_eol )
            {
                sprintf( p_sap->psz_port, "%s", psz_eol );
            }
        }
    }
    else
    {
        split( psz_url_arg, p_sap->psz_url, p_sap->psz_port, ':' );
    }

    /* Check if we have a port */
    if( !strlen( p_sap->psz_port ) )
    {
        sprintf( p_sap->psz_port, "%s", DEFAULT_PORT );
    }

    /* Make sure our port is valid and atoi it */
    i_port = atoi( p_sap->psz_port );

    if( !i_port )
    {
        sprintf( p_sap->psz_port, "%s", DEFAULT_PORT );
    }
    else
    {
        sprintf( p_sap->psz_port, "%i", i_port );
    }

    /* The name that we send */
    sprintf( p_sap->psz_name, "%s", psz_name_arg );

    p_sap->i_ip_version = ip_version;

    /* Only "6" triggers IPv6. IPv4 is default */
    if( ip_version != 6 )
    {
        msg_Dbg( p_sout, "creating IPv4 SAP socket" );

        /* Fill the socket descriptor */
        socket_desc.i_type            = NETWORK_UDP;
        socket_desc.psz_bind_addr     = "";
        socket_desc.i_bind_port       = 0;
        socket_desc.psz_server_addr   = SAP_IPV4_ADDR;
        socket_desc.i_server_port     = SAP_PORT;
        socket_desc.i_handle          = 0;

        /* Call the network module */
        p_sout->p_private = (void*) &socket_desc;
        if( !( p_network = module_Need( p_sout, "network", "ipv4" ) ) )
        {
             msg_Warn( p_sout, "failed to open a connection (udp)" );
             return NULL;
        }
        module_Unneed( p_sout, p_network );

        p_sap->i_socket = socket_desc.i_handle;
        if( p_sap->i_socket < 0 )
        {
            msg_Warn( p_sout, "unable to initialize SAP" );
            return NULL;
        }
    }
    else
    {
        msg_Dbg( p_sout, "creating IPv6 SAP socket with scope %s",
                         psz_v6_scope );

        /* Initialize and build the IPv6 address to broadcast to */
        sap_ipv6_addr = (char *) malloc( 28 * sizeof(char) );
        if ( !sap_ipv6_addr )
        {
            msg_Err( p_sout, "out of memory" );
            return NULL;
        }
        sprintf( sap_ipv6_addr, "%s%c%s",
                 SAP_IPV6_ADDR_1, psz_v6_scope[0], SAP_IPV6_ADDR_2 );

        /* Fill the socket descriptor */
        socket_desc.i_type          = NETWORK_UDP;
        socket_desc.psz_bind_addr   = "";
        socket_desc.i_bind_port     = 0;
        socket_desc.psz_server_addr = sap_ipv6_addr;
        socket_desc.i_server_port   = SAP_PORT;
        socket_desc.i_handle        = 0;

        /* Call the network module */
        p_sout->p_private = (void *) &socket_desc;
        if( !( p_network = module_Need( p_sout, "network", "ipv6" ) ) )
        {
            msg_Warn( p_sout, "failed to open a connection (udp)" );
            return NULL;
        }
        module_Unneed( p_sout, p_network );

        p_sap->i_socket = socket_desc.i_handle;
        if( p_sap->i_socket <= 0 )
        {
            msg_Warn( p_sout, "unable to initialize SAP" );
            return NULL;
        }

        /* Free what we allocated */
        if( sap_ipv6_addr )
        {
            free( sap_ipv6_addr );
        }
    }

    msg_Dbg( p_sout, "SAP initialization complete" );

    return p_sap;
}

/*****************************************************************************
 * sout_SAPDelete: Deletes a SAP Session
 *****************************************************************************/
void sout_SAPDelete( sout_instance_t *p_sout, sap_session_t * p_sap )
{
    int i_ret;

#if defined( UNDER_CE )
    i_ret = CloseHandle( (HANDLE)p_sap->i_socket );
#elif defined( WIN32 )
    i_ret = closesocket( p_sap->i_socket );
#else
    i_ret = close( p_sap->i_socket );
#endif

    if( i_ret )
    {
        msg_Err( p_sout, "unable to close SAP socket" );
    }

    free( p_sap );
}

/*****************************************************************************
 * sout_SAPSend: Sends a SAP packet
 *****************************************************************************/
void sout_SAPSend( sout_instance_t *p_sout, sap_session_t * p_sap )
{
    char psz_msg[1000];                     /* SDP content */
    char *psz_head;                         /* SAP header */
    char *psz_send;                         /* What we send */
    char *psz_type = "application/sdp";
    int i_header_size;                      /* SAP header size */
    int i_msg_size;                         /* SDP content size */
    int i_size;                             /* Total size */
    int i_ret = 0;

    /* We send a packet every 24 calls to the function */
    if( p_sap->i_calls++ < 24 )
    {
        return;
    }

    i_header_size = 8 + strlen( psz_type ) + 1;
    psz_head = (char *) malloc( i_header_size * sizeof( char ) );

    if( ! psz_head )
    {
        msg_Err( p_sout, "out of memory" );
        return;
    }

    /* Create the SAP headers */
    psz_head[0] = 0x20; /* Means IPv4, not encrypted, not compressed */
    psz_head[1] = 0x00; /* No authentification */
    psz_head[2] = 0x42; /* Version */
    psz_head[3] = 0x12; /* Version */

    psz_head[4] = 0x01; /* Source IP  FIXME: we should get the real address */
    psz_head[5] = 0x02; /* idem */
    psz_head[6] = 0x03; /* idem */
    psz_head[7] = 0x04; /* idem */

    strncpy( psz_head + 8, psz_type, 15 );
    psz_head[ i_header_size-1 ] = '\0';

    /* Create the SDP content */
    /* Do not add spaces at beginning of the lines ! */
    sprintf( psz_msg, "v=0\n"
                      "o=VideoLAN 3247692199 3247895918 IN IP4 VideoLAN\n"
                      "s=%s\n"
                      "u=VideoLAN\n"
                      "t=0 0\n"
                      "m=audio %s udp 14\n"
                      "c=IN IP4 %s/15\n"
                      "a=type:test\n",
             p_sap->psz_name, p_sap->psz_port, p_sap->psz_url );

    i_msg_size = strlen( psz_msg );
    i_size = i_msg_size + i_header_size;

    /* Create the message */
    psz_send = (char *) malloc( i_size*sizeof(char) );
    if( !psz_send )
    {
        msg_Err( p_sout, "out of memory" );
        return;
    }

    memcpy( psz_send, psz_head, i_header_size );
    memcpy( psz_send + i_header_size, psz_msg, i_msg_size );

    if( i_size < 1024 ) /* We mustn't send packets larger than 1024B */
    {
        i_ret = send( p_sap->i_socket, psz_send, i_size, 0 );
    }

    if( i_ret <= 0 )
    {
        msg_Warn( p_sout, "SAP send failed on socket %i (%s)",
                          p_sap->i_socket, strerror(errno) );
    }

    p_sap->i_calls = 0;

    /* Free what we allocated */
    free( psz_send );
    free( psz_head );
}

