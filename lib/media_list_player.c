/*****************************************************************************
 * media_list_player.c: libvlc new API media_list player functions
 *****************************************************************************
 * Copyright (C) 2007-2015 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Niles Bindel <zaggal69 # gmail.com>
 *          Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/libvlc_media_list_player.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>

#include "libvlc_internal.h"

#include "media_internal.h" // Abuse, could and should be removed
#include "media_player_internal.h"
#include "media_list_path.h"

//#define DEBUG_MEDIA_LIST_PLAYER

/* This is a very dummy implementation of playlist on top of
 * media_list and media_player.
 *
 * All this code is doing is simply computing the next item
 * of a tree of media_list (see get_next_index()), and play
 * the next item when the current is over.
 */

struct libvlc_media_list_player_t
{
    bool                        dead;
    libvlc_media_list_path_t    current_playing_item_path;
    libvlc_media_t *            p_current_playing_item;
    libvlc_media_list_t *       p_mlist;
    libvlc_media_player_t *     p_mi;
    libvlc_playback_mode_t      e_playback_mode;

    vlc_player_listener_id *    internal_listener;

    vlc_atomic_rc_t             rc;
};

/*
 * Forward declaration
 */

static
int set_relative_playlist_position_and_play(libvlc_media_list_player_t *p_mlp,
                                            bool next);
static void stop(libvlc_media_list_player_t * p_mlp);

/*
 * Private functions
 */

/**************************************************************************
 * Shortcuts
 **************************************************************************/
static inline void lock(libvlc_media_list_player_t * p_mlp)
{
    // Obtain an access to this structure
    vlc_player_Lock(p_mlp->p_mi->player);
}

static inline void unlock(libvlc_media_list_player_t * p_mlp)
{
    vlc_player_Unlock(p_mlp->p_mi->player);
}

/**************************************************************************
 *       get_next_path (private)
 *
 *  Returns the path to the next item in the list.
 *  If looping is specified and the current item is the last list item in
 *  the list it will return the first item in the list.
 **************************************************************************/
static libvlc_media_list_path_t
get_next_path(libvlc_media_list_player_t * p_mlp, bool b_loop)
{
    /* We are entered with libvlc_media_list_lock(p_mlp->p_list) */
    libvlc_media_list_path_t ret;
    libvlc_media_list_t * p_parent_of_playing_item;
    libvlc_media_list_t * p_sublist_of_playing_item;

    if (!p_mlp->current_playing_item_path)
    {
        if (!libvlc_media_list_count(p_mlp->p_mlist))
            return NULL;
        return libvlc_media_list_path_with_root_index(0);
    }

    p_sublist_of_playing_item = libvlc_media_list_sublist_at_path(
                            p_mlp->p_mlist,
                            p_mlp->current_playing_item_path);

    /* If item just gained a sublist just play it */
    if (p_sublist_of_playing_item)
    {
        int i_count = libvlc_media_list_count(p_sublist_of_playing_item);
        libvlc_media_list_release(p_sublist_of_playing_item);
        if (i_count > 0)
            return libvlc_media_list_path_copy_by_appending(p_mlp->current_playing_item_path, 0);
    }

    /* Try to catch parent element */
    p_parent_of_playing_item = libvlc_media_list_parentlist_at_path(p_mlp->p_mlist,
                            p_mlp->current_playing_item_path);

    int depth = libvlc_media_list_path_depth(p_mlp->current_playing_item_path);
    if (depth < 1 || !p_parent_of_playing_item)
    {
        if (p_parent_of_playing_item)
            libvlc_media_list_release(p_parent_of_playing_item);
        return NULL;
    }

    ret = libvlc_media_list_path_copy(p_mlp->current_playing_item_path);
    ret[depth - 1]++; /* set to next element */

    /* If this goes beyond the end of the list */
    while(ret[depth-1] >= libvlc_media_list_count(p_parent_of_playing_item))
    {
        depth--;
        if (depth <= 0)
        {
            if(b_loop)
            {
                ret[0] = 0;
                ret[1] = -1;
                break;
            }
            else
            {
                free(ret);
                libvlc_media_list_release(p_parent_of_playing_item);
                return NULL;
            }
        }
        ret[depth] = -1;
        ret[depth-1]++;
        libvlc_media_list_release(p_parent_of_playing_item);
        p_parent_of_playing_item  = libvlc_media_list_parentlist_at_path(
                                        p_mlp->p_mlist,
                                        ret);
    }

    libvlc_media_list_release(p_parent_of_playing_item);
    return ret;
}

/**************************************************************************
 *       find_last_item (private)
 *
 *  Returns the path of the last descendant of a given item path.
 *  Note: Due to the recursive nature of the function and the need to free
 *        media list paths, paths passed in may be freed if they are replaced.
          Recommended usage is to set return value to the same path that was
          passed to the function (i.e. item = find_last_item(list, item); )
 **************************************************************************/
static libvlc_media_list_path_t
find_last_item( libvlc_media_list_t * p_mlist, libvlc_media_list_path_t current_item )
{
    libvlc_media_list_t * p_sublist = libvlc_media_list_sublist_at_path(p_mlist, current_item);
    libvlc_media_list_path_t last_item_path = current_item;

    if(p_sublist)
    {
        int i_count = libvlc_media_list_count(p_sublist);
        if(i_count > 0)
        {
            /* Add the last sublist item to the path. */
            last_item_path = libvlc_media_list_path_copy_by_appending(current_item, i_count - 1);
            free(current_item);
            /* Check that sublist item for more descendants. */
            last_item_path = find_last_item(p_mlist, last_item_path);
        }

        libvlc_media_list_release(p_sublist);
    }

    return last_item_path;
}

/**************************************************************************
 *       get_previous_path (private)
 *
 *  Returns the path to the preceding item in the list.
 *  If looping is specified and the current item is the first list item in
 *  the list it will return the last descendant of the last item in the list.
 **************************************************************************/
static libvlc_media_list_path_t
get_previous_path(libvlc_media_list_player_t * p_mlp, bool b_loop)
{
    /* We are entered with libvlc_media_list_lock(p_mlp->p_list) */
    libvlc_media_list_path_t ret;
    libvlc_media_list_t * p_parent_of_playing_item;

    if (!p_mlp->current_playing_item_path)
    {
        if (!libvlc_media_list_count(p_mlp->p_mlist))
            return NULL;
        return libvlc_media_list_path_with_root_index(0);
    }

    /* Try to catch parent element */
    p_parent_of_playing_item = libvlc_media_list_parentlist_at_path(
                                            p_mlp->p_mlist,
                                            p_mlp->current_playing_item_path);

    int depth = libvlc_media_list_path_depth(p_mlp->current_playing_item_path);
    if (depth < 1 || !p_parent_of_playing_item)
        return NULL;

    /* Set the return path to the current path */
    ret = libvlc_media_list_path_copy(p_mlp->current_playing_item_path);

    /* Change the return path to the previous list entry */
    ret[depth - 1]--; /* set to previous element */
    ret[depth] = -1;

    /* Is the return path is beyond the start of the current list? */
    if(ret[depth - 1] < 0)
    {
        /* Move to parent of current item */
        depth--;

        /* Are we at the root level of the tree? */
        if (depth <= 0)
        {
            // Is looping enabled?
            if(b_loop)
            {
                int i_count = libvlc_media_list_count(p_parent_of_playing_item);

                /* Set current play item to the last element in the list */
                ret[0] = i_count - 1;
                ret[1] = -1;

                /* Set the return path to the last descendant item of the current item */
                ret = find_last_item(p_mlp->p_mlist, ret);
            }
            else
            {
                /* No looping so return empty path. */
                free(ret);
                ret = NULL;
            }
        }
        else
        {
            /* This is the case of moving backward from the beginning of the
            *  subitem list to its parent item.
            *  This ensures that current path is properly terminated to
            *  use that parent.
            */
            ret[depth] = -1;
        }
    }
    else
    {
        ret = find_last_item(p_mlp->p_mlist, ret);
    }

    libvlc_media_list_release(p_parent_of_playing_item);
    return ret;
}

/**************************************************************************
 *       set_current_playing_item (private)
 *
 * Playlist lock should be held
 **************************************************************************/
static libvlc_media_t *
set_current_playing_item(libvlc_media_list_player_t * p_mlp, libvlc_media_list_path_t path)
{
    /* First, save the new path that we are going to play */
    if (p_mlp->current_playing_item_path != path)
    {
        free(p_mlp->current_playing_item_path);
        p_mlp->current_playing_item_path = path;
    }

    if (!path)
        return NULL;

    return libvlc_media_list_item_at_path(p_mlp->p_mlist, path);
}

static void
internal_player_media_changed(vlc_player_t *player, input_item_t *new_media,
                              void *opaque)
{
    (void) player;
    libvlc_media_list_player_t *p_mlp = opaque;
    libvlc_media_t *md = NULL;

    libvlc_media_list_lock(p_mlp->p_mlist);

    /* Update the internal current path from the new media */
    libvlc_media_list_path_t path =
        libvlc_media_list_path_of_item(p_mlp->p_mlist, new_media->libvlc_owner);
    if (p_mlp->current_playing_item_path != path)
    {
        free(p_mlp->current_playing_item_path);
        p_mlp->current_playing_item_path = path;
    }

    /* Find and set the next media */
    if (p_mlp->e_playback_mode != libvlc_playback_mode_repeat)
    {
        bool b_loop = (p_mlp->e_playback_mode == libvlc_playback_mode_loop);
        path = get_next_path(p_mlp, b_loop);
    }
    else
        path = libvlc_media_list_path_copy(p_mlp->current_playing_item_path);


    if (path != NULL)
    {
        md = libvlc_media_list_item_at_path(p_mlp->p_mlist, path);
        free(path);
    }

    libvlc_media_list_unlock(p_mlp->p_mlist);

    libvlc_media_player_set_next_media(p_mlp->p_mi, md);
    libvlc_media_release(md);
}

static void
internal_player_subitems_changed(vlc_player_t *player, input_item_t *media,
                                 const input_item_node_t *new_subitems, void *opaque)
{
    (void) new_subitems;
    internal_player_media_changed(player, media, opaque);
}


/*
 * Public libvlc functions
 */

/**************************************************************************
 *         new (Public)
 **************************************************************************/
libvlc_media_list_player_t *
libvlc_media_list_player_new(libvlc_instance_t * p_instance,
                             const struct libvlc_media_player_cbs *cbs,
                             void *cbs_opaque)
{
    libvlc_media_list_player_t * p_mlp;
    p_mlp = calloc( 1, sizeof(libvlc_media_list_player_t) );
    if (unlikely(p_mlp == NULL))
    {
        libvlc_printerr("Not enough memory");
        return NULL;
    }

    vlc_atomic_rc_init(&p_mlp->rc);
    p_mlp->dead = false;

    /* Create the underlying media_player */
    p_mlp->p_mi = libvlc_media_player_new(p_instance, cbs, cbs_opaque);
    if( p_mlp->p_mi == NULL )
        goto error;

    vlc_player_t *player = p_mlp->p_mi->player;
    static const struct vlc_player_cbs internal_players_cbs =
    {
        .on_current_media_changed = internal_player_media_changed,
        .on_media_subitems_changed = internal_player_subitems_changed,
    };

    vlc_player_Lock(player);
    p_mlp->internal_listener =
        vlc_player_AddListener(player, &internal_players_cbs, p_mlp);
    vlc_player_Unlock(player);

    return p_mlp;
error:
    free(p_mlp);
    return NULL;
}

/**************************************************************************
 *         release (Public)
 **************************************************************************/
void libvlc_media_list_player_release(libvlc_media_list_player_t * p_mlp)
{
    if (!p_mlp)
        return;

    if (!vlc_atomic_rc_dec(&p_mlp->rc))
        return;

    vlc_player_t *player = p_mlp->p_mi->player;
    vlc_player_Lock(player);
    vlc_player_RemoveListener(player, p_mlp->internal_listener);
    vlc_player_Unlock(player);

    libvlc_media_player_release(p_mlp->p_mi);

    if (p_mlp->p_mlist)
        libvlc_media_list_release(p_mlp->p_mlist);

    free(p_mlp->current_playing_item_path);
    free(p_mlp);
}

/**************************************************************************
 *        retain (Public)
 **************************************************************************/
libvlc_media_list_player_t *libvlc_media_list_player_retain(libvlc_media_list_player_t * p_mlp)
{
    assert(p_mlp);
    vlc_atomic_rc_inc(&p_mlp->rc);
    return p_mlp;
}

/**************************************************************************
 *        get_media_player (Public)
 **************************************************************************/
libvlc_media_player_t * libvlc_media_list_player_get_media_player(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    libvlc_media_player_t *p_mi = p_mlp->p_mi;
    libvlc_media_player_retain(p_mi);
    unlock(p_mlp);
    return p_mi;
}

/**************************************************************************
 *       set_media_list (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_list(libvlc_media_list_player_t * p_mlp, libvlc_media_list_t * p_mlist)
{
    assert (p_mlist);

    lock(p_mlp);
    if (p_mlp->p_mlist)
        libvlc_media_list_release(p_mlp->p_mlist);
    libvlc_media_list_retain(p_mlist);

    p_mlp->p_mlist = p_mlist;
    if (libvlc_media_player_is_playing(p_mlp->p_mi))
    {
        stop(p_mlp);
        set_relative_playlist_position_and_play(p_mlp, true);
    }

    unlock(p_mlp);
}

/**************************************************************************
 *        Play (Public)
 **************************************************************************/
void libvlc_media_list_player_play(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    if (!p_mlp->current_playing_item_path)
    {
        set_relative_playlist_position_and_play(p_mlp, true);
        unlock(p_mlp);
        return; /* Will set to play */
    }
    libvlc_media_player_play(p_mlp->p_mi);
    unlock(p_mlp);
}


/**************************************************************************
 *        Pause (Public)
 **************************************************************************/
void libvlc_media_list_player_pause(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    libvlc_media_player_pause(p_mlp->p_mi);
    unlock(p_mlp);
}

void libvlc_media_list_player_set_pause(libvlc_media_list_player_t * p_mlp,
                                        int do_pause)
{
    lock(p_mlp);
    libvlc_media_player_set_pause(p_mlp->p_mi, do_pause);
    unlock(p_mlp);
}

/**************************************************************************
 *        is_playing (Public)
 **************************************************************************/
bool libvlc_media_list_player_is_playing(libvlc_media_list_player_t * p_mlp)
{
    libvlc_state_t state = libvlc_media_player_get_state(p_mlp->p_mi);
    return (state == libvlc_Opening) || (state == libvlc_Playing);
}

/**************************************************************************
 *        State (Public)
 **************************************************************************/
libvlc_state_t
libvlc_media_list_player_get_state(libvlc_media_list_player_t * p_mlp)
{
    return libvlc_media_player_get_state(p_mlp->p_mi);
}

/**************************************************************************
 *        Play item at index (Public)
 **************************************************************************/
int libvlc_media_list_player_play_item_at_index(libvlc_media_list_player_t * p_mlp, int i_index)
{
    libvlc_media_list_lock(p_mlp->p_mlist);
    libvlc_media_list_path_t path = libvlc_media_list_path_with_root_index(i_index);
    libvlc_media_t *new_media = set_current_playing_item(p_mlp, path);
    libvlc_media_list_unlock(p_mlp->p_mlist);

    libvlc_media_player_set_media(p_mlp->p_mi, new_media);
    libvlc_media_release(new_media);
    libvlc_media_player_play(p_mlp->p_mi);

    return new_media ? 0 : -1;
}

/**************************************************************************
 *        Play item (Public)
 **************************************************************************/
int libvlc_media_list_player_play_item(libvlc_media_list_player_t * p_mlp, libvlc_media_t * p_md)
{
    libvlc_media_list_lock(p_mlp->p_mlist);
    libvlc_media_list_path_t path = libvlc_media_list_path_of_item(p_mlp->p_mlist, p_md);
    if (!path)
    {
        libvlc_printerr("Item not found in media list");
        libvlc_media_list_unlock(p_mlp->p_mlist);
        return -1;
    }

    libvlc_media_t *new_media = set_current_playing_item(p_mlp, path);
    libvlc_media_list_unlock(p_mlp->p_mlist);

    libvlc_media_player_set_media(p_mlp->p_mi, new_media);
    libvlc_media_release(new_media);
    libvlc_media_player_play(p_mlp->p_mi);

    return new_media ? 0 : -1;
}

/**************************************************************************
 *       Stop (Private)
 *
 * Lock must be held.
 **************************************************************************/
static void stop(libvlc_media_list_player_t * p_mlp)
{
    libvlc_media_player_stop_async(p_mlp->p_mi);

    free(p_mlp->current_playing_item_path);
    p_mlp->current_playing_item_path = NULL;
}

/**************************************************************************
 *       Stop (Public)
 **************************************************************************/
void libvlc_media_list_player_stop_async(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    stop(p_mlp);
    unlock(p_mlp);
}

/**************************************************************************
 *       Set relative playlist position and play (Private)
 *
 * Sets the currently played item to the given relative play item position
 * (based on the currently playing item) and then begins the new item playback.
 * Lock must be held.
 **************************************************************************/
static int set_relative_playlist_position_and_play(
                                      libvlc_media_list_player_t * p_mlp,
                                      bool next)
{
    if (!p_mlp->p_mlist)
    {
        libvlc_printerr("No media list");
        return -1;
    }

    libvlc_media_list_lock(p_mlp->p_mlist);

    libvlc_media_list_path_t path = p_mlp->current_playing_item_path;
    libvlc_media_t *new_media;

    if(p_mlp->e_playback_mode != libvlc_playback_mode_repeat)
    {
        bool b_loop = (p_mlp->e_playback_mode == libvlc_playback_mode_loop);

        if (next)
        {
            path = get_next_path(p_mlp, b_loop);
            new_media = set_current_playing_item(p_mlp, path);
        }
        else
        {
            path = get_previous_path(p_mlp, b_loop);
            new_media = set_current_playing_item(p_mlp, path);
        }
    }
    else
    {
        new_media = set_current_playing_item(p_mlp, path);
    }

#ifdef DEBUG_MEDIA_LIST_PLAYER
    printf("Playing:");
    libvlc_media_list_path_dump(path);
#endif

    libvlc_media_list_unlock(p_mlp->p_mlist);

    if (!new_media)
        return -1;

    libvlc_media_player_set_media(p_mlp->p_mi, new_media);
    libvlc_media_release(new_media);
    libvlc_media_player_play(p_mlp->p_mi);

    return 0;
}

/**************************************************************************
 *       Next (Public)
 **************************************************************************/
int libvlc_media_list_player_next(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    int failure = set_relative_playlist_position_and_play(p_mlp, true);
    unlock(p_mlp);
    return failure;
}

/**************************************************************************
 *       Previous (Public)
 **************************************************************************/
int libvlc_media_list_player_previous(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    int failure = set_relative_playlist_position_and_play(p_mlp, false);
    unlock(p_mlp);
    return failure;
}

/**************************************************************************
 *       Set Playback Mode (Public)
 **************************************************************************/
void libvlc_media_list_player_set_playback_mode(
                                            libvlc_media_list_player_t * p_mlp,
                                            libvlc_playback_mode_t e_mode )
{
    lock(p_mlp);
    p_mlp->e_playback_mode = e_mode;
    unlock(p_mlp);
}
