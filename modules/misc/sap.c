/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sap.c,v 1.1 2002/12/03 16:29:04 gitan Exp $
 *
 * Authors: Arnaud Schauly <gitan@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>


#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include "network.h"

#define MAX_LINE_LENGTH 256

#define HELLO_PORT 9875  // SAP is aloways on that port
#define HELLO_GROUP "239.255.255.255"   // FIX ME !!!!!!!!!!!!!!!
#define ADD_SESSION 1;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/



static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );
static int Kill          ( intf_thread_t * );
        
static ssize_t NetRead    ( intf_thread_t*, int , byte_t *, size_t );

typedef struct  sess_descr_s {
/*    char *psz_version; // v field (protocol version) */
    char *psz_origin; /* o field (username sess-id sess-version 
                         nettype addrtype addr*/
    char *psz_sessionname; 
    char *psz_information; 
    char *psz_uri; 
    char *psz_emails;
    char *psz_phone;  
    char *psz_time; /* t start-time stop-time */
    char *psz_repeat; /* r repeat-interval typed-time */
    char *psz_attribute; 
    char *psz_media; /* m media port protocol */
} sess_descr_t;  
/* All this informations are not useful yet.  */

static int parse_sap ( char ** );
static int packet_handle ( char **, intf_thread_t * );

static sess_descr_t *  parse_sdp(char *  psz_pct) ;




/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("SAP"), NULL );
        add_string( "sap-addr", NULL, NULL, 
                     "SAP multicast address", "SAP multicast address" );
    set_description( _("SAP interface module") );
    set_capability( "interface", 0 );
    set_callbacks( Activate, NULL);
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: sap thread
 *****************************************************************************
 * Listens to SAP packets, and sends them to packet_handle
 *****************************************************************************/

static void Run( intf_thread_t *p_intf )
{
    char *psz_addr;
    char *psz_network = NULL;
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int fd,addrlen;
    char *psz_buf;

    module_t            *p_network;
    network_socket_t    socket_desc;
    
    
    if( !(psz_addr = config_GetPsz( p_intf, "sap-addr" ) ) )
    {
        psz_addr = strdup( HELLO_GROUP );
    }
    
    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = psz_addr;
    socket_desc.i_bind_port   = HELLO_PORT;
    socket_desc.psz_server_addr   = "";
    socket_desc.i_server_port     = 0;
    p_intf->p_private = (void*) &socket_desc;

    psz_network = "ipv4";
    
    if( !( p_network = module_Need( p_intf, "network", psz_network ) ) )
    {
        msg_Err( p_intf, "failed to open a connection (udp)" );
        return;
    }
    module_Unneed( p_intf, p_network ); 

    fd = socket_desc.i_handle;
    
    psz_buf = malloc( 1500 ); // FIXME!!

    /* read SAP packets */
    while( !p_intf->b_die )
    {
        int i_read;
        
        addrlen=sizeof(addr);
    
    
        memset(psz_buf, 0, 1500);
        
        i_read = NetRead( p_intf, fd, psz_buf, 1500 );
        
        if( i_read < 0 )
        {
            msg_Err( p_intf, "argggg" );
        }
        if( i_read == 0 )
        {
            continue;
        }


        msg_Dbg( p_intf, "New packet arrived" );
        packet_handle( &psz_buf, p_intf  );
                                   
    }
    free( psz_buf );

}

/********************************************************************
 * Kill 
 *******************************************************************
 * Kills the SAP interface. 
 ********************************************************************/
static int Kill( intf_thread_t *p_intf )
{

    p_intf->b_die = VLC_TRUE;

    return VLC_SUCCESS;
}

 
/********************************************************************
 * parse_sap : Takes care of the SAP headers
 ********************************************************************
 * checks if the packet has the true headers ; 
 ********************************************************************/

static int parse_sap( char **  ppsz_sa_packet ) {  /* Dummy Parser : does nothing !*/
/*   int j;
   
   for (j=0;j<255;j++) {
       fprintf(stderr, "%c",(*ppsz_sa_packet)[j]);
   }*/
   
   return ADD_SESSION; //Add this packet
}

/***********************************************************************
 * packet_handle : handle the received packet and enques the 
 * the understated session
 * ******************************************************************/

static int packet_handle( char **  ppsz_packet, intf_thread_t * p_intf )  {
    int j=0;
    sess_descr_t * sd;
    playlist_t *p_playlist; 
    char *  psz_enqueue;
    char *  psz_udp = "udp://@\0";
    int i;
    
    j=parse_sap( ppsz_packet ); 
    
    if(j != 0) {
        msg_Dbg( p_intf, "SAP packet parsed");
        sd = parse_sdp( * ppsz_packet ); 
        
        i = strlen( psz_udp ) + strlen( sd->psz_uri )+1 ;
        psz_enqueue = malloc ( i * sizeof (char) );
        memset( psz_enqueue, '\0',i );
        strcat ( psz_enqueue,psz_udp );
        strcat ( psz_enqueue,sd->psz_uri );
        
        p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        playlist_Add ( p_playlist, psz_enqueue, PLAYLIST_CHECK_INSERT, PLAYLIST_END); 
        msg_Dbg(p_intf,"SAP packet : End of treatment");
        vlc_object_release( p_playlist );
        
        return 1;
    }
    free ( sd );
    return 0; // Invalid Packet 
}
        



/******************************************************
 * parse_sap : SDP parsing
 * ****************************************************
 * Make a sess_descr_t with a psz
 ******************************************************/

static sess_descr_t *  parse_sdp(char *  psz_pct) 
{
    int j,k;
    char **  ppsz_fill;
    sess_descr_t *  sd;
    
    sd = malloc( sizeof(sess_descr_t) );
    for (j=0 ; j < 255 ; j++) 
    {
       if (psz_pct[j] == '=') 
       {           
           switch(psz_pct[(j-1)]) {
/*               case ('v') : {
//               ppsz_fill = & sd->psz_version;
//               break;
            } */
            case ('o') : {
               ppsz_fill = & sd->psz_origin;
               break;
            }
            case ('s') : {
               ppsz_fill = & sd->psz_sessionname;
               break;
            }
            case ('i') : {
               ppsz_fill = & sd->psz_information;
               break;
            }
            case ('u') : {
               ppsz_fill = & sd->psz_uri;
               break;
            }
            case ('e') : {
               ppsz_fill = & sd->psz_emails;
               break;
            }
            case ('p') : {
               ppsz_fill = & sd->psz_phone;
               break;
            }
            case ('t') : {
               ppsz_fill = & sd->psz_time;
               break;
            }
            case ('r') : {
               ppsz_fill = & sd->psz_repeat;
               break;
            }
            case ('a') : {
               ppsz_fill = & sd->psz_attribute;
               break;
            }
            case ('m') : {
               ppsz_fill = & sd->psz_media;
               break;
            }

            default : { 
               fprintf( stderr,"Warning : Ignored field \"%c\" \n",psz_pct[j-1] );
               ppsz_fill = NULL;
            }


         } 
      k=0;j++;
      while (psz_pct[j] != '\n'&& psz_pct[j] != EOF) {
         k++; j++;
      }
      j--;
      if( ppsz_fill != NULL )
      {
         *ppsz_fill= malloc( sizeof(char) * (k+1) );
         memccpy(*ppsz_fill, &(psz_pct[j-k+1]),'\n',  k ); 
         (*ppsz_fill)[k]='\0';
      }
      ppsz_fill = NULL;

      } // if
   } //for

   return sd;
}



/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************
 * Taken from udp.c
 ******************************************************************************/
static ssize_t NetRead( intf_thread_t *p_intf, 
                        int i_handle, byte_t *p_buffer, size_t i_len)
{
#ifdef UNDER_CE
    return -1;
    
#else
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;
    
    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( i_handle, &fds );
    
    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    
    /* Find if some data is available */
    i_ret = select( i_handle + 1, &fds,
    NULL, NULL, &timeout );
    
    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_intf, "network select error (%s)", strerror(errno) );
    }
    else if( i_ret > 0 )
    {
        ssize_t i_recv = recv( i_handle, p_buffer, i_len, 0 );
    
        if( i_recv < 0 )
        {
           msg_Err( p_intf, "recv failed (%s)", strerror(errno) );
        }
    
        return i_recv;
    }
    
    return 0;

#endif
}

