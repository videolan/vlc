/*****************************************************************************
 * vlc_playlist.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VideoLAN
 * $Id: vlc_playlist.h,v 1.12 2003/08/14 13:02:55 sigmunau Exp $
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
 * playlist item
 * \see playlist_t
 */
struct playlist_item_t
{
    char *     psz_name;       /**< text describing this item */
    char *     psz_uri;        /**< mrl of this item */
    mtime_t    i_duration;     /**< A hint about the duration of this
                                * item, in miliseconds*/
    char **    ppsz_options;   /**< options passed with the :foo=bar syntax */
    int        i_options;      /**< number of items in the
                                * ppsz_options array */
    int        i_type;         /**< unused yet */
    int        i_status;       /**< unused yet */
    vlc_bool_t b_autodeletion; /**< Indicates wether this item is to
                                * be deleted after playback. True mean
                                * that this item is to be deleted
                                * after playback, false otherwise */
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

    playlist_item_t **    pp_items; /**< array of pointers to the
                                     * playlist items */

    input_thread_t *      p_input;  /**< the input thread ascosiated
                                     * with the current item */
    /*@}*/
};

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

VLC_EXPORT( int,  playlist_Add,    ( playlist_t *, const char *, const char **, int, int, int ) );
VLC_EXPORT( int,  playlist_AddExt,    ( playlist_t *, const char *, const char *, mtime_t, const char **, int, int, int ) );
VLC_EXPORT( int,  playlist_AddItem, ( playlist_t *, playlist_item_t *, int, int ) );
VLC_EXPORT( int,  playlist_Delete, ( playlist_t *, int ) );
VLC_EXPORT( int,  playlist_Move, ( playlist_t *, int, int ) );
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
