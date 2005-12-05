/*****************************************************************************
 * interaction.c: User interaction functions
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
 * $Id: interface.c 10147 2005-03-05 17:18:30Z gbazin $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/**
 *   \file
 *   This file contains functions related to user interaction management
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                                   /* FILE */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_interaction.h"
#include "vlc_interface.h"
#include "vlc_playlist.h"

static void intf_InteractionInit( playlist_t *p_playlist );
static int intf_WaitAnswer( intf_thread_t *p_intf, interaction_dialog_t *p_interact );
static int intf_Send( intf_thread_t *p_intf, interaction_dialog_t *p_interact );

/**
 * Send an interaction element to the user
 *
 * \param p_this the calling vlc_object_t
 * \param p_interact the interaction element
 * \return VLC_SUCCESS or an error code
 */
int  __intf_Interact( vlc_object_t *p_this, interaction_dialog_t *
                                    p_interact )
{
    vlc_list_t  *p_list;
    int          i_index;
    intf_thread_t *p_chosen_intf;


    /* Search a suitable intf */
    p_list = vlc_list_find( p_this, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Err( p_this, "Unable to create module list" );
        return VLC_FALSE;
    }

    p_chosen_intf = NULL;
    for( i_index = 0; i_index < p_list->i_count; i_index ++ )
    {
        intf_thread_t *p_intf = (intf_thread_t *)
                                        p_list->p_values[i_index].p_object;
        if( p_intf->pf_interact != NULL )
        {
            p_chosen_intf = p_intf;
            break;
        }
    }
    if( !p_chosen_intf )
    {
        msg_Dbg( p_this, "No interface suitable for interaction" );
        return VLC_FALSE;
    }
    msg_Dbg( p_this, "found an interface for interaction" );

    /* Find id, if we don't already have one */
    if( p_interact->i_id == 0 )
    {
        p_interact->i_id = ++p_chosen_intf->i_last_id;
    }

    if( p_interact->i_type == INTERACT_ASK )
    {
        return intf_WaitAnswer( p_chosen_intf, p_interact );
    }
    else
    {
        return intf_Send( p_chosen_intf, p_interact );
    }
}

int intf_WaitAnswer( intf_thread_t *p_intf, interaction_dialog_t *p_interact )
{
    // TODO: Add to queue, wait for answer
    return VLC_SUCCESS;
}

int intf_Send( intf_thread_t *p_intf, interaction_dialog_t *p_interact )
{
    // TODO: Add to queue, return
    return VLC_SUCCESS;
}

// The playlist manages the user interaction to avoid creating another thread
void intf_InteractionManage( playlist_t *p_playlist )
{
    if( p_playlist->p_interaction == NULL )
    {
        intf_InteractionInit( p_playlist );
    }

    vlc_mutex_lock( &p_playlist->p_interaction->object_lock );

    /* Todo:
     *    - Walk the queue
     *    - If blocking
     *       - If have answer, signal what is waiting (vlc_cond ? dangerous in case of pb ?)
     *         And then, if not reusable, destroy
     *    - If have update, send update
     */

    vlc_mutex_unlock( &p_playlist->p_interaction->object_lock );
}

static void intf_InteractionInit( playlist_t *p_playlist )
{
    interaction_t *p_interaction;
    p_interaction = vlc_object_create( VLC_OBJECT( p_playlist ), sizeof( interaction_t ) );
    if( !p_interaction )
    {
        msg_Err( p_playlist,"out of memory" );
        return;
    }

    p_interaction->i_dialogs = 0;
    p_interaction->pp_dialogs = NULL;
}

/** Helper function to build a progress bar */
interaction_dialog_t *__intf_ProgressBuild( vlc_object_t *p_this,
                                          const char *psz_text )
{
    interaction_dialog_t *p_new = (interaction_dialog_t *)malloc(
                                        sizeof( interaction_dialog_t ) );


    return p_new;
}

#define INTERACT_INIT( new )                                            \
        interaction_dialog_t *new = (interaction_dialog_t*)malloc(          \
                                        sizeof( interaction_dialog_t ) ); \
        new->i_widgets = 0;                                             \
        new->pp_widgets = NULL;                                         \
        new->psz_title = NULL;                                          \
        new->psz_description = NULL;                                    \
        new->i_id = 0;

#define INTERACT_FREE( new )                                            \
        if( new->psz_title ) free( new->psz_title );                    \
        if( new->psz_description ) free( new->psz_description );

/** Helper function to send a fatal message */
void intf_UserFatal( vlc_object_t *p_this, const char *psz_title,
                     const char *psz_format, ... )
{
    va_list args;

    INTERACT_INIT( p_new );
    p_new->i_type = INTERACT_FATAL;
    p_new->psz_title = strdup( psz_title );

    va_start( args, psz_format );
    vasprintf( &p_new->psz_description, psz_format, args );
    va_end( args );

    intf_Interact( p_this, p_new );

    INTERACT_FREE( p_new );
}
