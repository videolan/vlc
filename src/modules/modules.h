/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

#ifndef LIBVLC_MODULES_H
# define LIBVLC_MODULES_H 1

# include <vlc_atomic.h>

/** The plugin handle type */
typedef void *module_handle_t;

/** VLC plugin */
typedef struct vlc_plugin_t
{
    struct vlc_plugin_t *next;
    module_t *module;
    unsigned modules_count;

    const char *textdomain; /**< gettext domain (or NULL) */

    /**
     * Variables set by the module to store its config options
     */
    struct
    {
        module_config_t *items; /**< Table of configuration parameters */
        size_t size; /**< Size of items table */
        size_t count; /**< Number of configuration items */
        size_t booleans; /**< Number of booleal config items */
    } conf;

#ifdef HAVE_DYNAMIC_PLUGINS
    atomic_bool loaded; /**< Whether the plug-in is mapped in memory */
    bool unloadable; /**< Whether the plug-in can be unloaded safely */
    module_handle_t handle; /**< Run-time linker handle (if loaded) */
    char *abspath; /**< Absolute path */

    char *path; /**< Relative path (within plug-in directory) */
    int64_t mtime; /**< Last modification time */
    uint64_t size; /**< File size */
#endif
} vlc_plugin_t;

/**
 * List of all plug-ins.
 */
extern struct vlc_plugin_t *vlc_plugins;

#define MODULE_SHORTCUT_MAX 20

/** Plugin entry point prototype */
typedef int (*vlc_plugin_cb) (int (*)(void *, void *, int, ...), void *);

/** Core module */
int vlc_entry__core (int (*)(void *, void *, int, ...), void *);

/**
 * Internal module descriptor
 */
struct module_t
{
    vlc_plugin_t *plugin; /**< Plug-in/library containing the module */
    module_t   *next;

    /** Shortcuts to the module */
    unsigned    i_shortcuts;
    const char **pp_shortcuts;

    /*
     * Variables set by the module to identify itself
     */
    const char *psz_shortname;                              /**< Module name */
    const char *psz_longname;                   /**< Module descriptive name */
    const char *psz_help;        /**< Long help string for "special" modules */

    const char *psz_capability;                              /**< Capability */
    int      i_score;                          /**< Score for the capability */

    /* Callbacks */
    const char *activate_name;
    const char *deactivate_name;
    void *pf_activate;
    void *pf_deactivate;
};

vlc_plugin_t *vlc_plugin_create(void);
void vlc_plugin_destroy(vlc_plugin_t *);
module_t *vlc_module_create(vlc_plugin_t *);
void vlc_module_destroy (module_t *);

vlc_plugin_t *vlc_plugin_describe(vlc_plugin_cb);
int vlc_plugin_resolve(vlc_plugin_t *, vlc_plugin_cb);

void module_InitBank (void);
size_t module_LoadPlugins( vlc_object_t * );
#define module_LoadPlugins(a) module_LoadPlugins(VLC_OBJECT(a))
void module_EndBank (bool);
int module_Map(vlc_object_t *, vlc_plugin_t *);

ssize_t module_list_cap (module_t ***, const char *);

int vlc_bindtextdomain (const char *);

/* Low-level OS-dependent handler */
int module_Load (vlc_object_t *, const char *, module_handle_t *, bool);
void *module_Lookup (module_handle_t, const char *);
void module_Unload (module_handle_t);

/* Plugins cache */
vlc_plugin_t *vlc_cache_load(vlc_object_t *, const char *, block_t **);
vlc_plugin_t *vlc_cache_lookup(vlc_plugin_t **, const char *relpath);

void CacheSave(vlc_object_t *, const char *, vlc_plugin_t *const *, size_t);

#endif /* !LIBVLC_MODULES_H */
