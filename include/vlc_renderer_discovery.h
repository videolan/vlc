/*****************************************************************************
 * vlc_renderer_discovery.h : Renderer Discovery functions
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef VLC_RENDERER_DISCOVERY_H
#define VLC_RENDERER_DISCOVERY_H 1

#include <vlc_input.h>
#include <vlc_probe.h>
#include <vlc_url.h>

/**
 * @defgroup vlc_renderer VLC renderer discovery
 * @ingroup interface
 * @{
 *
 * @file
 * This file declares VLC renderer discvoery structures and functions
 *
 * @defgroup vlc_renderer_item VLC renderer items returned by the discovery
 * @{
 */

#define VLC_RENDERER_CAN_AUDIO 0x0001
#define VLC_RENDERER_CAN_VIDEO 0x0002

/**
 * Create a new renderer item
 *
 * @param psz_type type of the item
 * @param psz_name name of the item
 * @param psz_uri uri of the renderer item, must contains a valid protocol and
 * a valid host
 * @param psz_extra_sout extra sout options
 * @param psz_demux_filter demux filter to use with the renderer
 * @param psz_icon_uri icon uri of the renderer item
 * @param i_flags flags for the item
 * @return a renderer item or NULL in case of error
 */
VLC_API vlc_renderer_item_t *
vlc_renderer_item_new(const char *psz_type, const char *psz_name,
                      const char *psz_uri, const char *psz_extra_sout,
                      const char *psz_demux_filter, const char *psz_icon_uri,
                      int i_flags) VLC_USED;

/**
 * Hold a renderer item, i.e. creates a new reference
 */
VLC_API vlc_renderer_item_t *
vlc_renderer_item_hold(vlc_renderer_item_t *p_item);

/**
 * Releases a renderer item, i.e. decrements its reference counter
 */
VLC_API void
vlc_renderer_item_release(vlc_renderer_item_t *p_item);

/**
 * Get the human readable name of a renderer item
 */
VLC_API const char *
vlc_renderer_item_name(const vlc_renderer_item_t *p_item);

/**
 * Get the type (not translated) of a renderer item. For now, the type can only
 * be "chromecast" ("upnp", "airplay" may come later).
 */
VLC_API const char *
vlc_renderer_item_type(const vlc_renderer_item_t *p_item);

/**
 * Get the demux filter to use with a renderer item
 */
VLC_API const char *
vlc_renderer_item_demux_filter(const vlc_renderer_item_t *p_item);

/**
 * Get the sout command of a renderer item
 */
VLC_API const char *
vlc_renderer_item_sout(const vlc_renderer_item_t *p_item);

/**
 * Get the icon uri of a renderer item
 */
VLC_API const char *
vlc_renderer_item_icon_uri(const vlc_renderer_item_t *p_item);

/**
 * Get the flags of a renderer item
 */
VLC_API int
vlc_renderer_item_flags(const vlc_renderer_item_t *p_item);

/**
 * @}
 * @defgroup vlc_renderer_discovery VLC renderer discovery interface
 * @{
 */

struct vlc_renderer_discovery_owner;

/**
 * Return a list of renderer discovery modules
 *
 * @param pppsz_names a pointer to a list of module name, NULL terminated
 * @param pppsz_longnames a pointer to a list of module longname, NULL
 * terminated
 *
 * @return VLC_SUCCESS on success, or VLC_EGENERIC on error
 */
VLC_API int
vlc_rd_get_names(vlc_object_t *p_obj, char ***pppsz_names,
                 char ***pppsz_longnames) VLC_USED;
#define vlc_rd_get_names(a, b, c) \
        vlc_rd_get_names(VLC_OBJECT(a), b, c)

/**
 * Create a new renderer discovery module
 *
 * @param psz_name name of the module to load, see vlc_rd_get_names() to get
 * the list of names
 *
 * @return a valid vlc_renderer_discovery, need to be released with
 * vlc_rd_release()
 */
VLC_API vlc_renderer_discovery_t *
vlc_rd_new(vlc_object_t *p_obj, const char *psz_name,
           const struct vlc_renderer_discovery_owner *owner) VLC_USED;

VLC_API void vlc_rd_release(vlc_renderer_discovery_t *p_rd);

/**
 * @}
 * @defgroup vlc_renderer_discovery_module VLC renderer module
 * @{
 */

struct vlc_renderer_discovery_owner
{
    void *sys;
    void (*item_added)(struct vlc_renderer_discovery_t *,
                       struct vlc_renderer_item_t *);
    void (*item_removed)(struct vlc_renderer_discovery_t *,
                         struct vlc_renderer_item_t *);
};

struct vlc_renderer_discovery_t
{
    struct vlc_object_t obj;
    module_t *          p_module;

    struct vlc_renderer_discovery_owner owner;

    char *              psz_name;
    config_chain_t *    p_cfg;

    void *p_sys;
};

/**
 * Add a new renderer item
 *
 * This will send the vlc_RendererDiscoveryItemAdded event
 */
static inline void vlc_rd_add_item(vlc_renderer_discovery_t * p_rd,
                                   vlc_renderer_item_t * p_item)
{
    p_rd->owner.item_added(p_rd, p_item);
}

/**
 * Add a new renderer item
 *
 * This will send the vlc_RendererDiscoveryItemRemoved event
 */
static inline void vlc_rd_remove_item(vlc_renderer_discovery_t * p_rd,
                                      vlc_renderer_item_t * p_item)
{
    p_rd->owner.item_removed(p_rd, p_item);
}

/**
 * Renderer Discovery proble helpers
 */
VLC_API int
vlc_rd_probe_add(vlc_probe_t *p_probe, const char *psz_name,
                 const char *psz_longname);

#define VLC_RD_PROBE_HELPER(name, longname) \
static int vlc_rd_probe_open(vlc_object_t *obj) \
{ \
    return vlc_rd_probe_add((struct vlc_probe_t *)obj, name, longname); \
}

#define VLC_RD_PROBE_SUBMODULE \
    add_submodule() \
        set_capability("renderer probe", 100) \
        set_callback(vlc_rd_probe_open)

/** @} @} */

#endif
