/*****************************************************************************
 * renderer_discoverer.c: libvlc renderer API
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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
#include <vlc/libvlc_renderer_discoverer.h>

#include <vlc_common.h>

#include "libvlc_internal.h"
#include "renderer_discoverer_internal.h"

struct libvlc_renderer_discoverer_t
{
    vlc_renderer_discovery_t *p_rd;
    libvlc_event_manager_t *p_event_manager;

    int                     i_items;
    vlc_renderer_item_t **  pp_items;
};

static_assert( VLC_RENDERER_CAN_AUDIO == LIBVLC_RENDERER_CAN_AUDIO &&
               VLC_RENDERER_CAN_VIDEO == LIBVLC_RENDERER_CAN_VIDEO,
              "core/libvlc renderer flags mismatch" );

const vlc_renderer_item_t *
libvlc_renderer_item_to_vlc( const libvlc_renderer_item_t *p_item )
{
    return (const vlc_renderer_item_t*) p_item;
}

static void
renderer_discovery_item_added( const vlc_event_t *p_event, void *p_user_data )
{
    libvlc_renderer_discoverer_t *p_lrd = p_user_data;
    vlc_renderer_item_t *p_item =
        p_event->u.renderer_discovery_item_added.p_new_item;

    vlc_renderer_item_hold( p_item );

    TAB_APPEND( p_lrd->i_items, p_lrd->pp_items, p_item );

    libvlc_event_t event = {
        .type = libvlc_RendererDiscovererItemAdded,
        .u.renderer_discoverer_item_added.item =
            (libvlc_renderer_item_t*) p_item,
    };
    libvlc_event_send( p_lrd->p_event_manager, &event );
}

static void
renderer_discovery_item_removed( const vlc_event_t *p_event, void *p_user_data )
{
    libvlc_renderer_discoverer_t *p_lrd = p_user_data;
    vlc_renderer_item_t *p_item =
        p_event->u.renderer_discovery_item_removed.p_item;

    int i_idx;
    TAB_FIND( p_lrd->i_items, p_lrd->pp_items, p_item, i_idx );
    assert( i_idx != -1 );
    TAB_ERASE( p_lrd->i_items, p_lrd->pp_items, i_idx );

    libvlc_event_t event = {
        .type = libvlc_RendererDiscovererItemDeleted,
        .u.renderer_discoverer_item_deleted.item =
            (libvlc_renderer_item_t*) p_item,
    };
    libvlc_event_send( p_lrd->p_event_manager, &event );

    vlc_renderer_item_release( p_item );
}

const char *
libvlc_renderer_item_name( const libvlc_renderer_item_t *p_item )
{
    return vlc_renderer_item_name( (vlc_renderer_item_t *) p_item );
}

const char *
libvlc_renderer_item_type( const libvlc_renderer_item_t *p_item )
{
    return vlc_renderer_item_type( (vlc_renderer_item_t *) p_item );
}

const char *
libvlc_renderer_item_icon_uri( const libvlc_renderer_item_t *p_item )
{
    return vlc_renderer_item_icon_uri( (vlc_renderer_item_t *) p_item );
}

int
libvlc_renderer_item_flags( const libvlc_renderer_item_t *p_item )
{
    return vlc_renderer_item_flags( (vlc_renderer_item_t *) p_item );
}

libvlc_renderer_discoverer_t *
libvlc_renderer_discoverer_new( libvlc_instance_t *p_inst,
                                const char *psz_name )
{
    libvlc_renderer_discoverer_t *p_lrd =
        calloc( 1, sizeof(libvlc_renderer_discoverer_t) );

    if( unlikely(p_lrd == NULL) )
        return NULL;

    p_lrd->p_rd = vlc_rd_new( VLC_OBJECT( p_inst->p_libvlc_int ), psz_name );
    if( unlikely(p_lrd->p_rd == NULL) )
        goto error;

    TAB_INIT( p_lrd->i_items, p_lrd->pp_items );

    p_lrd->p_event_manager = libvlc_event_manager_new( p_lrd );
    if( unlikely(p_lrd->p_event_manager == NULL) )
        goto error;

    vlc_event_manager_t *p_rd_ev = vlc_rd_event_manager( p_lrd->p_rd );

    if( vlc_event_attach( p_rd_ev, vlc_RendererDiscoveryItemAdded,
                          renderer_discovery_item_added, p_lrd )
                          != VLC_SUCCESS )
        goto error;
    if( vlc_event_attach( p_rd_ev, vlc_RendererDiscoveryItemRemoved,
                          renderer_discovery_item_removed, p_lrd )
                          != VLC_SUCCESS )
        goto error;

    return p_lrd;

error:
    libvlc_renderer_discoverer_release( p_lrd );
    return NULL;
}

void
libvlc_renderer_discoverer_release( libvlc_renderer_discoverer_t *p_lrd )
{
    if( p_lrd->p_rd != NULL )
        vlc_rd_release( p_lrd->p_rd );

    if( p_lrd->p_event_manager != NULL )
        libvlc_event_manager_release( p_lrd->p_event_manager );

    free( p_lrd );
}

int
libvlc_renderer_discoverer_start( libvlc_renderer_discoverer_t *p_lrd )
{
    return vlc_rd_start( p_lrd->p_rd );
}

void
libvlc_renderer_discoverer_stop( libvlc_renderer_discoverer_t *p_lrd )
{
    vlc_rd_stop( p_lrd->p_rd );

    for( int i = 0; i < p_lrd->i_items; ++i )
        vlc_renderer_item_release( p_lrd->pp_items[i] );
    TAB_CLEAN( p_lrd->i_items, p_lrd->pp_items );
}

libvlc_event_manager_t *
libvlc_renderer_discoverer_event_manager( libvlc_renderer_discoverer_t *p_lrd )
{
    return p_lrd->p_event_manager;
}

void
libvlc_renderer_discoverer_list_release( libvlc_rd_description_t **pp_services,
                                         size_t i_count )
{
    if( i_count > 0 )
    {
        for( size_t i = 0; i < i_count; ++i )
        {
            free( pp_services[i]->psz_name );
            free( pp_services[i]->psz_longname );
        }
        free( *pp_services );
        free( pp_services );
    }
}

ssize_t
libvlc_renderer_discoverer_list_get( libvlc_instance_t *p_inst,
                                     libvlc_rd_description_t ***ppp_services )
{
    assert( p_inst != NULL && ppp_services != NULL );

    /* Fetch all rd names, and longnames */
    char **ppsz_names, **ppsz_longnames;
    int i_ret = vlc_rd_get_names( p_inst->p_libvlc_int, &ppsz_names,
                                  &ppsz_longnames );

    if( i_ret != VLC_SUCCESS )
    {
        *ppp_services = NULL;
        return -1;
    }

    /* Count the number of sd matching our category (i_cat/i_core_cat) */
    size_t i_nb_services = 0;
    char **ppsz_name = ppsz_names;
    for( ; *ppsz_name != NULL; ppsz_name++ )
        i_nb_services++;

    libvlc_rd_description_t **pp_services = NULL,
                                              *p_services = NULL;
    if( i_nb_services > 0 )
    {
        /* Double alloc here, so that the caller iterates through pointers of
         * struct instead of structs. This allows us to modify the struct
         * without breaking the API. */

        pp_services =
            malloc( i_nb_services
                    * sizeof(libvlc_rd_description_t *) );
        p_services =
            malloc( i_nb_services
                    * sizeof(libvlc_rd_description_t) );
        if( pp_services == NULL || p_services == NULL )
        {
            free( pp_services );
            free( p_services );
            pp_services = NULL;
            p_services = NULL;
            i_nb_services = -1;
            /* Even if alloc fails, the next loop must be run in order to free
             * names returned by vlc_sd_GetNames */
        }
    }

    /* Fill output pp_services or free unused name, longname */
    char **ppsz_longname = ppsz_longnames;
    unsigned int i_service_idx = 0;
    libvlc_rd_description_t *p_service = p_services;
    for( ppsz_name = ppsz_names; *ppsz_name != NULL; ppsz_name++, ppsz_longname++ )
    {
        if( pp_services != NULL )
        {
            p_service->psz_name = *ppsz_name;
            p_service->psz_longname = *ppsz_longname;
            pp_services[i_service_idx++] = p_service++;
        }
        else
        {
            free( *ppsz_name );
            free( *ppsz_longname );
        }
    }
    free( ppsz_names );
    free( ppsz_longnames );

    *ppp_services = pp_services;
    return i_nb_services;
}
