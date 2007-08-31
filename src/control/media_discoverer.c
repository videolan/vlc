/*****************************************************************************
 * media_discoverer.c: libvlc new API media discoverer functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>
#include "vlc_playlist.h"

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
    libvlc_media_descriptor_t * p_md;
    libvlc_media_discoverer_t * p_mdis = user_data;

    p_md = libvlc_media_descriptor_new_from_input_item(
            p_mdis->p_libvlc_instance,
            p_item, NULL );

    libvlc_media_list_lock( p_mdis->p_mlist );
    libvlc_media_list_add_media_descriptor( p_mdis->p_mlist, p_md, NULL );
    libvlc_media_list_unlock( p_mdis->p_mlist );
}

/**************************************************************************
 *       services_discovery_item_removed (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_item_removed( const vlc_event_t * p_event,
                                             void * user_data )
{
    /* Not handled */
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
                                       const char * psz_name,
                                       libvlc_exception_t * p_e )
{
    libvlc_media_discoverer_t * p_mdis;
    
    p_mdis = malloc(sizeof(libvlc_media_discoverer_t));
    if( !p_mdis )
    {
        libvlc_exception_raise( p_e, "Not enough memory" );
        return NULL;
    }

    p_mdis->p_libvlc_instance = p_inst;
    p_mdis->p_mlist = libvlc_media_list_new( p_inst, NULL );
    p_mdis->p_sd = services_discovery_Create( (vlc_object_t*)p_inst->p_libvlc_int, psz_name );

    if( !p_mdis->p_sd )
    {
        free( p_mdis );
        libvlc_exception_raise( p_e, "Can't find the services_discovery module named '%s'", psz_name );
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
    
    services_discovery_Start( p_mdis->p_sd );

    /* Here we go */

    return p_mdis;
}

/**************************************************************************
 * release (Public)
 **************************************************************************/
void
libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis )
{
    libvlc_media_list_release( p_mdis->p_mlist );
    services_discovery_Destroy( p_mdis->p_sd );
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

