/*****************************************************************************
 * media_discoverer.c: libvlc new API media discoverer functions
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_discoverer.h>
#include <vlc/libvlc_events.h>

#include <vlc_services_discovery.h>

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_new_from_input_item()
#include "media_list_internal.h" // _libvlc_media_list_add_media()

struct libvlc_media_discoverer_t
{
    libvlc_event_manager_t * p_event_manager;
    libvlc_instance_t *      p_libvlc_instance;
    services_discovery_t *   p_sd;
    libvlc_media_list_t *    p_mlist;
    bool                     running;
    vlc_dictionary_t         catname_to_submedialist;
};

/*
 * Private functions
 */

/**************************************************************************
 *       services_discovery_item_added (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_item_added( const vlc_event_t * p_event,
                                           void * user_data )
{
    input_item_t * p_item = p_event->u.services_discovery_item_added.p_new_item;
    const char * psz_cat = p_event->u.services_discovery_item_added.psz_category;
    libvlc_media_t * p_md;
    libvlc_media_discoverer_t * p_mdis = user_data;
    libvlc_media_list_t * p_mlist = p_mdis->p_mlist;

    p_md = libvlc_media_new_from_input_item( p_mdis->p_libvlc_instance,
                                             p_item );

    /* If we have a category, that mean we have to group the items having
     * that category in a media_list. */
    if( psz_cat )
    {
        p_mlist = vlc_dictionary_value_for_key( &p_mdis->catname_to_submedialist, psz_cat );

        if( p_mlist == kVLCDictionaryNotFound )
        {
            libvlc_media_t * p_catmd;
            p_catmd = libvlc_media_new_as_node( p_mdis->p_libvlc_instance, psz_cat );
            p_mlist = libvlc_media_subitems( p_catmd );
            p_mlist->b_read_only = true;

            /* Insert the newly created mlist in our dictionary */
            vlc_dictionary_insert( &p_mdis->catname_to_submedialist, psz_cat, p_mlist );

            /* Insert the md into the root list */
            libvlc_media_list_lock( p_mdis->p_mlist );
            _libvlc_media_list_add_media( p_mdis->p_mlist, p_catmd );
            libvlc_media_list_unlock( p_mdis->p_mlist );

            /* We don't release the mlist cause the dictionary
             * doesn't retain the object. But we release the md. */
            libvlc_media_release( p_catmd );
        }
    }

    libvlc_media_list_lock( p_mlist );
    _libvlc_media_list_add_media( p_mlist, p_md );
    libvlc_media_list_unlock( p_mlist );

    libvlc_media_release( p_md );
}

/**************************************************************************
 *       services_discovery_item_removed (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_item_removed( const vlc_event_t * p_event,
                                             void * user_data )
{
    input_item_t * p_item = p_event->u.services_discovery_item_added.p_new_item;
    libvlc_media_t * p_md;
    libvlc_media_discoverer_t * p_mdis = user_data;

    int i, count = libvlc_media_list_count( p_mdis->p_mlist );
    libvlc_media_list_lock( p_mdis->p_mlist );
    for( i = 0; i < count; i++ )
    {
        p_md = libvlc_media_list_item_at_index( p_mdis->p_mlist, i );
        if( p_md->p_input_item == p_item )
        {
            _libvlc_media_list_remove_index( p_mdis->p_mlist, i );
            break;
        }
    }
    libvlc_media_list_unlock( p_mdis->p_mlist );
}

/**************************************************************************
 *       services_discovery_removeall (Private) (VLC event callback)
 **************************************************************************/
static void services_discovery_removeall( const vlc_event_t * p_event,
                                             void * user_data )
{
    VLC_UNUSED(p_event);
    libvlc_media_discoverer_t * p_mdis = user_data;

    libvlc_media_list_lock( p_mdis->p_mlist );
    for( int i = 0; i < libvlc_media_list_count( p_mdis->p_mlist ); i++ )
    {
        _libvlc_media_list_remove_index( p_mdis->p_mlist, i );
    }
    libvlc_media_list_unlock( p_mdis->p_mlist );
}

/**************************************************************************
 *       services_discovery_started (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_started( const vlc_event_t * p_event,
                                        void * user_data )
{
    VLC_UNUSED(p_event);
    libvlc_media_discoverer_t * p_mdis = user_data;
    libvlc_event_t event;
    p_mdis->running = true;
    event.type = libvlc_MediaDiscovererStarted;
    libvlc_event_send( p_mdis->p_event_manager, &event );
}

/**************************************************************************
 *       services_discovery_ended (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_ended( const vlc_event_t * p_event,
                                      void * user_data )
{
    VLC_UNUSED(p_event);
    libvlc_media_discoverer_t * p_mdis = user_data;
    libvlc_event_t event;
    p_mdis->running = false;
    event.type = libvlc_MediaDiscovererEnded;
    libvlc_event_send( p_mdis->p_event_manager, &event );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name )
{
    /* podcast SD is a hack and only works with custom playlist callbacks. */
    if( !strncasecmp( psz_name, "podcast", 7 ) )
        return NULL;

    libvlc_media_discoverer_t *p_mdis = malloc(sizeof(*p_mdis));
    if( unlikely(!p_mdis) )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_mdis->p_libvlc_instance = p_inst;
    p_mdis->p_mlist = libvlc_media_list_new( p_inst );
    p_mdis->p_mlist->b_read_only = true;
    p_mdis->running = false;

    vlc_dictionary_init( &p_mdis->catname_to_submedialist, 0 );

    p_mdis->p_event_manager = libvlc_event_manager_new( p_mdis, p_inst );
    if( unlikely(p_mdis->p_event_manager == NULL) )
    {
        free( p_mdis );
        return NULL;
    }

    libvlc_event_manager_register_event_type( p_mdis->p_event_manager,
            libvlc_MediaDiscovererStarted );
    libvlc_event_manager_register_event_type( p_mdis->p_event_manager,
            libvlc_MediaDiscovererEnded );

    p_mdis->p_sd = vlc_sd_Create( (vlc_object_t*)p_inst->p_libvlc_int,
                                  psz_name );
    if( unlikely(p_mdis->p_sd == NULL) )
    {
        libvlc_printerr( "%s: no such discovery module found", psz_name );
        libvlc_media_list_release( p_mdis->p_mlist );
        libvlc_event_manager_release( p_mdis->p_event_manager );
        free( p_mdis );
        return NULL;
    }

    vlc_event_attach( services_discovery_EventManager( p_mdis->p_sd ),
                      vlc_ServicesDiscoveryItemAdded,
                      services_discovery_item_added,
                      p_mdis );
    vlc_event_attach( services_discovery_EventManager( p_mdis->p_sd ),
                      vlc_ServicesDiscoveryItemRemoved,
                      services_discovery_item_removed,
                      p_mdis );
    vlc_event_attach( services_discovery_EventManager( p_mdis->p_sd ),
                      vlc_ServicesDiscoveryStarted,
                      services_discovery_started,
                      p_mdis );
    vlc_event_attach( services_discovery_EventManager( p_mdis->p_sd ),
                      vlc_ServicesDiscoveryEnded,
                      services_discovery_ended,
                      p_mdis );
    vlc_event_attach( services_discovery_EventManager( p_mdis->p_sd ),
                      vlc_ServicesDiscoveryItemRemoveAll,
                      services_discovery_removeall,
                      p_mdis );

    /* Here we go */
    if( !vlc_sd_Start( p_mdis->p_sd ) )
    {
        libvlc_printerr( "%s: internal module error",
                         p_mdis->p_sd->psz_name );
        libvlc_media_list_release( p_mdis->p_mlist );
        libvlc_event_manager_release( p_mdis->p_event_manager );
        free( p_mdis );
        return NULL;
    }

    return p_mdis;
}

/**************************************************************************
 * release (Public)
 **************************************************************************/
void
libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis )
{
    int i;

    vlc_event_detach( services_discovery_EventManager( p_mdis->p_sd ),
                     vlc_ServicesDiscoveryItemAdded,
                     services_discovery_item_added,
                     p_mdis );
    vlc_event_detach( services_discovery_EventManager( p_mdis->p_sd ),
                     vlc_ServicesDiscoveryItemRemoved,
                     services_discovery_item_removed,
                     p_mdis );
    vlc_event_detach( services_discovery_EventManager( p_mdis->p_sd ),
                     vlc_ServicesDiscoveryStarted,
                     services_discovery_started,
                     p_mdis );
    vlc_event_detach( services_discovery_EventManager( p_mdis->p_sd ),
                     vlc_ServicesDiscoveryEnded,
                     services_discovery_ended,
                     p_mdis );
    vlc_event_detach( services_discovery_EventManager( p_mdis->p_sd ),
                     vlc_ServicesDiscoveryItemRemoveAll,
                     services_discovery_removeall,
                     p_mdis );

    libvlc_media_list_release( p_mdis->p_mlist );

    vlc_sd_StopAndDestroy( p_mdis->p_sd );

    /* Free catname_to_submedialist and all the mlist */
    char ** all_keys = vlc_dictionary_all_keys( &p_mdis->catname_to_submedialist );
    for( i = 0; all_keys[i]; i++ )
    {
        libvlc_media_list_t * p_catmlist = vlc_dictionary_value_for_key( &p_mdis->catname_to_submedialist, all_keys[i] );
        libvlc_media_list_release( p_catmlist );
        free( all_keys[i] );
    }
    free( all_keys );

    vlc_dictionary_clear( &p_mdis->catname_to_submedialist, NULL, NULL );
    libvlc_event_manager_release( p_mdis->p_event_manager );

    free( p_mdis );
}

/**************************************************************************
 * localized_name (Public)
 **************************************************************************/
char *
libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis )
{
    return services_discovery_GetLocalizedName( p_mdis->p_sd );
}

/**************************************************************************
 * media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis )
{
    libvlc_media_list_retain( p_mdis->p_mlist );
    return p_mdis->p_mlist;
}

/**************************************************************************
 * event_manager (Public)
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis )
{
    return p_mdis->p_event_manager;
}


/**************************************************************************
 * running (Public)
 **************************************************************************/
int
libvlc_media_discoverer_is_running( libvlc_media_discoverer_t * p_mdis )
{
    return p_mdis->running;
}
