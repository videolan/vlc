/*****************************************************************************
 * daap.c :  Apple DAAP discovery module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "network.h"

#include <vlc/input.h>

#include <daap/client.h>

/************************************************************************
 * Macros and definitions
 ************************************************************************/

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );
    static int  OpenAccess ( vlc_object_t * );
    static void CloseAccess( vlc_object_t * );

vlc_module_begin();
    set_description( _("DAAP shares") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

    add_submodule();
        set_description( _( "DAAP access") );
        set_capability( "access2", 0 );
        set_callbacks( OpenAccess, CloseAccess );
vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

typedef struct dhost_s {
    char *psz_name;
    int i_id;

    DAAP_SClientHost *p_host;
    vlc_bool_t b_updated;
    vlc_bool_t b_new;
    int i_database_id;

    playlist_item_t *p_node;

    DAAP_ClientHost_DatabaseItem *p_songs;
    int i_songs;
} dhost_t;

typedef struct daap_db_s {
    dhost_t **pp_hosts;
    int       i_hosts;

    int i_last_id;

    vlc_mutex_t search_lock;
} daap_db_t;

struct services_discovery_sys_t {
    playlist_item_t *p_node;

    DAAP_SClient *p_client;
    DAAP_SClientHost *p_host;

    daap_db_t *p_db;
};

struct access_sys_t {
    vlc_url_t url;

    dhost_t *p_host;
    int i_host;
    int i_song;

    daap_db_t *p_db;

    DAAP_ClientHost_Song song;
    DAAP_ClientHost_DatabaseItem songdata;
    int i_orig_size;
    void *p_orig_buffer;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Main functions */
    static void Run    ( services_discovery_t *p_sd );
    static void Callback( DAAP_SClient *p_client, DAAP_Status status,
                          int i_pos, void *p_context );
    static int EnumerateCallback( DAAP_SClient *p_client,
                                  DAAP_SClientHost *p_host,
                                  void *p_context );
    static void OnHostsUpdate( services_discovery_t *p_sd );
    static void ProcessHost( services_discovery_t *p_sd, dhost_t *p_host );
    static void FreeHost( services_discovery_t *p_sd, dhost_t *p_host );

    static int Control( access_t *p_access, int i_query, va_list args );
    static int Read( access_t *, uint8_t *, int );
    static int Seek( access_t *, int64_t );

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
    vlc_value_t         val;

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    p_sys->p_db = (daap_db_t *)malloc( sizeof( daap_db_t ) );
    if( !p_sys->p_db )
    {
        return VLC_EGENERIC;
    }
    p_sys->p_db->pp_hosts = NULL;
    p_sys->p_db->i_hosts = 0;

    var_Create( p_sd->p_vlc, "daap-db", VLC_VAR_ADDRESS );
    val.p_address = p_sys->p_db;
    var_Set( p_sd->p_vlc, "daap-db", val );

    vlc_mutex_init( p_sd, &p_sys->p_db->search_lock );

    /* Init DAAP */
    p_sys->p_client = DAAP_Client_Create( Callback, p_sd );
    p_sys->p_db->i_last_id = 0;

    /* TODO: Set debugging correctly */
//    DAAP_Client_SetDebug( p_sys->p_client, "+trace" );


    /* Create our playlist node */
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling DAAP" );
        return VLC_EGENERIC;
    }

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                         _("DAAP shares"), p_view->p_root );
    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );
    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

static int OpenAccess( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    vlc_value_t val;
    vlc_bool_t b_found = VLC_FALSE;
    int i, i_ret;

    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );

    i_ret = var_Get( p_access->p_vlc , "daap-db", &val );
    p_sys->p_db = val.p_address;

    if( p_sys->p_db == NULL || i_ret )
    {
        msg_Err( p_access, "The DAAP services_discovery module must be enabled" );
        return VLC_EGENERIC;
    }

    vlc_UrlParse( &p_sys->url, p_access->psz_path, 0 );

    p_sys->p_host = NULL;
    p_sys->i_host = atoi( p_sys->url.psz_host ) ;
    p_sys->i_song = p_sys->url.i_port;

    if( !p_sys->i_host || !p_sys->i_song )
    {
        msg_Err( p_access, "invalid host or song" );
        return VLC_EGENERIC;
    }

    /* Search the host */
    vlc_mutex_lock( &p_sys->p_db->search_lock );
    for( i = 0 ; i < p_sys->p_db->i_hosts ; i++ )
    {
        if( p_sys->p_db->pp_hosts[i]->i_id == p_sys->i_host )
        {
            p_sys->p_host = p_sys->p_db->pp_hosts[i];
            break;
        }
    }
    if( p_sys->p_host )
    {
       for( i = 0 ; i < p_sys->p_host->i_songs ; i++ )
       {
           if( p_sys->p_host->p_songs[i].id == p_sys->i_song )
           {
               p_sys->songdata = p_sys->p_host->p_songs[i];
               b_found = VLC_TRUE;
               break;
           }
       }
       if( !b_found )
       {
           msg_Err( p_access, "invalid song (not found in %i)",
                             p_sys->p_host->i_songs );
       }
    }
    else
    {
        msg_Warn( p_access, "invalid host (not found in %i)",
                             p_sys->p_db->i_hosts );
    }
    vlc_mutex_unlock( &p_sys->p_db->search_lock );

    if( !p_sys->p_host || !b_found )
    {
        return VLC_EGENERIC;
    }


    msg_Dbg( p_access, "Downloading %s song %i (db %i)",
                           p_sys->songdata.songformat,
                           p_sys->i_song, p_sys->p_host->i_database_id );

    /* FIXME: wait for better method by upstream */
    i_ret = DAAP_ClientHost_GetAudioFile( p_sys->p_host->p_host,
                                          p_sys->p_host->i_database_id,
                                          p_sys->i_song,
                                          p_sys->songdata.songformat,
                                          &(p_sys->song) );

    msg_Dbg( p_access, "Finished downloading, read %i bytes (ret %i)",
                                          p_sys->song.size, i_ret );

    p_access->info.i_size = p_sys->song.size;

    if( i_ret != 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    playlist_t *p_playlist;
    int i;

    p_playlist = (playlist_t *) vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );

    for( i = 0 ; i< p_sys->p_db->i_hosts ; i++ )
    {
        FreeHost( p_sd, p_sys->p_db->pp_hosts[i] );
    }

    var_Destroy( p_sd->p_vlc, "daap-db" );

    if( p_playlist )
    {
        playlist_NodeDelete( p_playlist, p_sys->p_node, VLC_TRUE, VLC_TRUE );
        vlc_object_release( p_playlist );
    }

    free( p_sys );
}

static void CloseAccess( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*) p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys )
    {
        if( p_sys->p_host )
        {
            p_sys->song.data = p_sys->p_orig_buffer;
            p_sys->song.size = p_sys->i_orig_size;
            DAAP_ClientHost_FreeAudioFile( p_sys->p_host->p_host, &p_sys->song );
        }
        free( p_sys );
    }
}

/*****************************************************************************
 * Run: main DAAP thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    while( !p_sd->b_die )
    {
        msleep( 100000 );
    }
}

/*****************************************************************************
 * Access functions
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;
    int64_t *pi_64;
    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t *)va_arg( args, vlc_bool_t *);
            *pb_bool = VLC_TRUE;
            break;
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t *)va_arg( args, vlc_bool_t *);
            *pb_bool = VLC_TRUE;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t *)va_arg( args, int64_t *);
            *pi_64 = (int64_t)300000;
            break;

        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query control %i", i_query );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Read( access_t *p_access, uint8_t *p_buffer, int i_size )
{
    access_sys_t *p_sys = (access_sys_t *)p_access->p_sys;
    int i_send;

    if( i_size < p_sys->song.size && p_sys->song.size > 0 )
    {
        i_send = i_size;
    }
    else if( p_sys->song.size == 0 )
    {
        return 0;
    }
    else
    {
        i_send = p_sys->song.size;
    }

    memcpy( p_buffer, p_sys->song.data, i_send );
    p_sys->song.size -= i_send;
    p_sys->song.data += i_send;

    return i_send;
}

static int Seek( access_t *p_access, int64_t i_pos )
{
    if( i_pos > p_access->p_sys->i_orig_size )
    {
        return VLC_EGENERIC;
    }
    p_access->p_sys->song.size = p_access->p_sys->i_orig_size - i_pos;
    p_access->p_sys->song.data = p_access->p_sys->p_orig_buffer + i_pos;
    return VLC_SUCCESS;
}

/**************************************************************
 * Local functions
 **************************************************************/
static void Callback( DAAP_SClient *p_client, DAAP_Status status,
                      int i_pos, void *p_context )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_context;

    if( status == DAAP_STATUS_hostschanged )
    {
        OnHostsUpdate( p_sd );
    }
    else if( status == DAAP_STATUS_downloading )
    {
    }
}

static void OnHostsUpdate( services_discovery_t *p_sd )
{
    int i;

    for( i = 0 ; i< p_sd->p_sys->p_db->i_hosts ; i ++ )
    {
        p_sd->p_sys->p_db->pp_hosts[i]->b_updated = VLC_FALSE;
        p_sd->p_sys->p_db->pp_hosts[i]->b_new     = VLC_FALSE;
    }

    vlc_mutex_lock( &p_sd->p_sys->p_db->search_lock );
    DAAP_Client_EnumerateHosts( p_sd->p_sys->p_client, EnumerateCallback, p_sd);

    for( i = 0 ; i< p_sd->p_sys->p_db->i_hosts ; i ++ )
    {
        if( p_sd->p_sys->p_db->pp_hosts[i]->b_updated == VLC_FALSE )
        {
            dhost_t *p_host = p_sd->p_sys->p_db->pp_hosts[i];
            FreeHost( p_sd, p_host );
            REMOVE_ELEM( p_sd->p_sys->p_db->pp_hosts,
                         p_sd->p_sys->p_db->i_hosts, i );
        }
    }
    vlc_mutex_unlock( &p_sd->p_sys->p_db->search_lock );

    for( i = 0 ; i< p_sd->p_sys->p_db->i_hosts ; i ++ )
    {
        if( p_sd->p_sys->p_db->pp_hosts[i]->b_new )
            ProcessHost( p_sd, p_sd->p_sys->p_db->pp_hosts[i] );
    }
}

static int EnumerateCallback( DAAP_SClient *p_client,
                              DAAP_SClientHost *p_host,
                              void *p_context )
{
    int i;
    int i_size = DAAP_ClientHost_GetSharename( p_host, NULL, 0 );
    vlc_bool_t b_found = VLC_FALSE;
    char *psz_buffer = (char *)malloc( i_size );
    DAAP_ClientHost_GetSharename( p_host, psz_buffer, i_size );

    services_discovery_t *p_sd = (services_discovery_t *)p_context;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    for( i = 0 ; i< p_sys->p_db->i_hosts; i++ )
    {
        if( !strcmp( p_sys->p_db->pp_hosts[i]->psz_name, psz_buffer ) )
        {
            p_sys->p_db->pp_hosts[i]->b_updated = VLC_TRUE;
            b_found = VLC_TRUE;
            break;
        }
    }

    if( !b_found )
    {
        dhost_t *p_vlchost = (dhost_t *)malloc( sizeof( dhost_t ) );
        p_vlchost->p_node = NULL;
        p_vlchost->p_host = p_host;
        p_vlchost->psz_name = psz_buffer;
        p_vlchost->b_new = VLC_TRUE;
        p_vlchost->b_updated = VLC_TRUE;
        INSERT_ELEM( p_sys->p_db->pp_hosts, p_sys->p_db->i_hosts,
                     p_sys->p_db->i_hosts, p_vlchost );
    }

    return VLC_SUCCESS;
}

static void ProcessHost( services_discovery_t *p_sd, dhost_t *p_host )
{
    int i_dbsize, i_db, i, i_songsize, i_ret;
    int i_size = DAAP_ClientHost_GetSharename( p_host->p_host, NULL, 0 );

    playlist_t *p_playlist;

    p_playlist = (playlist_t *) vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( !p_playlist )
    {
        return;
    }

    /* Connect to host */
    if( p_host->b_new )
    {
        p_host->psz_name = (char *)malloc( i_size );
        p_host->b_new = VLC_FALSE;
        DAAP_ClientHost_GetSharename( p_host->p_host, p_host->psz_name ,
                                      i_size );

        msg_Dbg( p_sd, "new share %s", p_host->psz_name );
        DAAP_ClientHost_AddRef( p_host->p_host );
        i_ret = DAAP_ClientHost_Connect( p_host->p_host );
        if( i_ret )
        {
            msg_Warn( p_sd, "unable to connect to DAAP host %s",
                             p_host->psz_name );
//            DAAP_ClientHost_Release( p_host->p_host );
            vlc_object_release( p_playlist );
            return;
        }

        p_host->p_node = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                              p_host->psz_name,
                                              p_sd->p_sys->p_node );
        p_host->i_id = ++p_sd->p_sys->p_db->i_last_id;
    }

    /* Get DB */
    i_dbsize = DAAP_ClientHost_GetDatabases( p_host->p_host, NULL, NULL, 0 );

    DAAP_ClientHost_Database *p_database = malloc( i_dbsize );
    DAAP_ClientHost_GetDatabases( p_host->p_host, p_database, &i_db, i_dbsize );


    if( !i_db || !p_database )
    {
        msg_Warn( p_sd, "no database on DAAP host %s", p_host->psz_name );
        vlc_object_release( p_playlist );
        return;
    }

    /* We only use the first database */
    p_host->i_database_id = p_database[0].id;

    /* Get songs */
    i_songsize = DAAP_ClientHost_GetDatabaseItems( p_host->p_host,
                                                   p_host->i_database_id,
                                                   NULL, NULL, 0 );
    if( !i_songsize )
    {
        vlc_object_release( p_playlist );
        return;
    }
    p_host->p_songs = malloc( i_songsize );

    DAAP_ClientHost_GetDatabaseItems( p_host->p_host ,
                                      p_host->i_database_id,
                                      p_host->p_songs,
                                      &p_host->i_songs, i_songsize );

    for( i = 0; i< p_host->i_songs; i++ )
    {
        playlist_item_t *p_item;
        int i_len = 7 + 10 + 1 + 10 ;    /* "daap://" + host + ":" + song */
        char *psz_buff = (char *)malloc( i_len );

        snprintf( psz_buff, i_len, "daap://%i:%i", p_host->i_id,
                                                   p_host->p_songs[i].id );
        p_item = playlist_ItemNew( p_sd, psz_buff,
                                         p_host->p_songs[i].itemname );
        vlc_input_item_AddInfo( &p_item->input, _("Meta-information"),
                                _("Artist"), p_host->p_songs[i].songartist );
        vlc_input_item_AddInfo( &p_item->input, _("Meta-information"),
                                _("Album"), p_host->p_songs[i].songalbum );

        playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                              p_host->p_node, PLAYLIST_APPEND, PLAYLIST_END );

    }

    DAAP_ClientHost_AsyncWaitUpdate( p_host->p_host );

    vlc_object_release( p_playlist );
}

static void FreeHost( services_discovery_t *p_sd, dhost_t *p_host )
{
    playlist_t *p_playlist;

    if( p_host->p_host )
    {
        DAAP_ClientHost_Disconnect( p_host->p_host );
        DAAP_ClientHost_Release( p_host->p_host );
    }

    p_playlist = (playlist_t *) vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( p_playlist )
    {
        if( p_host->p_node )
            playlist_NodeDelete( p_playlist, p_host->p_node, VLC_TRUE ,
                                                             VLC_TRUE);
        vlc_object_release( p_playlist );
    }

    if( p_host->p_songs ) free( p_host->p_songs );
}
