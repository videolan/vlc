/*****************************************************************************
 * netutils.c: various network functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: netutils.c,v 1.30 2001/05/18 09:49:16 xav Exp $
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
#include <string.h>                                      /* bzero(), bcopy() */
#include <unistd.h>                                         /* gethostname() */
#include <sys/time.h>                                        /* gettimeofday */

#ifndef WIN32
#include <netdb.h>                                        /* gethostbyname() */
#include <netinet/in.h>                               /* BSD: struct in_addr */
#include <sys/socket.h>                              /* BSD: struct sockaddr */
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>                           /* inet_ntoa(), inet_aton() */
#endif

#ifdef SYS_LINUX
#include <sys/ioctl.h>                                            /* ioctl() */
#endif

#ifdef WIN32                            /* tools to get the MAC adress from  */
#include <windows.h>                    /* the interface under Windows	     */
#include <stdio.h>
#include <iostream>
#include <strstream>
#include <string>
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
    int         i_channel_id;                      /* current channel number */
    mtime_t     last_change;                             /* last change date */
} input_channel_t;


#ifdef WIN32

/*****************************************************************************
 * GetAdapterInfo : gets some informations about the interface using NETBIOS *
 *****************************************************************************/ 


  using namespace std ;

  bool GetAdapterInfo ( int nAdapterNum, string & sMAC )
  {
      /* Reset the LAN adapter so that we can begin querying it */
      
    NCB Ncb ;
    memset ( &Ncb, 0, sizeof ( Ncb ) ) ;
    Ncb.ncb_command = NCBRESET ;
    Ncb.ncb_lana_num = nAdapterNum ;
    
    if ( Netbios ( &Ncb ) != NRC_GOODRET ) 
    {
        char acTemp [ 80 ] ;
        ostrstream outs ( acTemp, sizeof ( acTemp ) ) ;
	
        /* FIXME This should use vlc's standard handling error functions */ 
	  
        outs << "error " << Ncb.ncb_retcode << " on reset" << ends ;
        sMAC = acTemp ;
        return false ;
    }
      
    /* Prepare to get the adapter status block */
      
    memset ( &Ncb, 0, sizeof ( Ncb ) ) ;     /* Initialization */
    Ncb.ncb_command = NCBASTAT ;
    Ncb.ncb_lana_num = nAdapterNum ;
    
    strcpy ( ( char * ) Ncb.ncb_callname, "*" ) ;
    
    struct ASTAT {
        ADAPTER_STATUS adapt ;
        NAME_BUFFER NameBuff[30] ;
    } Adapter ;
      
    memset ( &Adapter, 0, sizeof ( Adapter ) ) ;
    Ncb.ncb_buffer = ( unsigned char * ) &Adapter ;
    Ncb.ncb_length = sizeof ( Adapter ) ;
      
      /* Get the adapter's info and, if this works, return it in standard,
      colon-delimited form. */
      
    if ( Netbios( &Ncb ) == 0 ) 
    {
        char acMAC [ 18 ] ;
        sprintf ( acMAC, "%02X:%02X:%02X:%02X:%02X:%02X",
                int ( Adapter.adapt.adapter_address[0] ),
                int ( Adapter.adapt.adapter_address[1] ),
                int ( Adapter.adapt.adapter_address[2] ),
                int ( Adapter.adapt.adapter_address[3] ),
                int ( Adapter.adapt.adapter_address[4] ),
                int ( Adapter.adapt.adapter_address[5] ) );
        sMAC = acMAC;
        return true;
    }
    else 
    {
        char acTemp[80] ;
        ostrstream outs ( acTemp, sizeof ( acTemp ) ) ;
        /* FIXME Same thing as up there */
	  
        outs << "error " << Ncb.ncb_retcode << " on ASTAT" << ends;
 
        sMAC = acTemp;
        return false;
    }
}

/*****************************************************************************
 * GetMacAddress : Extracts the MAC Address from the informations collected in
 * GetAdapterInfo
 ****************************************************************************/

string GetMacAddress()
{
    /* Get adapter list - support for more than one adapter */
    
    LANA_ENUM AdapterList ;
    NCB Ncb ;
   
    memset ( &Ncb, 0, sizeof ( NCB ) ) ;
    Ncb.ncb_command = NCBENUM ;
    Ncb.ncb_buffer = ( unsigned char * ) &AdapterList ;
    Ncb.ncb_length = sizeof ( AdapterList ) ;
    Netbios ( &Ncb ) ;
    
    /* Get all of the local ethernet addresses */
      
    string sMAC;
    
    for ( int i = 0; i < AdapterList.length ; ++i ) 
    {
        if ( GetAdapterInfo ( AdapterList.lana [ i ] , sMAC ) ) 
        {
            cout << "Adapter " << int ( AdapterList. lana [ i ] ) <<
                    "'s MAC is " << sMAC << endl ;
        }
        else 
        {

	    /* FIXME those bloody error messages */
		  
            cerr << "Failed to get MAC address! Do you" << endl;
            cerr << "have the NetBIOS protocol installed?" << endl;
	      
            break;
        }
    }

    return sMac;
}

#endif

/*****************************************************************************
 * network_BuildLocalAddr : fill a sockaddr_in structure for local binding
 *****************************************************************************/
int network_BuildLocalAddr( struct sockaddr_in * p_socket, int i_port, 
                            char * psz_broadcast )
{
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
}

/*****************************************************************************
 * network_BuildRemoteAddr : fill a sockaddr_in structure for remote host
 *****************************************************************************/
int network_BuildRemoteAddr( struct sockaddr_in * p_socket, char * psz_server )
{
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
/* Even when BSD are supported, BeOS is not likely to be supported, so 
 * I prefer to put it apart */    
#if defined( SYS_BEOS )
    intf_ErrMsg( "error: channel changing is not yet supported under BeOS" );
    return( 1 );

#elif defined( SYS_LINUX )
/* FIXME : channels handling only work for linux */
    /* Allocate structure */
    p_main->p_channel = malloc( sizeof( input_channel_t ) );
    if( p_main->p_channel == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( -1 );
    }

    /* Initialize structure */
    p_main->p_channel->i_channel_id   = 0;
    p_main->p_channel->last_change = 0;

    intf_Msg("Channels initialized\n");
    return( 0 );

#else
    intf_ErrMsg( "error : channel changing only works with linux yest" );
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
 * is effective. Note that once a channel is no more used, it's interface 
 * should be unlocked using input_ChannelLeave().
 * Non 0 will be returned in case of error.
 *****************************************************************************/
int network_ChannelJoin( int i_channel_id )
{
/* I still prefer to put BeOS a bit apart */   
#if defined( SYS_BEOS )
    intf_ErrMsg( "Channels are not yet supported under BeOS" );
    return( -1 );

#elif defined( SYS_LINUX )
    int                 i_socket_cl;
    int                 i_fromlen;
    struct ifreq        s_interface;
    struct sockaddr_in  sa_server;
    struct sockaddr_in  sa_client;
    unsigned int        i_version = 12;
    char                psz_mess[80];
    char                i_mess_length = 80;
    struct timeval *    p_date_cl;
    struct timeval      s_time;
    long unsigned int   i_date;
    int                 i_nbanswer;
    char                i_answer;
    fd_set              s_rfds;
    unsigned int        i_rc;
 
    if( ! p_main->b_channels )
    {
        intf_ErrMsg( "Channels disabled. To enable them, use the --channels"
                     " option" );
        return( -1 );
    }
    /* debug */ 
    intf_DbgMsg( "ChannelJoin : %d", i_channel_id );
    /* If last change is too recent, wait a while */
    if( mdate() - p_main->p_channel->last_change < INPUT_CHANNEL_CHANGE_DELAY )
    {
        intf_Msg( "Waiting before changing channel...\n" );
        mwait( p_main->p_channel->last_change + INPUT_CHANNEL_CHANGE_DELAY );
    }
    p_main->p_channel->last_change = mdate();
    p_main->p_channel->i_channel_id = i_channel_id;

    intf_Msg( "Joining channel %d\n", i_channel_id );

    /*      
     * Looking for information about the eth0 interface
     */
    s_interface.ifr_addr.sa_family = AF_INET;
    strcpy( s_interface.ifr_name, INPUT_IFACE_DEFAULT );
    
    
    /*
     * Initialysing the socket
     */
    i_socket_cl=socket( AF_INET, SOCK_DGRAM, 0 );

    
    /* 
     * Getting the server's information 
     */
    bzero( &sa_server, sizeof(struct sockaddr_in) );
    sa_server.sin_family = AF_INET;
    sa_server.sin_port   = htons( INPUT_CHANNEL_PORT_DEFAULT );
    inet_aton( INPUT_CHANNEL_SERVER_DEFAULT, &(sa_server.sin_addr) );

    /*
     * Looking for the interface MAC address
     */
    ioctl( i_socket_cl, SIOCGIFHWADDR, &s_interface );
    intf_DbgMsg(
        "CHANNELSERVER: macaddr == %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
        s_interface.ifr_hwaddr.sa_data[0] & 0xff,
        s_interface.ifr_hwaddr.sa_data[1] & 0xff,
        s_interface.ifr_hwaddr.sa_data[2] & 0xff,
        s_interface.ifr_hwaddr.sa_data[3] & 0xff,
        s_interface.ifr_hwaddr.sa_data[4] & 0xff,
        s_interface.ifr_hwaddr.sa_data[5] & 0xff );
    
    /*
     * Getting date of the client
     */
    p_date_cl=malloc( sizeof(struct timeval) );
    if( p_date_cl == NULL )
    {
        intf_ErrMsg( "CHANNELSERVER: unable to allocate memory\n" );
    /*    return VS_R_MEMORY;*/
        return( -1);
    }
    
    if ( gettimeofday( p_date_cl, 0 ) == -1 )
    {
        return( -1);
    }
    i_date = p_date_cl->tv_sec;
    free( p_date_cl );
    intf_DbgMsg( "CHANNELSERVER: date %lu\n", i_date );


    /* 
     * Build of the message
     */
    sprintf( psz_mess, "%d %u %lu %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x \n",
          i_channel_id, i_version, i_date,
        s_interface.ifr_hwaddr.sa_data[0] & 0xff, 
        s_interface.ifr_hwaddr.sa_data[1] & 0xff,
        s_interface.ifr_hwaddr.sa_data[2] & 0xff,
        s_interface.ifr_hwaddr.sa_data[3] & 0xff,
        s_interface.ifr_hwaddr.sa_data[4] & 0xff,
        s_interface.ifr_hwaddr.sa_data[5] & 0xff );
 
    intf_DbgMsg( "CHANNELSERVER: The message is %s\n", psz_mess );

    /*
     * Open the socket 2
     */
    bzero( &sa_client, sizeof(struct sockaddr_in) );
    sa_client.sin_family = AF_INET;
    sa_client.sin_port   = htons(4312);
    sa_client.sin_addr.s_addr = INADDR_ANY;
    i_fromlen = sizeof( struct sockaddr );
    i_rc = bind( i_socket_cl, (struct sockaddr *)(&sa_client),\
                 sizeof(struct sockaddr) );
    if ( i_rc )
    {
        intf_ErrMsg( "CHANNELSERVER: Unable to bind socket:%u\n", i_rc ); 
    /* TODO put CS_R_BIND in types.h*/
    /*    return CS_R_SOCKET;*/
        return -1;
    }


    /*
     * Send the message
     */
    sendto( i_socket_cl, psz_mess, i_mess_length, 0, \
            (struct sockaddr *)(&sa_server),   \
            sizeof(struct sockaddr) );
   
     /*
     * Waiting 5 sec for one answer from the server
     */
    s_time.tv_sec  = 5;
    s_time.tv_usec = 0;
    FD_ZERO( &s_rfds );
    FD_SET( i_socket_cl, &s_rfds );
    i_nbanswer = select( i_socket_cl+1, &s_rfds, NULL, NULL, &s_time );
    if( i_nbanswer == 0 )
    {
        intf_DbgMsg( "CHANNELSERVER: no answer\n" );
    }
    else if( i_nbanswer == -1 )
    {
        intf_DbgMsg( "CHANNELSERVER: Unable to receive the answer\n ");
    }
    else
    {
        recvfrom( i_socket_cl, &i_answer, sizeof(char), 0,\
                  (struct sockaddr *)(&sa_client), &i_fromlen);
        intf_DbgMsg( "CHANNELSERVER: the answer : %hhd\n", i_answer );
        if( i_answer == -1 )
        {
            intf_DbgMsg(
                  "CHANNELSERVER: The server failed to create the thread\n" );
        }
        else if( i_answer == 0 )
        {
            intf_DbgMsg(
                  "CHANNELSERVER: The server tries to change the channel\n" );
        }
        else
        {
            intf_DbgMsg( "CHANNELSERVER: Unknown answer !\n" );
        }
    }
    
    /*
     * Close the socket
     */
    close( i_socket_cl );

    return( 0 );

#else
    intf_ErrMsg( "Channels only work under linux yet" );
    return( -1 );

#endif
}

