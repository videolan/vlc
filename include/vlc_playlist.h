/*****************************************************************************
 * vlc_playlist.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: vlc_playlist.h,v 1.22 2004/01/10 03:36:03 hartman Exp $
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
 * Playlist info item
 * \see playlist_item_t
 */

struct item_info_t
{
    char * psz_name;            /**< Name of this info */
    char * psz_value;           /**< Value of the info */
};

/**
 * playlist item info category
 * \see playlist_item_t
 * \see item_info_t
 */
struct item_info_category_t
{
    char * psz_name;            /**< Name of this category */
    int i_infos;                /**< Number of infos in the category */
    item_info_t **pp_infos;     /**< Pointer to an array of infos */
};

/**
 * playlist item
 * \see playlist_t
 */
struct playlist_item_t
{
    char *     psz_name;       /**< text describing this item */
    char *     psz_uri;        /**< mrl of this item */
    mtime_t    i_duration;     /**< A hint about the duration of this
                                * item, in milliseconds*/
    int i_categories;          /**< Number of info categories */
    item_info_category_t **pp_categories;
                               /**< Pointer to the first info category */
    int        i_status;       /**< unused yet */
    int        i_nb_played;    /**< How many times was this item played ? */
    vlc_bool_t b_autodeletion; /**< Indicates whther this item is to
                                * be deleted after playback. True mean
                                * that this item is to be deleted
                                * after playback, false otherwise */
    vlc_bool_t b_enabled;      /**< Indicates whether this item is to be
                                * played or skipped */
    int        i_group;         /**< Which group does this item belongs to ? */
    int        i_id;           /**< Unique id to track this item */
};

/**
 * playlist group
 * \see playlist_t
 */
struct playlist_group_t
{
    char *   psz_name;        /**< name of the group */
    int      i_id;            /**< Identifier for the group */
};

/**
 * Playlist status
 */
typedef enum { PLAYLIST_STOPPED,PLAYLIST_RUNNING,PLAYLIST_PAUSED } playlist_status_t;

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
    playlist_status_t     i_status; /**< current status of playlist */
    int                   i_size;   /**< total size of the list */
    int                   i_enabled; /**< How many items are enabled ? */
    playlist_item_t **    pp_items; /**< array of pointers to the
                                     * playlist items */
    int                   i_groups; /**< How many groups are in the playlist */
    playlist_group_t **   pp_groups;/**< array of pointers to the playlist
                                     * groups */
    int                   i_last_group; /**< Maximal group id given */
    input_thread_t *      p_input;  /**< the input thread ascosiated
                                     * with the current item */
    int                   i_last_id; /**< Last id to an item */
    /*@}*/
};

#define SORT_TITLE 0
#define SORT_AUTHOR 1
#define SORT_GROUP 2
#define SORT_RANDOM 3
#define SORT_ID 4

#define ORDER_NORMAL 0
#define ORDER_REVERSE 1

#define PLAYLIST_TYPE_MANUAL 1
#define PLAYLIST_TYPE_SAP 2

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define playlist_Create(a) __playlist_Create(VLC_OBJECT(a))
playlist_t * __playlist_Create   ( vlc_object_t * );
void           playlist_Destroy  ( playlist_t * );

#define playlist_Play(p) playlist_Command(p,PLAYLIST_PLAY,0)
#define playlist_Pause(p) playlist_Command(p,PLAYLIST_PAUSE,0)
#define playlist_Stop(p) playlist_Command(p,PLAYLIST_STOP,0)
#define playlist_Next(p) playlist_Command(p,PLAYLIST_SKIP,1)
#define playlist_Prev(p) playlist_Command(p,PLAYLIST_SKIP,-1)
#define playlist_Skip(p,i) playlist_Command(p,PLAYLIST_SKIP,i)
#define playlist_Goto(p,i) playlist_Command(p,PLAYLIST_GOTO,i)

VLC_EXPORT( void, playlist_Command, ( playlist_t *, playlist_command_t, int ) );


/* Item functions */
VLC_EXPORT( int,  playlist_Add,    ( playlist_t *, const char *, const char *, int, int ) );
VLC_EXPORT( int,  playlist_AddWDuration, ( playlist_t *, const char *, const char *, int, int, mtime_t ) );
/* For internal use. Do not use this one anymore */
VLC_EXPORT( int,  playlist_AddItem, ( playlist_t *, playlist_item_t *, int, int ) );
VLC_EXPORT( int,  playlist_Clear, ( playlist_t * ) );
VLC_EXPORT( int,  playlist_Delete, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_Disable, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_Enable, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_DisableGroup, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_EnableGroup, ( playlist_t *, int ) );

/* Basic item informations accessors */
VLC_EXPORT( int, playlist_SetGroup, (playlist_t *, int, int ) );
VLC_EXPORT( int, playlist_SetName, (playlist_t *, int, char * ) );
VLC_EXPORT( int, playlist_SetDuration, (playlist_t *, int, mtime_t ) );

/* Item search functions */
VLC_EXPORT( int, playlist_GetPositionById, (playlist_t *, int) );
VLC_EXPORT( playlist_item_t *, playlist_GetItemById, (playlist_t *, int) );


/* Group management functions */
VLC_EXPORT( playlist_group_t *, playlist_CreateGroup, (playlist_t *, char* ) );
VLC_EXPORT( int, playlist_DeleteGroup, (playlist_t *, int ) );
VLC_EXPORT( char *, playlist_FindGroup, (playlist_t *, int ) );
VLC_EXPORT( int, playlist_GroupToId, (playlist_t *, char * ) );

/* Info functions */
VLC_EXPORT( char * , playlist_GetInfo, ( playlist_t * , int, const char *, const char *) );
VLC_EXPORT( char * , playlist_GetItemInfo, ( playlist_item_t * , const char *, const char *) );

VLC_EXPORT( item_info_category_t*, playlist_GetCategory, ( playlist_t *, int, const char *) );
VLC_EXPORT( item_info_category_t*, playlist_GetItemCategory, ( playlist_item_t *, const char *) );

VLC_EXPORT( item_info_category_t*, playlist_CreateCategory, ( playlist_t *, int, const char *) );
VLC_EXPORT( item_info_category_t*, playlist_CreateItemCategory, ( playlist_item_t *, const char *) );

VLC_EXPORT( int, playlist_AddInfo, (playlist_t *, int, const char * , const char *, const char *, ...) );
VLC_EXPORT( int, playlist_AddItemInfo, (playlist_item_t *, const char * , const char *, const char *, ...) );

/* Option functions */
VLC_EXPORT( int, playlist_AddOption, (playlist_t *, int, const char *, ...) );
VLC_EXPORT( int, playlist_AddItemOption, (playlist_item_t *, const char *, ...) );

/* Playlist sorting */
#define playlist_SortTitle(p, i) playlist_Sort( p, SORT_TITLE, i)
#define playlist_SortAuthor(p, i) playlist_Sort( p, SORT_AUTHOR, i)
#define playlist_SortGroup(p, i) playlist_Sort( p, SORT_GROUP, i)
VLC_EXPORT( int,  playlist_Sort, ( playlist_t *, int, int) );
VLC_EXPORT( int,  playlist_Move, ( playlist_t *, int, int ) );

/* Load/Save */
VLC_EXPORT( int,  playlist_LoadFile, ( playlist_t *, const char * ) );
VLC_EXPORT( int,  playlist_SaveFile, ( playlist_t *, const char * ) );

/**
 *  tell if a playlist is currently playing.
 *  \param p_playlist the playlist to check
 *  \return true if playlist is playing, false otherwise
 */
static inline vlc_bool_t playlist_IsPlaying( playlist_t * p_playlist )
{
    vlc_bool_t b_playing;

    vlc_mutex_lock( &p_playlist->object_lock );
    b_playing = p_playlist->i_status == PLAYLIST_RUNNING; 
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
