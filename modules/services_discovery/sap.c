/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Includes
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include <network.h>
#include <charset.h>

#include <ctype.h>
#include <errno.h>

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
/* Global-scope SAP address */
#define SAP_V4_GLOBAL_ADDRESS   "224.2.127.254"
/* Organization-local SAP address */
#define SAP_V4_ORG_ADDRESS      "239.195.255.255"
/* Local (smallest non-link-local scope) SAP address */
#define SAP_V4_LOCAL_ADDRESS    "239.255.255.255"
/* Link-local SAP address */
#define SAP_V4_LINK_ADDRESS     "224.0.0.255"
#define ADD_SESSION 1

#define SAP_V6_1 "FF0"
/* Scope is inserted between them */
#define SAP_V6_2 "::2:7FFE"
/* See RFC3513 for list of valid scopes */
/* FIXME: find a way to listen to link-local scope */
static const char ipv6_scopes[] = "1456789ABCDE";


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SAP_ADDR_TEXT N_( "SAP multicast address" )
#define SAP_ADDR_LONGTEXT N_( "The SAP module normally chooses itself the " \
                              "right addresses to listen to. However, you " \
                              "can specify a specific address." )
#define SAP_IPV4_TEXT N_( "IPv4 SAP" )
#define SAP_IPV4_LONGTEXT N_( \
      "Listen to IPv4 announcements " \
      "on the standard address." )
#define SAP_IPV6_TEXT N_( "IPv6 SAP" )
#define SAP_IPV6_LONGTEXT N_( \
      "Listen to IPv6 announcements " \
      "on the standard addresses." )
#define SAP_SCOPE_TEXT N_( "IPv6 SAP scope" )
#define SAP_SCOPE_LONGTEXT N_( \
       "Scope for IPv6 announcements (default is 8)." )
#define SAP_TIMEOUT_TEXT N_( "SAP timeout (seconds)" )
#define SAP_TIMEOUT_LONGTEXT N_( \
       "Delay after which SAP items get deleted if no new announcement " \
       "is received." )
#define SAP_PARSE_TEXT N_( "Try to parse the announce" )
#define SAP_PARSE_LONGTEXT N_( \
       "This enables actual parsing of the announces by the SAP module. " \
       "Otherwise, all announcements are parsed by the \"livedotcom\" " \
       "(RTP/RTSP) module." )
#define SAP_STRICT_TEXT N_( "SAP Strict mode" )
#define SAP_STRICT_LONGTEXT N_( \
       "When this is set, the SAP parser will discard some non-compliant " \
       "announcements." )
#define SAP_CACHE_TEXT N_("Use SAP cache")
#define SAP_CACHE_LONGTEXT N_( \
       "This enables a SAP caching mechanism. " \
       "This will result in lower SAP startup time, but you could end up " \
       "with items corresponding to legacy streams." )
#define SAP_TIMESHIFT_TEXT N_("Allow timeshifting")
#define SAP_TIMESHIFT_LONGTEXT N_( "This automatically enables timeshifting " \
        "for streams discovered through SAP announcements." )

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );
    static int  OpenDemux ( vlc_object_t * );
    static void CloseDemux ( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("SAP"));
    set_description( _("SAP Announcements") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_string( "sap-addr", NULL, NULL,
                SAP_ADDR_TEXT, SAP_ADDR_LONGTEXT, VLC_TRUE );
    add_bool( "sap-ipv4", 1 , NULL,
               SAP_IPV4_TEXT,SAP_IPV4_LONGTEXT, VLC_TRUE );
    add_bool( "sap-ipv6", 1 , NULL,
              SAP_IPV6_TEXT, SAP_IPV6_LONGTEXT, VLC_TRUE );
    add_integer( "sap-timeout", 1800, NULL,
                 SAP_TIMEOUT_TEXT, SAP_TIMEOUT_LONGTEXT, VLC_TRUE );
    add_bool( "sap-parse", 1 , NULL,
               SAP_PARSE_TEXT,SAP_PARSE_LONGTEXT, VLC_TRUE );
    add_bool( "sap-strict", 0 , NULL,
               SAP_STRICT_TEXT,SAP_STRICT_LONGTEXT, VLC_TRUE );
#if 0
    add_bool( "sap-cache", 0 , NULL,
               SAP_CACHE_TEXT,SAP_CACHE_LONGTEXT, VLC_TRUE );
#endif
    add_bool( "sap-timeshift", 0 , NULL,
              SAP_TIMESHIFT_TEXT,SAP_TIMESHIFT_LONGTEXT, VLC_TRUE );

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

    /* o field */
    char *psz_username;
    char *psz_network_type;
    char *psz_address_type;
    char *psz_address;
    int64_t i_session_id;

    /* "computed" URI */
    char *psz_uri;

    int           i_in; /* IP version */

    int           i_media;
    int           i_media_type;

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

    int i_input_id;
    int i_item_id_cat;
    int i_item_id_one;
};

struct services_discovery_sys_t
{
    /* Socket descriptors */
    int i_fd;
    int *pi_fd;

    /* playlist node */
    playlist_item_t *p_node_cat;
    playlist_item_t *p_node_one;

    /* Table of announces */
    int i_announces;
    struct sap_announce_t **pp_announces;

    /* Modes */
    vlc_bool_t  b_strict;
    vlc_bool_t  b_parse;
    vlc_bool_t  b_timeshift;

    int i_timeout;
};

struct demux_sys_t
{
    sdp_t *p_sdp;
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
    static int ParseSAP( services_discovery_t *p_sd, const uint8_t *p_buffer, size_t i_read );
    static sdp_t *ParseSDP (vlc_object_t *p_sd, const char *psz_sdp);
    static sap_announce_t *CreateAnnounce( services_discovery_t *, uint16_t, sdp_t * );
    static int RemoveAnnounce( services_discovery_t *p_sd, sap_announce_t *p_announce );

/* Helper functions */
    static char *GetAttribute( sdp_t *p_sdp, const char *psz_search );
    static vlc_bool_t IsSameSession( sdp_t *p_sdp1, sdp_t *p_sdp2 );
    static int InitSocket( services_discovery_t *p_sd, const char *psz_address, int i_port );
    static int Decompress( const unsigned char *psz_src, unsigned char **_dst, int i_len );
    static void FreeSDP( sdp_t *p_sdp );

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)
                                malloc( sizeof( services_discovery_sys_t ) );

    p_sys->i_timeout = var_CreateGetInteger( p_sd, "sap-timeout" );

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    p_sys->pi_fd = NULL;
    p_sys->i_fd = 0;

    p_sys->b_strict = var_CreateGetInteger( p_sd, "sap-strict");
    p_sys->b_parse = var_CreateGetInteger( p_sd, "sap-parse" );

#if 0
    if( var_CreateGetInteger( p_sd, "sap-cache" ) )
    {
        CacheLoad( p_sd );
    }
#endif

    /* Cache sap_timeshift value */
    p_sys->b_timeshift = var_CreateGetInteger( p_sd, "sap-timeshift" )
            ? VLC_TRUE : VLC_FALSE;

    /* Create our playlist node */
    pl_Yield( p_sd );

    playlist_NodesPairCreate( pl_Get( p_sd ), _("SAP sessions"),
                              &p_sys->p_node_cat, &p_sys->p_node_one,
                              VLC_TRUE );

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
    int i_max_sdp = 1024;
    int i_sdp = 0;
    char *psz_sdp = NULL;
    sdp_t *p_sdp = NULL;

    if( !var_CreateGetInteger( p_demux, "sap-parse" ) )
    {
        /* We want livedotcom module to parse this SDP file */
        return VLC_EGENERIC;
    }

    /* Probe for SDP */
    if( p_demux->s )
    {
        if( stream_Peek( p_demux->s, &p_peek, 7 ) < 7 ) return VLC_EGENERIC;

        if( strncmp( (char*)p_peek, "v=0\r\n", 5 ) &&
            strncmp( (char*)p_peek, "v=0\n", 4 ) &&
            ( p_peek[0] < 'a' || p_peek[0] > 'z' || p_peek[1] != '=' ) )
        {
            return VLC_EGENERIC;
        }
    }

    psz_sdp = (char *)malloc( i_max_sdp );
    if( !psz_sdp ) return VLC_EGENERIC;

    /* Gather the complete sdp file */
    for( ;; )
    {
        int i_read = stream_Read( p_demux->s,
                                  &psz_sdp[i_sdp], i_max_sdp - i_sdp - 1 );

        if( i_read < 0 )
        {
            msg_Err( p_demux, "failed to read SDP" );
            goto error;
        }

        i_sdp += i_read;

        if( i_read < i_max_sdp - i_sdp - 1 )
        {
            psz_sdp[i_sdp] = '\0';
            break;
        }

        i_max_sdp += 1000;
        psz_sdp = (char *)realloc( psz_sdp, i_max_sdp );
    }

    p_sdp = ParseSDP( VLC_OBJECT(p_demux), psz_sdp );

    if( !p_sdp )
    {
        msg_Warn( p_demux, "invalid SDP");
        goto error;
    }

    if( p_sdp->i_media > 1 )
    {
        goto error;
    }

    if( ParseConnection( VLC_OBJECT( p_demux ), p_sdp ) )
    {
        p_sdp->psz_uri = NULL;
    }
    if( p_sdp->i_media_type != 33 && p_sdp->i_media_type != 32 &&
        p_sdp->i_media_type != 14 )
        goto error;

    if( p_sdp->psz_uri == NULL ) goto error;

    p_demux->p_sys = (demux_sys_t *)malloc( sizeof(demux_sys_t) );
    p_demux->p_sys->p_sdp = p_sdp;
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    FREENULL( psz_sdp );
    return VLC_SUCCESS;

error:
    FREENULL( psz_sdp );
    if( p_sdp ) FreeSDP( p_sdp ); p_sdp = NULL;
    stream_Seek( p_demux->s, 0 );
    return VLC_EGENERIC;
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
    FREENULL( p_sys->pi_fd );

#if 0
    if( config_GetInt( p_sd, "sap-cache" ) )
    {
        CacheSave( p_sd );
    }
#endif

    for( i = p_sys->i_announces  - 1;  i>= 0; i-- )
    {
        RemoveAnnounce( p_sd, p_sys->pp_announces[i] );
    }
    FREENULL( p_sys->pp_announces );

    playlist_NodeDelete( pl_Get(p_sd), p_sys->p_node_cat, VLC_TRUE,
                         VLC_TRUE );
    playlist_NodeDelete( pl_Get(p_sd), p_sys->p_node_one, VLC_TRUE,
                         VLC_TRUE );
    pl_Release( p_sd );
    free( p_sys );
}

/*****************************************************************************
 * CloseDemux: Close the demuxer
 *****************************************************************************/
static void CloseDemux( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    if( p_demux->p_sys )
    {
        if( p_demux->p_sys->p_sdp ) { FreeSDP( p_demux->p_sys->p_sdp ); p_demux->p_sys->p_sdp = NULL; }
        free( p_demux->p_sys );
    }
}

/*****************************************************************************
 * Run: main SAP thread
 *****************************************************************************
 * Listens to SAP packets, and sends them to packet_handle
 *****************************************************************************/
#define MAX_SAP_BUFFER 5000

static void Run( services_discovery_t *p_sd )
{
    char *psz_addr;
    int i;

    /* Braindead Winsock DNS resolver will get stuck over 2 seconds per failed
     * DNS queries, even if the DNS server returns an error with milliseconds.
     * You don't want to know why the bug (as of XP SP2) wasn't fixed since
     * Winsock 1.1 from Windows 95, if not Windows 3.1.
     * Anyway, to avoid a 30 seconds delay for failed IPv6 socket creation,
     * we have to open sockets in Run() rather than Open(). */
    if( var_CreateGetInteger( p_sd, "sap-ipv4" ) )
    {
        InitSocket( p_sd, SAP_V4_GLOBAL_ADDRESS, SAP_PORT );
        InitSocket( p_sd, SAP_V4_ORG_ADDRESS, SAP_PORT );
        InitSocket( p_sd, SAP_V4_LOCAL_ADDRESS, SAP_PORT );
        InitSocket( p_sd, SAP_V4_LINK_ADDRESS, SAP_PORT );
    }
    if( var_CreateGetInteger( p_sd, "sap-ipv6" ) )
    {
        char psz_address[] = SAP_V6_1"0"SAP_V6_2;
        const char *c_scope;

        for( c_scope = ipv6_scopes; *c_scope; c_scope++ )
        {
            psz_address[sizeof(SAP_V6_1) - 1] = *c_scope;
            InitSocket( p_sd, psz_address, SAP_PORT );
        }
    }

    psz_addr = var_CreateGetString( p_sd, "sap-addr" );
    if( psz_addr && *psz_addr )
    {
        InitSocket( p_sd, psz_addr, SAP_PORT );
        free( psz_addr );
    }

    if( p_sd->p_sys->i_fd == 0 )
    {
        msg_Err( p_sd, "unable to listen on any address" );
        return;
    }

    /* read SAP packets */
    while( !p_sd->b_die )
    {
        int i_read;
        uint8_t p_buffer[MAX_SAP_BUFFER+1];

        i_read = net_Select( p_sd, p_sd->p_sys->pi_fd, NULL,
                             p_sd->p_sys->i_fd, p_buffer,
                             MAX_SAP_BUFFER, 500000 );

        /* Check for items that need deletion */
        for( i = 0; i < p_sd->p_sys->i_announces; i++ )
        {
            mtime_t i_timeout = ( mtime_t ) 1000000 * p_sd->p_sys->i_timeout;

            if( mdate() - p_sd->p_sys->pp_announces[i]->i_last > i_timeout )
            {
                RemoveAnnounce( p_sd, p_sd->p_sys->pp_announces[i] );
            }
        }

        /* Minimum length is > 6 */
        if( i_read <= 6 )
        {
            if( i_read < 0 )
            {
                msg_Warn( p_sd, "socket read error" );
            }
            continue;
        }

        p_buffer[i_read] = '\0';

        /* Parse the packet */
        ParseSAP( p_sd, p_buffer, i_read );
    }
}

/**********************************************************************
 * Demux: reads and demuxes data packets
 * Return -1 if error, 0 if EOF, 1 else
 **********************************************************************/
static int Demux( demux_t *p_demux )
{
    sdp_t *p_sdp = p_demux->p_sys->p_sdp;
    input_thread_t *p_input;
    input_item_t *p_parent_input;

    playlist_t *p_playlist = pl_Yield( p_demux );
    p_input = (input_thread_t *)vlc_object_find( p_demux, VLC_OBJECT_INPUT,
                                                 FIND_PARENT );
    assert( p_input );
    if( !p_input )
    {
        msg_Err( p_demux, "parent input could not be found" );
        return VLC_EGENERIC;
    }

    p_parent_input = p_input->input.p_item;

    vlc_mutex_lock( &p_parent_input->lock );
    FREENULL( p_parent_input->psz_uri );
    p_parent_input->psz_uri = strdup( p_sdp->psz_uri );
    FREENULL( p_parent_input->psz_name );
    p_parent_input->psz_name = strdup( EnsureUTF8( p_sdp->psz_sessionname ) );
    p_parent_input->i_type = ITEM_TYPE_NET;

    if( p_playlist->status.p_item &&
             p_playlist->status.p_item->p_input == p_parent_input )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.p_node, p_playlist->status.p_item );
    }

    vlc_mutex_unlock( &p_parent_input->lock );
    vlc_object_release( p_input );
    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/**************************************************************
 * Local functions
 **************************************************************/

/* i_read is at least > 6 */
static int ParseSAP( services_discovery_t *p_sd, const uint8_t *buf,
                     size_t len )
{
    int i;
    const char          *psz_sdp;
    const uint8_t *end = buf + len;
    sdp_t               *p_sdp;

    assert (buf[len] == '\0');

    if (len < 4)
        return VLC_EGENERIC;

    uint8_t flags = buf[0];

    /* First, check the sap announce is correct */
    if ((flags >> 5) != 1)
        return VLC_EGENERIC;

    vlc_bool_t b_ipv6 = (flags & 0x10) != 0;
    vlc_bool_t b_need_delete = (flags & 0x04) != 0;

    if (flags & 0x02)
    {
        msg_Dbg( p_sd, "encrypted packet, unsupported" );
        return VLC_EGENERIC;
    }

    vlc_bool_t b_compressed = (flags & 0x01) != 0;

    uint16_t i_hash = U16_AT (buf + 2);

    if( p_sd->p_sys->b_strict && i_hash == 0 )
    {
        msg_Dbg( p_sd, "strict mode, discarding announce with null id hash");
        return VLC_EGENERIC;
    }

    // Skips source address and auth data
    buf += 4 + (b_ipv6 ? 16 : 4) + buf[1];
    if (buf > end)
        return VLC_EGENERIC;

    uint8_t *decomp = NULL;
    if (b_compressed)
    {
        int newsize = Decompress (buf, &decomp, end - buf);
        if (newsize < 0)
        {
            msg_Warn( p_sd, "decompression of sap packet failed" );
            return VLC_EGENERIC;
        }

        decomp = realloc (decomp, newsize + 1);
        decomp[newsize++] = '\0';

        psz_sdp = (const char *)decomp;
        len = newsize;
    }
    else
    {
        psz_sdp = (const char *)buf;
        len = end - buf;
    }

    assert (buf[len] == '\0');

    /* Skip payload type */
    /* SAPv1 has implicit "application/sdp" payload type: first line is v=0 */
    if (strncmp (psz_sdp, "v=0", 3))
    {
        size_t clen = strlen (psz_sdp) + 1;

        if (strcmp (psz_sdp, "application/sdp"))
        {
            msg_Dbg (p_sd, "unsupported content type: %s", psz_sdp);
            return VLC_EGENERIC;
        }

        // skips content type
        if (len <= clen)
            return VLC_EGENERIC;

        len -= clen;
        psz_sdp += clen;
    }

    /* Parse SDP info */
    p_sdp = ParseSDP( VLC_OBJECT(p_sd), psz_sdp );

    if( p_sdp == NULL )
    {
        return VLC_EGENERIC;
    }

    /* Decide whether we should add a playlist item for this SDP */
    /* Parse connection information (c= & m= ) */
    if( ParseConnection( VLC_OBJECT(p_sd), p_sdp ) )
    {
        p_sdp->psz_uri = NULL;
    }

    /* Multi-media or no-parse -> pass to LIVE.COM */
    if( p_sdp->i_media > 1 || ( p_sdp->i_media_type != 14 &&
                                p_sdp->i_media_type != 32 &&
                                p_sdp->i_media_type != 33) ||
        p_sd->p_sys->b_parse == VLC_FALSE )
    {
        if( p_sdp->psz_uri ) free( p_sdp->psz_uri );
        asprintf( &p_sdp->psz_uri, "sdp://%s", p_sdp->psz_sdp );
    }

    if( p_sdp->psz_uri == NULL ) return VLC_EGENERIC;

    for( i = 0 ; i< p_sd->p_sys->i_announces ; i++ )
    {
        /* FIXME: slow */
        /* FIXME: we create a new announce each time the sdp changes */
        if( IsSameSession( p_sd->p_sys->pp_announces[i]->p_sdp, p_sdp ) )
        {
            if( b_need_delete )
            {
                RemoveAnnounce( p_sd, p_sd->p_sys->pp_announces[i]);
            }
            else
            {
                p_sd->p_sys->pp_announces[i]->i_last = mdate();
            }
            FreeSDP( p_sdp ); p_sdp = NULL;
            return VLC_SUCCESS;
        }
    }
    /* Add item */
    if( p_sdp->i_media > 1 )
    {
        msg_Dbg( p_sd, "passing to liveMedia" );
    }

    CreateAnnounce( p_sd, i_hash, p_sdp );

    FREENULL (decomp);
    return VLC_SUCCESS;
}

sap_announce_t *CreateAnnounce( services_discovery_t *p_sd, uint16_t i_hash,
                                sdp_t *p_sdp )
{
    input_item_t *p_input;
    playlist_item_t     *p_item, *p_child;
    char *psz_value;
    sap_announce_t *p_sap = (sap_announce_t *)malloc(
                                        sizeof(sap_announce_t ) );
    services_discovery_sys_t *p_sys;
    if( p_sap == NULL )
        return NULL;

    p_sys = p_sd->p_sys;

    EnsureUTF8( p_sdp->psz_sessionname );
    p_sap->i_last = mdate();
    p_sap->i_hash = i_hash;
    p_sap->p_sdp = p_sdp;

    /* Create the actual playlist item here */
    p_input = input_ItemNewWithType( VLC_OBJECT(p_sd),
                                     p_sap->p_sdp->psz_uri,
                                     p_sdp->psz_sessionname,
                                     0, NULL, -1, ITEM_TYPE_NET );
    p_sap->i_input_id = p_input->i_id;
    if( !p_input )
    {
        free( p_sap );
        return NULL;
    }

    if( p_sys->b_timeshift )
        input_ItemAddOption( p_input, ":access-filter=timeshift" );

    psz_value = GetAttribute( p_sap->p_sdp, "tool" );
    if( psz_value != NULL )
    {
        input_ItemAddInfo( p_input, _("Session"),_("Tool"), psz_value );
    }
    if( strcmp( p_sdp->psz_username, "-" ) )
    {
        input_ItemAddInfo( p_input, _("Session"),
                                _("User"), p_sdp->psz_username );
    }

    /* Handle group */
    psz_value = GetAttribute( p_sap->p_sdp, "x-plgroup" );
    if( psz_value == NULL )
        psz_value = GetAttribute( p_sap->p_sdp, "plgroup" );

    if( psz_value != NULL )
    {
        EnsureUTF8( psz_value );

        p_child = playlist_ChildSearchName( p_sys->p_node_cat, psz_value );

        if( p_child == NULL )
        {
            p_child = playlist_NodeCreate( pl_Get( p_sd ), psz_value,
                                           p_sys->p_node_cat );
            p_child->i_flags &= ~PLAYLIST_SKIP_FLAG;
        }
    }
    else
    {
        p_child = p_sys->p_node_cat;
    }

    p_item = playlist_NodeAddInput( pl_Get( p_sd ), p_input, p_child,
                                    PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_item->i_flags &= ~PLAYLIST_SAVE_FLAG;
    p_sap->i_item_id_cat = p_item->i_id;

    p_item = playlist_NodeAddInput( pl_Get( p_sd ), p_input,
                        p_sys->p_node_one, PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_item->i_flags &= ~PLAYLIST_SAVE_FLAG;
    p_sap->i_item_id_one = p_item->i_id;

    TAB_APPEND( p_sys->i_announces, p_sys->pp_announces, p_sap );

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
    char *psz_eof = NULL;
    char *psz_parse = NULL;
    char psz_uri[1026];
    char *psz_proto = NULL;
    int i_port = 0;

    /* Parse c= field */
    if( p_sdp->psz_connection )
    {
        char hostname[1024];
        int ipv;

        /*
         * NOTE: we ignore the TTL parameter on-purpose, as some SAP
         * advertisers don't include it (and it is utterly useless).
         */
        if (sscanf (p_sdp->psz_connection, "IN IP%d %1023[^/]", &ipv,
                    hostname) != 2)
        {
            msg_Warn (p_obj, "unable to parse c field: \"%s\"",
                      p_sdp->psz_connection);
            return VLC_EGENERIC;
        }

        switch (ipv)
        {
            case 4:
            case 6:
                break;

            default:
                msg_Warn (p_obj, "unknown IP version %d", ipv);
                return VLC_EGENERIC;
        }

        if (strchr (hostname, ':') != NULL)
            sprintf (psz_uri, "[%s]", hostname);
        else
            strcpy (psz_uri, hostname);
    }

    /* Parse m= field */
    if( p_sdp->psz_media )
    {
        psz_parse = p_sdp->psz_media;

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';

            /*
             * That's ugly. We should go through every media, and make sure
             * at least one of them is audio or video. In the mean time, I
             * need to accept data too.
             */
            if( strncmp( psz_parse, "audio", 5 )
             && strncmp( psz_parse, "video", 5 )
             && strncmp( psz_parse, "data", 4 ) )
            {
                msg_Warn( p_obj, "unhandled media type \"%s\"", psz_parse );
                return VLC_EGENERIC;
            }

            psz_parse = psz_eof + 1;
        }
        else
        {
            msg_Warn( p_obj, "unable to parse m field (1)");
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
            return VLC_EGENERIC;
        }

        psz_eof = strchr( psz_parse, ' ' );

        if( psz_eof )
        {
            *psz_eof = '\0';
            psz_proto = strdup( psz_parse );

            psz_parse = psz_eof + 1;
            p_sdp->i_media_type = atoi( psz_parse );

        }
        else
        {
            msg_Dbg( p_obj, "incorrect m field, %s", p_sdp->psz_media );
            p_sdp->i_media_type = 33;
            psz_proto = strdup( psz_parse );
        }
    }

    if( psz_proto && !strncmp( psz_proto, "RTP/AVP", 7 ) )
    {
        free( psz_proto );
        psz_proto = strdup( "rtp" );
    }
    if( psz_proto && !strncasecmp( psz_proto, "UDP", 3 ) )
    {
        free( psz_proto );
        psz_proto = strdup( "udp" );
    }

    /* FIXME: HTTP support */

    if( i_port == 0 )
    {
        i_port = 1234;
    }

    /* handle SSM case */
    psz_parse = GetAttribute( p_sdp, "source-filter" );
    char psz_source[258] = "";
    if (psz_parse != NULL)
    {
        char psz_source_ip[256];

        if (sscanf (psz_parse, " incl IN IP%*c %*s %255s ", psz_source_ip) == 1)
        {
            if (strchr (psz_source_ip, ':') != NULL)
                sprintf (psz_source, "[%s]", psz_source_ip);
            else
                strcpy (psz_source, psz_source_ip);
        }
    }

    asprintf( &p_sdp->psz_uri, "%s://%s@%s:%i", psz_proto, psz_source,
              psz_uri, i_port );

    FREENULL( psz_proto );
    return VLC_SUCCESS;
}

/***********************************************************************
 * ParseSDP : SDP parsing
 * *********************************************************************
 * Validate SDP and parse all fields
 ***********************************************************************/
static sdp_t *ParseSDP (vlc_object_t *p_obj, const char *psz_sdp)
{
    sdp_t *p_sdp;
    vlc_bool_t b_invalid = VLC_FALSE;
    vlc_bool_t b_end = VLC_FALSE;
    if( psz_sdp == NULL )
    {
        return NULL;
    }

    if( psz_sdp[0] != 'v' || psz_sdp[1] != '=' )
    {
        msg_Warn( p_obj, "bad packet" );
        return NULL;
    }

    p_sdp = (sdp_t *)malloc( sizeof( sdp_t ) );
    if( p_sdp == NULL )
        return NULL;

    p_sdp->psz_sdp = strdup( psz_sdp );
    if( p_sdp->psz_sdp == NULL )
    {
        free( p_sdp );
        return NULL;
    }

    p_sdp->psz_sessionname = NULL;
    p_sdp->psz_media       = NULL;
    p_sdp->psz_connection  = NULL;
    p_sdp->psz_uri         = NULL;
    p_sdp->psz_address     = NULL;
    p_sdp->psz_address_type= NULL;

    p_sdp->i_media         = 0;
    p_sdp->i_attributes    = 0;
    p_sdp->pp_attributes   = NULL;

    while( *psz_sdp != '\0' && b_end == VLC_FALSE  )
    {
        char *psz_eol;
        char *psz_eof;
        char *psz_parse;
        char *psz_sess_id;

        while( *psz_sdp == '\r' || *psz_sdp == '\n' ||
               *psz_sdp == ' ' || *psz_sdp == '\t' )
        {
            psz_sdp++;
        }

        if( ( psz_eol = strchr( psz_sdp, '\n' ) ) == NULL )
        {
            psz_eol = psz_sdp + strlen( psz_sdp );
            b_end = VLC_TRUE;
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
            FreeSDP( p_sdp ); p_sdp = NULL;
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
            {
                int i_field = 0;
                /* o field is <username> <session id> <version>
                 *  <network type> <address type> <address> */

#define GET_FIELD( store ) \
                psz_eof = strchr( psz_parse, ' ' ); \
                if( psz_eof ) \
                { \
                    *psz_eof=0; store = strdup( psz_parse ); \
                } \
                else \
                { \
                    if( i_field != 5 ) \
                    { \
                        b_invalid = VLC_TRUE; break; \
                    } \
                    else \
                    { \
                        store = strdup( psz_parse ); \
                    } \
                }; \
                psz_parse = psz_eof + 1; i_field++;


                psz_parse = &psz_sdp[2];
                GET_FIELD( p_sdp->psz_username );
                GET_FIELD( psz_sess_id );

                p_sdp->i_session_id = atoll( psz_sess_id );

                FREENULL( psz_sess_id );

                GET_FIELD( psz_sess_id );
                FREENULL( psz_sess_id );

                GET_FIELD( p_sdp->psz_network_type );
                GET_FIELD( p_sdp->psz_address_type );
                GET_FIELD( p_sdp->psz_address );

                break;
            }
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
                attribute_t *p_attr = malloc( sizeof( attribute_t ) );

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

        if( b_invalid )
        {
            FreeSDP( p_sdp ); p_sdp = NULL;
            return NULL;
        }

        psz_sdp = psz_eol;
    }

    return p_sdp;
}

static int InitSocket( services_discovery_t *p_sd, const char *psz_address,
                       int i_port )
{
    int i_fd = net_OpenUDP( p_sd, psz_address, i_port, NULL, 0 );

    if( i_fd != -1 )
    {
        net_StopSend( i_fd );
        INSERT_ELEM(  p_sd->p_sys->pi_fd, p_sd->p_sys->i_fd,
                      p_sd->p_sys->i_fd, i_fd );
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static int Decompress( const unsigned char *psz_src, unsigned char **_dst, int i_len )
{
#ifdef HAVE_ZLIB_H
    int i_result, i_dstsize, n;
    unsigned char *psz_dst;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;

    i_result = inflateInit(&d_stream);
    if( i_result != Z_OK )
        return( -1 );

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
            return( -1 );
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( i_result != Z_STREAM_END ) );

    i_dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    *_dst = (unsigned char *)realloc( psz_dst, i_dstsize );

    return i_dstsize;
#else
    (void)psz_src;
    (void)_dst;
    (void)i_len;
    return -1;
#endif
}


static void FreeSDP( sdp_t *p_sdp )
{
    int i;
    FREENULL( p_sdp->psz_sdp );
    FREENULL( p_sdp->psz_sessionname );
    FREENULL( p_sdp->psz_connection );
    FREENULL( p_sdp->psz_media );
    FREENULL( p_sdp->psz_uri );
    FREENULL( p_sdp->psz_username );
    FREENULL( p_sdp->psz_network_type );

    FREENULL( p_sdp->psz_address );
    FREENULL( p_sdp->psz_address_type );

    for( i= p_sdp->i_attributes - 1; i >= 0 ; i-- )
    {
        struct attribute_t *p_attr = p_sdp->pp_attributes[i];
        FREENULL( p_sdp->pp_attributes[i]->psz_field );
        FREENULL( p_sdp->pp_attributes[i]->psz_value );
        REMOVE_ELEM( p_sdp->pp_attributes, p_sdp->i_attributes, i);
        FREENULL( p_attr );
    }
    FREENULL( p_sdp );
}

static int RemoveAnnounce( services_discovery_t *p_sd,
                           sap_announce_t *p_announce )
{
    int i;

    if( p_announce->p_sdp )
    {
        FreeSDP( p_announce->p_sdp );
        p_announce->p_sdp = NULL;
    }

    if( p_announce->i_input_id > -1 )
        playlist_LockDeleteAllFromInput( pl_Get(p_sd), p_announce->i_input_id );

    for( i = 0; i< p_sd->p_sys->i_announces; i++)
    {
        if( p_sd->p_sys->pp_announces[i] == p_announce )
        {
            REMOVE_ELEM( p_sd->p_sys->pp_announces, p_sd->p_sys->i_announces,
                         i);
            break;
        }
    }

    free( p_announce );

    return VLC_SUCCESS;
}

static vlc_bool_t IsSameSession( sdp_t *p_sdp1, sdp_t *p_sdp2 )
{
    /* A session is identified by
     * username, session_id, network type, address type and address */
    if( p_sdp1->psz_username && p_sdp2->psz_username &&
        p_sdp1->psz_network_type && p_sdp2->psz_network_type &&
        p_sdp1->psz_address_type && p_sdp2->psz_address_type &&
        p_sdp1->psz_address &&  p_sdp2->psz_address )
    {
        if(!strcmp( p_sdp1->psz_username , p_sdp2->psz_username ) &&
           !strcmp( p_sdp1->psz_network_type , p_sdp2->psz_network_type ) &&
           !strcmp( p_sdp1->psz_address_type , p_sdp2->psz_address_type ) &&
           !strcmp( p_sdp1->psz_address , p_sdp2->psz_address ) &&
           p_sdp1->i_session_id == p_sdp2->i_session_id )
        {
            return VLC_TRUE;
        }
        else
        {
            return VLC_FALSE;
        }
    }
    else
    {
        return VLC_FALSE;
    }
}
