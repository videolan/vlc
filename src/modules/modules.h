/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 VLC authors and VideoLAN
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

typedef struct module_cache_t module_cache_t;

/*****************************************************************************
 * Module cache description structure
 *****************************************************************************/
struct module_cache_t
{
    /* Mandatory cache entry header */
    char  *path;
    time_t mtime;
    off_t  size;

    /* Optional extra data */
    module_t *p_module;
};


#define MODULE_SHORTCUT_MAX 20

/** The module handle type */
typedef void *module_handle_t;

/** Plugin entry point prototype */
typedef int (*vlc_plugin_cb) (int (*)(void *, void *, int, ...), void *);

/** Main module */
int vlc_entry__main (int (*)(void *, void *, int, ...), void *);

/**
 * Internal module descriptor
 */
struct module_t
{
    module_t   *next;
    module_t   *parent;
    module_t   *submodule;
    unsigned    submodule_count;

    /** Shortcuts to the module */
    unsigned    i_shortcuts;
    char        **pp_shortcuts;

    /*
     * Variables set by the module to identify itself
     */
    char *psz_shortname;                              /**< Module name */
    char *psz_longname;                   /**< Module descriptive name */
    char *psz_help;        /**< Long help string for "special" modules */

    char    *psz_capability;                                 /**< Capability */
    int      i_score;                          /**< Score for the capability */

    bool          b_loaded;        /* Set to true if the dll is loaded */
    bool b_unloadable;                        /**< Can we be dlclosed? */

    /* Callbacks */
    void *pf_activate;
    void *pf_deactivate;

    /*
     * Variables set by the module to store its config options
     */
    module_config_t *p_config;             /* Module configuration structure */
    size_t           confsize;            /* Number of module_config_t items */
    unsigned int     i_config_items;        /* number of configuration items */
    unsigned int     i_bool_items;            /* number of bool config items */

    /*
     * Variables used internally by the module manager
     */
    /* Plugin-specific stuff */
    module_handle_t     handle;                             /* Unique handle */
    char *              psz_filename;                     /* Module filename */
    char *              domain;                            /* gettext domain */
};

module_t *vlc_plugin_describe (vlc_plugin_cb);
module_t *vlc_module_create (module_t *);
void vlc_module_destroy (module_t *);

void module_InitBank (void);
size_t module_LoadPlugins( vlc_object_t * );
#define module_LoadPlugins(a) module_LoadPlugins(VLC_OBJECT(a))
void module_EndBank (bool);
int module_Map (vlc_object_t *, module_t *);

ssize_t module_list_cap (module_t ***, const char *);

int vlc_bindtextdomain (const char *);

/* Low-level OS-dependent handler */
int module_Load (vlc_object_t *, const char *, module_handle_t *, bool);
void *module_Lookup (module_handle_t, const char *);
void module_Unload (module_handle_t);

/* Plugins cache */
void   CacheMerge (vlc_object_t *, module_t *, module_t *);
void   CacheDelete(vlc_object_t *, const char *);
size_t CacheLoad  (vlc_object_t *, const char *, module_cache_t **);

struct stat;

int CacheAdd (module_cache_t **, size_t *,
              const char *, const struct stat *, module_t *);
void CacheSave  (vlc_object_t *, const char *, module_cache_t *, size_t);
module_t *CacheFind (module_cache_t *, size_t,
                     const char *, const struct stat *);

#endif /* !LIBVLC_MODULES_H */
