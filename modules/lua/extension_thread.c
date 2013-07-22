/*****************************************************************************
 * extension_thread.c: Extensions Manager, Threads manager (no Lua here)
 *****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
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

/* I don't want to include lua headers here */
typedef struct lua_State lua_State;

#include "extension.h"
#include "assert.h"

struct thread_sys_t
{
    extensions_manager_t *p_mgr;
    extension_t *p_ext;
};

/** Thread Run */
static void* Run( void *data );
static void FreeCommands( struct command_t *command );
static int RemoveActivated( extensions_manager_t *p_mgr, extension_t *p_ext );

/**
 * Activate an extension
 * @param p_mgr This manager
 * @param p_ext Extension to activate
 * @return The usual VLC return codes
 **/
int Activate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    assert( p_ext != NULL );

    struct extension_sys_t *p_sys = p_ext->p_sys;
    assert( p_sys != NULL );

    msg_Dbg( p_mgr, "Activating extension '%s'", p_ext->psz_title );

    if( IsActivated( p_mgr, p_ext ) )
    {
        msg_Warn( p_mgr, "Extension is already activated!" );
        return VLC_EGENERIC;
    }

    /* Add this script to the activated extensions list */
    vlc_mutex_lock( &p_mgr->p_sys->lock );
    ARRAY_APPEND( p_mgr->p_sys->activated_extensions, p_ext );
    vlc_mutex_unlock( &p_mgr->p_sys->lock );

    /* Prepare first command */
    p_sys->command = calloc( 1, sizeof( struct command_t ) );
    if( !p_sys->command )
        return VLC_ENOMEM;
    p_sys->command->i_command = CMD_ACTIVATE; /* No params */

    /* Start thread */
    p_sys->b_exiting = false;

    if( vlc_clone( &p_sys->thread, Run, p_ext, VLC_THREAD_PRIORITY_LOW )
        != VLC_SUCCESS )
    {
        p_sys->b_exiting = true;
        // Note: Automatically deactivating the extension...
        Deactivate( p_mgr, p_ext );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/** Look for an extension in the activated extensions list
 * @todo FIXME Should be entered with the lock held
 **/
bool IsActivated( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    assert( p_ext != NULL );
    vlc_mutex_lock( &p_mgr->p_sys->lock );

    extension_t *p_iter;
    FOREACH_ARRAY( p_iter, p_mgr->p_sys->activated_extensions )
    {
        if( !p_iter )
            break;
        assert( p_iter->psz_name != NULL );
        if( !strcmp( p_iter->psz_name, p_ext->psz_name ) )
        {
            vlc_mutex_unlock( &p_mgr->p_sys->lock );
            return true;
        }
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_mgr->p_sys->lock );
    return false;
}

/** Recursively drop and free commands starting from "command" */
static void FreeCommands( struct command_t *command )
{
    if( !command ) return;
    struct command_t *next = command->next;
    switch( command->i_command )
    {
        case CMD_ACTIVATE:
        case CMD_DEACTIVATE:
        case CMD_CLICK: // Arg1 must not be freed
            break;

        case CMD_TRIGGERMENU:
        case CMD_PLAYING_CHANGED:
            free( command->data[0] ); // Arg1 is int*, to free
            break;

        default:
            break;
    }
    free( command );
    FreeCommands( next );
}

/** Deactivate this extension: pushes immediate command and drops queued */
int Deactivate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    (void) p_mgr;
    vlc_mutex_lock( &p_ext->p_sys->command_lock );

    if( p_ext->p_sys->b_exiting )
    {
        vlc_mutex_unlock( &p_ext->p_sys->command_lock );
        return VLC_EGENERIC;
    }

    if( p_ext->p_sys->progress )
    {
        // Extension is stuck, kill it now
        dialog_ProgressDestroy( p_ext->p_sys->progress );
        p_ext->p_sys->progress = NULL;
        vlc_mutex_unlock( &p_ext->p_sys->command_lock );
        KillExtension( p_mgr, p_ext );
        return VLC_SUCCESS;
    }

    /* Free the list of commands */
    if( p_ext->p_sys->command )
        FreeCommands( p_ext->p_sys->command->next );

    /* Push command */
    struct command_t *cmd = calloc( 1, sizeof( struct command_t ) );
    cmd->i_command = CMD_DEACTIVATE;
    if( p_ext->p_sys->command )
        p_ext->p_sys->command->next = cmd;
    else
        p_ext->p_sys->command = cmd;

    vlc_cond_signal( &p_ext->p_sys->wait );
    vlc_mutex_unlock( &p_ext->p_sys->command_lock );

    return VLC_SUCCESS;
}

/** Remove an extension from the activated list */
static int RemoveActivated( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    if( p_mgr->p_sys->b_killed )
        return VLC_SUCCESS;
    vlc_mutex_lock( &p_mgr->p_sys->lock );

    int i_idx = -1;
    extension_t *p_iter;
    FOREACH_ARRAY( p_iter, p_mgr->p_sys->activated_extensions )
    {
        i_idx++;
        if( !p_iter )
        {
            i_idx = -1;
            break;
        }
        assert( p_iter->psz_name != NULL );
        if( !strcmp( p_iter->psz_name, p_ext->psz_name ) )
            break;
    }
    FOREACH_END()

    if( i_idx >= 0 )
    {
        ARRAY_REMOVE( p_mgr->p_sys->activated_extensions, i_idx );
    }
    else
    {
        msg_Dbg( p_mgr, "Can't find extension '%s' in the activated list",
                 p_ext->psz_title );
    }

    vlc_mutex_unlock( &p_mgr->p_sys->lock );
    return (i_idx >= 0) ? VLC_SUCCESS : VLC_EGENERIC;
}

void KillExtension( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    /* Cancel thread if it seems stuck for a while */
    msg_Dbg( p_mgr, "Killing extension now" );
    vlc_cancel( p_ext->p_sys->thread );
    lua_ExtensionDeactivate( p_mgr, p_ext );
    p_ext->p_sys->b_exiting = true;
    RemoveActivated( p_mgr, p_ext );
}

/** Push a UI command */
int __PushCommand( extension_t *p_ext,  bool b_unique, command_type_e i_command,
                   va_list args )
{
    vlc_mutex_lock( &p_ext->p_sys->command_lock );

    /* Create command */
    struct command_t *cmd = calloc( 1, sizeof( struct command_t ) );
    cmd->i_command = i_command;
    switch( i_command )
    {
        case CMD_CLICK:
            cmd->data[0] = va_arg( args, void* );
            break;
        case CMD_TRIGGERMENU:
            {
                int *pi = malloc( sizeof( int ) );
                if( !pi )
                {
                    free( cmd );
                    vlc_mutex_unlock( &p_ext->p_sys->command_lock );
                    return VLC_ENOMEM;
                }
                *pi = va_arg( args, int );
                cmd->data[0] = pi;
            }
            break;
        case CMD_PLAYING_CHANGED:
            {
                int *pi = malloc( sizeof( int ) );
                if( !pi )
                {
                    free( cmd );
                    vlc_mutex_unlock( &p_ext->p_sys->command_lock );
                    return VLC_ENOMEM;
                }
                *pi = va_arg( args, int );
                cmd->data[0] = pi;
            }
            break;
        case CMD_CLOSE:
        case CMD_SET_INPUT:
        case CMD_UPDATE_META:
            // Nothing to do here
            break;
        default:
            msg_Dbg( p_ext->p_sys->p_mgr,
                     "Unknown command send to extension: %d", i_command );
            break;
    }

    /* Push command to the end of the queue */
    struct command_t *last = p_ext->p_sys->command;
    if( !last )
    {
        p_ext->p_sys->command = cmd;
    }
    else
    {
        bool b_skip = false;
        while( last->next != NULL )
        {
            if( b_unique && last->i_command == i_command )
            {
                // Do not push this 'unique' command a second time
                b_skip = !memcmp( last->data, cmd->data, sizeof( cmd->data ) );
                break;
            }
            else
            {
                last = last->next;
            }
        }
        if( !b_skip )
        {
            last->next = cmd;
        }
        else
        {
            FreeCommands( cmd );
        }
    }

    vlc_cond_signal( &p_ext->p_sys->wait );
    vlc_mutex_unlock( &p_ext->p_sys->command_lock );
    return VLC_SUCCESS;
}

/* Thread loop */
static void* Run( void *data )
{
    extension_t *p_ext = data;
    extensions_manager_t *p_mgr = p_ext->p_sys->p_mgr;

    vlc_mutex_lock( &p_ext->p_sys->command_lock );
    mutex_cleanup_push( &p_ext->p_sys->command_lock );

    while( !p_ext->p_sys->b_exiting )
    {
        struct command_t *cmd = p_ext->p_sys->command;

        /* Pop command in front */
        if( cmd == NULL )
        {
            vlc_cond_wait( &p_ext->p_sys->wait, &p_ext->p_sys->command_lock );
            continue;
        }
        p_ext->p_sys->command = cmd->next;
        cmd->next = NULL; /* unlink command (for FreeCommands()) */

        vlc_mutex_unlock( &p_ext->p_sys->command_lock );

        /* Run command */
        int cancel = vlc_savecancel();
        if( LockExtension( p_ext ) )
        {
            switch( cmd->i_command )
            {
                case CMD_ACTIVATE:
                {
                    if( lua_ExecuteFunction( p_mgr, p_ext, "activate", LUA_END ) < 0 )
                    {
                        msg_Err( p_mgr, "Could not activate extension!" );
                        Deactivate( p_mgr, p_ext );
                    }
                    break;
                }

                case CMD_DEACTIVATE:
                {
                    msg_Dbg( p_mgr, "Deactivating '%s'", p_ext->psz_title );
                    if( lua_ExtensionDeactivate( p_mgr, p_ext ) < 0 )
                    {
                        msg_Warn( p_mgr, "Extension '%s' did not deactivate properly",
                                  p_ext->psz_title );
                    }
                    p_ext->p_sys->b_exiting = true;
                    RemoveActivated( p_mgr, p_ext );
                    break;
                }

                case CMD_CLOSE:
                {
                    lua_ExecuteFunction( p_mgr, p_ext, "close", LUA_END );
                    break;
                }

                case CMD_CLICK:
                {
                    extension_widget_t *p_widget = cmd->data[0];
                    assert( p_widget );
                    msg_Dbg( p_mgr, "Clicking '%s': '%s'",
                             p_ext->psz_name, p_widget->psz_text );
                    if( lua_ExtensionWidgetClick( p_mgr, p_ext, p_widget ) < 0 )
                    {
                        msg_Warn( p_mgr, "Could not translate click" );
                    }
                    break;
                }

                case CMD_TRIGGERMENU:
                {
                    int *pi_id = cmd->data[0];
                    assert( pi_id );
                    msg_Dbg( p_mgr, "Trigger menu %d of '%s'",
                             *pi_id, p_ext->psz_name );
                    lua_ExtensionTriggerMenu( p_mgr, p_ext, *pi_id );
                    break;
                }

                case CMD_SET_INPUT:
                {
                    lua_ExecuteFunction( p_mgr, p_ext, "input_changed", LUA_END );
                    break;
                }

                case CMD_UPDATE_META:
                {
                    lua_ExecuteFunction( p_mgr, p_ext, "meta_changed", LUA_END );
                    break;
                }

                case CMD_PLAYING_CHANGED:
                {
                    lua_ExecuteFunction( p_mgr, p_ext, "playing_changed",
                            LUA_NUM, *((int *)cmd->data[0]), LUA_END );
                    break;
                }

                default:
                {
                    msg_Dbg( p_mgr, "Unknown command in extension command queue: %d",
                             cmd->i_command );
                    break;
                }
            }
            UnlockExtension( p_ext );
        }
        FreeCommands( cmd );

        vlc_restorecancel( cancel );
        vlc_mutex_lock( &p_ext->p_sys->command_lock );
    }

    vlc_cleanup_run( );
    msg_Dbg( p_mgr, "Extension thread end: '%s'", p_ext->psz_title );

    // Note: At this point, the extension should be deactivated
    return NULL;
}
