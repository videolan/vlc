/*****************************************************************************
 * interaction.c: User interaction functions
 *****************************************************************************
 * Copyright © 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Felix Kühne <fkuehne@videolan.org>
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

/**
 *   \file
 *   This file contains functions related to user interaction management
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_interface.h>
#include "interface.h"
#include "libvlc.h"

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/**
 * This structure contains the active interaction dialogs, and is
 * used by the manager
 */
struct interaction_t
{
    VLC_COMMON_MEMBERS

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t wait;

    int                         i_dialogs;      ///< Number of dialogs
    interaction_dialog_t      **pp_dialogs;     ///< Dialogs
    intf_thread_t              *p_intf;         ///< Interface to use
};

static void*                    InteractionLoop( void * );
static void                     InteractionManage( interaction_t * );

static void                     DialogDestroy( interaction_dialog_t * );

#define DIALOG_INIT( type, err ) \
        interaction_dialog_t* p_new = calloc( 1, sizeof( interaction_dialog_t ) ); \
        if( !p_new ) return err;                        \
        p_new->p_parent = vlc_object_hold( p_this );    \
        p_new->b_cancelled = false;                     \
        p_new->i_status = SENT_DIALOG;                  \
        p_new->i_flags = 0;                             \
        p_new->i_type = INTERACT_DIALOG_##type;         \
        p_new->psz_returned[0] = NULL;                  \
        p_new->psz_returned[1] = NULL

/**
 * Create the initial interaction object
 * (should only be used in libvlc_InternalInit, LibVLC private)
 *
 * \return a vlc_object_t that should be freed when done.
 */
interaction_t * interaction_Init( libvlc_int_t *p_libvlc )
{
    interaction_t *p_interaction;

    /* Make sure we haven't yet created an interaction object */
    assert( libvlc_priv(p_libvlc)->p_interaction == NULL );

    p_interaction = vlc_custom_create( VLC_OBJECT(p_libvlc),
                                       sizeof( *p_interaction ),
                                       VLC_OBJECT_GENERIC, "interaction" );
    if( !p_interaction )
        return NULL;

    vlc_object_attach( p_interaction, p_libvlc );
    p_interaction->i_dialogs = 0;
    p_interaction->pp_dialogs = NULL;
    p_interaction->p_intf = NULL;

    vlc_mutex_init( &p_interaction->lock );
    vlc_cond_init( &p_interaction->wait );

    if( vlc_clone( &p_interaction->thread, InteractionLoop, p_interaction,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_interaction, "Interaction control thread creation failed, "
                 "interaction will not be displayed" );
        vlc_object_detach( p_interaction );
        vlc_object_release( p_interaction );
        return NULL;
    }

    return p_interaction;
}

void interaction_Destroy( interaction_t *p_interaction )
{
    if( !p_interaction )
        return;

    vlc_cancel( p_interaction->thread );
    vlc_join( p_interaction->thread, NULL );
    vlc_cond_destroy( &p_interaction->wait );
    vlc_mutex_destroy( &p_interaction->lock );

    /* Remove all dialogs - Interfaces must be able to clean up their data */
    for( int i = p_interaction->i_dialogs -1 ; i >= 0; i-- )
    {
        interaction_dialog_t * p_dialog = p_interaction->pp_dialogs[i];
        DialogDestroy( p_dialog );
        REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs, i );
    }
    vlc_object_release( p_interaction );
}

static vlc_mutex_t intf_lock = VLC_STATIC_MUTEX;

int interaction_Register( intf_thread_t *intf )
{
    libvlc_priv_t *priv = libvlc_priv( intf->p_libvlc );
    int ret = VLC_EGENERIC;

    vlc_mutex_lock( &intf_lock );
    if( priv->p_interaction_intf == NULL )
    {   /* Since the interface is responsible for unregistering itself before
         * it terminates, an object reference is not needed. */
        priv->p_interaction_intf = intf;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock( &intf_lock );
    return ret;
}

int interaction_Unregister( intf_thread_t *intf )
{
    libvlc_priv_t *priv = libvlc_priv( intf->p_libvlc );
    int ret = VLC_EGENERIC;

    vlc_mutex_lock( &intf_lock );
    if( priv->p_interaction_intf == intf )
    {
        priv->p_interaction_intf = NULL;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock( &intf_lock );
    return ret;
}

/**********************************************************************
 * The following functions are local
 **********************************************************************/

/* Destroy a dialog */
static void DialogDestroy( interaction_dialog_t *p_dialog )
{
    free( p_dialog->psz_title );
    free( p_dialog->psz_description );
    free( p_dialog->psz_alternate_button );
    vlc_object_release( p_dialog->p_parent );
    free( p_dialog );
}

static void* InteractionLoop( void *p_this )
{
    interaction_t *p_interaction = p_this;

    vlc_mutex_lock( &p_interaction->lock );
    mutex_cleanup_push( &p_interaction->lock );
    for( ;; )
    {
        int canc = vlc_savecancel();
        InteractionManage( p_interaction );
        vlc_restorecancel( canc );

        vlc_cond_wait( &p_interaction->wait, &p_interaction->lock );
    }
    vlc_cleanup_pop( );
    assert( 0 );
}

/**
 * The main interaction processing loop
 *
 * \param p_interaction the interaction object
 * \return nothing
 */

static void InteractionManage( interaction_t *p_interaction )
{
    vlc_value_t val;
    int i_index;

    for( i_index = 0 ; i_index < p_interaction->i_dialogs; i_index ++ )
    {
        interaction_dialog_t *p_dialog = p_interaction->pp_dialogs[i_index];
        switch( p_dialog->i_status )
        {
        case ANSWERED_DIALOG:
            /* Ask interface to hide it */
            p_dialog->i_action = INTERACT_HIDE;
            val.p_address = p_dialog;
            var_Set( p_dialog->p_interface, "interaction", val );
            p_dialog->i_status = HIDING_DIALOG;
            break;
        case UPDATED_DIALOG:
            p_dialog->i_action = INTERACT_UPDATE;
            val.p_address = p_dialog;
            var_Set( p_dialog->p_interface, "interaction", val );
            p_dialog->i_status = SENT_DIALOG;
            break;
        case HIDDEN_DIALOG:
            if( !(p_dialog->i_flags & DIALOG_GOT_ANSWER) ) break;
            p_dialog->i_action = INTERACT_DESTROY;
            val.p_address = p_dialog;
            var_Set( p_dialog->p_interface, "interaction", val );
            break;
        case DESTROYED_DIALOG:
            /* Interface has now destroyed it, remove it */
            REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs,
                         i_index);
            i_index--;
            DialogDestroy( p_dialog );
            break;
        }
    }
}
