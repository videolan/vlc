/*****************************************************************************
 * sql_monitor.c: SQL-based media library: directory scanning and monitoring
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

/** **************************************************************************
 * MONITORING AND DIRECTORY SCANNING FUNCTIONS
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "sql_media_library.h"
#include "vlc_playlist.h"
#include "vlc_url.h"
#include "vlc_fs.h"

static const char* ppsz_MediaExtensions[] =
                        { EXTENSIONS_AUDIO_CSV, EXTENSIONS_VIDEO_CSV, NULL };


/* Monitoring and directory scanning private functions */
typedef struct stat_list_t stat_list_t;
typedef struct preparsed_item_t preparsed_item_t;
static void UpdateLibrary( monitoring_thread_t *p_mon );
static void ScanFiles( monitoring_thread_t *, int, bool, stat_list_t *stparent );
static int Sort( const char **, const char ** );

/* Struct used to verify there are no recursive directory */
struct stat_list_t
{
    stat_list_t *parent;
    struct stat st;
};

struct preparsed_item_t
{
    monitoring_thread_t *p_mon;
    char* psz_uri;
    int i_dir_id;
    int i_mtime;
    int i_update_id;
    bool b_update;
};

/**
 * @brief Remove a directory to monitor
 * @param p_ml A media library object
 * @param psz_dir the directory to remove
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int RemoveDirToMonitor( media_library_t *p_ml, const char *psz_dir )
{
    assert( p_ml );

    char **pp_results = NULL;
    int i_cols = 0, i_rows = 0, i_ret = VLC_SUCCESS;
    int i;

    bool b_recursive = var_CreateGetBool( p_ml, "ml-recursive-scan" );

    if( b_recursive )
    {
        i_ret = Query( p_ml, &pp_results, &i_rows, &i_cols,
                          "SELECT media.id FROM media JOIN directories ON "
                          "(media.directory_id = directories.id) WHERE "
                          "directories.uri LIKE '%q%%'",
                          psz_dir );
        if( i_ret != VLC_SUCCESS )
        {
            msg_Err( p_ml, "Error occured while making a query to the database" );
            return i_ret;
        }
        QuerySimple( p_ml, "DELETE FROM directories WHERE uri LIKE '%q%%'",
                        psz_dir );
    }
    else
    {
        i_ret = Query( p_ml, &pp_results, &i_rows, &i_cols,
                          "SELECT media.id FROM media JOIN directories ON "
                          "(media.directory_id = directories.id) WHERE "
                          "directories.uri = %Q",
                          psz_dir );
        if( i_ret != VLC_SUCCESS )
        {
            msg_Err( p_ml, "Error occured while making a query to the database" );
            return i_ret;
        }
        QuerySimple( p_ml, "DELETE FROM directories WHERE uri = %Q",
                        psz_dir );
    }

    vlc_array_t *p_where = vlc_array_new();
    for( i = 1; i <= i_rows; i++ )
    {
        int id = atoi( pp_results[i*i_cols] );
        ml_element_t* p_find = ( ml_element_t * ) calloc( 1, sizeof( ml_element_t ) );
        p_find->criteria = ML_ID;
        p_find->value.i = id;
        vlc_array_append( p_where, p_find );
    }
    Delete( p_ml, p_where );

    FreeSQLResult( p_ml, pp_results );
    for( i = 0; i < vlc_array_count( p_where ); i++ )
    {
        free( vlc_array_item_at_index( p_where, i ) );
    }
    vlc_array_destroy( p_where );
    return VLC_SUCCESS;
}

/**
 * @brief Get the list of the monitored directories
 * @param p_ml A media library object
 * @param p_array An initialized array where the list will be put in
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int ListMonitoredDirs( media_library_t *p_ml, vlc_array_t *p_array )
{
    char **pp_results;
    int i_cols, i_rows;
    int i;

    if( Query( p_ml, &pp_results, &i_rows, &i_cols,
            "SELECT uri AS directory_uri FROM directories WHERE recursive=0" )
        != VLC_SUCCESS )
        return VLC_EGENERIC;

    for( i = 1; i <= i_rows; i++ )
    {
        vlc_array_append( p_array, strdup( pp_results[i] ) );
    }
    FreeSQLResult( p_ml, pp_results );

    return VLC_SUCCESS;
}

/**
 * @brief Add a directory to monitor
 * @param p_ml This media_library_t object
 * @param psz_dir the directory to add
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int AddDirToMonitor( media_library_t *p_ml, const char *psz_dir )
{
    assert( p_ml );

    /* Verify if we can open the directory */
    DIR *dir = vlc_opendir( psz_dir );
    if( !dir )
    {
        int err = errno;
        if( err != ENOTDIR )
            msg_Err( p_ml, "%s: %m", psz_dir );
        else
            msg_Dbg( p_ml, "`%s' is not a directory", psz_dir );
        errno = err;
        return VLC_EGENERIC;
    }

    closedir( dir );

    msg_Dbg( p_ml, "Adding directory `%s' to be monitored", psz_dir );
    QuerySimple( p_ml, "INSERT INTO directories ( uri, timestamp, "
                          "recursive ) VALUES( %Q, 0, 0 )", psz_dir );
    vlc_cond_signal( &p_ml->p_sys->p_mon->wait );
    return VLC_SUCCESS;
}


static int Sort( const char **a, const char **b )
{
#ifdef HAVE_STRCOLL
    return strcoll( *a, *b );
#else
    return strcmp( *a, *b );
#endif
}

/**
 * @brief Directory Monitoring thread loop
 */
void *RunMonitoringThread( void *p_this )
{
    monitoring_thread_t *p_mon = (monitoring_thread_t*) p_this;
    vlc_cond_init( &p_mon->wait );
    vlc_mutex_init( &p_mon->lock );

    var_Create( p_mon, "ml-recursive-scan", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    while( vlc_object_alive( p_mon ) )
    {
        vlc_mutex_lock( &p_mon->lock );

        /* Update */
        UpdateLibrary( p_mon );

        /* We wait MONITORING_DELAY seconds or wait that the media library
           signals us to do something */
        vlc_cond_timedwait( &p_mon->wait, &p_mon->lock,
                            mdate() + 1000000*MONITORING_DELAY );

        vlc_mutex_unlock( &p_mon->lock );
    }
    vlc_cond_destroy( &p_mon->wait );
    vlc_mutex_destroy( &p_mon->lock );
    return NULL;
}

/**
 * @brief Update library if new files found or updated
 */
static void UpdateLibrary( monitoring_thread_t *p_mon )
{
    int i_rows, i_cols, i;
    char **pp_results;
    media_library_t *p_ml = p_mon->p_ml;

    struct stat s_stat;

    bool b_recursive = var_GetBool( p_mon, "ml-recursive-scan" );

    msg_Dbg( p_mon, "Scanning directories" );

    Query( p_ml, &pp_results, &i_rows, &i_cols,
              "SELECT id AS directory_id, uri AS directory_uri, "
              "timestamp AS directory_ts FROM directories" );
    msg_Dbg( p_mon, "%d directories to scan", i_rows );

    for( i = 1; i <= i_rows; i++ )
    {
        int id = atoi( pp_results[i*i_cols] );
        char *psz_dir = pp_results[i*i_cols+1];
        int timestamp = atoi( pp_results[i*i_cols+2] );

        if( vlc_stat( psz_dir, &s_stat ) == -1 )
        {
            int err = errno;
            if( err == ENOTDIR || err == ENOENT )
            {
                msg_Dbg( p_mon, "Removing `%s'", psz_dir );
                RemoveDirToMonitor( p_ml, psz_dir );
            }
            else
            {
                msg_Err( p_mon, "%s: %m", psz_dir );
                FreeSQLResult( p_ml, pp_results );
                return;
            }
            errno = err;
        }

        if( !S_ISDIR( s_stat.st_mode ) )
        {
            msg_Dbg( p_mon, "Removing `%s'", psz_dir );
            RemoveDirToMonitor( p_ml, psz_dir );
        }

        if( timestamp < s_stat.st_mtime )
        {
            msg_Dbg( p_mon, "Adding `%s'", psz_dir );
            ScanFiles( p_mon, id, b_recursive, NULL );
        }
    }
    FreeSQLResult( p_ml, pp_results );
}

/**
 * @brief Callback for input item preparser to directory monitor
 */
static void PreparseComplete( const vlc_event_t * p_event, void *p_data )
{
    int i_ret = VLC_SUCCESS;
    preparsed_item_t* p_itemobject = (preparsed_item_t*) p_data;
    monitoring_thread_t *p_mon = p_itemobject->p_mon;
    media_library_t *p_ml = (media_library_t *)p_mon->p_ml;
    input_item_t *p_input = (input_item_t*) p_event->p_obj;

    if( input_item_IsPreparsed( p_input ) )
    {
        if( p_itemobject->b_update )
        {
            //TODO: Perhaps we don't have to load everything?
            ml_media_t* p_media = GetMedia( p_ml, p_itemobject->i_update_id,
                    ML_MEDIA_SPARSE, true );
            CopyInputItemToMedia( p_media, p_input );
            i_ret = UpdateMedia( p_ml, p_media );
            ml_gc_decref( p_media );
        }
        else
            i_ret = AddInputItem( p_ml, p_input );
    }

    if( i_ret != VLC_SUCCESS )
        msg_Dbg( p_mon, "Item could not be correctly added"
                " or updated during scan: %s", p_input->psz_uri );
    QuerySimple( p_ml, "UPDATE media SET directory_id=%d, timestamp=%d "
                          "WHERE id=%d",
                    p_itemobject->i_dir_id, p_itemobject->i_mtime,
                    GetMediaIdOfURI( p_ml, p_input->psz_uri ) );
    vlc_event_detach( &p_input->event_manager, vlc_InputItemPreparsedChanged,
                  PreparseComplete, p_itemobject );
    vlc_gc_decref( p_input );
    free( p_itemobject->psz_uri );
}

/**
 * @brief Scan files in a particular directory
 */
static void ScanFiles( monitoring_thread_t *p_mon, int i_dir_id,
                       bool b_recursive, stat_list_t *stparent )
{
    int i_rows, i_cols, i_dir_content, i, i_mon_rows, i_mon_cols;
    char **ppsz_monitored_files;
    char **pp_results, *psz_dir;
    char **pp_dir_content;
    bool *pb_processed;
    input_item_t *p_input;
    struct stat s_stat;
    media_library_t *p_ml = (media_library_t *)p_mon->p_ml;

    Query( p_ml, &pp_results, &i_rows, &i_cols,
              "SELECT uri AS directory_uri FROM directories WHERE id = '%d'",
              i_dir_id );
    if( i_rows < 1 )
    {
        msg_Dbg( p_mon, "query returned no directory for dir_id: %d (%s:%d)",
                 i_dir_id, __FILE__, __LINE__ );
        return;
    }
    psz_dir = strdup( pp_results[1] );
    FreeSQLResult( p_ml, pp_results );

    struct stat_list_t stself;

    if( vlc_stat( psz_dir, &stself.st ) == -1 )
    {
        msg_Err( p_ml, "Cannot stat `%s': %m", psz_dir );
        free( psz_dir );
        return;
    }
#ifndef WIN32
    for( stat_list_t *stats = stparent; stats != NULL; stats = stats->parent )
    {
        if( ( stself.st.st_ino == stats->st.st_ino ) &&
            ( stself.st.st_dev == stats->st.st_dev ) )
        {
            msg_Warn( p_ml, "Ignoring infinitely recursive directory `%s'",
                      psz_dir );
            free( psz_dir );
            return;
        }
    }
#else
    /* Windows has st_dev (driver letter - 'A'), but it zeroes st_ino,
     * so that the test above will always incorrectly succeed.
     * Besides, Windows does not have dirfd(). */
#endif
    stself.parent = stparent;

    QuerySimple( p_ml, "UPDATE directories SET timestamp=%d WHERE id = %d",
                    stself.st.st_mtime, i_dir_id );
    Query( p_ml, &ppsz_monitored_files, &i_mon_rows, &i_mon_cols,
              "SELECT id AS media_id, timestamp AS media_ts, uri AS media_uri "
              "FROM media WHERE directory_id = %d",
              i_dir_id );
    pb_processed = malloc(sizeof(bool) * i_mon_rows);
    for( i = 0; i < i_mon_rows ; i++)
        pb_processed[i] = false;

    i_dir_content = vlc_scandir( psz_dir, &pp_dir_content, NULL, Sort );
    if( i_dir_content == -1 )
    {
        msg_Err( p_mon, "Cannot read `%s': %m", psz_dir );
        free( pb_processed );
        free( psz_dir );
        return;
    }
    else if( i_dir_content == 0 )
    {
        msg_Dbg( p_mon, "Nothing in directory `%s'", psz_dir );
        free( pb_processed );
        free( psz_dir );
        return;
    }

    for( i = 0; i < i_dir_content; i++ )
    {
        const char *psz_entry = pp_dir_content[i];

        if( psz_entry[0] != '.' )
        {
            /* 7 is the size of "file://" */
            char psz_uri[strlen(psz_dir) + strlen(psz_entry) + 2 + 7];
            sprintf( psz_uri, "%s/%s", psz_dir, psz_entry );

            if( vlc_stat( psz_uri, &s_stat ) == -1 )
            {
                msg_Err( p_mon, "%s: %m", psz_uri );
                free( pb_processed );
                free( psz_dir );
                return;
            }

            if( S_ISREG( s_stat.st_mode ) )
            {
                const char *psz_dot = strrchr( psz_uri, '.' );
                if( psz_dot++ && *psz_dot )
                {
                    int i_is_media = 0;
                    for( int a = 0; ppsz_MediaExtensions[a]; a++ )
                    {
                        if( !strcasecmp( psz_dot, ppsz_MediaExtensions[a] ) )
                        {
                            i_is_media = 1;
                            break;
                        }
                    }
                    if( !i_is_media )
                    {
                        msg_Dbg( p_mon, "ignoring file %s", psz_uri );
                        continue;
                    }
                }

                char * psz_tmp = encode_URI_component( psz_uri );
                char * psz_encoded_uri = ( char * )calloc( strlen( psz_tmp ) + 9, 1 );
                strcpy( psz_encoded_uri, "file:///" );
                strcat( psz_encoded_uri, psz_tmp );
                free( psz_tmp );

                /* Check if given media is already in DB and it has been updated */
                bool b_skip = false;
                bool b_update = false;
                int j = 1;
                for( j = 1; j <= i_mon_rows; j++ )
                {
                    if( strcasecmp( ppsz_monitored_files[ j * i_mon_cols + 2 ],
                                    psz_encoded_uri ) != 0 )
                        continue;
                    b_update = true;
                    pb_processed[ j - 1 ] = true;
                    if( atoi( ppsz_monitored_files[ j * i_mon_cols + 1 ] )
                        < s_stat.st_mtime )
                    {
                        b_skip = false;
                        break;
                    }
                    else
                    {
                        b_skip = true;
                        break;
                    }
                }
                msg_Dbg( p_ml , "Checking if %s is in DB. Found: %d", psz_encoded_uri,
                         b_skip? 1 : 0 );
                if( b_skip )
                    continue;

                p_input = input_item_New( psz_encoded_uri, psz_entry );

                playlist_t* p_pl = pl_Get( p_mon );
                preparsed_item_t* p_itemobject;
                p_itemobject = malloc( sizeof( preparsed_item_t ) );
                p_itemobject->i_dir_id = i_dir_id;
                p_itemobject->psz_uri = psz_encoded_uri;
                p_itemobject->i_mtime = s_stat.st_mtime;
                p_itemobject->p_mon = p_mon;
                p_itemobject->b_update = b_update;
                p_itemobject->i_update_id = b_update ?
                    atoi( ppsz_monitored_files[ j * i_mon_cols + 0 ] ) : 0 ;

                vlc_event_manager_t *p_em = &p_input->event_manager;
                vlc_event_attach( p_em, vlc_InputItemPreparsedChanged,
                      PreparseComplete, p_itemobject );
                playlist_PreparseEnqueue( p_pl, p_input );
            }
            else if( S_ISDIR( s_stat.st_mode ) && b_recursive )
            {
                Query( p_ml, &pp_results, &i_rows, &i_cols,
                        "SELECT id AS directory_id FROM directories "
                        "WHERE uri=%Q", psz_uri );
                FreeSQLResult( p_ml, pp_results );

                if( i_rows <= 0 )
                {
                    msg_Dbg( p_mon, "New directory `%s' in dir of id %d",
                             psz_uri, i_dir_id );
                    QuerySimple( p_ml,
                                    "INSERT INTO directories (uri, timestamp, "
                                    "recursive) VALUES(%Q, 0, 1)", psz_uri );

                    // We get the id of the directory we've just added
                    Query( p_ml, &pp_results, &i_rows, &i_cols,
                    "SELECT id AS directory_id FROM directories WHERE uri=%Q",
                              psz_uri );
                    if( i_rows <= 0 )
                    {
                        msg_Err( p_mon, "Directory `%s' was not sucessfully"
                                " added to the database", psz_uri );
                        FreeSQLResult( p_ml, pp_results );
                        continue;
                    }

                    ScanFiles( p_mon, atoi( pp_results[1] ), b_recursive,
                               &stself );
                    FreeSQLResult( p_ml, pp_results );
                }
            }
        }
    }

    vlc_array_t* delete_ids = vlc_array_new();
    for( i = 0; i < i_mon_rows; i++ )
    {
       if( !pb_processed[i] )
        {
            /* This file doesn't exist anymore. Let's...urm...delete it. */
            ml_element_t* find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) );
            find->criteria = ML_ID;
            find->value.i = atoi( ppsz_monitored_files[ (i + 1) * i_mon_cols ] );
            vlc_array_append( delete_ids, find );
       }
    }

    /* Delete the unfound media */
    if( Delete( p_ml, delete_ids ) != VLC_SUCCESS )
        msg_Dbg( p_ml, "Something went wrong in multi delete" );

    for( i = 0; i < vlc_array_count( delete_ids ); i++ )
    {
       free( vlc_array_item_at_index( delete_ids, i ) );
    }
    vlc_array_destroy( delete_ids );

    FreeSQLResult( p_ml, ppsz_monitored_files );
    for( i = 0; i < i_dir_content; i++ )
        free( pp_dir_content[i] );
    free( pp_dir_content );
    free( psz_dir );
    free( pb_processed );
}
