/*****************************************************************************
 * vlc_playlist.h
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#ifndef VLC_PLAYLIST_NEW_H
#define VLC_PLAYLIST_NEW_H

#include <vlc_common.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \defgroup playlist VLC playlist
 * \ingroup interface
 *
 * A VLC playlist contains a list of "playlist items".
 *
 * Each playlist item contains exactly one media (input item). In the future,
 * it might contain associated data.
 *
 * The API is intended to be simple, UI-friendly and allow for an
 * implementation both correct (no race conditions) and performant for common
 * use cases.
 *
 * UI frameworks typically use "list models" to provide a list of items to a
 * list view component. A list model requires to implement functions to:
 *  - return the total number of items,
 *  - return the item at a given index.
 *
 * In addition, it must notify the view when changes occur when:
 *  - items are inserted (providing index and count),
 *  - items are removed (providing index and count),
 *  - items are moved (providing index, count, and target index),
 *  - items are updated (providing index and count),
 *  - the model is reset (the whole content should be considered changed).
 *
 * The API directly exposes what list models require.
 *
 * The core playlist may be modified from any thread, so it may not be used as
 * a direct data source for a list model. In other word, the functions of a
 * list model must not delegate the calls to the playlist. This would require
 * locking the playlist individually for each call to get the count and
 * retrieve each item (which is, in itself, not a good idea for UI
 * responsiveness), and would not be sufficient to guarantee correctness: the
 * playlist content could change between view calls so that a request to
 * retrieve an item at a specific index could be invalid (which would break the
 * list model expected behavior).
 *
 * As a consequence, the UI playlist should be considered as a remote
 * out-of-sync view of the core playlist. This implies that the UI needs to
 * keep a copy of the playlist content.
 *
 * Note that the copy must not limited to the list of playlist items (pointers)
 * themselves, but also to the items content which is displayed and susceptible
 * to change asynchronously (e.g. media metadata, like title or duration). The
 * UI should never lock a media (input item) for rendering a playlist item;
 * otherwise, the content could be changed (and exposed) before the list model
 * notified the view of this change (which, again, would break the list model
 * expected behavior).
 *
 * It is very important that the copy hold by the UI is only modified through
 * the core playlist callbacks, to guarantee that the indexes notified are
 * valid in the context of the list model. In other words, from the client, the
 * playlist copy is a read-only "desynchronized" view of the core playlist.
 *
 * Moreover, the events triggered by the playlist must be kept in order until
 * they are handled. The callbacks may be called from any thread, with lock
 * held (in practice, the thread from which a change is requested). An UI will
 * typically need to handle the events in the UI thread, so it will usually
 * post the events in an even loop, to handle them from the UI thread. In that
 * case, be careful to always post the events in the event loop, even if the
 * current thread is already the UI thread, not to break the order of events.
 *
 * The playlist also handles the playback order and the repeat mode. It also
 * manages a cursor to the "current" item, and expose whether a previous and
 * next items (which depend on the playback order and repeat mode) are
 * available.
 *
 * When a user requests to insert, move or remove items, or to set the current
 * item, before the core playlist lock is successfully acquired, another client
 * may have changed the list. Therefore, vlc_playlist_Request*() functions are
 * exposed to resolve potential conflicts and apply the changes. The actual
 * changes applied are notified through the callbacks
 *
 * @{
 */

/* forward declarations */
typedef struct input_item_t input_item_t;
typedef struct vlc_player_t vlc_player_t;

/* opaque types */
typedef struct vlc_playlist vlc_playlist_t;
typedef struct vlc_playlist_item vlc_playlist_item_t;
typedef struct vlc_playlist_listener_id vlc_playlist_listener_id;

enum vlc_playlist_playback_repeat
{
    VLC_PLAYLIST_PLAYBACK_REPEAT_NONE,
    VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT,
    VLC_PLAYLIST_PLAYBACK_REPEAT_ALL,
};

enum vlc_playlist_playback_order
{
    VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL,
    VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM,
};

enum vlc_playlist_sort_key
{
    VLC_PLAYLIST_SORT_KEY_TITLE,
    VLC_PLAYLIST_SORT_KEY_DURATION,
    VLC_PLAYLIST_SORT_KEY_ARTIST,
    VLC_PLAYLIST_SORT_KEY_ALBUM,
    VLC_PLAYLIST_SORT_KEY_ALBUM_ARTIST,
    VLC_PLAYLIST_SORT_KEY_GENRE,
    VLC_PLAYLIST_SORT_KEY_DATE,
    VLC_PLAYLIST_SORT_KEY_TRACK_NUMBER,
    VLC_PLAYLIST_SORT_KEY_DISC_NUMBER,
    VLC_PLAYLIST_SORT_KEY_URL,
    VLC_PLAYLIST_SORT_KEY_RATING,
};

enum vlc_playlist_sort_order
{
    VLC_PLAYLIST_SORT_ORDER_ASCENDING,
    VLC_PLAYLIST_SORT_ORDER_DESCENDING,
};

struct vlc_playlist_sort_criterion
{
    enum vlc_playlist_sort_key key;
    enum vlc_playlist_sort_order order;
};

/**
 * Playlist callbacks.
 *
 * A client may register a listener using vlc_playlist_AddListener() to listen
 * playlist events.
 *
 * All callbacks are called with the playlist locked (see vlc_playlist_Lock()).
 */
struct vlc_playlist_callbacks
{
    /**
     * Called when the whole content has changed (e.g. when the playlist has
     * been cleared, shuffled or sorted).
     *
     * \param playlist the playlist
     * \param items    the whole new content of the playlist
     * \param count    the number of items
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_items_reset)(vlc_playlist_t *, vlc_playlist_item_t *const items[],
                      size_t count, void *userdata);

    /**
     * Called when items have been added to the playlist.
     *
     * \param playlist the playlist
     * \param index    the index of the insertion
     * \param items    the array of added items
     * \param count    the number of items added
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_items_added)(vlc_playlist_t *playlist, size_t index,
                      vlc_playlist_item_t *const items[], size_t count,
                      void *userdata);

    /**
     * Called when a slice of items have been moved.
     *
     * \param playlist the playlist
     * \param index    the index of the first moved item
     * \param count    the number of items moved
     * \param target   the new index of the moved slice
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_items_moved)(vlc_playlist_t *playlist, size_t index, size_t count,
                      size_t target, void *userdata);
    /**
     * Called when a slice of items have been removed from the playlist.
     *
     * \param playlist the playlist
     * \param index    the index of the first removed item
     * \param count    the number of items removed
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_items_removed)(vlc_playlist_t *playlist, size_t index, size_t count,
                        void *userdata);

    /**
     * Called when an item has been updated via (pre-)parsing.
     *
     * \param playlist the playlist
     * \param index    the index of the first updated item
     * \param items    the array of updated items
     * \param count    the number of items updated
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_items_updated)(vlc_playlist_t *playlist, size_t index,
                        vlc_playlist_item_t *const items[], size_t count,
                        void *userdata);

    /**
     * Called when the playback repeat mode has been changed.
     *
     * \param playlist the playlist
     * \param repeat   the new playback "repeat" mode
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_playback_repeat_changed)(vlc_playlist_t *playlist,
                                  enum vlc_playlist_playback_repeat repeat,
                                  void *userdata);

    /**
     * Called when the playback order mode has been changed.
     *
     * \param playlist the playlist
     * \param rorder   the new playback order
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_playback_order_changed)(vlc_playlist_t *playlist,
                                 enum vlc_playlist_playback_order order,
                                 void *userdata);

    /**
     * Called when the current item index has changed.
     *
     * Note that the current item index may have changed while the current item
     * is still the same: it may have been moved.
     *
     * \param playlist the playlist
     * \param index    the new current index (-1 if there is no current item)
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_current_index_changed)(vlc_playlist_t *playlist, ssize_t index,
                                void *userdata);

    /**
     * Called when the "has previous item" property has changed.
     *
     * This is typically useful to update any "previous" button in the UI.
     *
     * \param playlist the playlist
     * \param has_prev true if there is a previous item, false otherwise
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_has_prev_changed)(vlc_playlist_t *playlist, bool has_prev,
                           void *userdata);

    /**
     * Called when the "has next item" property has changed.
     *
     * This is typically useful to update any "next" button in the UI.
     *
     * \param playlist the playlist
     * \param has_next true if there is a next item, false otherwise
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_has_next_changed)(vlc_playlist_t *playlist,
                           bool has_next, void *userdata);
};

/* Playlist items */

/**
 * Hold a playlist item.
 *
 * Increment the refcount of the playlist item.
 */
VLC_API void
vlc_playlist_item_Hold(vlc_playlist_item_t *);

/**
 * Release a playlist item.
 *
 * Decrement the refcount of the playlist item, and destroy it if necessary.
 */
VLC_API void
vlc_playlist_item_Release(vlc_playlist_item_t *);

/**
 * Return the media associated to the playlist item.
 */
VLC_API input_item_t *
vlc_playlist_item_GetMedia(vlc_playlist_item_t *);

/**
 * Return a unique id for the playlist item instance.
 */
VLC_API uint64_t
vlc_playlist_item_GetId(vlc_playlist_item_t *);

/* Playlist */

/**
 * Create a new playlist.
 *
 * \param parent   a VLC object
 * \return a pointer to a valid playlist instance, or NULL if an error occurred
 */
VLC_API VLC_USED vlc_playlist_t *
vlc_playlist_New(vlc_object_t *parent);

/**
 * Delete a playlist.
 *
 * All playlist items are released, and listeners are removed and destroyed.
 */
VLC_API void
vlc_playlist_Delete(vlc_playlist_t *);

/**
 * Lock the playlist/player.
 *
 * The playlist and its player share the same lock, to avoid lock-order
 * inversion issues.
 *
 * \warning Do not forget that the playlist and player lock are the same (or
 * you could lock twice the same and deadlock).
 *
 * Almost all playlist functions must be called with lock held (check their
 * description).
 *
 * The lock is not recursive.
 */
VLC_API void
vlc_playlist_Lock(vlc_playlist_t *);

/**
 * Unlock the playlist/player.
 */
VLC_API void
vlc_playlist_Unlock(vlc_playlist_t *);

/**
 * Add a playlist listener.
 *
 * Return an opaque listener identifier, to be passed to
 * vlc_player_RemoveListener().
 *
 * If notify_current_state is true, the callbacks are called once with the
 * current state of the playlist. This is useful because when a client
 * registers to the playlist, it may already contain items. Calling callbacks
 * is a convenient way to initialize the client automatically.
 *
 * \param playlist             the playlist, locked
 * \param cbs                  the callbacks (must be valid until the listener
 *                             is removed)
 * \param userdata             userdata provided as a parameter in callbacks
 * \param notify_current_state true to notify the current state immediately via
 *                             callbacks
 * \return a listener identifier, or NULL if an error occurred
 */
VLC_API VLC_USED vlc_playlist_listener_id *
vlc_playlist_AddListener(vlc_playlist_t *playlist,
                         const struct vlc_playlist_callbacks *cbs,
                         void *userdata, bool notify_current_state);

/**
 * Remove a player listener.
 *
 * \param playlist the playlist, locked
 * \param id       the listener identifier returned by
 *                 vlc_playlist_AddListener()
 */
VLC_API void
vlc_playlist_RemoveListener(vlc_playlist_t *, vlc_playlist_listener_id *);

/**
 * Return the number of items.
 *
 * \param playlist the playlist, locked
 */
VLC_API size_t
vlc_playlist_Count(vlc_playlist_t *playlist);

/**
 * Return the item at a given index.
 *
 * The index must be in range (less than vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index
 * \return the playlist item
 */
VLC_API vlc_playlist_item_t *
vlc_playlist_Get(vlc_playlist_t *playlist, size_t index);

/**
 * Clear the playlist.
 *
 * \param playlist the playlist, locked
 */
VLC_API void
vlc_playlist_Clear(vlc_playlist_t *playlist);

/**
 * Insert a list of media at a given index.
 *
 * The index must be in range (less than or equal to vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \index index    the index where the media are to be inserted
 * \param media    the array of media to insert
 * \param count    the number of media to insert
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_Insert(vlc_playlist_t *playlist, size_t index,
                    input_item_t *const media[], size_t count);

/**
 * Insert a media at a given index.
 *
 * The index must be in range (less than or equal to vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \index index    the index where the media is to be inserted
 * \param media    the media to insert
 * \return VLC_SUCCESS on success, another value on error
 */
static inline int
vlc_playlist_InsertOne(vlc_playlist_t *playlist, size_t index,
                       input_item_t *media)
{
    return vlc_playlist_Insert(playlist, index, &media, 1);
}

/**
 * Add a list of media at the end of the playlist.
 *
 * \param playlist the playlist, locked
 * \param media    the array of media to append
 * \param count    the number of media to append
 * \return VLC_SUCCESS on success, another value on error
 */
static inline int
vlc_playlist_Append(vlc_playlist_t *playlist, input_item_t *const media[],
                    size_t count)
{
    size_t size = vlc_playlist_Count(playlist);
    return vlc_playlist_Insert(playlist, size, media, count);
}

/**
 * Add a media at the end of the playlist.
 *
 * \param playlist the playlist, locked
 * \param media    the media to append
 * \return VLC_SUCCESS on success, another value on error
 */
static inline int
vlc_playlist_AppendOne(vlc_playlist_t *playlist, input_item_t *media)
{
    return vlc_playlist_Append(playlist, &media, 1);
}

/**
 * Move a slice of items to a given target index.
 *
 * The slice and the target must be in range (both index+count and target+count
 * less than or equal to vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index of the first item to move
 * \param count    the number of items to move
 * \param target   the new index of the moved slice
 */
VLC_API void
vlc_playlist_Move(vlc_playlist_t *playlist, size_t index, size_t count,
                  size_t target);

/**
 * Move an item to a given target index.
 *
 * The index and the target must be in range (index less than, and target less
 * than or equal to, vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index of the item to move
 * \param target   the new index of the moved item
 */
static inline void
vlc_playlist_MoveOne(vlc_playlist_t *playlist, size_t index, size_t target)
{
    vlc_playlist_Move(playlist, index, 1, target);
}

/**
 * Remove a slice of items at a given index.
 *
 * The slice must be in range (index+count less than or equal to
 * vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index of the first item to remove
 * \param count    the number of items to remove
 */
VLC_API void
vlc_playlist_Remove(vlc_playlist_t *playlist, size_t index, size_t count);

/**
 * Remove an item at a given index.
 *
 * The index must be in range (less than vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index of the item to remove
 */
static inline void
vlc_playlist_RemoveOne(vlc_playlist_t *playlist, size_t index)
{
    vlc_playlist_Remove(playlist, index, 1);
}

/**
 * Insert a list of media at a given index (if in range), or append.
 *
 * Contrary to vlc_playlist_Insert(), the index need not be in range: if it is
 * out of bounds, items will be appended.
 *
 * This is an helper to apply a desynchronized insert request, i.e. the
 * playlist content may have changed since the request had been submitted.
 * This is typically the case for user requests (e.g. from UI), because the
 * playlist lock has to be acquired *after* the user requested the
 * change.
 *
 * \param playlist the playlist, locked
 * \index index    the index where the media are to be inserted
 * \param media    the array of media to insert
 * \param count    the number of media to insert
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_RequestInsert(vlc_playlist_t *playlist, size_t index,
                           input_item_t *const media[], size_t count);

/**
 * Move a slice of items by value.
 *
 * If the indices are known, use vlc_playlist_Move() instead.
 *
 * This is an helper to apply a desynchronized move request, i.e. the playlist
 * content may have changed since the request had been submitted. This is
 * typically the case for user requests (e.g. from UI), because the playlist
 * lock has to be acquired *after* the user requested the change.
 *
 * For optimization purpose, it is possible to pass an `index_hint`, which is
 * the expected index of the first item of the slice (as known by the client).
 * Hopefully, the index should often match, since conflicts are expected to be
 * rare. Pass -1 not to pass any hint.
 *
 * \param playlist   the playlist, locked
 * \param items      the array of items to move
 * \param count      the number of items to move
 * \param target     the new index of the moved slice
 * \param index_hint the expected index of the first item (-1 for none)
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_RequestMove(vlc_playlist_t *playlist,
                         vlc_playlist_item_t *const items[], size_t count,
                         size_t target, ssize_t index_hint);

/**
 * Remove a slice of items by value.
 *
 * If the indices are known, use vlc_playlist_Remove() instead.
 *
 * This is an helper to apply a desynchronized remove request, i.e. the
 * playlist content may have changed since the request had been submitted.
 * This is typically the case for user requests (e.g. from UI), because the
 * playlist lock has to be acquired *after* the user requested the change.
 *
 * For optimization purpose, it is possible to pass an `index_hint`, which is
 * the expected index of the first item of the slice (as known by the client).
 * Hopefully, the index should often match, since conflicts are expected to be
 * rare. Pass -1 not to pass any hint.
 *
 * \param playlist   the playlist, locked
 * \param items      the array of items to remove
 * \param count      the number of items to remove
 * \param index_hint the expected index of the first item (-1 for none)
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_RequestRemove(vlc_playlist_t *playlist,
                           vlc_playlist_item_t *const items[], size_t count,
                           ssize_t index_hint);

/**
 * Shuffle the playlist.
 *
 * \param playlist the playlist, locked
 */
VLC_API void
vlc_playlist_Shuffle(vlc_playlist_t *playlist);

/**
 * Sort the playlist by a list of criteria.
 *
 * \param playlist the playlist, locked
 * \param criteria the sort criteria (in order)
 * \param count    the number of criteria
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_Sort(vlc_playlist_t *playlist,
                  const struct vlc_playlist_sort_criterion criteria[],
                  size_t count);

/**
 * Return the index of a given item.
 *
 * \param playlist the playlist, locked
 * \param item     the item to locate
 * \return the index of the item (-1 if not found)
 */
VLC_API ssize_t
vlc_playlist_IndexOf(vlc_playlist_t *playlist, const vlc_playlist_item_t *item);

/**
 * Return the index of a given media.
 *
 * \param playlist the playlist, locked
 * \param media    the media to locate
 * \return the index of the playlist item containing the media (-1 if not found)
 */
VLC_API ssize_t
vlc_playlist_IndexOfMedia(vlc_playlist_t *playlist, const input_item_t *media);

/**
 * Return the index of a given item id.
 *
 * \param playlist the playlist, locked
 * \param id       the id to locate
 * \return the index of the playlist item having the id (-1 if not found)
 */
VLC_API ssize_t
vlc_playlist_IndexOfId(vlc_playlist_t *playlist, uint64_t id);

/**
 * Return the playback "repeat" mode.
 *
 * \param playlist the playlist, locked
 * \return the playback "repeat" mode
 */
VLC_API enum vlc_playlist_playback_repeat
vlc_playlist_GetPlaybackRepeat(vlc_playlist_t *playlist);

/**
 * Return the playback order.
 *
 * \param playlist the playlist, locked
 * \return the playback order
 */
VLC_API enum vlc_playlist_playback_order
vlc_playlist_GetPlaybackOrder(vlc_playlist_t *);

/**
 * Change the playback "repeat" mode.
 *
 * \param playlist the playlist, locked
 * \param repeat the new playback "repeat" mode
 */
VLC_API void
vlc_playlist_SetPlaybackRepeat(vlc_playlist_t *playlist,
                               enum vlc_playlist_playback_repeat repeat);

/**
 * Change the playback order
 *
 * \param playlist the playlist, locked
 * \param repeat the new playback order
 */
VLC_API void
vlc_playlist_SetPlaybackOrder(vlc_playlist_t *playlist,
                              enum vlc_playlist_playback_order order);

/**
 * Return the index of the current item.
 *
 * \param playlist the playlist, locked
 * \return the index of the current item, -1 if none.
 */
VLC_API ssize_t
vlc_playlist_GetCurrentIndex(vlc_playlist_t *playlist);

/**
 * Indicate whether a previous item is available.
 *
 * \param playlist the playlist, locked
 * \retval true if a previous item is available
 * \retval false if no previous item is available
 */
VLC_API bool
vlc_playlist_HasPrev(vlc_playlist_t *playlist);

/**
 * Indicate whether a next item is available.
 *
 * \param playlist the playlist, locked
 * \retval true if a next item is available
 * \retval false if no next item is available
 */
VLC_API bool
vlc_playlist_HasNext(vlc_playlist_t *playlist);

/**
 * Go to the previous item.
 *
 * Return VLC_EGENERIC if vlc_playlist_HasPrev() returns false.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_Prev(vlc_playlist_t *playlist);

/**
 * Go to the next item.
 *
 * Return VLC_EGENERIC if vlc_playlist_HasNext() returns false.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_Next(vlc_playlist_t *playlist);

/**
 * Go to a given index.
 *
 * the index must be -1 or in range (less than vlc_playlist_Count()).
 *
 * \param playlist the playlist, locked
 * \param index    the index to go to (-1 to none)
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_GoTo(vlc_playlist_t *playlist, ssize_t index);

/**
 * Go to a given item.
 *
 * If the index is known, use vlc_playlist_GoTo() instead.
 *
 * This is an helper to apply a desynchronized "go to" request, i.e. the
 * playlist content may have changed since the request had been submitted.
 * This is typically the case for user requests (e.g. from UI), because the
 * playlist lock has to be acquired *after* the user requested the change.
 *
 * For optimization purpose, it is possible to pass an `index_hint`, which is
 * the expected index of the first item of the slice (as known by the client).
 * Hopefully, the index should often match, since conflicts are expected to be
 * rare. Pass -1 not to pass any hint.
 *
 * \param playlist   the playlist, locked
 * \param item       the item to go to (NULL for none)
 * \param index_hint the expected index of the item (-1 for none)
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_RequestGoTo(vlc_playlist_t *playlist, vlc_playlist_item_t *item,
                         ssize_t index_hint);

/**
 * Return the player owned by the playlist.
 *
 * \param playlist the playlist (not necessarily locked)
 * \return the player
 */
VLC_API vlc_player_t *
vlc_playlist_GetPlayer(vlc_playlist_t *playlist);

/**
 * Start the player.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API int
vlc_playlist_Start(vlc_playlist_t *playlist);

/**
 * Stop the player.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API void
vlc_playlist_Stop(vlc_playlist_t *playlist);

/**
 * Pause the player.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API void
vlc_playlist_Pause(vlc_playlist_t *playlist);

/**
 * Resume the player.
 *
 * \param playlist the playlist, locked
 * \return VLC_SUCCESS on success, another value on error
 */
VLC_API void
vlc_playlist_Resume(vlc_playlist_t *playlist);

/**
 * Go to the given index and plays the corresponding item.
 *
 * \param playlist the playlist, locked
 * \param index    the index to play at
 * \return VLC_SUCCESS on success, another value on error
 */
static inline int
vlc_playlist_PlayAt(vlc_playlist_t *playlist, size_t index)
{
    int ret = vlc_playlist_GoTo(playlist, index);
    if (ret != VLC_SUCCESS)
        return ret;
    return vlc_playlist_Start(playlist);
}

/**
 * Preparse a media, and expand it in the playlist on subitems added.
 *
 * \param playlist the playlist (not necessarily locked)
 * \param libvlc the libvlc instance
 * \param media the media to preparse
 */
VLC_API void
vlc_playlist_Preparse(vlc_playlist_t *playlist, input_item_t *media);

/**
 * Export the playlist to a file.
 *
 * \param filename the location where the exported file will be saved
 * \param type the type of the playlist file to create (m3u, m3u8, xspf, ...)
 * \return VLC_SUCCESS on success, another value on error
 */
// XXX use vlc_memstream instead of filename?
VLC_API int
vlc_playlist_Export(vlc_playlist_t *playlist, const char *filename,
                    const char *type);

/** @} */
# ifdef __cplusplus
}
# endif

#endif
