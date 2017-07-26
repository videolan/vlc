/*****************************************************************************
 * libvlc_renderer_discoverer.h:  libvlc external API
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_RENDERER_DISCOVERER_H
#define VLC_LIBVLC_RENDERER_DISCOVERER_H 1

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @defgroup libvlc_renderer_discoverer LibVLC renderer discoverer
 * @ingroup libvlc
 * LibVLC renderer discoverer finds available renderers available on the local
 * network
 * @{
 * @file
 * LibVLC renderer discoverer external API
 */

typedef struct libvlc_renderer_discoverer_t libvlc_renderer_discoverer_t;

/**
 * Renderer discoverer description
 *
 * \see libvlc_renderer_discoverer_list_get()
 */
typedef struct libvlc_rd_description_t
{
    char *psz_name;
    char *psz_longname;
} libvlc_rd_description_t;

/** The renderer can render audio */
#define LIBVLC_RENDERER_CAN_AUDIO 0x0001
/** The renderer can render video */
#define LIBVLC_RENDERER_CAN_VIDEO 0x0002

/**
 * Renderer item
 *
 * This struct is passed by a @ref libvlc_event_t when a new renderer is added
 * or deleted.
 *
 * An item is valid until the @ref libvlc_RendererDiscovererItemDeleted event
 * is called with the same pointer.
 *
 * \see libvlc_renderer_discoverer_event_manager()
 */
typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;


/**
 * Hold a renderer item, i.e. creates a new reference
 *
 * This functions need to called from the libvlc_RendererDiscovererItemAdded
 * callback if the libvlc user wants to use this item after. (for display or
 * for passing it to the mediaplayer for example).
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return the current item
 */
LIBVLC_API libvlc_renderer_item_t *
libvlc_renderer_item_hold(libvlc_renderer_item_t *p_item);

/**
 * Releases a renderer item, i.e. decrements its reference counter
 *
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API void
libvlc_renderer_item_release(libvlc_renderer_item_t *p_item);

/**
 * Get the human readable name of a renderer item
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return the name of the item (can't be NULL, must *not* be freed)
 */
LIBVLC_API const char *
libvlc_renderer_item_name(const libvlc_renderer_item_t *p_item);

/**
 * Get the type (not translated) of a renderer item. For now, the type can only
 * be "chromecast" ("upnp", "airplay" may come later).
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return the type of the item (can't be NULL, must *not* be freed)
 */
LIBVLC_API const char *
libvlc_renderer_item_type(const libvlc_renderer_item_t *p_item);

/**
 * Get the icon uri of a renderer item
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return the uri of the item's icon (can be NULL, must *not* be freed)
 */
LIBVLC_API const char *
libvlc_renderer_item_icon_uri(const libvlc_renderer_item_t *p_item);

/**
 * Get the flags of a renderer item
 *
 * \see LIBVLC_RENDERER_CAN_AUDIO
 * \see LIBVLC_RENDERER_CAN_VIDEO
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return bitwise flag: capabilities of the renderer, see
 */
LIBVLC_API int
libvlc_renderer_item_flags(const libvlc_renderer_item_t *p_item);

/**
 * Create a renderer discoverer object by name
 *
 * After this object is created, you should attach to events in order to be
 * notified of the discoverer events.
 *
 * You need to call libvlc_renderer_discoverer_start() in order to start the
 * discovery.
 *
 * \see libvlc_renderer_discoverer_event_manager()
 * \see libvlc_renderer_discoverer_start()
 *
 * \version LibVLC 3.0.0 or later
 *
 * \param p_inst libvlc instance
 * \param psz_name service name; use libvlc_renderer_discoverer_list_get() to
 * get a list of the discoverer names available in this libVLC instance
 * \return media discover object or NULL in case of error
 */
LIBVLC_API libvlc_renderer_discoverer_t *
libvlc_renderer_discoverer_new( libvlc_instance_t *p_inst,
                                const char *psz_name );

/**
 * Release a renderer discoverer object
 *
 * \version LibVLC 3.0.0 or later
 *
 * \param p_rd renderer discoverer object
 */
LIBVLC_API void
libvlc_renderer_discoverer_release( libvlc_renderer_discoverer_t *p_rd );

/**
 * Start renderer discovery
 *
 * To stop it, call libvlc_renderer_discoverer_stop() or
 * libvlc_renderer_discoverer_release() directly.
 *
 * \see libvlc_renderer_discoverer_stop()
 *
 * \version LibVLC 3.0.0 or later
 *
 * \param p_rd renderer discoverer object
 * \return -1 in case of error, 0 otherwise
 */
LIBVLC_API int
libvlc_renderer_discoverer_start( libvlc_renderer_discoverer_t *p_rd );

/**
 * Stop renderer discovery.
 *
 * \see libvlc_renderer_discoverer_start()
 *
 * \version LibVLC 3.0.0 or later
 *
 * \param p_rd renderer discoverer object
 */
LIBVLC_API void
libvlc_renderer_discoverer_stop( libvlc_renderer_discoverer_t *p_rd );

/**
 * Get the event manager of the renderer discoverer
 *
 * The possible events to attach are @ref libvlc_RendererDiscovererItemAdded
 * and @ref libvlc_RendererDiscovererItemDeleted.
 *
 * The @ref libvlc_renderer_item_t struct passed to event callbacks is owned by
 * VLC, users should take care of holding/releasing this struct for their
 * internal usage.
 *
 * \see libvlc_event_t.u.renderer_discoverer_item_added.item
 * \see libvlc_event_t.u.renderer_discoverer_item_removed.item
 *
 * \version LibVLC 3.0.0 or later
 *
 * \return a valid event manager (can't fail)
 */
LIBVLC_API libvlc_event_manager_t *
libvlc_renderer_discoverer_event_manager( libvlc_renderer_discoverer_t *p_rd );

/**
 * Get media discoverer services
 *
 * \see libvlc_renderer_list_release()
 *
 * \version LibVLC 3.0.0 and later
 *
 * \param p_inst libvlc instance
 * \param ppp_services address to store an allocated array of renderer
 * discoverer services (must be freed with libvlc_renderer_list_release() by
 * the caller) [OUT]
 *
 * \return the number of media discoverer services (0 on error)
 */
LIBVLC_API size_t
libvlc_renderer_discoverer_list_get( libvlc_instance_t *p_inst,
                                     libvlc_rd_description_t ***ppp_services );

/**
 * Release an array of media discoverer services
 *
 * \see libvlc_renderer_discoverer_list_get()
 *
 * \version LibVLC 3.0.0 and later
 *
 * \param pp_services array to release
 * \param i_count number of elements in the array
 */
LIBVLC_API void
libvlc_renderer_discoverer_list_release( libvlc_rd_description_t **pp_services,
                                         size_t i_count );

/** @} */

# ifdef __cplusplus
}
# endif

#endif
