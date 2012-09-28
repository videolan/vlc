/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001-2011 VLC authors and VideoLAN
 * $Id$
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
#include <string.h>
#ifdef ENABLE_NLS
# include <libintl.h>
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include "libvlc.h"
#include "config/configuration.h"
#include "vlc_arrays.h"
#include "modules/modules.h"

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
    p_list = xmalloc( count * sizeof( module_list_t ) );

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

        if (module_Map (p_this, p_cand))
            continue;
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
