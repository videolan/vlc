/*****************************************************************************
 * flat_media_list.c: libvlc flat media list functions. (extension to
 * media_list.c).
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_list_view.h>
#include <vlc/libvlc_events.h>

#include "libvlc_internal.h"  // Abuse, could and should be removed

#include "media_internal.h"  // Abuse, could and should be removed
#include "media_list_internal.h"  // Abuse, could and should be removed
#include "media_list_view_internal.h"

//#define DEBUG_FLAT_LIST

#ifdef DEBUG_FLAT_LIST
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

/*
 * Private functions
 */
static void
media_list_item_added( const libvlc_event_t * p_event, void * p_user_data );
static void
media_list_item_removed( const libvlc_event_t * p_event, void * p_user_data );
static void
media_list_subitem_added( const libvlc_event_t * p_event, void * p_user_data );

static void
install_md_listener( libvlc_media_list_view_t * p_mlv,
                     libvlc_media_t * p_md)
{
    libvlc_media_list_t * p_mlist;
    if((p_mlist = libvlc_media_subitems( p_md )))
    {
        libvlc_media_list_lock( p_mlist );
        int i, count = libvlc_media_list_count( p_mlist );
        for( i = 0; i < count; i++)
        {
            libvlc_event_t added_event;
            libvlc_media_t * p_submd;
            p_submd = libvlc_media_list_item_at_index( p_mlist, i, NULL );

            /* Install our listeners */
            install_md_listener( p_mlv, p_submd );

            /* For each item, send a notification to the mlv subclasses */
            added_event.u.media_list_item_added.item = p_submd;
            added_event.u.media_list_item_added.index = 0;
            if( p_mlv->pf_ml_item_added ) p_mlv->pf_ml_item_added( &added_event, p_mlv );
            libvlc_media_release( p_submd );
        }
        libvlc_event_attach( p_mlist->p_event_manager,
                             libvlc_MediaListItemAdded,
                             media_list_item_added, p_mlv );
        libvlc_event_attach( p_mlist->p_event_manager,
                             libvlc_MediaListItemDeleted,
                             media_list_item_removed, p_mlv );
        libvlc_media_list_unlock( p_mlist );
        libvlc_media_list_release( p_mlist );
    }
    else
    {
        /* No mlist, wait for a subitem added event */
        libvlc_event_attach( p_md->p_event_manager,
                            libvlc_MediaSubItemAdded,
                            media_list_subitem_added, p_mlv );
    }
}

static void
uninstall_md_listener( libvlc_media_list_view_t * p_mlv,
                       libvlc_media_t * p_md)
{
    libvlc_media_list_t * p_mlist;
    libvlc_event_detach( p_md->p_event_manager,
                         libvlc_MediaSubItemAdded,
                         media_list_subitem_added, p_mlv );
    if((p_mlist = libvlc_media_subitems( p_md )))
    {
        libvlc_media_list_lock( p_mlist );
        libvlc_event_detach( p_mlist->p_event_manager,
                             libvlc_MediaListItemAdded,
                             media_list_item_added, p_mlv );
        libvlc_event_detach( p_mlist->p_event_manager,
                             libvlc_MediaListItemDeleted,
                             media_list_item_removed, p_mlv );

        int i, count = libvlc_media_list_count( p_mlist );
        for( i = 0; i < count; i++)
        {
            libvlc_media_t * p_submd;
            p_submd = libvlc_media_list_item_at_index( p_mlist,i, NULL );
            uninstall_md_listener( p_mlv, p_submd );
            libvlc_media_release( p_submd );
        }
        libvlc_media_list_unlock( p_mlist );
        libvlc_media_list_release( p_mlist );
    }
}

static void
media_list_item_added( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_view_t * p_mlv = p_user_data;
    libvlc_media_t * p_md = p_event->u.media_list_item_added.item;
    install_md_listener( p_mlv, p_md );
    if( p_mlv->pf_ml_item_added ) p_mlv->pf_ml_item_added( p_event, p_mlv );
}

static void
media_list_item_removed( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_view_t * p_mlv = p_user_data;
    libvlc_media_t * p_md = p_event->u.media_list_item_added.item;
    uninstall_md_listener( p_mlv, p_md );
    if( p_mlv->pf_ml_item_removed ) p_mlv->pf_ml_item_removed( p_event, p_mlv );
}

static void
media_list_subitem_added( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_t * p_mlist;
    libvlc_event_t added_event;
    libvlc_media_list_view_t * p_mlv = p_user_data;
    libvlc_media_t * p_submd = p_event->u.media_subitem_added.new_child;
    libvlc_media_t * p_md = p_event->p_obj;

    if((p_mlist = libvlc_media_subitems( p_md )))
    {
        /* We have a mlist to which we're going to listen to events
         * thus, no need to wait for SubItemAdded events */
        libvlc_event_detach( p_md->p_event_manager,
                             libvlc_MediaSubItemAdded,
                             media_list_subitem_added, p_mlv );
        libvlc_media_list_lock( p_mlist );

        libvlc_event_attach( p_mlist->p_event_manager,
                             libvlc_MediaListItemAdded,
                             media_list_item_added, p_mlv );
        libvlc_event_attach( p_mlist->p_event_manager,
                             libvlc_MediaListItemDeleted,
                             media_list_item_removed, p_mlv );
        libvlc_media_list_unlock( p_mlist );
        libvlc_media_list_release( p_mlist );
    }

    install_md_listener( p_mlv, p_submd );

    added_event.u.media_list_item_added.item = p_submd;
    added_event.u.media_list_item_added.index = 0;
    if( p_mlv->pf_ml_item_added ) p_mlv->pf_ml_item_added( &added_event, p_mlv );
}

/*
 * LibVLC Internal functions
 */
/**************************************************************************
 *       libvlc_media_list_view_set_ml_notification_callback (Internal)
 * The mlist lock should be held when entered
 **************************************************************************/
void
libvlc_media_list_view_set_ml_notification_callback(
                libvlc_media_list_view_t * p_mlv,
                void (*item_added)(const libvlc_event_t *, libvlc_media_list_view_t *),
                void (*item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *) )
{
    p_mlv->pf_ml_item_added = item_added;
    p_mlv->pf_ml_item_removed = item_removed;
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         media_list_item_added, p_mlv );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         media_list_item_removed, p_mlv );
    int i, count = libvlc_media_list_count( p_mlv->p_mlist );
    for( i = 0; i < count; i++)
    {
        libvlc_media_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, i, NULL );
        install_md_listener( p_mlv, p_md );
        libvlc_media_release( p_md );
    }
}

/**************************************************************************
 *       libvlc_media_list_view_notify_deletion (Internal)
 **************************************************************************/
void
libvlc_media_list_view_will_delete_item(
                libvlc_media_list_view_t * p_mlv,
                libvlc_media_t * p_item,
                int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListViewWillDeleteItem;
    event.u.media_list_view_will_delete_item.item = p_item;
    event.u.media_list_view_will_delete_item.index = index;

    /* Send the event */
    libvlc_event_send( p_mlv->p_event_manager, &event );
}

/**************************************************************************
 *       libvlc_media_list_view_item_deleted (Internal)
 **************************************************************************/
void
libvlc_media_list_view_item_deleted(
                libvlc_media_list_view_t * p_mlv,
                libvlc_media_t * p_item,
                int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListViewItemDeleted;
    event.u.media_list_view_item_deleted.item = p_item;
    event.u.media_list_view_item_deleted.index = index;

    /* Send the event */
    libvlc_event_send( p_mlv->p_event_manager, &event );
}

/**************************************************************************
 *       libvlc_media_list_view_will_add_item (Internal)
 **************************************************************************/
void
libvlc_media_list_view_will_add_item(
                libvlc_media_list_view_t * p_mlv,
                libvlc_media_t * p_item,
                int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListViewWillAddItem;
    event.u.media_list_view_will_add_item.item = p_item;
    event.u.media_list_view_will_add_item.index = index;

    /* Send the event */
    libvlc_event_send( p_mlv->p_event_manager, &event );
}

/**************************************************************************
 *       libvlc_media_list_view_item_added (Internal)
 **************************************************************************/
void
libvlc_media_list_view_item_added(
                libvlc_media_list_view_t * p_mlv,
                libvlc_media_t * p_item,
                int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListViewItemAdded;
    event.u.media_list_view_item_added.item = p_item;
    event.u.media_list_view_item_added.index = index;

    /* Send the event */
    libvlc_event_send( p_mlv->p_event_manager, &event );
}

/**************************************************************************
 *       libvlc_media_list_view_new (Internal)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_view_new( libvlc_media_list_t * p_mlist,
                            libvlc_media_list_view_count_func_t pf_count,
                            libvlc_media_list_view_item_at_index_func_t pf_item_at_index,
                            libvlc_media_list_view_children_at_index_func_t pf_children_at_index,
                            libvlc_media_list_view_constructor_func_t pf_constructor,
                            libvlc_media_list_view_release_func_t pf_release,
                            void * this_view_data )
{
    libvlc_media_list_view_t * p_mlv;
    p_mlv = calloc( 1, sizeof(libvlc_media_list_view_t) );
    if( unlikely(p_mlv == NULL) )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_mlv->p_libvlc_instance = p_mlist->p_libvlc_instance;
    p_mlv->p_event_manager = libvlc_event_manager_new( p_mlist,
                                    p_mlv->p_libvlc_instance );
    if( unlikely(p_mlv->p_event_manager == NULL) )
    {
        free(p_mlv);
        return NULL;
    }

    libvlc_event_manager_register_event_type( p_mlv->p_event_manager,
            libvlc_MediaListViewItemAdded );
    libvlc_event_manager_register_event_type( p_mlv->p_event_manager,
            libvlc_MediaListViewWillAddItem );
    libvlc_event_manager_register_event_type( p_mlv->p_event_manager,
            libvlc_MediaListViewItemDeleted );
    libvlc_event_manager_register_event_type( p_mlv->p_event_manager,
            libvlc_MediaListViewWillDeleteItem );

    libvlc_media_list_retain( p_mlist );
    p_mlv->p_mlist = p_mlist;

    p_mlv->pf_count             = pf_count;
    p_mlv->pf_item_at_index     = pf_item_at_index;
    p_mlv->pf_children_at_index = pf_children_at_index;
    p_mlv->pf_constructor       = pf_constructor;
    p_mlv->pf_release           = pf_release;

    p_mlv->p_this_view_data = this_view_data;

    vlc_mutex_init( &p_mlv->object_lock );
    p_mlv->i_refcount = 1;

    return p_mlv;
}


/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_view_retain (Public)
 **************************************************************************/
void
libvlc_media_list_view_retain( libvlc_media_list_view_t * p_mlv )
{
    vlc_mutex_lock( &p_mlv->object_lock );
    p_mlv->i_refcount++;
    vlc_mutex_unlock( &p_mlv->object_lock );
}

/**************************************************************************
 *       libvlc_media_list_view_release (Public)
 **************************************************************************/
void
libvlc_media_list_view_release( libvlc_media_list_view_t * p_mlv )
{
    vlc_mutex_lock( &p_mlv->object_lock );
    p_mlv->i_refcount--;
    if( p_mlv->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mlv->object_lock );
        return;
    }
    vlc_mutex_unlock( &p_mlv->object_lock );

    /* Refcount null, time to free */
    libvlc_media_list_lock( p_mlv->p_mlist );

    if( p_mlv->pf_ml_item_added )
    {
        libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                            libvlc_MediaListItemAdded,
                            media_list_item_added, p_mlv );
    }
    if( p_mlv->pf_ml_item_removed )
    {
        libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                            libvlc_MediaListItemDeleted,
                            media_list_item_removed, p_mlv );
    }
    int i, count = libvlc_media_list_count( p_mlv->p_mlist );
    for( i = 0; i < count; i++)
    {
        libvlc_media_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, i, NULL );
        uninstall_md_listener( p_mlv, p_md );
        libvlc_media_release( p_md );
    }
    libvlc_media_list_unlock( p_mlv->p_mlist );

    libvlc_event_manager_release( p_mlv->p_event_manager );

    if( p_mlv->pf_release ) p_mlv->pf_release( p_mlv );
    libvlc_media_list_release( p_mlv->p_mlist );
    vlc_mutex_destroy( &p_mlv->object_lock );
}

/**************************************************************************
 *       libvlc_media_list_view_event_manager (Public)
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_view_event_manager( libvlc_media_list_view_t * p_mlv )
{
    libvlc_event_manager_t * p_em;
    vlc_mutex_lock( &p_mlv->object_lock );
    p_em = p_mlv->p_event_manager;
    vlc_mutex_unlock( &p_mlv->object_lock );
    return p_em;
}

/**************************************************************************
 *       libvlc_media_list_view_parent_media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_view_parent_media_list( libvlc_media_list_view_t * p_mlv,
                                         libvlc_exception_t * p_e)
{
    (void)p_e;
    libvlc_media_list_t * p_mlist;
    vlc_mutex_lock( &p_mlv->object_lock );
    p_mlist = p_mlv->p_mlist;
    libvlc_media_list_retain( p_mlv->p_mlist );
    vlc_mutex_unlock( &p_mlv->object_lock );
    return p_mlist;
}

/**************************************************************************
 *       libvlc_media_list_view_children_for_item (Public)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_view_children_for_item( libvlc_media_list_view_t * p_mlv,
                                          libvlc_media_t * p_md,
                                          libvlc_exception_t * p_e)
{
    (void)p_e;
    libvlc_media_list_t * p_mlist;
    libvlc_media_list_view_t * ret;

    p_mlist = libvlc_media_subitems(p_md);
    if(!p_mlist) return NULL;

    ret = p_mlv->pf_constructor( p_mlist, p_e );
    libvlc_media_list_release( p_mlist );

    return ret;
}

/* Limited to four args, because it should be enough */

#define AN_SELECT( collapser, dec1, dec2, dec3, dec4, p, ...) p
#define ARGS(...) AN_SELECT( collapser, ##__VA_ARGS__, \
                                              (p_mlv, arg1, arg2, arg3, arg4, p_e), \
                                              (p_mlv, arg1, arg2, arg3, p_e), \
                                              (p_mlv, arg1, arg2, p_e), \
                                              (p_mlv, arg1, p_e), (p_mlv, p_e) )

#define MEDIA_LIST_VIEW_FUNCTION( name, ret_type, default_ret_value, /* Params */ ... ) \
    ret_type \
    libvlc_media_list_view_##name( libvlc_media_list_view_t * p_mlv, \
                                  ##__VA_ARGS__, \
                                  libvlc_exception_t * p_e ) \
    { \
        if( p_mlv->pf_##name ) \
            return p_mlv->pf_##name ARGS(__VA_ARGS__) ; \
        libvlc_exception_raise( p_e ); \
        libvlc_printerr( "No '" #name "' method in this media_list_view" ); \
        return default_ret_value;\
    }

#define MEDIA_LIST_VIEW_FUNCTION_VOID_RET( name, /* Params */ ... ) \
    void \
    libvlc_media_list_view_##name( libvlc_media_list_view_t * p_mlv, \
                                  ##__VA_ARGS__, \
                                  libvlc_exception_t * p_e ) \
    { \
        if( p_mlv->pf_##name ) \
        { \
            p_mlv->pf_##name ARGS(__VA_ARGS__) ; \
            return; \
        } \
        libvlc_exception_raise( p_e ); \
        libvlc_printerr( "No '" #name "' method in this media_list_view" ); \
    }


MEDIA_LIST_VIEW_FUNCTION( count, int, 0 )
MEDIA_LIST_VIEW_FUNCTION( item_at_index, libvlc_media_t *, NULL, int arg1 )
MEDIA_LIST_VIEW_FUNCTION( children_at_index, libvlc_media_list_view_t *, NULL, int arg1 )

