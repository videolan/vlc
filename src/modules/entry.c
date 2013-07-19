/*****************************************************************************
 * entry.c : Callbacks for module entry point
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * Copyright © 2007-2008 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_memory.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>

#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

static char *strdup_null (const char *str)
{
    return (str != NULL) ? strdup (str) : NULL;
}

module_t *vlc_module_create (module_t *parent)
{
    module_t *module = malloc (sizeof (*module));
    if (module == NULL)
        return NULL;

    /* TODO: replace module/submodules with plugin/modules */
    if (parent == NULL)
    {
        module->next = NULL;
        module->parent = NULL;
    }
    else
    {
        module->next = parent->submodule;
        parent->submodule = module;
        parent->submodule_count++;
        module->parent = parent;
    }

    module->submodule = NULL;
    module->submodule_count = 0;

    module->psz_shortname = NULL;
    module->psz_longname = NULL;
    module->psz_help = NULL;
    module->pp_shortcuts = NULL;
    module->i_shortcuts = 0;
    module->psz_capability = NULL;
    module->i_score = (parent != NULL) ? parent->i_score : 1;
    module->b_loaded = false;
    module->b_unloadable = parent == NULL;
    module->pf_activate = NULL;
    module->pf_deactivate = NULL;
    module->p_config = NULL;
    module->confsize = 0;
    module->i_config_items = 0;
    module->i_bool_items = 0;
    /*module->handle = garbage */
    module->psz_filename = NULL;
    module->domain = NULL;
    return module;
}

/**
 * Destroys a plug-in.
 * @warning If the plug-in is loaded in memory, the handle will be leaked.
 */
void vlc_module_destroy (module_t *module)
{
    assert (!module->b_loaded || !module->b_unloadable);

    for (module_t *m = module->submodule, *next; m != NULL; m = next)
    {
        next = m->next;
        vlc_module_destroy (m);
    }

    config_Free (module->p_config, module->confsize);

    free (module->domain);
    free (module->psz_filename);
    for (unsigned i = 0; i < module->i_shortcuts; i++)
        free (module->pp_shortcuts[i]);
    free (module->pp_shortcuts);
    free (module->psz_capability);
    free (module->psz_help);
    free (module->psz_longname);
    free (module->psz_shortname);
    free (module);
}

static module_config_t *vlc_config_create (module_t *module, int type)
{
    unsigned confsize = module->confsize;
    module_config_t *tab = module->p_config;

    if ((confsize & 0xf) == 0)
    {
        tab = realloc_or_free (tab, (confsize + 17) * sizeof (*tab));
        if (tab == NULL)
            return NULL;

        module->p_config = tab;
    }

    memset (tab + confsize, 0, sizeof (tab[confsize]));
    if (IsConfigIntegerType (type))
    {
        tab[confsize].max.i = INT_MAX;
        tab[confsize].min.i = INT_MIN;
    }
    tab[confsize].i_type = type;

    if (CONFIG_ITEM(type))
    {
        module->i_config_items++;
        if (type == CONFIG_ITEM_BOOL)
            module->i_bool_items++;
    }

    module->confsize++;
    return tab + confsize;
}


/**
 * Callback for the plugin descriptor functions.
 */
static int vlc_plugin_setter (void *plugin, void *tgt, int propid, ...)
{
    module_t **pprimary = plugin;
    module_t *module = tgt;
    module_config_t *item = tgt;
    va_list ap;
    int ret = 0;

    va_start (ap, propid);
    switch (propid)
    {
        case VLC_MODULE_CREATE:
        {
            module = *pprimary;
            module_t *submodule = vlc_module_create (module);
            if (unlikely(submodule == NULL))
            {
                ret = -1;
                break;
            }

            *(va_arg (ap, module_t **)) = submodule;
            if (*pprimary == NULL)
            {
                *pprimary = submodule;
                break;
            }
            /* Inheritance. Ugly!! */
            submodule->pp_shortcuts = xmalloc (sizeof ( *submodule->pp_shortcuts ));
            submodule->pp_shortcuts[0] = strdup_null (module->pp_shortcuts[0]);
            submodule->i_shortcuts = 1; /* object name */

            submodule->psz_shortname = strdup_null (module->psz_shortname);
            submodule->psz_longname = strdup_null (module->psz_longname);
            submodule->psz_capability = strdup_null (module->psz_capability);
            break;
        }

        case VLC_CONFIG_CREATE:
        {
            int type = va_arg (ap, int);
            module_config_t **pp = va_arg (ap, module_config_t **);

            item = vlc_config_create (*pprimary, type);
            if (unlikely(item == NULL))
            {
                ret = -1;
                break;
            }
            *pp = item;
            break;
        }

        case VLC_MODULE_SHORTCUT:
        {
            unsigned i_shortcuts = va_arg (ap, unsigned);
            unsigned index = module->i_shortcuts;
            /* The cache loader accept only a small number of shortcuts */
            assert(i_shortcuts + index <= MODULE_SHORTCUT_MAX);

            const char *const *tab = va_arg (ap, const char *const *);
            char **pp = realloc (module->pp_shortcuts,
                                 sizeof (pp[0]) * (index + i_shortcuts));
            if (unlikely(pp == NULL))
            {
                ret = -1;
                break;
            }
            module->pp_shortcuts = pp;
            module->i_shortcuts = index + i_shortcuts;
            pp += index;
            for (unsigned i = 0; i < i_shortcuts; i++)
                pp[i] = strdup (tab[i]);
            break;
        }

        case VLC_MODULE_CAPABILITY:
            free (module->psz_capability);
            module->psz_capability = strdup (va_arg (ap, char *));
            break;

        case VLC_MODULE_SCORE:
            module->i_score = va_arg (ap, int);
            break;

        case VLC_MODULE_CB_OPEN:
            module->pf_activate = va_arg (ap, void *);
            break;

        case VLC_MODULE_CB_CLOSE:
            module->pf_deactivate = va_arg (ap, void *);
            break;

        case VLC_MODULE_NO_UNLOAD:
            assert (module->parent == NULL);
            module->b_unloadable = false;
            break;

        case VLC_MODULE_NAME:
        {
            const char *value = va_arg (ap, const char *);

            assert (module->i_shortcuts == 0);
            module->pp_shortcuts = malloc( sizeof( *module->pp_shortcuts ) );
            module->pp_shortcuts[0] = strdup (value);
            module->i_shortcuts = 1;

            assert (module->psz_longname == NULL);
            module->psz_longname = strdup (value);
            break;
        }

        case VLC_MODULE_SHORTNAME:
            assert (module->psz_shortname == NULL || module->parent != NULL);
            free (module->psz_shortname);
            module->psz_shortname = strdup (va_arg (ap, char *));
            break;

        case VLC_MODULE_DESCRIPTION:
            // TODO: do not set this in VLC_MODULE_NAME
            free (module->psz_longname);
            module->psz_longname = strdup (va_arg (ap, char *));
            break;

        case VLC_MODULE_HELP:
            assert (module->parent == NULL);
            assert (module->psz_help == NULL);
            module->psz_help = strdup (va_arg (ap, char *));
            break;

        case VLC_MODULE_TEXTDOMAIN:
            assert (module->parent == NULL);
            assert (module->domain == NULL);
            module->domain = strdup (va_arg (ap, char *));
            break;

        case VLC_CONFIG_NAME:
        {
            const char *name = va_arg (ap, const char *);

            assert (name != NULL);
            item->psz_name = strdup (name);
            break;
        }

        case VLC_CONFIG_VALUE:
        {
            if (IsConfigIntegerType (item->i_type)
             || !CONFIG_ITEM(item->i_type))
            {
                item->orig.i =
                item->value.i = va_arg (ap, int64_t);
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->orig.f =
                item->value.f = va_arg (ap, double);
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *value = va_arg (ap, const char *);
                item->value.psz = value ? strdup (value) : NULL;
                item->orig.psz = value ? strdup (value) : NULL;
            }
            break;
        }

        case VLC_CONFIG_RANGE:
        {
            if (IsConfigFloatType (item->i_type))
            {
                item->min.f = va_arg (ap, double);
                item->max.f = va_arg (ap, double);
            }
            else
            {
                item->min.i = va_arg (ap, int64_t);
                item->max.i = va_arg (ap, int64_t);
            }
            break;
        }

        case VLC_CONFIG_ADVANCED:
            item->b_advanced = true;
            break;

        case VLC_CONFIG_VOLATILE:
            item->b_unsaveable = true;
            break;

        case VLC_CONFIG_PRIVATE:
            item->b_internal = true;
            break;

        case VLC_CONFIG_REMOVED:
            item->b_removed = true;
            break;

        case VLC_CONFIG_CAPABILITY:
        {
            const char *cap = va_arg (ap, const char *);
            item->psz_type = cap ? strdup (cap) : NULL;
            break;
        }

        case VLC_CONFIG_SHORTCUT:
            item->i_short = va_arg (ap, int);
            break;

        case VLC_CONFIG_SAFE:
            item->b_safe = true;
            break;

        case VLC_CONFIG_DESC:
        {
            const char *text = va_arg (ap, const char *);
            const char *longtext = va_arg (ap, const char *);

            item->psz_text = text ? strdup (text) : NULL;
            item->psz_longtext = longtext ? strdup (longtext) : NULL;
            break;
        }

        case VLC_CONFIG_LIST:
        {
            size_t len = va_arg (ap, size_t);

            assert (item->list_count == 0); /* cannot replace choices */
            assert (item->list.psz_cb == NULL);
            if (len == 0)
                break; /* nothing to do */
            /* Copy values */
            if (IsConfigIntegerType (item->i_type))
            {
                const int *src = va_arg (ap, const int *);
                int *dst = xmalloc (sizeof (int) * len);

                memcpy (dst, src, sizeof (int) * len);
                item->list.i = dst;
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *const *src = va_arg (ap, const char *const *);
                char **dst = xmalloc (sizeof (char *) * len);

                for (size_t i = 0; i < len; i++)
                     dst[i] = src[i] ? strdup (src[i]) : NULL;
                item->list.psz = dst;
            }
            else
                break;

            /* Copy textual descriptions */
            const char *const *text = va_arg (ap, const char *const *);
            char **dtext = xmalloc (sizeof (char *) * (len + 1));
            for (size_t i = 0; i < len; i++)
                dtext[i] = text[i] ? strdup (text[i]) : NULL;
            item->list_text = dtext;
            item->list_count = len;
            break;
        }

        case VLC_CONFIG_LIST_CB:
            if (IsConfigIntegerType (item->i_type))
               item->list.i_cb = va_arg (ap, vlc_integer_list_cb);
            else
            if (IsConfigStringType (item->i_type))
               item->list.psz_cb = va_arg (ap, vlc_string_list_cb);
            else
                break;
            break;

        default:
            fprintf (stderr, "LibVLC: unknown module property %d\n", propid);
            fprintf (stderr, "LibVLC: too old to use this module?\n");
            ret = -1;
            break;
    }

    va_end (ap);
    return ret;
}

/**
 * Runs a plug-in descriptor. This loads the plug-in meta-data in memory.
 */
module_t *vlc_plugin_describe (vlc_plugin_cb entry)
{
    module_t *module = NULL;

    if (entry (vlc_plugin_setter, &module) != 0)
    {
        if (module != NULL) /* partially initialized plug-in... */
            vlc_module_destroy (module);
        module = NULL;
    }
    return module;
}
