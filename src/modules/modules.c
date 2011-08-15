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

int vlc_entry__main( module_t * );

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
static module_t * AllocatePlugin( vlc_object_t *, const char *, bool );
#endif
static int  AllocateBuiltinModule( vlc_object_t *, int ( * ) ( module_t * ) );
static void DeleteModule (module_t **, module_t *);
#ifdef HAVE_DYNAMIC_PLUGINS
static void   DupModule        ( module_t * );
static void   UndupModule      ( module_t * );
#endif

#undef module_InitBank
/**
 * Init bank
 *
 * Creates a module bank structure which will be filled later
 * on with all the modules found.
 * \param p_this vlc object structure
 * \return nothing
 */
void module_InitBank( vlc_object_t *p_this )
{
    vlc_mutex_lock (&modules.lock);

    if (modules.usage == 0)
    {
        /* Fills the module bank structure with the main module infos.
         * This is very useful as it will allow us to consider the main
         * library just as another module, and for instance the configuration
         * options of main will be available in the module bank structure just
         * as for every other module. */
        AllocateBuiltinModule( p_this, vlc_entry__main );
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

#undef module_EndBank
/**
 * Unloads all unused plugin modules and empties the module
 * bank in case of success.
 * \param p_this vlc object structure
 * \return nothing
 */
void module_EndBank( vlc_object_t *p_this, bool b_plugins )
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
        DeleteModule (&head, head);
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

module_t *module_hold (module_t *m)
{
    vlc_hold (&m->vlc_gc_data);
    return m;
}

void module_release (module_t *m)
{
    vlc_release (&m->vlc_gc_data);
}

#undef module_start
int module_start (vlc_object_t *obj, module_t *m)
{
   int (*activate) (vlc_object_t *) = m->pf_activate;

   return (activate != NULL) ? activate (obj) : VLC_SUCCESS;
}

#undef module_stop
void module_stop (vlc_object_t *obj, module_t *m)
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
    if (list == NULL)
        return;

    for (size_t i = 0; list[i] != NULL; i++)
         module_release (list[i]);
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
         tab[i++] = module_hold (mod);
         for (module_t *subm = mod->submodule; subm; subm = subm->next)
             tab[i++] = module_hold (subm);
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
        p_list[count].p_module = module_hold (p_module);
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

        if( !p_real->b_builtin && !p_real->b_loaded )
        {
            module_t *p_new_module =
                AllocatePlugin( p_this, p_real->psz_filename, false );
            if( p_new_module == NULL )
            {   /* Corrupted module */
                msg_Err( p_this, "possibly corrupt module cache" );
                module_release( p_cand );
                continue;
            }
            CacheMerge( p_this, p_real, p_new_module );
            DeleteModule (&modules.head, p_new_module);
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
            module_release( p_cand );
            break;

        default: /* bad module */
            module_release( p_cand );
            continue;
        }

        /* Release the remaining modules */
        while (++i < count)
            module_release (p_list[i].p_module);
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
    module_release(module);
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
        {
            module_hold (module);
            break;
        }
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
    module_t *p_module = module_find (psz_name);
    if( p_module )
        module_release (p_module);
    return p_module != NULL;
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
    {
        for (size_t j = 0; j < module->i_shortcuts; j++)
        {
            if (!strcmp (module->pp_shortcuts[j], psz_shortcut))
            {
                module_hold (module);
                goto out;
             }
        }
    }
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
            for( size_t i = 0; i < count; i++ )
            {
                if (cache[i].p_module != NULL)
                   DeleteModule (&modules.head, cache[i].p_module);
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
 * and module_unneed. It can be removed by DeleteModule.
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
        p_module = AllocatePlugin( p_this, path, true );
    if( p_module == NULL )
        return -1;

    /* We have not already scanned and inserted this module */
    assert( p_module->next == NULL );

    /* Unload plugin until we really need it */
    assert( !p_module->b_builtin );
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
             assert( !p_module->b_loaded );
             DeleteModule (&modules.head, p_module);
             p_module = AllocatePlugin( p_this, path, false );
             break;
         }

    p_module->next = modules.head;
    modules.head = p_module;

    if( mode == CACHE_IGNORE )
        return 0;

    /* Add entry to cache */
    CacheAdd (&p_bank->cache, &p_bank->i_cache, path, st, p_module);
    /* TODO: deal with errors */
    return  0;
}

/*****************************************************************************
 * AllocatePlugin: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_need
 * and module_unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static module_t *AllocatePlugin( vlc_object_t * p_this, const char *psz_file,
                                 bool fast )
{
    module_t * p_module = NULL;
    module_handle_t handle;

    if( module_Load( p_this, psz_file, &handle, fast ) )
        return NULL;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */
    p_module = vlc_module_create();
    if( p_module == NULL )
    {
        module_Unload( handle );
        return NULL;
    }

    p_module->psz_filename = strdup( psz_file );
    p_module->handle = handle;
    p_module->b_loaded = true;

    /* Initialize the module: fill p_module, default config */
    static const char entry[] = "vlc_entry" MODULE_SUFFIX;

    /* Try to resolve the symbol */
    int (*pf_symbol)(module_t * p_module)
        = (int (*)(module_t *)) module_Lookup( p_module->handle,entry );
    if( pf_symbol == NULL )
    {
        msg_Warn( p_this, "cannot find symbol \"%s\" in plugin `%s'",
                  entry, psz_file );
        goto error;
    }
    else
    /* We can now try to call the symbol */
    if( pf_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_this, "cannot initialize plugin `%s'", psz_file );
        goto error;
    }

    DupModule( p_module );
    assert( !p_module->b_builtin );
    return p_module;
error:
    free( p_module->psz_filename );
    module_release( p_module );
    module_Unload( handle );
    return NULL;
}

/*****************************************************************************
 * DupModule: make a plugin module standalone.
 *****************************************************************************
 * This function duplicates all strings in the module, so that the dynamic
 * object can be unloaded. It acts recursively on submodules.
 *****************************************************************************/
static void DupModule( module_t *p_module )
{
    char **pp_shortcuts = p_module->pp_shortcuts;
    for( unsigned i = 0; i < p_module->i_shortcuts; i++ )
        pp_shortcuts[i] = strdup( p_module->pp_shortcuts[i] );

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->psz_capability =
        p_module->psz_capability ? strdup( p_module->psz_capability ) : NULL;
    p_module->psz_shortname = p_module->psz_shortname ?
                                 strdup( p_module->psz_shortname ) : NULL;
    p_module->psz_longname = strdup( p_module->psz_longname );
    p_module->psz_help = p_module->psz_help ? strdup( p_module->psz_help )
                                            : NULL;
    p_module->domain = p_module->domain ? strdup( p_module->domain ) : NULL;

    for (module_t *subm = p_module->submodule; subm; subm = subm->next)
        DupModule (subm);
}

/*****************************************************************************
 * UndupModule: free a duplicated module.
 *****************************************************************************
 * This function frees the allocations done in DupModule().
 *****************************************************************************/
static void UndupModule( module_t *p_module )
{
    char **pp_shortcuts = p_module->pp_shortcuts;

    for (module_t *subm = p_module->submodule; subm; subm = subm->next)
        UndupModule (subm);

    for( unsigned i = 0; i < p_module->i_shortcuts; i++ )
        free( pp_shortcuts[i] );

    free( p_module->psz_capability );
    FREENULL( p_module->psz_shortname );
    free( p_module->psz_longname );
    FREENULL( p_module->psz_help );
    free( p_module->domain );
}

#endif /* HAVE_DYNAMIC_PLUGINS */

/*****************************************************************************
 * AllocateBuiltinModule: initialize a builtin module.
 *****************************************************************************
 * This function registers a builtin module and allocates a structure
 * for its information data. The module can then be handled by module_need
 * and module_unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocateBuiltinModule( vlc_object_t * p_this,
                                  int ( *pf_entry ) ( module_t * ) )
{
    module_t * p_module;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */
    p_module = vlc_module_create();
    if( p_module == NULL )
        return -1;

    /* Initialize the module : fill *p_module structure */
    if( pf_entry( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_this, "failed calling entry point in builtin module" );
        module_release( p_module );
        return -1;
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->b_builtin = true;
    /* LOCK */
    p_module->next = modules.head;
    modules.head = p_module;
    /* UNLOCK */

    return 0;
}

/*****************************************************************************
 * DeleteModule: delete a module and its structure.
 *****************************************************************************
 * This function can only be called if the module isn't being used.
 *****************************************************************************/
static void DeleteModule (module_t **head, module_t *p_module)
{
    assert( p_module );

    /* Unlist the module (if it is in the list) */
    module_t **pp_self = head;
    while (*pp_self != NULL && *pp_self != p_module)
        pp_self = &((*pp_self)->next);
    if (*pp_self)
        *pp_self = p_module->next;

    /* We free the structures that we strdup()ed in Allocate*Module(). */
#ifdef HAVE_DYNAMIC_PLUGINS
    if( !p_module->b_builtin )
    {
        if( p_module->b_loaded && p_module->b_unloadable )
        {
            module_Unload( p_module->handle );
        }
        UndupModule( p_module );
        free( p_module->psz_filename );
    }
#endif

    /* Free and detach the object's children */
    while (p_module->submodule)
    {
        module_t *submodule = p_module->submodule;
        p_module->submodule = submodule->next;
        module_release (submodule);
    }

    config_Free( p_module );
    module_release( p_module );
}
