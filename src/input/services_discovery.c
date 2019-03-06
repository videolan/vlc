/*****************************************************************************
 * services_discovery.c : Manage playlist services_discovery modules
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_probe.h>
#include <vlc_modules.h>
#include "../libvlc.h"

typedef struct
{
    char *name;
    char *longname;
    int category;
} vlc_sd_probe_t;

int vlc_sd_probe_Add (vlc_probe_t *probe, const char *name,
                      const char *longname, int category)
{
    vlc_sd_probe_t names = { strdup(name), strdup(longname), category };

    if (unlikely (names.name == NULL || names.longname == NULL
               || vlc_probe_add (probe, &names, sizeof (names))))
    {
        free (names.name);
        free (names.longname);
        return VLC_ENOMEM;
    }
    return VLC_PROBE_CONTINUE;
}

#undef vlc_sd_GetNames

/**
 * Gets the list of available services discovery plugins.
 */
char **vlc_sd_GetNames (vlc_object_t *obj, char ***pppsz_longnames, int **pp_categories)
{
    size_t count;
    vlc_sd_probe_t *tab = vlc_probe (obj, "services probe", &count);

    if (count == 0)
    {
        free (tab);
        return NULL;
    }

    char **names = vlc_alloc (count + 1, sizeof(char *));
    char **longnames = vlc_alloc (count + 1, sizeof(char *));
    int *categories = vlc_alloc (count + 1, sizeof(int));

    if (unlikely (names == NULL || longnames == NULL || categories == NULL))
    {
        free(names);
        free(longnames);
        free(categories);
        free(tab);
        return NULL;
    }
    for( size_t i = 0; i < count; i++ )
    {
        names[i] = tab[i].name;
        longnames[i] = tab[i].longname;
        categories[i] = tab[i].category;
    }
    free (tab);
    names[count] = longnames[count] = NULL;
    categories[count] = 0;
    *pppsz_longnames = longnames;
    if( pp_categories ) *pp_categories = categories;
    else free( categories );
    return names;
}

/*
 * Services discovery
 * Basically you just listen to Service discovery event through the
 * sd's event manager.
 * That's how the playlist get's Service Discovery information
 */

#undef vlc_sd_Create
services_discovery_t *vlc_sd_Create(vlc_object_t *parent, const char *cfg,
    const struct services_discovery_owner_t *restrict owner)
{
    services_discovery_t *sd = vlc_custom_create(parent, sizeof (*sd),
                                                 "services discovery");
    if (unlikely(sd == NULL))
        return NULL;

    free(config_ChainCreate(&sd->psz_name, &sd->p_cfg, cfg));
    sd->description = NULL;
    sd->owner = *owner;

    sd->p_module = module_need(sd, "services_discovery",
                               sd->psz_name, true);
    if (sd->p_module == NULL)
    {
        msg_Err(sd, "no suitable services discovery module");
        vlc_sd_Destroy(sd);
        sd = NULL;
    }

    return sd;
}

void vlc_sd_Destroy(services_discovery_t *sd)
{
    if (sd->p_module != NULL)
        module_unneed(sd, sd->p_module);
    config_ChainDestroy(sd->p_cfg);
    free(sd->psz_name);
    vlc_object_delete(sd);
}
