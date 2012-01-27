/*****************************************************************************
 * sql_media_library.h : Media Library Interface
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

#ifndef SQL_MEDIA_LIBRARY_H
#define SQL_MEDIA_LIBRARY_H

#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include <vlc_common.h>
#include <vlc_sql.h>
#include <vlc_media_library.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_arrays.h>
#include <vlc_charset.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_modules.h>

#include "item_list.h"

/*****************************************************************************
 * Static parameters
 *****************************************************************************/
#define THREAD_SLEEP_DELAY   2  /* Time between two calls to item_list_loop */
#define MONITORING_DELAY    30  /* Media library updates interval */
#define ITEM_LOOP_UPDATE     1  /* An item is updated after 1 loop */
#define ITEM_LOOP_MAX_AGE   10  /* An item is deleted after 10 loops */
#define ML_DBVERSION         1  /* The current version of the database */
#define ML_MEDIAPOOL_HASH_LENGTH 100 /* The length of the media pool hash */

/*****************************************************************************
 * Structures and types definitions
 *****************************************************************************/
typedef struct monitoring_thread_t monitoring_thread_t;
typedef struct ml_poolobject_t     ml_poolobject_t;

struct ml_poolobject_t
{
    ml_media_t* p_media;
    ml_poolobject_t* p_next;
};

struct media_library_sys_t
{
    /* Lock on the ML object */
    vlc_mutex_t lock;

    /* SQL object */
    sql_t *p_sql;

    /* Monitoring thread */
    monitoring_thread_t *p_mon;

    /* Watch thread */
    watch_thread_t *p_watch;

    /* Holds all medias */
    DECL_ARRAY( ml_media_t* ) mediapool;
    ml_poolobject_t* p_mediapool[ ML_MEDIAPOOL_HASH_LENGTH ];
    vlc_mutex_t pool_mutex;

    /* Info on update/collection rebuilding */
    bool b_updating;
    bool b_rebuilding;
};

/* Directory Monitoring thread */
struct monitoring_thread_t
{
    VLC_COMMON_MEMBERS;

    vlc_cond_t wait;
    vlc_mutex_t lock;
    vlc_thread_t thread;
    media_library_t *p_ml;
};

/* Media status Watching thread */
struct watch_thread_t
{
    media_library_t *p_ml;
    vlc_thread_t thread;
    vlc_cond_t cond;
    vlc_mutex_t lock;

    /* Input items watched */
    struct item_list_t* p_hlist[ ML_ITEMLIST_HASH_LENGTH ];
    vlc_mutex_t list_mutex;

    /* List of items to check */
    input_item_t** item_append_queue;
    vlc_mutex_t item_append_queue_lock;
    int item_append_queue_count;
};



/*****************************************************************************
 * Function headers
 *****************************************************************************/
/* General functions */
int CreateEmptyDatabase( media_library_t *p_ml );
int InitDatabase( media_library_t *p_ml );

/* Module Control */
int Control( media_library_t *p_ml,
             int i_query,
             va_list args );

/* Add functions */
int AddMedia( media_library_t *p_ml,
              ml_media_t *p_media );
int AddAlbum( media_library_t *p_ml, const char *psz_title,
              const char *psz_cover, const int i_album_artist );
int AddPeople( media_library_t *p_ml,
               const char *psz_name,
               const char *psz_role );
int AddPlaylistItem( media_library_t *p_ml,
                     playlist_item_t *p_playlist_item );
int AddInputItem( media_library_t *p_ml,
                  input_item_t *p_input );

/* Create and Copy functions */
ml_media_t* GetMedia( media_library_t* p_ml, int id,
                        ml_select_e select, bool reload );
input_item_t* GetInputItemFromMedia( media_library_t *p_ml,
                                     int i_media );
void CopyInputItemToMedia( ml_media_t *p_media,
                           input_item_t *p_item );
void CopyMediaToInputItem( input_item_t *p_item,
                           ml_media_t *p_media );

/* Get functions */
int GetDatabaseVersion( media_library_t *p_ml );
int GetMediaIdOfInputItem( media_library_t *p_ml,
                           input_item_t *p_item );
int GetMediaIdOfURI( media_library_t *p_ml,
                     const char *psz_uri );

/* Search in the database */
int BuildSelectVa( media_library_t *p_ml,
                   char **ppsz_query,
                   ml_result_type_e *p_result_type,
                   va_list criterias );
int BuildSelect( media_library_t *p_ml,
                 char **ppsz_query,
                 ml_result_type_e *p_result_type,
                 const char *psz_selected_type_lvalue,
                 ml_select_e selected_type,
                 ml_ftree_t *tree );
int Find( media_library_t *p_ml,
          vlc_array_t *results,
          ... );
int FindVa( media_library_t *p_ml,
            vlc_array_t *results,
            va_list criterias );
int FindAdv( media_library_t *p_ml,
             vlc_array_t *results,
             ml_select_e selected_type,
             const char* psz_lvalue,
             ml_ftree_t *tree );

/* Update the database */
int Update( media_library_t *p_ml,
            ml_select_e selected_type,
            const char* psz_lvalue,
            ml_ftree_t *where,
            vlc_array_t *changes );
int BuildUpdate( media_library_t *p_ml,
                 char **ppsz_query,
                 char **ppsz_id_query,
                 const char *psz_lvalue,
                 ml_select_e selected_type,
                 ml_ftree_t* where,
                 vlc_array_t *changes );
int UpdateMedia( media_library_t *p_ml,
                 ml_media_t *p_media );
int SetArtCover( media_library_t *p_ml,
                 int i_album_id,
                 const char *psz_cover );

/* Delete medias in the database */
int Delete( media_library_t *p_ml, vlc_array_t *p_array );

/* Do some query on the database */
int QuerySimple( media_library_t *p_ml,
                 const char *psz_fmt, ... );
int Query( media_library_t *p_ml,
           char ***ppp_res,
           int *pi_rows,
           int *pi_cols,
           const char *psz_fmt,
           ... );
int QueryVa( media_library_t *p_ml,
             char ***ppp_res,
             int *pi_rows,
             int *pi_cols,
             const char *psz_fmt,
             va_list args );
int QuerySimpleVa( media_library_t *p_ml,
                   const char *psz_fmt,
                   va_list argp );

/* Convert SQL results to ML results */
int StringToResult( ml_result_t *res,
                    const char *psz,
                    const char *psz_id,
                    ml_result_type_e result_type );
int SQLToMediaArray( media_library_t *p_ml,
                     vlc_array_t *p_result_array,
                     char **pp_results,
                     int i_rows,
                     int i_cols );
int SQLToResultArray( media_library_t *p_ml,
                      vlc_array_t *p_result_array,
                      char **pp_results,
                      int i_rows,
                      int i_cols,
                      ml_result_type_e result_type );

/* Database locking functions */

/**
 * @brief Begin a transaction
 * @param p_ml The Media Library object
 * @return VLC_SUCCESS and VLC_EGENERIC
 * @note This creates a SHARED lock in SQLITE. All queries made between
 * a Begin and Commit/Rollback will be transactional.
 */
static inline int Begin( media_library_t* p_ml )
{
    return sql_BeginTransaction( p_ml->p_sys->p_sql );
}

/**
 * @brief Commits the transaction
 * @param p_ml The Media Library object
 */
static inline void Commit( media_library_t* p_ml )
{
    sql_CommitTransaction( p_ml->p_sys->p_sql );
}

/**
 * @brief Rollback the transaction
 * @param p_ml The Media Library Object
 */
static inline void Rollback( media_library_t* p_ml )
{
    sql_RollbackTransaction( p_ml->p_sys->p_sql );
}

/****************************************************************************
 * Scanning/monitoring functions
 *****************************************************************************/
void *RunMonitoringThread( void *p_mon );
int AddDirToMonitor( media_library_t *p_ml,
                     const char *psz_dir );
int ListMonitoredDirs( media_library_t *p_ml,
                       vlc_array_t *p_array );
int RemoveDirToMonitor( media_library_t *p_ml,
                        const char *psz_dir );


/*****************************************************************************
 * Media pool functions
 *****************************************************************************/
ml_media_t* pool_GetMedia( media_library_t* p_ml, int media_id );
int pool_InsertMedia( media_library_t* p_ml, ml_media_t* media, bool locked );
void pool_GC( media_library_t* p_ml );

/*****************************************************************************
 * Items watching system
 *****************************************************************************/
/* Watching thread */
#define watch_add_Item( a, b, c ) __watch_add_Item( a, b, c, false )

int watch_Init( media_library_t *p_ml );
void watch_Close( media_library_t *p_ml );
int __watch_add_Item( media_library_t *p_ml, input_item_t *p_item,
                            ml_media_t* p_media, bool locked );

#define watch_del_Item( a, b ) __watch_del_Item( a, b, false )
int __watch_del_Item( media_library_t *p_ml, input_item_t *p_item, bool locked );
int watch_del_MediaById( media_library_t* p_ml, int i_media_id );
input_item_t* watch_get_itemOfMediaId( media_library_t *p_ml, int i_media_id );
ml_media_t* watch_get_mediaOfMediaId( media_library_t* p_ml, int i_media_id );
int watch_get_mediaIdOfItem( media_library_t *p_ml, input_item_t *p_item );
void watch_Force_Update( media_library_t* p_ml );

/*****************************************************************************
 * Free result of ml_Query
 *****************************************************************************/
static inline void FreeSQLResult( media_library_t *p_ml, char **ppsz_result )
{
    if( ppsz_result )
    {
        sql_Free( p_ml->p_sys->p_sql, ppsz_result );
    }
}

#endif /* SQL_MEDIA_LIBRARY_H */
