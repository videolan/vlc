/*****************************************************************************
 * interaction.c: User interaction functions
 *****************************************************************************
 * Copyright (C) 2005-2006 VideoLAN
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void                  intf_InteractionInit( playlist_t *p_playlist );
static interaction_t *       intf_InteractionGet( vlc_object_t *p_this );
static void                  intf_InteractionSearchInterface( interaction_t *
                                                          p_interaction );
static int                   intf_WaitAnswer( interaction_t *p_interact,
                             interaction_dialog_t *p_dialog );
static int                   intf_Send( interaction_t *p_interact,
                             interaction_dialog_t *p_dialog );
static interaction_dialog_t *intf_InteractionGetById( vlc_object_t* , int );
static void                  intf_InteractionDialogDestroy(
                                              interaction_dialog_t *p_dialog );

/**
 * Send an interaction element to the user
 *
 * \param p_this the calling vlc_object_t
 * \param p_interact the interaction element
 * \return VLC_SUCCESS or an error code
 */
int  __intf_Interact( vlc_object_t *p_this, interaction_dialog_t *
                                    p_dialog )
{
    interaction_t *p_interaction = intf_InteractionGet( p_this );

    /* Get an id, if we don't already have one */
    if( p_dialog->i_id == 0 )
    {
        p_dialog->i_id = ++p_interaction->i_last_id;
    }

    if( p_this->i_flags & OBJECT_FLAGS_NOINTERACT )
    {
       return VLC_EGENERIC;
    }

    p_dialog->p_interaction = p_interaction;
    p_dialog->p_parent = p_this;

    if( p_dialog->i_type == INTERACT_DIALOG_TWOWAY )
    {
        return intf_WaitAnswer( p_interaction, p_dialog );
    }
    else
    {
        p_dialog->i_flags |=  DIALOG_GOT_ANSWER;
        return intf_Send( p_interaction, p_dialog );
    }
}

/**
 * Destroy the interaction system
 * \param The interaction object to destroy
 * \return nothing
 */
void intf_InteractionDestroy( interaction_t *p_interaction )
{
    int i;

    // Remove all dialogs - Interfaces must be able to clean up their data

    for( i = p_interaction->i_dialogs -1 ; i >= 0; i-- )
    {
        interaction_dialog_t * p_dialog = p_interaction->pp_dialogs[i];
        intf_InteractionDialogDestroy( p_dialog );
        REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs, i );
    }

    vlc_object_destroy( p_interaction );
}

/**
 * The main interaction processing loop
 * This function is called from the playlist loop
 *
 * \param p_playlist the parent playlist
 * \return nothing
 */
void intf_InteractionManage( playlist_t *p_playlist )
{
    vlc_value_t val;
    int i_index;
    interaction_t *p_interaction;

    p_interaction = p_playlist->p_interaction;

    // Nothing to do
    if( p_interaction->i_dialogs == 0 ) return;

    vlc_mutex_lock( &p_interaction->object_lock );

    intf_InteractionSearchInterface( p_interaction );

    if( !p_interaction->p_intf )
    {
        // We mark all dialogs as answered with their "default" answer
        for( i_index = 0 ; i_index < p_interaction->i_dialogs; i_index ++ )
        {
            interaction_dialog_t *p_dialog = p_interaction->pp_dialogs[i_index];

            // Give default answer
            p_dialog->i_return = DIALOG_DEFAULT;
            if( p_dialog->i_flags & DIALOG_OK_CANCEL )
                p_dialog->i_return = DIALOG_CANCELLED;

            // Pretend we have hidden and destroyed it
            if( p_dialog->i_status == HIDDEN_DIALOG )
            {
                p_dialog->i_status = DESTROYED_DIALOG;
            }
            else
            {
                p_dialog->i_status = HIDING_DIALOG;
            }
        }
    }
    else
    {
        vlc_object_yield( p_interaction->p_intf );
    }

    for( i_index = 0 ; i_index < p_interaction->i_dialogs; i_index ++ )
    {
        interaction_dialog_t *p_dialog = p_interaction->pp_dialogs[i_index];
        switch( p_dialog->i_status )
        {
        case ANSWERED_DIALOG:
            // Ask interface to hide it
            p_dialog->i_action = INTERACT_HIDE;
            val.p_address = p_dialog;
            if( p_interaction->p_intf )
                var_Set( p_interaction->p_intf, "interaction", val );
            p_dialog->i_status = HIDING_DIALOG;
            break;
        case UPDATED_DIALOG:
            p_dialog->i_action = INTERACT_UPDATE;
            val.p_address = p_dialog;
            if( p_interaction->p_intf )
                var_Set( p_interaction->p_intf, "interaction", val );
            p_dialog->i_status = SENT_DIALOG;
            break;
        case HIDDEN_DIALOG:
            if( !(p_dialog->i_flags & DIALOG_GOT_ANSWER) ) break;
            if( !(p_dialog->i_flags & DIALOG_REUSABLE) )
            {
                p_dialog->i_action = INTERACT_DESTROY;
                val.p_address = p_dialog;
                if( p_interaction->p_intf )
                    var_Set( p_interaction->p_intf, "interaction", val );
            }
            break;
        case DESTROYED_DIALOG:
            // Interface has now destroyed it, remove it
            REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs,
                         i_index);
            i_index--;
            intf_InteractionDialogDestroy( p_dialog );
            break;
        case NEW_DIALOG:
            // This is truly a new dialog, send it.
            p_dialog->i_action = INTERACT_NEW;
            val.p_address = p_dialog;
            if( p_interaction->p_intf )
                var_Set( p_interaction->p_intf, "interaction", val );
            p_dialog->i_status = SENT_DIALOG;
            break;
        }
    }

    if( p_interaction->p_intf )
    {
        vlc_object_release( p_interaction->p_intf );
    }

    vlc_mutex_unlock( &p_playlist->p_interaction->object_lock );
}



#define INTERACT_INIT( new )                                            \
        new = (interaction_dialog_t*)malloc(                            \
                        sizeof( interaction_dialog_t ) );               \
        new->i_widgets = 0;                                             \
        new->pp_widgets = NULL;                                         \
        new->psz_title = NULL;                                          \
        new->psz_description = NULL;                                    \
        new->i_id = 0;                                                  \
        new->i_flags = 0;                                               \
        new->i_status = NEW_DIALOG;

#define INTERACT_FREE( new )                                            \
        if( new->psz_title ) free( new->psz_title );                    \
        if( new->psz_description ) free( new->psz_description );

/** Helper function to send an error message
 *  \param p_this     Parent vlc_object
 *  \param i_id       A predefined ID, 0 if not applicable
 *  \param psz_title  Title for the dialog
 *  \param psz_format The message to display
 *  */
void __intf_UserFatal( vlc_object_t *p_this,
                       const char *psz_title,
                       const char *psz_format, ... )
{
    va_list args;
    interaction_dialog_t *p_new = NULL;
    user_widget_t *p_widget = NULL;
    int i_id = DIALOG_ERRORS;

    if( i_id > 0 )
    {
        p_new = intf_InteractionGetById( p_this, i_id );
    }
    if( !p_new )
    {
        INTERACT_INIT( p_new );
        if( i_id > 0 ) p_new->i_id = i_id ;
    }
    else
    {
        p_new->i_status = UPDATED_DIALOG;
    }

    p_new->i_flags |= DIALOG_REUSABLE;

    p_new->i_type = INTERACT_DIALOG_ONEWAY;
    p_new->psz_title = strdup( psz_title );

    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );

    p_widget->i_type = WIDGET_TEXT;
    p_widget->val.psz_string = NULL;

    va_start( args, psz_format );
    vasprintf( &p_widget->psz_text, psz_format, args );
    va_end( args );

    INSERT_ELEM ( p_new->pp_widgets,
                  p_new->i_widgets,
                  p_new->i_widgets,
                  p_widget );

    p_new->i_flags |= DIALOG_CLEAR_NOSHOW;

    intf_Interact( p_this, p_new );
}

/** Helper function to ask a yes-no question
 *  \param p_this           Parent vlc_object
 *  \param psz_title        Title for the dialog
 *  \param psz_description  A description
 *  \return                 Clicked button code
 */
int __intf_UserYesNo( vlc_object_t *p_this,
                      const char *psz_title,
                      const char *psz_description )
{
    int i_ret;
    interaction_dialog_t *p_new = NULL;
    user_widget_t *p_widget = NULL;

    INTERACT_INIT( p_new );

    p_new->i_type = INTERACT_DIALOG_TWOWAY;
    p_new->psz_title = strdup( psz_title );

    /* Text */
    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );
    p_widget->i_type = WIDGET_TEXT;
    p_widget->psz_text = strdup( psz_description );
    p_widget->val.psz_string = NULL;
    INSERT_ELEM ( p_new->pp_widgets, p_new->i_widgets,
                  p_new->i_widgets,  p_widget );

    p_new->i_flags = DIALOG_YES_NO_CANCEL;

    i_ret = intf_Interact( p_this, p_new );

    return i_ret;
}

/** Helper function to make a progressbar box
 *  \param p_this           Parent vlc_object
 *  \param psz_title        Title for the dialog
 *  \param psz_status       Current status
 *  \param f_position       Current position (0.0->100.0)
 *  \return                 Dialog id, to give to UserProgressUpdate
 */
int __intf_UserProgress( vlc_object_t *p_this,
                         const char *psz_title,
                         const char *psz_status,
                         float f_pos )
{
    int i_ret;
    interaction_dialog_t *p_new = NULL;
    user_widget_t *p_widget = NULL;

    INTERACT_INIT( p_new );

    p_new->i_type = INTERACT_DIALOG_ONEWAY;
    p_new->psz_title = strdup( psz_title );

    /* Progress bar */
    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );
    p_widget->i_type = WIDGET_PROGRESS;
    p_widget->psz_text = strdup( psz_status );
    p_widget->val.f_float = f_pos;
    INSERT_ELEM ( p_new->pp_widgets, p_new->i_widgets,
                  p_new->i_widgets,  p_widget );

    i_ret = intf_Interact( p_this, p_new );

    return p_new->i_id;
}

/** Update a progress bar
 *  \param p_this           Parent vlc_object
 *  \param i_id             Identifier of the dialog
 *  \param psz_status       New status
 *  \param f_position       New position (0.0->100.0)
 *  \return                 nothing
 */
void __intf_UserProgressUpdate( vlc_object_t *p_this, int i_id,
                                const char *psz_status, float f_pos )
{
    interaction_t *p_interaction = intf_InteractionGet( p_this );
    interaction_dialog_t *p_dialog;

    if( !p_interaction ) return;

    vlc_mutex_lock( &p_interaction->object_lock );
    p_dialog  =  intf_InteractionGetById( p_this, i_id );

    if( !p_dialog || p_dialog->i_status == NEW_DIALOG )
    {
        vlc_mutex_unlock( &p_interaction->object_lock ) ;
        return;
    }

    if( p_dialog->pp_widgets[0]->psz_text )
        free( p_dialog->pp_widgets[0]->psz_text );
    p_dialog->pp_widgets[0]->psz_text = strdup( psz_status );

    p_dialog->pp_widgets[0]->val.f_float = f_pos;

    p_dialog->i_status = UPDATED_DIALOG;
    vlc_mutex_unlock( &p_interaction->object_lock) ;
}

/** Helper function to make a login/password box
 *  \param p_this           Parent vlc_object
 *  \param psz_title        Title for the dialog
 *  \param psz_description  A description
 *  \param ppsz_login       Returned login
 *  \param ppsz_password    Returned password
 *  \return                 Clicked button code
 */
int __intf_UserLoginPassword( vlc_object_t *p_this,
                              const char *psz_title,
                              const char *psz_description,
                              char **ppsz_login,
                              char **ppsz_password )
{
    int i_ret;
    interaction_dialog_t *p_new = NULL;
    user_widget_t *p_widget = NULL;

    INTERACT_INIT( p_new );

    p_new->i_type = INTERACT_DIALOG_TWOWAY;
    p_new->psz_title = strdup( psz_title );

    /* Text */
    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );
    p_widget->i_type = WIDGET_TEXT;
    p_widget->psz_text = strdup( psz_description );
    p_widget->val.psz_string = NULL;
    INSERT_ELEM ( p_new->pp_widgets, p_new->i_widgets,
                  p_new->i_widgets,  p_widget );

    /* Login */
    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );
    p_widget->i_type = WIDGET_INPUT_TEXT;
    p_widget->psz_text = strdup( _("Login") );
    p_widget->val.psz_string = NULL;
    INSERT_ELEM ( p_new->pp_widgets, p_new->i_widgets,
                  p_new->i_widgets,  p_widget );

    /* Password */
    p_widget = (user_widget_t* )malloc( sizeof( user_widget_t ) );
    p_widget->i_type = WIDGET_INPUT_TEXT;
    p_widget->psz_text = strdup( _("Password") );
    p_widget->val.psz_string = NULL;
    INSERT_ELEM ( p_new->pp_widgets, p_new->i_widgets,
                  p_new->i_widgets,  p_widget );

    p_new->i_flags = DIALOG_OK_CANCEL;

    i_ret = intf_Interact( p_this, p_new );

    if( i_ret != DIALOG_CANCELLED )
    {
        *ppsz_login = strdup( p_new->pp_widgets[1]->val.psz_string );
        *ppsz_password = strdup( p_new->pp_widgets[2]->val.psz_string );
    }
    return i_ret;
}

/** Hide an interaction dialog
 * \param p_this the parent vlc object
 * \param i_id the id of the item to hide
 * \return nothing
 */
void __intf_UserHide( vlc_object_t *p_this, int i_id )
{
    interaction_t *p_interaction = intf_InteractionGet( p_this );
    interaction_dialog_t *p_dialog;

    if( !p_interaction ) return;

    vlc_mutex_lock( &p_interaction->object_lock );
    p_dialog  =  intf_InteractionGetById( p_this, i_id );

    if( !p_dialog )
    {
       vlc_mutex_unlock( &p_interaction->object_lock );
       return;
    }

    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_interaction->object_lock );
}



/**********************************************************************
 * The following functions are local
 **********************************************************************/

/* Get the interaction object. Create it if needed */
static interaction_t * intf_InteractionGet( vlc_object_t *p_this )
{
    playlist_t *p_playlist;
    interaction_t *p_interaction;

    p_playlist = (playlist_t*) vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );

    if( !p_playlist )
    {
        return NULL;
    }

    if( p_playlist->p_interaction == NULL )
    {
        intf_InteractionInit( p_playlist );
    }

    p_interaction = p_playlist->p_interaction;

    vlc_object_release( p_playlist );

    return p_interaction;
}

/* Create the interaction object in the given playlist object */
static void intf_InteractionInit( playlist_t *p_playlist )
{
    interaction_t *p_interaction;

    msg_Dbg( p_playlist, "initializing interaction system" );

    p_interaction = vlc_object_create( VLC_OBJECT( p_playlist ),
                                       sizeof( interaction_t ) );
    if( !p_interaction )
    {
        msg_Err( p_playlist,"out of memory" );
        return;
    }

    p_interaction->i_dialogs = 0;
    p_interaction->pp_dialogs = NULL;
    p_interaction->p_intf = NULL;
    p_interaction->i_last_id = DIALOG_LAST_PREDEFINED + 1;

    vlc_mutex_init( p_interaction , &p_interaction->object_lock );

    p_playlist->p_interaction  = p_interaction;
}

/* Look for an interface suitable for interaction */
static void intf_InteractionSearchInterface( interaction_t *p_interaction )
{
    vlc_list_t  *p_list;
    int          i_index;

    p_interaction->p_intf = NULL;

    p_list = vlc_list_find( p_interaction, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Err( p_interaction, "Unable to create module list" );
        return;
    }

    for( i_index = 0; i_index < p_list->i_count; i_index ++ )
    {
        intf_thread_t *p_intf = (intf_thread_t *)
                                        p_list->p_values[i_index].p_object;
        if( p_intf->b_interaction )
        {
            p_interaction->p_intf = p_intf;
            break;
        }
    }
    vlc_list_release ( p_list );
}

/* Add a dialog to the queue and wait for answer */
static int intf_WaitAnswer( interaction_t *p_interact,
                            interaction_dialog_t *p_dialog )
{
    int i;
    vlc_bool_t b_found = VLC_FALSE;
    vlc_mutex_lock( &p_interact->object_lock );
    for( i = 0 ; i< p_interact->i_dialogs; i++ )
    {
        if( p_interact->pp_dialogs[i]->i_id == p_dialog->i_id )
        {
            b_found = VLC_TRUE;
        }
    }
    if( ! b_found )
    {
        INSERT_ELEM( p_interact->pp_dialogs,
                     p_interact->i_dialogs,
                     p_interact->i_dialogs,
                     p_dialog );
    }
    else
        p_dialog->i_status = UPDATED_DIALOG;
    vlc_mutex_unlock( &p_interact->object_lock );

    /// \todo Check that the initiating object is not dying
    while( p_dialog->i_status != ANSWERED_DIALOG &&
           p_dialog->i_status != HIDING_DIALOG &&
           p_dialog->i_status != HIDDEN_DIALOG &&
           !p_dialog->p_parent->b_die )
    {
        msleep( 100000 );
    }
    /// \todo locking
    if( p_dialog->p_parent->b_die )
    {
        p_dialog->i_return = DIALOG_CANCELLED;
        p_dialog->i_status = ANSWERED_DIALOG;
    }
    p_dialog->i_flags |= DIALOG_GOT_ANSWER;
    return p_dialog->i_return;
}

/* Add a dialog to the queue and return */
static int intf_Send( interaction_t *p_interact,
                      interaction_dialog_t *p_dialog )
{
    int i;
    vlc_bool_t b_found = VLC_FALSE;
    if( p_interact == NULL ) return VLC_ENOOBJ;
    vlc_mutex_lock( &p_interact->object_lock );

    for( i = 0 ; i< p_interact->i_dialogs; i++ )
    {
        if( p_interact->pp_dialogs[i]->i_id == p_dialog->i_id )
        {
            b_found = VLC_TRUE;
        }
    }
    if( !b_found )
    {
        INSERT_ELEM( p_interact->pp_dialogs,
                     p_interact->i_dialogs,
                     p_interact->i_dialogs,
                     p_dialog );
    }
    else
        p_dialog->i_status = UPDATED_DIALOG;
    // Pretend we already retrieved the "answer"
    p_dialog->i_flags |= DIALOG_GOT_ANSWER;
    vlc_mutex_unlock( &p_interact->object_lock );
    return VLC_SUCCESS;
}

/* Find an interaction dialog by its id */
static interaction_dialog_t *intf_InteractionGetById( vlc_object_t* p_this,
                                                       int i_id )
{
    interaction_t *p_interaction = intf_InteractionGet( p_this );
    int i;

    if( !p_interaction ) return NULL;

    for( i = 0 ; i< p_interaction->i_dialogs; i++ )
    {
        if( p_interaction->pp_dialogs[i]->i_id == i_id )
        {
            return p_interaction->pp_dialogs[i];
        }
    }
    return NULL;
}

#define FREE( i ) { if( i ) free( i ); i = NULL; }

static void intf_InteractionDialogDestroy( interaction_dialog_t *p_dialog )
{
    int i;
    for( i = p_dialog->i_widgets - 1 ; i >= 0 ; i-- )
    {
        user_widget_t *p_widget = p_dialog->pp_widgets[i];
        FREE( p_widget->psz_text );
        if( p_widget->i_type == WIDGET_INPUT_TEXT )
        {
            FREE( p_widget->val.psz_string );
        }

        REMOVE_ELEM( p_dialog->pp_widgets, p_dialog->i_widgets, i );
        free( p_widget );
    }
    FREE( p_dialog->psz_title );
    FREE( p_dialog->psz_description );

    free( p_dialog );
}
