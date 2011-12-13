/*****************************************************************************
 * exit.c: LibVLC termination event
 *****************************************************************************
 * Copyright (C) 2009-2010 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_interface.h>
#include "libvlc.h"
#include "../lib/libvlc_internal.h"

void vlc_ExitInit( vlc_exit_t *exit )
{
    vlc_mutex_init( &exit->lock );
    exit->handler = NULL;
    exit->opaque = NULL;
    exit->killed = false;
}

void vlc_ExitDestroy( vlc_exit_t *exit )
{
    vlc_mutex_destroy( &exit->lock );
}


/**
 * Registers a callback for the LibVLC exit event.
 *
 * @note This function conflicts with libvlc_InternalWait().
 * Use either or none of them, but not both.
 */
void libvlc_SetExitHandler( libvlc_int_t *p_libvlc, void (*handler) (void *),
                            void *opaque )
{
    vlc_exit_t *exit = &libvlc_priv( p_libvlc )->exit;

    vlc_mutex_lock( &exit->lock );
    if( exit->killed ) /* already exited! (race condition) */
        handler( opaque );
    exit->handler = handler;
    exit->opaque = opaque;
    vlc_mutex_unlock( &exit->lock );
}

/**
 * Posts an exit signal to LibVLC instance. This only emits a notification to
 * the main thread. It might take a while before the actual cleanup occurs.
 * This function should only be called on behalf of the user.
 */
void libvlc_Quit( libvlc_int_t *p_libvlc )
{
    vlc_exit_t *exit = &libvlc_priv( p_libvlc )->exit;

    vlc_mutex_lock( &exit->lock );
    if( !exit->killed )
    {
        msg_Dbg( p_libvlc, "exiting" );
        exit->killed = true;
        if( exit->handler != NULL )
            exit->handler( exit->opaque );
    }
    vlc_mutex_unlock( &exit->lock );
}


static void exit_wakeup( void *data )
{
    vlc_cond_signal( data );
}

/**
 * Waits until the LibVLC instance gets an exit signal.
 * This normally occurs when the user "exits" an interface plugin. But it can
 * also be triggered by the special vlc://quit item, the update checker, or
 * the playlist engine.
 */
void libvlc_InternalWait( libvlc_int_t *p_libvlc )
{
    vlc_exit_t *exit = &libvlc_priv( p_libvlc )->exit;
    vlc_cond_t wait;

    vlc_cond_init( &wait );

    vlc_mutex_lock( &exit->lock );
    exit->handler = exit_wakeup;
    exit->opaque = &wait;
    while( !exit->killed )
        vlc_cond_wait( &wait, &exit->lock );
    vlc_mutex_unlock( &exit->lock );

    vlc_cond_destroy( &wait );
}
