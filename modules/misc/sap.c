/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sap.c,v 1.17 2003/07/02 18:44:27 zorglub Exp $
 *
 * Authors: Arnaud Schauly <gitan@via.ecp.fr>
 *          Clément Stenac <zorglub@via.ecp.fr>
 *          Damien Lucas <nitrox@videolan.org>
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

#ifdef UNDER_CE
#   define close(a) CloseHandle(a);
#elif defined( WIN32 )
#   define close(a) closesocket(a);
#endif

#include "network.h"

#define MAX_LINE_LENGTH 256

/* SAP is always on that port */
#define HELLO_PORT 9875
#define HELLO_GROUP "224.2.127.254"
#define ADD_SESSION 1

#define IPV6_ADDR_1 "FF0"  /* Scope is inserted between them */
#define IPV6_ADDR_2 "::2:7FFE"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct media_descr_t media_descr_t;
typedef struct sess_descr_t sess_descr_t;

static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );
static int Kill          ( intf_thread_t * );

static ssize_t NetRead    ( intf_thread_t*, int, int , byte_t *, size_t );

/* playlist related functions */
static int  sess_toitem( intf_thread_t *, sess_descr_t * );

/* sap/sdp related functions */
static int parse_sap ( char * );
static int packet_handle ( intf_thread_t *, char *, int );
static sess_descr_t *  parse_sdp( intf_thread_t *, char * ) ;

/* specific sdp fields parsing */

static void cfield_parse( char *, char ** );
static void mfield_parse( char *psz_mfield, char **ppsz_proto,
               char **ppsz_port );

static void free_sd( sess_descr_t * );

/* Detect multicast addresses */
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

#define SAP_ADDR_TEXT N_("SAP multicast address")
#define SAP_ADDR_LONGTEXT N_("SAP multicast address")
#define SAP_IPV4_TEXT N_("No IPv4-SAP listening")
#define SAP_IPV4_LONGTEXT N_("Set this if you do not want SAP to listen for IPv4 announces")
#define SAP_IPV6_TEXT N_("IPv6-SAP listening")
#define SAP_IPV6_LONGTEXT N_("Set this if you want SAP to listen for IPv6 announces")
#define SAP_SCOPE_TEXT N_("IPv6 SAP scope")
#define SAP_SCOPE_LONGTEXT N_("Sets the scope for IPv6 announces (default is 8)")

vlc_module_begin();
    add_category_hint( N_("SAP"), NULL, VLC_TRUE );
        add_string( "sap-addr", NULL, NULL,
                     SAP_ADDR_TEXT, SAP_ADDR_LONGTEXT, VLC_TRUE );
    
        add_bool( "no-sap-ipv4", 0 , NULL,
                     SAP_IPV4_TEXT,SAP_IPV4_LONGTEXT, VLC_TRUE);

        add_bool( "sap-ipv6", 0 , NULL,
                   SAP_IPV6_TEXT, SAP_IPV6_LONGTEXT, VLC_TRUE);

        add_string( "sap-ipv6-scope", "8" , NULL,
                    SAP_SCOPE_TEXT, SAP_SCOPE_LONGTEXT, VLC_TRUE);

    set_description( _("SAP interface") );
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
#define MAX_SAP_BUFFER 2000

static void Run( intf_thread_t *p_intf )
{
    char *psz_addr;
    char *psz_addrv6;
    char *psz_network = NULL;
    int fd            = - 1;
    int fdv6          = -1;
    
    int no_sap_ipv4      = config_GetInt( p_intf, "no-sap-ipv4" );
    int sap_ipv6         = config_GetInt( p_intf, "sap-ipv6" );
    char *sap_ipv6_scope = config_GetPsz( p_intf, "sap-ipv6-scope" );
    
    char buffer[MAX_SAP_BUFFER + 1];

    module_t            *p_network;
    network_socket_t    socket_desc;

    if( no_sap_ipv4 == -1 || sap_ipv6 == -1 || sap_ipv6_scope == NULL )
    {
        msg_Warn( p_intf, "Unable to parse module configuration" );
        return;
    }
    
    /* Prepare IPv4 Networking */
    if ( no_sap_ipv4 == 0)
    {
        if( !(psz_addr = config_GetPsz( p_intf, "sap-addr" ) ) )
        { 
            psz_addr = strdup( HELLO_GROUP );
        } 
            
        /* Prepare the network_socket_t structure */
        socket_desc.i_type            = NETWORK_UDP;
        socket_desc.psz_bind_addr     = psz_addr;
        socket_desc.i_bind_port       = HELLO_PORT;
        socket_desc.psz_server_addr   = "";
        socket_desc.i_server_port     = 0;
        p_intf->p_private = (void*) &socket_desc;

        psz_network = "ipv4"; 

       /* Create, Bind the socket, ... with the appropriate module  */
 
        if( !( p_network = module_Need( p_intf, "network", psz_network ) ) )
        {
            msg_Warn( p_intf, "failed to open a connection (udp)" );
            return;
        }
        module_Unneed( p_intf, p_network );

        fd = socket_desc.i_handle;
    }

    /* Prepare IPv6 Networking */
    if ( sap_ipv6 > 0)
    {
        /* Prepare the network_socket_t structure */

        psz_addrv6=(char *)malloc(sizeof(char)*38); 
        /* Max size of an IPv6 address */
        
        sprintf(psz_addrv6,"[%s%c%s]",IPV6_ADDR_1,
                        sap_ipv6_scope[0],IPV6_ADDR_2);
        
        socket_desc.i_type            = NETWORK_UDP;
        socket_desc.psz_bind_addr     = psz_addrv6;
        socket_desc.i_bind_port       = HELLO_PORT;
        socket_desc.psz_server_addr   = "";
        socket_desc.i_server_port     = 0;
        p_intf->p_private = (void*) &socket_desc;

        psz_network = "ipv6"; 

       /* Create, Bind the socket, ... with the appropriate module  */
 
        if( !( p_network = module_Need( p_intf, "network", psz_network ) ) )
        {
            msg_Warn( p_intf, "failed to open a connection (udp)" );
            return;
        }
        module_Unneed( p_intf, p_network );

        fdv6 = socket_desc.i_handle;
    }

    
    /* read SAP packets */
    while( !p_intf->b_die )
    {
        int i_read;

        //memset( buffer, 0, MAX_SAP_BUFFER + 1);

        i_read = NetRead( p_intf, fd, fdv6, buffer, MAX_SAP_BUFFER );

        if( i_read < 0 )
        {
            msg_Err( p_intf, "Cannot read in the socket" );
        }
        if( i_read == 0 )
        {
            continue;
        }
        buffer[i_read] = '\0';

        packet_handle( p_intf, buffer, i_read );

    }

    /* Closing socket */
    close( socket_desc.i_handle );
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

    for( i_count = 0 ; i_count < p_sd->i_media ; i_count++ )
    {
        p_item = malloc( sizeof( playlist_item_t ) );
        if( p_item == NULL )
        {
            msg_Err( p_intf, "Not enough memory for p_item in sesstoitem()" );
            return 0;
        }
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
            psz_uri = psz_uri_default;
        }

        if( psz_uri == NULL )
        {
            return 0;
        }


        /* Filling p_item->psz_uri */
        i_multicast = ismult( psz_uri );

        p_item->psz_uri = malloc( strlen( psz_proto ) + strlen( psz_uri ) +
                        strlen( psz_port ) + 5 +i_multicast );
        if( p_item->psz_uri == NULL )
        {
            msg_Err( p_intf, "Not enough memory");
            free( p_item );
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
 * returns the SAP header lenhth
 ***********************************************************************/

static int parse_sap( char *p_packet )
{
    // According to RFC 2974
    int i_hlen = 4;                           // Minimum header length is 4
    i_hlen += (p_packet[0] & 0x10) ? 16 : 4;  // Address type IPv6=16bytes
    i_hlen +=  p_packet[1];                   // Authentification length

    //Looks for the first '\0' byte after length
    for(;p_packet[i_hlen]!='\0'; i_hlen++);

    return(i_hlen);
}

/*************************************************************************
 * packet_handle : handle the received packet and enques the
 * the understated session
 *************************************************************************/

static int packet_handle( intf_thread_t * p_intf, char *p_packet, int i_len )
{
    sess_descr_t * p_sd;
    int i_hlen;                             // Header length

    i_hlen = parse_sap(p_packet);

    if( (i_hlen > 0) && (i_hlen < i_len) )
    {
        p_sd = parse_sdp( p_intf, p_packet + i_hlen +1);
        if(p_sd)
        {
            sess_toitem ( p_intf, p_sd );
            free_sd ( p_sd );
            return VLC_TRUE;
        }
    }

    return VLC_FALSE; // Invalid Packet
}




/***********************************************************************
 * parse_sdp : SDP parsing
 * *********************************************************************
 * Make a sess_descr_t with a psz
 ***********************************************************************/

static sess_descr_t *  parse_sdp( intf_thread_t * p_intf, char *p_packet )
{
    sess_descr_t *  sd;

    // According to RFC 2327, the first bytes should be exactly "v="
    if((p_packet[0] != 'v') || (p_packet[1] != '='))
    {
        msg_Warn(p_intf, "Bad SDP packet");
        return NULL;
    }

    if( ( sd = malloc( sizeof(sess_descr_t) ) ) == NULL )
    {
        msg_Err( p_intf, "Not enough memory for sd in parse_sdp()" );
        return( NULL );
    }

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

    sd->i_media = 0;

    while( *p_packet != '\0'  )
    {
#define FIELD_COPY( p ) \
        p = strndup( &p_packet[2], i_field_len );

        char *psz_end;
        int  i_field_len;

        while( *p_packet == '\n' || *p_packet == ' ' || *p_packet == '\t' )
        {
            p_packet++;
        }
        if( *p_packet == '\0' )
        {
            break;
        }

        if( ( psz_end = strchr( p_packet, '\n' ) ) == NULL )
        {
            psz_end = p_packet + strlen( p_packet );
        }
        i_field_len = psz_end - &p_packet[2];

        if( p_packet[1] == '=' && i_field_len > 0)
        {
            switch( *p_packet )
            {
                case( 'v' ):
                    FIELD_COPY( sd->psz_version );
                    break;
                case ( 'o' ):
                    FIELD_COPY( sd->psz_origin );
                    break;
                case( 's' ):
                    FIELD_COPY( sd->psz_sessionname );
                    break;
                case( 'i' ):
                    FIELD_COPY( sd->psz_information );
                    break;
                case( 'u' ):
                    FIELD_COPY( sd->psz_uri );
                    break;
                case( 'e' ):
                    FIELD_COPY( sd->psz_emails );
                    break;
                case( 'p' ):
                    FIELD_COPY( sd->psz_phone );
                    break;
                case( 't' ):
                    FIELD_COPY( sd->psz_time );
                    break;
                case( 'r' ):
                    FIELD_COPY( sd->psz_repeat );
                    break;
                case( 'a' ):
                    FIELD_COPY( sd->psz_attribute );
                    break;

                case( 'm' ):
                    if( sd->pp_media )
                    {
                        sd->pp_media =
                            realloc( sd->pp_media,
                               sizeof( media_descr_t ) * ( sd->i_media + 1 ) );
                    }
                    else
                    {
                        sd->pp_media = malloc( sizeof( void * ) );
                    }

                    sd->pp_media[sd->i_media] =
                            malloc( sizeof( media_descr_t ) );
                    sd->pp_media[sd->i_media]->psz_medianame = NULL;
                    sd->pp_media[sd->i_media]->psz_mediaconnection = NULL;
                    sd->pp_media[sd->i_media]->psz_medianame = strndup( &p_packet[2], i_field_len );

                    sd->i_media++;
                    break;

                case( 'c' ):
                    if( sd->i_media <= 0 )
                    {
                        FIELD_COPY(sd->psz_connection);
                    }
                    else
                    {
                        FIELD_COPY(sd->pp_media[sd->i_media - 1]->psz_mediaconnection);
                    }
                   break;
                default:
                   break;
            }
        }
        p_packet = psz_end;
#undef FIELD_COPY
    }

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

        for( i = 0; i < p_sd->i_media ; i++ )
        {
            FREE( p_sd->pp_media[i]->psz_medianame );
            FREE( p_sd->pp_media[i]->psz_mediaconnection );
        }
        FREE( p_sd->pp_media );

        free( p_sd );
    }
    else
    {
        ;
    }
    return;
}

/***********************************************************************
 * ismult: returns true if we have a multicast address
 ***********************************************************************/

static int ismult( char *psz_uri )
{
    char *psz_end;
    int  i_value;

    i_value = strtol( psz_uri, &psz_end, 0 );

    /* IPv6 */
    if( psz_uri[0] == '[') 
    {
        if( strncasecmp( &psz_uri[1], "FF0" , 3) )
            return( VLC_TRUE );
        else
            return( VLC_FALSE ); 
    } 

    if( *psz_end != '.' ) { return( VLC_FALSE ); }

    return( i_value < 224 ? VLC_FALSE : VLC_TRUE );
}



/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************
 * Taken from udp.c
 ******************************************************************************/
static ssize_t NetRead( intf_thread_t *p_intf,
                        int i_handle, int i_handle_v6, 
                        byte_t *p_buffer, size_t i_len)
{
#ifdef UNDER_CE
    return -1;

#else
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;
    int             i_max_handle;

    ssize_t i_recv=-1;
    
    /* Get the max handle for select */
    if( i_handle_v6 > i_handle )
        i_max_handle = i_handle_v6;
    else
        i_max_handle = i_handle;

    
    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    if(   i_handle > 0   ) FD_SET( i_handle, &fds );
    if( i_handle_v6  > 0 ) FD_SET( i_handle_v6, &fds);


    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    i_ret = select( i_max_handle + 1, &fds,
    NULL, NULL, &timeout );

    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_intf, "network select error (%s)", strerror(errno) );
    }
    else if( i_ret > 0 )
   {
      /* Get the data */     
      if(i_handle >0)
      {
         if(FD_ISSET( i_handle, &fds ))
         {
             i_recv = recv( i_handle, p_buffer, i_len, 0 );
         }
      }
      if(i_handle_v6 >0)
      {
         if(FD_ISSET( i_handle_v6, &fds ))
         {
            i_recv = recv( i_handle_v6, p_buffer, i_len, 0 );
         }
      }
      
       if( i_recv < 0 )
        {
           msg_Err( p_intf, "recv failed (%s)", strerror(errno) );
        }
        return i_recv;
    }

    return 0;

#endif
}

