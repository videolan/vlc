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
            return 0;

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
    module->confsize++;

    memset (tab + confsize, 0, sizeof (tab[confsize]));
    return tab + confsize;
}

int vlc_config_set (module_config_t *restrict item, vlc_config_t id, ...)
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

            item->psz_text = text ? strdup (gettext (text)) : NULL;
            item->psz_longtext = longtext ? strdup (gettext (text)) : NULL;
            ret = 0;
            break;
        }

        case VLC_CONFIG_VALUE:
        {
            if (IsConfigIntegerType (item->i_type))
            {
                item->value.i = va_arg (ap, int);
                ret = 0;
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->value.f = va_arg (ap, double);
                ret = 0;
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *value = va_arg (ap, const char *);
                item->value.psz = value ? strdup (value) : NULL;
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
            item->psz_current = "SUPPRESSED";
            ret = 0;
            break;

        case VLC_CONFIG_CAPABILITY:
        {
            const char *cap = va_arg (ap, const char *);
            item->psz_type = cap ? strdup (cap) : NULL;
            ret = 0;
            break;
        }
    }

    va_end (ap);
    return ret;
}
