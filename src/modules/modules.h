/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

/* The module handle type. */
#if defined(HAVE_DL_DYLD) && !defined(__x86_64__)
#   if defined (HAVE_MACH_O_DYLD_H)
#       include <mach-o/dyld.h>
#   endif
typedef NSModule module_handle_t;
#elif defined(HAVE_IMAGE_H)
typedef int module_handle_t;
#elif defined(WIN32) || defined(UNDER_CE) || defined(__SYMBIAN32__)
typedef void * module_handle_t;
#elif defined(HAVE_DL_DLOPEN)
typedef void * module_handle_t;
#endif

/**
 * Internal module descriptor
 */
struct module_t
{
    char       *psz_object_name;
    gc_object_t vlc_gc_data;

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

    bool          b_builtin;  /* Set to true if the module is built in */
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

module_t *vlc_module_create (void);
module_t *vlc_submodule_create (module_t *module);

void  module_InitBank( vlc_object_t * );
#define module_InitBank(a) module_InitBank(VLC_OBJECT(a))
void module_LoadPlugins( vlc_object_t * );
#define module_LoadPlugins(a) module_LoadPlugins(VLC_OBJECT(a))
void module_EndBank( vlc_object_t *, bool );
#define module_EndBank(a,b) module_EndBank(VLC_OBJECT(a), b)

int vlc_bindtextdomain (const char *);

/* Low-level OS-dependent handler */
int module_Load (vlc_object_t *, const char *, module_handle_t *, bool);
void *module_Lookup (module_handle_t, const char *);
void module_Unload (module_handle_t);

/* Plugins cache */
void   CacheMerge (vlc_object_t *, module_t *, module_t *);
void   CacheDelete(vlc_object_t *, const char *);
size_t CacheLoad  (vlc_object_t *, const char *, module_cache_t ***);
void   CacheSave  (vlc_object_t *, const char *, module_cache_t **, size_t);
module_t *CacheFind (module_cache_t *const *, size_t,
                     const char *, const struct stat *);
int CacheAdd (module_cache_t ***, size_t *,
              const char *, const struct stat *, module_t *);

#endif /* !LIBVLC_MODULES_H */
