/*****************************************************************************
 * netutils.c: various network functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: netutils.c,v 1.45 2001/11/13 00:46:23 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Benoit Steiner <benny@via.ecp.fr>
 *          Henri Fallon <henri@videolan.org>
 *          Xavier Marchesini <xav@via.ecp.fr>
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

#include <stdlib.h>                             /* free(), realloc(), atoi() */
#include <errno.h>                                                /* errno() */
#include <string.h>                                              /* memset() */

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                      /* gethostname() */
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#if !defined( _MSC_VER )
#include <sys/time.h>                                        /* gettimeofday */
#endif

#ifdef WIN32
#   include <winsock2.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>                           /* BSD: struct sockaddr */
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#ifdef SYS_LINUX
#include <sys/ioctl.h>                                            /* ioctl() */
#endif

#if defined( WIN32 )                    /* tools to get the MAC adress from  */
#include <windows.h>                    /* the interface under Windows       */
#include <stdio.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>                            /* interface (arch-dependent) */
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "threads.h"
#include "main.h"

#include "intf_msg.h"
#include "intf_playlist.h"

#include "netutils.h"

/*****************************************************************************
 * input_channel_t: channel library data
 *****************************************************************************
 * Store global channel library data.
 * The part of the code concerning the channel changing process is unstable
 * as it depends on the VideoLAN channel server, which isn't frozen for
 * the time being.
 *****************************************************************************/
typedef struct input_channel_s
{
    int         i_channel;                         /* current channel number */
    mtime_t     last_change;                             /* last change date */
} input_channel_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int GetMacAddress   ( int i_fd, char *psz_mac );
#ifdef WIN32
static int GetAdapterInfo  ( int i_adapter, char *psz_string );
#endif

/*****************************************************************************
 * network_BuildLocalAddr : fill a sockaddr_in structure for local binding
 *****************************************************************************/
int network_BuildLocalAddr( struct sockaddr_in * p_socket, int i_port,
                            char * psz_broadcast )
{
#if defined( SYS_BEOS )
    intf_ErrMsg( "error: channel changing is not yet supported under BeOS" );
    return( 1 );

#else
    char                psz_hostname[INPUT_MAX_SOURCE_LENGTH];
    struct hostent    * p_hostent;

    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                /* family */
    p_socket->sin_port = htons( i_port );
    if( psz_broadcast == NULL )
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
        /* I didn't manage to make INADDR_ANYT work, even with setsockopt
         * so, as it's kludgy to try and determine the broadcast addr
         * it is passed as an argument in the command line */
        strncpy( psz_hostname, psz_broadcast, INPUT_MAX_SOURCE_LENGTH );
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
#endif
}

/*****************************************************************************
 * network_BuildRemoteAddr : fill a sockaddr_in structure for remote host
 *****************************************************************************/
int network_BuildRemoteAddr( struct sockaddr_in * p_socket, char * psz_server )
{
#if defined( SYS_BEOS )
    intf_ErrMsg( "error: channel changing is not yet supported under BeOS" );
    return( 1 );

#else
    struct hostent            * p_hostent;

    /* Reset structure */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                /* family */
    p_socket->sin_port = htons( 0 );               /* This is for remote end */

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
#endif
}

/*****************************************************************************
 * network_ChannelCreate: initialize global channel method data
 *****************************************************************************
 * Initialize channel input method global data. This function should be called
 * once before any input thread is created or any call to other
 * input_Channel*() function is attempted.
 *****************************************************************************/
int network_ChannelCreate( void )
{
#if defined( SYS_LINUX ) || defined( WIN32 )

    /* Allocate structure */
    p_main->p_channel = malloc( sizeof( input_channel_t ) );
    if( p_main->p_channel == NULL )
    {
        intf_ErrMsg( "network error: could not create channel bank" );
        return( -1 );
    }

    /* Initialize structure */
    p_main->p_channel->i_channel   = 0;
    p_main->p_channel->last_change = 0;

    intf_WarnMsg( 2, "network: channels initialized" );
    return( 0 );

#else
    intf_ErrMsg( "network error : channels not supported on this platform" );
    return( 1 );

#endif
}

/*****************************************************************************
 * network_ChannelJoin: join a channel
 *****************************************************************************
 * This function will try to join a channel. If the relevant interface is
 * already on the good channel, nothing will be done. Else, and if possible
 * (if the interface is not locked), the channel server will be contacted
 * and a change will be requested. The function will block until the change
 * is effective. Note that once a channel is no more used, its interface
 * should be unlocked using input_ChannelLeave().
 * Non 0 will be returned in case of error.
 *****************************************************************************/
int network_ChannelJoin( int i_channel )
{
#if defined( SYS_LINUX ) || defined( WIN32 )

#define VLCS_VERSION 12
#define MESSAGE_LENGTH 256

    char psz_mess[ MESSAGE_LENGTH ];
    char psz_mac[ 40 ];
    int i_fd, i_dummy, i_port;
    char *psz_vlcs;
    struct sockaddr_in sa_server;
    struct sockaddr_in sa_client;
    struct timeval delay;
    fd_set fds;

    if( !main_GetIntVariable( INPUT_NETWORK_CHANNEL_VAR,
                              INPUT_NETWORK_CHANNEL_DEFAULT  ) )
    {
        intf_ErrMsg( "network: channels disabled, to enable them, use the"
                     "--channels option" );
        return -1;
    }

    /* If last change is too recent, wait a while */
    if( mdate() - p_main->p_channel->last_change < INPUT_CHANNEL_CHANGE_DELAY )
    {
        intf_WarnMsg( 2, "network: waiting before changing channel" );
        /* XXX Isn't this completely brain-damaged ??? -- Sam */
        mwait( p_main->p_channel->last_change + INPUT_CHANNEL_CHANGE_DELAY );
    }

    /* Initializing the socket */
    i_fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( i_fd < 0 )
    {
        intf_ErrMsg( "network error: unable to create vlcs socket (%s)",
                     strerror( errno ) );
        return -1;
    }

    i_dummy = 1;
    if( setsockopt( i_fd, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_dummy, sizeof( i_dummy ) ) == -1 )
    {
        intf_ErrMsg( "network error: can't SO_REUSEADDR vlcs socket (%s)",
                     strerror(errno));
        close( i_fd );
        return -1;
    }

    /* Getting information about the channel server */
    psz_vlcs = main_GetPszVariable( INPUT_CHANNEL_SERVER_VAR,
                                    INPUT_CHANNEL_SERVER_DEFAULT );
    i_port = main_GetIntVariable( INPUT_CHANNEL_PORT_VAR,
                                  INPUT_CHANNEL_PORT_DEFAULT );

    intf_WarnMsg( 5, "network: socket %i, vlcs '%s', port %d",
                     i_fd, psz_vlcs, i_port );

    memset( &sa_client, 0x00, sizeof(struct sockaddr_in) );
    memset( &sa_server, 0x00, sizeof(struct sockaddr_in) );
    sa_client.sin_family      = AF_INET;
    sa_server.sin_family      = AF_INET;
    sa_client.sin_port        = htons( 4312 );
    sa_server.sin_port        = htons( i_port );
    sa_client.sin_addr.s_addr = INADDR_ANY;
#ifdef HAVE_ARPA_INET_H
    inet_aton( psz_vlcs, &sa_server.sin_addr );
#else
    sa_server.sin_addr.s_addr = inet_addr( psz_vlcs );
#endif

    /* Bind the socket */
    if( bind( i_fd, (struct sockaddr*)(&sa_client), sizeof(sa_client) ) )
    {
        intf_ErrMsg( "network: unable to bind vlcs socket (%s)",
                     strerror( errno ) );
        close( i_fd );
        return -1;
    }

    /* Look for the interface MAC address */
    if( GetMacAddress( i_fd, psz_mac ) )
    {
        intf_ErrMsg( "network error: failed getting MAC address" );
        close( i_fd );
        return -1;
    }

    intf_WarnMsg( 6, "network: MAC address is %s", psz_mac );

    /* Build the message */
    sprintf( psz_mess, "%d %u %lu %s \n", i_channel, VLCS_VERSION,
                       (unsigned long)(mdate() / (unsigned long long)1000000),
                       psz_mac );

    /* Send the message */
    sendto( i_fd, psz_mess, MESSAGE_LENGTH, 0,
            (struct sockaddr *)(&sa_server), sizeof(struct sockaddr) );

    intf_WarnMsg( 2, "network: attempting to join channel %d", i_channel );

    /* We have changed channels ! (or at least, we tried) */
    p_main->p_channel->last_change = mdate();
    p_main->p_channel->i_channel = i_channel;

    /* Wait 5 sec for an answer from the server */
    delay.tv_sec = 5;
    delay.tv_usec = 0;
    FD_ZERO( &fds );
    FD_SET( i_fd, &fds );

    switch( select( i_fd + 1, &fds, NULL, NULL, &delay ) )
    {
        case 0:
            intf_ErrMsg( "network error: no answer from vlcs" );
            close( i_fd );
            return -1;
            break;

        case -1:
            intf_ErrMsg( "network error: error while listening to vlcs" );
            close( i_fd );
            return -1;
            break;
    }

    i_dummy = sizeof( struct sockaddr );
    recvfrom( i_fd, psz_mess, MESSAGE_LENGTH, 0,
              (struct sockaddr *)(&sa_client), &i_dummy);
    psz_mess[ MESSAGE_LENGTH - 1 ] = 0;

    if( !strncasecmp( psz_mess, "E: ", 3 ) )
    {
        intf_ErrMsg( "network error: vlcs said '%s'", psz_mess + 3 );
        close( i_fd );
        return -1;
    }
    else if( !strncasecmp( psz_mess, "I: ", 3 ) )
    {
        intf_WarnMsg( 2, "network info: vlcs said '%s'", psz_mess + 3 );
    }
    else /* We got something to play ! FIXME: not very nice */
    {
#   define p_item \
        (&p_main->p_playlist->p_item[ p_main->p_playlist->i_index + 1])
        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        free( p_item->psz_name );
        p_item->psz_name = strdup( psz_mess );
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }

    /* Close the socket and return nicely */
    close( i_fd );

    return 0;

#else
    intf_ErrMsg( "network error: channels not supported on this platform" );
    return -1; 

#endif
}

/* Following functions are local */

/*****************************************************************************
 * GetMacAddress: extract the MAC Address
 *****************************************************************************/
static int GetMacAddress( int i_fd, char *psz_mac )
{
#if defined( SYS_LINUX )
    struct ifreq interface;
    int i_ret;

    /*
     * Looking for information about the eth0 interface
     */
    interface.ifr_addr.sa_family = AF_INET;
    strcpy( interface.ifr_name, 
            main_GetPszVariable( INPUT_IFACE_VAR, INPUT_IFACE_DEFAULT ) );

    i_ret = ioctl( i_fd, SIOCGIFHWADDR, &interface );

    if( i_ret )
    {
        intf_ErrMsg( "network error: ioctl SIOCGIFHWADDR failed" );
        return( i_ret );
    }

    sprintf( psz_mac, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
                      interface.ifr_hwaddr.sa_data[0] & 0xff,
                      interface.ifr_hwaddr.sa_data[1] & 0xff,
                      interface.ifr_hwaddr.sa_data[2] & 0xff,
                      interface.ifr_hwaddr.sa_data[3] & 0xff,
                      interface.ifr_hwaddr.sa_data[4] & 0xff,
                      interface.ifr_hwaddr.sa_data[5] & 0xff );

    return( 0 );

#elif defined( WIN32 )
    int i, i_ret = -1;

    /* Get adapter list - support for more than one adapter */
    LANA_ENUM AdapterList;
    NCB       Ncb;

    intf_WarnMsg( 2, "network: looking for MAC address" );

    memset( &Ncb, 0, sizeof( NCB ) );
    Ncb.ncb_command = NCBENUM;
    Ncb.ncb_buffer = (unsigned char *)&AdapterList;
    Ncb.ncb_length = sizeof( AdapterList );
    Netbios( &Ncb );

    /* Get all of the local ethernet addresses */
    for ( i = 0; i < AdapterList.length ; ++i )
    {
        if ( GetAdapterInfo ( AdapterList.lana[ i ], psz_mac ) == 0 )
        {
            i_ret = 0;
        }
    }

    return( i_ret );

#else
    return( -1);

#endif
}

#ifdef WIN32
/*****************************************************************************
 * GetAdapterInfo : gets some informations about the interface using NETBIOS
 *****************************************************************************/
static int GetAdapterInfo( int i_adapter, char *psz_string )
{
    struct ASTAT
    {
        ADAPTER_STATUS adapt;
        NAME_BUFFER    psz_name[30];
    } Adapter;

    /* Reset the LAN adapter so that we can begin querying it */
    NCB Ncb;
    memset( &Ncb, 0, sizeof ( Ncb ) );
    Ncb.ncb_command  = NCBRESET;
    Ncb.ncb_lana_num = i_adapter;

    if( Netbios( &Ncb ) != NRC_GOODRET )
    {
        intf_ErrMsg( "network error: reset returned %i", Ncb.ncb_retcode );
        return -1;
    }

    /* Prepare to get the adapter status block */
    memset( &Ncb, 0, sizeof( Ncb ) ) ;     /* Initialization */
    Ncb.ncb_command = NCBASTAT;
    Ncb.ncb_lana_num = i_adapter;

    strcpy( (char *)Ncb.ncb_callname, "*" );

    memset( &Adapter, 0, sizeof ( Adapter ) );
    Ncb.ncb_buffer = ( unsigned char * ) &Adapter;
    Ncb.ncb_length = sizeof ( Adapter );

    /* Get the adapter's info and, if this works, return it in standard,
     * colon-delimited form. */
    if ( Netbios( &Ncb ) == 0 )
    {
        sprintf ( psz_string, "%02X:%02X:%02X:%02X:%02X:%02X",
                (int) ( Adapter.adapt.adapter_address[0] ),
                (int) ( Adapter.adapt.adapter_address[1] ),
                (int) ( Adapter.adapt.adapter_address[2] ),
                (int) ( Adapter.adapt.adapter_address[3] ),
                (int) ( Adapter.adapt.adapter_address[4] ),
                (int) ( Adapter.adapt.adapter_address[5] ) );

        intf_WarnMsg( 2, "network: found MAC address %s", psz_string );

        return 0;
    }
    else
    {
        intf_ErrMsg( "network error: ASTAT returned %i", Ncb.ncb_retcode );
        return -1;
    }
}
#endif /* WIN32 */

