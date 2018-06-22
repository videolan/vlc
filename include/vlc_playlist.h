/*****************************************************************************
 * vlc_playlist.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_PLAYLIST_H_
#define VLC_PLAYLIST_H_

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc_events.h>
#include <vlc_aout.h>

TYPEDEF_ARRAY(playlist_item_t*, playlist_item_array_t)

struct intf_thread_t;

/**
 * \defgroup playlist VLC playlist
 * VLC playlist controls
 * @{
 * \file
 * VLC playlist control interface
 *
 * The VLC playlist system has a tree structure. This allows advanced
 * categorization, like for SAP streams (which are grouped by "sap groups").
 *
 * The base structure for all playlist operations is the playlist_item_t.
 * This is essentially a node within the playlist tree. Each playlist item
 * references an input_item_t which contains the input stream info, such as
 * location, name and meta-data.
 *
 * A playlist item is uniquely identified by its input item:
 * \ref playlist_ItemGetByInput(). A single input item cannot be used by more
 * than one playlist item at a time; if necessary, a copy of the input item can
 * be made instead.
 *
 * The same playlist tree is visible to all user interfaces. To arbitrate
 * access, a lock is used, see \ref playlist_Lock() and \ref playlist_Unlock().
 *
 * Under the playlist root item node, the top-level items are the main
 * media sources and include:
 * - the actual playlist,
 * - the media library,
 * - the service discovery root node, whose children are services discovery
 *   module instances.
 *
 * So, here is an example:
 * \verbatim
 * Inputs array
 *  - input 1 -> name = foo 1 uri = ...
 *  - input 2 -> name = foo 2 uri = ...
 *
 * Playlist items tree
 * - playlist (id 1)
 *    - category 1 (id 2)
 *      - foo 2 (id 6 - input 2)
 * - media library (id 2)
 *    - foo 1 (id 5 - input 1)
 * \endverbatim
 *
 * Sometimes, an item creates subitems. This happens for the directory access
 * for example. In that case, if the item is under the "playlist" top-level
 * item and playlist is configured to be flat then the item will be deleted and
 * replaced with new subitems. If the item is under another top-level item, it
 * will be transformed to a node and removed from the list of all items without
 * nodes.
 *
 * For "standard" item addition, you can use playlist_Add(), playlist_AddExt()
 * (more options) or playlist_AddInput() if you already created your input
 * item. This will add the item at the root of "Playlist" or of "Media library"
 * in each of the two trees.
 *
 * You can create nodes with playlist_NodeCreate() and can create items from
 * existing input items to be placed under any node with
 * playlist_NodeAddInput().
 *
 * To delete an item, use playlist_NodeDelete( p_item ).
 *
 * The playlist defines the following event variables:
 *
 * - "item-change": It will contain a pointer to the input_item_t of a
 * changed input item monitored by the playlist.
 *
 * - "playlist-item-append": It will contain a pointer to a playlist_item_t.
 * - "playlist-item-deleted": It will contain a pointer to the playlist_item_t
 * about to be deleted.
 *
 * - "leaf-to-parent": It will contain the playlist_item_t->i_id of an item that is transformed
 *   into a node.
 *
 * The playlist contains rate-variable which is propagated to current input if
 * available also rate-slower/rate-faster is in use.
 */

/** Helper structure to export to file part of the playlist */
typedef struct playlist_export_t
{
    VLC_COMMON_MEMBERS
    char *base_url;
    FILE *p_file;
    playlist_item_t *p_root;
} playlist_export_t;

/** playlist item / node */
struct playlist_item_t
{
    input_item_t           *p_input;    /**< Linked input item */

    playlist_item_t      **pp_children; /**< Children nodes/items */
    playlist_item_t       *p_parent;    /**< Item parent */
    int                    i_children;  /**< Number of children, -1 if not a node */
    unsigned               i_nb_played; /**< Times played */

    int                    i_id;        /**< Playlist item specific id */
    uint8_t                i_flags;     /**< Flags \see playlist_item_flags_e */
};

typedef enum {
    PLAYLIST_DBL_FLAG          = 0x04,  /**< Is it disabled ? */
    PLAYLIST_RO_FLAG           = 0x08,  /**< Write-enabled ? */
    PLAYLIST_SUBITEM_STOP_FLAG = 0x40,  /**< Must playlist stop if the item gets subitems ?*/
    PLAYLIST_NO_INHERIT_FLAG   = 0x80,  /**< Will children inherit flags the R/O flag ? */
} playlist_item_flags_e;

/** Playlist status */
typedef enum
{ PLAYLIST_STOPPED,PLAYLIST_RUNNING,PLAYLIST_PAUSED } playlist_status_t;

/** Structure containing information about the playlist */
struct playlist_t
{
    VLC_COMMON_MEMBERS

    playlist_item_array_t items; /**< Arrays of items */

    playlist_item_array_t current; /**< Items currently being played */
    int                   i_current_index; /**< Index in current array */

    /* Predefined items */
    playlist_item_t  root;
    playlist_item_t *p_playing;
    playlist_item_t *p_media_library;
};

/* A bit of macro magic to generate an enum out of the following list,
 * and later, to generate a list of static functions out of the same list.
 * There is also SORT_RANDOM, which is always last and handled specially.
 */
#define VLC_DEFINE_SORT_FUNCTIONS \
    DEF( SORT_ID )\
    DEF( SORT_TITLE )\
    DEF( SORT_TITLE_NODES_FIRST )\
    DEF( SORT_ARTIST )\
    DEF( SORT_GENRE )\
    DEF( SORT_DURATION )\
    DEF( SORT_TITLE_NUMERIC )\
    DEF( SORT_ALBUM )\
    DEF( SORT_TRACK_NUMBER )\
    DEF( SORT_DESCRIPTION )\
    DEF( SORT_RATING )\
    DEF( SORT_URI )\
    DEF( SORT_DISC_NUMBER )\
    DEF( SORT_DATE )

#define DEF( s ) s,
enum
{
    VLC_DEFINE_SORT_FUNCTIONS
    SORT_RANDOM,
    NUM_SORT_FNS=SORT_RANDOM
};
#undef  DEF
#ifndef VLC_INTERNAL_PLAYLIST_SORT_FUNCTIONS
#undef  VLC_DEFINE_SORT_FUNCTIONS
#endif

enum
{
    ORDER_NORMAL = 0,
    ORDER_REVERSE = 1,
};

#define PLAYLIST_END           -1

enum pl_locked_state
{
    pl_Locked = true,
    pl_Unlocked = false
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Helpers */
#define PL_LOCK playlist_Lock( p_playlist )
#define PL_UNLOCK playlist_Unlock( p_playlist )
#define PL_ASSERT_LOCKED playlist_AssertLocked( p_playlist )

/** Playlist commands */
enum {
    PLAYLIST_PLAY,      /**< No arg.                            res=can fail*/
    PLAYLIST_VIEWPLAY,  /**< arg1= playlist_item_t*,*/
                        /**  arg2 = playlist_item_t*          , res=can fail */
    PLAYLIST_TOGGLE_PAUSE, /**< No arg                          res=can fail */
    PLAYLIST_STOP,      /**< No arg                             res=can fail*/
    PLAYLIST_SKIP,      /**< arg1=int,                          res=can fail*/
    PLAYLIST_PAUSE,     /**< No arg */
    PLAYLIST_RESUME,    /**< No arg */
};

#define playlist_Play(p) playlist_Control(p,PLAYLIST_PLAY, pl_Unlocked )
#define playlist_TogglePause(p) \
        playlist_Control(p, PLAYLIST_TOGGLE_PAUSE, pl_Unlocked)
#define playlist_Stop(p) playlist_Control(p,PLAYLIST_STOP, pl_Unlocked )
#define playlist_Next(p) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked, 1)
#define playlist_Prev(p) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked, -1)
#define playlist_Skip(p,i) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked,  (i) )
#define playlist_Pause(p) \
        playlist_Control(p, PLAYLIST_PAUSE, pl_Unlocked)
#define playlist_Resume(p) \
        playlist_Control(p, PLAYLIST_RESUME, pl_Unlocked)

/**
 * Locks the playlist.
 *
 * This function locks the playlist. While the playlist is locked, no other
 * thread can modify the playlist tree layout or current playing item and node.
 *
 * Locking the playlist is necessary before accessing, either for reading or
 * writing, any playlist item.
 *
 * \note Because of the potential for lock inversion / deadlocks, locking the
 * playlist shall not be attemped while holding an input item lock. An input
 * item lock can be acquired while holding the playlist lock.
 *
 * While holding the playlist lock, a thread shall not attempt to:
 * - probe, initialize or deinitialize a module or a plugin,
 * - install or deinstall a variable or event callback,
 * - set a variable or trigger a variable callback, with the sole exception
 *   of the playlist core triggering add/remove/leaf item callbacks,
 * - invoke a module/plugin callback other than:
 *   - playlist export,
 *   - logger message callback.
 */
VLC_API void playlist_Lock( playlist_t * );

/**
 * Unlocks the playlist.
 *
 * This function unlocks the playlist, allowing other threads to lock it. The
 * calling thread must have called playlist_Lock() before.
 *
 * This function invalidates all or any playlist item pointers.
 * There are no ways to ensure that playlist items are not modified or deleted
 * by another thread past this function call.
 *
 * To retain a reference to a playlist item while not holding the playlist
 * lock, a thread should take a reference to the input item within the
 * playlist item before unlocking. If this is not practical, then the thread
 * can store the playlist item ID (i_id) before unlocking.
 * Either way, this will not ensure that the playlist item is not deleted, so
 * the thread must be ready to handle that case later when calling
 * playlist_ItemGetByInput() or playlist_ItemGetById().
 *
 * Furthermore, if ID is used, then the playlist item might be deleted, and
 * another item could be assigned the same ID. To avoid that problem, use
 * the input item instead of the ID.
 */
VLC_API void playlist_Unlock( playlist_t * );

VLC_API void playlist_AssertLocked( playlist_t * );
VLC_API void playlist_Deactivate( playlist_t * );

/**
 * Do a playlist action.
 * If there is something in the playlist then you can do playlist actions.
 * Possible queries are listed in vlc_common.h
 * \param p_playlist the playlist to do the command on
 * \param i_query the command to do
 * \param b_locked TRUE if playlist is locked when entering this function
 * \param variable number of arguments
 */
VLC_API void playlist_Control( playlist_t *p_playlist, int i_query, int b_locked, ...  );

static inline void playlist_ViewPlay(playlist_t *pl, playlist_item_t *node,
                                     playlist_item_t *item)
{
    playlist_Control(pl, PLAYLIST_VIEWPLAY, pl_Locked, node, item);
}

/** Get current playing input. The object is retained.
 */
VLC_API input_thread_t * playlist_CurrentInput( playlist_t *p_playlist ) VLC_USED;
VLC_API input_thread_t *playlist_CurrentInputLocked( playlist_t *p_playlist ) VLC_USED;

/** Get the duration of all items in a node.
 */
VLC_API vlc_tick_t playlist_GetNodeDuration( playlist_item_t * );

/** Clear the playlist
 * \param b_locked TRUE if playlist is locked when entering this function
 */
VLC_API void playlist_Clear( playlist_t *, bool );

/* Playlist sorting */
VLC_API int playlist_TreeMove( playlist_t *, playlist_item_t *, playlist_item_t *, int );
VLC_API int playlist_TreeMoveMany( playlist_t *, int, playlist_item_t **, playlist_item_t *, int );
VLC_API int playlist_RecursiveNodeSort( playlist_t *, playlist_item_t *,int, int );

VLC_API playlist_item_t * playlist_CurrentPlayingItem( playlist_t * ) VLC_USED;
VLC_API int playlist_Status( playlist_t * );

/**
 * Export a node of the playlist to a certain type of playlistfile
 * \param b_playlist true for the playlist, false for the media library
 * \param psz_filename the location where the exported file will be saved
 * \param psz_type the type of playlist file to create (m3u, pls, ..)
 * \return VLC_SUCCESS on success
 */
VLC_API int playlist_Export( playlist_t *p_playlist, const char *psz_name,
                             bool b_playlist, const char *psz_type );

/**
 * Open a playlist file, add its content to the current playlist
 */
VLC_API int playlist_Import( playlist_t *p_playlist, const char *psz_file );

/********************** Services discovery ***********************/

/** Add a service discovery module */
VLC_API int playlist_ServicesDiscoveryAdd(playlist_t *, const char *);
/** Remove a services discovery module by name */
VLC_API int playlist_ServicesDiscoveryRemove(playlist_t *, const char *);
/** Check whether a given SD is loaded */
VLC_API bool playlist_IsServicesDiscoveryLoaded( playlist_t *,const char *) VLC_DEPRECATED;
/** Query a services discovery */
VLC_API int playlist_ServicesDiscoveryControl( playlist_t *, const char *, int, ... );

/********************** Renderer ***********************/
/**
 * Sets a renderer or remove the current one
 * @param p_item    The renderer item to be used, or NULL to disable the current
 *                  one. If a renderer is provided, its reference count will be
 *                  incremented.
 */
VLC_API int playlist_SetRenderer( playlist_t* p_pl, vlc_renderer_item_t* p_item );


/********************************************************
 * Item management
 ********************************************************/

/******************** Item addition ********************/
VLC_API int playlist_Add( playlist_t *, const char *, bool );
VLC_API int playlist_AddExt( playlist_t *, const char *, const char *, bool, int, const char *const *, unsigned, bool );
VLC_API int playlist_AddInput( playlist_t *, input_item_t *, bool, bool );
VLC_API playlist_item_t * playlist_NodeAddInput( playlist_t *, input_item_t *, playlist_item_t *, int );
VLC_API int playlist_NodeAddCopy( playlist_t *, playlist_item_t *, playlist_item_t *, int );

/********************************** Item search *************************/
VLC_API playlist_item_t * playlist_ItemGetById(playlist_t *, int ) VLC_USED;
VLC_API playlist_item_t *playlist_ItemGetByInput(playlist_t *,
                                                 const input_item_t * )
VLC_USED;

VLC_API int playlist_LiveSearchUpdate(playlist_t *, playlist_item_t *, const char *, bool );

/********************************************************
 * Tree management
 ********************************************************/
/* Node management */
VLC_API playlist_item_t * playlist_NodeCreate( playlist_t *, const char *, playlist_item_t * p_parent, int i_pos, int i_flags );
VLC_API playlist_item_t * playlist_ChildSearchName(playlist_item_t*, const char* ) VLC_USED;
VLC_API void playlist_NodeDelete( playlist_t *, playlist_item_t * );

/**************************
 * Audio output management
 **************************/

VLC_API audio_output_t *playlist_GetAout( playlist_t * );

VLC_API float playlist_VolumeGet( playlist_t * );
VLC_API int playlist_VolumeSet( playlist_t *, float );
VLC_API int playlist_VolumeUp( playlist_t *, int, float * );
#define playlist_VolumeDown(a, b, c) playlist_VolumeUp(a, -(b), c)
VLC_API int playlist_MuteSet( playlist_t *, bool );
VLC_API int playlist_MuteGet( playlist_t * );

static inline int playlist_MuteToggle( playlist_t *pl )
{
    int val = playlist_MuteGet( pl );
    if (val >= 0)
        val = playlist_MuteSet( pl, !val );
    return val;
}

VLC_API void playlist_EnableAudioFilter( playlist_t *, const char *, bool );

/***********************************************************************
 * Inline functions
 ***********************************************************************/
/** Tell if the playlist is empty */
static inline bool playlist_IsEmpty( playlist_t *p_playlist )
{
    PL_ASSERT_LOCKED;
    return p_playlist->items.i_size == 0;
}

/** Tell the number of items in the current playing context */
static inline int playlist_CurrentSize( playlist_t *p_playlist )
{
    PL_ASSERT_LOCKED;
    return p_playlist->current.i_size;
}

/** @} */
# ifdef __cplusplus
}
# endif

#endif
