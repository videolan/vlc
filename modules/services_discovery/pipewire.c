/*****************************************************************************
 * @file pipewire.c
 * @brief List of PipeWire sources for VLC media player
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Ayush Dey <deyayush6@gmail.com>
 *          Thomas Guillem <tguillem@videolan.org>
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
# include <config.h>
#endif

#include <search.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <pipewire/pipewire.h>
#include "audio_output/vlc_pipewire.h"

struct services_discovery_sys_t
{
    struct vlc_pw_context *context;
    struct spa_hook listener;
    void *nodes;
    void *root_card;
};

struct vlc_pw_node
{
    uint32_t id;
    uint64_t serial;
    input_item_t *item;
    services_discovery_t *sd;
};

struct card
{
    input_item_t *item;
    services_discovery_t *sd;
    char name[];
};

static void DestroySource (void *data)
{
    struct vlc_pw_node *node = data;

    services_discovery_RemoveItem (node->sd, node->item);
    input_item_Release (node->item);
    free (node);
}

static void DestroyCard(void *data)
{
    struct card *c = data;

    services_discovery_RemoveItem(c->sd, c->item);
    input_item_Release(c->item);
    free(c);
}

/**
 * Compares two nodes by ID (to support binary search).
 */
static int node_by_id(const void *a, const void *b)
{
    const struct vlc_pw_node *na = a, *nb = b;

    if (na->id > nb->id)
        return 1;
    if (na->id < nb->id)
        return -1;
    return 0;
}

static int cmpcard (const void *a, const void *b)
{
    const struct card *ca = a, *cb = b;
    return strcmp(ca->name, cb->name);
}

static input_item_t *AddCard (services_discovery_t *sd, const struct spa_dict *props)
{
    struct services_discovery_sys_t *sys = sd->p_sys;

    const char *card_name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (unlikely(card_name == NULL))
        card_name = N_("Generic");

    struct card *c = malloc(sizeof(*c) + strlen(card_name) + 1);
    if (unlikely(c == NULL))
        return NULL;
    strcpy(c->name, card_name);

    void **cp = tsearch(c, &sys->root_card, cmpcard);
    if (cp == NULL) /* Out-of-memory */
    {
        free(c);
        return NULL;
    }
    if (*cp != c)
    {
        free(c);
        c = *cp;
        assert(c->item != NULL);
        return c->item;
    }

    c->item = input_item_NewExt("vlc://nop", c->name,
                                INPUT_DURATION_INDEFINITE,
                                ITEM_TYPE_NODE, ITEM_LOCAL);

    if (unlikely(c->item == NULL))
    {
        tdelete(c, &sys->root_card, cmpcard);
        free(c);
        return NULL;
    }
    services_discovery_AddItem(sd, c->item);
    c->sd = sd;

    return c->item;
}

static void registry_node(services_discovery_t *sd, uint32_t id, uint32_t perms,
                          uint32_t version, const struct spa_dict *props)
{
    struct services_discovery_sys_t *sys = sd->p_sys;
    uint64_t serial;
    char *mrl;

    if (props == NULL)
        return;

    const char *class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (class == NULL)
        return;
    
    if (strstr(class, "Source") == NULL)
        return; /* Not a source */
    
    if (!spa_atou64(spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL), &serial, 0))
        return;

    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (unlikely(name == NULL))
        return;

    if (unlikely(asprintf (&mrl, "pipewire://%s", name) == -1))
        return;

    const char *desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (unlikely(desc == NULL))
        desc = "???";

    vlc_pw_debug(sys->context, "adding %s (%s)", name, desc);
    input_item_t *item = input_item_NewCard (mrl, desc);
    free (mrl);
    if (unlikely(item == NULL))
        return;

    struct vlc_pw_node *node = malloc(sizeof (*node));
    if (unlikely(node == NULL))
    {
        input_item_Release (item);
        return;
    }

    node->id = id;
    node->serial = serial;
    node->item = item;

    struct vlc_pw_node **pp = tsearch(node, &sys->nodes, node_by_id);
    if (unlikely(pp == NULL))
    { /* Memory allocation error in the tree */
        free(node);
        input_item_Release (item);
        return;
    }
    if (*pp != node)
    { /* Existing node, update it */
        free(node);
        node = *pp;
        input_item_SetURI (node->item, item->psz_uri);
        input_item_SetName (node->item, item->psz_name);
        input_item_Release (item);
        return;
    }

    input_item_t *card = AddCard(sd, props);
    services_discovery_AddSubItem(sd, card, item);
    node->sd = sd;
    return;
}

/**
 * Global callback.
 *
 * This gets called for every initial, then every new object from the PipeWire
 * server. We can find the usable sources through this.
 */
static void registry_global(void *data, uint32_t id, uint32_t perms,
                            const char *name, uint32_t version,
                            const struct spa_dict *props)
{
    services_discovery_t *sd = data;

    if (strcmp(name, PW_TYPE_INTERFACE_Node) == 0)
        registry_node(sd, id, perms, version, props);
}

/**
 * Global removal callback.
 *
 * This gets called when an object disappers. We can detect when a source is unplugged here.
 */
static void registry_global_remove(void *data, uint32_t id)
{
    services_discovery_t *sd = data;
    struct services_discovery_sys_t *sys = sd->p_sys;
    struct vlc_pw_node key = { .id = id };
    struct vlc_pw_node **pp = tfind(&key, &sys->nodes, node_by_id);
    if (pp == NULL)
        return;

    struct vlc_pw_node *node = *pp;
    tdelete(node, &sys->nodes, node_by_id);
    DestroySource (node);
}

static const struct pw_registry_events events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_global,
    .global_remove =  registry_global_remove
};

static int Open (vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t *)obj;
    
    struct services_discovery_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->context = vlc_pw_connect(obj, "services discovery");
    if (sys->context == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sd->p_sys = sys;
    sd->description = _("Audio and Video capture");
    sys->nodes = NULL;
    sys->root_card = NULL;
    sys->listener = (struct spa_hook){ };

    vlc_pw_lock(sys->context);
    /* Subscribe for source events */
    vlc_pw_registry_listen(sys->context, &sys->listener, &events, sd);
    vlc_pw_roundtrip_unlocked(sys->context); /* Enumerate existing sources */
    vlc_pw_unlock(sys->context);
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t *)obj;
    struct services_discovery_sys_t *sys = sd->p_sys;

    vlc_pw_disconnect(sys->context);
    tdestroy(sys->nodes, DestroySource);
    tdestroy(sys->root_card, DestroyCard);
    free (sys);
}

VLC_SD_PROBE_HELPER("pipewire", N_("Audio and Video capture"), SD_CAT_DEVICES);

vlc_module_begin ()
    set_shortname (N_("Audio and Video capture"))
    set_description (N_("Audio and Video capture (PipeWire)"))
    set_subcategory (SUBCAT_PLAYLIST_SD)
    set_capability ("services_discovery", 0)
    set_callbacks (Open, Close)
    add_shortcut ("pipewire", "pw")

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()
