/*****************************************************************************
 * entry.c : Callbacks for module entry point
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
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
#include <assert.h>
#include <stdarg.h>

#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

static void vlc_module_destruct (gc_object_t *obj)
{
    module_t *module = vlc_priv (obj, module_t);

    free (module->psz_object_name);
    free (module);
}

static const char default_name[] = "unnamed";

module_t *vlc_module_create (vlc_object_t *obj)
{
    module_t *module = malloc (sizeof (*module));
    if (module == NULL)
        return NULL;

    module->psz_object_name = strdup( default_name );
    module->next = NULL;
    module->submodule = NULL;
    module->parent = NULL;
    module->submodule_count = 0;
    vlc_gc_init (module, vlc_module_destruct);

    module->psz_shortname = NULL;
    module->psz_longname = (char*)default_name;
    module->psz_help = NULL;
    for (unsigned i = 0; i < MODULE_SHORTCUT_MAX; i++)
        module->pp_shortcuts[i] = NULL;
    module->psz_capability = (char*)"";
    module->i_score = 1;
    module->b_unloadable = true;
    module->b_submodule = false;
    module->pf_activate = NULL;
    module->pf_deactivate = NULL;
    module->p_config = NULL;
    module->confsize = 0;
    module->i_config_items = 0;
    module->i_bool_items = 0;
    /*module->handle = garbage */
    module->psz_filename = NULL;
    module->domain = NULL;
    module->b_builtin = false;
    module->b_loaded = false;

    (void)obj;
    return module;
}


static void vlc_submodule_destruct (gc_object_t *obj)
{
    module_t *module = vlc_priv (obj, module_t);
    free (module->psz_object_name);
    free (module);
}

module_t *vlc_submodule_create (module_t *module)
{
    assert (module != NULL);

    module_t *submodule = calloc( 1, sizeof(*submodule) );
    if( !submodule )
        return NULL;

    vlc_gc_init (submodule, vlc_submodule_destruct);

    submodule->next = module->submodule;
    submodule->parent = module;
    module->submodule = submodule;
    module->submodule_count++;

    /* Muahahaha! Heritage! Polymorphism! Ugliness!! */
    submodule->pp_shortcuts[0] = module->pp_shortcuts[0]; /* object name */
    for (unsigned i = 1; i < MODULE_SHORTCUT_MAX; i++)
        submodule->pp_shortcuts[i] = NULL;

    submodule->psz_object_name = strdup( module->psz_object_name );
    submodule->psz_shortname = module->psz_shortname;
    submodule->psz_longname = module->psz_longname;
    submodule->psz_capability = module->psz_capability;
    submodule->i_score = module->i_score;
    submodule->b_submodule = true;
    submodule->domain = module->domain;
    return submodule;
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
    tab[confsize].i_type = type;

    if (type & CONFIG_ITEM)
    {
        module->i_config_items++;
        if (type == CONFIG_ITEM_BOOL)
            module->i_bool_items++;
    }

    module->confsize++;
    return tab + confsize;
}


int vlc_plugin_set (module_t *module, module_config_t *item, int propid, ...)
{
    va_list ap;
    int ret = 0;
    typedef int(*int_fp_vlcobjectt)(vlc_object_t *) ;
    typedef void(*void_fp_vlcobjectt)(vlc_object_t *);

    va_start (ap, propid);
    switch (propid)
    {
        case VLC_SUBMODULE_CREATE:
        {
            module_t **pp = va_arg (ap, module_t **);
            *pp = vlc_submodule_create (module);
            if (*pp == NULL)
                ret = -1;
            break;
        }

        case VLC_CONFIG_CREATE:
        {
            int type = va_arg (ap, int);
            module_config_t **pp = va_arg (ap, module_config_t **);
            *pp = vlc_config_create (module, type);
            if (*pp == NULL)
                ret = -1;
            break;
        }

        case VLC_MODULE_SHORTCUT:
        {
            unsigned i;
            for (i = 0; module->pp_shortcuts[i] != NULL; i++);
                if (i >= (MODULE_SHORTCUT_MAX - 1))
                    break;

            module->pp_shortcuts[i] = va_arg (ap, char *);
            break;
        }

        case VLC_MODULE_CAPABILITY:
            module->psz_capability = va_arg (ap, char *);
            break;

        case VLC_MODULE_SCORE:
            module->i_score = va_arg (ap, int);
            break;

        case VLC_MODULE_CB_OPEN:
            module->pf_activate = va_arg (ap, int_fp_vlcobjectt);
            break;

        case VLC_MODULE_CB_CLOSE:
            module->pf_deactivate = va_arg (ap, void_fp_vlcobjectt);
            break;

        case VLC_MODULE_NO_UNLOAD:
            module->b_unloadable = false;
            break;

        case VLC_MODULE_NAME:
        {
            const char *value = va_arg (ap, const char *);
            free( module->psz_object_name );
            module->psz_object_name = strdup( value );
            module->pp_shortcuts[0] = (char*)value; /* dooh! */
            if (module->psz_longname == default_name)
                module->psz_longname = (char*)value; /* dooh! */
            break;
        }

        case VLC_MODULE_SHORTNAME:
            module->psz_shortname = va_arg (ap, char *);
            break;

        case VLC_MODULE_DESCRIPTION:
            module->psz_longname = va_arg (ap, char *);
            break;

        case VLC_MODULE_HELP:
            module->psz_help = va_arg (ap, char *);
            break;

        case VLC_MODULE_TEXTDOMAIN:
            module->domain = va_arg (ap, char *);
            break;

        case VLC_CONFIG_NAME:
        {
            const char *name = va_arg (ap, const char *);
            vlc_callback_t cb = va_arg (ap, vlc_callback_t);

            assert (name != NULL);
            item->psz_name = strdup (name);
            item->pf_callback = cb;
            break;
        }

        case VLC_CONFIG_VALUE:
        {
            if (IsConfigIntegerType (item->i_type))
            {
                item->orig.i = item->saved.i =
                item->value.i = va_arg (ap, int);
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->orig.f = item->saved.f =
                item->value.f = va_arg (ap, double);
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *value = va_arg (ap, const char *);
                item->value.psz = value ? strdup (value) : NULL;
                item->orig.psz = value ? strdup (value) : NULL;
                item->saved.psz = value ? strdup (value) : NULL;
            }
            break;
        }

        case VLC_CONFIG_RANGE:
        {
            if (IsConfigIntegerType (item->i_type)
             || item->i_type == CONFIG_ITEM_MODULE_LIST_CAT
             || item->i_type == CONFIG_ITEM_MODULE_CAT)
            {
                item->min.i = va_arg (ap, int);
                item->max.i = va_arg (ap, int);
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->min.f = va_arg (ap, double);
                item->max.f = va_arg (ap, double);
            }
            break;
        }

        case VLC_CONFIG_ADVANCED:
            item->b_advanced = true;
            break;

        case VLC_CONFIG_VOLATILE:
            item->b_unsaveable = true;
            break;

        case VLC_CONFIG_PERSISTENT:
            item->b_autosave = true;
            break;

        case VLC_CONFIG_RESTART:
            item->b_restart = true;
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

        case VLC_CONFIG_OLDNAME:
        {
            const char *oldname = va_arg (ap, const char *);
            item->psz_oldname = oldname ? strdup (oldname) : NULL;
            break;
        }

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

            /* Copy values */
            if (IsConfigIntegerType (item->i_type))
            {
                const int *src = va_arg (ap, const int *);
                int *dst = malloc (sizeof (int) * (len + 1));

                if (dst != NULL)
                {
                    memcpy (dst, src, sizeof (int) * len);
                    dst[len] = 0;
                }
                item->pi_list = dst;
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *const *src = va_arg (ap, const char *const *);
                char **dst = malloc (sizeof (char *) * (len + 1));

                if (dst != NULL)
                {
                    for (size_t i = 0; i < len; i++)
                        dst[i] = src[i] ? strdup (src[i]) : NULL;
                    dst[len] = NULL;
                }
                item->ppsz_list = dst;
            }
            else
                break;

            /* Copy textual descriptions */
            const char *const *text = va_arg (ap, const char *const *);
            if (text != NULL)
            {
                char **dtext = malloc (sizeof (char *) * (len + 1));
                if( dtext != NULL )
                {
                    for (size_t i = 0; i < len; i++)
                        dtext[i] = text[i] ? strdup (text[i]) : NULL;
                    dtext[len] = NULL;
                }
                item->ppsz_list_text = dtext;
            }
            else
                item->ppsz_list_text = NULL;

            item->i_list = len;
            item->pf_update_list = va_arg (ap, vlc_callback_t);
            break;
        }

        case VLC_CONFIG_ADD_ACTION:
        {
            vlc_callback_t cb = va_arg (ap, vlc_callback_t), *tabcb;
            const char *name = va_arg (ap, const char *);
            char **tabtext;

            tabcb = realloc (item->ppf_action,
                             (item->i_action + 2) * sizeof (cb));
            if (tabcb == NULL)
                break;
            item->ppf_action = tabcb;
            tabcb[item->i_action] = cb;
            tabcb[item->i_action + 1] = NULL;

            tabtext = realloc (item->ppsz_action_text,
                               (item->i_action + 2) * sizeof (name));
            if (tabtext == NULL)
                break;
            item->ppsz_action_text = tabtext;

            if (name)
                tabtext[item->i_action] = strdup (name);
            else
                tabtext[item->i_action] = NULL;
            tabtext[item->i_action + 1] = NULL;

            item->i_action++;
            break;
        }

        default:
            fprintf (stderr, "LibVLC: unknown module property %d\n", propid);
            fprintf (stderr, "LibVLC: too old to use this module?\n");
            ret = -1;
            break;
    }

    va_end (ap);
    return ret;
}
