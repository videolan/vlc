/*****************************************************************************
 * media_discoverer.c: libvlc new API media discoverer functions
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
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
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_discoverer.h>
#include <vlc/libvlc_events.h>

#include <vlc_services_discovery.h>

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_new_from_input_item()
#include "media_list_internal.h" // libvlc_media_list_internal_add_media()

struct libvlc_media_discoverer_t
{
    libvlc_instance_t *      p_libvlc_instance;
    services_discovery_t *   p_sd;
    libvlc_media_list_t *    p_mlist;
    vlc_dictionary_t         catname_to_submedialist;
    char                     name[];
};

/*
 * Private functions
 */

/**************************************************************************
 *       services_discovery_item_added (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_item_added( services_discovery_t *sd,
                                           input_item_t *parent,
                                           input_item_t *p_item,
                                           const char *psz_cat )
{
    libvlc_media_t * p_md;
    libvlc_media_discoverer_t *p_mdis = sd->owner.sys;
    libvlc_media_list_t * p_mlist = p_mdis->p_mlist;

    p_md = libvlc_media_new_from_input_item( p_mdis->p_libvlc_instance,
                                             p_item );

    if( parent != NULL )
    {
        /* Flatten items list for now. TODO: tree support. */
    }
    else
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
            libvlc_media_list_internal_add_media( p_mdis->p_mlist, p_catmd );
            libvlc_media_list_unlock( p_mdis->p_mlist );

            /* We don't release the mlist cause the dictionary
             * doesn't retain the object. But we release the md. */
            libvlc_media_release( p_catmd );
        }
    }

    libvlc_media_list_lock( p_mlist );
    libvlc_media_list_internal_add_media( p_mlist, p_md );
    libvlc_media_list_unlock( p_mlist );

    libvlc_media_release( p_md );
}

/**************************************************************************
 *       services_discovery_item_removed (Private) (VLC event callback)
 **************************************************************************/

static void services_discovery_item_removed( services_discovery_t *sd,
                                             input_item_t *p_item )
{
    libvlc_media_t * p_md;
    libvlc_media_discoverer_t *p_mdis = sd->owner.sys;

    int i, count = libvlc_media_list_count( p_mdis->p_mlist );
    libvlc_media_list_lock( p_mdis->p_mlist );
    for( i = 0; i < count; i++ )
    {
        p_md = libvlc_media_list_item_at_index( p_mdis->p_mlist, i );
        assert(p_md != NULL);
        if( p_md->p_input_item == p_item )
        {
            libvlc_media_list_internal_remove_index( p_mdis->p_mlist, i );
            libvlc_media_release( p_md );
            break;
        }
        libvlc_media_release( p_md );
    }
    libvlc_media_list_unlock( p_mdis->p_mlist );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 **************************************************************************/
libvlc_media_discoverer_t *
libvlc_media_discoverer_new( libvlc_instance_t * p_inst, const char * psz_name )
{
    /* podcast SD is a hack and only works with custom playlist callbacks. */
    if( !strncasecmp( psz_name, "podcast", 7 ) )
        return NULL;

    libvlc_media_discoverer_t *p_mdis;

    p_mdis = malloc(sizeof(*p_mdis) + strlen(psz_name) + 1);
    if( unlikely(p_mdis == NULL) )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_mdis->p_libvlc_instance = p_inst;
    p_mdis->p_mlist = libvlc_media_list_new();
    p_mdis->p_mlist->b_read_only = true;
    p_mdis->p_sd = NULL;

    vlc_dictionary_init( &p_mdis->catname_to_submedialist, 0 );

    libvlc_retain( p_inst );
    strcpy( p_mdis->name, psz_name );
    return p_mdis;
}

static const struct services_discovery_callbacks sd_cbs = {
    .item_added = services_discovery_item_added,
    .item_removed = services_discovery_item_removed,
};

/**************************************************************************
 *       start (Public)
 **************************************************************************/
LIBVLC_API int
libvlc_media_discoverer_start( libvlc_media_discoverer_t * p_mdis )
{
    struct services_discovery_owner_t owner = {
        &sd_cbs,
        p_mdis,
    };

    /* Here we go */
    p_mdis->p_sd = vlc_sd_Create( (vlc_object_t *)p_mdis->p_libvlc_instance->p_libvlc_int,
                                  p_mdis->name, &owner );
    if( p_mdis->p_sd == NULL )
    {
        libvlc_printerr( "%s: no such discovery module found", p_mdis->name );
        return -1;
    }

    return 0;
}

/**************************************************************************
 *       stop (Public)
 **************************************************************************/
LIBVLC_API void
libvlc_media_discoverer_stop( libvlc_media_discoverer_t * p_mdis )
{
    libvlc_media_list_t * p_mlist = p_mdis->p_mlist;
    libvlc_media_list_lock( p_mlist );
    libvlc_media_list_internal_end_reached( p_mlist );
    libvlc_media_list_unlock( p_mlist );

    vlc_sd_Destroy( p_mdis->p_sd );
    p_mdis->p_sd = NULL;
}

/**************************************************************************
 * release (Public)
 **************************************************************************/
static void
MediaListDictValueRelease( void* mlist, void* obj )
{
    libvlc_media_list_release( mlist );
    (void)obj;
}

void
libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis )
{
    if( p_mdis->p_sd != NULL )
        libvlc_media_discoverer_stop( p_mdis );

    libvlc_media_list_release( p_mdis->p_mlist );

    vlc_dictionary_clear( &p_mdis->catname_to_submedialist,
        MediaListDictValueRelease, NULL );

    libvlc_release( p_mdis->p_libvlc_instance );

    free( p_mdis );
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
 * running (Public)
 **************************************************************************/
bool libvlc_media_discoverer_is_running(libvlc_media_discoverer_t * p_mdis)
{
    return p_mdis->p_sd != NULL;
}

void
libvlc_media_discoverer_list_release( libvlc_media_discoverer_description_t **pp_services,
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

size_t
libvlc_media_discoverer_list_get( libvlc_instance_t *p_inst,
                                  libvlc_media_discoverer_category_t i_cat,
                                  libvlc_media_discoverer_description_t ***ppp_services )
{
    assert( p_inst != NULL && ppp_services != NULL );

    int i_core_cat;
    switch( i_cat )
    {
    case libvlc_media_discoverer_devices:
        i_core_cat = SD_CAT_DEVICES;
        break;
    case libvlc_media_discoverer_lan:
        i_core_cat = SD_CAT_LAN;
        break;
    case libvlc_media_discoverer_podcasts:
        i_core_cat = SD_CAT_INTERNET;
        break;
    case libvlc_media_discoverer_localdirs:
        i_core_cat = SD_CAT_MYCOMPUTER;
        break;
    default:
        vlc_assert_unreachable();
        *ppp_services = NULL;
        return 0;
    }

    /* Fetch all sd names, longnames and categories */
    char **ppsz_names, **ppsz_longnames;
    int *p_categories;
    ppsz_names = vlc_sd_GetNames( p_inst->p_libvlc_int, &ppsz_longnames,
                                  &p_categories );

    if( ppsz_names == NULL )
    {
        *ppp_services = NULL;
        return 0;
    }

    /* Count the number of sd matching our category (i_cat/i_core_cat) */
    size_t i_nb_services = 0;
    char **ppsz_name = ppsz_names;
    int *p_category = p_categories;
    for( ; *ppsz_name != NULL; ppsz_name++, p_category++ )
    {
        if( *p_category == i_core_cat )
            i_nb_services++;
    }

    libvlc_media_discoverer_description_t **pp_services = NULL, *p_services = NULL;
    if( i_nb_services > 0 )
    {
        /* Double alloc here, so that the caller iterates through pointers of
         * struct instead of structs. This allows us to modify the struct
         * without breaking the API. */

        pp_services = malloc( i_nb_services
                              * sizeof(libvlc_media_discoverer_description_t *) );
        p_services = malloc( i_nb_services
                             * sizeof(libvlc_media_discoverer_description_t) );
        if( pp_services == NULL || p_services == NULL )
        {
            free( pp_services );
            free( p_services );
            pp_services = NULL;
            p_services = NULL;
            i_nb_services = 0;
            /* Even if alloc fails, the next loop must be run in order to free
             * names returned by vlc_sd_GetNames */
        }
    }

    /* Fill output pp_services or free unused name, longname */
    char **ppsz_longname = ppsz_longnames;
    ppsz_name = ppsz_names;
    p_category = p_categories;
    unsigned int i_service_idx = 0;
    libvlc_media_discoverer_description_t *p_service = p_services;
    for( ; *ppsz_name != NULL; ppsz_name++, ppsz_longname++, p_category++ )
    {
        if( pp_services != NULL && *p_category == i_core_cat )
        {
            p_service->psz_name = *ppsz_name;
            p_service->psz_longname = *ppsz_longname;
            p_service->i_cat = i_cat;
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
    free( p_categories );

    *ppp_services = pp_services;
    return i_nb_services;
}
