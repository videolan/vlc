/*****************************************************************************
 * extension_thread.c: Extensions Manager, Threads manager (no Lua here)
 *****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
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

#include "vlc.h"
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

    vlc_mutex_lock( &p_sys->command_lock );
    if ( p_sys->b_activated == false )
    {
        /* Prepare first command */
        assert(p_sys->command == NULL);
        p_sys->command = calloc( 1, sizeof( struct command_t ) );
        if( !p_sys->command )
        {
            vlc_mutex_unlock( &p_sys->command_lock );
            return VLC_ENOMEM;
        }
        p_sys->command->i_command = CMD_ACTIVATE; /* No params */
        if (p_sys->b_thread_running == true)
        {
            msg_Dbg( p_mgr, "Reactivating extension %s", p_ext->psz_title);
            vlc_cond_signal( &p_sys->wait );
        }
    }
    vlc_mutex_unlock( &p_sys->command_lock );

    if (p_sys->b_thread_running == true)
        return VLC_SUCCESS;

    msg_Dbg( p_mgr, "Activating extension '%s'", p_ext->psz_title );
    /* Start thread */
    p_sys->b_exiting = false;
    p_sys->b_thread_running = true;

    if( vlc_clone( &p_sys->thread, Run, p_ext, VLC_THREAD_PRIORITY_LOW )
        != VLC_SUCCESS )
    {
        p_sys->b_exiting = true;
        p_sys->b_thread_running = false;
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
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

bool QueueDeactivateCommand( extension_t *p_ext )
{
    struct command_t *cmd = calloc( 1, sizeof( struct command_t ) );
    if( unlikely( cmd == NULL ) )
        return false;
    /* Free the list of commands */
    if( p_ext->p_sys->command )
        FreeCommands( p_ext->p_sys->command->next );

    /* Push command */

    cmd->i_command = CMD_DEACTIVATE;
    if( p_ext->p_sys->command )
        p_ext->p_sys->command->next = cmd;
    else
        p_ext->p_sys->command = cmd;

    vlc_cond_signal( &p_ext->p_sys->wait );
    return true;
}

/** Deactivate this extension: pushes immediate command and drops queued */
int Deactivate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    vlc_mutex_lock( &p_ext->p_sys->command_lock );

    if( p_ext->p_sys->b_exiting )
    {
        vlc_mutex_unlock( &p_ext->p_sys->command_lock );
        return VLC_EGENERIC;
    }

    if( p_ext->p_sys->p_progress_id != NULL )
    {
        // Extension is stuck, kill it now
        vlc_dialog_release( p_mgr, p_ext->p_sys->p_progress_id );
        p_ext->p_sys->p_progress_id = NULL;
        KillExtension( p_mgr, p_ext );
        vlc_mutex_unlock( &p_ext->p_sys->command_lock );
        return VLC_SUCCESS;
    }

    bool b_success = QueueDeactivateCommand( p_ext );
    vlc_mutex_unlock( &p_ext->p_sys->command_lock );

    return b_success ? VLC_SUCCESS : VLC_ENOMEM;
}

/* MUST be called with command_lock held */
void KillExtension( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    msg_Dbg( p_mgr, "Killing extension now" );
    vlclua_fd_interrupt( &p_ext->p_sys->dtable );
    p_ext->p_sys->b_activated = false;
    p_ext->p_sys->b_exiting = true;
    vlc_cond_signal( &p_ext->p_sys->wait );
}

/** Push a UI command */
int PushCommand__( extension_t *p_ext,  bool b_unique, command_type_e i_command,
                   va_list args )
{
    /* Create command */
    struct command_t *cmd = calloc( 1, sizeof( struct command_t ) );
    if( unlikely( cmd == NULL ) )
        return VLC_ENOMEM;
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

    vlc_mutex_lock( &p_ext->p_sys->command_lock );

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
        // Create watch timer
        vlc_timer_schedule( p_ext->p_sys->timer, false, WATCH_TIMER_PERIOD,
                            VLC_TIMER_FIRE_ONCE );
        vlc_mutex_unlock( &p_ext->p_sys->command_lock );

        /* Run command */
        vlc_mutex_lock( &p_ext->p_sys->running_lock );
        switch( cmd->i_command )
        {
            case CMD_ACTIVATE:
            {
                if( lua_ExecuteFunction( p_mgr, p_ext, "activate", LUA_END ) < 0 )
                {
                    msg_Err( p_mgr, "Could not activate extension!" );
                    vlc_mutex_lock( &p_ext->p_sys->command_lock );
                    QueueDeactivateCommand( p_ext );
                    vlc_mutex_unlock( &p_ext->p_sys->command_lock );
                    break;
                }
                vlc_mutex_lock( &p_ext->p_sys->command_lock );
                p_ext->p_sys->b_activated = true;
                vlc_mutex_unlock( &p_ext->p_sys->command_lock );
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
                vlc_mutex_lock( &p_ext->p_sys->command_lock );
                p_ext->p_sys->b_activated = false;
                vlc_mutex_unlock( &p_ext->p_sys->command_lock );
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
        vlc_mutex_unlock( &p_ext->p_sys->running_lock );

        FreeCommands( cmd );

        vlc_mutex_lock( &p_ext->p_sys->command_lock );
        // Reset watch timer and timestamp
        if( p_ext->p_sys->p_progress_id != NULL )
        {
            vlc_dialog_release( p_mgr, p_ext->p_sys->p_progress_id );
            p_ext->p_sys->p_progress_id = NULL;
        }
        vlc_timer_disarm( p_ext->p_sys->timer );
    }

    vlc_mutex_unlock( &p_ext->p_sys->command_lock );
    msg_Dbg( p_mgr, "Extension thread end: '%s'", p_ext->psz_title );

    // Note: At this point, the extension should be deactivated
    return NULL;
}
