/*****************************************************************************
 * libvlc_callback.c: New libvlc callback control API
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

int libvlc_private_handle_callback( vlc_object_t *p_this, char const *psz_cmd,
                                    vlc_value_t oldval, vlc_value_t newval,
                                    void *p_data );

void libvlc_callback_register_for_event( libvlc_instance_t *p_instance,
                                        libvlc_event_type_t i_event_type,
                                        libvlc_callback_t f_callback,
                                        void *user_data,
                                        libvlc_exception_t *p_e )
{

    if ( ! &f_callback )
        RAISEVOID (" Callback function is null ");
    
    struct libvlc_callback_entry_t *entry = malloc( sizeof( struct libvlc_callback_entry_t ) );
    entry->f_callback = f_callback;
    entry->i_event_type = i_event_type;
    entry->p_user_data = user_data;

    const char *callback_name;
    
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

    int res = var_AddCallback( p_instance->p_libvlc_int,
                               callback_name,
                               libvlc_private_handle_callback,
                               entry );
    
    if (res != VLC_SUCCESS)
    {
        free ( entry );
        RAISEVOID("Internal callback registration was not successful. Callback not registered.");
    }
    
    add_callback_entry( entry, &p_instance->p_callback_list );

    return;
}

void libvlc_callback_unregister_for_event( libvlc_instance_t *p_instance,
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
            if( p_listitem->prev )
                p_listitem->prev->next = p_listitem->next;
            else
                p_instance->p_callback_list = p_listitem->next;
            
            p_listitem->next->prev = p_listitem->prev;
            free( p_listitem );
            break;
        }
        p_listitem = p_listitem->next;
    }
}


int libvlc_private_handle_callback( vlc_object_t *p_this, char const *psz_cmd,
                                     vlc_value_t oldval, vlc_value_t newval,
                                     void *p_data )
{
    struct libvlc_callback_entry_t *entry = p_data;
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
