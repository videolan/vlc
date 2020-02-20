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
#include <vlc/libvlc_events.h>
#include <assert.h>

#include "libvlc_internal.h"

#include "media_internal.h" // Abuse, could and should be removed
#include "media_list_path.h"

//#define DEBUG_MEDIA_LIST_PLAYER

/* This is a very dummy implementation of playlist on top of
 * media_list and media_player.
 *
 * All this code is doing is simply computing the next item
 * of a tree of media_list (see get_next_index()), and play
 * the next item when the current is over. This is happening
 * via the event callback media_player_reached_end().
 *
 * This is thread safe, and we use a two keys (locks) scheme
 * to discriminate between callbacks and regular uses.
 */

struct libvlc_media_list_player_t
{
    libvlc_event_manager_t      event_manager;
    int                         i_refcount;
    int                         seek_offset;
    bool                        dead;
    /* Protect access to this structure. */
    vlc_mutex_t                 object_lock;
    /* Protect access to this structure and from callback execution. */
    vlc_mutex_t                 mp_callback_lock;
    vlc_cond_t                  seek_pending;
    libvlc_media_list_path_t    current_playing_item_path;
    libvlc_media_t *            p_current_playing_item;
    libvlc_media_list_t *       p_mlist;
    libvlc_media_player_t *     p_mi;
    libvlc_playback_mode_t      e_playback_mode;

    vlc_thread_t                thread;
};

/*
 * Forward declaration
 */

static
int set_relative_playlist_position_and_play(libvlc_media_list_player_t *p_mlp,
                                            int i_relative_position);
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
    vlc_mutex_lock(&p_mlp->object_lock);

    // Make sure no callback will occurs at the same time
    vlc_mutex_lock(&p_mlp->mp_callback_lock);
}

static inline void unlock(libvlc_media_list_player_t * p_mlp)
{
    vlc_mutex_unlock(&p_mlp->mp_callback_lock);
    vlc_mutex_unlock(&p_mlp->object_lock);
}

static inline void assert_locked(libvlc_media_list_player_t * p_mlp)
{
    vlc_mutex_assert(&p_mlp->mp_callback_lock);
    (void) p_mlp;
}

static inline libvlc_event_manager_t * mlist_em(libvlc_media_list_player_t * p_mlp)
{
    assert_locked(p_mlp);
    return libvlc_media_list_event_manager(p_mlp->p_mlist);
}

static inline libvlc_event_manager_t * mplayer_em(libvlc_media_list_player_t * p_mlp)
{
    return libvlc_media_player_event_manager(p_mlp->p_mi);
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
    assert_locked(p_mlp);

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
        return NULL;

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
    assert_locked(p_mlp);

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

static void *playlist_thread(void *data)
{
    libvlc_media_list_player_t *mlp = data;

    vlc_mutex_lock(&mlp->mp_callback_lock);

    while (!mlp->dead)
    {
        if (mlp->seek_offset != 0)
        {
            set_relative_playlist_position_and_play(mlp, mlp->seek_offset);
            mlp->seek_offset = 0;
        }
        vlc_cond_wait(&mlp->seek_pending, &mlp->mp_callback_lock);
    }

    vlc_mutex_unlock(&mlp->mp_callback_lock);
    return NULL;
}

/**************************************************************************
 *       media_player_reached_end (private) (Event Callback)
 **************************************************************************/
static void
media_player_reached_end(const libvlc_event_t * p_event, void * p_user_data)
{
    VLC_UNUSED(p_event);
    libvlc_media_list_player_t * p_mlp = p_user_data;

    /* This event is triggered from the input thread, and changing item in
     * the media player requires the input thread to terminate. So we cannot
     * change the playlist state here (it would cause a deadlock). Instead, we
     * defer to a separate thread. Avoiding this would be nice... */
    vlc_mutex_lock(&p_mlp->mp_callback_lock);
    p_mlp->seek_offset++;
    vlc_cond_signal(&p_mlp->seek_pending);
    vlc_mutex_unlock(&p_mlp->mp_callback_lock);
}

/**************************************************************************
 *       playlist_item_deleted (private) (Event Callback)
 **************************************************************************/
static void
mlist_item_deleted(const libvlc_event_t * p_event, void * p_user_data)
{
    // Nothing to do. For now.
    (void)p_event; (void)p_user_data;
}


/**************************************************************************
 * install_playlist_observer (private)
 **************************************************************************/
static void
install_playlist_observer(libvlc_media_list_player_t * p_mlp)
{
    assert_locked(p_mlp);
    libvlc_event_attach(mlist_em(p_mlp), libvlc_MediaListItemDeleted, mlist_item_deleted, p_mlp);
}

/**************************************************************************
 * uninstall_playlist_observer (private)
 **************************************************************************/
static void
uninstall_playlist_observer(libvlc_media_list_player_t * p_mlp)
{
    assert_locked(p_mlp);
    if (!p_mlp->p_mlist) return;
    libvlc_event_detach(mlist_em(p_mlp), libvlc_MediaListItemDeleted, mlist_item_deleted, p_mlp);
}

/**************************************************************************
 * install_media_player_observer (private)
 **************************************************************************/
static void
install_media_player_observer(libvlc_media_list_player_t * p_mlp)
{
    libvlc_event_attach(mplayer_em(p_mlp), libvlc_MediaPlayerEndReached, media_player_reached_end, p_mlp);
}


/**************************************************************************
 *       uninstall_media_player_observer (private)
 **************************************************************************/
static void
uninstall_media_player_observer(libvlc_media_list_player_t * p_mlp)
{
    assert_locked(p_mlp);

    // Allow callbacks to run, because detach() will wait until all callbacks are processed.
    // This is safe because only callbacks are allowed, and there execution will be cancelled.
    vlc_mutex_unlock(&p_mlp->mp_callback_lock);
    libvlc_event_detach(mplayer_em(p_mlp), libvlc_MediaPlayerEndReached, media_player_reached_end, p_mlp);

    // Now, lock back the callback lock. No more callback will be present from this point.
    vlc_mutex_lock(&p_mlp->mp_callback_lock);

    // What is here is safe, because we guarantee that we won't be able to anything concurrently,
    // - except (cancelled) callbacks - thanks to the object_lock.
}

/**************************************************************************
 *       set_current_playing_item (private)
 *
 * Playlist lock should be held
 **************************************************************************/
static void
set_current_playing_item(libvlc_media_list_player_t * p_mlp, libvlc_media_list_path_t path)
{
    assert_locked(p_mlp);

    /* First, save the new path that we are going to play */
    if (p_mlp->current_playing_item_path != path)
    {
        free(p_mlp->current_playing_item_path);
        p_mlp->current_playing_item_path = path;
    }

    if (!path)
        return;

    libvlc_media_t * p_md;
    p_md = libvlc_media_list_item_at_path(p_mlp->p_mlist, path);
    if (!p_md)
        return;

    /* Make sure media_player_reached_end() won't get called */
    uninstall_media_player_observer(p_mlp);

    libvlc_media_player_set_media(p_mlp->p_mi, p_md);

    install_media_player_observer(p_mlp);
    libvlc_media_release(p_md); /* for libvlc_media_list_item_at_index */
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *         new (Public)
 **************************************************************************/
libvlc_media_list_player_t *
libvlc_media_list_player_new(libvlc_instance_t * p_instance)
{
    libvlc_media_list_player_t * p_mlp;
    p_mlp = calloc( 1, sizeof(libvlc_media_list_player_t) );
    if (unlikely(p_mlp == NULL))
    {
        libvlc_printerr("Not enough memory");
        return NULL;
    }

    p_mlp->i_refcount = 1;
    p_mlp->seek_offset = 0;
    p_mlp->dead = false;
    vlc_mutex_init(&p_mlp->object_lock);
    vlc_mutex_init(&p_mlp->mp_callback_lock);
    vlc_cond_init(&p_mlp->seek_pending);
    libvlc_event_manager_init(&p_mlp->event_manager, p_mlp);

    /* Create the underlying media_player */
    p_mlp->p_mi = libvlc_media_player_new(p_instance);
    if( p_mlp->p_mi == NULL )
        goto error;
    install_media_player_observer(p_mlp);

    if (vlc_clone(&p_mlp->thread, playlist_thread, p_mlp,
                  VLC_THREAD_PRIORITY_LOW))
    {
        libvlc_media_player_release(p_mlp->p_mi);
        goto error;
    }

    return p_mlp;
error:
    libvlc_event_manager_destroy(&p_mlp->event_manager);
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

    lock(p_mlp);
    p_mlp->i_refcount--;
    if (p_mlp->i_refcount > 0)
    {
        unlock(p_mlp);
        return;
    }
    assert(p_mlp->i_refcount == 0);
    unlock(p_mlp);

    vlc_mutex_lock(&p_mlp->mp_callback_lock);
    p_mlp->dead = true;
    vlc_cond_signal(&p_mlp->seek_pending);
    vlc_mutex_unlock(&p_mlp->mp_callback_lock);
    vlc_join(p_mlp->thread, NULL);

    lock(p_mlp);
    /* Keep the lock(), because the uninstall functions
     * check for it. That's convenient. */
    uninstall_media_player_observer(p_mlp);
    libvlc_media_player_release(p_mlp->p_mi);

    if (p_mlp->p_mlist)
    {
        uninstall_playlist_observer(p_mlp);
        libvlc_media_list_release(p_mlp->p_mlist);
    }

    unlock(p_mlp);

    libvlc_event_manager_destroy(&p_mlp->event_manager);
    free(p_mlp->current_playing_item_path);
    free(p_mlp);
}

/**************************************************************************
 *        retain (Public)
 **************************************************************************/
void libvlc_media_list_player_retain(libvlc_media_list_player_t * p_mlp)
{
    if (!p_mlp)
        return;

    lock(p_mlp);
    p_mlp->i_refcount++;
    unlock(p_mlp);
}

/**************************************************************************
 *        event_manager (Public)
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_player_event_manager(libvlc_media_list_player_t * p_mlp)
{
    return &p_mlp->event_manager;
}

/**************************************************************************
 *        set_media_player (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_player(libvlc_media_list_player_t * p_mlp, libvlc_media_player_t * p_mi)
{
    libvlc_media_player_t *p_oldmi;

    assert(p_mi != NULL);
    libvlc_media_player_retain(p_mi);

    lock(p_mlp);
    uninstall_media_player_observer(p_mlp);
    p_oldmi = p_mlp->p_mi;
    p_mlp->p_mi = p_mi;
    install_media_player_observer(p_mlp);
    unlock(p_mlp);

    libvlc_media_player_release(p_oldmi);
}

/**************************************************************************
 *        get_media_player (Public)
 **************************************************************************/
libvlc_media_player_t * libvlc_media_list_player_get_media_player(libvlc_media_list_player_t * p_mlp)
{
    libvlc_media_player_retain(p_mlp->p_mi);
    return p_mlp->p_mi;
}

/**************************************************************************
 *       set_media_list (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_list(libvlc_media_list_player_t * p_mlp, libvlc_media_list_t * p_mlist)
{
    assert (p_mlist);

    lock(p_mlp);
    if (p_mlp->p_mlist)
    {
        uninstall_playlist_observer(p_mlp);
        libvlc_media_list_release(p_mlp->p_mlist);
    }
    libvlc_media_list_retain(p_mlist);
    p_mlp->p_mlist = p_mlist;

    install_playlist_observer(p_mlp);

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
        set_relative_playlist_position_and_play(p_mlp, 1);
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
    lock(p_mlp);
    libvlc_media_list_path_t path = libvlc_media_list_path_with_root_index(i_index);
    set_current_playing_item(p_mlp, path);
    libvlc_media_t *p_md = libvlc_media_player_get_media(p_mlp->p_mi);
    libvlc_media_player_play(p_mlp->p_mi);
    unlock(p_mlp);

    if (!p_md)
        return -1;

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    event.u.media_list_player_next_item_set.item = p_md;
    libvlc_event_send(&p_mlp->event_manager, &event);
    libvlc_media_release(p_md);
    return 0;
}

/**************************************************************************
 *        Play item (Public)
 **************************************************************************/
int libvlc_media_list_player_play_item(libvlc_media_list_player_t * p_mlp, libvlc_media_t * p_md)
{
    lock(p_mlp);
    libvlc_media_list_path_t path = libvlc_media_list_path_of_item(p_mlp->p_mlist, p_md);
    if (!path)
    {
        libvlc_printerr("Item not found in media list");
        unlock(p_mlp);
        return -1;
    }

    set_current_playing_item(p_mlp, path);
    libvlc_media_player_play(p_mlp->p_mi);
    unlock(p_mlp);
    return 0;
}

/**************************************************************************
 *       Stop (Private)
 *
 * Lock must be held.
 **************************************************************************/
static void stop(libvlc_media_list_player_t * p_mlp)
{
    assert_locked(p_mlp);

    /* We are not interested in getting media stop event now */
    uninstall_media_player_observer(p_mlp);
    libvlc_media_player_stop_async(p_mlp->p_mi);
    install_media_player_observer(p_mlp);

    free(p_mlp->current_playing_item_path);
    p_mlp->current_playing_item_path = NULL;

    /* Send the event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerStopped;
    libvlc_event_send(&p_mlp->event_manager, &event);
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
                                      int i_relative_position)
{
    assert_locked(p_mlp);

    if (!p_mlp->p_mlist)
    {
        libvlc_printerr("No media list");
        return -1;
    }

    libvlc_media_list_lock(p_mlp->p_mlist);

    libvlc_media_list_path_t path = p_mlp->current_playing_item_path;

    if(p_mlp->e_playback_mode != libvlc_playback_mode_repeat)
    {
        bool b_loop = (p_mlp->e_playback_mode == libvlc_playback_mode_loop);

        while (i_relative_position > 0)
        {
            path = get_next_path(p_mlp, b_loop);
            set_current_playing_item(p_mlp, path);
            --i_relative_position;
        }

        while (i_relative_position < 0)
        {
            path = get_previous_path(p_mlp, b_loop);
            set_current_playing_item(p_mlp, path);
            ++i_relative_position;
        }
    }
    else
    {
        set_current_playing_item(p_mlp, path);
    }

#ifdef DEBUG_MEDIA_LIST_PLAYER
    printf("Playing:");
    libvlc_media_list_path_dump(path);
#endif

    if (!path)
    {
        libvlc_media_list_unlock(p_mlp->p_mlist);
        /* Send list played event */
        libvlc_event_t event;
        event.type = libvlc_MediaListPlayerPlayed;
        libvlc_event_send(&p_mlp->event_manager, &event);
        return -1;
    }

    libvlc_media_player_play(p_mlp->p_mi);

    libvlc_media_list_unlock(p_mlp->p_mlist);

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    libvlc_media_t * p_md = libvlc_media_list_item_at_path(p_mlp->p_mlist, path);
    event.u.media_list_player_next_item_set.item = p_md;
    libvlc_event_send(&p_mlp->event_manager, &event);
    libvlc_media_release(p_md);
    return 0;
}

/**************************************************************************
 *       Next (Public)
 **************************************************************************/
int libvlc_media_list_player_next(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    int failure = set_relative_playlist_position_and_play(p_mlp, 1);
    unlock(p_mlp);
    return failure;
}

/**************************************************************************
 *       Previous (Public)
 **************************************************************************/
int libvlc_media_list_player_previous(libvlc_media_list_player_t * p_mlp)
{
    lock(p_mlp);
    int failure = set_relative_playlist_position_and_play(p_mlp, -1);
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
