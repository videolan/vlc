/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
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


/*
 * Private functions
 */

static int handle_event( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval,
                         void *p_data )
{
    struct libvlc_callback_entry_t *entry = p_data; /* FIXME: we need some locking here */
    libvlc_event_t event;
    event.type = entry->i_event_type;
    switch ( event.type )
    {
        case VOLUME_CHANGED:
            event.value_type = BOOLEAN_EVENT;
            break;
        case INPUT_POSITION_CHANGED:
            break;
        default:
            break;
    }
    event.old_value = oldval;
    event.new_value = newval;

    /* Call the client entry */
    entry->f_callback( entry->p_instance, &event, entry->p_user_data );

    return VLC_SUCCESS;
}

static inline void add_callback_entry( struct libvlc_callback_entry_t *entry,
                                       struct libvlc_callback_entry_list_t **list )
{
    struct libvlc_callback_entry_list_t *new_listitem;

    /* malloc/free strategy:
     *  - alloc-ded in add_callback_entry
     *  - free-ed by libvlc_event_remove_callback
     *  - free-ed in libvlc_destroy threw libvlc_event_remove_callback
     *    when entry is destroyed
     */
    new_listitem = malloc( sizeof( struct libvlc_callback_entry_list_t ) );
    new_listitem->elmt = entry;
    new_listitem->next = *list;
    new_listitem->prev = NULL;

    if(*list)
        (*list)->prev = new_listitem;

    *list = new_listitem;
}

/*
 * Public libvlc functions
 */

void libvlc_event_add_callback( libvlc_instance_t *p_instance,
                                libvlc_event_type_t i_event_type,
                                libvlc_callback_t f_callback,
                                void *user_data,
                                libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_t *entry;
    const char *callback_name = NULL;
    int res;

    if ( !f_callback )
        RAISEVOID (" Callback function is null ");

    /* malloc/free strategy:
     *  - alloc-ded in libvlc_event_add_callback
     *  - free-ed by libvlc_event_add_callback on error
     *  - free-ed by libvlc_event_remove_callback
     *  - free-ed in libvlc_destroy threw libvlc_event_remove_callback
     *    when entry is destroyed
     */
    entry = malloc( sizeof( struct libvlc_callback_entry_t ) );
    entry->f_callback = f_callback;
    entry->i_event_type = i_event_type;
    entry->p_user_data = user_data;
    
    switch ( i_event_type )
    {
        case VOLUME_CHANGED:
            callback_name = "volume-change";
            break;
        case INPUT_POSITION_CHANGED:
            break;
        default:
            free( entry );
            RAISEVOID( "Unsupported event." );
    }

    res = var_AddCallback( p_instance->p_libvlc_int,
                           callback_name,
                           handle_event,
                           entry );
    
    if (res != VLC_SUCCESS)
    {
        free ( entry );
        RAISEVOID("Internal callback registration was not successful. Callback not registered.");
    }
    
    add_callback_entry( entry, &p_instance->p_callback_list );

    return;
}

void libvlc_event_remove_all_callbacks( libvlc_instance_t *p_instance,
                                       libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_list_t *p_listitem = p_instance->p_callback_list;

    while( p_listitem )
    {
        libvlc_event_remove_callback( p_instance,
            p_listitem->elmt->i_event_type,
            p_listitem->elmt->f_callback,
            p_listitem->elmt->p_user_data,
            p_e);
        /* libvlc_event_remove_callback will reset the p_callback_list */
        p_listitem = p_instance->p_callback_list;
    }
}

void libvlc_event_remove_callback( libvlc_instance_t *p_instance,
                                   libvlc_event_type_t i_event_type,
                                   libvlc_callback_t f_callback,
                                   void *p_user_data,
                                   libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_list_t *p_listitem = p_instance->p_callback_list;

    while( p_listitem )
    {
        if( p_listitem->elmt->f_callback == f_callback
            && ( p_listitem->elmt->i_event_type == i_event_type )
            && ( p_listitem->elmt->p_user_data == p_user_data )
        
        )
        {
            const char * callback_name = NULL;
            int res;

            if( p_listitem->prev )
                p_listitem->prev->next = p_listitem->next;
            else
                p_instance->p_callback_list = p_listitem->next;

            p_listitem->next->prev = p_listitem->prev;

            switch ( i_event_type )
            {
                case VOLUME_CHANGED:
                    callback_name = "volume-change";
                    break;
                case INPUT_POSITION_CHANGED:
                    break;
                default:
                    RAISEVOID( "Unsupported event." );
            }

            res = var_DelCallback( p_instance->p_libvlc_int,
                                      callback_name,
                                      p_listitem->elmt );
            if (res != VLC_SUCCESS)
            {
                RAISEVOID("Internal callback unregistration was not successful. Callback not unregistered.");
            }

            free( p_listitem->elmt ); /* FIXME: need some locking here */
            free( p_listitem );
            break;
        }
        p_listitem = p_listitem->next;
    }
}
