/*****************************************************************************
 * services_discovery.c : Manage playlist services_discovery modules
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
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
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include "playlist_internal.h"

struct vlc_sd_internal_t
{
    /* the playlist items for category and onelevel */
    playlist_item_t      *node;
    services_discovery_t *sd; /**< Loaded service discovery modules */
    char name[];
};

 /* A new item has been added to a certain sd */
static void playlist_sd_item_added(services_discovery_t *sd,
                                   input_item_t *parent, input_item_t *p_input,
                                   const char *psz_cat)
{
    assert(parent == NULL || psz_cat == NULL);

    vlc_sd_internal_t *sds = sd->owner.sys;
    playlist_t *playlist = (playlist_t *)sd->obj.parent;
    playlist_item_t *node;
    const char *longname = (sd->description != NULL) ? sd->description : "?";

    msg_Dbg(sd, "adding: %s", p_input->psz_name ? p_input->psz_name : "(null)");

    playlist_Lock(playlist);
    if (sds->node == NULL)
        sds->node = playlist_NodeCreate(playlist, longname, &playlist->root,
                                        PLAYLIST_END, PLAYLIST_RO_FLAG);

    if (parent != NULL)
        node = playlist_ItemGetByInput(playlist, parent);
    else
    if (psz_cat == NULL)
        node = sds->node;
    else
    {   /* Parent is NULL (root) and category is specified.
         * This is clearly a hack. TODO: remove this. */
        node = playlist_ChildSearchName(sds->node, psz_cat);
        if (node == NULL)
            node = playlist_NodeCreate(playlist, psz_cat, sds->node,
                                       PLAYLIST_END, PLAYLIST_RO_FLAG);
    }

    playlist_NodeAddInput(playlist, p_input, node, PLAYLIST_END);
    playlist_Unlock(playlist);
}

 /* A new item has been removed from a certain sd */
static void playlist_sd_item_removed(services_discovery_t *sd,
                                     input_item_t *p_input)
{
    vlc_sd_internal_t *sds = sd->owner.sys;
    playlist_t *playlist = (playlist_t *)sd->obj.parent;
    playlist_item_t *node, *item;

    msg_Dbg(sd, "removing: %s", p_input->psz_name ? p_input->psz_name : "(null)");

    playlist_Lock(playlist);
    item = playlist_ItemGetByInput(playlist, p_input);
    if (unlikely(item == NULL))
    {
        msg_Err(sd, "removing item not added"); /* SD plugin bug */
        playlist_Unlock(playlist);
        return;
    }

#ifndef NDEBUG
    /* Check that the item belonged to the SD */
    for (playlist_item_t *i = item->p_parent; i != sds->node; i = i->p_parent)
        assert(i != NULL);
#endif

    node = item->p_parent;
    /* if the item was added under a category and the category node
       becomes empty, delete that node as well */
    if (node != sds->node && node->i_children == 1)
        item = node;
    playlist_NodeDeleteExplicit(playlist, item,
        PLAYLIST_DELETE_FORCE | PLAYLIST_DELETE_STOP_IF_CURRENT );
    playlist_Unlock(playlist);
}

int playlist_ServicesDiscoveryAdd(playlist_t *playlist, const char *chain)
{
    vlc_sd_internal_t *sds = malloc(sizeof (*sds) + strlen(chain) + 1);
    if (unlikely(sds == NULL))
        return VLC_ENOMEM;

    sds->node = NULL;

    struct services_discovery_owner_t owner = {
        sds,
        playlist_sd_item_added,
        playlist_sd_item_removed,
    };

    /* Perform the addition */
    sds->sd = vlc_sd_Create(VLC_OBJECT(playlist), chain, &owner);
    if (unlikely(sds->sd == NULL))
    {
        free(sds);
        return VLC_ENOMEM;
    }

    strcpy(sds->name, chain);

    playlist_Lock(playlist);
    /* Backward compatibility with Qt UI: create the node even if the SD
     * has not discovered any item. */
    if (sds->node == NULL && sds->sd->description != NULL)
        sds->node = playlist_NodeCreate(playlist, sds->sd->description,
                                        &playlist->root, PLAYLIST_END,
                                        PLAYLIST_RO_FLAG);

    TAB_APPEND(pl_priv(playlist)->i_sds, pl_priv(playlist)->pp_sds, sds);
    playlist_Unlock(playlist);
    return VLC_SUCCESS;
}

static void playlist_ServicesDiscoveryInternalRemoveLocked(playlist_t *playlist,
                                                           vlc_sd_internal_t *sds)
{
    assert(sds->sd != NULL);

    playlist_Unlock(playlist);

    vlc_sd_Destroy(sds->sd);
    /* Remove the sd playlist node if it exists */
    playlist_Lock(playlist);

    if (sds->node != NULL)
        playlist_NodeDeleteExplicit(playlist, sds->node,
            PLAYLIST_DELETE_FORCE | PLAYLIST_DELETE_STOP_IF_CURRENT );

    free(sds);
}

int playlist_ServicesDiscoveryRemove(playlist_t *playlist, const char *name)
{
    playlist_private_t *priv = pl_priv(playlist);
    vlc_sd_internal_t *sds = NULL;

    playlist_Lock(playlist);
    for (int i = 0; i < priv->i_sds; i++)
    {
        vlc_sd_internal_t *entry = priv->pp_sds[i];

        if (!strcmp(name, entry->name))
        {
            TAB_ERASE(priv->i_sds, priv->pp_sds, i);
            sds = entry;
            break;
        }
    }

    if (sds == NULL)
    {
        msg_Warn(playlist, "discovery %s is not loaded", name);
        playlist_Unlock(playlist);
        return VLC_EGENERIC;
    }

    playlist_ServicesDiscoveryInternalRemoveLocked(playlist, sds);

    playlist_Unlock(playlist);

    return VLC_SUCCESS;
}

bool playlist_IsServicesDiscoveryLoaded( playlist_t * playlist,
                                         const char *psz_name )
{
    playlist_private_t *priv = pl_priv( playlist );
    bool found = false;
    playlist_Lock(playlist);

    for( int i = 0; i < priv->i_sds; i++ )
    {
        vlc_sd_internal_t *sds = priv->pp_sds[i];

        if (!strcmp(psz_name, sds->name))
        {
            found = true;
            break;
        }
    }
    playlist_Unlock(playlist);
    return found;
}

int playlist_ServicesDiscoveryControl( playlist_t *playlist, const char *psz_name, int i_control, ... )
{
    playlist_private_t *priv = pl_priv( playlist );
    int i_ret = VLC_EGENERIC;
    int i;

    playlist_Lock(playlist);
    for( i = 0; i < priv->i_sds; i++ )
    {
        vlc_sd_internal_t *sds = priv->pp_sds[i];
        if (!strcmp(psz_name, sds->name))
        {
            va_list args;
            va_start( args, i_control );
            i_ret = vlc_sd_control(sds->sd, i_control, args );
            va_end( args );
            break;
        }
    }

    assert( i != priv->i_sds );
    playlist_Unlock(playlist);

    return i_ret;
}

void playlist_ServicesDiscoveryKillAll(playlist_t *playlist)
{
    playlist_private_t *priv = pl_priv(playlist);

    playlist_Lock(playlist);
    while (priv->i_sds > 0)
    {
        vlc_sd_internal_t *sds = priv->pp_sds[priv->i_sds - 1];
        TAB_ERASE(priv->i_sds, priv->pp_sds, priv->i_sds - 1);

        playlist_ServicesDiscoveryInternalRemoveLocked(playlist, sds);
    }

    playlist_Unlock(playlist);
}
