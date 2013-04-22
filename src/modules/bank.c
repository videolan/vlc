/*****************************************************************************
 * bank.c : Modules list
 *****************************************************************************
 * Copyright (C) 2001-2011 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins (vlc_object_t *);
#endif
static module_t *module_InitStatic (vlc_plugin_cb);

static void module_StoreBank (module_t *module)
{
    /*vlc_assert_locked (&modules.lock);*/
    module->next = modules.head;
    modules.head = module;
}

#if defined(__ELF__) || !HAVE_DYNAMIC_PLUGINS
# ifdef __GNUC__
__attribute__((weak))
# else
#  pragma weak vlc_static_modules
# endif
extern vlc_plugin_cb vlc_static_modules[];

static void module_InitStaticModules(void)
{
    if (!vlc_static_modules)
        return;

    for (unsigned i = 0; vlc_static_modules[i]; i++) {
        module_t *module = module_InitStatic (vlc_static_modules[i]);
        if (likely(module != NULL))
            module_StoreBank (module);
    }
}
#else
static void module_InitStaticModules(void) { }
#endif

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
 * \return total number of modules in bank after loading all plug-ins
 */
size_t module_LoadPlugins (vlc_object_t *obj)
{
    /*vlc_assert_locked (&modules.lock); not for static mutexes :( */

    if (modules.usage == 1)
    {
        module_InitStaticModules ();
#ifdef HAVE_DYNAMIC_PLUGINS
        msg_Dbg (obj, "searching plug-in modules");
        AllocateAllPlugins (obj);
#endif
        config_UnsortConfig ();
        config_SortConfig ();
    }
    vlc_mutex_unlock (&modules.lock);

    size_t count;
    module_t **list = module_list_get (&count);
    module_list_free (list);
    msg_Dbg (obj, "plug-ins loaded: %zu modules", count);
    return count;
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
 * @param n [OUT] pointer to the number of modules
 * @return table of module pointers (release with module_list_free()),
 *         or NULL in case of error (in that case, *n is zeroed).
 */
module_t **module_list_get (size_t *n)
{
    module_t **tab = NULL;
    size_t i = 0;

    assert (n != NULL);

    for (module_t *mod = modules.head; mod; mod = mod->next)
    {
         module_t **nt;
         nt  = realloc (tab, (i + 1 + mod->submodule_count) * sizeof (*tab));
         if (unlikely(nt == NULL))
         {
             free (tab);
             *n = 0;
             return NULL;
         }

         tab = nt;
         tab[i++] = mod;
         for (module_t *subm = mod->submodule; subm; subm = subm->next)
             tab[i++] = subm;
    }
    *n = i;
    return tab;
}

static int modulecmp (const void *a, const void *b)
{
    const module_t *const *ma = a, *const *mb = b;
    /* Note that qsort() uses _ascending_ order,
     * so the smallest module is the one with the biggest score. */
    return (*mb)->i_score - (*ma)->i_score;
}

/**
 * Builds a sorted list of all VLC modules with a given capability.
 * The list is sorted from the highest module score to the lowest.
 * @param list pointer to the table of modules [OUT]
 * @param cap capability of modules to look for
 * @return the number of matching found, or -1 on error (*list is then NULL).
 * @note *list must be freed with module_list_free().
 */
ssize_t module_list_cap (module_t ***restrict list, const char *cap)
{
    /* TODO: This is quite inefficient. List should be sorted by capability. */
    ssize_t n = 0;

    assert (list != NULL);

    for (module_t *mod = modules.head; mod != NULL; mod = mod->next)
    {
         if (module_provides (mod, cap))
             n++;
         for (module_t *subm = mod->submodule; subm != NULL; subm = subm->next)
             if (module_provides (subm, cap))
                 n++;
    }

    module_t **tab = malloc (sizeof (*tab) * n);
    *list = tab;
    if (unlikely(tab == NULL))
        return -1;

    for (module_t *mod = modules.head; mod != NULL; mod = mod->next)
    {
         if (module_provides (mod, cap))
             *(tab++)= mod;
         for (module_t *subm = mod->submodule; subm != NULL; subm = subm->next)
             if (module_provides (subm, cap))
                 *(tab++) = subm;
    }

    assert (tab == *list + n);
    qsort (*list, n, sizeof (*tab), modulecmp);
    return n;
}

#ifdef HAVE_DYNAMIC_PLUGINS
typedef enum { CACHE_USE, CACHE_RESET, CACHE_IGNORE } cache_mode_t;

static void AllocatePluginPath (vlc_object_t *, const char *, cache_mode_t);

/**
 * Enumerates all dynamic plug-ins that can be found.
 *
 * This function will recursively browse the default plug-ins directory and any
 * directory listed in the VLC_PLUGIN_PATH environment variable.
 * For performance reasons, a cache is normally used so that plug-in shared
 * objects do not need to loaded and linked into the process.
 */
static void AllocateAllPlugins (vlc_object_t *p_this)
{
    char *paths;
    cache_mode_t mode;

    if( !var_InheritBool( p_this, "plugins-cache" ) )
        mode = CACHE_IGNORE;
    else if( var_InheritBool( p_this, "reset-plugins-cache" ) )
        mode = CACHE_RESET;
    else
        mode = CACHE_USE;

#if VLC_WINSTORE_APP
    /* Windows Store Apps can not load external plugins with absolute paths. */
    AllocatePluginPath (p_this, "plugins", mode);
#else
    /* Contruct the special search path for system that have a relocatable
     * executable. Set it to <vlc path>/plugins. */
    char *vlcpath = config_GetLibDir ();
    if (likely(vlcpath != NULL)
     && likely(asprintf (&paths, "%s" DIR_SEP "plugins", vlcpath) != -1))
    {
        AllocatePluginPath (p_this, paths, mode);
        free( paths );
    }
    free (vlcpath);
#endif /* VLC_WINSTORE_APP */

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

typedef struct module_bank
{
    vlc_object_t *obj;
    const char   *base;
    cache_mode_t  mode;

    size_t         i_cache;
    module_cache_t *cache;

    int            i_loaded_cache;
    module_cache_t *loaded_cache;
} module_bank_t;

static void AllocatePluginDir (module_bank_t *, unsigned,
                               const char *, const char *);

/**
 * Scans for plug-ins within a file system hierarchy.
 * \param path base directory to browse
 */
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

    bank.obj = p_this;
    bank.base = path;
    bank.mode = mode;
    bank.cache = NULL;
    bank.i_cache = 0;
    bank.loaded_cache = cache;
    bank.i_loaded_cache = count;

    /* Don't go deeper than 5 subdirectories */
    AllocatePluginDir (&bank, 5, path, NULL);

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

static int AllocatePluginFile (module_bank_t *, const char *,
                               const char *, const struct stat *);

/**
 * Recursively browses a directory to look for plug-ins.
 */
static void AllocatePluginDir (module_bank_t *bank, unsigned maxdepth,
                               const char *absdir, const char *reldir)
{
    if (maxdepth == 0)
        return;
    maxdepth--;

    DIR *dh = vlc_opendir (absdir);
    if (dh == NULL)
        return;

    /* Parse the directory and try to load all files it contains. */
    for (;;)
    {
        char *file = vlc_readdir (dh), *relpath = NULL, *abspath = NULL;
        if (file == NULL)
            break;

        /* Skip ".", ".." */
        if (!strcmp (file, ".") || !strcmp (file, ".."))
            goto skip;

        /* Compute path relative to plug-in base directory */
        if (reldir != NULL)
        {
            if (asprintf (&relpath, "%s"DIR_SEP"%s", reldir, file) == -1)
                relpath = NULL;
        }
        else
            relpath = strdup (file);
        if (unlikely(relpath == NULL))
            goto skip;

        /* Compute absolute path */
        if (asprintf (&abspath, "%s"DIR_SEP"%s", bank->base, relpath) == -1)
        {
            abspath = NULL;
            goto skip;
        }

        struct stat st;
        if (vlc_stat (abspath, &st) == -1)
            goto skip;

        if (S_ISREG (st.st_mode))
        {
            static const char prefix[] = "lib";
            static const char suffix[] = "_plugin"LIBEXT;
            size_t len = strlen (file);

#ifndef __OS2__
            /* Check that file matches the "lib*_plugin"LIBEXT pattern */
            if (len > strlen (suffix)
             && !strncmp (file, prefix, strlen (prefix))
             && !strcmp (file + len - strlen (suffix), suffix))
#else
            /* We load all the files ending with LIBEXT on OS/2,
             * because OS/2 has a 8.3 length limitation for DLL name */
            if (len > strlen (LIBEXT)
             && !strcasecmp (file + len - strlen (LIBEXT), LIBEXT))
#endif
                AllocatePluginFile (bank, abspath, relpath, &st);
        }
        else if (S_ISDIR (st.st_mode))
            /* Recurse into another directory */
            AllocatePluginDir (bank, maxdepth, abspath, relpath);
    skip:
        free (relpath);
        free (abspath);
        free (file);
    }
    closedir (dh);
}

static module_t *module_InitDynamic (vlc_object_t *, const char *, bool);

/**
 * Scans a plug-in from a file.
 */
static int AllocatePluginFile (module_bank_t *bank, const char *abspath,
                               const char *relpath, const struct stat *st)
{
    module_t *module = NULL;

    /* Check our plugins cache first then load plugin if needed */
    if (bank->mode == CACHE_USE)
    {
        module = CacheFind (bank->loaded_cache, bank->i_loaded_cache,
                            relpath, st);
        if (module != NULL)
        {
            module->psz_filename = strdup (abspath);
            if (unlikely(module->psz_filename == NULL))
            {
                vlc_module_destroy (module);
                module = NULL;
            }
        }
    }
    if (module == NULL)
        module = module_InitDynamic (bank->obj, abspath, true);
    if (module == NULL)
        return -1;

    /* We have not already scanned and inserted this module */
    assert (module->next == NULL);

    /* Unload plugin until we really need it */
    if (module->b_loaded && module->b_unloadable)
    {
        module_Unload (module->handle);
        module->b_loaded = false;
    }

    /* For now we force loading if the module's config contains callbacks.
     * Could be optimized by adding an API call.*/
    for (size_t n = module->confsize, i = 0; i < n; i++)
         if (module->p_config[i].list_count == 0
          && (module->p_config[i].list.psz_cb != NULL || module->p_config[i].list.i_cb != NULL))
         {
             /* !unloadable not allowed for plugins with callbacks */
             vlc_module_destroy (module);
             module = module_InitDynamic (bank->obj, abspath, false);
             if (unlikely(module == NULL))
                 return -1;
             break;
         }

    module_StoreBank (module);

    if (bank->mode != CACHE_IGNORE) /* Add entry to cache */
        CacheAdd (&bank->cache, &bank->i_cache, relpath, st, module);
    /* TODO: deal with errors */
    return  0;
}

#ifdef __OS2__
#   define EXTERN_PREFIX "_"
#else
#   define EXTERN_PREFIX
#endif


/**
 * Loads a dynamically-linked plug-in into memory and initialize it.
 *
 * The module can then be handled by module_need() and module_unneed().
 *
 * \param path file path of the shared object
 * \param fast whether to optimize loading for speed or safety
 *             (fast is used when the plug-in is registered but not used)
 */
static module_t *module_InitDynamic (vlc_object_t *obj, const char *path,
                                     bool fast)
{
    module_handle_t handle;

    if (module_Load (obj, path, &handle, fast))
        return NULL;

    /* Try to resolve the symbol */
    static const char entry_name[] = EXTERN_PREFIX "vlc_entry" MODULE_SUFFIX;
    vlc_plugin_cb entry =
        (vlc_plugin_cb) module_Lookup (handle, entry_name);
    if (entry == NULL)
    {
        msg_Warn (obj, "cannot find plug-in entry point in %s", path);
        goto error;
    }

    /* We can now try to call the symbol */
    module_t *module = vlc_plugin_describe (entry);
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
    module_t *module = vlc_plugin_describe (entry);
    if (unlikely(module == NULL))
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
