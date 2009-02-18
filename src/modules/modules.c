/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include "libvlc.h"

/* Some faulty libcs have a broken struct dirent when _FILE_OFFSET_BITS
 * is set to 64. Don't try to be cleverer. */
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */
#include <assert.h>

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if !defined(HAVE_DYNAMIC_PLUGINS)
    /* no support for plugins */
#elif defined(HAVE_DL_DYLD)
#   if defined(HAVE_MACH_O_DYLD_H)
#       include <mach-o/dyld.h>
#   endif
#elif defined(HAVE_DL_BEOS)
#   if defined(HAVE_IMAGE_H)
#       include <image.h>
#   endif
#elif defined(HAVE_DL_WINDOWS)
#   include <windows.h>
#elif defined(HAVE_DL_DLOPEN)
#   if defined(HAVE_DLFCN_H) /* Linux, BSD, Hurd */
#       include <dlfcn.h>
#   endif
#   if defined(HAVE_SYS_DL_H)
#       include <sys/dl.h>
#   endif
#elif defined(HAVE_DL_SHL_LOAD)
#   if defined(HAVE_DL_H)
#       include <dl.h>
#   endif
#endif

#include "config/configuration.h"

#include "vlc_charset.h"
#include "vlc_arrays.h"

#include "modules/modules.h"

static module_bank_t *p_module_bank = NULL;
static vlc_mutex_t module_lock = VLC_STATIC_MUTEX;

int vlc_entry__main( module_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins( vlc_object_t *, module_bank_t * );
static void AllocatePluginDir( vlc_object_t *, module_bank_t *, const char *,
                               unsigned );
static int  AllocatePluginFile( vlc_object_t *, module_bank_t *, const char *,
                                int64_t, int64_t );
static module_t * AllocatePlugin( vlc_object_t *, const char * );
#endif
static int  AllocateBuiltinModule( vlc_object_t *, int ( * ) ( module_t * ) );
static void DeleteModule ( module_bank_t *, module_t * );
#ifdef HAVE_DYNAMIC_PLUGINS
static void   DupModule        ( module_t * );
static void   UndupModule      ( module_t * );
#endif

/**
 * Init bank
 *
 * Creates a module bank structure which will be filled later
 * on with all the modules found.
 * \param p_this vlc object structure
 * \return nothing
 */
void __module_InitBank( vlc_object_t *p_this )
{
    module_bank_t *p_bank = NULL;

    vlc_mutex_lock( &module_lock );

    if( p_module_bank == NULL )
    {
        p_bank = calloc (1, sizeof(*p_bank));
        p_bank->i_usage = 1;
        p_bank->i_cache = p_bank->i_loaded_cache = 0;
        p_bank->pp_cache = p_bank->pp_loaded_cache = NULL;
        p_bank->b_cache = p_bank->b_cache_dirty = false;
        p_bank->head = NULL;

        /* Everything worked, attach the object */
        p_module_bank = p_bank;

        /* Fills the module bank structure with the main module infos.
         * This is very useful as it will allow us to consider the main
         * library just as another module, and for instance the configuration
         * options of main will be available in the module bank structure just
         * as for every other module. */
        AllocateBuiltinModule( p_this, vlc_entry__main );
    }
    else
        p_module_bank->i_usage++;

    /* We do retain the module bank lock until the plugins are loaded as well.
     * This is ugly, this staged loading approach is needed: LibVLC gets
     * some configuration parameters relevant to loading the plugins from
     * the main (builtin) module. The module bank becomes shared read-only data
     * once it is ready, so we need to fully serialize initialization.
     * DO NOT UNCOMMENT the following line unless you managed to squeeze
     * module_LoadPlugins() before you unlock the mutex. */
    /*vlc_mutex_unlock( &module_lock );*/
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
    module_bank_t *p_bank = p_module_bank;

    assert (p_bank != NULL);

    /* Save the configuration */
    config_AutoSaveConfigFile( p_this );

    /* If plugins were _not_ loaded, then the caller still has the bank lock
     * from module_InitBank(). */
    if( b_plugins )
        vlc_mutex_lock( &module_lock );
    /*else
        vlc_assert_locked( &module_lock ); not for static mutexes :( */

    if( --p_bank->i_usage > 0 )
    {
        vlc_mutex_unlock( &module_lock );
        return;
    }
    p_module_bank = NULL;
    vlc_mutex_unlock( &module_lock );

#ifdef HAVE_DYNAMIC_PLUGINS
    if( p_bank->b_cache )
        CacheSave( p_this, p_bank );
    while( p_bank->i_loaded_cache-- )
    {
        if( p_bank->pp_loaded_cache[p_bank->i_loaded_cache] )
        {
            DeleteModule( p_bank,
                    p_bank->pp_loaded_cache[p_bank->i_loaded_cache]->p_module );
            free( p_bank->pp_loaded_cache[p_bank->i_loaded_cache]->psz_file );
            free( p_bank->pp_loaded_cache[p_bank->i_loaded_cache] );
            p_bank->pp_loaded_cache[p_bank->i_loaded_cache] = NULL;
        }
    }
    if( p_bank->pp_loaded_cache )
    {
        free( p_bank->pp_loaded_cache );
        p_bank->pp_loaded_cache = NULL;
    }
    while( p_bank->i_cache-- )
    {
        free( p_bank->pp_cache[p_bank->i_cache]->psz_file );
        free( p_bank->pp_cache[p_bank->i_cache] );
        p_bank->pp_cache[p_bank->i_cache] = NULL;
    }
    if( p_bank->pp_cache )
    {
        free( p_bank->pp_cache );
        p_bank->pp_cache = NULL;
    }
#endif

    while( p_bank->head != NULL )
        DeleteModule( p_bank, p_bank->head );

    free( p_bank );
}

#undef module_LoadPlugins
/**
 * Loads module descriptions for all available plugins.
 * Fills the module bank structure with the plugin modules.
 *
 * \param p_this vlc object structure
 * \return nothing
 */
void module_LoadPlugins( vlc_object_t * p_this, bool b_cache_delete )
{
    module_bank_t *p_bank = p_module_bank;

    assert( p_bank );
    /*vlc_assert_locked( &module_lock ); not for static mutexes :( */

#ifdef HAVE_DYNAMIC_PLUGINS
    if( p_bank->i_usage == 1 )
    {
        msg_Dbg( p_this, "checking plugin modules" );
        p_module_bank->b_cache = config_GetInt( p_this, "plugins-cache" ) > 0;

        if( p_module_bank->b_cache || b_cache_delete )
            CacheLoad( p_this, p_module_bank, b_cache_delete );
        AllocateAllPlugins( p_this, p_module_bank );
    }
#endif
    p_module_bank->b_plugins = true;
    vlc_mutex_unlock( &module_lock );
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
    return m->psz_object_name;
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

    return m->psz_shortname ?: m->psz_object_name;
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

module_t *module_hold (module_t *m)
{
    vlc_hold (&m->vlc_gc_data);
    return m;
}

void module_release (module_t *m)
{
    vlc_release (&m->vlc_gc_data);
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

    assert (p_module_bank);
    for (module_t *mod = p_module_bank->head; mod; mod = mod->next)
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

/**
 * module Need
 *
 * Return the best module function, given a capability list.
 *
 * If the p_this object doesn't have it's psz_object_name set, then
 * psz_object_name will be set to the module's name, unless the user
 * provided an alias using the "module name@alias" syntax in which case
 * psz_object_name will be set to the alias.
 *
 * \param p_this the vlc object
 * \param psz_capability list of capabilities needed
 * \param psz_name name of the module asked
 * \param b_strict TRUE yto use the strict mode
 * \return the module or NULL in case of a failure
 */
module_t * __module_need( vlc_object_t *p_this, const char *psz_capability,
                          const char *psz_name, bool b_strict )
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
        psz_shortcuts = psz_last_shortcut = strdup( psz_name );

        for( psz_parser = psz_shortcuts; *psz_parser; psz_parser++ )
        {
            if( *psz_parser == ',' )
            {
                 *psz_parser = '\0';
                 i_shortcuts++;
                 psz_last_shortcut = psz_parser + 1;
            }
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
    unsigned i_cpu = vlc_CPU();

    /* Parse the module list for capabilities and probe each of them */
    count = 0;
    for (size_t i = 0; (p_module = p_all[i]) != NULL; i++)
    {
        bool b_shortcut_bonus = false;

        /* Test that this module can do what we need */
        if( !module_provides( p_module, psz_capability ) )
            continue;
        /* Test if we have the required CPU */
        if( (p_module->i_cpu & i_cpu) != p_module->i_cpu )
            continue;

        /* If we required a shortcut, check this plugin provides it. */
        if( i_shortcuts > 0 )
        {
            const char *psz_name = psz_shortcuts;

            for( unsigned i_short = i_shortcuts; i_short > 0; i_short-- )
            {
                for( unsigned i = 0; p_module->pp_shortcuts[i]; i++ )
                {
                    char *c;
                    if( ( c = strchr( psz_name, '@' ) )
                        ? !strncasecmp( psz_name, p_module->pp_shortcuts[i],
                                        c-psz_name )
                        : !strcasecmp( psz_name, p_module->pp_shortcuts[i] ) )
                    {
                        /* Found it */
                        if( c && c[1] )
                            psz_alias = c+1;
                        b_shortcut_bonus = true;
                        goto found_shortcut;
                    }
                }

                /* Go to the next shortcut... This is so lame! */
                psz_name += strlen( psz_name ) + 1;
            }

            /* If we are in "strict" mode and we couldn't
             * find the module in the list of provided shortcuts,
             * then kick the bastard out of here!!! */
            if( b_strict )
                continue;
        }
        /* If we didn't require a shortcut, trash <= 0 scored plugins */
        else if( p_module->i_score <= 0 )
        {
            continue;
        }

found_shortcut:
        /* Store this new module */
        p_list[count].p_module = module_hold (p_module);
        p_list[count].i_score = p_module->i_score;
        if( b_shortcut_bonus )
            p_list[count].i_score += 10000;
        p_list[count].b_force = b_shortcut_bonus && b_strict;
        count++;
    }

    /* We can release the list, interesting modules are held */
    module_list_free (p_all);

    /* Sort candidates by descending score */
    qsort (p_list, count, sizeof (p_list[0]), modulecmp);
#ifdef WIN32
    /* FIXME: Remove this hack after finding a general solution for %z's */
    msg_Dbg( p_this, "looking for %s module: %u candidate%s", psz_capability,
             count, count == 1 ? "" : "s" );
#else
    msg_Dbg( p_this, "looking for %s module: %zu candidate%s", psz_capability,
             count, count == 1 ? "" : "s" );
#endif

    /* Parse the linked list and use the first successful module */
    p_module = NULL;
    for (size_t i = 0; (i < count) && (p_module == NULL); i++)
    {
        module_t *p_cand = p_list[i].p_module;
#ifdef HAVE_DYNAMIC_PLUGINS
        /* Make sure the module is loaded in mem */
        module_t *p_real = p_cand->b_submodule ? p_cand->parent : p_cand;

        if( !p_real->b_builtin && !p_real->b_loaded )
        {
            module_t *p_new_module =
                AllocatePlugin( p_this, p_real->psz_filename );
            if( p_new_module )
            {
                CacheMerge( p_this, p_real, p_new_module );
                DeleteModule( p_module_bank, p_new_module );
            }
        }
#endif

        p_this->b_force = p_list[i].b_force;
        if( p_cand->pf_activate
         && p_cand->pf_activate( p_this ) == VLC_SUCCESS )
        {
            p_module = p_cand;
            /* Release the remaining modules */
            while (++i < count)
                module_release (p_list[i].p_module);
        }
        else
            module_release( p_cand );
    }

    free( p_list );
    p_this->b_force = b_force_backup;

    if( p_module != NULL )
    {
        msg_Dbg( p_this, "using %s module \"%s\"",
                 psz_capability, p_module->psz_object_name );
        if( !p_this->psz_object_name )
        {
            /* This assumes that p_this is the object which will be using the
             * module. That's not always the case ... but it is in most cases.
             */
            if( psz_alias )
                p_this->psz_object_name = strdup( psz_alias );
            else
                p_this->psz_object_name = strdup( p_module->psz_object_name );
        }
    }
    else if( count == 0 )
    {
        if( !strcmp( psz_capability, "access_demux" )
         || !strcmp( psz_capability, "stream_filter" )
         || !strcmp( psz_capability, "vout_window" ) )
        {
            msg_Dbg( p_this, "no %s module matched \"%s\"",
                psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
        }
        else
        {
            msg_Err( p_this, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );

            msg_StackSet( VLC_EGENERIC, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
        }
    }
    else if( psz_name != NULL && *psz_name )
    {
        msg_Warn( p_this, "no %s module matching \"%s\" could be loaded",
                  psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
    }
    else
        msg_StackSet( VLC_EGENERIC, "no suitable %s module", psz_capability );

    free( psz_shortcuts );
    free( psz_var );

    stats_TimerStop( p_this, STATS_TIMER_MODULE_NEED );
    stats_TimerDump( p_this, STATS_TIMER_MODULE_NEED );
    stats_TimerClean( p_this, STATS_TIMER_MODULE_NEED );

    /* Don't forget that the module is still locked */
    return p_module;
}

/**
 * Module unneed
 *
 * This function must be called by the thread that called module_need, to
 * decrease the reference count and allow for hiding of modules.
 * \param p_this vlc object structure
 * \param p_module the module structure
 * \return nothing
 */
void __module_unneed( vlc_object_t * p_this, module_t * p_module )
{
    /* Use the close method */
    if( p_module->pf_deactivate )
    {
        p_module->pf_deactivate( p_this );
    }

    msg_Dbg( p_this, "removing module \"%s\"", p_module->psz_object_name );

    module_release( p_module );
}

/**
 * Get a pointer to a module_t given it's name.
 *
 * \param psz_name the name of the module
 * \return a pointer to the module or NULL in case of a failure
 */
module_t *module_find( const char * psz_name )
{
    module_t **list, *module;

    list = module_list_get (NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; (module = list[i]) != NULL; i++)
    {
        const char *psz_module_name = module->psz_object_name;

        if( psz_module_name && !strcmp( psz_module_name, psz_name ) )
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
        for (size_t j = 0;
             (module->pp_shortcuts[j] != NULL) && (j < MODULE_SHORTCUT_MAX);
             j++)
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
 * GetModuleNamesForCapability
 *
 * Return a NULL terminated array with the names of the modules
 * that have a certain capability.
 * Free after uses both the string and the table.
 * \param psz_capability the capability asked
 * \param pppsz_longname an pointer to an array of string to contain
    the long names of the modules. If set to NULL the function don't use it.
 * \return the NULL terminated array
 */
char ** module_GetModulesNamesForCapability( const char *psz_capability,
                                             char ***pppsz_longname )
{
    size_t count = 0;
    char **psz_ret;

    module_t **list = module_list_get (NULL);

    /* Do it in two passes : count the number of modules before */
    for (size_t i = 0; list[i]; i++)
    {
        module_t *p_module = list[i];
        const char *psz_module_capability = p_module->psz_capability;

        if( psz_module_capability && !strcmp( psz_module_capability, psz_capability ) )
            count++;
    }

    psz_ret = malloc( sizeof(char*) * (count+1) );
    if( pppsz_longname )
        *pppsz_longname = malloc( sizeof(char*) * (count+1) );
    if( !psz_ret || ( pppsz_longname && *pppsz_longname == NULL ) )
    {
        free( psz_ret );
        if( pppsz_longname )
        {
            free( *pppsz_longname );
            *pppsz_longname = NULL;
        }
        module_list_free (list);
        return NULL;
    }

    for (size_t i = 0, j = 0; list[i]; i++)
    {
        module_t *p_module = list[i];
        const char *psz_module_capability = p_module->psz_capability;

        if( psz_module_capability && !strcmp( psz_module_capability, psz_capability ) )
        {
            int k = -1; /* hack to handle submodules properly */
            if( p_module->b_submodule )
            {
                while( p_module->pp_shortcuts[++k] != NULL );
                k--;
            }
            psz_ret[j] = strdup( k>=0?p_module->pp_shortcuts[k]
                                     :p_module->psz_object_name );
            if( pppsz_longname )
                (*pppsz_longname)[j] = strdup( module_get_name( p_module, true ) );
            j++;
        }
    }
    psz_ret[count] = NULL;

    module_list_free (list);

    return psz_ret;
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
         || item->b_unsaveable /* non-modifiable option */
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

 /*****************************************************************************
 * copy_next_paths_token: from a PATH_SEP_CHAR (a ':' or a ';') separated paths
 * return first path.
 *****************************************************************************/
static char * copy_next_paths_token( char * paths, char ** remaining_paths )
{
    char * path;
    int i, done;
    bool escaped = false;

    assert( paths );

    /* Alloc a buffer to store the path */
    path = malloc( strlen( paths ) + 1 );
    if( !path ) return NULL;

    /* Look for PATH_SEP_CHAR (a ':' or a ';') */
    for( i = 0, done = 0 ; paths[i]; i++ )
    {
        /* Take care of \\ and \: or \; escapement */
        if( escaped )
        {
            escaped = false;
            path[done++] = paths[i];
        }
#ifdef WIN32
        else if( paths[i] == '/' )
            escaped = true;
#else
        else if( paths[i] == '\\' )
            escaped = true;
#endif
        else if( paths[i] == PATH_SEP_CHAR )
            break;
        else
            path[done++] = paths[i];
    }
    path[done++] = 0;

    /* Return the remaining paths */
    if( remaining_paths ) {
        *remaining_paths = paths[i] ? &paths[i]+1 : NULL;
    }

    return path;
}

char *psz_vlcpath = NULL;

/*****************************************************************************
 * AllocateAllPlugins: load all plugin modules we can find.
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins( vlc_object_t *p_this, module_bank_t *p_bank )
{
    const char *vlcpath = psz_vlcpath;
    int count,i;
    char * path;
    vlc_array_t *arraypaths = vlc_array_new();

    /* Contruct the special search path for system that have a relocatable
     * executable. Set it to <vlc path>/modules and <vlc path>/plugins. */

    if( vlcpath && asprintf( &path, "%s" DIR_SEP "modules", vlcpath ) != -1 )
        vlc_array_append( arraypaths, path );
    if( vlcpath && asprintf( &path, "%s" DIR_SEP "plugins", vlcpath ) != -1 )
        vlc_array_append( arraypaths, path );
#ifndef WIN32
    vlc_array_append( arraypaths, strdup( PLUGIN_PATH ) );
#endif

    /* If the user provided a plugin path, we add it to the list */
    char *userpaths = config_GetPsz( p_this, "plugin-path" );
    char *paths_iter;

    for( paths_iter = userpaths; paths_iter; )
    {
        path = copy_next_paths_token( paths_iter, &paths_iter );
        if( path )
            vlc_array_append( arraypaths, path );
    }

    count = vlc_array_count( arraypaths );
    for( i = 0 ; i < count ; i++ )
    {
        path = vlc_array_item_at_index( arraypaths, i );
        if( !path )
            continue;

        msg_Dbg( p_this, "recursively browsing `%s'", path );

        /* Don't go deeper than 5 subdirectories */
        AllocatePluginDir( p_this, p_bank, path, 5 );

        free( path );
    }

    vlc_array_destroy( arraypaths );
    free( userpaths );
}

/*****************************************************************************
 * AllocatePluginDir: recursively parse a directory to look for plugins
 *****************************************************************************/
static void AllocatePluginDir( vlc_object_t *p_this, module_bank_t *p_bank,
                               const char *psz_dir, unsigned i_maxdepth )
{
/* FIXME: Needs to be ported to wide char on ALL Windows builds */
#ifdef WIN32
# undef opendir
# undef closedir
# undef readdir
#endif
#if defined( UNDER_CE ) || defined( _MSC_VER )
#ifdef UNDER_CE
    wchar_t psz_wpath[MAX_PATH + 256];
    wchar_t psz_wdir[MAX_PATH];
#endif
    char psz_path[MAX_PATH + 256];
    WIN32_FIND_DATA finddata;
    HANDLE handle;
    int rc;
#else
    int    i_dirlen;
    DIR *  dir;
    struct dirent * file;
#endif
    char * psz_file;

    if( i_maxdepth == 0 )
        return;

#if defined( UNDER_CE ) || defined( _MSC_VER )
#ifdef UNDER_CE
    MultiByteToWideChar( CP_ACP, 0, psz_dir, -1, psz_wdir, MAX_PATH );

    rc = GetFileAttributes( psz_wdir );
    if( rc<0 || !(rc&FILE_ATTRIBUTE_DIRECTORY) ) return; /* Not a directory */

    /* Parse all files in the directory */
    swprintf( psz_wpath, L"%ls\\*", psz_wdir );
#else
    rc = GetFileAttributes( psz_dir );
    if( rc<0 || !(rc&FILE_ATTRIBUTE_DIRECTORY) ) return; /* Not a directory */
#endif

    /* Parse all files in the directory */
    sprintf( psz_path, "%s\\*", psz_dir );

#ifdef UNDER_CE
    handle = FindFirstFile( psz_wpath, &finddata );
#else
    handle = FindFirstFile( psz_path, &finddata );
#endif
    if( handle == INVALID_HANDLE_VALUE )
    {
        /* Empty directory */
        return;
    }

    /* Parse the directory and try to load all files it contains. */
    do
    {
#ifdef UNDER_CE
        unsigned int i_len = wcslen( finddata.cFileName );
        swprintf( psz_wpath, L"%ls\\%ls", psz_wdir, finddata.cFileName );
        sprintf( psz_path, "%s\\%ls", psz_dir, finddata.cFileName );
#else
        unsigned int i_len = strlen( finddata.cFileName );
        sprintf( psz_path, "%s\\%s", psz_dir, finddata.cFileName );
#endif

        /* Skip ".", ".." */
        if( !*finddata.cFileName || !strcmp( finddata.cFileName, "." )
         || !strcmp( finddata.cFileName, ".." ) )
        {
            if( !FindNextFile( handle, &finddata ) ) break;
            continue;
        }

#ifdef UNDER_CE
        if( GetFileAttributes( psz_wpath ) & FILE_ATTRIBUTE_DIRECTORY )
#else
        if( GetFileAttributes( psz_path ) & FILE_ATTRIBUTE_DIRECTORY )
#endif
        {
            AllocatePluginDir( p_this, p_bank, psz_path, i_maxdepth - 1 );
        }
        else if( i_len > strlen( LIBEXT )
                  /* We only load files ending with LIBEXT */
                  && !strncasecmp( psz_path + strlen( psz_path)
                                   - strlen( LIBEXT ),
                                   LIBEXT, strlen( LIBEXT ) ) )
        {
            WIN32_FILE_ATTRIBUTE_DATA attrbuf;
            int64_t i_time = 0, i_size = 0;

#ifdef UNDER_CE
            if( GetFileAttributesEx( psz_wpath, GetFileExInfoStandard,
                                     &attrbuf ) )
#else
            if( GetFileAttributesEx( psz_path, GetFileExInfoStandard,
                                     &attrbuf ) )
#endif
            {
                i_time = attrbuf.ftLastWriteTime.dwHighDateTime;
                i_time <<= 32;
                i_time |= attrbuf.ftLastWriteTime.dwLowDateTime;
                i_size = attrbuf.nFileSizeHigh;
                i_size <<= 32;
                i_size |= attrbuf.nFileSizeLow;
            }
            psz_file = psz_path;

            AllocatePluginFile( p_this, p_bank, psz_file, i_time, i_size );
        }
    }
    while( !p_this->p_libvlc->b_die && FindNextFile( handle, &finddata ) );

    /* Close the directory */
    FindClose( handle );

#else
    dir = opendir( psz_dir );
    if( !dir )
    {
        return;
    }

    i_dirlen = strlen( psz_dir );

    /* Parse the directory and try to load all files it contains. */
    while( !p_this->p_libvlc->b_die && ( file = readdir( dir ) ) )
    {
        struct stat statbuf;
        unsigned int i_len;
        int i_stat;

        /* Skip ".", ".." */
        if( !*file->d_name || !strcmp( file->d_name, "." )
         || !strcmp( file->d_name, ".." ) )
        {
            continue;
        }

        i_len = strlen( file->d_name );
        psz_file = malloc( i_dirlen + 1 + i_len + 1 );
        sprintf( psz_file, "%s"DIR_SEP"%s", psz_dir, file->d_name );

        i_stat = stat( psz_file, &statbuf );
        if( !i_stat && statbuf.st_mode & S_IFDIR )
        {
            AllocatePluginDir( p_this, p_bank, psz_file, i_maxdepth - 1 );
        }
        else if( i_len > strlen( LIBEXT )
                  /* We only load files ending with LIBEXT */
                  && !strncasecmp( file->d_name + i_len - strlen( LIBEXT ),
                                   LIBEXT, strlen( LIBEXT ) ) )
        {
            int64_t i_time = 0, i_size = 0;

            if( !i_stat )
            {
                i_time = statbuf.st_mtime;
                i_size = statbuf.st_size;
            }

            AllocatePluginFile( p_this, p_bank, psz_file, i_time, i_size );
        }

        free( psz_file );
    }

    /* Close the directory */
    closedir( dir );

#endif
}

/*****************************************************************************
 * AllocatePluginFile: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_need
 * and module_unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocatePluginFile( vlc_object_t * p_this, module_bank_t *p_bank,
                               const char *psz_file,
                               int64_t i_file_time, int64_t i_file_size )
{
    module_t * p_module = NULL;
    module_cache_t *p_cache_entry = NULL;

    /*
     * Check our plugins cache first then load plugin if needed
     */
    p_cache_entry = CacheFind( p_bank, psz_file, i_file_time, i_file_size );
    if( !p_cache_entry )
    {
        p_module = AllocatePlugin( p_this, psz_file );
    }
    else
    {
        /* If junk dll, don't try to load it */
        if( p_cache_entry->b_junk )
        {
            p_module = NULL;
        }
        else
        {
            module_config_t *p_item = NULL, *p_end = NULL;

            p_module = p_cache_entry->p_module;
            p_module->b_loaded = false;

            /* For now we force loading if the module's config contains
             * callbacks or actions.
             * Could be optimized by adding an API call.*/
            for( p_item = p_module->p_config, p_end = p_item + p_module->confsize;
                 p_item < p_end; p_item++ )
            {
                if( p_item->pf_callback || p_item->i_action )
                {
                    p_module = AllocatePlugin( p_this, psz_file );
                    break;
                }
            }
            if( p_module == p_cache_entry->p_module )
                p_cache_entry->b_used = true;
        }
    }

    if( p_module )
    {
        /* Everything worked fine !
         * The module is ready to be added to the list. */
        p_module->b_builtin = false;

        /* msg_Dbg( p_this, "plugin \"%s\", %s",
                    p_module->psz_object_name, p_module->psz_longname ); */
        p_module->next = p_bank->head;
        p_bank->head = p_module;

        if( !p_module_bank->b_cache )
            return 0;

        /* Add entry to cache */
        p_bank->pp_cache =
            realloc( p_bank->pp_cache, (p_bank->i_cache + 1) * sizeof(void *) );
        p_bank->pp_cache[p_bank->i_cache] = malloc( sizeof(module_cache_t) );
        if( !p_bank->pp_cache[p_bank->i_cache] )
            return -1;
        p_bank->pp_cache[p_bank->i_cache]->psz_file = strdup( psz_file );
        p_bank->pp_cache[p_bank->i_cache]->i_time = i_file_time;
        p_bank->pp_cache[p_bank->i_cache]->i_size = i_file_size;
        p_bank->pp_cache[p_bank->i_cache]->b_junk = p_module ? 0 : 1;
        p_bank->pp_cache[p_bank->i_cache]->b_used = true;
        p_bank->pp_cache[p_bank->i_cache]->p_module = p_module;
        p_bank->i_cache++;
    }

    return p_module ? 0 : -1;
}

/*****************************************************************************
 * AllocatePlugin: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_need
 * and module_unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static module_t * AllocatePlugin( vlc_object_t * p_this, const char *psz_file )
{
    module_t * p_module = NULL;
    module_handle_t handle;

    if( module_Load( p_this, psz_file, &handle ) )
        return NULL;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */
    p_module = vlc_module_create( p_this );
    if( p_module == NULL )
    {
        module_Unload( handle );
        return NULL;
    }

    p_module->psz_filename = strdup( psz_file );
    p_module->handle = handle;
    p_module->b_loaded = true;

    /* Initialize the module: fill p_module, default config */
    if( module_Call( p_this, p_module ) != 0 )
    {
        /* We couldn't call module_init() */
        free( p_module->psz_filename );
        module_release( p_module );
        module_Unload( handle );
        return NULL;
    }

    DupModule( p_module );

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->b_builtin = false;

    return p_module;
}

/*****************************************************************************
 * DupModule: make a plugin module standalone.
 *****************************************************************************
 * This function duplicates all strings in the module, so that the dynamic
 * object can be unloaded. It acts recursively on submodules.
 *****************************************************************************/
static void DupModule( module_t *p_module )
{
    char **pp_shortcut;

    for( pp_shortcut = p_module->pp_shortcuts ; *pp_shortcut ; pp_shortcut++ )
    {
        *pp_shortcut = strdup( *pp_shortcut );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->psz_capability = strdup( p_module->psz_capability );
    p_module->psz_shortname = p_module->psz_shortname ?
                                 strdup( p_module->psz_shortname ) : NULL;
    p_module->psz_longname = strdup( p_module->psz_longname );
    p_module->psz_help = p_module->psz_help ? strdup( p_module->psz_help )
                                            : NULL;

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
    char **pp_shortcut;

    for (module_t *subm = p_module->submodule; subm; subm = subm->next)
        UndupModule (subm);

    for( pp_shortcut = p_module->pp_shortcuts ; *pp_shortcut ; pp_shortcut++ )
    {
        free( *pp_shortcut );
    }

    free( p_module->psz_capability );
    FREENULL( p_module->psz_shortname );
    free( p_module->psz_longname );
    FREENULL( p_module->psz_help );
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
    p_module = vlc_module_create( p_this );
    if( p_module == NULL )
        return -1;

    /* Initialize the module : fill p_module->psz_object_name, etc. */
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
    p_module->next = p_module_bank->head;
    p_module_bank->head = p_module;
    /* UNLOCK */

    /* msg_Dbg( p_this, "builtin \"%s\", %s",
                p_module->psz_object_name, p_module->psz_longname ); */

    return 0;
}

/*****************************************************************************
 * DeleteModule: delete a module and its structure.
 *****************************************************************************
 * This function can only be called if the module isn't being used.
 *****************************************************************************/
static void DeleteModule( module_bank_t *p_bank, module_t * p_module )
{
    assert( p_module );

    /* Unlist the module (if it is in the list) */
    module_t **pp_self = &p_bank->head;
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
