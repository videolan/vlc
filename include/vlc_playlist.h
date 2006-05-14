/*****************************************************************************
 * vlc_playlist.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#ifndef _VLC_PLAYLIST_H_
#define _VLC_PLAYLIST_H_

/**
 *  \file
 *  This file contain structures and function prototypes related
 *  to the playlist in vlc
 */

/**
 * \defgroup vlc_playlist Playlist
 * Brief description. Longer description
 * @{
 */

/**
 * playlist export helper structure
 */
struct playlist_export_t
{
    char *psz_filename;
    FILE *p_file;
    playlist_item_t *p_root;
};

/**
 * playlist item / node
 * \see playlist_t
 */
struct playlist_item_t
{
    input_item_t           *p_input;    /**< input item descriptor */

    /* Tree specific fields */
    int                    i_children;  /**< Number of children
                                             -1 if not a node */
    playlist_item_t      **pp_children; /**< Children nodes/items */
    playlist_item_t       *p_parent;    /**< Item parent */

    int                    i_id;        /**< Playlist item specific id */

    uint8_t                i_flags;     /**< Flags */
};

#define PLAYLIST_SAVE_FLAG      0x01    /**< Must it be saved */
#define PLAYLIST_SKIP_FLAG      0x02    /**< Must playlist skip after it ? */
#define PLAYLIST_DBL_FLAG       0x04    /**< Is it disabled ? */
#define PLAYLIST_RO_FLAG        0x10    /**< Write-enabled ? */
#define PLAYLIST_REMOVE_FLAG    0x20    /**< Remove this item at the end */
#define PLAYLIST_EXPANDED_FLAG  0x40    /**< Expanded node */

/**
 * Playlist status
 */
typedef enum
{ PLAYLIST_STOPPED,PLAYLIST_RUNNING,PLAYLIST_PAUSED } playlist_status_t;


struct services_discovery_t
{
    VLC_COMMON_MEMBERS
    char *psz_module;

    module_t *p_module;

    services_discovery_sys_t *p_sys;
    void (*pf_run) ( services_discovery_t *);
};

struct playlist_preparse_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_waiting;
    input_item_t  **pp_waiting;
};


/**
 * Structure containing information about the playlist
 */
struct playlist_t
{
    VLC_COMMON_MEMBERS
/**
   \name playlist_t
   These members are uniq to playlist_t
*/
/*@{*/
    int                   i_enabled; /**< How many items are enabled ? */

    int                   i_size;   /**< total size of the list */
    playlist_item_t **    pp_items; /**< array of pointers to the
                                     * playlist items */
    int                   i_all_size; /**< size of list of items and nodes */
    playlist_item_t **    pp_all_items; /**< array of pointers to the
                                         * playlist items and nodes */
    int                   i_last_playlist_id; /**< Last id to an item */

    int                   i_input_items;
    input_item_t **    pp_input_items;

    int                  i_last_input_id ;

    input_thread_t *      p_input;  /**< the input thread associated
                                     * with the current item */

    int                   i_sort; /**< Last sorting applied to the playlist */
    int                   i_order; /**< Last ordering applied to the playlist */

    playlist_item_t *     p_root_category;
    playlist_item_t *     p_root_onelevel;

    playlist_item_t *     p_local_category; /** < "Playlist" in CATEGORY view */
    playlist_item_t *     p_ml_category; /** < "Library" in CATEGORY view */
    playlist_item_t *     p_local_onelevel; /** < "Playlist" in ONELEVEL view */
    playlist_item_t *     p_ml_onelevel; /** < "Library" in ONELEVEL iew */

    services_discovery_t **pp_sds;
    int                   i_sds;

    struct {
        /* Current status. These fields are readonly, only the playlist
         * main loop can touch it*/
        playlist_status_t   i_status;  /**< Current status of playlist */
        playlist_item_t *   p_item; /**< Currently playing/active item */
        playlist_item_t *   p_node; /**< Current node to play from */
    } status;

    struct {
        /* Request. Use this to give orders to the playlist main loop  */
        int                 i_status; /**< requested playlist status */
        playlist_item_t *   p_node;   /**< requested node to play from */
        playlist_item_t *   p_item;   /**< requested item to play in the node */

        int                 i_skip;   /**< Number of items to skip */

        vlc_bool_t          b_request;/**< Set to true by the requester
                                           The playlist sets it back to false
                                           when processing the request */
        vlc_mutex_t         lock;     /**< Lock to protect request */
    } request;

    playlist_preparse_t     *p_preparse; /**< Preparser object */

    vlc_mutex_t gc_lock;         /**< Lock to protect the garbage collection */

    // Playlist-unrelated fields
    interaction_t *p_interaction;       /**< Interaction manager */
    global_stats_t *p_stats;            /**< Global statistics */

    /*@}*/
};

/* Helper to add an item */
struct playlist_add_t
{
    int i_node;
    int i_item;
    int i_view;
    int i_position;
};

#define SORT_ID 0
#define SORT_TITLE 1
#define SORT_TITLE_NODES_FIRST 2
#define SORT_AUTHOR 3
#define SORT_GENRE 4
#define SORT_RANDOM 5
#define SORT_DURATION 6
#define SORT_TITLE_NUMERIC 7
#define SORT_ALBUM 8

#define ORDER_NORMAL 0
#define ORDER_REVERSE 1

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Global thread */
#define playlist_ThreadCreate(a) __playlist_ThreadCreate(VLC_OBJECT(a))
playlist_t *__playlist_ThreadCreate   ( vlc_object_t * );
int           playlist_ThreadDestroy  ( playlist_t * );

/* Helpers */
#define PL_LOCK vlc_mutex_lock( &p_playlist->object_lock );
#define PL_UNLOCK vlc_mutex_unlock( &p_playlist->object_lock );

/* Creation/Deletion */
playlist_t *playlist_Create   ( vlc_object_t * );
void        playlist_Destroy  ( playlist_t * );

/* Engine */
void playlist_MainLoop( playlist_t * );
void playlist_LastLoop( playlist_t * );
void playlist_PreparseLoop( playlist_preparse_t * );

/* Control */
playlist_item_t * playlist_NextItem  ( playlist_t * );
int playlist_PlayItem  ( playlist_t *, playlist_item_t * );

/* Playlist control */
#define playlist_Play(p) playlist_LockControl(p,PLAYLIST_PLAY )
#define playlist_Pause(p) playlist_LockControl(p,PLAYLIST_PAUSE )
#define playlist_Stop(p) playlist_LockControl(p,PLAYLIST_STOP )
#define playlist_Next(p) playlist_LockControl(p,PLAYLIST_SKIP, 1)
#define playlist_Prev(p) playlist_LockControl(p,PLAYLIST_SKIP, -1)
#define playlist_Skip(p,i) playlist_LockControl(p,PLAYLIST_SKIP, i)

VLC_EXPORT( int, playlist_Control, ( playlist_t *, int, ...  ) );
VLC_EXPORT( int, playlist_LockControl, ( playlist_t *, int, ...  ) );

VLC_EXPORT( void,  playlist_Clear, ( playlist_t * ) );
VLC_EXPORT( void,  playlist_LockClear, ( playlist_t * ) );

VLC_EXPORT( int, playlist_PreparseEnqueue, (playlist_t *, input_item_t *) );
VLC_EXPORT( int, playlist_PreparseEnqueueItem, (playlist_t *, playlist_item_t *) );

/* Services discovery */

VLC_EXPORT( int, playlist_ServicesDiscoveryAdd, (playlist_t *, const char *));
VLC_EXPORT( int, playlist_ServicesDiscoveryRemove, (playlist_t *, const char *));
VLC_EXPORT( int, playlist_AddSDModules, (playlist_t *, char *));
VLC_EXPORT( vlc_bool_t, playlist_IsServicesDiscoveryLoaded, ( playlist_t *,const char *));


/* Item management functions (act on items) */
#define playlist_ItemNew( a , b, c ) __playlist_ItemNew(VLC_OBJECT(a) , b , c )
#define playlist_ItemCopy( a, b ) __playlist_ItemCopy(VLC_OBJECT(a), b )
VLC_EXPORT( playlist_item_t* , __playlist_ItemNew, ( vlc_object_t *,const char *,const char * ) );
VLC_EXPORT( playlist_item_t* , __playlist_ItemCopy, ( vlc_object_t *,playlist_item_t* ) );
VLC_EXPORT( playlist_item_t* , playlist_ItemNewWithType, ( vlc_object_t *,const char *,const char *, int , const char **, int, int) );
#define playlist_ItemNewFromInput(a,b) __playlist_ItemNewFromInput(VLC_OBJECT(a),b)
VLC_EXPORT( playlist_item_t *, __playlist_ItemNewFromInput, ( vlc_object_t *p_obj,input_item_t *p_input ) );


VLC_EXPORT( int, playlist_ItemDelete, ( playlist_item_t * ) );

/* Item informations accessors */
VLC_EXPORT( int, playlist_ItemSetName, (playlist_item_t *,  char * ) );
VLC_EXPORT( int, playlist_ItemSetDuration, (playlist_item_t *, mtime_t ) );

VLC_EXPORT( void, playlist_ItemAddOption, (playlist_item_t *, const char *) );

VLC_EXPORT(void, playlist_NodeDump, ( playlist_t *p_playlist, playlist_item_t *p_item, int i_level ) );

/** Counts the items of a view */
VLC_EXPORT( int, playlist_NodeChildrenCount, (playlist_t *,playlist_item_t* ) );

/* Node management */
VLC_EXPORT( playlist_item_t *, playlist_NodeCreate, ( playlist_t *, char *, playlist_item_t * p_parent ) );
VLC_EXPORT( int, playlist_NodeAppend, (playlist_t *,playlist_item_t*,playlist_item_t *) );
VLC_EXPORT( int, playlist_NodeInsert, (playlist_t *,playlist_item_t*,playlist_item_t *, int) );
VLC_EXPORT( int, playlist_NodeRemoveItem, (playlist_t *,playlist_item_t*,playlist_item_t *) );
VLC_EXPORT( playlist_item_t *, playlist_ChildSearchName, (playlist_item_t*, const char* ) );
VLC_EXPORT( int, playlist_NodeDelete, ( playlist_t *, playlist_item_t *, vlc_bool_t , vlc_bool_t ) );
VLC_EXPORT( int, playlist_NodeEmpty, ( playlist_t *, playlist_item_t *, vlc_bool_t ) );

/* Tree walking - These functions are only for playlist, not plugins */
playlist_item_t *playlist_GetNextLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item );
playlist_item_t *playlist_GetNextEnabledLeaf( playlist_t *p_playlist,
                                              playlist_item_t *p_root,
                                              playlist_item_t *p_item );
playlist_item_t *playlist_GetPrevLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item );
playlist_item_t *playlist_GetLastLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root );


/* Simple add/remove functions */
VLC_EXPORT( int,  playlist_PlaylistAdd,    ( playlist_t *, const char *, const char *, int, int ) );
VLC_EXPORT( int,  playlist_PlaylistAddExt, ( playlist_t *, const char *, const char *, int, int, mtime_t, const char **,int ) );
VLC_EXPORT( int, playlist_PlaylistAddInput, ( playlist_t *, input_item_t *,int , int ) );
VLC_EXPORT( playlist_item_t *, playlist_NodeAddInput, ( playlist_t *, input_item_t *,playlist_item_t *,int , int ) );
VLC_EXPORT( void, playlist_NodeAddItem, ( playlist_t *, playlist_item_t *, playlist_item_t *,int , int ) );
VLC_EXPORT( int, playlist_BothAddInput, ( playlist_t *, input_item_t *,playlist_item_t *,int , int ) );
VLC_EXPORT( void, playlist_AddWhereverNeeded, (playlist_t* , input_item_t*, playlist_item_t*,playlist_item_t*,vlc_bool_t, int ) );

void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id, int i_node_id );

/* Misc item operations (act on item+playlist) */
VLC_EXPORT( int,  playlist_DeleteAllFromInput, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_DeleteFromInput, ( playlist_t *, int, playlist_item_t *, vlc_bool_t ) );
VLC_EXPORT( int,  playlist_DeleteFromItemId, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_LockDelete, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_LockDeleteAllFromInput, ( playlist_t *, int ) );
VLC_EXPORT( playlist_item_t*, playlist_ItemToNode, (playlist_t *,playlist_item_t *) );
VLC_EXPORT( playlist_item_t*, playlist_LockItemToNode, (playlist_t *,playlist_item_t *) );

playlist_item_t *playlist_ItemFindFromInputAndRoot( playlist_t *p_playlist,
                                   int i_input_id, playlist_item_t *p_root );

/* Item search functions */
VLC_EXPORT( playlist_item_t *, playlist_ItemGetById, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_ItemGetByInput, (playlist_t *,input_item_t * ) );

static inline playlist_item_t *playlist_LockItemGetById( playlist_t *p_playlist,
                                                         int i_id)
{
    playlist_item_t *p_ret;
    vlc_mutex_lock( &p_playlist->object_lock );
    p_ret = playlist_ItemGetById( p_playlist, i_id );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return p_ret;
}

static inline playlist_item_t *playlist_LockItemGetByInput(
                                playlist_t *p_playlist, input_item_t *p_item )
{
    playlist_item_t *p_ret;
    vlc_mutex_lock( &p_playlist->object_lock );
    p_ret = playlist_ItemGetByInput( p_playlist, p_item );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return p_ret;
}

VLC_EXPORT( int, playlist_LiveSearchUpdate, (playlist_t *, playlist_item_t *, const char *) );

/* Playlist sorting */
#define playlist_SortID(p, i) playlist_Sort( p, SORT_ID, i)
#define playlist_SortTitle(p, i) playlist_Sort( p, SORT_TITLE, i)
#define playlist_SortAuthor(p, i) playlist_Sort( p, SORT_AUTHOR, i)
#define playlist_SortAlbum(p, i) playlist_Sort( p, SORT_ALBUM, i)
#define playlist_SortGroup(p, i) playlist_Sort( p, SORT_GROUP, i)
VLC_EXPORT( int,  playlist_TreeMove, ( playlist_t *, playlist_item_t *, playlist_item_t *, int ) );
VLC_EXPORT( int,  playlist_NodeGroup, ( playlist_t *, playlist_item_t *,playlist_item_t **,int, int, int ) );
VLC_EXPORT( int,  playlist_NodeSort, ( playlist_t *, playlist_item_t *,int, int ) );
VLC_EXPORT( int,  playlist_RecursiveNodeSort, ( playlist_t *, playlist_item_t *,int, int ) );

/* Load/Save */
VLC_EXPORT( int,  playlist_Import, ( playlist_t *, const char * ) );
VLC_EXPORT( int,  playlist_Export, ( playlist_t *, const char *, playlist_item_t *, const char * ) );

/***********************************************************************
 * Inline functions
 ***********************************************************************/

/** Tell if the playlist is currently running */
static inline vlc_bool_t playlist_IsPlaying( playlist_t * p_playlist )
{
    vlc_bool_t b_playing;
    vlc_mutex_lock( &p_playlist->object_lock );
    b_playing = p_playlist->status.i_status == PLAYLIST_RUNNING;
    vlc_mutex_unlock( &p_playlist->object_lock );
    return( b_playing );
}

/** Tell if the playlist is empty */
static inline vlc_bool_t playlist_IsEmpty( playlist_t * p_playlist )
{
    vlc_bool_t b_empty;
    vlc_mutex_lock( &p_playlist->object_lock );
    b_empty = p_playlist->i_size == 0;
    vlc_mutex_unlock( &p_playlist->object_lock );
    return( b_empty );
}

/**
 * @}
 */

#endif
