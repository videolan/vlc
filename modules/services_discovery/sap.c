/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Includes
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <vlc/input.h>

#include "network.h"

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

/************************************************************************
 * Macros and definitions
 ************************************************************************/

#define MAX_LINE_LENGTH 256

/* SAP is always on that port */
#define SAP_PORT 9875
#define SAP_V4_ADDRESS "224.2.127.254"
#define ADD_SESSION 1

#define IPV6_ADDR_1 "FF0"  /* Scope is inserted between them */
#define IPV6_ADDR_2 "::2:7FFE"


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SAP_ADDR_TEXT N_( "SAP multicast address" )
#define SAP_ADDR_LONGTEXT N_( "SAP multicast address" )
#define SAP_IPV4_TEXT N_( "IPv4-SAP listening" )
#define SAP_IPV4_LONGTEXT N_( \
      "Set this if you want the SAP module to listen to IPv4 announces" )
#define SAP_IPV6_TEXT N_( "IPv6-SAP listening" )
#define SAP_IPV6_LONGTEXT N_( \
      "Set this if you want the SAP module to listen to IPv6 announces" )
#define SAP_SCOPE_TEXT N_( "IPv6 SAP scope" )
#define SAP_SCOPE_LONGTEXT N_( \
       "Sets the scope for IPv6 announces (default is 8)" )
#define SAP_TIMEOUT_TEXT N_( "SAP timeout (seconds)" )
#define SAP_TIMEOUT_LONGTEXT N_( \
       "Sets the time before SAP items get deleted if no new announce " \
       "is received." )
#define SAP_PARSE_TEXT N_( "Try to parse the SAP" )
#define SAP_PARSE_LONGTEXT N_( \
       "When SAP can it will try to parse the SAP. If you don't select " \
       "this, all announces will be parsed by the livedotcom module" )
#define SAP_CACHE_TEXT N_("Use SAP cache")
#define SAP_CACHE_LONGTEXT N_( \
       "If this option is selected, a SAP caching mechanism will be used." \
       "This will result in lower SAP startup time, but you could end up " \
        "with items corresponding to legacy streams." )

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );
    static int  OpenDemux ( vlc_object_t * );
    static void CloseDemux ( vlc_object_t * );

vlc_module_begin();
    set_description( _("SAP interface") );

    add_string( "sap-addr", NULL, NULL,
                SAP_ADDR_TEXT, SAP_ADDR_LONGTEXT, VLC_TRUE );
    add_bool( "sap-ipv4", 1 , NULL,
               SAP_IPV4_TEXT,SAP_IPV4_LONGTEXT, VLC_TRUE );
    add_bool( "sap-ipv6", 0 , NULL,
              SAP_IPV6_TEXT, SAP_IPV6_LONGTEXT, VLC_TRUE );
    add_string( "sap-ipv6-scope", "8" , NULL,
                SAP_SCOPE_TEXT, SAP_SCOPE_LONGTEXT, VLC_TRUE);
    add_integer( "sap-timeout", 1800, NULL,
                 SAP_TIMEOUT_TEXT, SAP_TIMEOUT_LONGTEXT, VLC_TRUE );
    add_bool( "sap-parse", 1 , NULL,
               SAP_PARSE_TEXT,SAP_PARSE_LONGTEXT, VLC_TRUE );
    add_bool( "sap-cache", 0 , NULL,
               SAP_CACHE_TEXT,SAP_CACHE_LONGTEXT, VLC_TRUE );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

    add_submodule();
        set_description( _("SDP file parser for UDP") );
        add_shortcut( "sdp" );
        set_capability( "demux2", 51 );
        set_callbacks( OpenDemux, CloseDemux );
vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

typedef struct sdp_t sdp_t;
typedef struct attribute_t attribute_t;
typedef struct sap_announce_t sap_announce_t;

/* The structure that contains sdp information */
struct  sdp_t
{
    char *psz_sdp;

    /* s= field */
    char *psz_sessionname;

    /* Raw m= and c= fields */
    char *psz_connection;
    char *psz_media;

    /* "computed" URI */
    char *psz_uri;

    int         i_in; /* IP version */

    int           i_media;

    int           i_attributes;
    attribute_t  **pp_attributes;
};

struct attribute_t
{
    char *psz_field;
    char *psz_value;
};

struct sap_announce_t
{
    mtime_t i_last;

    uint16_t    i_hash;
    uint32_t    i_source[4];

    /* SAP annnounces must only contain one SDP */
    sdp_t       *p_sdp;

    playlist_item_t *p_item;
};

struct services_discovery_sys_t
{
    /* Socket descriptors */
    int i_fd;
    int *pi_fd;

    /* playlist node */
    playlist_item_t *p_node;

    /* Table of announces */
    int i_announces;
    struct sap_announce_t **pp_announces;

    /* Modes */
    vlc_bool_t  b_strict;
    vlc_bool_t  b_parse;

    int i_timeout;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/


/* Main functions */
    static int Demux( demux_t *p_demux );
    static int Control( demux_t *, int, va_list );
    static void Run    ( services_discovery_t *p_sd );

/* Main parsing functions */
    static int ParseConnection( vlc_object_t *p_obj, sdp_t *p_sdp );
    static int ParseSAP( services_discovery_t *p_sd, uint8_t *p_buffer, int i_read );
    static sdp_t *  ParseSDP( vlc_object_t *p_sd, char* psz_sdp );
    static sap_announce_t *CreateAnnounce( services_discovery_t *, uint16_t, sdp_t * );
    static int RemoveAnnounce( services_discovery_t *p_sd, sap_announce_t *p_announce );

/* Cache */
    static void CacheLoad( services_discovery_t *p_sd );
    static void CacheSave( services_discovery_t *p_sd );
/* Helper functions */
   static char *GetAttribute( sdp_t *p_sdp, const char *psz_search );
   static int InitSocket( services_discovery_t *p_sd, char *psz_address, int i_port );
#ifdef HAVE_ZLIB_H
   static int Decompress( unsigned char *psz_src, unsigned char **_dst, int i_len );
    static void FreeSDP( sdp_t *p_sdp );
#endif

/* Detect multicast addresses */
static int  ismult( char * );

#define FREE( p ) \
    if( p ) { free( p ); (p) = NULL; }
/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)
                                malloc( sizeof( services_discovery_sys_t ) );

    playlist_t          *p_playlist;
    playlist_view_t     *p_view;

    p_sys->i_timeout = config_GetInt( p_sd,"sap-timeout" );

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    p_sys->pi_fd = NULL;
    p_sys->i_fd = 0;

    /* FIXME */
    p_sys->b_strict = VLC_FALSE;
    p_sys->b_parse = config_GetInt( p_sd, "sap-parse" );

    if( config_GetInt( p_sd, "sap-cache" ) )
    {
        CacheLoad( p_sd );
    }

    if( config_GetInt( p_sd, "sap-ipv4" ) )
    {
        InitSocket( p_sd, SAP_V4_ADDRESS, SAP_PORT );
    }
    if( config_GetInt( p_sd, "sap-ipv6" ) )
    {
        /* TODO */
    }

    /* TODO : Handle additionnal adresses */

    if( p_sys->i_fd == 0 )
    {
        msg_Err( p_sd, "unable to read on any address");
        return VLC_EGENERIC;
    }

    /* Create our playlist node */
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling SAP listening");
        return VLC_EGENERIC;
    }

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                         _("SAP"), p_view->p_root );

    vlc_object_release( p_playlist );

    p_sys->i_announces = 0;
    p_sys->pp_announces = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenDemux: initialize and create stuff
 *****************************************************************************/
static int OpenDemux( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    uint8_t *p_peek;

    /* Probe for SDP */
    if( p_demux->s )
    {
        if( stream_Peek( p_demux->s, &p_peek, 7 ) < 7 )
        {
            msg_Err( p_demux, "cannot peek" );
            return VLC_EGENERIC;
        }
        if( strncmp( (char*)p_peek, "v=0\r\n", 5 ) &&
            strncmp( (char*)p_peek, "v=0\n", 4 ) &&
            ( p_peek[0] < 'a' || p_peek[0] > 'z' || p_peek[1] != '=' ) )
        {
            msg_Warn( p_demux, "SDP (UDP) module discarded" );
            return VLC_EGENERIC;
        }
    }

    free( p_peek );

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    int i;

    for( i = p_sys->i_fd-1 ; i >= 0 ; i-- )
    {
        net_Close( p_sys->pi_fd[i] );
    }

    if( config_GetInt( p_sd, "sap-cache" ) )
    {
        CacheSave( p_sd );
    }

    for( i = p_sys->i_announces  - 1;  i>= 0; i-- )
    {
        RemoveAnnounce( p_sd, p_sys->pp_announces[i] );
    }

    free( p_sys );
}

/*****************************************************************************
 * CloseDemux: Close the demuxer
 *****************************************************************************/
static void CloseDemux( vlc_object_t *p_this )
{

}

/*****************************************************************************
 * Run: main SAP thread
 *****************************************************************************
 * Listens to SAP packets, and sends them to packet_handle
 *****************************************************************************/
#define MAX_SAP_BUFFER 5000

static void Run( services_discovery_t *p_sd )
{
    uint8_t     *p_buffer;
    /* Dirty hack to slow down the startup of the sap interface */
    /* Unneeded now : our node is in no_select mode */
    //    msleep( 500000 );

    /* read SAP packets */
    while( !p_sd->b_die )
    {
        p_buffer = (uint8_t *)malloc( MAX_SAP_BUFFER );

        if( !p_buffer )
        {
            msg_Err( p_sd, "out of memory");
            p_sd->b_die = VLC_TRUE;
            continue;
        }

        int i_read = net_Select( p_sd, p_sd->p_sys->pi_fd, NULL,
                                 p_sd->p_sys->i_fd, p_buffer,
                                 MAX_SAP_BUFFER, 500000 );
#if 0
        /* Check for items that need deletion */
        for( i = 0 ; i< p_sd->p_sys->i_announces ; i++ )
        {
           struct sap_announce_t *p_announce;
           mtime_t i_timeout = ( mtime_t ) 1000000*p_sys->i_timeout;
           if( mdate() - p_sd->p_sys->pp_announces[i]->i_last > i_timeout )
           {
               msg_Dbg( p_sd,"Time out for %s, deleting (%i/%i)",
                        p_sd->p_sys->pp_announces[i]->psz_name,
                        i , p_sd->p_sys->i_announces );

             /* Remove the playlist item */
               p_playlist = vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                              FIND_ANYWHERE );
               if( p_playlist )
               {
                   int i_pos = playlist_GetPositionById( p_playlist,
                              p_sd->p_sys->pp_announces[i]->i_id );
                   playlist_Delete( p_playlist, i_pos );
                   vlc_object_release( p_playlist );
               }

               /* Free the p_announce */
               p_announce =  p_sd->p_sys->pp_announces[i];
               if( p_announce->psz_name )
                  free(  p_announce->psz_name );
               if( p_announce->psz_uri )
                  free(  p_announce->psz_uri );

              /* Remove the sap_announce from the array */
              REMOVE_ELEM( p_sd->p_sys->pp_announces,
                           p_sd->p_sys->i_announces,
                           i );

              free( p_announce );

           }
        }
#endif

        /* Minimum length is > 6 */
        if( i_read <= 6 )
        {
            if( i_read < 0 )
            {
                msg_Warn( p_sd, "socket read error" );
            }
            free( p_buffer );
            continue;
        }

        p_buffer[i_read] = '\0';

        /* Parse the packet */
        ParseSAP( p_sd, p_buffer, i_read );

        free( p_buffer );
    }
}

/**********************************************************************
 * Demux: reads and demuxes data packets
 * Return -1 if error, 0 if EOF, 1 else
 **********************************************************************/
static int Demux( demux_t *p_demux )
{
   int i_max_sdp = 1024;
   int i_sdp = 0;
   char *psz_sdp = (char *)malloc( i_max_sdp );
   sdp_t *p_sdp;

   playlist_t *p_playlist;

   /* Gather the complete sdp file */
   for( ;; )
   {
        int i_read = stream_Read( p_demux->s,
                                  &psz_sdp[i_sdp], i_max_sdp - i_sdp - 1 );


        if( i_read < 0 )

        {
            msg_Err( p_demux, "failed to read SDP" );
            return VLC_EGENERIC;
        }

        i_sdp += i_read;

        if( i_read < i_max_sdp - i_sdp - 1 )
        {
            psz_sdp[i_sdp] = '\0';
            break;
        }

        i_max_sdp += 1000;
        psz_sdp = (uint8_t*)realloc( psz_sdp, i_max_sdp );
   }

   p_sdp = ParseSDP( VLC_OBJECT(p_demux), psz_sdp );

   if( !p_sdp ) return -1;

   if( p_sdp->i_media > 1 )
   {
        return -1;
   }

   if( ParseConnection( VLC_OBJECT( p_demux ), p_sdp ) )
   {
       p_sdp->psz_uri = NULL;
   }

   if( p_sdp->psz_uri == NULL ) return VLC_EGENERIC;

   p_playlist = (playlist_t *)vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

   p_playlist->status.p_item->i_flags |= PLAYLIST_DEL_FLAG;

   playlist_Add( p_playlist, p_sdp->psz_uri, p_sdp->psz_sessionname,
                 PLAYLIST_APPEND, PLAYLIST_END );

   vlc_object_release( p_playlist );

   FreeSDP( p_sdp );
   free( psz_sdp );

   return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/**************************************************************
 * Local functions
 **************************************************************/

static int ParseSAP( services_discovery_t *p_sd, uint8_t *p_buffer, int i_read )
{
    int                 i_version, i_address_type, i_hash, i;
    uint8_t             *psz_sdp;
    sdp_t               *p_sdp;
    vlc_bool_t          b_compressed;
    vlc_bool_t          b_need_delete = VLC_FALSE;
#ifdef HAVE_ZLIB_H
    int                 i_decompressed_size;
    uint8_t             *p_decompressed_buffer;
#endif

    /* First, check the sap announce is correct */
    i_version = p_buffer[0] >> 5;

    if( i_version != 1 )
    {
       msg_Dbg( p_sd, "strange sap version %d found", i_version );
    }

    i_address_type = p_buffer[0] & 0x10;

    if( (p_buffer[0] & 0x08) != 0 )
    {
        msg_Dbg( p_sd, "reserved bit incorrectly set" );
        return VLC_EGENERIC;
    }

    if( (p_buffer[0] & 0x04) != 0 )
    {
        msg_Dbg( p_sd, "session deletion packet" );
        b_need_delete = VLC_TRUE;
    }

    if( p_buffer[0] & 0x02  )
    {
        msg_Dbg( p_sd, "encrypted packet, unsupported" );
        return VLC_EGENERIC;
    }

    b_compressed = p_buffer[0] & 0x01;

    i_hash = ( p_buffer[2] << 8 ) + p_buffer[3];

    if( p_sd->p_sys->b_strict && i_hash == 0 )
    {
        msg_Dbg( p_sd, "strict mode, discarding announce with null id hash");
        return VLC_EGENERIC;
    }

    psz_sdp  = &p_buffer[4];

    if( i_address_type == 0 ) /* ipv4 source address */
    {
        psz_sdp += 4;
    }
    else /* ipv6 source address */
    {
        psz_sdp += 16;
    }

    if( b_compressed )
    {
#ifdef HAVE_ZLIB_H
        i_decompressed_size = Decompress( psz_sdp,
                   &p_decompressed_buffer,i_read - ( psz_sdp - p_buffer ) );
        if( i_decompressed_size > 0 && i_decompressed_size < MAX_SAP_BUFFER )
        {
            memcpy( psz_sdp, p_decompressed_buffer, i_decompressed_size );
            psz_sdp[i_decompressed_size] = '\0';
            free( p_decompressed_buffer );
        }
#else
        msg_Warn( p_sd, "Ignoring compressed sap packet" );
        return VLC_EGENERIC;
#endif
    }

    /* Add the size of authentification info */
    psz_sdp += p_buffer[1];

    /* Skip payload type */
    /* Handle announces without \0 between SAP and SDP */
    while( *psz_sdp != '\0' && ( psz_sdp[0] != 'v' && psz_sdp[1] != '=' ) )
    {
        psz_sdp++;
    }

    if( *psz_sdp == '\0' )
    {
        psz_sdp++;
    }

    /* Parse SDP info */
    p_sdp = ParseSDP( VLC_OBJECT(p_sd), psz_sdp );

    if( p_sdp == NULL )
    {
        return VLC_EGENERIC;
    }

    /* Decide whether we should add a playlist item for this SDP */

    /* Multi-media or no-parse -> pass to LIVE.COM */
    if( p_sdp->i_media > 1 || p_sd->p_sys->b_parse == VLC_FALSE )
    {
        asprintf( &p_sdp->psz_uri, "sdp://%s", p_sdp->psz_sdp );
    }
    else
    {
        /* Parse connection information (c= & m= ) */
        if( ParseConnection( VLC_OBJECT(p_sd), p_sdp ) )
        {
            p_sdp->psz_uri = NULL;
        }
    }

    if( p_sdp->psz_uri == NULL ) return VLC_EGENERIC;

    for( i = 0 ; i< p_sd->p_sys->i_announces ; i++ )
    {
        /* FIXME: slow */
        /* FIXME: we create a new announce each time the sdp changes */
        if( !strcmp( p_sd->p_sys->pp_announces[i]->p_sdp->psz_sdp,
                    p_sdp->psz_sdp ) )
        {
            if( b_need_delete )
            {
                RemoveAnnounce( p_sd, p_sd->p_sys->pp_announces[i]);
                return VLC_SUCCESS;
            }
            else
            {
                p_sd->p_sys->pp_announces[i]->i_last = mdate();
                FreeSDP( p_sdp );
                return VLC_SUCCESS;
            }
        }
    }
    /* Add item */
    if( p_sdp->i_media > 1 )
    {
        msg_Dbg( p_sd, "passing to LIVE.COM" );
    }

    CreateAnnounce( p_sd, i_hash, p_sdp );

    return VLC_SUCCESS;
}

sap_announce_t *CreateAnnounce( services_discovery_t *p_sd, uint16_t i_hash,
                                sdp_t *p_sdp )
{
    playlist_t          *p_playlist;
    playlist_item_t     *p_item, *p_child;
    char                *psz_value;
    sap_announce_t *p_sap = (sap_announce_t *)malloc(
                                        sizeof(sap_announce_t ) );
    if( !p_sap )
    {
        msg_Err( p_sd, "out of memory");
        p_sd->b_die = VLC_TRUE;
        return NULL;
    }
    p_sap->i_last = mdate();
    p_sap->i_hash = i_hash;
    p_sap->p_sdp = p_sdp;
    p_sap->p_item = NULL;

    /* Create the playlist item here */
    p_item = playlist_ItemNew( p_sd, p_sap->p_sdp->psz_uri,
                               p_sap->p_sdp->psz_sessionname );

    if( !p_item )
    {
        return NULL;
    }

    psz_value = GetAttribute( p_sap->p_sdp, "x-plgroup" );

    if( psz_value == NULL )
    {
        psz_value = GetAttribute( p_sap->p_sdp, "plgroup" );
    }

    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        FREE( psz_value );
        free( p_sap );
        return NULL;
    }

    if( psz_value != NULL )
    {
        p_child = playlist_ChildSearchName( p_sd->p_sys->p_node, psz_value );

        if( p_child == NULL )
        {
            p_child = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                           psz_value, p_sd->p_sys->p_node );
        }
    }
    else
    {
        p_child = p_sd->p_sys->p_node;
    }

    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;

    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY, p_child,
                          PLAYLIST_APPEND, PLAYLIST_END );

    vlc_object_release( p_playlist );

    p_sap->p_item = p_item;

    TAB_APPEND( p_sd->p_sys->i_announces,
                p_sd->p_sys->pp_announces,
                p_sap );

    return p_sap;
}

static char *GetAttribute( sdp_t *p_sdp, const char *psz_search )
{
    int i;

    for( i = 0 ; i< p_sdp->i_attributes; i++ )
    {
        if( !strncmp( p_sdp->pp_attributes[i]->psz_field, psz_search,
                      strlen( p_sdp->pp_attributes[i]->psz_field ) ) )
        {
            return p_sdp->pp_attributes[i]->psz_value;
        }
    }
    return NULL;
}


/* Fill p_sdp->psz_uri */
static int ParseConnection( vlc_object_t *p_obj, sdp_t *p_sdp )
{
    char *psz_eof;
    char *psz_parse;
    char *psz_uri = NULL;
    char *psz_proto = NULL;
    int i_port = 0;

    /* Parse c= field */
    if( p_sdp->psz_connection )
    {
        psz_parse = p_sdp->psz_connection;

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';
            psz_parse = psz_eof + 1;
        }
        else
        {
            msg_Warn( p_obj, "unable to parse c field (1)");
            return VLC_EGENERIC;
        }

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';
            if( !strncmp( psz_parse, "IP4", 3 ) )
            {
                p_sdp->i_in = 4;
            }
            else if( !strncmp( psz_parse, "IP6", 3 ) )
            {
                p_sdp->i_in = 6;
            }
            else
            {
                p_sdp->i_in = 0;
            }
            psz_parse = psz_eof + 1;
        }
        else
        {
            msg_Warn( p_obj, "unable to parse c field (2)");
            return VLC_EGENERIC;
        }

        psz_eof = strchr( psz_parse, '/' );

        if( psz_eof )
        {
            *psz_eof = 0;
            psz_uri = strdup( psz_parse );
        }

        else
        {
            msg_Warn( p_obj, "unable to parse c field (3)");
            return VLC_EGENERIC;
        }

    }

    /* Parse m= field */
    if( p_sdp->psz_media )
    {
        psz_parse = p_sdp->psz_media;

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';

            if( strncmp( psz_parse, "audio", 5 )  &&
                strncmp( psz_parse, "video",5 ) )
            {
                msg_Warn( p_obj, "unhandled media type -%s-", psz_parse );
                FREE( psz_uri );
                return VLC_EGENERIC;
            }

            psz_parse = psz_eof + 1;
        }
        else
        {
            msg_Warn( p_obj, "unable to parse m field (1)");
            FREE( psz_uri );
            return VLC_EGENERIC;
        }

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';

            /* FIXME : multiple port ! */
            i_port = atoi( psz_parse );

            if( i_port <= 0 || i_port >= 65536 )
            {
                msg_Warn( p_obj, "invalid transport port %i", i_port );
            }

            psz_parse = psz_eof + 1;
        }
        else
        {
            msg_Warn( p_obj, "unable to parse m field (2)");
            FREE( psz_uri );
            return VLC_EGENERIC;
        }

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';

            psz_proto = strdup( psz_parse );
        }
        else
        {
            msg_Warn( p_obj, "unable to parse m field (3)");
            return VLC_EGENERIC;
        }
    }

    /* FIXME: HTTP support */

    if( i_port == 0 )
    {
        i_port = 1234;
    }

    if( ismult( psz_uri ) )
    {
        asprintf( &p_sdp->psz_uri, "%s://@%s:%i", psz_proto, psz_uri, i_port );
    }
    else
    {
        asprintf( &p_sdp->psz_uri, "%s://%s:%i", psz_proto, psz_uri, i_port );
    }
    FREE( psz_uri );
    FREE( psz_proto );
    return VLC_SUCCESS;
}

/***********************************************************************
 * ParseSDP : SDP parsing
 * *********************************************************************
 * Validate SDP and parse all fields
 ***********************************************************************/
static sdp_t *  ParseSDP( vlc_object_t *p_obj, char* psz_sdp )
{
    sdp_t *p_sdp;

    if( psz_sdp[0] != 'v' || psz_sdp[1] != '=' )
    {
        msg_Warn( p_obj, "bad SDP packet" );
        return NULL;
    }

    p_sdp = (sdp_t *)malloc( sizeof( sdp_t ) );

    p_sdp->psz_sdp = strdup( psz_sdp );

    p_sdp->psz_sessionname = NULL;
    p_sdp->psz_media       = NULL;
    p_sdp->psz_connection  = NULL;

    p_sdp->i_media         = 0;
    p_sdp->i_attributes    = 0;
    p_sdp->pp_attributes   = NULL;

    while( *psz_sdp != '\0'  )
    {
        char *psz_eol;
        while( *psz_sdp == '\r' || *psz_sdp == '\n' || *psz_sdp == ' ' || *psz_sdp == '\t' )
        {
            psz_sdp++;
        }

        if( ( psz_eol = strchr( psz_sdp, '\n' ) ) == NULL )
        {
            psz_eol = psz_sdp + strlen( psz_sdp );
        }
        if( psz_eol > psz_sdp && *( psz_eol - 1 ) == '\r' )
        {
            psz_eol--;
        }

        if( psz_eol <= psz_sdp )
        {
            break;
        }
        *psz_eol++ = '\0';

        /* no space allowed between fields */
        if( psz_sdp[1] != '=' )
        {
            msg_Warn( p_obj, "invalid packet" ) ;
            /* MEMLEAK ! */
            return NULL;
        }

        /* Now parse each line */
        switch( psz_sdp[0] )
        {
            case( 'v' ):
                break;
            case( 's' ):
                p_sdp->psz_sessionname = strdup( &psz_sdp[2] );
                break;
            case ( 'o' ):
            case( 'i' ):
            case( 'u' ):
            case( 'e' ):
            case( 'p' ):
            case( 't' ):
            case( 'r' ):
                break;
            case( 'a' ): /* attribute */
            {
                char *psz_eon = strchr( &psz_sdp[2], ':' );

                 attribute_t *p_attr = (attribute_t *)malloc(
                                        sizeof( attribute_t ) );

                /* Attribute with value */
                if( psz_eon )
                {
                    *psz_eon++ = '\0';

                    p_attr->psz_field = strdup( &psz_sdp[2] );
                    p_attr->psz_value = strdup( psz_eon );
                }
                else /* Attribute without value */
                {
                    p_attr->psz_field = strdup( &psz_sdp[2] );
                    p_attr->psz_value = NULL;
                }

                TAB_APPEND( p_sdp->i_attributes, p_sdp->pp_attributes, p_attr );
                break;
            }

            case( 'm' ): /* Media announcement */
            {
                /* If we have several medias, we pass the announcement to
                 * LIVE.COM, so just count them */
                p_sdp->i_media++;
                if( p_sdp->i_media == 1 )
                {
                    p_sdp->psz_media = strdup( &psz_sdp[2] );
                }
                break;
            }

            case( 'c' ):
            {
                if( p_sdp->i_media > 1 )
                    break;

                p_sdp->psz_connection = strdup( &psz_sdp[2] );
                break;
            }

            default:
               break;
        }

        psz_sdp = psz_eol;
    }

    return p_sdp;
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

static int InitSocket( services_discovery_t *p_sd, char *psz_address, int i_port )
{
    int i_fd = net_OpenUDP( p_sd, psz_address, i_port, "", 0 );

    if( i_fd != -1 )
    {
        INSERT_ELEM(  p_sd->p_sys->pi_fd,
                      p_sd->p_sys->i_fd,
                      p_sd->p_sys->i_fd,
                      i_fd );
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

#ifdef HAVE_ZLIB_H
static int Decompress( unsigned char *psz_src, unsigned char **_dst, int i_len )
{
    int i_result, i_dstsize, n;
    unsigned char *psz_dst;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;

    i_result = inflateInit(&d_stream);
    if( i_result != Z_OK )
    {
        printf( "inflateInit() failed. Result: %d\n", i_result );
        return( -1 );
    }
#if 0
    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    i_position = p_playlist->i_index;
    
    /* Gather the complete sdp file */
    for( ;; )
    {
        int i_read = stream_Read( p_demux->s, &p_sdp[i_sdp], i_sdp_max - i_sdp - 1 );
#endif
    d_stream.next_in = (Bytef *)psz_src;
    d_stream.avail_in = i_len;
    n = 0;

    psz_dst = NULL;

    do
    {
        n++;
        psz_dst = (unsigned char *)realloc( psz_dst, n * 1000 );
        d_stream.next_out = (Bytef *)&psz_dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;

        i_result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( i_result != Z_OK ) && ( i_result != Z_STREAM_END ) )
        {
            printf( "Zlib decompression failed. Result: %d\n", i_result );
            return( -1 );
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( i_result != Z_STREAM_END ) );

    i_dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    *_dst = (unsigned char *)realloc( psz_dst, i_dstsize );

    return i_dstsize;
}
#endif


static void FreeSDP( sdp_t *p_sdp )
{
    int i;
    FREE( p_sdp->psz_sdp );
    FREE( p_sdp->psz_sessionname );
    FREE( p_sdp->psz_connection );
    FREE( p_sdp->psz_media );
    FREE( p_sdp->psz_uri );
    for( i= p_sdp->i_attributes - 1; i >= 0 ; i-- )
    {
        struct attribute_t *p_attr = p_sdp->pp_attributes[i];
        FREE( p_sdp->pp_attributes[i]->psz_field );
        FREE( p_sdp->pp_attributes[i]->psz_value );
        REMOVE_ELEM( p_sdp->pp_attributes, p_sdp->i_attributes, i);
        FREE( p_attr );
    }
    free( p_sdp );
}

static int RemoveAnnounce( services_discovery_t *p_sd,
                           sap_announce_t *p_announce )
{

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                          VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_announce->p_sdp )
    {
        FreeSDP( p_announce->p_sdp );
    }

    if( !p_playlist )
    {
        return VLC_EGENERIC;
    }

    if( p_announce->p_item )
    {
        playlist_Delete( p_playlist, p_announce->p_item->input.i_id );
    }

    vlc_object_release( p_playlist );

    free( p_announce );

    return VLC_SUCCESS;
}


static void CacheLoad( services_discovery_t *p_sd )
{
    msg_Warn( p_sd, "Cache not implemented") ;
}

static void CacheSave( services_discovery_t *p_sd )
{
    msg_Warn( p_sd, "Cache not implemented") ;
}
