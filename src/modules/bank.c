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
#include <unistd.h>
#ifdef HAVE_SEARCH_H
# include <search.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_fs.h>
#include <vlc_block.h>
#include "libvlc.h"
#include "config/configuration.h"
#include "modules/modules.h"

typedef struct vlc_modcap
{
    char *name;
    module_t **modv;
    size_t modc;
} vlc_modcap_t;

static int vlc_modcap_cmp(const void *a, const void *b)
{
    const vlc_modcap_t *capa = a, *capb = b;
    return strcmp(capa->name, capb->name);
}

static void vlc_modcap_free(void *data)
{
    vlc_modcap_t *cap = data;

    free(cap->modv);
    free(cap->name);
    free(cap);
}

static int vlc_module_cmp (const void *a, const void *b)
{
    const module_t *const *ma = a, *const *mb = b;
    /* Note that qsort() uses _ascending_ order,
     * so the smallest module is the one with the biggest score. */
    return (*mb)->i_score - (*ma)->i_score;
}

static void vlc_modcap_sort(const void *node, const VISIT which,
                            const int depth)
{
    vlc_modcap_t *const *cp = node, *cap = *cp;

    if (which != postorder && which != leaf)
        return;

    qsort(cap->modv, cap->modc, sizeof (*cap->modv), vlc_module_cmp);
    (void) depth;
}

static struct
{
    vlc_mutex_t lock;
    block_t *caches;
    void *caps_tree;
    unsigned usage;
} modules = { VLC_STATIC_MUTEX, NULL, NULL, 0 };

vlc_plugin_t *vlc_plugins = NULL;

/**
 * Adds a module to the bank
 */
static int vlc_module_store(module_t *mod)
{
    const char *name = module_get_capability(mod);
    vlc_modcap_t *cap = malloc(sizeof (*cap));
    if (unlikely(cap == NULL))
        return -1;

    cap->name = strdup(name);
    cap->modv = NULL;
    cap->modc = 0;

    if (unlikely(cap->name == NULL))
        goto error;

    void **cp = tsearch(cap, &modules.caps_tree, vlc_modcap_cmp);
    if (unlikely(cp == NULL))
        goto error;

    if (*cp != cap)
    {
        vlc_modcap_free(cap);
        cap = *cp;
    }

    module_t **modv = realloc(cap->modv, sizeof (*modv) * (cap->modc + 1));
    if (unlikely(modv == NULL))
        return -1;

    cap->modv = modv;
    cap->modv[cap->modc] = mod;
    cap->modc++;
    return 0;
error:
    vlc_modcap_free(cap);
    return -1;
}

/**
 * Adds a plugin (and all its modules) to the bank
 */
static void vlc_plugin_store(vlc_plugin_t *lib)
{
    vlc_mutex_assert(&modules.lock);

    lib->next = vlc_plugins;
    vlc_plugins = lib;

    for (module_t *m = lib->module; m != NULL; m = m->next)
        vlc_module_store(m);
}

/**
 * Registers a statically-linked plug-in.
 */
static vlc_plugin_t *module_InitStatic(vlc_plugin_cb entry)
{
    /* Initializes the statically-linked library */
    vlc_plugin_t *lib = vlc_plugin_describe (entry);
    if (unlikely(lib == NULL))
        return NULL;

#ifdef HAVE_DYNAMIC_PLUGINS
    atomic_init(&lib->handle, 0);
    lib->unloadable = false;
#endif
    return lib;
}

/* With weak linking in __ELF__, there is no need to provide a definition
 * of vlc_static_modules at build time and it will be evaluated to NULL if
 * not provided at runtime. However, although __MACH__ implies the same runtime
 * consequences for weak linking, it will still require the definition to exist
 * at build time. To workaround this, we add -Wl,-U,vlc_static_modules. */
#if defined(__ELF__) || defined(__MACH__) || !HAVE_DYNAMIC_PLUGINS
VLC_WEAK
extern vlc_plugin_cb vlc_static_modules[];

static void module_InitStaticModules(void)
{
    if (!vlc_static_modules)
        return;

    for (unsigned i = 0; vlc_static_modules[i]; i++)
    {
        vlc_plugin_t *lib = module_InitStatic(vlc_static_modules[i]);
        if (likely(lib != NULL))
            vlc_plugin_store(lib);
    }
}
#else
static void module_InitStaticModules(void) { }
#endif

#ifdef HAVE_DYNAMIC_PLUGINS
static const char *module_GetVersion(void *handle)
{
    const char *(*get_api_version)(void);

    get_api_version = vlc_dlsym(handle, "vlc_entry_api_version");
    if (get_api_version == NULL)
        return NULL;

    return get_api_version();
}

static void *module_Open(struct vlc_logger *log,
                         const char *path, bool fast)
{
    void *handle = vlc_dlopen(path, fast);
    if (handle == NULL)
    {
        char *errmsg = vlc_dlerror();

        vlc_error(log, "cannot load plug-in %s: %s", path,
                  errmsg ? errmsg : "unknown error");
        free(errmsg);
        return NULL;
    }

    const char *str = module_GetVersion(handle);
    if (str == NULL) {
        vlc_error(log, "cannot load plug-in %s: %s", path,
                  "unknown version or not a plug-in");
error:
        vlc_dlclose(handle);
        return NULL;
    }

    if (strcmp(str, VLC_API_VERSION_STRING)) {
        vlc_error(log, "cannot load plug-in %s: unsupported version %s", path,
                  str);
        goto error;
    }

    return handle;
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
static vlc_plugin_t *module_InitDynamic(vlc_object_t *obj, const char *path,
                                        bool fast)
{
    void *handle = module_Open(obj->logger, path, fast);
    if (handle == NULL)
        return NULL;

    /* Try to resolve the symbol */
    vlc_plugin_cb entry = vlc_dlsym(handle, "vlc_entry");
    if (entry == NULL)
    {
        msg_Warn (obj, "cannot find plug-in entry point in %s", path);
        goto error;
    }

    /* We can now try to call the symbol */
    vlc_plugin_t *plugin = vlc_plugin_describe(entry);
    if (unlikely(plugin == NULL))
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err (obj, "cannot initialize plug-in %s", path);
        goto error;
    }

    atomic_init(&plugin->handle, (uintptr_t)handle);
    return plugin;
error:
    vlc_dlclose(handle);
    return NULL;
}

typedef enum
{
    CACHE_READ_FILE  = 0x1,
    CACHE_SCAN_DIR   = 0x2,
    CACHE_WRITE_FILE = 0x4,
} cache_mode_t;

typedef struct module_bank
{
    vlc_object_t *obj;
    const char   *base;
    cache_mode_t  mode;

    size_t        size;
    vlc_plugin_t **plugins;
    vlc_plugin_t *cache;
} module_bank_t;

/**
 * Scans a plug-in from a file.
 */
static int AllocatePluginFile (module_bank_t *bank, const char *abspath,
                               const char *relpath, const struct stat *st)
{
    vlc_plugin_t *plugin = NULL;

    /* Check our plugins cache first then load plugin if needed */
    if (bank->mode & CACHE_READ_FILE)
    {
        plugin = vlc_cache_lookup(&bank->cache, relpath);

        if (plugin != NULL
         && (plugin->mtime != (int64_t)st->st_mtime
          || plugin->size != (uint64_t)st->st_size))
        {
            msg_Err(bank->obj, "stale plugins cache: modified %s",
                    plugin->abspath);
            vlc_plugin_destroy(plugin);
            plugin = NULL;
        }
    }

    if (plugin == NULL)
    {
        char *path = strdup(relpath);
        if (path == NULL)
            return -1;

        plugin = module_InitDynamic(bank->obj, abspath, true);

        if (plugin != NULL)
        {
            plugin->path = path;
            plugin->mtime = st->st_mtime;
            plugin->size = st->st_size;
        }
        else free(path);
    }

    if (plugin == NULL)
        return -1;

    vlc_plugin_store(plugin);

    if (bank->mode & CACHE_WRITE_FILE) /* Add entry to to-be-saved cache */
    {
        bank->plugins = xrealloc(bank->plugins,
                                 (bank->size + 1) * sizeof (vlc_plugin_t *));
        bank->plugins[bank->size] = plugin;
        bank->size++;
    }

    /* TODO: deal with errors */
    return  0;
}

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
        char *relpath = NULL, *abspath = NULL;
        const char *file = vlc_readdir (dh);
        if (file == NULL)
            break;

        /* Skip ".", ".." */
        if (!strcmp (file, ".") || !strcmp (file, ".."))
            continue;

        /* Compute path relative to plug-in base directory */
        if (reldir != NULL)
        {
            if (asprintf (&relpath, "%s"DIR_SEP"%s", reldir, file) == -1)
                relpath = NULL;
        }
        else
            relpath = strdup (file);
        if (unlikely(relpath == NULL))
            continue;

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
    }
    closedir (dh);
}

/**
 * Scans for plug-ins within a file system hierarchy.
 * \param path base directory to browse
 */
static void AllocatePluginPath(vlc_object_t *obj, const char *path,
                               cache_mode_t mode)
{
    module_bank_t bank =
    {
        .obj = obj,
        .base = path,
        .mode = mode,
    };

    if (mode & CACHE_READ_FILE)
        bank.cache = vlc_cache_load(obj, path, &modules.caches);
    else
        msg_Dbg(bank.obj, "ignoring plugins cache file");

    if (mode & CACHE_SCAN_DIR)
    {
        msg_Dbg(obj, "recursively browsing `%s'", bank.base);

        /* Don't go deeper than 5 subdirectories */
        AllocatePluginDir(&bank, 5, path, NULL);
    }

    /* Deal with unmatched cache entries from cache file */
    while (bank.cache != NULL)
    {
        vlc_plugin_t *plugin = bank.cache;

        bank.cache = plugin->next;
        if (mode & CACHE_SCAN_DIR)
            vlc_plugin_destroy(plugin);
        else
            vlc_plugin_store(plugin);
    }

    if (mode & CACHE_WRITE_FILE)
        CacheSave(obj, path, bank.plugins, bank.size);

    free(bank.plugins);
}

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
    cache_mode_t mode = 0;

    if (var_InheritBool(p_this, "plugins-cache"))
        mode |= CACHE_READ_FILE;
    if (var_InheritBool(p_this, "plugins-scan"))
        mode |= CACHE_SCAN_DIR;
    if (var_InheritBool(p_this, "reset-plugins-cache"))
        mode = (mode | CACHE_WRITE_FILE) & ~CACHE_READ_FILE;

#if VLC_WINSTORE_APP
    /* Windows Store Apps can not load external plugins with absolute paths. */
    AllocatePluginPath (p_this, "plugins", mode);
#else
    /* Contruct the special search path for system that have a relocatable
     * executable. Set it to <vlc path>/plugins. */
    char *vlcpath = config_GetSysPath(VLC_PKG_LIB_DIR, "plugins");
    if (likely(vlcpath != NULL))
    {
        AllocatePluginPath(p_this, vlcpath, mode);
        free(vlcpath);
    }
#endif /* VLC_WINSTORE_APP */

    /* If the user provided a plugin path, we add it to the list */
    paths = getenv( "VLC_PLUGIN_PATH" );
    if( paths == NULL )
        return;

#ifdef _WIN32
    paths = realpath( paths, NULL );
#else
    paths = strdup( paths ); /* don't harm the environment ! :) */
#endif
    if( unlikely(paths == NULL) )
        return;

    for( char *buf, *path = strtok_r( paths, PATH_SEP, &buf );
         path != NULL;
         path = strtok_r( NULL, PATH_SEP, &buf ) )
        AllocatePluginPath (p_this, path, mode);

    free( paths );
}

/**
 * Ensures that a plug-in is loaded.
 *
 * \note This function is thread-safe but not re-entrant.
 *
 * \return 0 on success, -1 on failure
 */
int module_Map(struct vlc_logger *log, vlc_plugin_t *plugin)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;

    if (plugin->abspath == NULL)
        return 0; /* static module needs not be mapped */
    if (atomic_load_explicit(&plugin->handle, memory_order_acquire))
        return 0; /* fast path: already loaded */

    /* Try to load the plug-in (without locks, so read-only) */
    assert(plugin->abspath != NULL);

    void *handle = module_Open(log, plugin->abspath, false);
    if (handle == NULL)
        return -1;

    vlc_plugin_cb entry = vlc_dlsym(handle, "vlc_entry");
    if (entry == NULL)
    {
        vlc_error(log, "cannot find plug-in entry point in %s",
                  plugin->abspath);
        goto error;
    }

    vlc_mutex_lock(&lock);
    if (atomic_load_explicit(&plugin->handle, memory_order_relaxed) == 0)
    {   /* Lock is held, update the plug-in structure */
        if (vlc_plugin_resolve(plugin, entry))
        {
            vlc_mutex_unlock(&lock);
            goto error;
        }

        atomic_store_explicit(&plugin->handle, (uintptr_t)handle,
                              memory_order_release);
    }
    else /* Another thread won the race to load the plugin */
        vlc_dlclose(handle);
    vlc_mutex_unlock(&lock);

    return 0;
error:
    vlc_dlclose(handle);
    return -1;
}

/**
 * Ensures that a module is not loaded.
 *
 * \note This function is not thread-safe. The caller must ensure that the
 * plug-in is no longer used before calling this function.
 */
static void module_Unmap(vlc_plugin_t *plugin)
{
    if (!plugin->unloadable)
        return;

    void *handle = (void *)atomic_exchange_explicit(&plugin->handle, 0,
                                                    memory_order_acquire);
    if (handle != NULL)
        vlc_dlclose(handle);
}

void *module_Symbol(struct vlc_logger *log,
                    vlc_plugin_t *plugin, const char *name)
{
    if (plugin->abspath == NULL || module_Map(log, plugin))
        return NULL;

    void *handle = (void *)atomic_load_explicit(&plugin->handle,
                                                memory_order_relaxed);
    assert(handle != NULL);
    return vlc_dlsym(handle, name);
}
#else
int module_Map(struct vlc_logger *log, vlc_plugin_t *plugin)
{
    (void) log; (void) plugin;
    return 0;
}

static void module_Unmap(vlc_plugin_t *plugin)
{
    (void) plugin;
}

void *module_Symbol(struct vlc_logger *log,
                    vlc_plugin_t *plugin, const char *name)
{
    (void) log; (void) plugin; (void) name;
    return NULL;
}
#endif /* HAVE_DYNAMIC_PLUGINS */

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
        /* Fills the module bank structure with the core module infos.
         * This is very useful as it will allow us to consider the core
         * library just as another module, and for instance the configuration
         * options of core will be available in the module bank structure just
         * as for every other module. */
        vlc_plugin_t *plugin = module_InitStatic(vlc_entry__core);
        if (likely(plugin != NULL))
            vlc_plugin_store(plugin);
        config_SortConfig ();
    }
    modules.usage++;

    /* We do retain the module bank lock until the plugins are loaded as well.
     * This is ugly, this staged loading approach is needed: LibVLC gets
     * some configuration parameters relevant to loading the plugins from
     * the core (builtin) module. The module bank becomes shared read-only data
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
    vlc_plugin_t *libs = NULL;
    block_t *caches = NULL;
    void *caps_tree = NULL;

    /* If plugins were _not_ loaded, then the caller still has the bank lock
     * from module_InitBank(). */
    if( b_plugins )
        vlc_mutex_lock (&modules.lock);
    else
        vlc_mutex_assert(&modules.lock);

    assert (modules.usage > 0);
    if (--modules.usage == 0)
    {
        config_UnsortConfig ();
        libs = vlc_plugins;
        caches = modules.caches;
        caps_tree = modules.caps_tree;
        vlc_plugins = NULL;
        modules.caches = NULL;
        modules.caps_tree = NULL;
    }
    vlc_mutex_unlock (&modules.lock);

    tdestroy(caps_tree, vlc_modcap_free);

    while (libs != NULL)
    {
        vlc_plugin_t *lib = libs;

        libs = lib->next;
        module_Unmap(lib);
        vlc_plugin_destroy(lib);
    }

    block_ChainRelease(caches);
}

#undef module_LoadPlugins
/**
 * Loads module descriptions for all available plugins.
 * Fills the module bank structure with the plugin modules.
 *
 * \param p_this vlc object structure
 */
void module_LoadPlugins(vlc_object_t *obj)
{
    /*vlc_mutex_assert (&modules.lock); not for static mutexes :( */

    if (modules.usage == 1)
    {
        module_InitStaticModules ();
#ifdef HAVE_DYNAMIC_PLUGINS
        msg_Dbg (obj, "searching plug-in modules");
        AllocateAllPlugins (obj);
#endif
        config_UnsortConfig ();
        config_SortConfig ();

        twalk(modules.caps_tree, vlc_modcap_sort);
    }
    vlc_mutex_unlock (&modules.lock);

    size_t count;
    module_t **list = module_list_get (&count);
    module_list_free (list);
    msg_Dbg (obj, "plug-ins loaded: %zu modules", count);
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

    for (vlc_plugin_t *lib = vlc_plugins; lib != NULL; lib = lib->next)
    {
        module_t **nt = realloc(tab, (i + lib->modules_count) * sizeof (*tab));
        if (unlikely(nt == NULL))
        {
            free (tab);
            *n = 0;
            return NULL;
        }

        tab = nt;
        for (module_t *m = lib->module; m != NULL; m = m->next)
            tab[i++] = m;
    }
    *n = i;
    return tab;
}

size_t module_list_cap(module_t *const **restrict list, const char *name)
{
    const void **cp = tfind(&name, &modules.caps_tree, vlc_modcap_cmp);
    if (cp == NULL)
    {
        *list = NULL;
        return 0;
    }

    const vlc_modcap_t *cap = *cp;

    *list = cap->modv;
    return cap->modc;
}
