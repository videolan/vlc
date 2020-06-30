/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
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

bool module_provides (const module_t *m, const char *cap)
{
    return !strcmp (module_get_capability (m), cap);
}

const char *module_get_object( const module_t *m )
{
    if (unlikely(m->i_shortcuts == 0))
        return "unnamed";
    return m->pp_shortcuts[0];
}

const char *module_get_name( const module_t *m, bool long_name )
{
    if( long_name && ( m->psz_longname != NULL) )
        return m->psz_longname;

    if (m->psz_shortname != NULL)
        return m->psz_shortname;
    return module_get_object (m);
}

const char *module_get_help( const module_t *m )
{
    return m->psz_help;
}

const char *module_get_capability (const module_t *m)
{
    return (m->psz_capability != NULL) ? m->psz_capability : "none";
}

int module_get_score( const module_t *m )
{
    return m->i_score;
}

const char *module_gettext (const module_t *m, const char *str)
{
    if (unlikely(str == NULL || *str == '\0'))
        return "";
#ifdef ENABLE_NLS
    const char *domain = m->plugin->textdomain;
    return dgettext ((domain != NULL) ? domain : PACKAGE_NAME, str);
#else
    (void)m;
    return str;
#endif
}

static bool module_match_name(const module_t *m, const char *name, size_t len)
{
     for (size_t i = 0; i < m->i_shortcuts; i++)
          if (strncasecmp(m->pp_shortcuts[i], name, len) == 0
           && m->pp_shortcuts[i][len] == '\0')
              return true;

     return false;
}

/**
 * Finds the candidate modules for given criteria.
 *
 * All candidates modules having the specified capability and name will be
 * sorted in decreasing order of priority and returned in a heap-allocated
 * table.
 *
 * \param capability capability, i.e. class of module
 * \param names string of comma-separated requested module shortcut names
 * \param strict whether to exclude modules with no unmatching shortcut names
 * \param modules storage location for the base address of a sorted table
 *                of candidate modules [OUT]
 * \param strict_matches storage location for the count of strictly matched
 *                       modules (optional) [OUT]
 * \return number of modules found or a strictly negative value on error
 */
static
ssize_t vlc_module_match(const char *capability, const char *names,
                         bool strict, module_t ***restrict modules,
                         size_t *restrict strict_matches)
{
    module_t *const *tab;
    size_t total = module_list_cap(&tab, capability);
    module_t **unsorted = malloc(total * sizeof (*unsorted));
    module_t **sorted = malloc(total * sizeof (*sorted));
    size_t matches = 0;

    *modules = sorted;

    if (total > 0) {
        if (unlikely(unsorted == NULL || sorted == NULL)) {
            free(unsorted);
            free(sorted);
            return -1;
        }
        memcpy(unsorted, tab, total * sizeof (*unsorted));
    }

    /* Go through the list of module shortcut names. */
    while (names[0] != '\0') {
        const char *shortcut = names;
        size_t slen = strcspn(names, ",");

        names += slen;
        names += strspn(names, ",");

        /* "none" matches nothing and ends the search */
        if (slen == 4 && strncasecmp("none", shortcut, 4) == 0) {
            total = 0;
            break;
        }

        /* "any" matches everything with strictly positive score */
        if (slen == 3 && strncasecmp("any", shortcut, 3) == 0) {
            strict = false;
            break;
        }

        for (size_t i = 0; i < total; i++) {
            module_t *cand = unsorted[i];

            if (cand != NULL && module_match_name(cand, shortcut, slen)) {
                assert(matches < total);
                sorted[matches++] = cand;
                unsorted[i] = NULL;
            }
        }
    }

    if (strict_matches != NULL)
        *strict_matches = matches;

    if (!strict) {
        /* List remaining modules with strictly positive score. */
        for (size_t i = 0; i < total; i++) {
            module_t *cand = unsorted[i];

            if (cand != NULL) {
                /* Modules are sorted by decreasing score, so there is no point
                 * carrying on after the first zero score is found.
                 */
                if (module_get_score(cand) <= 0)
                    break;

                assert(matches < total);
                sorted[matches++] = cand;
            }
        }
    }

    free(unsorted);
    return matches;
}

static int module_load(vlc_logger_t *log, module_t *m,
                       vlc_activate_t init, bool forced, va_list args)
{
    int ret = VLC_SUCCESS;

    if (module_Map(log, m->plugin))
        return VLC_EGENERIC;

    if (m->pf_activate != NULL)
    {
        va_list ap;

        va_copy (ap, args);
        ret = init(m->pf_activate, forced, ap);
        va_end (ap);
    }

    return ret;
}

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
 * \param log logger (or NULL to ignore)
 * \param capability capability, i.e. class of module
 * \param name name of the module asked, if any
 * \param strict if true, do not fallback to plugin with a different name
 *                 but the same capability
 * \param probe module probe callback
 * \return the module or NULL in case of a failure
 */
module_t *(vlc_module_load)(struct vlc_logger *log, const char *capability,
                            const char *name, bool strict,
                            vlc_activate_t probe, ...)
{
    if (name == NULL || name[0] == '\0')
        name = "any";

    /* Find matching modules */
    module_t **mods;
    size_t strict_total;
    ssize_t total = vlc_module_match(capability, name, strict,
                                     &mods, &strict_total);

    if (unlikely(total < 0))
        return NULL;

    vlc_debug(log, "looking for %s module matching \"%s\": %zd candidates",
              capability, name, total);

    module_t *module = NULL;
    va_list args;

    va_start(args, probe);

    for (size_t i = 0; i < (size_t)total; i++) {
        module_t *cand = mods[i];
        int ret = module_load(log, cand, probe, i < strict_total, args);

        switch (ret) {
            case VLC_SUCCESS:
                vlc_debug(log, "using %s module \"%s\"", capability,
                          module_get_object(cand));
                module = cand;
                /* fall through */
            case VLC_ETIMEOUT:
                goto done;
        }
    }

    va_end (args);

    if (module == NULL)
done:   vlc_debug(log, "no %s modules matched", capability);

    free(mods);
    return module;
}

static int generic_start(void *func, bool forced, va_list ap)
{
    vlc_object_t *obj = va_arg(ap, vlc_object_t *);
    int (*activate)(vlc_object_t *) = func;
    int ret;

    obj->force = forced;
    ret = activate(obj);
    if (ret != VLC_SUCCESS)
        vlc_objres_clear(obj);
    return ret;
}

#undef module_need
module_t *module_need(vlc_object_t *obj, const char *cap, const char *name,
                      bool strict)
{
    const bool b_force_backup = obj->force; /* FIXME: remove this */
    module_t *module = vlc_module_load(obj->logger, cap, name, strict,
                                       generic_start, obj);
    if (module != NULL) {
        var_Create(obj, "module-name", VLC_VAR_STRING);
        var_SetString(obj, "module-name", module_get_object(module));
    }

    obj->force = b_force_backup;
    return module;
}

#undef module_unneed
void module_unneed(vlc_object_t *obj, module_t *module)
{
    msg_Dbg(obj, "removing module \"%s\"", module_get_object(module));
    var_Destroy(obj, "module-name");

    if (module->deactivate != NULL)
        module->deactivate(obj);

    vlc_objres_clear(obj);
}

module_t *module_find (const char *name)
{
    size_t count;
    module_t **list = module_list_get (&count);

    assert (name != NULL);

    for (size_t i = 0; i < count; i++)
    {
        module_t *module = list[i];

        if (unlikely(module->i_shortcuts == 0))
            continue;
        if (!strcmp (module->pp_shortcuts[0], name))
        {
            module_list_free (list);
            return module;
        }
    }
    module_list_free (list);
    return NULL;
}

bool module_exists (const char * psz_name)
{
    return module_find (psz_name) != NULL;
}

module_config_t *module_config_get( const module_t *module, unsigned *restrict psize )
{
    const vlc_plugin_t *plugin = module->plugin;

    if (plugin->module != module)
    {   /* For backward compatibility, pretend non-first modules have no
         * configuration items. */
        *psize = 0;
        return NULL;
    }

    unsigned i,j;
    size_t size = plugin->conf.size;
    module_config_t *config = vlc_alloc( size, sizeof( *config ) );

    assert( psize != NULL );
    *psize = 0;

    if( !config )
        return NULL;

    for( i = 0, j = 0; i < size; i++ )
    {
        const module_config_t *item = plugin->conf.items + i;
        if( item->b_internal /* internal option */
         || item->b_removed /* removed option */ )
            continue;

        memcpy( config + j, item, sizeof( *config ) );
        j++;
    }
    *psize = j;

    return config;
}

void module_config_free( module_config_t *config )
{
    free( config );
}
