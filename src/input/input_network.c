/*****************************************************************************
 * network.c: functions to read from the network
 * Manages a socket.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Benoît Steiner <benny@via.ecp.fr>
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
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                               /* close() */
#include <errno.h>                                                  /* errno */
#include <sys/time.h>                                   /* "input_network.h" */

#if defined(SYS_BSD) || defined(SYS_BEOS)
#include <sys/socket.h>                                   /* struct sockaddr */
#endif

#include <netdb.h>     /* servent, getservbyname(), hostent, gethostbyname() */
#include <netinet/in.h>                     /* sockaddr_in, htons(), htonl() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "netutils.h"

#include "input.h"
#include "input_network.h"
#include "input_vlan.h"

#include "intf_msg.h"
#include "plugins.h"
#include "main.h"

/*****************************************************************************
 * input_NetworkOpen: initialize a network stream
 *****************************************************************************/
int input_NetworkOpen( input_thread_t *p_input )
{
    int                     i_socket_option;
    struct sockaddr_in      sa_in;
    char                    psz_hostname[INPUT_MAX_SOURCE_LENGTH];

    /* First and foremost, in the VLAN method, join the desired VLAN. */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {
        if( input_VlanJoin( p_input->i_vlan ) )
        {
            intf_ErrMsg("error: can't join vlan %d\n", p_input->i_vlan);
            return( 1 );
        }
    }

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (p_input->i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == (-1) )
    {
        intf_ErrMsg("error: can't create socket (%s)\n", strerror(errno));
        return( 1 );
    }

    /*
     * Set up the options of the socket
     */

    /* Set SO_REUSEADDR option which allows to re-bind() a busy port */
    i_socket_option = 1;
    if( setsockopt( p_input->i_handle,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &i_socket_option,
                    sizeof( i_socket_option ) ) == (-1) )
    {
        intf_ErrMsg("error: can't configure socket (SO_REUSEADDR: %s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }

#ifndef SYS_BEOS
    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_socket_option = 524288;
    if( setsockopt( p_input->i_handle,
                    SOL_SOCKET,
                    SO_RCVBUF,
                    &i_socket_option,
                    sizeof( i_socket_option ) ) == (-1) )
    {
        intf_ErrMsg("error: can't configure socket (SO_RCVBUF: %s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }
#endif /* SYS_BEOS */

    /*
     * Bind the socket
     */

    /* Use default port if not specified */
    if( p_input->i_port == 0 )
    {
        p_input->i_port = main_GetIntVariable( INPUT_PORT_VAR, INPUT_PORT_DEFAULT );
    }

    /* Find the address. */
    switch( p_input->i_method )
    {
    case INPUT_METHOD_TS_BCAST:
    case INPUT_METHOD_TS_VLAN_BCAST:
        /* In that case, we have to bind with the broadcast address.
         * broadcast addresses are very hard to find and depends on
         * implementation, so we thought using a #define would be much
         * simpler. */
#ifdef INPUT_BCAST_ADDR
        if( BuildInetAddr( &sa_in, INPUT_BCAST_ADDR, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
#else
        /* We bind with any address. Security problem ! */
        if( BuildInetAddr( &sa_in, NULL, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( -1 );
        }
#endif
        break;

    case INPUT_METHOD_TS_UCAST:
        /* Unicast: bind with the local address. */
        if( gethostname( psz_hostname, sizeof( psz_hostname ) ) == (-1) )
        {
            intf_ErrMsg("error: can't get hostname (%s)\n", strerror(errno));
            close( p_input->i_handle );
            return( 1 );
        }
        if( BuildInetAddr( &sa_in, psz_hostname, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
        break;

    case INPUT_METHOD_TS_MCAST:
        /* Multicast: bind with 239.0.0.1. */
        if( BuildInetAddr( &sa_in, "239.0.0.1", p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
        break;
    }

    /* Effectively bind the socket. */
    if( bind( p_input->i_handle, (struct sockaddr *) &sa_in, sizeof( sa_in ) ) < 0 )
    {
        intf_ErrMsg("error: can't bind socket (%s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }

    /*
     * Connect the socket to the remote server
     */

    /* Use default host if not specified */
    if( p_input->p_source == NULL )
    {
        p_input->p_source = main_GetPszVariable( INPUT_SERVER_VAR, INPUT_SERVER_DEFAULT );
    }

    if( BuildInetAddr( &sa_in, p_input->p_source, htons(0) ) == (-1) )
    {
        close( p_input->i_handle );
        return( -1 );
    }

    /* Connect the socket. */
    if( connect( p_input->i_handle, (struct sockaddr *) &sa_in,
                 sizeof( sa_in ) ) == (-1) )
    {
        intf_ErrMsg("error: can't connect socket\n" );
        close( p_input->i_handle );
        return( 1 );
    }
    return( 0 );
}

/*****************************************************************************
 * input_NetworkRead: read a stream from the network
 *****************************************************************************
 * Wait for data during up to 1 second and then abort if none is arrived. The
 * number of bytes read is returned or -1 if an error occurs (so 0 is returned
 * after a timeout)
 * We don't have to make any test on presentation times, since we suppose
 * the network server sends us data when we need it.
 *****************************************************************************/
int input_NetworkRead( input_thread_t *p_input, const struct iovec *p_vector,
                       size_t i_count )
{
    fd_set rfds;
    struct timeval tv;
    int i_rc;

    /* Watch the given fd to see when it has input */
    FD_ZERO(&rfds);
    FD_SET(p_input->i_handle, &rfds);

    /* Wait up to 1 second */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    i_rc = select(p_input->i_handle+1, &rfds, NULL, NULL, &tv);

    if( i_rc > 0 )
    {
        /* Data were received before timeout */
        i_rc = readv( p_input->i_handle, p_vector, i_count );
    }

    return( i_rc );
}

/*****************************************************************************
 * input_NetworkClose: close a network stream
 *****************************************************************************/
void input_NetworkClose( input_thread_t *p_input )
{
    /* Close local socket. */
    if( p_input->i_handle )
    {
        close( p_input->i_handle );
    }

    /* Leave vlan if required */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {
        input_VlanLeave( p_input->i_vlan );
    }
}

