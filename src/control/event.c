/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <vlc_playlist.h>


/*
 * Private functions
 */

static int handle_event( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval,
                         void *p_data )
{
    /* This is thread safe, as the var_*Callback already provide the locking
     * facility for p_data */
    struct libvlc_callback_entry_t *entry = p_data;
    libvlc_event_t event;

    event.type = entry->i_event_type;

    if (event.type == INPUT_POSITION_CHANGED && !strcmp(psz_cmd, "intf-change"))
    {
        input_thread_t * p_input = (input_thread_t *)p_this;
        vlc_value_t val;
        var_Get( p_input, "position", &val );

        /* Only send event at a reasonable time precision (500ms) */
        /* (FIXME: this should be configurable) */
        if ((val.i_time % I64C(500000)) != 0)
        {
            /* Don't send this event */
            return VLC_SUCCESS;
        }
    }

    /* Call the client entry */
    entry->f_callback( entry->p_instance, &event, entry->p_user_data );

    return VLC_SUCCESS;
}

/* Utility function: Object should be released by vlc_object_release afterwards */
static input_thread_t * get_input(libvlc_instance_t * p_instance)
{
    libvlc_exception_t p_e_unused; /* FIXME: error checking here */
    libvlc_input_t * p_libvlc_input = libvlc_playlist_get_input( p_instance, &p_e_unused );
    input_thread_t * p_input;

    if( !p_libvlc_input )
        return NULL;
    
    p_input = libvlc_get_input_thread( p_libvlc_input, &p_e_unused );

    libvlc_input_free(p_libvlc_input);

    return p_input;
}

static int install_input_event( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data )
{
    libvlc_instance_t * p_instance = p_data;
    struct libvlc_callback_entry_list_t *p_listitem;
    input_thread_t * p_input = get_input( p_instance );

    vlc_mutex_lock( &p_instance->instance_lock );

    p_listitem = p_instance->p_callback_list;

    for( ; p_listitem ; p_listitem = p_listitem->next )
    {
        if (p_listitem->elmt->i_event_type == INPUT_POSITION_CHANGED)
        {
            /* FIXME: here we shouldn't listen on intf-change, we have to provide
             * in vlc core a more accurate callback */
            var_AddCallback( p_input, "intf-change", handle_event, p_listitem->elmt );
            var_AddCallback( p_input, "position", handle_event, p_listitem->elmt );
        }
    }

    vlc_mutex_unlock( &p_instance->instance_lock );
    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

static inline void add_callback_to_list( struct libvlc_callback_entry_t *entry,
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

static int remove_variable_callback( libvlc_instance_t *p_instance, 
                                     struct libvlc_callback_entry_t * p_entry )
{
    input_thread_t * p_input = get_input( p_instance );
    int res = VLC_SUCCESS;

    /* Note: Appropriate lock should be held by the caller */

    switch ( p_entry->i_event_type )
    {
        case VOLUME_CHANGED:
            res = var_DelCallback( p_instance->p_libvlc_int, "volume-change",
                             handle_event, p_entry );
            break;
        case INPUT_POSITION_CHANGED:
            /* We may not be deleting the right p_input callback, in this case this
             * will be a no-op */
            var_DelCallback( p_input, "intf-change",
                             handle_event, p_entry );
            var_DelCallback( p_input, "position",
                             handle_event, p_entry );
            break;
    }
    
    if (p_input)
        vlc_object_release( p_input );

    return res;
}

/*
 * Internal libvlc functions
 */
void libvlc_event_init( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    playlist_t *p_playlist = p_instance->p_libvlc_int->p_playlist;

    if( !p_playlist )
        RAISEVOID ("Can't listen to input event");

    /* Install a Callback for input changes, so
     * so we can track input event */
     var_AddCallback( p_playlist, "playlist-current",
                      install_input_event, p_instance );
}

void libvlc_event_fini( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    playlist_t *p_playlist = p_instance->p_libvlc_int->p_playlist;
    libvlc_exception_t p_e_unused;

    libvlc_event_remove_all_callbacks( p_instance, &p_e_unused );

    if( !p_playlist )
        RAISEVOID ("Can't unregister input events");

    var_DelCallback( p_playlist, "playlist-current",
                     install_input_event, p_instance );
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
    vlc_value_t unused1, unused2;
    int res = VLC_SUCCESS;

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
            res = var_AddCallback( p_instance->p_libvlc_int, "volume-change",
                           handle_event, entry );
            break;
        case INPUT_POSITION_CHANGED:
            install_input_event( NULL, NULL, unused1, unused2, p_instance);
            break;
        default:
            free( entry );
            RAISEVOID( "Unsupported event." );
    }

    if (res != VLC_SUCCESS)
    {
        free ( entry );
        RAISEVOID("Internal callback registration was not successful. Callback not registered.");
    }
    
    vlc_mutex_lock( &p_instance->instance_lock );
    add_callback_to_list( entry, &p_instance->p_callback_list );
    vlc_mutex_unlock( &p_instance->instance_lock );

    return;
}

void libvlc_event_remove_all_callbacks( libvlc_instance_t *p_instance,
                                       libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_list_t *p_listitem;

    vlc_mutex_lock( &p_instance->instance_lock );

    p_listitem = p_instance->p_callback_list;

    while( p_listitem )
    {
        remove_variable_callback( p_instance, p_listitem->elmt ); /* FIXME: We could warn on error */
        p_listitem = p_listitem->next;

    }
    p_instance->p_callback_list = NULL;

    vlc_mutex_unlock( &p_instance->instance_lock );
}

void libvlc_event_remove_callback( libvlc_instance_t *p_instance,
                                   libvlc_event_type_t i_event_type,
                                   libvlc_callback_t f_callback,
                                   void *p_user_data,
                                   libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_list_t *p_listitem;

    vlc_mutex_lock( &p_instance->instance_lock );

    p_listitem = p_instance->p_callback_list;

    while( p_listitem )
    {
        if( p_listitem->elmt->f_callback == f_callback
            && ( p_listitem->elmt->i_event_type == i_event_type )
            && ( p_listitem->elmt->p_user_data == p_user_data )
        
        )
        {
            remove_variable_callback( p_instance, p_listitem->elmt ); /* FIXME: We should warn on error */

            if( p_listitem->prev )
                p_listitem->prev->next = p_listitem->next;
            else
                p_instance->p_callback_list = p_listitem->next;


            p_listitem->next->prev = p_listitem->prev;

            free( p_listitem->elmt );

            free( p_listitem );
            break;
        }
        
        p_listitem = p_listitem->next;
    }
    vlc_mutex_unlock( &p_instance->instance_lock );
}
