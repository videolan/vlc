/*****************************************************************************
 * entry.c : Callbacks for module entry point
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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

#include <vlc/vlc.h>
#include <assert.h>
#include <stdarg.h>

#include "modules/modules.h"
#include "config/config.h"
#include "libvlc.h"

static const char default_name[] = "unnamed";

module_t *vlc_module_create (vlc_object_t *obj)
{
    module_t *module =
        (module_t *)vlc_custom_create (obj, sizeof (module_t),
                                       VLC_OBJECT_MODULE, "module");
    if (module == NULL)
        return NULL;

    module->b_reentrant = module->b_unloadable = VLC_TRUE;
    module->psz_object_name = module->psz_longname = default_name;
    module->psz_capability = "";
    module->i_score = 1;
    module->i_config_items = module->i_bool_items = 0;

    return module;
}


module_t *vlc_submodule_create (module_t *module)
{
    assert (module != NULL);
    assert (!module->b_submodule); // subsubmodules are not supported

    module_t *submodule =
        (module_t *)vlc_custom_create (VLC_OBJECT (module), sizeof (module_t),
                                       VLC_OBJECT_MODULE, "submodule");
    if (submodule == NULL)
        return NULL;

    vlc_object_attach (submodule, module);
    submodule->b_submodule = VLC_TRUE;

    /* Muahahaha! Heritage! Polymorphism! Ugliness!! */
    memcpy (submodule->pp_shortcuts, module->pp_shortcuts,
            sizeof (submodule->pp_shortcuts));

    submodule->psz_object_name = module->psz_object_name;
    submodule->psz_shortname = module->psz_shortname;
    submodule->psz_longname = module->psz_longname;
    submodule->psz_capability = module->psz_capability;
    submodule->i_score = module->i_score;
    submodule->i_cpu = module->i_cpu;
    return submodule;
}


int vlc_module_set (module_t *module, int propid, void *value)
{
    switch (propid)
    {
        case VLC_MODULE_CPU_REQUIREMENT:
            assert (!module->b_submodule);
            module->i_cpu |= (intptr_t)value;
            break;

        case VLC_MODULE_SHORTCUT:
        {
            unsigned i;
            for (i = 0; module->pp_shortcuts[i] != NULL; i++);
            if (i >= (MODULE_SHORTCUT_MAX - 1))
                return VLC_ENOMEM;

            module->pp_shortcuts[i] = (char *)value;
            break;
        }

        case VLC_MODULE_SHORTNAME:
            module->psz_shortname = (char *)value;
            break;

        case VLC_MODULE_DESCRIPTION:
            module->psz_longname = (char *)value;
            break;

        case VLC_MODULE_HELP:
            module->psz_help = (char *)value;
            break;

        case VLC_MODULE_CAPABILITY:
            module->psz_capability = (char *)value;
            break;

        case VLC_MODULE_SCORE:
            module->i_score = (intptr_t)value;
            break;

        case VLC_MODULE_CB_OPEN:
            module->pf_activate = (int (*) (vlc_object_t *))value;
            break;

        case VLC_MODULE_CB_CLOSE:
            module->pf_deactivate = (void (*) (vlc_object_t *))value;
            break;

        case VLC_MODULE_UNLOADABLE:
            module->b_unloadable = (value != NULL);
            break;

        case VLC_MODULE_NAME:
            module->pp_shortcuts[0] = module->psz_object_name = (char *)value;
            if (module->psz_longname == default_name)
                module->psz_longname = (char *)value;
            break;

        case VLC_MODULE_PROGRAM:
            msg_Warn (module, "deprecated module property %d", propid);
            break;

        default:
            msg_Err (module, "unknown module property %d", propid);
            msg_Err (module, "LibVLC might be too old to use this module.");
            return VLC_EGENERIC;
    }
    return 0;
}

module_config_t *vlc_config_create (module_t *module, int type)
{
    unsigned confsize = module->confsize;
    module_config_t *tab = module->p_config;

    if ((confsize & 0xf) == 0)
    {
        tab = realloc (tab, (confsize + 17) * sizeof (*tab));
        if (tab == NULL)
            return NULL;

        module->p_config = tab;
    }

    memset (tab + confsize, 0, sizeof (tab[confsize]));
    tab[confsize].i_type = type;
    tab[confsize].p_lock = &module->object_lock;

    if (type & CONFIG_ITEM)
    {
        module->i_config_items++;
        if (type == CONFIG_ITEM_BOOL)
            module->i_bool_items++;
    }

    module->confsize++;
    return tab + confsize;
}

int vlc_config_set (module_config_t *restrict item, int id, ...)
{
    int ret = -1;
    va_list ap;

    assert (item != NULL);
    va_start (ap, id);

    switch (id)
    {
        case VLC_CONFIG_NAME:
        {
            const char *name = va_arg (ap, const char *);
            vlc_callback_t cb = va_arg (ap, vlc_callback_t);

            assert (name != NULL);
            item->psz_name = strdup (name);
            item->pf_callback = cb;
            ret = 0;
            break;
        }

        case VLC_CONFIG_DESC:
        {
            const char *text = va_arg (ap, const char *);
            const char *longtext = va_arg (ap, const char *);

            item->psz_text = text ? strdup ( _(text)) : NULL;
            item->psz_longtext = longtext ? strdup ( _(longtext)) : NULL;
            ret = 0;
            break;
        }

        case VLC_CONFIG_VALUE:
        {
            if (IsConfigIntegerType (item->i_type))
            {
                item->orig.i = item->saved.i =
                item->value.i = va_arg (ap, int);
                ret = 0;
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->orig.f = item->saved.f =
                item->value.f = va_arg (ap, double);
                ret = 0;
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *value = va_arg (ap, const char *);
                item->value.psz = value ? strdup (value) : NULL;
                item->orig.psz = value ? strdup (value) : NULL;
                item->saved.psz = value ? strdup (value) : NULL;
                ret = 0;
            }
            break;
        }

        case VLC_CONFIG_RANGE:
        {
            if (IsConfigIntegerType (item->i_type))
            {
                item->min.i = va_arg (ap, int);
                item->max.i = va_arg (ap, int);
                ret = 0;
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->min.f = va_arg (ap, double);
                item->max.f = va_arg (ap, double);
                ret = 0;
            }
            break;
        }

        case VLC_CONFIG_ADVANCED:
            item->b_advanced = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_VOLATILE:
            item->b_unsaveable = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_PERSISTENT:
            item->b_autosave = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_RESTART:
            item->b_restart = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_PRIVATE:
            item->b_internal = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_REMOVED:
            item->b_removed = VLC_TRUE;
            ret = 0;
            break;

        case VLC_CONFIG_CAPABILITY:
        {
            const char *cap = va_arg (ap, const char *);
            item->psz_type = cap ? strdup (cap) : NULL;
            ret = 0;
            break;
        }

        case VLC_CONFIG_SHORTCUT:
            item->i_short = va_arg (ap, int);
            ret = 0;
            break;

        case VLC_CONFIG_LIST:
        {
            size_t len = va_arg (ap, size_t);
            char **dtext = malloc (sizeof (char *) * (len + 1));

            if (dtext == NULL)
                break;

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
#if 0
            if (IsConfigFloatType (item->i_type))
            {
                const float *src = va_arg (ap, const float *);
                float *dst = malloc (sizeof (float) * (len + 1));

                if (dst != NULL)
                {
                    memcpy (dst, src, sizeof (float) * len);
                    dst[len] = 0.;
                }
                item->pf_list = dst;
            }
            else
#endif
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
                for (size_t i = 0; i < len; i++)
                    dtext[i] = text[i] ? strdup ( _(text[i])) : NULL;

                dtext[len] = NULL;
                item->ppsz_list_text = dtext;
            }
            else
            {
                free (dtext);
                item->ppsz_list_text = NULL;
            }

            item->i_list = len;
            ret = 0;
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
                tabtext[item->i_action] = strdup ( _(name));
            else
                tabtext[item->i_action] = NULL;
            tabtext[item->i_action + 1] = NULL;

            item->i_action++;
            ret = 0;
            break;
        }

        case VLC_CONFIG_OLDNAME:
        {
            const char *oldname = va_arg (ap, const char *);
            item->psz_oldname = oldname ? strdup (oldname) : NULL;
            ret = 0;
            break;
        }

        case VLC_CONFIG_UNSAFE:
            item->b_unsafe = VLC_TRUE;
            ret = 0;
            break;
    }

    va_end (ap);
    return ret;
}
