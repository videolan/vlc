/*****************************************************************************
 * bank.c : Modules list
 *****************************************************************************
 * Copyright (C) 2001-2011 the VideoLAN team
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_fs.h>
#include "libvlc.h"
#include "config/configuration.h"
#include "modules/modules.h"

static struct
{
    vlc_mutex_t lock;
    module_t *head;
    unsigned usage;
} modules = { VLC_STATIC_MUTEX, NULL, 0 };

module_t *vlc_entry__main (void);

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
typedef enum { CACHE_USE, CACHE_RESET, CACHE_IGNORE } cache_mode_t;
typedef struct module_bank module_bank_t;

static void AllocateAllPlugins (vlc_object_t *);
static void AllocatePluginPath (vlc_object_t *, const char *, cache_mode_t);
static void AllocatePluginDir( vlc_object_t *, module_bank_t *, const char *,
                               unsigned, cache_mode_t );
static int  AllocatePluginFile( vlc_object_t *, module_bank_t *, const char *,
                                const struct stat *, cache_mode_t );
static module_t *module_InitDynamic (vlc_object_t *, const char *, bool);
#endif
static module_t *module_InitStatic (vlc_plugin_cb);

static void module_StoreBank (module_t *module)
{
    /*vlc_assert_locked (&modules.lock);*/
    module->next = modules.head;
    modules.head = module;
}

/**
 * Init bank
 *
 * Creates a module bank structure which will be filled later
 * on with all the modules found.
 */
void module_InitBank (void)
{
    vlc_mutex_lock (&modules.lock);

    if (modules.usage == 0)
    {
        /* Fills the module bank structure with the main module infos.
         * This is very useful as it will allow us to consider the main
         * library just as another module, and for instance the configuration
         * options of main will be available in the module bank structure just
         * as for every other module. */
        module_t *module = module_InitStatic (vlc_entry__main);
        if (likely(module != NULL))
            module_StoreBank (module);

        vlc_rwlock_init (&config_lock);
        config_SortConfig ();
    }
    modules.usage++;

    /* We do retain the module bank lock until the plugins are loaded as well.
     * This is ugly, this staged loading approach is needed: LibVLC gets
     * some configuration parameters relevant to loading the plugins from
     * the main (builtin) module. The module bank becomes shared read-only data
     * once it is ready, so we need to fully serialize initialization.
     * DO NOT UNCOMMENT the following line unless you managed to squeeze
     * module_LoadPlugins() before you unlock the mutex. */
    /*vlc_mutex_unlock (&modules.lock);*/
}

/**
 * Unloads all unused plugin modules and empties the module
 * bank in case of success.
 */
void module_EndBank (bool b_plugins)
{
    module_t *head = NULL;

    /* If plugins were _not_ loaded, then the caller still has the bank lock
     * from module_InitBank(). */
    if( b_plugins )
        vlc_mutex_lock (&modules.lock);
    /*else
        vlc_assert_locked (&modules.lock); not for static mutexes :( */

    assert (modules.usage > 0);
    if (--modules.usage == 0)
    {
        config_UnsortConfig ();
        vlc_rwlock_destroy (&config_lock);
        head = modules.head;
        modules.head = NULL;
    }
    vlc_mutex_unlock (&modules.lock);

    while (head != NULL)
    {
        module_t *module = head;

        head = module->next;
#ifdef HAVE_DYNAMIC_PLUGINS
        if (module->b_loaded && module->b_unloadable)
        {
            module_Unload (module->handle);
            module->b_loaded = false;
        }
#endif
        vlc_module_destroy (module);
    }
}

#undef module_LoadPlugins
/**
 * Loads module descriptions for all available plugins.
 * Fills the module bank structure with the plugin modules.
 *
 * \param p_this vlc object structure
 * \return nothing
 */
void module_LoadPlugins (vlc_object_t *obj)
{
    /*vlc_assert_locked (&modules.lock); not for static mutexes :( */

#ifdef HAVE_DYNAMIC_PLUGINS
    if (modules.usage == 1)
    {
        msg_Dbg (obj, "searching plug-in modules");
        AllocateAllPlugins (obj);
        config_UnsortConfig ();
        config_SortConfig ();
    }
#endif
    vlc_mutex_unlock (&modules.lock);
}

/**
 * Frees the flat list of VLC modules.
 * @param list list obtained by module_list_get()
 * @param length number of items on the list
 * @return nothing.
 */
void module_list_free (module_t **list)
{
    free (list);
}

/**
 * Gets the flat list of VLC modules.
 * @param n [OUT] pointer to the number of modules or NULL
 * @return NULL-terminated table of module pointers
 *         (release with module_list_free()), or NULL in case of error.
 */
module_t **module_list_get (size_t *n)
{
    /* TODO: this whole module lookup is quite inefficient */
    /* Remove this and improve module_need */
    module_t **tab = NULL;
    size_t i = 0;

    for (module_t *mod = modules.head; mod; mod = mod->next)
    {
         module_t **nt;
         nt  = realloc (tab, (i + 2 + mod->submodule_count) * sizeof (*tab));
         if (nt == NULL)
         {
             module_list_free (tab);
             return NULL;
         }

         tab = nt;
         tab[i++] = mod;
         for (module_t *subm = mod->submodule; subm; subm = subm->next)
             tab[i++] = subm;
         tab[i] = NULL;
    }
    if (n != NULL)
        *n = i;
    return tab;
}

char *psz_vlcpath = NULL;

#ifdef HAVE_DYNAMIC_PLUGINS

/*****************************************************************************
 * AllocateAllPlugins: load all plugin modules we can find.
 *****************************************************************************/
static void AllocateAllPlugins (vlc_object_t *p_this)
{
    const char *vlcpath = psz_vlcpath;
    char *paths;
    cache_mode_t mode;

    if( !var_InheritBool( p_this, "plugins-cache" ) )
        mode = CACHE_IGNORE;
    else if( var_InheritBool( p_this, "reset-plugins-cache" ) )
        mode = CACHE_RESET;
    else
        mode = CACHE_USE;

    /* Contruct the special search path for system that have a relocatable
     * executable. Set it to <vlc path>/plugins. */
    assert( vlcpath );

    if( asprintf( &paths, "%s" DIR_SEP "plugins", vlcpath ) != -1 )
    {
        AllocatePluginPath (p_this, paths, mode);
        free( paths );
    }

    /* If the user provided a plugin path, we add it to the list */
    paths = getenv( "VLC_PLUGIN_PATH" );
    if( paths == NULL )
        return;

    paths = strdup( paths ); /* don't harm the environment ! :) */
    if( unlikely(paths == NULL) )
        return;

    for( char *buf, *path = strtok_r( paths, PATH_SEP, &buf );
         path != NULL;
         path = strtok_r( NULL, PATH_SEP, &buf ) )
        AllocatePluginPath (p_this, path, mode);

    free( paths );
}

struct module_bank
{
    /* Plugins cache */
    size_t         i_cache;
    module_cache_t *cache;

    int            i_loaded_cache;
    module_cache_t *loaded_cache;
};

static void AllocatePluginPath (vlc_object_t *p_this, const char *path,
                                cache_mode_t mode)
{
    module_bank_t bank;
    module_cache_t *cache = NULL;
    size_t count = 0;

    switch( mode )
    {
        case CACHE_USE:
            count = CacheLoad( p_this, path, &cache );
            break;
        case CACHE_RESET:
            CacheDelete( p_this, path );
            break;
        case CACHE_IGNORE:
            msg_Dbg( p_this, "ignoring plugins cache file" );
    }

    msg_Dbg( p_this, "recursively browsing `%s'", path );

    bank.cache = NULL;
    bank.i_cache = 0;
    bank.loaded_cache = cache;
    bank.i_loaded_cache = count;

    /* Don't go deeper than 5 subdirectories */
    AllocatePluginDir (p_this, &bank, path, 5, mode);

    switch( mode )
    {
        case CACHE_USE:
            /* Discard unmatched cache entries */
            for( size_t i = 0; i < count; i++ )
            {
                if (cache[i].p_module != NULL)
                   vlc_module_destroy (cache[i].p_module);
                free (cache[i].path);
            }
            free( cache );
        case CACHE_RESET:
            CacheSave (p_this, path, bank.cache, bank.i_cache);
        case CACHE_IGNORE:
            break;
    }
}

/*****************************************************************************
 * AllocatePluginDir: recursively parse a directory to look for plugins
 *****************************************************************************/
static void AllocatePluginDir( vlc_object_t *p_this, module_bank_t *p_bank,
                               const char *psz_dir, unsigned i_maxdepth,
                               cache_mode_t mode )
{
    if( i_maxdepth == 0 )
        return;

    DIR *dh = vlc_opendir (psz_dir);
    if (dh == NULL)
        return;

    /* Parse the directory and try to load all files it contains. */
    for (;;)
    {
        char *file = vlc_readdir (dh), *path;
        struct stat st;

        if (file == NULL)
            break;

        /* Skip ".", ".." */
        if (!strcmp (file, ".") || !strcmp (file, ".."))
        {
            free (file);
            continue;
        }

        const int pathlen = asprintf (&path, "%s"DIR_SEP"%s", psz_dir, file);
        free (file);
        if (pathlen == -1 || vlc_stat (path, &st))
            continue;

        if (S_ISDIR (st.st_mode))
            /* Recurse into another directory */
            AllocatePluginDir (p_this, p_bank, path, i_maxdepth - 1, mode);
        else
        if (S_ISREG (st.st_mode)
         && strncmp (path, "lib", 3)
         && ((size_t)pathlen >= sizeof ("_plugin"LIBEXT))
         && !strncasecmp (path + pathlen - strlen ("_plugin"LIBEXT),
                          "_plugin"LIBEXT, strlen ("_plugni"LIBEXT)))
            /* ^^ We only load files matching "lib*_plugin"LIBEXT */
            AllocatePluginFile (p_this, p_bank, path, &st, mode);

        free (path);
    }
    closedir (dh);
}

/*****************************************************************************
 * AllocatePluginFile: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_need
 * and module_unneed.
 *****************************************************************************/
static int AllocatePluginFile( vlc_object_t * p_this, module_bank_t *p_bank,
                               const char *path, const struct stat *st,
                               cache_mode_t mode )
{
    module_t * p_module = NULL;

    /* Check our plugins cache first then load plugin if needed */
    if( mode == CACHE_USE )
        p_module = CacheFind (p_bank->loaded_cache, p_bank->i_loaded_cache,
                              path, st);
    if( p_module == NULL )
        p_module = module_InitDynamic (p_this, path, true);
    if( p_module == NULL )
        return -1;

    /* We have not already scanned and inserted this module */
    assert( p_module->next == NULL );

    /* Unload plugin until we really need it */
    if( p_module->b_loaded && p_module->b_unloadable )
    {
        module_Unload( p_module->handle );
        p_module->b_loaded = false;
    }

    /* For now we force loading if the module's config contains
     * callbacks or actions.
     * Could be optimized by adding an API call.*/
    for( size_t n = p_module->confsize, i = 0; i < n; i++ )
         if( p_module->p_config[i].i_action )
         {
             /* !unloadable not allowed for plugins with callbacks */
             vlc_module_destroy (p_module);
             p_module = module_InitDynamic (p_this, path, false);
             break;
         }

    module_StoreBank (p_module);

    if( mode == CACHE_IGNORE )
        return 0;

    /* Add entry to cache */
    CacheAdd (&p_bank->cache, &p_bank->i_cache, path, st, p_module);
    /* TODO: deal with errors */
    return  0;
}

/**
 * Loads a dynamically-linked plug-in into memory and initialize it.
 *
 * The module can then be handled by module_need() and module_unneed().
 *
 * \param path file path of the shared object
 * \param fast whether to optimize loading for speed or safety
 *             (fast is used when the plug-in is registered but not used)
 */
static module_t *module_InitDynamic (vlc_object_t *obj,
                                     const char *path, bool fast)
{
    module_handle_t handle;

    if (module_Load (obj, path, &handle, fast))
        return NULL;

    /* Try to resolve the symbol */
    static const char entry_name[] = "vlc_entry" MODULE_SUFFIX;
    vlc_plugin_cb entry =
        (vlc_plugin_cb) module_Lookup (handle, entry_name);
    if (entry == NULL)
    {
        msg_Warn (obj, "cannot find plug-in entry point in %s", path);
        goto error;
    }

    /* We can now try to call the symbol */
    module_t *module = entry ();
    if (unlikely(module == NULL))
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err (obj, "cannot initialize plug-in %s", path);
        goto error;
    }

    module->psz_filename = strdup (path);
    if (unlikely(module->psz_filename == NULL))
    {
        vlc_module_destroy (module);
        goto error;
    }
    module->handle = handle;
    module->b_loaded = true;
    return module;
error:
    module_Unload( handle );
    return NULL;
}
#endif /* HAVE_DYNAMIC_PLUGINS */

/**
 * Registers a statically-linked plug-in.
 */
static module_t *module_InitStatic (vlc_plugin_cb entry)
{
    /* Initializes the module */
    module_t *module = entry ();
    if (unlikely (module == NULL))
        return NULL;

    module->b_loaded = true;
    module->b_unloadable = false;
    return module;
}

/**
 * Makes sure the module is loaded in memory.
 * \return 0 on success, -1 on failure
 */
int module_Map (vlc_object_t *obj, module_t *module)
{
    if (module->parent != NULL)
        module = module->parent;

#warning FIXME: race condition!
    if (module->b_loaded)
        return 0;
    assert (module->psz_filename != NULL);

#ifdef HAVE_DYNAMIC_PLUGINS
    module_t *uncache = module_InitDynamic (obj, module->psz_filename, false);
    if (uncache != NULL)
    {
        CacheMerge (obj, module, uncache);
        vlc_module_destroy (uncache);
        return 0;
    }
#endif
    msg_Err (obj, "corrupt module: %s", module->psz_filename);
    return -1;
}
