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

#include <vlc_common.h>
#include <vlc_messages.h>

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
 * @param p_ext Extension to activate
 * @return The usual VLC return codes
 **/
int Activate(extension_t *p_ext)
{
    assert( p_ext != NULL );

    struct lua_extension *sys = p_ext->p_sys;
    assert(sys != NULL);

    vlc_mutex_lock(&sys->command_lock);
    if (sys->b_activating)
    {
        vlc_mutex_unlock(&sys->command_lock);
        return VLC_SUCCESS;
    }
    if (!sys->b_activated)
    {
        /* Prepare first command */
        assert(sys->command == NULL);
        sys->command = calloc(1, sizeof(*sys->command));
        if (!sys->command)
        {
            vlc_mutex_unlock(&sys->command_lock);
            return VLC_ENOMEM;
        }
        sys->command->i_command = CMD_ACTIVATE; /* No params */
        if (sys->b_thread_running)
        {
            vlc_debug(p_ext->logger, "Reactivating extension %s", p_ext->psz_title);
            vlc_cond_signal(&sys->wait);
        }
        sys->b_activating = true;
    }
    vlc_mutex_unlock(&sys->command_lock);

    if (sys->b_thread_running)
        return VLC_SUCCESS;

    vlc_debug(p_ext->logger, "Activating extension '%s'", p_ext->psz_title);
    /* Start thread */
    sys->b_exiting = false;
    sys->b_thread_running = true;

    if (vlc_clone(&sys->thread, Run, p_ext) != VLC_SUCCESS)
    {
        sys->b_exiting = true;
        sys->b_thread_running = false;
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
    struct lua_extension *sys = p_ext->p_sys;
    vlc_mutex_assert(&sys->command_lock);

    struct command_t *cmd = calloc( 1, sizeof( struct command_t ) );
    if( unlikely( cmd == NULL ) )
        return false;
    /* Free the list of commands */
    if (sys->command != NULL)
        FreeCommands(sys->command->next);

    /* Push command */

    cmd->i_command = CMD_DEACTIVATE;
    if (sys->command)
        sys->command->next = cmd;
    else
        sys->command = cmd;

    sys->b_deactivating = true;
    sys->b_activating = false;
    vlc_cond_signal(&sys->wait);
    return true;
}

/** Deactivate this extension: pushes immediate command and drops queued */
int Deactivate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    struct lua_extension *sys = p_ext->p_sys;
    vlc_mutex_lock(&sys->command_lock);
    if (!sys->b_activated && !sys->b_activating)
    {
        vlc_mutex_unlock(&sys->command_lock);
        return VLC_SUCCESS;
    }

    if (sys->b_exiting)
    {
        vlc_mutex_unlock(&sys->command_lock);
        return VLC_EGENERIC;
    }

    if (sys->p_progress_id != NULL)
    {
        // Extension is stuck, kill it now
        vlc_dialog_release(p_mgr, sys->p_progress_id);
        sys->p_progress_id = NULL;
        KillExtension(p_ext);
        vlc_mutex_unlock(&sys->command_lock);
        return VLC_SUCCESS;
    }

    bool b_success = QueueDeactivateCommand( p_ext );
    vlc_mutex_unlock(&sys->command_lock);

    return b_success ? VLC_SUCCESS : VLC_ENOMEM;
}

/* MUST be called with command_lock held */
void KillExtension(extension_t *p_ext)
{
    struct lua_extension *sys = p_ext->p_sys;
    vlc_debug(p_ext->logger, "Killing extension now");
    vlclua_fd_interrupt(&sys->dtable);
    sys->b_activated = false;
    sys->b_exiting = true;
    vlc_cond_signal(&sys->wait);
}

/** Push a UI command */
int PushCommand__( extension_t *p_ext,  bool b_unique, command_type_e i_command,
                   va_list args )
{
    struct lua_extension *sys = p_ext->p_sys;
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
            msg_Dbg(sys->p_mgr, "Unknown command send to extension: %d",
                    i_command);
            break;
    }

    vlc_mutex_lock(&sys->command_lock);

    /* Push command to the end of the queue */
    struct command_t *last = sys->command;
    if( !last )
    {
        sys->command = cmd;
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

    vlc_cond_signal(&sys->wait);
    vlc_mutex_unlock(&sys->command_lock);
    return VLC_SUCCESS;
}

/* Thread loop */
static void* Run( void *data )
{
    extension_t *p_ext = data;
    struct lua_extension *sys = p_ext->p_sys;
    extensions_manager_t *p_mgr = sys->p_mgr;

    vlc_thread_set_name("vlc-lua-ext");

    vlc_mutex_lock(&sys->command_lock);

    while (!sys->b_exiting || (sys->command && sys->command->i_command == CMD_DEACTIVATE))
    {
        struct command_t *cmd = sys->command;

        /* Pop command in front */
        if( cmd == NULL )
        {
            vlc_cond_wait(&sys->wait, &sys->command_lock);
            continue;
        }
        sys->command = cmd->next;
        cmd->next = NULL; /* unlink command (for FreeCommands()) */
        // Create watch timer
        vlc_timer_schedule(sys->timer, false, WATCH_TIMER_PERIOD,
                           VLC_TIMER_FIRE_ONCE);
        vlc_mutex_unlock(&sys->command_lock);

        /* Run command */
        vlc_mutex_lock(&sys->running_lock);
        switch( cmd->i_command )
        {
            case CMD_ACTIVATE:
            {
                if( lua_ExecuteFunction( p_mgr, p_ext, "activate", LUA_END ) < 0 )
                {
                    msg_Err( p_mgr, "Could not activate extension!" );
                    vlc_mutex_lock(&sys->command_lock);
                    QueueDeactivateCommand( p_ext );
                    vlc_mutex_unlock(&sys->command_lock);
                    break;
                }
                vlc_mutex_lock(&sys->command_lock);
                sys->b_activated = true;
                sys->b_activating = false;
                vlc_mutex_unlock(&sys->command_lock);
                break;
            }

            case CMD_DEACTIVATE:
            {
                vlc_debug(p_ext->logger, "Deactivating '%s'", p_ext->psz_title);
                if( lua_ExtensionDeactivate( p_mgr, p_ext ) < 0 )
                {
                    vlc_warning(p_ext->logger, "Extension '%s' did not deactivate properly",
                                p_ext->psz_title);
                }
                vlc_mutex_lock(&sys->command_lock);
                sys->b_activated = false;
                sys->b_deactivating = false;
                vlc_mutex_unlock(&sys->command_lock);
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
                vlc_debug(p_ext->logger, "Clicking '%s': '%s'",
                         p_ext->psz_name, p_widget->psz_text);
                if( lua_ExtensionWidgetClick( p_mgr, p_ext, p_widget ) < 0 )
                    vlc_debug(p_ext->logger, "Could not translate click");
                break;
            }

            case CMD_TRIGGERMENU:
            {
                int *pi_id = cmd->data[0];
                assert( pi_id );
                vlc_debug(p_ext->logger, "Trigger menu %d of '%s'",
                          *pi_id, p_ext->psz_name);
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
        vlc_mutex_unlock(&sys->running_lock);

        FreeCommands( cmd );

        vlc_mutex_lock(&sys->command_lock);
        // Reset watch timer and timestamp
        if (sys->p_progress_id != NULL)
        {
            vlc_dialog_release(p_mgr, sys->p_progress_id);
            sys->p_progress_id = NULL;
        }
        vlc_timer_disarm(sys->timer);
    }

    vlc_mutex_unlock(&sys->command_lock);
    msg_Dbg( p_mgr, "Extension thread end: '%s'", p_ext->psz_title );

    // Note: At this point, the extension should be deactivated
    return NULL;
}
