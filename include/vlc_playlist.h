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

#include <vlc_input.h>
#include <vlc_events.h>

TYPEDEF_ARRAY(playlist_item_t*, playlist_item_array_t)

/**
 * \file
 * This file contain structures and function prototypes related
 * to the playlist in vlc
 *
 * \defgroup vlc_playlist Playlist
 *
 * The VLC playlist system has a tree structure. This allows advanced
 * categorization, like for SAP streams (which are grouped by "sap groups").
 *
 * The base structure for all playlist operations is the input_item_t. This
 * contains all information needed to play a stream and get info, ie, mostly,
 * mrl and metadata. This structure contains a unique i_id field. ids are
 * not recycled when an item is destroyed.
 *
 * Input items are not used directly, but through playlist items.
 * The playlist items are themselves in a tree structure. They only contain
 * a link to the input item, a unique id and a few flags. the playlist
 * item id is NOT the same as the input item id.
 * Several playlist items can be attached to a single input item. The input
 * item is refcounted and is automatically destroyed when it is not used
 * anymore.
 *
 * The top-level items are the main media sources and include:
 * playlist, media library, SAP, Shoutcast, devices, ...
 *
 * It is envisioned that a third tree will appear: VLM, but it's not done yet
 *
 * The playlist also stores, for utility purposes, an array of all input
 * items, an array of all playlist items and an array of all playlist items
 * and nodes (both are represented by the same structure).
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
 * for example. In that case, if the item is under the "playlist" top-level item
 * and playlist is configured to be flat then the item will be deleted and
 * replaced with new subitems. If the item is under another top-level item, it
 * will be transformed to a node and removed from the list of all items without
 * nodes.
 *
 * For "standard" item addition, you can use playlist_Add, playlist_AddExt
 * (more options) or playlist_AddInput if you already created your input
 * item. This will add the item at the root of "Playlist" or of "Media library"
 * in each of the two trees.
 *
 * You can create nodes with playlist_NodeCreate and can create items from
 * existing input items to be placed under any node with playlist_NodeAddInput.
 *
 * To delete an item, use playlist_DeleteFromInput( p_item ) which will
 * remove all occurrences of the input.
 *
 *
 * The playlist defines the following event variables:
 *
 * - "item-change": It will contain the input_item_t->i_id of a changed input
 * item monitored by the playlist.
 * item being played.
 *
 * - "playlist-item-append": It will contain a pointer to a playlist_add_t.
 * - "playlist-item-deleted": It will contain the playlist_item_t->i_id of a
 * deleted playlist_item_t.
 *
 * - "leaf-to-parent": It will contain the playlist_item_t->i_id of an item that is transformed
 *   into a node.
 *
 * The playlist contains rate-variable which is propagated to current input if available
 * also rate-slower/rate-faster is in use
 *
 * XXX Be really carefull, playlist_item_t->i_id and input_item_t->i_id are not
 * the same. Yes, the situation is pretty bad.
 *
 * @{
 */

/** Helper structure to export to file part of the playlist */
typedef struct playlist_export_t
{
    VLC_COMMON_MEMBERS
    const char *psz_filename;
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

    int                    i_id;        /**< Playlist item specific id */
    uint8_t                i_flags;     /**< Flags \see playlist_item_flags_e */

    playlist_t            *p_playlist;  /**< Parent playlist */
};

typedef enum {
    PLAYLIST_SAVE_FLAG         = 0x0001,  /**< Must it be saved */
    PLAYLIST_SKIP_FLAG         = 0x0002,  /**< Must playlist skip after it ? */
    PLAYLIST_DBL_FLAG          = 0x0004,  /**< Is it disabled ? */
    PLAYLIST_RO_FLAG           = 0x0008,  /**< Write-enabled ? */
    PLAYLIST_REMOVE_FLAG       = 0x0010,  /**< Remove this item at the end */
    PLAYLIST_EXPANDED_FLAG     = 0x0020,  /**< Expanded node */
    PLAYLIST_SUBITEM_STOP_FLAG = 0x0040,  /**< Must playlist stop if the item gets subitems ?*/
} playlist_item_flags_e;

/** Playlist status */
typedef enum
{ PLAYLIST_STOPPED,PLAYLIST_RUNNING,PLAYLIST_PAUSED } playlist_status_t;

/** Structure containing information about the playlist */
struct playlist_t
{
    VLC_COMMON_MEMBERS

    playlist_item_array_t items; /**< Arrays of items */
    playlist_item_array_t all_items; /**< Array of items and nodes */

    playlist_item_array_t current; /**< Items currently being played */
    int                   i_current_index; /**< Index in current array */

    /* Predefined items */
    playlist_item_t *     p_root;
    playlist_item_t *     p_playing;
    playlist_item_t *     p_media_library;

    //Phony ones, point to those above;
    playlist_item_t *     p_root_category; /**< Root of category tree */
    playlist_item_t *     p_root_onelevel; /**< Root of onelevel tree */
    playlist_item_t *     p_local_category; /** < "Playlist" in CATEGORY view */
    playlist_item_t *     p_ml_category; /** < "Library" in CATEGORY view */
    playlist_item_t *     p_local_onelevel; /** < "Playlist" in ONELEVEL view */
    playlist_item_t *     p_ml_onelevel; /** < "Library" in ONELEVEL view */
};

/** Helper to add an item */
struct playlist_add_t
{
    int i_node; /**< Playist id of the parent node */
    int i_item; /**< Playist id of the playlist_item_t */
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
    DEF( SORT_URI )

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

/* Used by playlist_Import */
#define PLAYLIST_INSERT          0x0001
#define PLAYLIST_APPEND          0x0002
#define PLAYLIST_GO              0x0004
#define PLAYLIST_PREPARSE        0x0008
#define PLAYLIST_SPREPARSE       0x0010
#define PLAYLIST_NO_REBUILD      0x0020

#define PLAYLIST_END           -666

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

VLC_API playlist_t * pl_Get( vlc_object_t * );
#define pl_Get( a ) pl_Get( VLC_OBJECT(a) )

/* Playlist control */
#define playlist_Play(p) playlist_Control(p,PLAYLIST_PLAY, pl_Unlocked )
#define playlist_Pause(p) playlist_Control(p,PLAYLIST_PAUSE, pl_Unlocked )
#define playlist_Stop(p) playlist_Control(p,PLAYLIST_STOP, pl_Unlocked )
#define playlist_Next(p) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked, 1)
#define playlist_Prev(p) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked, -1)
#define playlist_Skip(p,i) playlist_Control(p,PLAYLIST_SKIP, pl_Unlocked,  (i) )

VLC_API void playlist_Lock( playlist_t * );
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
 * \return VLC_SUCCESS or an error
 */
VLC_API int playlist_Control( playlist_t *p_playlist, int i_query, bool b_locked, ...  );

/** Get current playing input. The object is retained.
 */
VLC_API input_thread_t * playlist_CurrentInput( playlist_t *p_playlist ) VLC_USED;

/** Get the duration of all items in a node.
 */
VLC_API mtime_t playlist_GetNodeDuration( playlist_item_t * );

/** Clear the playlist
 * \param b_locked TRUE if playlist is locked when entering this function
 */
VLC_API void playlist_Clear( playlist_t *, bool );

/** Enqueue an input item for preparsing */
VLC_API int playlist_PreparseEnqueue(playlist_t *, input_item_t * );

/** Request the art for an input item to be fetched */
VLC_API int playlist_AskForArtEnqueue(playlist_t *, input_item_t * );

/* Playlist sorting */
VLC_API int playlist_TreeMove( playlist_t *, playlist_item_t *, playlist_item_t *, int );
VLC_API int playlist_TreeMoveMany( playlist_t *, int, playlist_item_t **, playlist_item_t *, int );
VLC_API int playlist_RecursiveNodeSort( playlist_t *, playlist_item_t *,int, int );

VLC_API playlist_item_t * playlist_CurrentPlayingItem( playlist_t * ) VLC_USED;
VLC_API int playlist_Status( playlist_t * );

/**
 * Export a node of the playlist to a certain type of playlistfile
 * \param p_playlist the playlist to export
 * \param psz_filename the location where the exported file will be saved
 * \param p_export_root the root node to export
 * \param psz_type the type of playlist file to create (m3u, pls, ..)
 * \return VLC_SUCCESS on success
 */
VLC_API int playlist_Export( playlist_t *p_playlist, const char *psz_name, playlist_item_t *p_export_root, const char *psz_type );

/**
 * Open a playlist file, add its content to the current playlist
 */
VLC_API int playlist_Import( playlist_t *p_playlist, const char *psz_file );

/********************** Services discovery ***********************/

/** Add a list of comma-separated service discovery modules */
VLC_API int playlist_ServicesDiscoveryAdd(playlist_t *, const char *);
/** Remove a services discovery module by name */
VLC_API int playlist_ServicesDiscoveryRemove(playlist_t *, const char *);
/** Check whether a given SD is loaded */
VLC_API bool playlist_IsServicesDiscoveryLoaded( playlist_t *,const char *) VLC_DEPRECATED;
/** Query a services discovery */
VLC_API int playlist_ServicesDiscoveryControl( playlist_t *, const char *, int, ... );



/********************************************************
 * Item management
 ********************************************************/

/*************************** Item deletion **************************/
VLC_API int playlist_DeleteFromInput( playlist_t *, input_item_t *, bool );

/******************** Item addition ********************/
VLC_API int playlist_Add( playlist_t *, const char *, const char *, int, int, bool, bool );
VLC_API int playlist_AddExt( playlist_t *, const char *, const char *, int, int, mtime_t, int, const char *const *, unsigned, bool, bool );
VLC_API int playlist_AddInput( playlist_t *, input_item_t *, int, int, bool, bool );
VLC_API playlist_item_t * playlist_NodeAddInput( playlist_t *, input_item_t *, playlist_item_t *, int, int, bool );
VLC_API int playlist_NodeAddCopy( playlist_t *, playlist_item_t *, playlist_item_t *, int );

/********************************** Item search *************************/
VLC_API playlist_item_t * playlist_ItemGetById(playlist_t *, int ) VLC_USED;
VLC_API playlist_item_t * playlist_ItemGetByInput(playlist_t *,input_item_t * ) VLC_USED;

VLC_API int playlist_LiveSearchUpdate(playlist_t *, playlist_item_t *, const char *, bool );

/********************************************************
 * Tree management
 ********************************************************/
/* Node management */
VLC_API playlist_item_t * playlist_NodeCreate( playlist_t *, const char *, playlist_item_t * p_parent, int i_pos, int i_flags, input_item_t * );
VLC_API int playlist_NodeAppend(playlist_t *,playlist_item_t*,playlist_item_t *);
VLC_API int playlist_NodeInsert(playlist_t *,playlist_item_t*,playlist_item_t *, int);
VLC_API int playlist_NodeRemoveItem(playlist_t *,playlist_item_t*,playlist_item_t *);
VLC_API playlist_item_t * playlist_ChildSearchName(playlist_item_t*, const char* ) VLC_USED;
VLC_API int playlist_NodeDelete( playlist_t *, playlist_item_t *, bool , bool );

VLC_API playlist_item_t * playlist_GetNextLeaf( playlist_t *p_playlist, playlist_item_t *p_root, playlist_item_t *p_item, bool b_ena, bool b_unplayed ) VLC_USED;
VLC_API playlist_item_t * playlist_GetPrevLeaf( playlist_t *p_playlist, playlist_item_t *p_root, playlist_item_t *p_item, bool b_ena, bool b_unplayed ) VLC_USED;

/**************************
 * Audio output management
 **************************/

VLC_API audio_output_t *playlist_GetAout( playlist_t * );

#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_MAX                 512

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
/** Small helper tp get current playing input or NULL. Release the input after use. */
#define pl_CurrentInput(a) __pl_CurrentInput( VLC_OBJECT(a) )
static  inline input_thread_t * __pl_CurrentInput( vlc_object_t * p_this )
{
    return playlist_CurrentInput( pl_Get( p_this ) );
}

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
