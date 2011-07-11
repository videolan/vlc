/*****************************************************************************
 * variables.c: Generic lua<->vlc variables interface
 *****************************************************************************
 * Copyright (C) 2007-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "../vlc.h"
#include "../libs.h"
#include "variables.h"
#include "objects.h"

/*****************************************************************************
 * Variables handling
 *****************************************************************************/
static int vlclua_pushvalue( lua_State *L, int i_type, vlc_value_t val, bool b_error_void )
{
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_VOID:
            if( b_error_void )
                vlclua_error( L );
            else
                lua_pushnil( L );
            break;
        case VLC_VAR_BOOL:
            lua_pushboolean( L, val.b_bool );
            break;
        case VLC_VAR_INTEGER:
            lua_pushinteger( L, val.i_int );
            break;
        case VLC_VAR_STRING:
            lua_pushstring( L, val.psz_string );
            break;
        case VLC_VAR_FLOAT:
            lua_pushnumber( L, val.f_float );
            break;
        case VLC_VAR_TIME:
            /* FIXME? (we're losing some precision, but does it really matter?) */
            lua_pushnumber( L, ((double)val.i_time)/1000000. );
            break;
        case VLC_VAR_ADDRESS:
            vlclua_error( L );
            break;
        case VLC_VAR_MUTEX:
            vlclua_error( L );
            break;
        default:
            vlclua_error( L );
    }
    return 1;
}

static int vlclua_pushlist( lua_State *L, vlc_list_t *p_list )
{
    int i_count = p_list->i_count;

    lua_createtable( L, i_count, 0 );
    for( int i = 0; i < i_count; i++ )
    {
        lua_pushinteger( L, i+1 );
        if( !vlclua_pushvalue( L, p_list->pi_types[i],
                               p_list->p_values[i], true ) )
             lua_pushnil( L );
        lua_settable( L, -3 );
    }
    return 1;
}

static int vlclua_tovalue( lua_State *L, int i_type, vlc_value_t *val )
{
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_VOID:
            break;
        case VLC_VAR_BOOL:
            val->b_bool = luaL_checkboolean( L, -1 );
            break;
        case VLC_VAR_INTEGER:
            val->i_int = luaL_checkint( L, -1 );
            break;
        case VLC_VAR_STRING:
            val->psz_string = (char*)luaL_checkstring( L, -1 ); /* XXX: Beware, this only stays valid as long as (L,-1) stays in the stack */
            break;
        case VLC_VAR_FLOAT:
            val->f_float = luaL_checknumber( L, -1 );
            break;
        case VLC_VAR_TIME:
            {
                double f = luaL_checknumber( L, -1 );
                val->i_time = (int64_t)(f*1000000.);
            }
            break;
        case VLC_VAR_ADDRESS:
            vlclua_error( L );
            break;
        case VLC_VAR_MUTEX:
            vlclua_error( L );
            break;
        default:
            vlclua_error( L );
    }
    return 1;
}

static int vlclua_var_get( lua_State *L )
{
    vlc_value_t val;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    int i_type = var_Type( *pp_obj, psz_var );
    if( var_Get( *pp_obj, psz_var, &val ) != VLC_SUCCESS )
        return 0;

    lua_pop( L, 2 );
    return vlclua_pushvalue( L, i_type, val, true );
}

static int vlclua_var_set( lua_State *L )
{
    vlc_value_t val;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    int i_type = var_Type( *pp_obj, psz_var );
    vlclua_tovalue( L, i_type, &val );

    int i_ret = var_Set( *pp_obj, psz_var, val );

    lua_pop( L, 3 );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_var_create( lua_State *L )
{
    int i_type, i_ret;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    switch( lua_type( L, 3 ) )
    {
        case LUA_TNUMBER:
            i_type = VLC_VAR_FLOAT;
            break;
        case LUA_TBOOLEAN:
            i_type = VLC_VAR_BOOL;
            break;
        case LUA_TSTRING:
            i_type = VLC_VAR_STRING;
            break;
        case LUA_TNIL:
            i_type = VLC_VAR_VOID;
            break;
        default:
            return 0;
    }

    if( ( i_ret = var_Create( *pp_obj, psz_var, i_type ) ) != VLC_SUCCESS )
        return vlclua_push_ret( L, i_ret );

    // Special case for void variables
    if( i_type == VLC_VAR_VOID )
        return 0;

    vlc_value_t val;
    vlclua_tovalue( L, i_type, &val );
    return vlclua_push_ret( L, var_Set( *pp_obj, psz_var, val ) );
}

static int vlclua_var_get_list( lua_State *L )
{
    vlc_value_t val;
    vlc_value_t text;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    int i_ret = var_Change( *pp_obj, psz_var, VLC_VAR_GETLIST, &val, &text );
    if( i_ret < 0 )
        return vlclua_push_ret( L, i_ret );

    vlclua_pushlist( L, val.p_list );
    vlclua_pushlist( L, text.p_list );

    var_FreeList( &val, &text );
    return 2;
}

static int vlclua_command( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    char *psz_msg;

    const char *psz_name = luaL_checkstring( L, 1 );
    const char *psz_cmd = luaL_checkstring( L, 2 );
    const char *psz_arg = luaL_checkstring( L, 3 );
    int ret = var_Command( p_this, psz_name, psz_cmd, psz_arg, &psz_msg );
    lua_pop( L, 3 );

    if( psz_msg )
    {
        lua_pushstring( L, psz_msg );
        free( psz_msg );
    }
    else
    {
        lua_pushliteral( L, "" );
    }
    return vlclua_push_ret( L, ret ) + 1;
}

static int vlclua_libvlc_command( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    vlc_value_t val_arg;

    const char *psz_cmd = luaL_checkstring( L, 1 );
    val_arg.psz_string = (char*)luaL_optstring( L, 2, "" );

    int i_type = var_Type( p_this->p_libvlc, psz_cmd );
    if( ! (i_type & VLC_VAR_ISCOMMAND) )
    {
        return luaL_error( L, "libvlc's \"%s\" is not a command",
                           psz_cmd );
    }

    int i_ret = var_Set( p_this->p_libvlc, psz_cmd, val_arg );
    lua_pop( L, 2 );

    return vlclua_push_ret( L, i_ret );
}

#undef vlclua_var_toggle_or_set
int vlclua_var_toggle_or_set( lua_State *L, vlc_object_t *p_obj,
                              const char *psz_name )
{
    bool b_bool;
    if( lua_gettop( L ) > 1 ) return vlclua_error( L );

    if( lua_gettop( L ) == 0 )
        b_bool = var_ToggleBool( p_obj, psz_name );
    else /* lua_gettop( L ) == 1 */
    {
        b_bool = luaL_checkboolean( L, -1 );
        lua_pop( L, 1 );
        if( b_bool != var_GetBool( p_obj, psz_name ) )
            var_SetBool( p_obj, psz_name, b_bool );
    }

    lua_pushboolean( L, b_bool );
    return 1;
}

static inline const void *luaL_checklightuserdata( lua_State *L, int narg )
{
    luaL_checktype( L, narg, LUA_TLIGHTUSERDATA ); /* can raise an error */
    return lua_topointer( L, narg );
}

typedef struct
{
    int i_index;
    int i_type;
    lua_State *L;
}   vlclua_callback_t;

static int vlclua_callback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    vlclua_callback_t *p_callback = (vlclua_callback_t*)p_data;
    lua_State *L = p_callback->L;

    /* <empty stack> */
    lua_getglobal( L, "vlc" );
    /* vlc */
    lua_getfield( L, -1, "callbacks" );
    /* vlc callbacks */
    lua_remove( L, -2 );
    /* callbacks */
    lua_pushinteger( L, p_callback->i_index );
    /* callbacks index */
    lua_gettable( L, -2 );
    /* callbacks callbacks[index] */
    lua_remove( L, -2 );
    /* callbacks[index] */
    lua_getfield( L, -1, "callback" );
    /* callbacks[index] callback */
    lua_pushstring( L, psz_var );
    /* callbacks[index] callback var */
    vlclua_pushvalue( L, p_callback->i_type, oldval, false );
    /* callbacks[index] callback var oldval */
    vlclua_pushvalue( L, p_callback->i_type, newval, false );
    /* callbacks[index] callback var oldval newval */
    lua_getfield( L, -5, "data" );
    /* callbacks[index] callback var oldval newval data */
    lua_remove( L, -6 );
    /* callback var oldval newval data */

    if( lua_pcall( L, 4, 0, 0 ) )
    {
        /* errormessage */
        const char *psz_err = lua_tostring( L, -1 );
        msg_Err( p_this, "Error while running lua interface callback: %s",
                 psz_err );
        /* empty the stack (should only contain the error message) */
        lua_settop( L, 0 );
        return VLC_EGENERIC;
    }

    /* empty the stack (should already be empty) */
    lua_settop( L, 0 );

    return VLC_SUCCESS;
}

static int vlclua_add_callback( lua_State *L )
{
    vlclua_callback_t *p_callback;
    static int i_index = 0;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    lua_settop( L, 4 ); /* makes sure that optional data arg is set */
    if( !lua_isfunction( L, 3 ) )
        return vlclua_error( L );

    if( !pp_obj || !*pp_obj )
        return vlclua_error( L );

    int i_type = var_Type( *pp_obj, psz_var );

    /* Check variable type, in order to avoid PANIC */
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_INTEGER:
        case VLC_VAR_STRING:
        case VLC_VAR_FLOAT:
        case VLC_VAR_TIME:
            break;
        case VLC_VAR_ADDRESS:
        case VLC_VAR_MUTEX:
        default:
            return vlclua_error( L );
    }

    i_index++;

    p_callback = (vlclua_callback_t*)malloc( sizeof( vlclua_callback_t ) );
    if( !p_callback )
        return vlclua_error( L );

    /* obj var func data */
    lua_getglobal( L, "vlc" );
    /* obj var func data vlc */
    lua_getfield( L, -1, "callbacks" );
    if( lua_isnil( L, -1 ) )
    {
        lua_pop( L, 1 );
        lua_newtable( L );
        lua_setfield( L, -2, "callbacks" );
        lua_getfield( L, -1, "callbacks" );
    }
    /* obj var func data vlc callbacks */
    lua_remove( L, -2 );
    /* obj var func data callbacks */
    lua_pushinteger( L, i_index );
    /* obj var func data callbacks index */
    lua_insert( L, -4 );
    /* obj var index func data callbacks */
    lua_insert( L, -4 );
    /* obj var callbacks index func data */
    lua_createtable( L, 0, 0 );
    /* obj var callbacks index func data cbtable */
    lua_insert( L, -2 );
    /* obj var callbacks index func cbtable data */
    lua_setfield( L, -2, "data" );
    /* obj var callbacks index func cbtable */
    lua_insert( L, -2 );
    /* obj var callbacks index cbtable func */
    lua_setfield( L, -2, "callback" );
    /* obj var callbacks index cbtable */
    lua_pushlightuserdata( L, *pp_obj ); /* will be needed in vlclua_del_callback */
    /* obj var callbacks index cbtable p_obj */
    lua_setfield( L, -2, "private1" );
    /* obj var callbacks index cbtable */
    lua_pushvalue( L, 2 ); /* will be needed in vlclua_del_callback */
    /* obj var callbacks index cbtable var */
    lua_setfield( L, -2, "private2" );
    /* obj var callbacks index cbtable */
    lua_pushlightuserdata( L, p_callback ); /* will be needed in vlclua_del_callback */
    /* obj var callbacks index cbtable p_callback */
    lua_setfield( L, -2, "private3" );
    /* obj var callbacks index cbtable */
    lua_settable( L, -3 );
    /* obj var callbacks */
    lua_pop( L, 3 );
    /* <empty stack> */

    /* Do not move this before the lua specific code (it somehow changes
     * the function in the stack to nil) */
    p_callback->i_index = i_index;
    p_callback->i_type = i_type;
    p_callback->L = lua_newthread( L ); /* Do we have to keep a reference to this thread somewhere to prevent garbage collection? */

    var_AddCallback( *pp_obj, psz_var, vlclua_callback, p_callback );
    return 0;
}

static int vlclua_del_callback( lua_State *L )
{
    vlclua_callback_t *p_callback;
    bool b_found = false;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    lua_settop( L, 4 ); /* makes sure that optional data arg is set */
    if( !lua_isfunction( L, 3 ) )
        return vlclua_error( L );

    /* obj var func data */
    lua_getglobal( L, "vlc" );
    /* obj var func data vlc */
    lua_getfield( L, -1, "callbacks" );
    if( lua_isnil( L, -1 ) )
        return luaL_error( L, "Couldn't find matching callback." );
    /* obj var func data vlc callbacks */
    lua_remove( L, -2 );
    /* obj var func data callbacks */
    lua_pushnil( L );
    /* obj var func data callbacks index */
    while( lua_next( L, -2 ) )
    {
        /* obj var func data callbacks index value */
        if( lua_isnumber( L, -2 ) )
        {
            lua_getfield( L, -1, "private2" );
            /* obj var func data callbacks index value private2 */
            if( lua_equal( L, 2, -1 ) ) /* var name is equal */
            {
                lua_pop( L, 1 );
                /* obj var func data callbacks index value */
                lua_getfield( L, -1, "callback" );
                /* obj var func data callbacks index value callback */
                if( lua_equal( L, 3, -1 ) ) /* callback function is equal */
                {
                    lua_pop( L, 1 );
                    /* obj var func data callbacks index value */
                    lua_getfield( L, -1, "data" ); /* callback data is equal */
                    /* obj var func data callbacks index value data */
                    if( lua_equal( L, 4, -1 ) )
                    {
                        vlc_object_t *p_obj2;
                        lua_pop( L, 1 );
                        /* obj var func data callbacks index value */
                        lua_getfield( L, -1, "private1" );
                        /* obj var func data callbacks index value private1 */
                        p_obj2 = (vlc_object_t*)luaL_checklightuserdata( L, -1 );
                        if( p_obj2 == *pp_obj ) /* object is equal */
                        {
                            lua_pop( L, 1 );
                            /* obj var func data callbacks index value */
                            lua_getfield( L, -1, "private3" );
                            /* obj var func data callbacks index value private3 */
                            p_callback = (vlclua_callback_t*)luaL_checklightuserdata( L, -1 );
                            lua_pop( L, 2 );
                            /* obj var func data callbacks index */
                            b_found = true;
                            break;
                        }
                        else
                        {
                            /* obj var func data callbacks index value private1 */
                            lua_pop( L, 1 );
                            /* obj var func data callbacks index value */
                        }
                    }
                    else
                    {
                        /* obj var func data callbacks index value data */
                        lua_pop( L, 1 );
                        /* obj var func data callbacks index value */
                    }
                }
                else
                {
                    /* obj var func data callbacks index value callback */
                    lua_pop( L, 1 );
                    /* obj var func data callbacks index value */
                }
            }
            else
            {
                /* obj var func data callbacks index value private2 */
                lua_pop( L, 1 );
                /* obj var func data callbacks index value */
            }
        }
        /* obj var func data callbacks index value */
        lua_pop( L, 1 );
        /* obj var func data callbacks index */
    }
    if( !b_found )
        /* obj var func data callbacks */
        return luaL_error( L, "Couldn't find matching callback." );
    /* else */
        /* obj var func data callbacks index*/

    var_DelCallback( *pp_obj, psz_var, vlclua_callback, p_callback );
    free( p_callback );

    /* obj var func data callbacks index */
    lua_pushnil( L );
    /* obj var func data callbacks index nil */
    lua_settable( L, -3 ); /* delete the callback table entry */
    /* obj var func data callbacks */
    lua_pop( L, 5 );
    /* <empty stack> */
    return 0;
}

static int vlclua_trigger_callback( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    return vlclua_push_ret( L, var_TriggerCallback( *pp_obj, psz_var ) );
}

static int vlclua_inc_integer( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    int64_t i_val = var_IncInteger( *pp_obj, psz_var );

    lua_pushinteger( L, i_val );
    return 1;
}

static int vlclua_dec_integer( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    int64_t i_val = var_DecInteger( *pp_obj, psz_var );

    lua_pushinteger( L, i_val );
    return 1;
}

static int vlclua_countchoices( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    int i_count = var_CountChoices( *pp_obj, psz_var );

    lua_pushinteger( L, i_count );
    return 1;
}

static int vlclua_togglebool( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );
    bool b_val = var_ToggleBool( *pp_obj, psz_var );

    lua_pushboolean( L, b_val );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_var_reg[] = {
    { "get", vlclua_var_get },
    { "get_list", vlclua_var_get_list },
    { "set", vlclua_var_set },
    { "create", vlclua_var_create },
    { "add_callback", vlclua_add_callback },
    { "del_callback", vlclua_del_callback },
    { "trigger_callback", vlclua_trigger_callback },
    { "command", vlclua_command },
    { "libvlc_command", vlclua_libvlc_command },
    { "inc_integer", vlclua_inc_integer },
    { "dec_integer", vlclua_dec_integer },
    { "count_choices", vlclua_countchoices },
    { "toggle_bool", vlclua_togglebool },
    { NULL, NULL }
};

void luaopen_variables( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_var_reg );
    lua_setfield( L, -2, "var" );
}
