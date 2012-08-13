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
        abort();
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
    vlc_event_manager_register_event_type(em, vlc_ServicesDiscoveryItemRemoveAll);
    vlc_event_manager_register_event_type(em, vlc_ServicesDiscoveryStarted);
    vlc_event_manager_register_event_type(em, vlc_ServicesDiscoveryEnded);

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

    vlc_event_t event = {
        .type = vlc_ServicesDiscoveryStarted
    };
    vlc_event_send( &p_sd->event_manager, &event );
    return true;
}

/*******************************************************************//**
 * Stop a Service Discovery
 ***********************************************************************/
void vlc_sd_Stop ( services_discovery_t * p_sd )
{
    vlc_event_t event = {
        .type = vlc_ServicesDiscoveryEnded
    };

    vlc_event_send( &p_sd->event_manager, &event );

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
 * Remove all items from the Service Discovery listing
 ***********************************************************************/
void
services_discovery_RemoveAll ( services_discovery_t * p_sd )
{
    vlc_event_t event;
    event.type = vlc_ServicesDiscoveryItemRemoveAll;

    vlc_event_send( &p_sd->event_manager, &event );
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
    playlist_item_t      *p_node;
    services_discovery_t *p_sd; /**< Loaded service discovery modules */
    char                 *psz_name;
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
            p_cat->i_flags &= ~PLAYLIST_SKIP_FLAG;
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

/* A request to remove all ideas from SD */
static void playlist_sd_item_removeall( const vlc_event_t * p_event, void * user_data )
{
    VLC_UNUSED(p_event);
    playlist_item_t* p_sd_node = user_data;
    if( p_sd_node == NULL ) return;
    playlist_t* p_playlist = p_sd_node->p_playlist;
    PL_LOCK;
    playlist_NodeEmpty( p_playlist, p_sd_node, true );
    PL_UNLOCK;
}

int playlist_ServicesDiscoveryAdd( playlist_t *p_playlist,
                                   const char *psz_name )
{
    /* Perform the addition */
    services_discovery_t *p_sd;

    msg_Dbg( p_playlist, "adding services_discovery %s...", psz_name );
    p_sd = vlc_sd_Create( VLC_OBJECT(p_playlist), psz_name );
    if( !p_sd )
        return VLC_ENOMEM;

    /* Free in playlist_ServicesDiscoveryRemove */
    vlc_sd_internal_t * p_sds = malloc( sizeof(*p_sds) );
    if( !p_sds )
    {
        vlc_sd_Destroy( p_sd );
        return VLC_ENOMEM;
    }

    playlist_item_t *p_node;

    /* Look for configuration chain "longname" */
    const char *psz_longname = "?";
    if( p_sd->p_cfg )
    {
        config_chain_t *cfg = p_sd->p_cfg;
        while( cfg )
        {
            if( cfg->psz_name && !strcmp( cfg->psz_name, "longname" ) )
            {
                psz_longname = cfg->psz_value;
                break;
            }
            cfg = cfg->p_next;
        }
    }

    PL_LOCK;
    p_node = playlist_NodeCreate( p_playlist, psz_longname,
                                  p_playlist->p_root, PLAYLIST_END, 0, NULL );
    PL_UNLOCK;

    vlc_event_manager_t *em = services_discovery_EventManager( p_sd );
    vlc_event_attach( em, vlc_ServicesDiscoveryItemAdded,
                      playlist_sd_item_added, p_node );

    vlc_event_attach( em, vlc_ServicesDiscoveryItemRemoved,
                      playlist_sd_item_removed, p_node );

    vlc_event_attach( em, vlc_ServicesDiscoveryItemRemoveAll,
                      playlist_sd_item_removeall, p_node );

    if( !vlc_sd_Start( p_sd ) )
    {
        vlc_sd_Destroy( p_sd );
        free( p_sds );
        return VLC_EGENERIC;
    }

    p_sds->p_sd = p_sd;
    p_sds->p_node = p_node;
    p_sds->psz_name = strdup( psz_name );

    PL_LOCK;
    TAB_APPEND( pl_priv(p_playlist)->i_sds, pl_priv(p_playlist)->pp_sds, p_sds );
    PL_UNLOCK;

    return VLC_SUCCESS;
}

int playlist_ServicesDiscoveryRemove( playlist_t * p_playlist,
                                      const char *psz_name )
{
    playlist_private_t *priv = pl_priv( p_playlist );
    vlc_sd_internal_t * p_sds = NULL;

    PL_LOCK;
    for( int i = 0; i < priv->i_sds; i++ )
    {
        if( !strcmp( psz_name, priv->pp_sds[i]->psz_name ) )
        {
            p_sds = priv->pp_sds[i];
            REMOVE_ELEM( priv->pp_sds, priv->i_sds, i );
            break;
        }
    }
    PL_UNLOCK;

    if( !p_sds )
    {
        msg_Warn( p_playlist, "discovery %s is not loaded", psz_name );
        return VLC_EGENERIC;
    }

    services_discovery_t *p_sd = p_sds->p_sd;
    assert( p_sd );

    vlc_sd_Stop( p_sd );

    vlc_event_detach( services_discovery_EventManager( p_sd ),
                        vlc_ServicesDiscoveryItemAdded,
                        playlist_sd_item_added,
                        p_sds->p_node );

    vlc_event_detach( services_discovery_EventManager( p_sd ),
                        vlc_ServicesDiscoveryItemRemoved,
                        playlist_sd_item_removed,
                        p_sds->p_node );

    /* Remove the sd playlist node if it exists */
    PL_LOCK;
    playlist_NodeDelete( p_playlist, p_sds->p_node, true, false );
    PL_UNLOCK;

    vlc_sd_Destroy( p_sd );
    free( p_sds->psz_name );
    free( p_sds );

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
        vlc_sd_internal_t *sd = priv->pp_sds[i];

        if( sd->psz_name && !strcmp( psz_name, sd->psz_name ) )
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
        vlc_sd_internal_t *sd = priv->pp_sds[i];
        if( sd->psz_name && !strcmp( psz_name, sd->psz_name ) )
        {
            va_list args;
            va_start( args, i_control );
            i_ret = vlc_sd_control( sd->p_sd, i_control, args );
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
        playlist_ServicesDiscoveryRemove( p_playlist,
                                          priv->pp_sds[0]->psz_name );
}
