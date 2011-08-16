/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001-2011 the VideoLAN team
 * $Id$
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_memory.h>
#include <vlc_modules.h>
#include "libvlc.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */
#include <assert.h>

#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifdef ENABLE_NLS
# include <libintl.h>
#endif

#include "config/configuration.h"

#include <vlc_fs.h>
#include "vlc_arrays.h"

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
 * Checks whether a module implements a capability.
 *
 * \param m the module
 * \param cap the capability to check
 * \return TRUE if the module have the capability
 */
bool module_provides( const module_t *m, const char *cap )
{
    if (unlikely(m->psz_capability == NULL))
        return false;
    return !strcmp( m->psz_capability, cap );
}

/**
 * Get the internal name of a module
 *
 * \param m the module
 * \return the module name
 */
const char *module_get_object( const module_t *m )
{
    if (unlikely(m->i_shortcuts == 0))
        return "unnamed";
    return m->pp_shortcuts[0];
}

/**
 * Get the human-friendly name of a module.
 *
 * \param m the module
 * \param long_name TRUE to have the long name of the module
 * \return the short or long name of the module
 */
const char *module_get_name( const module_t *m, bool long_name )
{
    if( long_name && ( m->psz_longname != NULL) )
        return m->psz_longname;

    if (m->psz_shortname != NULL)
        return m->psz_shortname;
    return module_get_object (m);
}

/**
 * Get the help for a module
 *
 * \param m the module
 * \return the help
 */
const char *module_get_help( const module_t *m )
{
    return m->psz_help;
}

/**
 * Get the capability for a module
 *
 * \param m the module
 * return the capability
 */
const char *module_get_capability( const module_t *m )
{
    return m->psz_capability;
}

/**
 * Get the score for a module
 *
 * \param m the module
 * return the score for the capability
 */
int module_get_score( const module_t *m )
{
    return m->i_score;
}

/**
 * Translate a string using the module's text domain
 *
 * \param m the module
 * \param str the American English ASCII string to localize
 * \return the gettext-translated string
 */
const char *module_gettext (const module_t *m, const char *str)
{
    if (m->parent != NULL)
        m = m->parent;
    if (unlikely(str == NULL || *str == '\0'))
        return "";
#ifdef ENABLE_NLS
    const char *domain = m->domain;
    return dgettext ((domain != NULL) ? domain : PACKAGE_NAME, str);
#else
    (void)m;
    return str;
#endif
}

#undef module_start
int module_start (vlc_object_t *obj, const module_t *m)
{
   int (*activate) (vlc_object_t *) = m->pf_activate;

   return (activate != NULL) ? activate (obj) : VLC_SUCCESS;
}

#undef module_stop
void module_stop (vlc_object_t *obj, const module_t *m)
{
   void (*deactivate) (vlc_object_t *) = m->pf_deactivate;

    if (deactivate != NULL)
        deactivate (obj);
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

typedef struct module_list_t
{
    module_t *p_module;
    int16_t  i_score;
    bool     b_force;
} module_list_t;

static int modulecmp (const void *a, const void *b)
{
    const module_list_t *la = a, *lb = b;
    /* Note that qsort() uses _ascending_ order,
     * so the smallest module is the one with the biggest score. */
    return lb->i_score - la->i_score;
}

#undef vlc_module_load
/**
 * Finds and instantiates the best module of a certain type.
 * All candidates modules having the specified capability and name will be
 * sorted in decreasing order of priority. Then the probe callback will be
 * invoked for each module, until it succeeds (returns 0), or all candidate
 * module failed to initialize.
 *
 * The probe callback first parameter is the address of the module entry point.
 * Further parameters are passed as an argument list; it corresponds to the
 * variable arguments passed to this function. This scheme is meant to
 * support arbitrary prototypes for the module entry point.
 *
 * \param p_this VLC object
 * \param psz_capability capability, i.e. class of module
 * \param psz_name name name of the module asked, if any
 * \param b_strict if true, do not fallback to plugin with a different name
 *                 but the same capability
 * \param probe module probe callback
 * \return the module or NULL in case of a failure
 */
module_t *vlc_module_load(vlc_object_t *p_this, const char *psz_capability,
                          const char *psz_name, bool b_strict,
                          vlc_activate_t probe, ...)
{
    stats_TimerStart( p_this, "module_need()", STATS_TIMER_MODULE_NEED );

    module_list_t *p_list;
    module_t *p_module;
    int i_shortcuts = 0;
    char *psz_shortcuts = NULL, *psz_var = NULL, *psz_alias = NULL;
    bool b_force_backup = p_this->b_force;

    /* Deal with variables */
    if( psz_name && psz_name[0] == '$' )
    {
        psz_name = psz_var = var_CreateGetString( p_this, psz_name + 1 );
    }

    /* Count how many different shortcuts were asked for */
    if( psz_name && *psz_name )
    {
        char *psz_parser, *psz_last_shortcut;

        /* If the user wants none, give him none. */
        if( !strcmp( psz_name, "none" ) )
        {
            free( psz_var );
            stats_TimerStop( p_this, STATS_TIMER_MODULE_NEED );
            stats_TimerDump( p_this, STATS_TIMER_MODULE_NEED );
            stats_TimerClean( p_this, STATS_TIMER_MODULE_NEED );
            return NULL;
        }

        i_shortcuts++;
        psz_parser = psz_shortcuts = psz_last_shortcut = strdup( psz_name );

        while( ( psz_parser = strchr( psz_parser, ',' ) ) )
        {
             *psz_parser = '\0';
             i_shortcuts++;
             psz_last_shortcut = ++psz_parser;
        }

        /* Check if the user wants to override the "strict" mode */
        if( psz_last_shortcut )
        {
            if( !strcmp(psz_last_shortcut, "none") )
            {
                b_strict = true;
                i_shortcuts--;
            }
            else if( !strcmp(psz_last_shortcut, "any") )
            {
                b_strict = false;
                i_shortcuts--;
            }
        }
    }

    /* Sort the modules and test them */
    size_t count;
    module_t **p_all = module_list_get (&count);
    p_list = malloc( count * sizeof( module_list_t ) );

    /* Parse the module list for capabilities and probe each of them */
    count = 0;
    for (size_t i = 0; (p_module = p_all[i]) != NULL; i++)
    {
        int i_shortcut_bonus = 0;

        /* Test that this module can do what we need */
        if( !module_provides( p_module, psz_capability ) )
            continue;

        /* If we required a shortcut, check this plugin provides it. */
        if( i_shortcuts > 0 )
        {
            const char *name = psz_shortcuts;

            for( unsigned i_short = i_shortcuts; i_short > 0; i_short-- )
            {
                for( unsigned i = 0; i < p_module->i_shortcuts; i++ )
                {
                    char *c;
                    if( ( c = strchr( name, '@' ) )
                        ? !strncasecmp( name, p_module->pp_shortcuts[i],
                                        c-name )
                        : !strcasecmp( name, p_module->pp_shortcuts[i] ) )
                    {
                        /* Found it */
                        if( c && c[1] )
                            psz_alias = c+1;
                        i_shortcut_bonus = i_short * 10000;
                        goto found_shortcut;
                    }
                }

                /* Go to the next shortcut... This is so lame! */
                name += strlen( name ) + 1;
            }

            /* If we are in "strict" mode and we couldn't
             * find the module in the list of provided shortcuts,
             * then kick the bastard out of here!!! */
            if( b_strict )
                continue;
        }

        /* Trash <= 0 scored plugins (they can only be selected by shortcut) */
        if( p_module->i_score <= 0 )
            continue;

found_shortcut:
        /* Store this new module */
        p_list[count].p_module = p_module;
        p_list[count].i_score = p_module->i_score + i_shortcut_bonus;
        p_list[count].b_force = i_shortcut_bonus && b_strict;
        count++;
    }

    /* We can release the list, interesting modules are held */
    module_list_free (p_all);

    /* Sort candidates by descending score */
    qsort (p_list, count, sizeof (p_list[0]), modulecmp);
    msg_Dbg( p_this, "looking for %s module: %zu candidate%s", psz_capability,
             count, count == 1 ? "" : "s" );

    /* Parse the linked list and use the first successful module */
    va_list args;

    va_start(args, probe);
    p_module = NULL;

    for (size_t i = 0; (i < count) && (p_module == NULL); i++)
    {
        module_t *p_cand = p_list[i].p_module;
#ifdef HAVE_DYNAMIC_PLUGINS
        /* Make sure the module is loaded in mem */
        module_t *p_real = p_cand->parent ? p_cand->parent : p_cand;

        if (!p_real->b_loaded)
        {
            module_t *uncache; /* Map module in process */

            assert (p_real->psz_filename != NULL);
            uncache = module_InitDynamic (p_this, p_real->psz_filename, false);
            if (uncache == NULL)
            {   /* Corrupted module */
                msg_Err( p_this, "possibly corrupt module cache" );
                continue;
            }
            CacheMerge (p_this, p_real, uncache);
            vlc_module_destroy (uncache);
        }
#endif
        p_this->b_force = p_list[i].b_force;

        int ret;

        if (likely(p_cand->pf_activate != NULL))
        {
            va_list ap;

            va_copy(ap, args);
            ret = probe(p_cand->pf_activate, ap);
            va_end(ap);
        }
        else
            ret = VLC_SUCCESS;

        switch (ret)
        {
        case VLC_SUCCESS:
            /* good module! */
            p_module = p_cand;
            break;

        case VLC_ETIMEOUT:
            /* good module, but aborted */
            break;

        default: /* bad module */
            continue;
        }
    }

    va_end (args);
    free( p_list );
    p_this->b_force = b_force_backup;

    if( p_module != NULL )
    {
        msg_Dbg( p_this, "using %s module \"%s\"",
                 psz_capability, module_get_object(p_module) );
        vlc_object_set_name( p_this, psz_alias ? psz_alias
                                               : module_get_object(p_module) );
    }
    else if( count == 0 )
        msg_Dbg( p_this, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
    else
        msg_Dbg( p_this, "no %s module matching \"%s\" could be loaded",
                  psz_capability, (psz_name && *psz_name) ? psz_name : "any" );

    free( psz_shortcuts );
    free( psz_var );

    stats_TimerStop( p_this, STATS_TIMER_MODULE_NEED );
    stats_TimerDump( p_this, STATS_TIMER_MODULE_NEED );
    stats_TimerClean( p_this, STATS_TIMER_MODULE_NEED );

    /* Don't forget that the module is still locked */
    return p_module;
}


/**
 * Deinstantiates a module.
 * \param module the module pointer as returned by vlc_module_load()
 * \param deinit deactivation callback
 */
void vlc_module_unload(module_t *module, vlc_deactivate_t deinit, ...)
{
    if (module->pf_deactivate != NULL)
    {
        va_list ap;

        va_start(ap, deinit);
        deinit(module->pf_deactivate, ap);
        va_end(ap);
    }
}


static int generic_start(void *func, va_list ap)
{
    vlc_object_t *obj = va_arg(ap, vlc_object_t *);
    int (*activate)(vlc_object_t *) = func;

    return activate(obj);
}

static void generic_stop(void *func, va_list ap)
{
    vlc_object_t *obj = va_arg(ap, vlc_object_t *);
    void (*deactivate)(vlc_object_t *) = func;

    deactivate(obj);
}

#undef module_need
module_t *module_need(vlc_object_t *obj, const char *cap, const char *name,
                      bool strict)
{
    return vlc_module_load(obj, cap, name, strict, generic_start, obj);
}

#undef module_unneed
void module_unneed(vlc_object_t *obj, module_t *module)
{
    msg_Dbg(obj, "removing module \"%s\"", module_get_object(module));
    vlc_module_unload(module, generic_stop, obj);
}

/**
 * Get a pointer to a module_t given it's name.
 *
 * \param name the name of the module
 * \return a pointer to the module or NULL in case of a failure
 */
module_t *module_find (const char *name)
{
    module_t **list, *module;

    assert (name != NULL);
    list = module_list_get (NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; (module = list[i]) != NULL; i++)
    {
        if (unlikely(module->i_shortcuts == 0))
            continue;
        if (!strcmp (module->pp_shortcuts[0], name))
            break;
    }
    module_list_free (list);
    return module;
}

/**
 * Tell if a module exists and release it in thic case
 *
 * \param psz_name th name of the module
 * \return TRUE if the module exists
 */
bool module_exists (const char * psz_name)
{
    return module_find (psz_name) != NULL;
}

/**
 * Get a pointer to a module_t that matches a shortcut.
 * This is a temporary hack for SD. Do not re-use (generally multiple modules
 * can have the same shortcut, so this is *broken* - use module_need()!).
 *
 * \param psz_shortcut shortcut of the module
 * \param psz_cap capability of the module
 * \return a pointer to the module or NULL in case of a failure
 */
module_t *module_find_by_shortcut (const char *psz_shortcut)
{
    module_t **list, *module;

    list = module_list_get (NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; (module = list[i]) != NULL; i++)
        for (size_t j = 0; j < module->i_shortcuts; j++)
            if (!strcmp (module->pp_shortcuts[j], psz_shortcut))
                goto out;
out:
    module_list_free (list);
    return module;
}

/**
 * Get the configuration of a module
 *
 * \param module the module
 * \param psize the size of the configuration returned
 * \return the configuration as an array
 */
module_config_t *module_config_get( const module_t *module, unsigned *restrict psize )
{
    unsigned i,j;
    unsigned size = module->confsize;
    module_config_t *config = malloc( size * sizeof( *config ) );

    assert( psize != NULL );
    *psize = 0;

    if( !config )
        return NULL;

    for( i = 0, j = 0; i < size; i++ )
    {
        const module_config_t *item = module->p_config + i;
        if( item->b_internal /* internal option */
         || item->b_removed /* removed option */ )
            continue;

        memcpy( config + j, item, sizeof( *config ) );
        j++;
    }
    *psize = j;

    return config;
}

/**
 * Release the configuration
 *
 * \param the configuration
 * \return nothing
 */
void module_config_free( module_config_t *config )
{
    free( config );
}

/*****************************************************************************
 * Following functions are local.
 *****************************************************************************/

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
