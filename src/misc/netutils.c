/*****************************************************************************
 * netutils.c: various network functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: netutils.c,v 1.74 2002/10/05 19:26:23 jlj Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Benoit Steiner <benny@via.ecp.fr>
 *          Henri Fallon <henri@videolan.org>
 *          Xavier Marchesini <xav@alarue.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <stdlib.h>                             /* free(), realloc(), atoi() */
#include <errno.h>                                                /* errno() */
#include <string.h>                                              /* memset() */

#include <vlc/vlc.h>

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
#else
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

#ifdef SYS_DARWIN
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOEthernetController.h>
#endif

#if defined( WIN32 )                    /* tools to get the MAC adress from  */
#include <windows.h>                    /* the interface under Windows       */
#include <stdio.h>
#include <nb30.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>                            /* interface (arch-dependent) */
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "netutils.h"
#include "vlc_playlist.h"

#include "network.h"

/*****************************************************************************
 * input_channel_t: channel library data
 *****************************************************************************
 * Store global channel library data.
 * The part of the code concerning the channel changing process is unstable
 * as it depends on the VideoLAN channel server, which isn't frozen for
 * the time being.
 *****************************************************************************/
struct input_channel_t
{
    int         i_channel;                         /* current channel number */
    mtime_t     last_change;                             /* last change date */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int GetMacAddress   ( vlc_object_t *, int i_fd, char *psz_mac );
#ifdef SYS_DARWIN
static int GetNetIntfCtrl  ( const char *psz_interface, 
                             io_object_t *ctrl_service );
#elif defined( WIN32 )
static int GetAdapterInfo  ( int i_adapter, char *psz_string );
#endif

/*****************************************************************************
 * network_ChannelCreate: initialize global channel method data
 *****************************************************************************
 * Initialize channel input method global data. This function should be called
 * once before any input thread is created or any call to other
 * input_Channel*() function is attempted.
 *****************************************************************************/
int __network_ChannelCreate( vlc_object_t *p_this )
{
#if !defined( SYS_LINUX ) && !defined( WIN32 )
    msg_Err( p_this, "VLAN-based channels are not supported "
                     "on this architecture" );
#endif

    /* Allocate structure */
    p_this->p_vlc->p_channel = malloc( sizeof( input_channel_t ) );
    if( p_this->p_vlc->p_channel == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return( -1 );
    }

    /* Initialize structure */
    p_this->p_vlc->p_channel->i_channel   = 0;
    p_this->p_vlc->p_channel->last_change = 0;

    msg_Dbg( p_this, "channels initialized" );
    return( 0 );
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
int __network_ChannelJoin( vlc_object_t *p_this, int i_channel )
{
#define VLCS_VERSION 13
#define MESSAGE_LENGTH 256

    module_t *       p_network;
    char *           psz_network = NULL;
    network_socket_t socket_desc;
    char psz_mess[ MESSAGE_LENGTH ];
    char psz_mac[ 40 ];
    int i_fd, i_port;
    char *psz_vlcs;
    struct timeval delay;
    fd_set fds;

    if( p_this->p_vlc->p_channel->i_channel == i_channel )
    {
        return 0;
    }

    if( !config_GetInt( p_this, "network-channel" ) )
    {
        msg_Err( p_this, "channels disabled, to enable them, use the"
                         " --channels option" );
        return -1;
    }

    if( config_GetInt( p_this, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    if( config_GetInt( p_this, "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    /* Getting information about the channel server */
    if( !(psz_vlcs = config_GetPsz( p_this, "channel-server" )) )
    {
        msg_Err( p_this, "configuration variable channel-server empty" );
        return -1;
    }

    i_port = config_GetInt( p_this, "channel-port" );

    msg_Dbg( p_this, "connecting to %s:%d", psz_vlcs, i_port );

    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = "";
    socket_desc.i_bind_port = 4321;
    socket_desc.psz_server_addr = psz_vlcs;
    socket_desc.i_server_port = i_port;

    /* Find an appropriate network module */
    p_network = module_Need( p_this, "network", psz_network/*, &socket_desc*/ );
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_this, p_network );

    free( psz_vlcs ); /* Do we really need this ? -- Meuuh */
    i_fd = socket_desc.i_handle;

    /* Look for the interface MAC address */
    if( GetMacAddress( p_this, i_fd, psz_mac ) )
    {
        msg_Err( p_this, "failed getting MAC address" );
        close( i_fd );
        return -1;
    }

    msg_Dbg( p_this, "MAC address is %s", psz_mac );

    /* Build the message */
    sprintf( psz_mess, "%d %u %lu %s \n", i_channel, VLCS_VERSION,
                       (unsigned long)(mdate() / (u64)1000000),
                       psz_mac );

    /* Send the message */
    send( i_fd, psz_mess, MESSAGE_LENGTH, 0 );

    msg_Dbg( p_this, "attempting to join channel %d", i_channel );

    /* We have changed channels ! (or at least, we tried) */
    p_this->p_vlc->p_channel->last_change = mdate();
    p_this->p_vlc->p_channel->i_channel = i_channel;

    /* Wait 5 sec for an answer from the server */
    delay.tv_sec = 5;
    delay.tv_usec = 0;
    FD_ZERO( &fds );
    FD_SET( i_fd, &fds );

    switch( select( i_fd + 1, &fds, NULL, NULL, &delay ) )
    {
        case 0:
            msg_Err( p_this, "no answer from vlcs" );
            close( i_fd );
            return -1;

        case -1:
            msg_Err( p_this, "error while listening to vlcs" );
            close( i_fd );
            return -1;
    }

    recv( i_fd, psz_mess, MESSAGE_LENGTH, 0 );
    psz_mess[ MESSAGE_LENGTH - 1 ] = '\0';

    if( !strncasecmp( psz_mess, "E:", 2 ) )
    {
        msg_Err( p_this, "vlcs said '%s'", psz_mess + 2 );
        close( i_fd );
        return -1;
    }
    else if( !strncasecmp( psz_mess, "I:", 2 ) )
    {
        msg_Dbg( p_this, "vlcs said '%s'", psz_mess + 2 );
    }
    else
    {
        /* We got something to play ! */
        playlist_t *p_playlist;
        p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
        if( p_playlist != NULL )
        {
            playlist_Add( p_playlist, psz_mess,
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }

    /* Close the socket and return nicely */
#ifndef WIN32
    close( i_fd );
#else
    closesocket( i_fd );
#endif

    return 0;
}

/* Following functions are local */

/*****************************************************************************
 * GetMacAddress: extract the MAC Address
 *****************************************************************************/
static int GetMacAddress( vlc_object_t *p_this, int i_fd, char *psz_mac )
{
#if defined( SYS_LINUX )
    struct ifreq interface;
    int i_ret;
    char *psz_interface;

    /*
     * Looking for information about the eth0 interface
     */
    interface.ifr_addr.sa_family = AF_INET;
    if( !(psz_interface = config_GetPsz( p_this, "iface" )) )
    {
        msg_Err( p_this, "configuration variable iface empty" );
        return -1;
    }
    strcpy( interface.ifr_name, psz_interface );
    free( psz_interface );

    i_ret = ioctl( i_fd, SIOCGIFHWADDR, &interface );

    if( i_ret )
    {
        msg_Err( p_this, "ioctl SIOCGIFHWADDR failed" );
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

#elif defined( SYS_DARWIN )
    char *psz_interface;
    io_object_t ctrl_service;
    CFTypeRef cfd_mac_address;
    UInt8 ui_mac_address[kIOEthernetAddressSize];

    if( !(psz_interface = config_GetPsz( p_this, "iface" )) )
    {
        msg_Err( p_this, "configuration variable iface empty" );
        return( -1 );
    }

    if( GetNetIntfCtrl( psz_interface, &ctrl_service ) )
    {
        msg_Err( p_this, "GetNetIntfCtrl failed" );
        return( -1 );
    }

    cfd_mac_address = IORegistryEntryCreateCFProperty( ctrl_service,
                                                       CFSTR(kIOMACAddress),
                                                       kCFAllocatorDefault,
                                                       0 ); 
    IOObjectRelease( ctrl_service );
    if( cfd_mac_address == NULL )
    {
        msg_Err( p_this, "IORegistryEntryCreateCFProperty failed" );
        return( -1 );
    }

    CFDataGetBytes( cfd_mac_address, 
                    CFRangeMake(0, kIOEthernetAddressSize), 
                    ui_mac_address );
    CFRelease( cfd_mac_address );

    sprintf( psz_mac, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
                      ui_mac_address[0], ui_mac_address[1],
                      ui_mac_address[2], ui_mac_address[3],
                      ui_mac_address[4], ui_mac_address[5] ); 

    return( 0 );

#elif defined( WIN32 )
    int i, i_ret = -1;

    /* Get adapter list - support for more than one adapter */
    LANA_ENUM AdapterList;
    NCB       Ncb;

    msg_Dbg( p_this, "looking for MAC address" );

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
    strcpy( psz_mac, "00:00:00:00:00:00" );
    return( 0 );

#endif
}

#ifdef SYS_DARWIN
/*****************************************************************************
 * GetNetIntfCtrl : get parent controller for network interface 
 *****************************************************************************/
static int GetNetIntfCtrl( const char *psz_interface,
                           io_object_t *ctrl_service )
{
    mach_port_t port;
    kern_return_t ret;
    io_object_t intf_service;
    io_iterator_t intf_iterator;

    /* get port for IOKit communication */
    if( ( ret = IOMasterPort( MACH_PORT_NULL, &port ) ) != KERN_SUCCESS )
    {
        return( -1 );
    }

    /* look up the IOService object for the interface */
    ret = IOServiceGetMatchingServices( port, IOBSDNameMatching( port, 0, 
                                        psz_interface ), &intf_iterator ); 
    if( ret != KERN_SUCCESS )
    {
        return( -1 );
    }

    intf_service = IOIteratorNext( intf_iterator );
    if( intf_service == NULL )
    {
        return( -1 );
    }

    ret = IORegistryEntryGetParentEntry( intf_service,
                                         kIOServicePlane,
                                         ctrl_service );
    IOObjectRelease( intf_service );
    if( ret != KERN_SUCCESS )
    {
        return( -1 );
    }

    return( 0 );
}

#elif defined( WIN32 )
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
//X        intf_ErrMsg( "network error: reset returned %i", Ncb.ncb_retcode );
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

//X        intf_WarnMsg( 2, "network: found MAC address %s", psz_string );

        return 0;
    }
    else
    {
//X        intf_ErrMsg( "network error: ASTAT returned %i", Ncb.ncb_retcode );
        return -1;
    }
}
#endif /* WIN32 */

