/*****************************************************************************
 * flat_media_list.c: libvlc flat media list functions. (extension to
 * media_list.c).
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: flat_media_list.c 21287 2007-08-20 01:28:12Z pdherbemont $
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

//#define DEBUG_FLAT_LIST

#ifdef DEBUG_FLAT_LIST
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

/*
 * Private functions
 */
static void add_media_list( libvlc_media_list_t * p_mlist, libvlc_media_list_t * p_submlist );
static void remove_media_list( libvlc_media_list_t * p_fmlist, libvlc_media_list_t * p_mlist );
static void add_item( libvlc_media_list_t * p_mlist, libvlc_media_descriptor_t * p_md );
static void remove_item( libvlc_media_list_t * p_mlist, libvlc_media_descriptor_t * p_md );
static void subitems_created( const libvlc_event_t * p_event , void * p_user_data);
static void sublist_item_added( const libvlc_event_t * p_event, void * p_user_data );
static void sublist_item_removed( const libvlc_event_t * p_event, void * p_user_data );
static void install_flat_mlist_observer( libvlc_media_list_t * p_mlist );
static void uninstall_flat_mlist_observer( libvlc_media_list_t * p_mlist );

/**************************************************************************
 *       uninstall_media_list_observer (Private)
 **************************************************************************/
static void
uninstall_media_list_observer( libvlc_media_list_t * p_mlist,
                             libvlc_media_list_t * p_submlist )
{
    trace("\n");
    libvlc_event_detach( p_submlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         sublist_item_added, p_mlist, NULL );
    libvlc_event_detach( p_submlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         sublist_item_removed, p_mlist, NULL );
}

/**************************************************************************
 *       install_media_list_observer (Private)
 **************************************************************************/
static void
install_media_list_observer( libvlc_media_list_t * p_mlist,
                             libvlc_media_list_t * p_submlist )
{
    trace("\n");
    libvlc_event_attach( p_submlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         sublist_item_added, p_mlist, NULL );
    libvlc_event_attach( p_submlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         sublist_item_removed, p_mlist, NULL );
}

/**************************************************************************
 *       add_media_list (Private)
 **************************************************************************/
static void
add_media_list( libvlc_media_list_t * p_mlist,
                libvlc_media_list_t * p_submlist )
{
    int count = libvlc_media_list_count( p_submlist, NULL );
    int i;
    trace("\n");

    for( i = 0; i < count; i++ )
    {
        libvlc_media_descriptor_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_submlist, i, NULL );
        add_item( p_mlist, p_md );
    }
    install_media_list_observer( p_mlist, p_submlist );
}

/**************************************************************************
 *       remove_media_list (Private)
 **************************************************************************/
static void
remove_media_list( libvlc_media_list_t * p_mlist,
                   libvlc_media_list_t * p_submlist )
{
    trace("\n");
    int count = libvlc_media_list_count( p_submlist, NULL );
    int i;
    uninstall_media_list_observer( p_mlist, p_submlist );

    for( i = 0; i < count; i++ )
    {
        libvlc_media_descriptor_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_submlist, i, NULL );
        remove_item( p_mlist, p_md );
    }
}


/**************************************************************************
 *       add_item (private)
 **************************************************************************/
static void
add_item( libvlc_media_list_t * p_mlist, libvlc_media_descriptor_t * p_md )
{
    trace( "p_md '%s'\n", p_md->p_input_item->psz_name );

    /* Only add the media descriptor once to our flat list */
    if( libvlc_media_list_index_of_item( p_mlist->p_flat_mlist, p_md, NULL ) < 0 )
    {
        if( p_md->p_subitems )
        {
            add_media_list( p_mlist, p_md->p_subitems );
        }
        else
        {
            libvlc_media_list_lock( p_mlist->p_flat_mlist );
            libvlc_event_attach( p_md->p_event_manager,
                                 libvlc_MediaDescriptorSubItemAdded,
                                 subitems_created, p_mlist, NULL );
            uninstall_flat_mlist_observer( p_mlist );
            libvlc_media_list_add_media_descriptor( p_mlist->p_flat_mlist,
                                                    p_md, NULL );
            install_flat_mlist_observer( p_mlist );
            libvlc_media_list_unlock( p_mlist->p_flat_mlist );
 
        }
    }
}

/**************************************************************************
 *       remove_item (private)
 **************************************************************************/
static void
remove_item( libvlc_media_list_t * p_mlist, libvlc_media_descriptor_t * p_md )
{
    trace( "p_md '%s'\n", p_md->p_input_item->psz_name );

    if( p_md->p_subitems ) /* XXX: Don't access that directly */
        remove_media_list( p_mlist, p_md->p_subitems );
    libvlc_event_detach( p_md->p_event_manager,
                         libvlc_MediaDescriptorSubItemAdded,
                         subitems_created, p_mlist, NULL );

    libvlc_media_list_lock( p_mlist->p_flat_mlist );
    int i = libvlc_media_list_index_of_item( p_mlist->p_flat_mlist, p_md, NULL );
    if( i >= 0 )
    {
        uninstall_flat_mlist_observer( p_mlist );
        libvlc_media_list_remove_index( p_mlist->p_flat_mlist, i, NULL );
        install_flat_mlist_observer( p_mlist );
    }
    libvlc_media_list_unlock( p_mlist->p_flat_mlist );
}

/**************************************************************************
 *       subitems_created (private) (Event Callback)
 **************************************************************************/
static void
subitems_created( const libvlc_event_t * p_event , void * p_user_data)
{
    libvlc_media_list_t * p_mlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->p_obj;

    trace( "parent p_md '%s'\n", p_md->p_input_item->psz_name );

    /* Remove the item and add the playlist */
    libvlc_media_list_lock( p_mlist->p_flat_mlist );
    int i = libvlc_media_list_index_of_item( p_mlist->p_flat_mlist, p_md, NULL );
    if( i >=  0 )
    {
        libvlc_event_detach_lock_state( p_md->p_event_manager,
                         libvlc_MediaDescriptorSubItemAdded,
                         subitems_created, p_mlist, libvlc_Locked, NULL );
        uninstall_flat_mlist_observer( p_mlist );
        libvlc_media_list_remove_index( p_mlist->p_flat_mlist, i, NULL );
        install_flat_mlist_observer( p_mlist );
    }
    libvlc_media_list_unlock( p_mlist->p_flat_mlist );

    trace( "Adding p_md '%s''s media list\n", p_md->p_input_item->psz_name );

    add_media_list( p_mlist, p_md->p_subitems );

    trace( "done\n" );

}

/**************************************************************************
 *       sublist_item_added (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
sublist_item_added( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_t * p_mlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;
    trace( "p_md '%s'\n", p_md->p_input_item->psz_name );

    add_item( p_mlist, p_md );
}

/**************************************************************************
 *       sublist_remove_item (private) (Event Callback)
 **************************************************************************/
static void
sublist_item_removed( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_t * p_mlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_deleted.item;
    trace( "p_md '%s'\n", p_md->p_input_item->psz_name );

    remove_item( p_mlist, p_md );
}

/**************************************************************************
 *       remove_item_in_submlist_rec (private)
 **************************************************************************/
static void
remove_item_in_submlist_rec( libvlc_media_list_t * p_mlist,
                             libvlc_media_list_t * p_submlist,
                             libvlc_media_descriptor_t * p_md )
{
    libvlc_media_descriptor_t * p_md_insub;
    int count = libvlc_media_list_count( p_submlist, NULL );
    int i;
    trace("p_md '%s'\n", p_md->p_input_item->psz_name);

    for( i = 0; i < count; i++ )
    {
        p_md_insub = libvlc_media_list_item_at_index( p_submlist,
                                                      i, NULL );
        if( p_md == p_md_insub )
        {
            libvlc_media_list_lock( p_submlist );
            uninstall_media_list_observer( p_mlist, p_submlist );
            libvlc_media_list_remove_index( p_submlist, i, NULL );
            install_media_list_observer( p_mlist, p_submlist );
            libvlc_media_list_unlock( p_submlist );
        } else if( p_md_insub->p_subitems )
            remove_item_in_submlist_rec( p_mlist, p_md_insub->p_subitems, p_md );
    }
}

/**************************************************************************
 *       flat_mlist_item_removed (private) (Event Callback)
 **************************************************************************/
static void
flat_mlist_item_removed( const libvlc_event_t * p_event, void * p_user_data )
{
    trace("\n");
    /* Remove all occurences of that one in sublist */
    libvlc_media_list_t * p_mlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_deleted.item;
    remove_item( p_mlist, p_md ); /* Just to detach the event */
    remove_item_in_submlist_rec( p_mlist, p_mlist, p_md );
}

/**************************************************************************
 *       flat_mlist_item_added (private) (Event Callback)
 **************************************************************************/
static void
flat_mlist_item_added( const libvlc_event_t * p_event, void * p_user_data )
{
    trace("\n");
    libvlc_media_list_t * p_mlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;

    libvlc_event_attach( p_md->p_event_manager,
                        libvlc_MediaDescriptorSubItemAdded,
                        subitems_created, p_mlist, NULL );

    /* Add in our root */
    uninstall_media_list_observer( p_mlist, p_mlist );
    libvlc_media_list_add_media_descriptor( p_mlist, p_md, NULL );
    install_media_list_observer( p_mlist, p_mlist );
}

/**************************************************************************
 *       install_flat_mlist_observer (Private)
 **************************************************************************/
static void
install_flat_mlist_observer( libvlc_media_list_t * p_mlist )
{
    trace("\n");
    libvlc_event_attach( p_mlist->p_flat_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         flat_mlist_item_added, p_mlist, NULL );
    libvlc_event_attach( p_mlist->p_flat_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         flat_mlist_item_removed, p_mlist, NULL );

}

/**************************************************************************
 *       uninstall_flat_mlist_observer (Private)
 **************************************************************************/
static void
uninstall_flat_mlist_observer( libvlc_media_list_t * p_mlist )
{
    trace("\n");
    libvlc_event_detach( p_mlist->p_flat_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         flat_mlist_item_added, p_mlist, NULL );
    libvlc_event_detach( p_mlist->p_flat_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         flat_mlist_item_removed, p_mlist, NULL );

}

/*
 * libvlc Internal functions
 */
/**************************************************************************
 *       flat_media_list_release (Internal)
 **************************************************************************/
void
libvlc_media_list_flat_media_list_release( libvlc_media_list_t * p_mlist )
{
    if( !p_mlist->p_flat_mlist )
        return;
    uninstall_flat_mlist_observer( p_mlist );
    libvlc_media_list_release( p_mlist->p_flat_mlist );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_flat_media_list( libvlc_media_list_t * p_mlist,
                                   libvlc_exception_t * p_e )
{
    trace("\n");
    libvlc_media_list_lock( p_mlist );
    if( !p_mlist->p_flat_mlist )
    {
        p_mlist->p_flat_mlist = libvlc_media_list_new(
                                            p_mlist->p_libvlc_instance,
                                            p_e );
        add_media_list( p_mlist, p_mlist );
    }
    libvlc_media_list_unlock( p_mlist );
    libvlc_media_list_retain( p_mlist->p_flat_mlist );
    return p_mlist->p_flat_mlist;
}
