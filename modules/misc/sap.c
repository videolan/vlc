/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sap.c,v 1.5 2002/12/10 00:02:29 gitan Exp $
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
#   if HAVE_ARPA_INET_H
#      include <arpa/inet.h>
#   elif defined( SYS_BEOS )
#      include <net/netdb.h>
#   endif
#endif

#include "network.h"

#define MAX_LINE_LENGTH 256

/* SAP is always on that port */
#define HELLO_PORT 9875
#define HELLO_GROUP "224.2.127.254"
#define ADD_SESSION 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct media_descr_t media_descr_t;
typedef struct sess_descr_t sess_descr_t;

static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );
static int Kill          ( intf_thread_t * );

static ssize_t NetRead    ( intf_thread_t*, int , byte_t *, size_t );

/* playlist related functions */
static int  sess_toitem( intf_thread_t *, sess_descr_t * );

/* sap/sdp related functions */
static int parse_sap ( char ** );
static int packet_handle ( intf_thread_t *, char ** );
static sess_descr_t *  parse_sdp( char *,intf_thread_t * ) ;

/* specific sdp fields parsing */

static void cfield_parse( char *, char ** );
static void mfield_parse( char *psz_mfield, char **ppsz_proto,
               char **ppsz_port );

static void free_sd( sess_descr_t * );
static int  ismult( char * );

/* The struct that contains sdp informations */
struct  sess_descr_t {
    char *psz_version;
    char *psz_origin;
    char *psz_sessionname;
    char *psz_information;
    char *psz_uri;
    char *psz_emails;
    char *psz_phone;
    char *psz_time;
    char *psz_repeat;
    char *psz_attribute;
    char *psz_connection;
    int  i_media;
    media_descr_t ** pp_media;
};

/* All this informations are not useful yet.  */
struct media_descr_t {
    char *psz_medianame;
    char *psz_mediaconnection;
};

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
    int fd,addrlen;
    char *psz_buf;

    module_t            *p_network;
    network_socket_t    socket_desc;

    psz_buf = NULL;

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

    psz_network = "ipv4"; // FIXME

    /* Create, Bind the socket, ... with the appropriate module  */

    if( !( p_network = module_Need( p_intf, "network", psz_network ) ) )
    {
        msg_Err( p_intf, "failed to open a connection (udp)" );
        return;
    }
    module_Unneed( p_intf, p_network );

    fd = socket_desc.i_handle;


    /* read SAP packets */

    psz_buf = malloc( 2000 );

    while( !p_intf->b_die )
    {
        int i_read;
        addrlen=sizeof(addr);


        memset(psz_buf, 0, 2000);

        i_read = NetRead( p_intf, fd, psz_buf, 2000 );

        if( i_read < 0 )
        {
            msg_Err( p_intf, "Cannot read in the socket" );
        }
        if( i_read == 0 )
        {
            continue;
        }


        packet_handle( p_intf,  &psz_buf );

    }
    free( psz_buf );

    /* Closing socket */

#ifdef UNDER_CE
    CloseHandle( socket_desc.i_handle );
#elif defined( WIN32 )
    closesocket( socket_desc.i_handle );
#else
    close( socket_desc.i_handle );
#endif

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

/*******************************************************************
 * sess_toitem : changes a sess_descr_t into a hurd of
 * playlist_item_t, which are enqueued.
 *******************************************************************
 * Note : does not support sessions that take place on consecutive
 * port or adresses yet.
 *******************************************************************/

static int sess_toitem( intf_thread_t * p_intf, sess_descr_t * p_sd )
{
    playlist_item_t * p_item;
    char *psz_uri, *psz_proto;
    char *psz_port;
    char *psz_uri_default;
    int i_multicast;
    int i_count;
    playlist_t *p_playlist;

    psz_uri_default = NULL;
    cfield_parse( p_sd->psz_connection, &psz_uri_default );

    for( i_count=0 ; i_count <= p_sd->i_media ; i_count ++ )
    {

        p_item = malloc( sizeof( playlist_item_t ) );
        p_item->psz_name = strdup( p_sd->psz_sessionname );
        p_item->i_type = 0;
        p_item->i_status = 0;
        p_item->b_autodeletion = VLC_FALSE;
        p_item->psz_uri = NULL;

        psz_uri = NULL;

        /* Build what we have to put in p_item->psz_uri, with the m and
         *  c fields  */

        if( !p_sd->pp_media[i_count] )
        {
            return 0;
        }

        mfield_parse( p_sd->pp_media[i_count]->psz_medianame,
                        & psz_proto, & psz_port );

        if( !psz_proto || !psz_port )
        {
            return 0;
        }

        if( p_sd->pp_media[i_count]->psz_mediaconnection )
        {
            cfield_parse( p_sd->pp_media[i_count]->psz_mediaconnection,
                            & psz_uri );
        }
        else
        {
            psz_uri = NULL;
        }

        if( psz_uri == NULL )
        {
            if( psz_uri_default )
            {
                psz_uri = psz_uri_default;
            }
            else
            {
                return 0;
            }
        }


        /* Filling p_item->psz_uri */
        i_multicast = ismult( psz_uri );

        p_item->psz_uri = malloc( strlen( psz_proto ) + strlen( psz_uri ) +
                        strlen( psz_port ) + 5 +i_multicast );
        if( p_item->psz_uri == NULL )
        {
            msg_Err( p_intf, "Not enough memory");
            return 0;
        }

        if( i_multicast == 1)
        {
            sprintf( p_item->psz_uri, "%s://@%s:%s", psz_proto,
                            psz_uri, psz_port );
        }
        else
        {
            sprintf( p_item->psz_uri, "%s://%s:%s", psz_proto,
                            psz_uri, psz_port );
        }

        /* Enqueueing p_item in the playlist */

        if( p_item )
        {
            p_playlist = vlc_object_find( p_intf,
            VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

            playlist_AddItem ( p_playlist, p_item,
            PLAYLIST_CHECK_INSERT, PLAYLIST_END);
            vlc_object_release( p_playlist );
        }


    }


    return 1;

}

/**********************************************************************
 * cfield_parse
 *********************************************************************
 * put into *ppsz_uri, the the uri in the cfield, psz_cfield.
 *********************************************************************/

static void cfield_parse( char *psz_cfield, char **ppsz_uri )
{

    char *psz_pos;
    if( psz_cfield )
    {
        psz_pos = psz_cfield;

        while( *psz_pos != ' ' && *psz_pos !='\0' )
        {
            psz_pos++;
        }
        psz_pos++;
        while( *psz_pos != ' ' && *psz_pos !='\0' )
        {
            psz_pos++;
        }
        psz_pos++;
        *ppsz_uri = psz_pos;
        while( *psz_pos != ' ' && *psz_pos !='/'
                        && *psz_pos != '\0' )
        {
            psz_pos++;
        }
        *psz_pos = '\0';

    }
    else
    {
        ppsz_uri = NULL;
    }

    return;

}

/**********************************************************************
 * mfield_parse
 *********************************************************************
 * put into *ppsz_proto, and *ppsz_port, the protocol and the port.
 *********************************************************************/


static void mfield_parse( char *psz_mfield, char **ppsz_proto,
               char **ppsz_port )
{
    char *psz_pos;
    if( psz_mfield )
    {
        psz_pos = psz_mfield;
        while( *psz_pos != '\0' && *psz_pos != ' ' )
        {
            psz_pos++;
        }
        psz_pos++;
        *ppsz_port = psz_pos;
        while( *psz_pos != '\0' && *psz_pos && *psz_pos !=' ' && *psz_pos!='/' )
        {
            psz_pos++;
        }
        if( *psz_pos == '/' )  // FIXME does not support multi-port
        {
            *psz_pos = '\0';
            psz_pos++;
            while( *psz_pos != '\0' && *psz_pos !=' ' )
            {
            psz_pos++;
            }
        }
        *psz_pos = '\0';
        psz_pos++;
        *ppsz_proto = psz_pos;
        while( *psz_pos!='\0' && *psz_pos !=' ' && 
                        *psz_pos!='/' )
        {
            *psz_pos = tolower( *psz_pos );
            psz_pos++;
        }
        *psz_pos = '\0';
    }
    else
    {
        *ppsz_proto = NULL;
        *ppsz_port = NULL;
    }
    return;
}

/***********************************************************************
 * parse_sap : Takes care of the SAP headers
 ***********************************************************************
 * checks if the packet has the true headers ;
 ***********************************************************************/

static int parse_sap( char **  ppsz_sa_packet ) {  /* Dummy Parser : does nothing !*/
   if( *ppsz_sa_packet )  return ADD_SESSION; //Add this packet
   return 0; /* FIXME */
}

/*************************************************************************
 * packet_handle : handle the received packet and enques the
 * the understated session
 *************************************************************************/

static int packet_handle( intf_thread_t * p_intf, char **  ppsz_packet )  {
    int j=0;
    sess_descr_t * p_sd;

    j=parse_sap( ppsz_packet );

    if(j != 0) {

        p_sd = parse_sdp( *ppsz_packet, p_intf );


        sess_toitem ( p_intf, p_sd );

        free_sd ( p_sd );
        return 1;
    }
    return 0; // Invalid Packet
}




/***********************************************************************
 * parse_sdp : SDP parsing
 * *********************************************************************
 * Make a sess_descr_t with a psz
 ***********************************************************************/

static sess_descr_t *  parse_sdp( char *  psz_pct, intf_thread_t * p_intf )
{
    int j,k;
    char **  ppsz_fill=NULL;
    sess_descr_t *  sd;

    sd = malloc( sizeof(sess_descr_t) );


    if( sd == NULL )
    {
        msg_Err( p_intf, "out of memory" );
    }
    else
    {
        sd->pp_media = NULL;
        sd->psz_origin = NULL;
        sd->psz_sessionname = NULL;
        sd->psz_information = NULL;
        sd->psz_uri = NULL;
        sd->psz_emails = NULL;
        sd->psz_phone = NULL;
        sd->psz_time = NULL;
        sd->psz_repeat = NULL;
        sd->psz_attribute = NULL;
        sd->psz_connection = NULL;

        sd->i_media=-1;
        j=0;
        while( psz_pct[j]!=EOF && psz_pct[j] != '\0' )
        {
           j++;
           if (psz_pct[j] == '=')
           {
               switch(psz_pct[(j-1)]) {
               case ('v') : {
                     ppsz_fill = & sd->psz_version;
                     break;
                }
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
                   sd->i_media++;
                   if( sd->pp_media ) {
                       sd->pp_media = realloc( sd->pp_media,
                                ( sizeof( void * )  * (sd->i_media + 1)) );
                   }
                   else
                   {
                       sd->pp_media = malloc( sizeof ( void * ) );
                   }
                   sd->pp_media[sd->i_media] =
                           malloc( sizeof( media_descr_t ) );

                   sd->pp_media[sd->i_media]->psz_medianame = NULL;
                   sd->pp_media[sd->i_media]->psz_mediaconnection = NULL;

                   ppsz_fill = & sd->pp_media[sd->i_media]->psz_medianame;
                   break;
                }
                case ('c') : {
                   if( sd->i_media == -1 )
                   {
                       ppsz_fill = & sd->psz_connection;
                   }
                   else
                   {
                       ppsz_fill = & sd->pp_media[sd->i_media]->
                               psz_mediaconnection;
                   }
                   break;
                }

                default : {
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
             memcpy(*ppsz_fill, &(psz_pct[j-k+1]), k );
             (*ppsz_fill)[k]='\0';
          }
          ppsz_fill = NULL;
          } // if
       } //for
    } //if

    return sd;
}

#define FREE( p ) \
    if( p ) { free( p ); (p) = NULL; }
static void free_sd( sess_descr_t * p_sd )
{
    int i;
    if( p_sd )
    {

    FREE( p_sd->psz_origin );
    FREE( p_sd->psz_sessionname );
    FREE( p_sd->psz_information );
    FREE( p_sd->psz_uri );
    FREE( p_sd->psz_emails );
    FREE( p_sd->psz_phone );
    FREE( p_sd->psz_time );
    FREE( p_sd->psz_repeat );
    FREE( p_sd->psz_attribute );
    FREE( p_sd->psz_connection );

    if( p_sd->i_media >= 0 && p_sd->pp_media )
    {
        for( i=0; i <= p_sd->i_media ; i++ )
        {
            FREE( p_sd->pp_media[i]->psz_medianame );
            FREE( p_sd->pp_media[i]->psz_mediaconnection );
        }
        FREE( p_sd->pp_media );
    }
    free( p_sd );
    }
    else
    {
        ;
    }
    return;
}

/***********************************************************************
 * ismult
 ***********************************************************************/

static int ismult( char *psz_uri )
{
    char *psz_c;
    int i;

    psz_c = malloc( 3 );

    memcpy( psz_c, psz_uri, 3 );
    if( psz_c[2] == '.' || psz_c[1] == '.' )
    {
        free( psz_c );
        return 0;
    }

    i = atoi( psz_c );
    if( i < 224 )
    {
        free( psz_c );
        return 0;
    }

    free( psz_c );
    return 1;
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

