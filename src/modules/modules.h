/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

# include <stdatomic.h>
# include <vlc_plugin.h>

struct vlc_param;

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
        struct vlc_param *params; /**< Table of configuration items */
        size_t size; /**< Total count of all items */
        size_t count; /**< Count of real options (excludes hints) */
        size_t booleans; /**< Count of options that are of boolean type */
    } conf;

#ifdef HAVE_DYNAMIC_PLUGINS
    bool unloadable; /**< Whether the plug-in can be unloaded safely */
    atomic_uintptr_t handle; /**< Run-time linker handle (or nul) */
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

/** Plugin deactivation callback */
typedef void (*vlc_deactivate_cb)(vlc_object_t*);

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
    vlc_deactivate_cb deactivate;
};

vlc_plugin_t *vlc_plugin_create(void);
void vlc_plugin_destroy(vlc_plugin_t *);
module_t *vlc_module_create(vlc_plugin_t *);
void vlc_module_destroy (module_t *);

vlc_plugin_t *vlc_plugin_describe(vlc_plugin_cb);
int vlc_plugin_resolve(vlc_plugin_t *, vlc_plugin_cb);

void module_InitBank (void);
void module_LoadPlugins(libvlc_int_t *);
void module_EndBank (bool);
int vlc_plugin_Map(struct vlc_logger *, vlc_plugin_t *);
void *vlc_plugin_Symbol(struct vlc_logger *, vlc_plugin_t *, const char *name);

/**
 * Lists of all VLC modules with a given capability.
 *
 * This functions returns a table of all VLC modules whose capability
 * matches the supplied capability name. Entries are sorted by decreasing
 * module score.
 *
 * \note This function cannot fail. It returns zero if, and only if, no
 * modules match the requested capability inside the module bank.
 *
 * @param tab pointer to the table of modules [OUT]
 * @param name capability nul-terminated string (cannot be NULL)
 * @return the number of entries in the table
 */
size_t module_list_cap(module_t *const **tab, const char *name);

int vlc_bindtextdomain (const char *);

/* Low-level OS-dependent handler */

/**
 * Loads a dynamically linked library.
 *
 * \param path library file path
 * \param lazy whether to resolve the symbols lazily
 * \return a module handle on success, or NULL on error.
 */
void *vlc_dlopen(const char *path, bool) VLC_USED;

/**
 * Unloads a dynamic library.
 *
 * This function unloads a previously opened dynamically linked library
 * using a system dependent method.
 * \param handle handle of the library
 * \retval 0 on success
 * \retval -1 on error (none are defined though)
 */
int vlc_dlclose(void *);

/**
 * Looks up a symbol from a dynamically loaded library
 *
 * This function looks for a named symbol within a loaded library.
 *
 * \param handle handle to the library
 * \param name function name
 * \return the address of the symbol on success, or NULL on error
 *
 * \note If the symbol address is NULL, errors cannot be detected. However,
 * normal symbols such as function or global variables cannot have NULL as
 * their address.
 */
void *vlc_dlsym(void *handle, const char *) VLC_USED;

/**
 * Formats an error message for vlc_dlopen() or vlc_dlsym().
 *
 * \return a heap-allocated nul-terminated error string, or NULL.
 */
char *vlc_dlerror(void) VLC_USED;

/* Plugins cache */
vlc_plugin_t *vlc_cache_load(libvlc_int_t *, const char *, block_t **);
vlc_plugin_t *vlc_cache_lookup(vlc_plugin_t **, const char *relpath);

void CacheSave(libvlc_int_t *, const char *, vlc_plugin_t *const *, size_t);

#endif /* !LIBVLC_MODULES_H */
