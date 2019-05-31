/*****************************************************************************
 * extension.h: Lua Extensions (meta data, web information, ...)
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

#ifndef LUA_EXTENSION_H
#define LUA_EXTENSION_H

#include <vlc_extensions.h>
#include <vlc_arrays.h>
#include <vlc_dialog.h>

#define WATCH_TIMER_PERIOD    VLC_TICK_FROM_SEC(10) ///< 10s period for the timer

/* List of available commands */
typedef enum
{
    CMD_ACTIVATE = 1,
    CMD_DEACTIVATE,
    CMD_TRIGGERMENU,    /* Arg1 = int*, pointing to id to trigger. free */
    CMD_CLICK,          /* Arg1 = extension_widget_t* */
    CMD_CLOSE,
    CMD_SET_INPUT,      /* No arg. Just signal current input changed */
    CMD_UPDATE_META,    /* No arg. Just signal current input item meta changed */
    CMD_PLAYING_CHANGED /* Arg1 = int*, New playing status  */
} command_type_e;

//Data types
typedef enum
{
    LUA_END = 0,
    LUA_NUM,
    LUA_TEXT
} lua_datatype_e;

struct extension_sys_t
{
    /* Extension general */
    int i_capabilities;

    /* Lua specific */
    lua_State *L;

    vlclua_dtable_t dtable;

    /* Thread data */
    vlc_thread_t thread;
    vlc_mutex_t command_lock;
    vlc_mutex_t running_lock;
    vlc_cond_t wait;

    /* The item this extension should use for vlc.input
     * or NULL if it should use playlist's current input */
    struct input_item_t *p_item;

    extensions_manager_t *p_mgr;     ///< Parent
    /* Queue of commands to execute */
    struct command_t
    {
        command_type_e i_command;
        void *data[10];         ///< Optional void* arguments
        struct command_t *next; ///< Next command
    } *command;

    // The two following booleans are protected by command_lock
    vlc_dialog_id *p_progress_id;
    vlc_timer_t timer; ///< This timer makes sure Lua never gets stuck >5s

    bool b_exiting;

    bool b_thread_running; //< Only accessed out of the extension thread.
    bool b_activated; ///< Protected by the command lock
};

/* Extensions: manager functions */
int Activate( extensions_manager_t *p_mgr, extension_t * );
int Deactivate( extensions_manager_t *p_mgr, extension_t * );
bool QueueDeactivateCommand( extension_t *p_ext );
void KillExtension( extensions_manager_t *p_mgr, extension_t *p_ext );
int PushCommand__( extension_t *ext, bool unique, command_type_e cmd, va_list options );
static inline int PushCommand( extension_t *ext, int cmd, ... )
{
    va_list args;
    va_start( args, cmd );
    int i_ret = PushCommand__( ext, false, cmd, args );
    va_end( args );
    return i_ret;
}
static inline int PushCommandUnique( extension_t *ext, int cmd, ... )
{
    va_list args;
    va_start( args, cmd );
    int i_ret = PushCommand__( ext, true, cmd, args );
    va_end( args );
    return i_ret;
}

/* Lua specific functions */
void vlclua_extension_set( lua_State *L, extension_t * );
extension_t *vlclua_extension_get( lua_State *L );
int lua_ExtensionActivate( extensions_manager_t *, extension_t * );
int lua_ExtensionDeactivate( extensions_manager_t *, extension_t * );
int lua_ExecuteFunctionVa( extensions_manager_t *p_mgr, extension_t *p_ext,
                            const char *psz_function, va_list args );
int lua_ExecuteFunction( extensions_manager_t *p_mgr, extension_t *p_ext,
                         const char *psz_function, ... );
int lua_ExtensionWidgetClick( extensions_manager_t *p_mgr,
                              extension_t *p_ext,
                              extension_widget_t *p_widget );
int lua_ExtensionTriggerMenu( extensions_manager_t *p_mgr,
                              extension_t *p_ext, int id );

/* Dialog specific */
int lua_DialogFlush( lua_State *L );

#endif // LUA_EXTENSION_H
