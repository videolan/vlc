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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

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
};

struct item_parent_t
{
    int i_view;
    playlist_item_t *p_parent;
};

/**
 * playlist item / node
 * \see playlist_t
 */
struct playlist_item_t
{
    input_item_t           input;       /**< input item descriptor */

    /* Tree specific fields */
    int                    i_children;  /**< Number of children
                                             -1 if not a node */
    playlist_item_t      **pp_children; /**< Children nodes/items */
    int                    i_parents;   /**< Number of parents */
    struct item_parent_t **pp_parents;  /**< Parents */
    int                    i_serial;    /**< Has this node been updated ? */

    uint8_t                i_flags;     /**< Flags */


    int        i_nb_played;       /**< How many times was this item played ? */

    /* LEGACY FIELDS */
    vlc_bool_t b_autodeletion;    /**< Indicates whther this item is to
                                   * be deleted after playback. True mean
                                   * that this item is to be deleted
                                   * after playback, false otherwise */
    vlc_bool_t b_enabled;         /**< Indicates whether this item is to be
                                   * played or skipped */
    /* END LEGACY FIELDS */
};

#define PLAYLIST_SAVE_FLAG      0x01     /**< Must it be saved */
#define PLAYLIST_SKIP_FLAG      0x02     /**< Must playlist skip after it ? */
#define PLAYLIST_ENA_FLAG       0x04     /**< Is it enabled ? */
#define PLAYLIST_DEL_FLAG       0x08     /**< Autodelete ? */
#define PLAYLIST_RO_FLAG        0x10    /**< Write-enabled ? */
#define PLAYLIST_REMOVE_FLAG    0x20    /**< Remove this item at the end */

/**
 * playlist view
 * \see playlist_t
*/
struct playlist_view_t
{
    char            *   psz_name;        /**< View name */
    int                 i_id;            /**< Identifier for the view */
    playlist_item_t *   p_root;          /**< Root node */
};


/**
 * predefined views
 *
 */
#define VIEW_CATEGORY 1
#define VIEW_SIMPLE   2
#define VIEW_ALL      3
#define VIEW_FIRST_SORTED  4
#define VIEW_S_AUTHOR 4
#define VIEW_S_GENRE 5
#define VIEW_S_ALBUM  6

#define VIEW_LAST_SORTED  10

#define VIEW_FIRST_CUSTOM 100

/**
 * Playlist status
 */
typedef enum { PLAYLIST_STOPPED,PLAYLIST_RUNNING,PLAYLIST_PAUSED } playlist_status_t;


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
    int                   i_index;  /**< current index into the playlist */
    int                   i_enabled; /**< How many items are enabled ? */

    int                   i_size;   /**< total size of the list */
    playlist_item_t **    pp_items; /**< array of pointers to the
                                     * playlist items */
    int                   i_all_size; /**< size of list of items and nodes */
    playlist_item_t **    pp_all_items; /**< array of pointers to the
                                         * playlist items and nodes */

    int                   i_views; /**< Number of views */
    playlist_view_t **    pp_views; /**< array of pointers to the
                                     * playlist views */

    input_thread_t *      p_input;  /**< the input thread ascosiated
                                     * with the current item */

    mtime_t               request_date; /**< Used for profiling */

    int                   i_last_id; /**< Last id to an item */
    int                   i_sort; /**< Last sorting applied to the playlist */
    int                   i_order; /**< Last ordering applied to the playlist */

    playlist_item_t *    p_general; /**< Keep a pointer on the "general"
                                        category */

    services_discovery_t **pp_sds;
    int                   i_sds;

    vlc_bool_t          b_go_next; /*< Go further than the parent node ? */

    struct {
        /* Current status */
        playlist_status_t   i_status;  /**< Current status of playlist */

        /* R/O fields, don't touch if you aren't the playlist thread */
        /* Use a request */
        playlist_item_t *   p_item; /**< Currently playing/active item */
        playlist_item_t *   p_node;   /**< Current node to play from */
        int                 i_view;    /**< Current view */
    } status;

    struct {
        /* Request */
        /* Playlist thread uses this info to calculate the next position */
        int                 i_view;   /**< requested view id */
        playlist_item_t *   p_node;   /**< requested node to play from */
        playlist_item_t *   p_item;   /**< requested item to play in the node */

        int                 i_skip;   /**< Number of items to skip */
        int                 i_goto;   /**< Direct index to go to (non-view)*/

        vlc_bool_t          b_request; /**< Set to true by the requester
                                            The playlist sets it back to false
                                            when processing the request */
        vlc_mutex_t         lock;      /**< Lock to protect request */
    } request;

    playlist_preparse_t     *p_preparse;

    vlc_mutex_t gc_lock;         /**< Lock to protect the garbage collection */

    // The following members are about user interaction
    // The playlist manages the user interaction to avoid creating another
    // thread
    interaction_t *p_interaction;

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

/* Creation/Deletion */
#define playlist_Create(a) __playlist_Create(VLC_OBJECT(a))
playlist_t * __playlist_Create   ( vlc_object_t * );
int            playlist_Destroy  ( playlist_t * );

/* Playlist control */
#define playlist_Play(p) playlist_LockControl(p,PLAYLIST_PLAY )
#define playlist_Pause(p) playlist_LockControl(p,PLAYLIST_PAUSE )
#define playlist_Stop(p) playlist_LockControl(p,PLAYLIST_STOP )
#define playlist_Next(p) playlist_LockControl(p,PLAYLIST_SKIP, 1)
#define playlist_Prev(p) playlist_LockControl(p,PLAYLIST_SKIP, -1)
#define playlist_Skip(p,i) playlist_LockControl(p,PLAYLIST_SKIP, i)
#define playlist_Goto(p,i) playlist_LockControl(p,PLAYLIST_GOTO, i)

VLC_EXPORT( int, playlist_Control, ( playlist_t *, int, ...  ) );
VLC_EXPORT( int, playlist_LockControl, ( playlist_t *, int, ...  ) );

VLC_EXPORT( int,  playlist_Clear, ( playlist_t * ) );
VLC_EXPORT( int,  playlist_LockClear, ( playlist_t * ) );

VLC_EXPORT( int, playlist_PreparseEnqueue, (playlist_t *, input_item_t *) );
VLC_EXPORT( int, playlist_PreparseEnqueueItem, (playlist_t *, playlist_item_t *) );

/* Services discovery */

VLC_EXPORT( int, playlist_ServicesDiscoveryAdd, (playlist_t *, const char *));
VLC_EXPORT( int, playlist_ServicesDiscoveryRemove, (playlist_t *, const char *));
VLC_EXPORT( int, playlist_AddSDModules, (playlist_t *, char *));
VLC_EXPORT( vlc_bool_t, playlist_IsServicesDiscoveryLoaded, ( playlist_t *,const char *));


/* Item management functions (act on items) */
#define playlist_AddItem(p,pi,i1,i2) playlist_ItemAdd(p,pi,i1,i2)
#define playlist_ItemNew( a , b, c ) __playlist_ItemNew(VLC_OBJECT(a) , b , c )
#define playlist_ItemCopy( a, b ) __playlist_ItemCopy(VLC_OBJECT(a), b )
VLC_EXPORT( playlist_item_t* , __playlist_ItemNew, ( vlc_object_t *,const char *,const char * ) );
VLC_EXPORT( playlist_item_t* , __playlist_ItemCopy, ( vlc_object_t *,playlist_item_t* ) );
VLC_EXPORT( playlist_item_t* , playlist_ItemNewWithType, ( vlc_object_t *,const char *,const char *, int ) );
VLC_EXPORT( int, playlist_ItemDelete, ( playlist_item_t * ) );
VLC_EXPORT( int, playlist_ItemAddParent, ( playlist_item_t *, int,playlist_item_t *) );
VLC_EXPORT( int, playlist_CopyParents, ( playlist_item_t *,playlist_item_t *) );
/* Item informations accessors */
VLC_EXPORT( int, playlist_ItemSetName, (playlist_item_t *,  char * ) );
VLC_EXPORT( int, playlist_ItemSetDuration, (playlist_item_t *, mtime_t ) );


/* View management functions */
VLC_EXPORT( int, playlist_ViewInsert, (playlist_t *, int, char * ) );
VLC_EXPORT( int, playlist_ViewDelete, (playlist_t *,playlist_view_t* ) );
VLC_EXPORT( playlist_view_t *, playlist_ViewFind, (playlist_t *, int ) );
VLC_EXPORT( int, playlist_ViewUpdate, (playlist_t *, int ) );
VLC_EXPORT( int, playlist_ViewDump, (playlist_t *, playlist_view_t * ) );
VLC_EXPORT( int, playlist_ViewEmpty, (playlist_t *, int, vlc_bool_t ) );

/* Node management */
VLC_EXPORT( playlist_item_t *, playlist_NodeCreate, ( playlist_t *,int,char *, playlist_item_t * p_parent ) );
VLC_EXPORT( int, playlist_NodeAppend, (playlist_t *,int,playlist_item_t*,playlist_item_t *) );
VLC_EXPORT( int, playlist_NodeInsert, (playlist_t *,int,playlist_item_t*,playlist_item_t *, int) );
VLC_EXPORT( int, playlist_NodeRemoveItem, (playlist_t *,playlist_item_t*,playlist_item_t *) );
VLC_EXPORT( int, playlist_NodeRemoveParent, (playlist_t *,playlist_item_t*,playlist_item_t *) );
VLC_EXPORT( int, playlist_NodeChildrenCount, (playlist_t *,playlist_item_t* ) );
VLC_EXPORT( playlist_item_t *, playlist_ChildSearchName, (playlist_item_t*, const char* ) );
VLC_EXPORT( int, playlist_NodeDelete, ( playlist_t *, playlist_item_t *, vlc_bool_t , vlc_bool_t ) );
VLC_EXPORT( int, playlist_NodeEmpty, ( playlist_t *, playlist_item_t *, vlc_bool_t ) );

/* Tree walking */
playlist_item_t *playlist_FindNextFromParent( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_node,
                playlist_item_t *p_item );
playlist_item_t *playlist_FindPrevFromParent( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_node,
                playlist_item_t *p_item );


/* Simple add/remove functions */
/* These functions add the item to the "simple" view (+all & category )*/
VLC_EXPORT( int,  playlist_Add,    ( playlist_t *, const char *, const char *, int, int ) );
VLC_EXPORT( int,  playlist_AddExt, ( playlist_t *, const char *, const char *, int, int, mtime_t, const char **,int ) );
VLC_EXPORT( int,  playlist_ItemAdd, ( playlist_t *, playlist_item_t *, int, int ) );
VLC_EXPORT(int, playlist_NodeAddItem, ( playlist_t *, playlist_item_t *,int,playlist_item_t *,int , int ) );

/* Misc item operations (act on item+playlist) */
VLC_EXPORT( int,  playlist_Delete, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_LockDelete, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_Disable, ( playlist_t *, playlist_item_t * ) );
VLC_EXPORT( int,  playlist_Enable, ( playlist_t *, playlist_item_t * ) );
VLC_EXPORT( int, playlist_ItemToNode, (playlist_t *,playlist_item_t *) );
VLC_EXPORT( int, playlist_LockItemToNode, (playlist_t *,playlist_item_t *) );
VLC_EXPORT( int, playlist_Replace, (playlist_t *,playlist_item_t *, input_item_t*) );
VLC_EXPORT( int, playlist_LockReplace, (playlist_t *,playlist_item_t *, input_item_t*) );


/* Item search functions */
VLC_EXPORT( playlist_item_t *, playlist_ItemGetById, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_LockItemGetById, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_ItemGetByPos, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_LockItemGetByPos, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_ItemGetByInput, (playlist_t *,input_item_t * ) );
VLC_EXPORT( playlist_item_t *, playlist_LockItemGetByInput, (playlist_t *,input_item_t * ) );
VLC_EXPORT( int, playlist_GetPositionById, (playlist_t *,int ) );

VLC_EXPORT( int, playlist_ItemAddOption, (playlist_item_t *, const char *) );

/* Playlist sorting */
#define playlist_SortID(p, i) playlist_Sort( p, SORT_ID, i)
#define playlist_SortTitle(p, i) playlist_Sort( p, SORT_TITLE, i)
#define playlist_SortAuthor(p, i) playlist_Sort( p, SORT_AUTHOR, i)
#define playlist_SortAlbum(p, i) playlist_Sort( p, SORT_ALBUM, i)
#define playlist_SortGroup(p, i) playlist_Sort( p, SORT_GROUP, i)
VLC_EXPORT( int,  playlist_Sort, ( playlist_t *, int, int) );
VLC_EXPORT( int,  playlist_Move, ( playlist_t *, int, int ) );
VLC_EXPORT( int,  playlist_NodeGroup, ( playlist_t *, int,playlist_item_t *,playlist_item_t **,int, int, int ) );
VLC_EXPORT( int,  playlist_NodeSort, ( playlist_t *, playlist_item_t *,int, int ) );
VLC_EXPORT( int,  playlist_RecursiveNodeSort, ( playlist_t *, playlist_item_t *,int, int ) );

/* Load/Save */
VLC_EXPORT( int,  playlist_Import, ( playlist_t *, const char * ) );
VLC_EXPORT( int,  playlist_Export, ( playlist_t *, const char *, const char * ) );

/***********************************************************************
 * Inline functions
 ***********************************************************************/


/**
 *  tell if a playlist is currently playing.
 *  \param p_playlist the playlist to check
 *  \return true if playlist is playing, false otherwise
 */
static inline vlc_bool_t playlist_IsPlaying( playlist_t * p_playlist )
{
    vlc_bool_t b_playing;

    vlc_mutex_lock( &p_playlist->object_lock );
    b_playing = p_playlist->status.i_status == PLAYLIST_RUNNING;
    vlc_mutex_unlock( &p_playlist->object_lock );

    return( b_playing );
}

/**
 *  tell if a playlist is currently empty
 *  \param p_playlist the playlist to check
 *  \return true if the playlist is empty, false otherwise
 */
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
