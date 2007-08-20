/*****************************************************************************
 * media_list.c: libvlc tree functions
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

/*
 * Private libvlc functions
 */

/**************************************************************************
 *       notify_subtree_addition (private)
 *
 * Do the appropriate action when a subtree is added.
 **************************************************************************/
static void
notify_subtree_addition( libvlc_tree_t * p_tree,
                         libvlc_tree_t * p_subtree,
                         int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_TreeSubtreeAdded;
    event.u.tree_subtree_added.subtree = p_subtree;
    event.u.tree_subtree_added.index = index;

    /* Send the event */
    libvlc_event_send( p_tree->p_event_manager, &event );
}

/**************************************************************************
 *       notify_subtree_deletion (private)
 *
 * Do the appropriate action when a subtree is deleted.
 **************************************************************************/
static void
notify_subtree_deletion( libvlc_tree_t * p_tree,
                         libvlc_tree_t * p_subtree,
                         int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_TreeSubtreeDeleted;
    event.u.tree_subtree_deleted.subtree = p_subtree;
    event.u.tree_subtree_deleted.index = index;

    /* Send the event */
    libvlc_event_send( p_tree->p_event_manager, &event );
}

#ifdef NOT_USED
/**************************************************************************
 *       notify_tree_item_value_changed (private)
 *
 * Do the appropriate action when a tree's item changes.
 **************************************************************************/
static void
notify_tree_item_value_changed( libvlc_tree_t * p_tree,
                                libvlc_tree_t * p_subtree,
                                int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_TreeItemValueChanged;
    event.u.tree_item_value_changed.new_value = p_tree->p_item;

    /* Send the event */
    libvlc_event_send( p_tree->p_event_manager, &event );
}
#endif

/**************************************************************************
 *       new (Private)
 **************************************************************************/
static libvlc_tree_t *
libvlc_tree_new( libvlc_retain_function item_retain,
                 libvlc_retain_function item_release,
                 void * item,
                 libvlc_exception_t * p_e )
{
	(void)p_e;
    libvlc_tree_t * p_tree;

	p_tree = malloc(sizeof(libvlc_tree_t));

	if( !p_tree )
		return NULL;

    p_tree->i_refcount = 1;
    p_tree->p_item = item;
    ARRAY_INIT( p_tree->subtrees );
    
    p_tree->p_event_manager = libvlc_event_manager_new( p_tree, NULL, p_e );
    libvlc_event_manager_register_event_type( p_tree->p_event_manager,
            libvlc_TreeSubtreeAdded, p_e );
    libvlc_event_manager_register_event_type( p_tree->p_event_manager,
            libvlc_TreeSubtreeDeleted, p_e );
    libvlc_event_manager_register_event_type( p_tree->p_event_manager,
            libvlc_TreeItemValueChanged, p_e );

	return p_tree;
}

/**************************************************************************
 *        item (Private)
 **************************************************************************/
static void *
libvlc_tree_item( libvlc_tree_t * p_tree,
                  libvlc_exception_t * p_e )
{
    if( p_tree->pf_item_retain ) p_tree->pf_item_retain( p_tree->p_item );
    return p_tree->p_item;
}


/*
 * Public libvlc functions
 */


/**************************************************************************
 *       new_with_media_list (Public)
 **************************************************************************/
libvlc_tree_t *
libvlc_tree_new_with_media_list_as_item( libvlc_media_list_t * p_mlist,
                                         libvlc_exception_t * p_e )
{
	(void)p_e;
    libvlc_tree_t * p_tree = libvlc_tree_new(
        (libvlc_retain_function)libvlc_media_list_retain,
        (libvlc_release_function)libvlc_media_list_release,
        p_mlist,
        p_e );
    
	return p_tree;
}

/**************************************************************************
 *       new_with_string (Public)
 **************************************************************************/
libvlc_tree_t *
libvlc_tree_new_with_string_as_item( const char * psz,
                                     libvlc_exception_t * p_e )
{
	(void)p_e;
    libvlc_tree_t * p_tree = libvlc_tree_new( NULL,
                                    (libvlc_release_function)free,
                                    psz ? strdup( psz ): NULL,
                                    p_e );
    
	return p_tree;
}

/**************************************************************************
 *       release (Public)
 **************************************************************************/
void libvlc_tree_release( libvlc_tree_t * p_tree )
{
    libvlc_tree_t * p_subtree;

    p_tree->i_refcount--;

    if( p_tree->i_refcount > 0 )
        return;

    if( p_tree->pf_item_release && p_tree->p_item )
        p_tree->pf_item_release( p_tree->p_item );

    FOREACH_ARRAY( p_subtree, p_tree->subtrees )
        libvlc_tree_release( p_subtree );
    FOREACH_END()

	free( p_tree );
}

/**************************************************************************
 *       retain (Public)
 **************************************************************************/
void libvlc_tree_retain( libvlc_tree_t * p_tree )
{
	p_tree->i_refcount++;
}

/**************************************************************************
 *        item_as_string (Public)
 **************************************************************************/
char *
libvlc_tree_item_as_string( libvlc_tree_t * p_tree,
                            libvlc_exception_t * p_e )
{
	(void)p_e;
    return p_tree->p_item ? strdup( p_tree->p_item ) : NULL;
}

/**************************************************************************
 *        item_as_media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_tree_item_as_media_list( libvlc_tree_t * p_tree,
                                libvlc_exception_t * p_e )
{
    /* Automatically retained */
    return libvlc_tree_item( p_tree, p_e );
}

/**************************************************************************
 *        count (Public)
 **************************************************************************/
int
libvlc_tree_subtree_count( libvlc_tree_t * p_tree, libvlc_exception_t * p_e )
{
	(void)p_e;
    return p_tree->subtrees.i_size;
}

/**************************************************************************
 *        subtree_at_index (Public)
 *
 * Note: The subtree won't be retained
 **************************************************************************/
libvlc_tree_t *
libvlc_tree_subtree_at_index( libvlc_tree_t * p_tree,
                              int index,
                              libvlc_exception_t * p_e )
{
	(void)p_e;
    libvlc_tree_t * p_subtree;

    p_subtree = ARRAY_VAL( p_tree->subtrees, index );
    libvlc_tree_retain( p_subtree );

    return p_subtree;
}

/**************************************************************************
 *        insert_subtree_at_index (Public)
 *
 * Note: The subtree won't be retained
 **************************************************************************/
void
libvlc_tree_insert_subtree_at_index( libvlc_tree_t * p_tree,
                                     libvlc_tree_t * p_subtree,
                                     int index,
                                     libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_tree_retain( p_tree );

    ARRAY_INSERT( p_tree->subtrees, p_subtree, index);
    notify_subtree_addition( p_tree, p_subtree, index );
}

/**************************************************************************
 *        remove_subtree_at_index (Public)
 **************************************************************************/
void
libvlc_tree_remove_subtree_at_index( libvlc_tree_t * p_tree,
                                     int index,
                                     libvlc_exception_t * p_e )
{
	(void)p_e;
    libvlc_tree_t * p_subtree;

    p_subtree = ARRAY_VAL( p_tree->subtrees, index );

    ARRAY_REMOVE( p_tree->subtrees, index );
    notify_subtree_deletion( p_tree, p_subtree, index );

    libvlc_tree_release( p_subtree );
}