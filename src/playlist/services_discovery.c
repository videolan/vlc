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
#include "vlc_playlist.h"
#include "vlc_events.h"
#include <vlc_services_discovery.h>
#include <vlc_probe.h>
#include <vlc_modules.h>
#include "playlist_internal.h"
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

    char **names = malloc (sizeof(char *) * (count + 1));
    char **longnames = malloc (sizeof(char *) * (count + 1));
    int *categories = malloc(sizeof(int) * (count + 1));

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


static void services_discovery_Destructor ( vlc_object_t *p_obj );

/*
 * Services discovery
 * Basically you just listen to Service discovery event through the
 * sd's event manager.
 * That's how the playlist get's Service Discovery information
 */

/*******************************************************************//**
 * Create a Service discovery
 ***********************************************************************/
services_discovery_t *vlc_sd_Create( vlc_object_t *p_super,
                                     const char *cfg )
{
    services_discovery_t *p_sd;

    p_sd = vlc_custom_create( p_super, sizeof( *p_sd ), "services discovery" );
    if( !p_sd )
        return NULL;
    free(config_ChainCreate( &p_sd->psz_name, &p_sd->p_cfg, cfg ));

    vlc_event_manager_t *em = &p_sd->event_manager;
    vlc_event_manager_init( em, p_sd );
    vlc_event_manager_register_event_type(em, vlc_ServicesDiscoveryItemAdded);
    vlc_event_manager_register_event_type(em, vlc_ServicesDiscoveryItemRemoved);

    vlc_object_set_destructor( p_sd, services_discovery_Destructor );
    return p_sd;
}

/*******************************************************************//**
 * Start a Service Discovery
 ***********************************************************************/
bool vlc_sd_Start ( services_discovery_t * p_sd )
{
    assert(!p_sd->p_module);

    p_sd->p_module = module_need( p_sd, "services_discovery",
                                  p_sd->psz_name, true );
    if( p_sd->p_module == NULL )
    {
        msg_Err( p_sd, "no suitable services discovery module" );
        return false;
    }

    return true;
}

/*******************************************************************//**
 * Stop a Service Discovery
 ***********************************************************************/
void vlc_sd_Stop ( services_discovery_t * p_sd )
{
    module_unneed( p_sd, p_sd->p_module );
    p_sd->p_module = NULL;
}

/*******************************************************************//**
 * Destroy a Service Discovery
 ***********************************************************************/
void vlc_sd_Destroy( services_discovery_t *p_sd )
{
    config_ChainDestroy( p_sd->p_cfg );
    free( p_sd->psz_name );
    vlc_object_release( p_sd );
}

/*******************************************************************//**
 * Destructor of the Service Discovery
 ***********************************************************************/
static void services_discovery_Destructor ( vlc_object_t *p_obj )
{
    services_discovery_t * p_sd = (services_discovery_t *)p_obj;
    assert(!p_sd->p_module); /* Forgot to call Stop */
    vlc_event_manager_fini( &p_sd->event_manager );
}

/*******************************************************************//**
 * Get the Localized Name
 *
 * This is useful for interfaces and libVLC
 ***********************************************************************/
char *
services_discovery_GetLocalizedName ( services_discovery_t * p_sd )
{
    if (p_sd->p_module == NULL)
        return NULL;
    return strdup( module_get_name( p_sd->p_module, true ) );
}

/*******************************************************************//**
 * Getter for the EventManager
 *
 * You can receive event notification
 * This is the preferred way to get new items
 ***********************************************************************/
vlc_event_manager_t *
services_discovery_EventManager ( services_discovery_t * p_sd )
{
    return &p_sd->event_manager;
}

/*******************************************************************//**
 * Add an item to the Service Discovery listing
 ***********************************************************************/
void
services_discovery_AddItem ( services_discovery_t * p_sd, input_item_t * p_item,
                             const char * psz_category )
{
    vlc_event_t event;
    event.type = vlc_ServicesDiscoveryItemAdded;
    event.u.services_discovery_item_added.p_new_item = p_item;
    event.u.services_discovery_item_added.psz_category = psz_category;

    vlc_event_send( &p_sd->event_manager, &event );
}

/*******************************************************************//**
 * Remove an item from the Service Discovery listing
 ***********************************************************************/
void
services_discovery_RemoveItem ( services_discovery_t * p_sd, input_item_t * p_item )
{
    vlc_event_t event;
    event.type = vlc_ServicesDiscoveryItemRemoved;
    event.u.services_discovery_item_removed.p_item = p_item;

    vlc_event_send( &p_sd->event_manager, &event );
}

/*
 * Playlist - Services discovery bridge
 */

struct vlc_sd_internal_t
{
    /* the playlist items for category and onelevel */
    playlist_item_t      *node;
    services_discovery_t *sd; /**< Loaded service discovery modules */
    char name[];
};

 /* A new item has been added to a certain sd */
static void playlist_sd_item_added( const vlc_event_t * p_event, void * user_data )
{
    input_item_t * p_input = p_event->u.services_discovery_item_added.p_new_item;
    const char * psz_cat = p_event->u.services_discovery_item_added.psz_category;
    playlist_item_t * p_parent = user_data;
    playlist_t * p_playlist = p_parent->p_playlist;

    msg_Dbg( p_playlist, "Adding %s in %s",
                p_input->psz_name ? p_input->psz_name : "(null)",
                psz_cat ? psz_cat : "(null)" );

    PL_LOCK;
    /* If p_parent is in root category (this is clearly a hack) and we have a cat */
    if( !EMPTY_STR(psz_cat) )
    {
        /* */
        playlist_item_t * p_cat;
        p_cat = playlist_ChildSearchName( p_parent, psz_cat );
        if( !p_cat )
        {
            p_cat = playlist_NodeCreate( p_playlist, psz_cat,
                                         p_parent, PLAYLIST_END, 0, NULL );
            p_cat->i_flags |= PLAYLIST_RO_FLAG | PLAYLIST_SKIP_FLAG;
        }
        p_parent = p_cat;
    }

    playlist_NodeAddInput( p_playlist, p_input, p_parent,
                           PLAYLIST_APPEND, PLAYLIST_END,
                           pl_Locked );
    PL_UNLOCK;
}

 /* A new item has been removed from a certain sd */
static void playlist_sd_item_removed( const vlc_event_t * p_event, void * user_data )
{
    input_item_t * p_input = p_event->u.services_discovery_item_removed.p_item;
    playlist_item_t * p_sd_node = user_data;
    playlist_t *p_playlist = p_sd_node->p_playlist;

    PL_LOCK;
    playlist_item_t *p_item =
        playlist_ItemFindFromInputAndRoot( p_playlist, p_input,
                                           p_sd_node, false );
    if( !p_item )
    {
        PL_UNLOCK; return;
    }
    playlist_item_t *p_parent = p_item->p_parent;
    /* if the item was added under a category and the category node
       becomes empty, delete that node as well */
    if( p_parent->i_children > 1 || p_parent == p_sd_node )
        playlist_DeleteItem( p_playlist, p_item, true );
    else
        playlist_NodeDelete( p_playlist, p_parent, true, true );
    PL_UNLOCK;
}

int playlist_ServicesDiscoveryAdd(playlist_t *playlist, const char *chain)
{
    vlc_sd_internal_t *sds = malloc(sizeof (*sds) + strlen(chain) + 1);
    if (unlikely(sds == NULL))
        return VLC_ENOMEM;

    /* Look for configuration chain "longname" */
    const char *longname = "?";
    config_chain_t *cfg;
    char *name;

    free(config_ChainCreate(&name, &cfg, chain));
    msg_Dbg(playlist, "adding services_discovery %s...", name);

    for (config_chain_t *p = cfg; p != NULL; p = p->p_next)
        if (cfg->psz_name != NULL && !strcmp(cfg->psz_name, "longname"))
        {
            if (cfg->psz_value != NULL)
                longname = cfg->psz_value;
            break;
        }

    playlist_Lock(playlist);
    sds->node = playlist_NodeCreate(playlist, longname, playlist->p_root,
                                    PLAYLIST_END,
                                    PLAYLIST_RO_FLAG|PLAYLIST_SKIP_FLAG, NULL);
    playlist_Unlock(playlist);

    config_ChainDestroy(cfg);
    free(name);

    if (unlikely(sds->node == NULL))
    {
        free(sds);
        return VLC_ENOMEM;
    }

    /* Perform the addition */
    sds->sd = vlc_sd_Create(VLC_OBJECT(playlist), chain);
    if (unlikely(sds->sd == NULL))
    {
        playlist_Lock(playlist);
        playlist_NodeDelete(playlist, sds->node, true, false);
        playlist_Unlock(playlist);
        free(sds);
        return VLC_ENOMEM;
    }

    vlc_event_manager_t *em = services_discovery_EventManager(sds->sd);
    vlc_event_attach(em, vlc_ServicesDiscoveryItemAdded,
                     playlist_sd_item_added, sds->node);
    vlc_event_attach(em, vlc_ServicesDiscoveryItemRemoved,
                     playlist_sd_item_removed, sds->node);

    if (!vlc_sd_Start(sds->sd))
    {
        vlc_sd_Destroy(sds->sd);
        playlist_Lock(playlist);
        playlist_NodeDelete(playlist, sds->node, true, false);
        playlist_Unlock(playlist);
        free(sds);
        return VLC_EGENERIC;
    }

    strcpy(sds->name, chain);

    playlist_Lock(playlist);
    TAB_APPEND(pl_priv(playlist)->i_sds, pl_priv(playlist)->pp_sds, sds);
    playlist_Unlock(playlist);
    return VLC_SUCCESS;
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
            REMOVE_ELEM(priv->pp_sds, priv->i_sds, i);
            sds = entry;
            break;
        }
    }
    playlist_Unlock(playlist);

    if (sds == NULL)
    {
        msg_Warn(playlist, "discovery %s is not loaded", name);
        return VLC_EGENERIC;
    }

    assert(sds->sd != NULL);

    vlc_sd_Stop(sds->sd);

    vlc_event_detach(services_discovery_EventManager(sds->sd),
                     vlc_ServicesDiscoveryItemAdded,
                     playlist_sd_item_added, sds->node);
    vlc_event_detach(services_discovery_EventManager(sds->sd),
                     vlc_ServicesDiscoveryItemRemoved,
                     playlist_sd_item_removed, sds->node);
    vlc_sd_Destroy(sds->sd);

    /* Remove the sd playlist node if it exists */
    playlist_Lock(playlist);
    playlist_NodeDelete(playlist, sds->node, true, false);
    playlist_Unlock(playlist);

    free(sds);

    return VLC_SUCCESS;
}

bool playlist_IsServicesDiscoveryLoaded( playlist_t * p_playlist,
                                         const char *psz_name )
{
    playlist_private_t *priv = pl_priv( p_playlist );
    bool found = false;
    PL_LOCK;

    for( int i = 0; i < priv->i_sds; i++ )
    {
        vlc_sd_internal_t *sds = priv->pp_sds[i];

        if (!strcmp(psz_name, sds->name))
        {
            found = true;
            break;
        }
    }
    PL_UNLOCK;
    return found;
}

int playlist_ServicesDiscoveryControl( playlist_t *p_playlist, const char *psz_name, int i_control, ... )
{
    playlist_private_t *priv = pl_priv( p_playlist );
    int i_ret = VLC_EGENERIC;
    int i;

    PL_LOCK;
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
    PL_UNLOCK;

    return i_ret;
}

void playlist_ServicesDiscoveryKillAll( playlist_t *p_playlist )
{
    playlist_private_t *priv = pl_priv( p_playlist );

    while( priv->i_sds > 0 )
        playlist_ServicesDiscoveryRemove(p_playlist,
                                         priv->pp_sds[0]->name);
}
