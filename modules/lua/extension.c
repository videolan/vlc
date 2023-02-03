/*****************************************************************************
 * extension.c: Lua Extensions (meta data, web information, ...)
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc.h"
#include "libs.h"
#include "extension.h"
#include "assert.h"

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_events.h>
#include <vlc_dialog.h>
#include <vlc_player.h>

/* Functions to register */
static const luaL_Reg p_reg[] =
{
    { NULL, NULL }
};

/*
 * Extensions capabilities
 * Note: #define and ppsz_capabilities must be in sync
 */
static const char caps[][20] = {
    "menu",
    "trigger",
    "input-listener",
    "meta-listener",
    "playing-listener",
};

static int ScanExtensions( extensions_manager_t *p_this );
static int ScanLuaCallback( vlc_object_t *p_this, const char *psz_script,
                            const struct luabatch_context_t * );
static int Control( extensions_manager_t *, int, extension_t *, va_list );
static int GetMenuEntries( extensions_manager_t *p_mgr, extension_t *p_ext,
                    char ***pppsz_titles, uint16_t **ppi_ids );
static lua_State* GetLuaState( extensions_manager_t *p_mgr,
                               extension_t *p_ext );
static int TriggerMenu( extension_t *p_ext, int id );
static int TriggerExtension( extensions_manager_t *p_mgr,
                             extension_t *p_ext );
static void WatchTimerCallback( void* );

static int vlclua_extension_deactivate( lua_State *L );
static int vlclua_extension_keep_alive( lua_State *L );

/* Interactions */
static int vlclua_extension_dialog_callback( vlc_object_t *p_this,
                                             char const *psz_var,
                                             vlc_value_t oldval,
                                             vlc_value_t newval,
                                             void *p_data );

/**
 * Module entry-point
 **/
int Open_Extension( vlc_object_t *p_this )
{
    if( lua_Disabled( p_this ) )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "Opening Lua Extension module" );

    extensions_manager_t *p_mgr = ( extensions_manager_t* ) p_this;

    p_mgr->pf_control = Control;

    p_mgr->p_sys = NULL;
    vlc_mutex_init( &p_mgr->lock );

    /* Scan available Lua Extensions */
    if( ScanExtensions( p_mgr ) != VLC_SUCCESS )
    {
        msg_Err( p_mgr, "Can't load extensions modules" );
        return VLC_EGENERIC;
    }

    // Create the dialog-event variable
    var_Create( p_this, "dialog-event", VLC_VAR_ADDRESS );
    var_AddCallback( p_this, "dialog-event",
                     vlclua_extension_dialog_callback, NULL );

    return VLC_SUCCESS;
}

/**
 * Module unload function
 **/
void Close_Extension( vlc_object_t *p_this )
{
    extensions_manager_t *p_mgr = ( extensions_manager_t* ) p_this;

    var_DelCallback( p_this, "dialog-event",
                     vlclua_extension_dialog_callback, NULL );
    var_Destroy( p_mgr, "dialog-event" );

    extension_t *p_ext = NULL;

    /* Free extensions' memory */
    ARRAY_FOREACH( p_ext, p_mgr->extensions )
    {
        if( !p_ext )
            break;
        struct lua_extension *sys = p_ext->p_sys;

        vlc_mutex_lock(&sys->command_lock);
        if (sys->b_activated && sys->p_progress_id == NULL &&
            !sys->b_deactivating)
        {
            // QueueDeactivateCommand will signal the wait condition.
            sys->b_exiting = true;
            QueueDeactivateCommand( p_ext );
        }
        else
        {
            if (sys->L != NULL)
                vlclua_fd_interrupt(&sys->dtable);
            // however here we need to manually signal the wait cond, since no command is queued.
            sys->b_exiting = true;
            vlc_cond_signal(&sys->wait);
        }
        vlc_mutex_unlock(&sys->command_lock);

        if (sys->b_thread_running)
            vlc_join(sys->thread, NULL);

        /* Clear Lua State */
        if (sys->L)
        {
            lua_close(sys->L);
            vlclua_fd_cleanup(&sys->dtable);
        }

        free( p_ext->psz_name );
        free( p_ext->psz_title );
        free( p_ext->psz_author );
        free( p_ext->psz_description );
        free( p_ext->psz_shortdescription );
        free( p_ext->psz_url );
        free( p_ext->psz_version );
        free( p_ext->p_icondata );

        vlc_timer_destroy(sys->timer);

        free( p_ext->p_sys );
        free( p_ext );
    }

    ARRAY_RESET( p_mgr->extensions );
}

/**
 * Batch scan all Lua files in folder "extensions"
 * @param p_mgr This extensions_manager_t object
 **/
static int ScanExtensions( extensions_manager_t *p_mgr )
{
    int i_ret =
        vlclua_scripts_batch_execute( VLC_OBJECT( p_mgr ),
                                      "extensions",
                                      &ScanLuaCallback,
                                      NULL );

    if( !i_ret )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/**
 * Dummy Lua function: does nothing
 * @note This function can be used to replace "require" while scanning for
 * extensions
 * Even the built-in libraries are not loaded when calling descriptor()
 **/
static int vlclua_dummy_require( lua_State *L )
{
    (void) L;
    return 0;
}

/**
 * Replacement for "require", adding support for packaged extensions
 * @note Loads modules in the modules/ folder of a package
 * @note Try first with .luac and then with .lua
 **/
static int vlclua_extension_require( lua_State *L )
{
    const char *psz_module = luaL_checkstring( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    extension_t *p_ext = vlclua_extension_get( L );
    msg_Dbg( p_this, "loading module '%s' from extension package",
             psz_module );
    char *psz_fullpath, *psz_package, *sep;
    psz_package = strdup( p_ext->psz_name );
    sep = strrchr( psz_package, '/' );
    if( !sep )
    {
        free( psz_package );
        return luaL_error( L, "could not find package name" );
    }
    *sep = '\0';
    if( -1 == asprintf( &psz_fullpath,
                        "%s/modules/%s.luac", psz_package, psz_module ) )
    {
        free( psz_package );
        return 1;
    }
    int i_ret = vlclua_dofile( p_this, L, psz_fullpath );
    if( i_ret != 0 )
    {
        // Remove trailing 'c' --> try with .lua script
        psz_fullpath[ strlen( psz_fullpath ) - 1 ] = '\0';
        i_ret = vlclua_dofile( p_this, L, psz_fullpath );
    }
    free( psz_fullpath );
    free( psz_package );
    if( i_ret != 0 )
    {
        return luaL_error( L, "unable to load module '%s' from package",
                           psz_module );
    }
    return 0;
}

static char *
GetStringFieldOrNull(lua_State *L, const char *name)
{
    lua_getfield(L, -1, name);
    char *value = luaL_strdupornull(L, -1);
    lua_pop(L, 1);
    return value;
}

/**
 * Batch scan all Lua files in folder "extensions": callback
 * @param p_this This extensions_manager_t object
 * @param psz_filename Name of the script to run
 * @param L Lua State, common to all scripts here
 * @param dummy: unused
 **/
int ScanLuaCallback( vlc_object_t *p_this, const char *psz_filename,
                     const struct luabatch_context_t *dummy )
{
    VLC_UNUSED(dummy);
    extensions_manager_t *p_mgr = ( extensions_manager_t* ) p_this;
    bool b_ok = false;

    msg_Dbg( p_mgr, "Scanning Lua script %s", psz_filename );

    /* Experimental: read .vle packages (Zip archives) */
    char *psz_script = NULL;
    char *extension = strrchr(psz_filename, '.');
    if (extension != NULL && strcmp(extension, ".vle") == 0)
    {
        msg_Dbg( p_this, "reading Lua script in a zip archive" );
        if (asprintf(&psz_script, "zip://%s!/script.lua", psz_filename) == -1)
            return VLC_ENOMEM;
    }
    else
    {
        psz_script = strdup( psz_filename );
        if( !psz_script )
            return 0;
    }

    /* Create new script descriptor */
    extension_t *p_ext = calloc( 1, sizeof( extension_t ) );
    if( !p_ext )
    {
        free( psz_script );
        return 0;
    }

    p_ext->logger = vlc_object_logger(p_mgr);
    p_ext->psz_name = psz_script;
    struct lua_extension *sys
        = p_ext->p_sys
        = calloc(1, sizeof(*sys));
    if (sys == NULL || !p_ext->psz_name)
    {
        free( p_ext->psz_name );
        free(sys);
        free( p_ext );
        return 0;
    }
    sys->p_mgr = p_mgr;

    /* Watch timer */
    if( vlc_timer_create( &sys->timer, WatchTimerCallback, p_ext ) )
    {
        free( p_ext->psz_name );
        free(sys);
        free( p_ext );
        return 0;
    }

    /* Mutexes and conditions */
    vlc_mutex_init(&sys->command_lock);
    vlc_mutex_init(&sys->running_lock);
    vlc_cond_init(&sys->wait);

    /* Prepare Lua state */
    lua_State *L = luaL_newstate();
    lua_register( L, "require", &vlclua_dummy_require );

    /* Let's run it */
    if( vlclua_dofile( p_this, L, psz_script ) ) // luaL_dofile
    {
        msg_Warn( p_mgr, "Error loading script %s: %s", psz_script,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        goto exit;
    }

    /* Scan script for capabilities */
    lua_getglobal( L, "descriptor" );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function descriptor() not found", psz_script );
        goto exit;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function descriptor(): %s", psz_script,
                  lua_tostring( L, lua_gettop( L ) ) );
        goto exit;
    }

    if( lua_gettop( L ) )
    {
        if( lua_istable( L, -1 ) )
        {
            /* Get caps */
            lua_getfield( L, -1, "capabilities" );
            if( lua_istable( L, -1 ) )
            {
                lua_pushnil( L );
                while( lua_next( L, -2 ) != 0 )
                {
                    /* Key is at index -2 and value at index -1. Discard key */
                    const char *psz_cap = luaL_checkstring( L, -1 );
                    bool found = false;
                    /* Find this capability's flag */
                    for( size_t i = 0; i < ARRAY_SIZE(caps); i++ )
                    {
                        if( !strcmp( caps[i], psz_cap ) )
                        {
                            /* Flag it! */
                            sys->i_capabilities |= 1 << i;
                            found = true;
                            break;
                        }
                    }
                    if( !found )
                    {
                        msg_Warn( p_mgr, "Extension capability '%s' unknown in"
                                  " script %s", psz_cap, psz_script );
                    }
                    /* Removes 'value'; keeps 'key' for next iteration */
                    lua_pop( L, 1 );
                }
            }
            else
            {
                msg_Warn( p_mgr, "In script %s, function descriptor() "
                              "did not return a table of capabilities.",
                              psz_script );
            }
            lua_pop( L, 1 );

            /* Get title */
            lua_getfield( L, -1, "title" );
            if( lua_isstring( L, -1 ) )
            {
                p_ext->psz_title = strdup( luaL_checkstring( L, -1 ) );
            }
            else
            {
                msg_Dbg( p_mgr, "In script %s, function descriptor() "
                                "did not return a string as title.",
                                psz_script );
                p_ext->psz_title = strdup( psz_script );
            }
            lua_pop( L, 1 );

            /* Get the fields from the extension manifest. */
            p_ext->psz_author = GetStringFieldOrNull(L, "author");
            p_ext->psz_description = GetStringFieldOrNull(L, "description");
            p_ext->psz_shortdescription = GetStringFieldOrNull(L, "shortdesc");
            p_ext->psz_url = GetStringFieldOrNull(L, "url");
            p_ext->psz_version = GetStringFieldOrNull(L, "version");
            /* Get icon data */
            lua_getfield( L, -1, "icon" );
            if( !lua_isnil( L, -1 ) && lua_isstring( L, -1 ) )
            {
                int len = lua_strlen( L, -1 );
                p_ext->p_icondata = malloc( len );
                if( p_ext->p_icondata )
                {
                    p_ext->i_icondata_size = len;
                    memcpy( p_ext->p_icondata, lua_tostring( L, -1 ), len );
                }
            }
            lua_pop( L, 1 );
        }
        else
        {
            msg_Warn( p_mgr, "In script %s, function descriptor() "
                      "did not return a table!", psz_script );
            goto exit;
        }
    }
    else
    {
        msg_Err( p_mgr, "Script %s went completely foobar", psz_script );
        goto exit;
    }

    msg_Dbg(p_mgr, "Script %s has the following capability flags: 0x%x",
            psz_script, sys->i_capabilities);

    b_ok = true;
exit:
    lua_close( L );
    if( !b_ok )
    {
        free( p_ext->psz_name );
        free( p_ext->psz_title );
        free( p_ext->psz_url );
        free( p_ext->psz_author );
        free( p_ext->psz_description );
        free( p_ext->psz_shortdescription );
        free( p_ext->psz_version );
        free(sys);
        free( p_ext );
    }
    else
    {
        /* Add the extension to the list of known extensions */
        ARRAY_APPEND( p_mgr->extensions, p_ext );
    }

    /* Continue batch execution */
    return VLC_EGENERIC;
}

static int Control(extensions_manager_t *p_mgr, int i_control,
                   extension_t *ext, va_list args)
{
    struct lua_extension *sys = ext->p_sys;

    bool *pb = NULL;
    uint16_t **ppus = NULL;
    char ***pppsz = NULL;
    int i = 0;

    switch( i_control )
    {
        case EXTENSION_ACTIVATE:
            return Activate(ext);

        case EXTENSION_DEACTIVATE:
            return Deactivate(p_mgr, ext);

        case EXTENSION_IS_ACTIVATED:
            pb = va_arg( args, bool* );
            vlc_mutex_lock(&sys->command_lock);
            *pb = sys->b_activated;
            vlc_mutex_unlock(&sys->command_lock);
            break;

        case EXTENSION_HAS_MENU:
            pb = va_arg( args, bool* );
            *pb = (sys->i_capabilities & EXT_HAS_MENU) ? 1 : 0;
            break;

        case EXTENSION_GET_MENU:
            pppsz = va_arg( args, char*** );
            ppus = va_arg( args, uint16_t** );
            return GetMenuEntries(p_mgr, ext, pppsz, ppus);

        case EXTENSION_TRIGGER_ONLY:
            pb = va_arg( args, bool* );
            *pb = (sys->i_capabilities & EXT_TRIGGER_ONLY) ? 1 : 0;
            break;

        case EXTENSION_TRIGGER:
            return TriggerExtension(p_mgr, ext);

        case EXTENSION_TRIGGER_MENU:
            i = va_arg( args, int );
            return TriggerMenu(ext, i);

        case EXTENSION_SET_INPUT:
        {
            input_item_t *p_item = va_arg( args, struct input_item_t * );

            vlc_mutex_lock(&sys->command_lock);
            if (sys->b_exiting)
            {
                vlc_mutex_unlock(&sys->command_lock);
                return VLC_EGENERIC;
            }
            vlc_mutex_unlock(&sys->command_lock);

            vlc_mutex_lock(&sys->running_lock);

            // Change input
            input_item_t *old = sys->p_item;
            if( old )
                input_item_Release( old );

            sys->p_item = p_item ? input_item_Hold(p_item) : NULL;

            // Tell the script the input changed
            if (sys->i_capabilities & EXT_INPUT_LISTENER)
            {
                PushCommandUnique(ext, CMD_SET_INPUT);
            }

            vlc_mutex_unlock(&sys->running_lock);
            break;
        }
        case EXTENSION_PLAYING_CHANGED:
        {
            assert(ext->psz_name != NULL);
            i = va_arg( args, int );
            if (sys->i_capabilities & EXT_PLAYING_LISTENER)
            {
                PushCommand(ext, CMD_PLAYING_CHANGED, i);
            }
            break;
        }
        case EXTENSION_META_CHANGED:
        {
            ext = va_arg( args, extension_t* );
            PushCommand(ext, CMD_UPDATE_META);
            break;
        }
        default:
            msg_Warn( p_mgr, "Control '%d' not yet implemented in Extension",
                      i_control );
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

int lua_ExtensionActivate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    assert( p_mgr != NULL && p_ext != NULL );
    return lua_ExecuteFunction( p_mgr, p_ext, "activate", LUA_END );
}

int lua_ExtensionDeactivate( extensions_manager_t *p_mgr, extension_t *p_ext )
{
    assert( p_mgr != NULL && p_ext != NULL );
    struct lua_extension *sys = p_ext->p_sys;

    vlclua_fd_interrupt(&sys->dtable);

    // Unset and release input objects
    if (sys->p_item)
    {
        input_item_Release(sys->p_item);
        sys->p_item = NULL;
    }

    return lua_ExecuteFunction( p_mgr, p_ext, "deactivate", LUA_END );
}

int lua_ExtensionWidgetClick( extensions_manager_t *p_mgr,
                              extension_t *p_ext,
                              extension_widget_t *p_widget )
{
    struct lua_extension *sys = p_ext->p_sys;
    if (sys->L == NULL)
        return VLC_SUCCESS;

    lua_State *L = GetLuaState( p_mgr, p_ext );
    lua_pushlightuserdata( L, p_widget );
    lua_gettable( L, LUA_REGISTRYINDEX );
    return lua_ExecuteFunction( p_mgr, p_ext, NULL, LUA_END );
}


/**
 * Get the list of menu entries from an extension script
 * @param p_mgr
 * @param p_ext
 * @param pppsz_titles Pointer to NULL. All strings must be freed by the caller
 * @param ppi_ids Pointer to NULL. Must be freed by the caller.
 * @note This function is allowed to run in the UI thread. This means
 *       that it MUST respond very fast.
 * @todo Remove the menu() hook and provide a new function vlc.set_menu()
 **/
static int GetMenuEntries( extensions_manager_t *p_mgr, extension_t *p_ext,
                    char ***pppsz_titles, uint16_t **ppi_ids )
{
    assert( *pppsz_titles == NULL );
    assert( *ppi_ids == NULL );
    struct lua_extension *sys = p_ext->p_sys;

    vlc_mutex_lock(&sys->command_lock);
    if (!sys->b_activated || sys->b_exiting)
    {
        vlc_mutex_unlock(&sys->command_lock);
        msg_Dbg( p_mgr, "Can't get menu of an unactivated/dying extension!" );
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&sys->command_lock);

    vlc_mutex_lock(&sys->running_lock);

    int i_ret = VLC_EGENERIC;
    lua_State *L = GetLuaState( p_mgr, p_ext );

    if ((sys->i_capabilities & EXT_HAS_MENU) == 0)
    {
        msg_Dbg( p_mgr, "can't get a menu from an extension without menu!" );
        goto exit;
    }

    lua_getglobal( L, "menu" );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function menu() not found", p_ext->psz_name );
        goto exit;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function menu(): %s", p_ext->psz_name,
                  lua_tostring( L, lua_gettop( L ) ) );
        goto exit;
    }

    if (lua_gettop(L) == 0)
    {
        msg_Warn(p_mgr, "Script %s went completely foobar", p_ext->psz_name);
        goto exit;
    }

    if (!lua_istable(L, -1))
    {
        msg_Warn(p_mgr, "Function menu() in script %s "
                 "did not return a table", p_ext->psz_name);
        goto exit;
    }

    /* Get table size */
    size_t i_size = lua_objlen( L, -1 );
    *pppsz_titles = ( char** ) calloc( i_size+1, sizeof( char* ) );
    *ppi_ids = ( uint16_t* ) calloc( i_size+1, sizeof( uint16_t ) );

    /* Walk table */
    size_t i_idx = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        assert(i_idx < i_size);
        if(!lua_isstring(L, -1) || !lua_isnumber(L, -2))
        {
            msg_Warn(p_mgr, "In script %s, an entry in "
                     "the menu table is invalid!", p_ext->psz_name);
            goto exit;
        }
        (*pppsz_titles)[i_idx] = strdup(luaL_checkstring(L, -1));
        (*ppi_ids)[i_idx] = luaL_checkinteger(L, -2) & 0xFFFF;
        i_idx++;
        lua_pop(L, 1);
    }

    i_ret = VLC_SUCCESS;

exit:
    vlc_mutex_unlock(&sys->running_lock);
    if( i_ret != VLC_SUCCESS )
    {
        msg_Dbg( p_mgr, "Something went wrong in %s (%s:%d)",
                 __func__, __FILE__, __LINE__ );
    }
    return i_ret;
}

/* Must be entered with the Lock on Extension */
static lua_State* GetLuaState( extensions_manager_t *p_mgr,
                               extension_t *p_ext )
{
    assert( p_ext != NULL );
    struct lua_extension *sys = p_ext->p_sys;
    lua_State *L = sys->L;

    if( !L )
    {
        L = luaL_newstate();
        if( !L )
        {
            msg_Err( p_mgr, "Could not create new Lua State" );
            return NULL;
        }
        vlclua_set_this( L, p_mgr );
        intf_thread_t *intf = (intf_thread_t *) vlc_object_parent(p_mgr);
        vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);
        vlclua_set_playlist_internal(L, playlist);
        vlclua_extension_set( L, p_ext );

        luaL_openlibs( L );
        luaL_register_namespace( L, "vlc", p_reg );
        luaopen_msg( L );

        /* Load more libraries */
        luaopen_config( L );
        luaopen_dialog( L, p_ext );
        luaopen_input( L );
        luaopen_msg( L );
        if (vlclua_fd_init(L, &sys->dtable))
        {
            lua_close( L );
            return NULL;
        }
        luaopen_object( L );
        luaopen_osd( L );
        luaopen_playlist( L );
        luaopen_stream( L );
        luaopen_strings( L );
        luaopen_variables( L );
        luaopen_video( L );
        luaopen_vlm( L );
        luaopen_volume( L );
        luaopen_xml( L );
        luaopen_vlcio( L );
        luaopen_errno( L );
        luaopen_rand( L );
        luaopen_rd( L );
        luaopen_ml( L );
#if defined(_WIN32) && !defined(VLC_WINSTORE_APP)
        luaopen_win( L );
#endif

        /* Register extension specific functions */
        lua_getglobal( L, "vlc" );
        lua_pushcfunction( L, vlclua_extension_deactivate );
        lua_setfield( L, -2, "deactivate" );
        lua_pushcfunction( L, vlclua_extension_keep_alive );
        lua_setfield( L, -2, "keep_alive" );

        /* Setup the module search path */
        if( !strncmp( p_ext->psz_name, "zip://", 6 ) )
        {
            /* Load all required modules manually */
            lua_register( L, "require", &vlclua_extension_require );
        }
        else if (vlclua_add_modules_path(L, p_ext->psz_name))
        {
            msg_Warn(p_mgr, "Error while setting the module "
                     "search path for %s", p_ext->psz_name);
            vlclua_fd_cleanup(&sys->dtable);
            lua_close(L);
            return NULL;
        }

        /* Load and run the script(s) */
        if( vlclua_dofile( VLC_OBJECT( p_mgr ), L, p_ext->psz_name ) )
        {
            msg_Warn( p_mgr, "Error loading script %s: %s", p_ext->psz_name,
                      lua_tostring( L, lua_gettop( L ) ) );
            vlclua_fd_cleanup(&sys->dtable);
            lua_close( L );
            return NULL;
        }

        sys->L = L;
    }

    return L;
}

int lua_ExecuteFunction( extensions_manager_t *p_mgr, extension_t *p_ext,
                            const char *psz_function, ... )
{
    va_list args;
    va_start( args, psz_function );
    int i_ret = lua_ExecuteFunctionVa( p_mgr, p_ext, psz_function, args );
    va_end( args );
    return i_ret;
}

/**
 * Execute a function in a Lua script
 * @param psz_function Name of global function to execute. If NULL, assume
 *                     that the function object is already on top of the
 *                     stack.
 * @return < 0 in case of failure, >= 0 in case of success
 * @note It's better to call this function from a dedicated thread
 * (see extension_thread.c)
 **/
int lua_ExecuteFunctionVa( extensions_manager_t *p_mgr, extension_t *p_ext,
                           const char *psz_function, va_list args )
{
    int i_ret = VLC_SUCCESS;
    int i_args = 0;
    assert( p_mgr != NULL );
    assert( p_ext != NULL );

    lua_State *L = GetLuaState( p_mgr, p_ext );
    if( !L )
        return -1;

    if( psz_function )
        lua_getglobal( L, psz_function );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function %s() not found", p_ext->psz_name, psz_function );
        lua_pop( L, 1 );
        goto exit;
    }

    lua_datatype_e type = LUA_END;
    while( ( type = va_arg( args, int ) ) != LUA_END )
    {
        if( type == LUA_NUM )
        {
            lua_pushnumber( L , va_arg( args, int ) );
        }
        else if( type == LUA_TEXT )
        {
            lua_pushstring( L , va_arg( args, char* ) );
        }
        else
        {
            msg_Warn( p_mgr, "Undefined argument type %d to lua function %s"
                   "from script %s", type, psz_function, p_ext->psz_name );
            if( i_args > 0 )
                lua_pop( L, i_args );
            goto exit;
        }
        i_args ++;
    }

    // Start actual call to Lua
    if( lua_pcall( L, i_args, 1, 0 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function %s(): %s", p_ext->psz_name, psz_function,
                  lua_tostring( L, lua_gettop( L ) ) );
        i_ret = VLC_EGENERIC;
    }

    i_ret |= lua_DialogFlush( L );

exit:
    return i_ret;

}

static inline int TriggerMenu( extension_t *p_ext, int i_id )
{
    return PushCommand( p_ext, CMD_TRIGGERMENU, i_id );
}

int lua_ExtensionTriggerMenu( extensions_manager_t *p_mgr,
                              extension_t *p_ext, int id )
{
    int i_ret = VLC_SUCCESS;
    lua_State *L = GetLuaState( p_mgr, p_ext );

    if( !L )
        return VLC_EGENERIC;

    luaopen_dialog( L, p_ext );

    lua_getglobal( L, "trigger_menu" );
    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function trigger_menu() not found", p_ext->psz_name );
        return VLC_EGENERIC;
    }

    /* Pass id as unique argument to the function */
    lua_pushinteger( L, id );

    if( lua_pcall( L, 1, 1, 0 ) != 0 )
    {
        msg_Warn( p_mgr, "Error while running script %s, "
                  "function trigger_menu(): %s", p_ext->psz_name,
                  lua_tostring( L, lua_gettop( L ) ) );
        i_ret = VLC_EGENERIC;
    }

    i_ret |= lua_DialogFlush( L );
    if( i_ret < VLC_SUCCESS )
    {
        msg_Dbg( p_mgr, "Something went wrong in %s (%s:%d)",
                 __func__, __FILE__, __LINE__ );
    }

    return i_ret;
}

/** Directly trigger an extension, without activating it
 * This is NOT multithreaded, and this code runs in the UI thread
 * @param p_mgr
 * @param p_ext Extension to trigger
 * @return Value returned by the lua function "trigger"
 **/
static int TriggerExtension( extensions_manager_t *p_mgr,
                             extension_t *p_ext )
{
    struct lua_extension *sys = p_ext->p_sys;

    int i_ret = lua_ExecuteFunction( p_mgr, p_ext, "trigger", LUA_END );

    /* Close lua state for trigger-only extensions */
    if (sys->L)
    {
        vlclua_fd_cleanup(&sys->dtable);
        lua_close(sys->L);
    }
    sys->L = NULL;

    return i_ret;
}

/** Set extension associated to the current script
 * @param L current lua_State
 * @param p_ext the extension
 */
void vlclua_extension_set( lua_State *L, extension_t *p_ext )
{
    lua_pushlightuserdata( L, vlclua_extension_set );
    lua_pushlightuserdata( L, p_ext );
    lua_rawset( L, LUA_REGISTRYINDEX );
}

/** Retrieve extension associated to the current script
 * @param L current lua_State
 * @return Extension pointer
 **/
extension_t *vlclua_extension_get( lua_State *L )
{
    lua_pushlightuserdata( L, vlclua_extension_set );
    lua_rawget( L, LUA_REGISTRYINDEX );
    extension_t *p_ext = (extension_t*) lua_topointer( L, -1 );
    lua_pop( L, 1 );
    return p_ext;
}

/** Deactivate an extension by order from the extension itself
 * @param L lua_State
 * @note This is an asynchronous call. A script calling vlc.deactivate() will
 * be executed to the end before the last call to deactivate() is done.
 **/
int vlclua_extension_deactivate( lua_State *L )
{
    extension_t *p_ext = vlclua_extension_get( L );
    struct lua_extension *sys = p_ext->p_sys;

    vlc_mutex_lock(&sys->command_lock);
    bool b_ret = QueueDeactivateCommand( p_ext );
    vlc_mutex_unlock(&sys->command_lock);
    return ( b_ret == true ) ? 1 : 0;
}

/** Keep an extension alive. This resets the watch timer to 0
 * @param L lua_State
 * @note This is the "vlc.keep_alive()" function
 **/
int vlclua_extension_keep_alive( lua_State *L )
{
    extension_t *p_ext = vlclua_extension_get( L );
    struct lua_extension *sys = p_ext->p_sys;

    vlc_mutex_lock(&sys->command_lock);
    if (sys->p_progress_id != NULL)
    {
        vlc_dialog_release(sys->p_mgr, sys->p_progress_id);
        sys->p_progress_id = NULL;
    }
    vlc_timer_schedule(sys->timer, false, WATCH_TIMER_PERIOD,
                       VLC_TIMER_FIRE_ONCE);
    vlc_mutex_unlock(&sys->command_lock);

    return 1;
}

/** Callback for the variable "dialog-event"
 * @param p_this Current object owner of the extension and the dialog
 * @param psz_var "dialog-event"
 * @param oldval Unused
 * @param newval Address of the dialog
 * @param p_data Unused
 **/
static int vlclua_extension_dialog_callback( vlc_object_t *p_this,
                                             char const *psz_var,
                                             vlc_value_t oldval,
                                             vlc_value_t newval,
                                             void *p_data )
{
    /* psz_var == "dialog-event" */
    ( void ) psz_var;
    ( void ) oldval;
    ( void ) p_data;

    extension_dialog_command_t *command = newval.p_address;
    assert( command != NULL );
    assert( command->p_dlg != NULL);

    extension_t *p_ext = command->p_dlg->p_sys;
    assert( p_ext != NULL );

    extension_widget_t *p_widget = command->p_data;

    switch( command->event )
    {
        case EXTENSION_EVENT_CLICK:
            assert( p_widget != NULL );
            PushCommandUnique( p_ext, CMD_CLICK, p_widget );
            break;
        case EXTENSION_EVENT_CLOSE:
            PushCommandUnique( p_ext, CMD_CLOSE );
            break;
        default:
            msg_Dbg( p_this, "Received unknown UI event %d, discarded",
                     command->event );
            break;
    }

    return VLC_SUCCESS;
}

/** Watch timer callback
 * The timer expired, Lua may be stuck, ask the user what to do now
 **/
static void WatchTimerCallback( void *data )
{
    extension_t *p_ext = data;
    struct lua_extension *sys = p_ext->p_sys;
    extensions_manager_t *p_mgr = sys->p_mgr;

    vlc_mutex_lock(&sys->command_lock);

    for( struct command_t *cmd = sys->command;
         cmd != NULL;
         cmd = cmd->next )
        if( cmd->i_command == CMD_DEACTIVATE )
        {   /* We have a pending Deactivate command... */
            if (sys->p_progress_id != NULL)
            {
                vlc_dialog_release(p_mgr, sys->p_progress_id);
                sys->p_progress_id = NULL;
            }
            KillExtension(p_ext);
            vlc_mutex_unlock(&sys->command_lock);
            return;
        }

    if (sys->p_progress_id == NULL)
    {
        sys->p_progress_id =
            vlc_dialog_display_progress( p_mgr, true, 0.0,
                                         _( "Yes" ),
                                         _( "Extension not responding!" ),
                                         _( "Extension '%s' does not respond.\n"
                                         "Do you want to kill it now? " ),
                                         p_ext->psz_title );
        if (sys->p_progress_id == NULL)
        {
            KillExtension(p_ext);
            vlc_mutex_unlock(&sys->command_lock);
            return;
        }
        vlc_timer_schedule(sys->timer, false, VLC_TICK_FROM_MS(100),
                           VLC_TIMER_FIRE_ONCE);
    }
    else
    {
        if (vlc_dialog_is_cancelled(p_mgr, sys->p_progress_id))
        {
            vlc_dialog_release(p_mgr, sys->p_progress_id);
            sys->p_progress_id = NULL;
            KillExtension(p_ext);
            vlc_mutex_unlock(&sys->command_lock);
            return;
        }
        vlc_timer_schedule(sys->timer, false, VLC_TICK_FROM_MS(100),
                           VLC_TIMER_FIRE_ONCE);
    }
    vlc_mutex_unlock(&sys->command_lock);
}
