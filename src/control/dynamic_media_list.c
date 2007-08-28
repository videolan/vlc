/*****************************************************************************
 * dynamic_media_list.c: libvlc new API media list functions
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
#include "vlc_arrays.h"

static const char * libvlc_default_dynamic_media_list_tag_key = "VLCPlaylist";

/*
 * Private functions
 */

/**************************************************************************
 *       own_media_list_item_added (private) (Event Callback)
 **************************************************************************/
static void
own_media_list_item_added( const libvlc_event_t * p_event, void * p_user_data )
{
    /* Add our tag */
    libvlc_dynamic_media_list_t * p_dmlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;

    if( !p_dmlist->tag || !p_dmlist->psz_tag_key )
        return;

    libvlc_media_descriptor_add_tag( p_md, p_dmlist->tag,
                p_dmlist->psz_tag_key, NULL );
}

/**************************************************************************
 *       own_media_list_item_removed (private) (Event Callback)
 **************************************************************************/
static void
own_media_list_item_deleted( const libvlc_event_t * p_event, void * p_user_data )
{
    /* Remove our tag */
    libvlc_dynamic_media_list_t * p_dmlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;

    if( !p_dmlist->tag || !p_dmlist->psz_tag_key )
        return;

    libvlc_media_descriptor_remove_tag( p_md, p_dmlist->tag,
                p_dmlist->psz_tag_key, NULL );
}


/**************************************************************************
 *       dynamic_list_propose_item (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
dynamic_list_propose_item( const libvlc_event_t * p_event, void * p_user_data )
{
    /* Check if the item matches our tag query */
    libvlc_dynamic_media_list_t * p_dmlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;

    //libvlc_media_descriptor_lock( p_md );
    if( libvlc_tag_query_match( p_dmlist->p_query, p_md, NULL ) )
    {
        int i = libvlc_media_list_index_of_item( p_dmlist->p_mlist, p_md, NULL );
        if( i >= 0 )
        {
            /* We already have it */
            libvlc_media_list_unlock( p_dmlist->p_mlist );
            return;
        }
        
        libvlc_media_list_lock( p_dmlist->p_mlist );
        libvlc_media_list_add_media_descriptor( p_dmlist->p_mlist, p_md, NULL );
        libvlc_media_list_unlock( p_dmlist->p_mlist );
    }
    //libvlc_media_descriptor_unlock( p_md );
}

/**************************************************************************
 *       dynamic_list_remove_item (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
dynamic_list_remove_item( const libvlc_event_t * p_event, void * p_user_data )
{
    /* Remove the item here too if we have it */
    libvlc_dynamic_media_list_t * p_dmlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_deleted.item;

    //libvlc_media_descriptor_lock( p_md );
    if( libvlc_tag_query_match( p_dmlist->p_query, p_md, NULL ) )
    {
        int i;
        libvlc_media_list_lock( p_dmlist->p_mlist );        
        i = libvlc_media_list_index_of_item( p_dmlist->p_mlist, p_md, NULL );
        if ( i < 0 )
        {
            /* We've missed one item addition, that could happen especially
             * if we add item in a threaded maner, so we just ignore */
            libvlc_media_list_unlock( p_dmlist->p_mlist );
            //libvlc_media_descriptor_unlock( p_md );           
            return;
        }
        libvlc_media_list_remove_index( p_dmlist->p_mlist, i, NULL );
        libvlc_media_list_unlock( p_dmlist->p_mlist );
    }
    //libvlc_media_descriptor_unlock( p_md );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 **************************************************************************/
libvlc_dynamic_media_list_t *
libvlc_dynamic_media_list_new(
                libvlc_media_list_t * p_mlist,
                libvlc_tag_query_t * p_query,
                libvlc_tag_t tag,
                libvlc_exception_t * p_e )
{
    libvlc_dynamic_media_list_t * p_dmlist;
    libvlc_event_manager_t * p_em;
    int count, i;

    (void)p_e;

    p_dmlist = malloc(sizeof(libvlc_dynamic_media_list_t));

    if( !p_mlist )
        return NULL;

    p_dmlist->i_refcount = 1;
    p_dmlist->p_libvlc_instance = p_mlist->p_libvlc_instance;
    p_dmlist->tag = strdup( tag );
    p_dmlist->psz_tag_key = strdup( libvlc_default_dynamic_media_list_tag_key );

    p_dmlist->p_mlist = libvlc_media_list_new( p_mlist->p_libvlc_instance, p_e );
    if( !p_dmlist->p_mlist )
    {
        if( !libvlc_exception_raised( p_e ) )
            libvlc_exception_raise( p_e, "Can't get the new media_list" );
        return NULL;
    }

    /* We have a query */
    libvlc_tag_query_retain( p_query );
    p_dmlist->p_query = p_query;

    /* We have a media provider */
    libvlc_media_list_retain( p_mlist );
    p_dmlist->p_media_provider = p_mlist;

    libvlc_media_list_lock( p_mlist );        
    
    count = libvlc_media_list_count( p_mlist, p_e );

    /* This should be running in a thread, a good plan to achieve that
     * move all the dynamic code to libvlc_tag_query. */
    for( i = 0; i < count; i++ )
    {
        libvlc_media_descriptor_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_mlist, i, p_e );
        if( libvlc_tag_query_match( p_query, p_md, NULL ) )
            libvlc_media_list_add_media_descriptor( p_dmlist->p_mlist, p_md, p_e );
    }

    /* And we will listen to its event, so we can update p_dmlist->p_mlist
     * accordingly */
    p_em = libvlc_media_list_event_manager( p_mlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemAdded,
                         dynamic_list_propose_item, p_dmlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemDeleted,
                         dynamic_list_remove_item, p_dmlist, p_e );

    libvlc_media_list_unlock( p_mlist );        

    /* Make sure item added/removed will gain/loose our mark */
    p_em = libvlc_media_list_event_manager( p_dmlist->p_mlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemAdded,
                         own_media_list_item_added, p_dmlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemDeleted,
                         own_media_list_item_deleted, p_dmlist, p_e );


    return p_dmlist;
}

/**************************************************************************
 *       release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_dynamic_media_list_release( libvlc_dynamic_media_list_t * p_dmlist )
{
    p_dmlist->i_refcount--;
    if( p_dmlist->i_refcount > 0 )
        return;

    free( p_dmlist->tag );
    free( p_dmlist->psz_tag_key );

    /* Refcount null, time to free */
    if( p_dmlist->p_media_provider )
        libvlc_media_list_release( p_dmlist->p_media_provider );

    if( p_dmlist->p_query )
        libvlc_tag_query_release( p_dmlist->p_query );
 
    free( p_dmlist );
}

/**************************************************************************
 *       retain (Public)
 **************************************************************************/
void libvlc_dynamic_media_list_retain( libvlc_dynamic_media_list_t * p_dmlist )
{
    p_dmlist->i_refcount++;
}


/**************************************************************************
 *       media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_dynamic_media_list_media_list(
                                libvlc_dynamic_media_list_t * p_dmlist,
                                libvlc_exception_t * p_e )
{
    libvlc_media_list_retain( p_dmlist->p_mlist );
    return p_dmlist->p_mlist;
}
