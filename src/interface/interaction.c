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
static interaction_t *          InteractionGet( vlc_object_t * );
static void                     InteractionSearchInterface( interaction_t * );
static void*                    InteractionLoop( vlc_object_t * );
static void                     InteractionManage( interaction_t * );

static interaction_dialog_t    *DialogGetById( interaction_t* , int );
static void                     DialogDestroy( interaction_dialog_t * );
static int DialogSend( vlc_object_t *, interaction_dialog_t * );

#define DIALOG_INIT( type ) \
        DECMALLOC_ERR( p_new, interaction_dialog_t );       \
        memset( p_new, 0, sizeof( interaction_dialog_t ) ); \
        p_new->b_cancelled = false;                     \
        p_new->i_status = NEW_DIALOG;                       \
        p_new->i_flags = 0;                                 \
        p_new->i_type = INTERACT_DIALOG_##type;             \
        p_new->psz_returned[0] = NULL;                      \
        p_new->psz_returned[1] = NULL

#define FORMAT_DESC \
        va_start( args, psz_format ); \
        if( vasprintf( &p_new->psz_description, psz_format, args ) == -1 ) \
            return VLC_EGENERIC; \
        va_end( args )

/**
 * Send an error message, both in a blocking and non-blocking way
 *
 * \param p_this     Parent vlc_object
 * \param b_blocking Is this dialog blocking or not?
 * \param psz_title  Title for the dialog
 * \param psz_format The message to display
 * \return           VLC_SUCCESS or VLC_EGENERIC
 */
int __intf_UserFatal( vlc_object_t *p_this, bool b_blocking,
                       const char *psz_title,
                       const char *psz_format, ... )
{
    va_list args;
    DIALOG_INIT( ONEWAY );

    p_new->psz_title = strdup( psz_title );
    FORMAT_DESC;

    if( b_blocking )
        p_new->i_flags = DIALOG_BLOCKING_ERROR;
    else
        p_new->i_flags = DIALOG_NONBLOCKING_ERROR;

    return DialogSend( p_this, p_new );
}

/**
 * Helper function to send a warning, which is always shown non-blocking
 *
 * \param p_this     Parent vlc_object
 * \param psz_title  Title for the dialog
 * \param psz_format The message to display
 * \return           VLC_SUCCESS or VLC_EGENERIC
 */
int __intf_UserWarn( vlc_object_t *p_this,
                     const char *psz_title,
                     const char *psz_format, ... )
{
    va_list args;
    DIALOG_INIT( ONEWAY );

    p_new->psz_title = strdup( psz_title );
    FORMAT_DESC;

    p_new->i_flags = DIALOG_WARNING;

    return DialogSend( p_this, p_new );
}

/**
 * Helper function to ask a yes-no-cancel question
 *
 * \param p_this           Parent vlc_object
 * \param psz_title        Title for the dialog
 * \param psz_description  A description
 * \param psz_default      caption for the default button
 * \param psz_alternate    caption for the alternate button
 * \param psz_other        caption for the optional 3rd button (== cancel)
 * \return                 Clicked button code
 */
int __intf_UserYesNo( vlc_object_t *p_this,
                      const char *psz_title,
                      const char *psz_description,
                      const char *psz_default,
                      const char *psz_alternate,
                      const char *psz_other )
{
    DIALOG_INIT( TWOWAY );

    p_new->psz_title = strdup( psz_title );
    p_new->psz_description = strdup( psz_description );
    p_new->i_flags = DIALOG_YES_NO_CANCEL;
    p_new->psz_default_button = strdup( psz_default );
    p_new->psz_alternate_button = strdup( psz_alternate );
    if( psz_other )
        p_new->psz_other_button = strdup( psz_other );

    return DialogSend( p_this, p_new );
}

/**
 * Helper function to create a dialogue showing a progress-bar with some info
 *
 * \param p_this           Parent vlc_object
 * \param psz_title        Title for the dialog (NULL implies main intf )
 * \param psz_status       Current status
 * \param f_position       Current position (0.0->100.0)
 * \param i_timeToGo       Time (in sec) to go until process is finished
 * \return                 Dialog id, to give to UserProgressUpdate
 */
int __intf_Progress( vlc_object_t *p_this, const char *psz_title,
                     const char *psz_status, float f_pos, int i_time )
{
    DIALOG_INIT( ONEWAY );
    p_new->psz_description = strdup( psz_status );
    p_new->val.f_float = f_pos;
    p_new->i_timeToGo = i_time;
    p_new->psz_alternate_button = strdup( _( "Cancel" ) );

    if( psz_title )
    {
        p_new->psz_title = strdup( psz_title );
        p_new->i_flags = DIALOG_USER_PROGRESS;
    }
    else
        p_new->i_flags = DIALOG_INTF_PROGRESS;

    DialogSend( p_this, p_new );
    return p_new->i_id;
}

/**
 * Update a progress bar in a dialogue
 *
 * \param p_this           Parent vlc_object
 * \param i_id             Identifier of the dialog
 * \param psz_status       New status
 * \param f_position       New position (0.0->100.0)
 * \param i_timeToGo       Time (in sec) to go until process is finished
 * \return                 nothing
 */
void __intf_ProgressUpdate( vlc_object_t *p_this, int i_id,
                            const char *psz_status, float f_pos, int i_time )
{
    interaction_t *p_interaction = InteractionGet( p_this );
    interaction_dialog_t *p_dialog;

    if( !p_interaction ) return;

    vlc_object_lock( p_interaction );
    p_dialog  =  DialogGetById( p_interaction, i_id );

    if( !p_dialog )
    {
        vlc_object_unlock( p_interaction );
        vlc_object_release( p_interaction );
        return;
    }

    free( p_dialog->psz_description );
    p_dialog->psz_description = strdup( psz_status );

    p_dialog->val.f_float = f_pos;
    p_dialog->i_timeToGo = i_time;

    p_dialog->i_status = UPDATED_DIALOG;

    vlc_object_signal_unlocked( p_interaction );
    vlc_object_unlock( p_interaction );
    vlc_object_release( p_interaction );
}

/**
 * Helper function to communicate dialogue cancellations between the
 * interface module and the caller
 *
 * \param p_this           Parent vlc_object
 * \param i_id             Identifier of the dialogue
 * \return                 Either true or false
 */
bool __intf_UserProgressIsCancelled( vlc_object_t *p_this, int i_id )
{
    interaction_t *p_interaction = InteractionGet( p_this );
    interaction_dialog_t *p_dialog;
    bool b_cancel;

    if( !p_interaction ) return true;

    vlc_object_lock( p_interaction );
    p_dialog  =  DialogGetById( p_interaction, i_id );
    if( !p_dialog )
    {
        vlc_object_unlock( p_interaction ) ;
        vlc_object_release( p_interaction );
        return true;
    }

    b_cancel = p_dialog->b_cancelled;
    vlc_object_unlock( p_interaction );
    vlc_object_release( p_interaction );
    return b_cancel;
}

/**
 * Helper function to make a login/password dialogue
 *
 * \param p_this           Parent vlc_object
 * \param psz_title        Title for the dialog
 * \param psz_description  A description
 * \param ppsz_login       Returned login
 * \param ppsz_password    Returned password
 * \return                 Clicked button code
 */
int __intf_UserLoginPassword( vlc_object_t *p_this,
        const char *psz_title,
        const char *psz_description,
        char **ppsz_login,
        char **ppsz_password )
{
    int i_ret;
    DIALOG_INIT( TWOWAY );
    p_new->i_type = INTERACT_DIALOG_TWOWAY;
    p_new->psz_title = strdup( psz_title );
    p_new->psz_description = strdup( psz_description );
    p_new->psz_default_button = strdup( _("OK" ) );
    p_new->psz_alternate_button = strdup( _("Cancel" ) );

    p_new->i_flags = DIALOG_LOGIN_PW_OK_CANCEL;

    i_ret = DialogSend( p_this, p_new );

    if( i_ret != DIALOG_CANCELLED && i_ret != VLC_EGENERIC )
    {
        *ppsz_login = p_new->psz_returned[0]?
            strdup( p_new->psz_returned[0] ) : NULL;
        *ppsz_password = p_new->psz_returned[1]?
            strdup( p_new->psz_returned[1] ) : NULL;
    }
    return i_ret;
}

/**
 * Helper function to make a dialogue asking the user for !password string
 *
 * \param p_this           Parent vlc_object
 * \param psz_title        Title for the dialog
 * \param psz_description  A description
 * \param ppsz_usersString Returned login
 * \return                 Clicked button code
 */
int __intf_UserStringInput( vlc_object_t *p_this,
        const char *psz_title,
        const char *psz_description,
        char **ppsz_usersString )
{
    int i_ret;
    DIALOG_INIT( TWOWAY );
    p_new->i_type = INTERACT_DIALOG_TWOWAY;
    p_new->psz_title = strdup( psz_title );
    p_new->psz_description = strdup( psz_description );

    p_new->i_flags = DIALOG_PSZ_INPUT_OK_CANCEL;

    i_ret = DialogSend( p_this, p_new );

    if( i_ret != DIALOG_CANCELLED )
    {
        *ppsz_usersString = p_new->psz_returned[0]?
            strdup( p_new->psz_returned[0] ) : NULL;
    }
    return i_ret;
}

/**
 * Hide an interaction dialog
 *
 * \param p_this the parent vlc object
 * \param i_id the id of the item to hide
 * \return nothing
 */
void __intf_UserHide( vlc_object_t *p_this, int i_id )
{
    interaction_t *p_interaction = InteractionGet( p_this );
    interaction_dialog_t *p_dialog;

    if( !p_interaction ) return;

    vlc_object_lock( p_interaction );
    p_dialog = DialogGetById( p_interaction, i_id );

    if( p_dialog )
    {
        p_dialog->i_status = ANSWERED_DIALOG;
        vlc_object_signal_unlocked( p_interaction );
    }

    vlc_object_unlock( p_interaction );
    vlc_object_release( p_interaction );
}

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
    p_interaction->i_last_id = 0;

    if( vlc_thread_create( p_interaction, "Interaction control",
                           InteractionLoop, VLC_THREAD_PRIORITY_LOW,
                           false ) )
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

    vlc_object_kill( p_interaction );
    vlc_thread_join( p_interaction );
    vlc_object_release( p_interaction );
}

/**********************************************************************
 * The following functions are local
 **********************************************************************/

/* Get the interaction object. Create it if needed */
static interaction_t * InteractionGet( vlc_object_t *p_this )
{
    interaction_t *obj = libvlc_priv(p_this->p_libvlc)->p_interaction;
    if( obj )
        vlc_object_yield( obj );
    return obj;
}


/* Look for an interface suitable for interaction */
static void InteractionSearchInterface( interaction_t *p_interaction )
{
    vlc_list_t  *p_list;
    int          i_index;

    p_interaction->p_intf = NULL;

    p_list = vlc_list_find( p_interaction, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Err( p_interaction, "unable to create module list" );
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

/* Find an interaction dialog by its id */
static interaction_dialog_t *DialogGetById( interaction_t *p_interaction,
                                            int i_id )
{
    int i;
    for( i = 0 ; i< p_interaction->i_dialogs; i++ )
    {
        if( p_interaction->pp_dialogs[i]->i_id == i_id )
            return p_interaction->pp_dialogs[i];
    }
    return NULL;
}

/* Destroy a dialog */
static void DialogDestroy( interaction_dialog_t *p_dialog )
{
    free( p_dialog->psz_title );
    free( p_dialog->psz_description );
    free( p_dialog->psz_default_button );
    free( p_dialog->psz_alternate_button );
    free( p_dialog->psz_other_button );
    free( p_dialog );
}

/* Ask for the dialog to be sent to the user. Wait for answer
 * if required */
static int DialogSend( vlc_object_t *p_this, interaction_dialog_t *p_dialog )
{
    interaction_t *p_interaction = InteractionGet( p_this );

    if( !p_interaction )
        return VLC_EGENERIC;

    /* Get an id, if we don't already have one */
    vlc_object_lock( p_interaction );
    if( p_dialog->i_id == 0 )
        p_dialog->i_id = ++p_interaction->i_last_id;
    vlc_object_unlock( p_interaction );

    if( p_this->i_flags & OBJECT_FLAGS_NOINTERACT )
    {
        vlc_object_release( p_interaction );
        return VLC_EGENERIC;
    }

    if( config_GetInt( p_this, "interact" ) ||
        p_dialog->i_flags & DIALOG_BLOCKING_ERROR ||
        p_dialog->i_flags & DIALOG_NONBLOCKING_ERROR )
    {
        bool b_found = false;
        int i;
        p_dialog->p_interaction = p_interaction;
        p_dialog->p_parent = p_this;

        /* Check if we have already added this dialog */
        vlc_object_lock( p_interaction );
        for( i = 0 ; i< p_interaction->i_dialogs; i++ )
        {
            if( p_interaction->pp_dialogs[i]->i_id == p_dialog->i_id )
                b_found = true;
        }
        /* Add it to the queue, the main loop will send the orders to the
         * interface */
        if( ! b_found )
        {
            INSERT_ELEM( p_interaction->pp_dialogs,
                         p_interaction->i_dialogs,
                         p_interaction->i_dialogs,
                         p_dialog );
        }
        else
            p_dialog->i_status = UPDATED_DIALOG;

        if( p_dialog->i_type == INTERACT_DIALOG_TWOWAY ) /* Wait for answer */
        {
            vlc_object_signal_unlocked( p_interaction );
            while( p_dialog->i_status != ANSWERED_DIALOG &&
                   p_dialog->i_status != HIDING_DIALOG &&
                   p_dialog->i_status != HIDDEN_DIALOG &&
                   !p_dialog->p_parent->b_die )
            {
                vlc_object_unlock( p_interaction );
                msleep( 100000 );
                vlc_object_lock( p_interaction );
            }
            if( p_dialog->p_parent->b_die )
            {
                p_dialog->i_return = DIALOG_CANCELLED;
                p_dialog->i_status = ANSWERED_DIALOG;
            }
            p_dialog->i_flags |= DIALOG_GOT_ANSWER;
            vlc_object_signal_unlocked( p_interaction );
            vlc_object_unlock( p_interaction );
            vlc_object_release( p_interaction );
            return p_dialog->i_return;
        }
        else
        {
            /* Pretend we already retrieved the "answer" */
            p_dialog->i_flags |=  DIALOG_GOT_ANSWER;
            vlc_object_signal_unlocked( p_interaction );
            vlc_object_unlock( p_interaction );
            vlc_object_release( p_interaction );
            return VLC_SUCCESS;
        }
    }
    else
    {
        vlc_object_release( p_interaction );
        return VLC_EGENERIC;
    }
}

static void* InteractionLoop( vlc_object_t *p_this )
{
    int i;
    interaction_t *p_interaction = (interaction_t*) p_this;

    vlc_object_lock( p_this );
    while( vlc_object_alive( p_this ) )
    {
        InteractionManage( p_interaction );
        vlc_object_wait( p_this );
    }
    vlc_object_unlock( p_this );

    /* Remove all dialogs - Interfaces must be able to clean up their data */
    for( i = p_interaction->i_dialogs -1 ; i >= 0; i-- )
    {
        interaction_dialog_t * p_dialog = p_interaction->pp_dialogs[i];
        DialogDestroy( p_dialog );
        REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs, i );
    }
    return NULL;
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

    /* Nothing to do */
    if( p_interaction->i_dialogs == 0 ) return;

    InteractionSearchInterface( p_interaction );
    if( !p_interaction->p_intf )
    {
        /* We mark all dialogs as answered with their "default" answer */
        for( i_index = 0 ; i_index < p_interaction->i_dialogs; i_index ++ )
        {
            interaction_dialog_t *p_dialog = p_interaction->pp_dialogs[i_index];
            p_dialog->i_return = DIALOG_DEFAULT; /* Give default answer */

            /* Pretend we have hidden and destroyed it */
            if( p_dialog->i_status == HIDDEN_DIALOG )
                p_dialog->i_status = DESTROYED_DIALOG;
            else
                p_dialog->i_status = HIDING_DIALOG;
        }
    }
    else
        vlc_object_yield( p_interaction->p_intf );

    for( i_index = 0 ; i_index < p_interaction->i_dialogs; i_index ++ )
    {
        interaction_dialog_t *p_dialog = p_interaction->pp_dialogs[i_index];
        switch( p_dialog->i_status )
        {
        case ANSWERED_DIALOG:
            /* Ask interface to hide it */
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
            p_dialog->i_action = INTERACT_DESTROY;
            val.p_address = p_dialog;
            if( p_interaction->p_intf )
                var_Set( p_interaction->p_intf, "interaction", val );
            break;
        case DESTROYED_DIALOG:
            /* Interface has now destroyed it, remove it */
            REMOVE_ELEM( p_interaction->pp_dialogs, p_interaction->i_dialogs,
                         i_index);
            i_index--;
            DialogDestroy( p_dialog );
            break;
        case NEW_DIALOG:
            /* This is truly a new dialog, send it. */

            p_dialog->i_action = INTERACT_NEW;
            val.p_address = p_dialog;
            if( p_interaction->p_intf )
                var_Set( p_interaction->p_intf, "interaction", val );
            p_dialog->i_status = SENT_DIALOG;
            break;
        }
    }

    if( p_interaction->p_intf )
        vlc_object_release( p_interaction->p_intf );
}
