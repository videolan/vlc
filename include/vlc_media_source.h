/*****************************************************************************
 * vlc_media_source.h
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

#ifndef VLC_MEDIA_SOURCE_H
#define VLC_MEDIA_SOURCE_H

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_services_discovery.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup media_source Media source
 * \ingroup input
 * @{
 */

/**
 * Media source API aims to manage "services discovery" easily from UI clients.
 *
 * A "media source provider", associated to the libvlc instance, allows to
 * retrieve media sources (each associated to a services discovery module).
 *
 * Requesting a services discovery that is not open will automatically open it.
 * If several "clients" request the same media source (i.e. by requesting the
 * same name), they will receive the same (refcounted) media source instance.
 * As soon as a media source is released by all its clients, the associated
 * services discovery is closed.
 *
 * Each media source holds a media tree, used to store both the media
 * detected by the services discovery and the media detected by preparsing.
 * Clients may listen to the tree to be notified of changes.
 *
 * To preparse a media belonging to a media tree, use vlc_media_tree_Preparse().
 * If subitems are detected during the preparsing, the media tree is updated
 * accordingly.
 */

/**
 * Media tree.
 *
 * Nodes must be traversed with locked held (vlc_media_tree_Lock()).
 */
typedef struct vlc_media_tree {
    input_item_node_t root;
} vlc_media_tree_t;

/**
 * Callbacks to receive media tree events.
 */
struct vlc_media_tree_callbacks
{
    /**
     * Called when the whole content of a subtree has changed.
     *
     * \param playlist the playlist
     * \param node     the node having its children reset (may be root)
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_children_reset)(vlc_media_tree_t *tree, input_item_node_t *node,
                         void *userdata);

    /**
     * Called when children has been added to a node.
     *
     * The children may themselves contain children, which will not be notified
     * separately.
     *
     * \param playlist the playlist
     * \param node     the node having children added
     * \param children the children added
     * \param count    the number of children added
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_children_added)(vlc_media_tree_t *tree, input_item_node_t *node,
                         input_item_node_t *const children[], size_t count,
                         void *userdata);

    /**
     * Called when children has been removed from a node.
     *
     * \param playlist the playlist
     * \param node     the node having children removed
     * \param children the children removed
     * \param count    the number of children removed
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_children_removed)(vlc_media_tree_t *tree, input_item_node_t *node,
                           input_item_node_t *const children[], size_t count,
                           void *userdata);

    /**
     * Called when the preparsing of a node is complete
     *
     * \param tree     the media tree
     * \param node     the node being parsed
     * \param status   the reason for the preparsing termination
     * \param userdata userdata provided to AddListener()
     */
    void
    (*on_preparse_end)(vlc_media_tree_t *tree, input_item_node_t * node,
                       enum input_item_preparse_status status,
                       void *userdata);
};

/**
 * Listener for media tree events.
 */
typedef struct vlc_media_tree_listener_id vlc_media_tree_listener_id;

/**
 * Add a listener. The lock must NOT be held.
 *
 * \param tree                 the media tree, unlocked
 * \param cbs                  the callbacks (must be valid until the listener
 *                             is removed)
 * \param userdata             userdata provided as a parameter in callbacks
 * \param notify_current_state true to notify the current state immediately via
 *                             callbacks
 */
VLC_API vlc_media_tree_listener_id *
vlc_media_tree_AddListener(vlc_media_tree_t *tree,
                           const struct vlc_media_tree_callbacks *cbs,
                           void *userdata, bool notify_current_state);

/**
 * Remove a listener. The lock must NOT be held.
 *
 * \param tree     the media tree, unlocked
 * \param listener the listener identifier returned by
 *                 vlc_media_tree_AddListener()
 */
VLC_API void
vlc_media_tree_RemoveListener(vlc_media_tree_t *tree,
                              vlc_media_tree_listener_id *listener);

/**
 * Lock the media tree (non-recursive).
 */
VLC_API void
vlc_media_tree_Lock(vlc_media_tree_t *);

/**
 * Unlock the media tree.
 */
VLC_API void
vlc_media_tree_Unlock(vlc_media_tree_t *);

/**
 * Find the node containing the requested input item (and its parent).
 *
 * \param tree the media tree, locked
 * \param result point to the matching node if the function returns true [OUT]
 * \param result_parent if not NULL, point to the matching node parent
 *                      if the function returns true [OUT]
 *
 * \retval true if item was found
 * \retval false if item was not found
 */
VLC_API bool
vlc_media_tree_Find(vlc_media_tree_t *tree, const input_item_t *media,
                    input_item_node_t **result,
                    input_item_node_t **result_parent);

/**
 * Preparse a media, and expand it in the media tree on subitems added.
 *
 * \param tree   the media tree (not necessarily locked)
 * \param libvlc the libvlc instance
 * \param media  the media to preparse
 * \param id     a task identifier
 */
VLC_API void
vlc_media_tree_Preparse(vlc_media_tree_t *tree, libvlc_int_t *libvlc,
                        input_item_t *media, void *id);


/**
 * Cancel a media tree preparse request
 *
 * \param libvlc the libvlc instance
 * \param id the preparse task id
 */
VLC_API void
vlc_media_tree_PreparseCancel(libvlc_int_t *libvlc, void* id);

/**
 * Media source.
 *
 * A media source is associated to a "service discovery". It stores the
 * detected media in a media tree.
 */
typedef struct vlc_media_source_t
{
    vlc_media_tree_t *tree;
    const char *description;
} vlc_media_source_t;

/**
 * Increase the media source reference count.
 */
VLC_API void
vlc_media_source_Hold(vlc_media_source_t *);

/**
 * Decrease the media source reference count.
 *
 * Destroy the media source and close the associated "service discovery" if it
 * reaches 0.
 */
VLC_API void
vlc_media_source_Release(vlc_media_source_t *);

/**
 * Media source provider (opaque pointer), used to get media sources.
 */
typedef struct vlc_media_source_provider_t vlc_media_source_provider_t;

/**
 * Return the media source provider associated to the libvlc instance.
 */
VLC_API vlc_media_source_provider_t *
vlc_media_source_provider_Get(libvlc_int_t *);

/**
 * Return the media source identified by psz_name.
 *
 * The resulting media source must be released by vlc_media_source_Release().
 */
VLC_API vlc_media_source_t *
vlc_media_source_provider_GetMediaSource(vlc_media_source_provider_t *,
                                         const char *name);

/**
 * Structure containing the description of a media source.
 */
struct vlc_media_source_meta
{
    char *name;
    char *longname;
    enum services_discovery_category_e category;
};

/** List of media source metadata (opaque). */
typedef struct vlc_media_source_meta_list vlc_media_source_meta_list_t;

/**
 * Return the list of metadata of available media sources.
 *
 * If category is not 0, then only media sources for the requested category are
 * listed.
 *
 * The result must be deleted by vlc_media_source_meta_list_Delete() (if not
 * null).
 *
 * Return NULL either on error or on empty list (this is due to the behavior
 * of the underlying vlc_sd_GetNames()).
 *
 * \param provider the media source provider
 * \param category the category to list (0 for all)
 */
VLC_API vlc_media_source_meta_list_t *
vlc_media_source_provider_List(vlc_media_source_provider_t *,
                               enum services_discovery_category_e category);

/**
 * Return the number of items in the list.
 */
VLC_API size_t
vlc_media_source_meta_list_Count(vlc_media_source_meta_list_t *);

/**
 * Return the item at index.
 */
VLC_API struct vlc_media_source_meta *
vlc_media_source_meta_list_Get(vlc_media_source_meta_list_t *, size_t index);

/**
 * Delete the list.
 *
 * Any struct vlc_media_source_meta retrieved from this list become invalid
 * after this call.
 */
VLC_API void
vlc_media_source_meta_list_Delete(vlc_media_source_meta_list_t *);

/** @} */

#ifdef __cplusplus
}
#endif

#endif

