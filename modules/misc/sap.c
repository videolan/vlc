/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sap.c,v 1.33 2003/11/08 23:02:38 sigmunau Exp $
 *
 * Authors: Arnaud Schauly <gitan@via.ecp.fr>
 *          Clément Stenac <zorglub@via.ecp.fr>
 *          Damien Lucas <nitrox@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

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
#   define close(a) CloseHandle(a)
#elif defined( WIN32 )
#   define close(a) closesocket(a)
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
 * Module descriptor
 *****************************************************************************/
#define SAP_ADDR_TEXT N_("SAP multicast address")
#define SAP_ADDR_LONGTEXT N_("SAP multicast address")
#define SAP_IPV4_TEXT N_("IPv4-SAP listening")
#define SAP_IPV4_LONGTEXT N_("Set this if you want SAP to listen for IPv4 announces")
#define SAP_IPV6_TEXT N_("IPv6-SAP listening")
#define SAP_IPV6_LONGTEXT N_("Set this if you want SAP to listen for IPv6 announces")
#define SAP_SCOPE_TEXT N_("IPv6 SAP scope")
#define SAP_SCOPE_LONGTEXT N_("Sets the scope for IPv6 announces (default is 8)")

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    add_category_hint( N_("SAP"), NULL, VLC_TRUE );
        add_string( "sap-addr", NULL, NULL,
                     SAP_ADDR_TEXT, SAP_ADDR_LONGTEXT, VLC_TRUE );

        add_bool( "sap-ipv4", 1 , NULL,
                     SAP_IPV4_TEXT,SAP_IPV4_LONGTEXT, VLC_TRUE);

        add_bool( "sap-ipv6", 0 , NULL,
                   SAP_IPV6_TEXT, SAP_IPV6_LONGTEXT, VLC_TRUE);

        add_string( "sap-ipv6-scope", "8" , NULL,
                    SAP_SCOPE_TEXT, SAP_SCOPE_LONGTEXT, VLC_TRUE);

    set_description( _("SAP interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void Run    ( intf_thread_t *p_intf );
static ssize_t NetRead( intf_thread_t *, int fd[2], uint8_t *, int );

typedef struct media_descr_t media_descr_t;
typedef struct sess_descr_t sess_descr_t;
typedef struct attr_descr_t attr_descr_t;

static void sess_toitem( intf_thread_t *, sess_descr_t * );

static sess_descr_t *  parse_sdp( intf_thread_t *, char * ) ;
static void free_sd( sess_descr_t * );

/* Detect multicast addresses */
static int  ismult( char * );

/* The struct that contains sdp informations */
struct  sess_descr_t
{
    int  i_version;
    char *psz_sessionname;
    char *psz_connection;

    int           i_media;
    media_descr_t **pp_media;
    int           i_attributes;
    attr_descr_t  **pp_attributes;
};

/* All this informations are not useful yet.  */
struct media_descr_t
{
    char *psz_medianame;
    char *psz_mediaconnection;
};

struct attr_descr_t
{
    char *psz_field;
    char *psz_value;
};

struct intf_sys_t
{
    /* IPV4 and IPV6 */
    int fd[2];

    /* playlist group */
    int i_group;
};

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys  = malloc( sizeof( intf_sys_t ) );

    playlist_t          *p_playlist;

    p_sys->fd[0] = -1;
    p_sys->fd[1] = -1;
    if( config_GetInt( p_intf, "sap-ipv4" ) )
    {
        char *psz_address = config_GetPsz( p_intf, "sap-addr" );
        network_socket_t    sock;
        module_t            *p_network;
        if( psz_address == NULL || *psz_address == '\0' )
        {
            psz_address = strdup( HELLO_GROUP );
        }

        /* Prepare the network_socket_t structure */
        sock.i_type            = NETWORK_UDP;
        sock.psz_bind_addr     = psz_address;
        sock.i_bind_port       = HELLO_PORT;
        sock.psz_server_addr   = "";
        sock.i_server_port     = 0;
        sock.i_ttl             = 0;
        p_intf->p_private = (void*) &sock;

        p_network = module_Need( p_intf, "network", "ipv4" );
        if( p_network )
        {
            p_sys->fd[0] = sock.i_handle;
            module_Unneed( p_intf, p_network );
        }
        else
        {
            msg_Warn( p_intf, "failed to open %s:%d", psz_address, HELLO_PORT );
        }
        free( psz_address );
    }

    if( config_GetInt( p_intf, "sap-ipv6" ) )
    {
        char psz_address[100];
        char *psz_scope = config_GetPsz( p_intf, "sap-ipv6-scope" );
        network_socket_t    sock;
        module_t            *p_network;

        if( psz_scope == NULL || *psz_scope == '\0' )
        {
            psz_scope = strdup( "8" );
        }
        snprintf( psz_address, 100, "[%s%c%s]",IPV6_ADDR_1, psz_scope[0], IPV6_ADDR_2 );
        free( psz_scope );

        sock.i_type            = NETWORK_UDP;
        sock.psz_bind_addr     = psz_address;
        sock.i_bind_port       = HELLO_PORT;
        sock.psz_server_addr   = "";
        sock.i_server_port     = 0;
        sock.i_ttl             = 0;
        p_intf->p_private = (void*) &sock;

        p_network = module_Need( p_intf, "network", "ipv6" );
        if( p_network )
        {
            p_sys->fd[1] = sock.i_handle;
            module_Unneed( p_intf, p_network );
        }
        else
        {
            msg_Warn( p_intf, "failed to open %s:%d", psz_address, HELLO_PORT );
        }
    }
    if( p_sys->fd[0] <= 0 && p_sys->fd[1] <= 0 )
    {
        msg_Warn( p_intf, "IPV4 and IPV6 failed" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Create our playlist group */
    p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_group_t *p_group = playlist_CreateGroup( p_playlist , "SAP" );
        p_sys->i_group = p_group->i_id;
        vlc_object_release( p_playlist );
    }

    p_intf->pf_run = Run;
    p_intf->p_sys  = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys  = p_intf->p_sys;

    if( p_sys->fd[0] > 0 )
    {
        close( p_sys->fd[0] );
    }
    if( p_sys->fd[1] > 0 )
    {
        close( p_sys->fd[1] );
    }

    free( p_sys );
}

/*****************************************************************************
 * Run: sap thread
 *****************************************************************************
 * Listens to SAP packets, and sends them to packet_handle
 *****************************************************************************/
#define MAX_SAP_BUFFER 2000

static void Run( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys  = p_intf->p_sys;
    uint8_t     buffer[MAX_SAP_BUFFER + 1];

    /* read SAP packets */
    while( !p_intf->b_die )
    {
        int i_read = NetRead( p_intf, p_sys->fd, buffer, MAX_SAP_BUFFER );
        uint8_t *p_sdp;

        /* Minimum length is > 6 */
        if( i_read <= 6 )
        {
            if( i_read < 0 )
            {
                msg_Warn( p_intf, "Cannot read in the socket" );
            }
            continue;
        }

        buffer[i_read] = '\0';

        /* Parse the SAP header */
        p_sdp  = &buffer[4];
        p_sdp += (buffer[0]&0x10) ? 16 : 4;
        p_sdp += buffer[1];

        while( p_sdp < &buffer[i_read-1] && *p_sdp != '\0' && p_sdp[0] != 'v' && p_sdp[1] != '=' )
        {
            p_sdp++;
        }
        if( *p_sdp == '\0' )
        {
            p_sdp++;
        }
        if( p_sdp < &buffer[i_read] )
        {
            sess_descr_t *p_sd = parse_sdp( p_intf, p_sdp );
            if( p_sd )
            {
                sess_toitem ( p_intf, p_sd );
                free_sd ( p_sd );
            }
        }
    }
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
    char *psz_media;
    if( psz_mfield )
    {
        psz_pos = psz_mfield;
        psz_media = psz_mfield;
        while( *psz_pos != '\0' && *psz_pos != ' ' )
        {
            psz_pos++;
        }
        if( *psz_pos != '\0' )
        {
            *psz_pos = '\0';
            if( strcmp( psz_media, "video" ) && strcmp( psz_media, "audio" ) )
            {
                *ppsz_proto = NULL;
                *ppsz_port = NULL;
                return;
            }
        }
        psz_pos++;
        *ppsz_port = psz_pos;
        while( *psz_pos != '\0' && *psz_pos !=' ' && *psz_pos!='/' )
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


/*******************************************************************
 * sess_toitem : changes a sess_descr_t into a hurd of
 * playlist_item_t, which are enqueued.
 *******************************************************************
 * Note : does not support sessions that take place on consecutive
 * port or adresses yet.
 *******************************************************************/

static void sess_toitem( intf_thread_t * p_intf, sess_descr_t * p_sd )
{
    playlist_item_t * p_item;
    char *psz_uri, *psz_proto;
    char *psz_port;
    char *psz_uri_default;
    int i_count , i;
    vlc_bool_t b_http = VLC_FALSE;
    char *psz_http_path = NULL;
    playlist_t *p_playlist;

    psz_uri_default = NULL;
    cfield_parse( p_sd->psz_connection, &psz_uri_default );

    for( i_count = 0 ; i_count < p_sd->i_media ; i_count++ )
    {
        p_item = malloc( sizeof( playlist_item_t ) );
        p_item->psz_name    = strdup( p_sd->psz_sessionname );
        p_item->psz_uri     = NULL;
        p_item->i_duration  = -1;
        p_item->ppsz_options= NULL;
        p_item->i_options   = 0;

        p_item->i_type      = 0;
        p_item->i_status    = 0;
        p_item->b_autodeletion = VLC_FALSE;
        p_item->b_enabled   = VLC_TRUE;
        p_item->i_group     = p_intf->p_sys->i_group;
        p_item->psz_author  = strdup( "" );

        psz_uri = NULL;

        /* Build what we have to put in p_item->psz_uri, with the m and
         *  c fields  */

        if( !p_sd->pp_media[i_count] )
        {
            return;
        }

        mfield_parse( p_sd->pp_media[i_count]->psz_medianame,
                        & psz_proto, & psz_port );

        if( !psz_proto || !psz_port )
        {
            return;
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
            return;
        }

        for( i = 0 ; i< p_sd->i_attributes ; i++ )
        {
            if(!strcasecmp( p_sd->pp_attributes[i]->psz_field , "type") &&
                strstr( p_sd->pp_attributes[i]->psz_value, "http") )
            {
                b_http = VLC_TRUE;
            }
            if(!strcasecmp( p_sd->pp_attributes[i]->psz_field , "http-path"))
            {
                psz_http_path = strdup(  p_sd->pp_attributes[i]->psz_value );
            }
        }


        /* Filling p_item->psz_uri */
        if( b_http == VLC_FALSE )
        {
            p_item->psz_uri = malloc( strlen( psz_proto ) + strlen( psz_uri ) +
                                      strlen( psz_port ) + 7 );
            if( ismult( psz_uri ) )
            {
                sprintf( p_item->psz_uri, "%s://@%s:%s",
                         psz_proto, psz_uri, psz_port );
            }
            else
            {
                sprintf( p_item->psz_uri, "%s://%s:%s",
                         psz_proto, psz_uri, psz_port );
            }
        }
        else
        {
            if( psz_http_path == NULL )
            {
                psz_http_path = strdup( "/" );
            }

            p_item->psz_uri = malloc( strlen( psz_proto ) + strlen( psz_uri ) +
                                      strlen( psz_port ) + strlen(psz_http_path) + 5 );
            sprintf( p_item->psz_uri, "%s://%s:%s%s", psz_proto,
                            psz_uri, psz_port,psz_http_path );

            if( psz_http_path )
            {
                free( psz_http_path );
            }
        }

        /* Enqueueing p_item in the playlist */
        p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        playlist_AddItem ( p_playlist, p_item, PLAYLIST_CHECK_INSERT, PLAYLIST_END );
        vlc_object_release( p_playlist );
    }
}

/***********************************************************************
 * parse_sdp : SDP parsing
 * *********************************************************************
 * Make a sess_descr_t with a psz
 ***********************************************************************/

static sess_descr_t *  parse_sdp( intf_thread_t * p_intf, char *p_packet )
{
    sess_descr_t *  sd;

    if( p_packet[0] != 'v' || p_packet[1] != '=' )
    {
        msg_Warn(p_intf, "bad SDP packet");
        return NULL;
    }

    sd = malloc( sizeof( sess_descr_t ) );
    sd->psz_sessionname = NULL;
    sd->psz_connection  = NULL;
    sd->i_media         = 0;
    sd->pp_media        = NULL;
    sd->i_attributes    = 0;
    sd->pp_attributes   = NULL;

    while( *p_packet != '\0'  )
    {
        char *psz_end;

        /* Search begin of field */
        while( *p_packet == '\n' || *p_packet == ' ' || *p_packet == '\t' )
        {
            p_packet++;
        }
        /* search end of line */
        if( ( psz_end = strchr( p_packet, '\n' ) ) == NULL )
        {
            psz_end = p_packet + strlen( p_packet );
        }
        if( psz_end > p_packet && *(psz_end - 1 ) == '\r' )
        {
            psz_end--;
        }
        
        if( psz_end <= p_packet )
        {
            break;
        }
        *psz_end++ = '\0';

        if( p_packet[1] != '=' )
        {
            msg_Warn( p_intf, "packet invalid" );
            free_sd( sd );
            return NULL;
        }

        switch( p_packet[0] )
        {
            case( 'v' ):
                sd->i_version = atoi( &p_packet[2] );
                break;
            case( 's' ):
                sd->psz_sessionname = strdup( &p_packet[2] );
                break;
            case ( 'o' ):
            case( 'i' ):
            case( 'u' ):
            case( 'e' ):
            case( 'p' ):
            case( 't' ):
            case( 'r' ):
                break;
            case( 'a' ):
            {
                char *psz_eof = strchr( &p_packet[2], ':' );

                if( psz_eof && psz_eof[1] != '\0' )
                {
                    attr_descr_t *attr = malloc( sizeof( attr_descr_t ) );

                    *psz_eof++ = '\0';

                    attr->psz_field = strdup( &p_packet[2] );
                    attr->psz_value = strdup( psz_eof );

                    TAB_APPEND( sd->i_attributes, sd->pp_attributes, attr );
                }
                break;
            }

            case( 'm' ):
            {
                media_descr_t *media = malloc( sizeof( media_descr_t ) );

                media->psz_medianame = strdup( &p_packet[2] );
                media->psz_mediaconnection = NULL;

                TAB_APPEND( sd->i_media, sd->pp_media, media );
                break;
            }

            case( 'c' ):
                if( sd->i_media <= 0 )
                {
                    sd->psz_connection = strdup( &p_packet[2] );
                }
                else
                {
                    sd->pp_media[sd->i_media-1]->psz_mediaconnection = strdup( &p_packet[2] );
                }
               break;

            default:
               break;
        }

        p_packet = psz_end;
    }

    return sd;
}

#define FREE( p ) \
    if( p ) { free( p ); (p) = NULL; }
static void free_sd( sess_descr_t * p_sd )
{
    int i;

    FREE( p_sd->psz_sessionname );
    FREE( p_sd->psz_connection );

    for( i = 0; i < p_sd->i_media ; i++ )
    {
        FREE( p_sd->pp_media[i]->psz_medianame );
        FREE( p_sd->pp_media[i]->psz_mediaconnection );
    }
    for( i = 0; i < p_sd->i_attributes ; i++ )
    {
        FREE( p_sd->pp_attributes[i]->psz_field );
        FREE( p_sd->pp_attributes[i]->psz_value );
    }
    FREE( p_sd->pp_attributes );
    FREE( p_sd->pp_media );

    free( p_sd );
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
      if( strncasecmp( &psz_uri[1], "FF0" , 3) ||
          strncasecmp( &psz_uri[2], "FF0" , 3))
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
 *****************************************************************************/
static ssize_t NetRead( intf_thread_t *p_intf,
                        int fd[2], uint8_t *p_buffer, int i_len )
{
#ifdef UNDER_CE
    return -1;
#else
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;
    int             i_handle_max = __MAX( fd[0], fd[1] );

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    if( fd[0] > 0 ) FD_SET( fd[0], &fds );
    if( fd[1] > 0 ) FD_SET( fd[1], &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    i_ret = select( i_handle_max + 1, &fds, NULL, NULL, &timeout );

    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_intf, "network select error (%s)", strerror(errno) );
    }
    else if( i_ret > 0 )
    {
        if( fd[0] > 0 && FD_ISSET( fd[0], &fds ) )
        {
             return recv( fd[0], p_buffer, i_len, 0 );
        }
        else if( fd[1] > 0 && FD_ISSET( fd[1], &fds ) )
        {
             return recv( fd[1], p_buffer, i_len, 0 );
        }
    }
    return 0;
#endif
}

