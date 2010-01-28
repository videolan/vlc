/*****************************************************************************
 * extension.h: Lua Extensions (meta data, web information, ...)
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

#ifndef LUA_EXTENSION_H
#define LUA_EXTENSION_H

#include <vlc_extensions.h>
#include <vlc_arrays.h>

///< Array of extension_t
TYPEDEF_ARRAY( extension_t, array_extension_t );

/* List of available commands */
#define CMD_ACTIVATE    1
#define CMD_DEACTIVATE  2
#define CMD_TRIGGERMENU 3    /* Arg1 = int*, pointing to id to trigger. free */
#define CMD_CLICK       4    /* Arg1 = extension_widget_t* */
#define CMD_CLOSE       5

struct extensions_manager_sys_t
{
    /* List of activated extensions */
    DECL_ARRAY( extension_t* ) activated_extensions;

    /* Lock for this list */
    vlc_mutex_t lock;

    /* Lua specific */
    lua_State *L;

    /* Flag indicating that the module is about to be unloaded */
    bool b_killed;
};

struct extension_sys_t
{
    /* Extension general */
    int i_capabilities;

    /* Lua specific */
    lua_State *L;

    /* Thread data */
    vlc_thread_t thread;
    vlc_mutex_t command_lock;
    vlc_mutex_t running_lock;
    vlc_cond_t wait;
    bool b_exiting;
    extensions_manager_t *p_mgr;     ///< Parent
    /* Queue of commands to execute */
    struct command_t
    {
        int i_command;
        void *data[10];         ///< Optional void* arguments
        struct command_t *next; ///< Next command
    } *command;
};

/* Extensions: manager functions */
int Activate( extensions_manager_t *p_mgr, extension_t * );
bool IsActivated( extensions_manager_t *p_mgr, extension_t * );
int Deactivate( extensions_manager_t *p_mgr, extension_t * );
void WaitForDeactivation( extension_t *p_ext );
int PushCommand( extension_t *p_ext, int i_command, ... );
bool LockExtension( extension_t *p_ext );
void UnlockExtension( extension_t *p_ext );

/* Lua specific functions */
extension_t *vlclua_extension_get( lua_State *L );
int lua_ExtensionActivate( extensions_manager_t *, extension_t * );
int lua_ExtensionDeactivate( extensions_manager_t *, extension_t * );
int lua_ExecuteFunction( extensions_manager_t *p_mgr, extension_t *p_ext,
                         const char *psz_function );
int lua_ExtensionWidgetClick( extensions_manager_t *p_mgr,
                              extension_t *p_ext,
                              extension_widget_t *p_widget );
int lua_ExtensionTriggerMenu( extensions_manager_t *p_mgr,
                              extension_t *p_ext, int id );

#endif // LUA_EXTENSION_H
