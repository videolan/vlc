/*****************************************************************************
 * netutils.c: various network functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: netutils.c,v 1.22 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Benoit Steiner <benny@via.ecp.fr>
 *          Henri Fallon <henri@videolan.org>
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

#include <netdb.h>                                        /* gethostbyname() */
#include <stdlib.h>                             /* free(), realloc(), atoi() */
#include <errno.h>                                                /* errno() */
#include <string.h>                                      /* bzero(), bcopy() */
#include <unistd.h>                                         /* gethostname() */

#include <netinet/in.h>                               /* BSD: struct in_addr */
#include <sys/socket.h>                              /* BSD: struct sockaddr */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>                           /* inet_ntoa(), inet_aton() */
#endif

#if defined (HAVE_NET_IF_H)
#include <net/if.h>                            /* interface (arch-dependent) */
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "threads.h"

#include "intf_msg.h"

#if !defined( SYS_BEOS ) && !defined( SYS_NTO )

#include "netutils.h"

/*****************************************************************************
 * input_BuildLocalAddr : fill a sockaddr_in structure for local binding
 *****************************************************************************/
int network_BuildLocalAddr( struct sockaddr_in * p_socket, int i_port, 
                            boolean_t b_broadcast )
{
    char                psz_hostname[INPUT_MAX_SOURCE_LENGTH];
    struct hostent    * p_hostent;
    
    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                 /* family */
    p_socket->sin_port = htons( i_port );
    if( !b_broadcast )
    {
        /* Try to get our own IP */
        if( gethostname( psz_hostname, sizeof(psz_hostname) ) )
        {
            intf_ErrMsg( "BuildLocalAddr : unable to resolve local name : %s",
                         strerror( errno ) );
            return( -1 );
        }

    }
    else
    {
        /* Instead of trying to find the broadcast address using non-portable
         * ioctl, let's bind INADDR_ANY */
        strncpy(psz_hostname,"0.0.0.0",sizeof(psz_hostname));
    }

    /* Try to convert address directly from in_addr - this will work if
     * psz_in_addr is dotted decimal. */
#ifdef HAVE_ARPA_INET_H
    if( !inet_aton( psz_hostname, &p_socket->sin_addr) )
#else
    if( (p_socket->sin_addr.s_addr = inet_addr( psz_hostname )) == -1 )
#endif
    {
        /* We have a fqdn, try to find its address */
        if ( (p_hostent = gethostbyname( psz_hostname )) == NULL )
        {
            intf_ErrMsg( "BuildLocalAddr: unknown host %s", psz_hostname );
            return( -1 );
        }
        
        /* Copy the first address of the host in the socket address */
        memcpy( &p_socket->sin_addr, p_hostent->h_addr_list[0], 
                 p_hostent->h_length );
    }
    return( 0 );
}

/*****************************************************************************
 * input_BuildRemoteAddr : fill a sockaddr_in structure for remote host
 *****************************************************************************/
int network_BuildRemoteAddr( struct sockaddr_in * p_socket, char * psz_server )
{
    struct hostent            * p_hostent;

    /* Reset structure */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                 /* family */
    p_socket->sin_port = htons( 0 );                /* This is for remote end */
    
     /* Try to convert address directly from in_addr - this will work if
      * psz_in_addr is dotted decimal. */

#ifdef HAVE_ARPA_INET_H
    if( !inet_aton( psz_server, &p_socket->sin_addr) )
#else
    if( (p_socket->sin_addr.s_addr = inet_addr( psz_server )) == -1 )
#endif
    {
        /* We have a fqdn, try to find its address */
        if ( (p_hostent = gethostbyname(psz_server)) == NULL )
        {
            intf_ErrMsg( "BuildRemoteAddr: unknown host %s", 
                         psz_server );
            return( -1 );
        }
        
        /* Copy the first address of the host in the socket address */
        memcpy( &p_socket->sin_addr, p_hostent->h_addr_list[0], 
                 p_hostent->h_length );
    }
    return( 0 );
}
#endif

