/*****************************************************************************
 * callbacks.c: Generic lua<->vlc callbacks interface
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#include <vlc/vlc.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

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
    vlclua_pushvalue( L, p_callback->i_type, oldval );
    /* callbacks[index] callback var oldval */
    vlclua_pushvalue( L, p_callback->i_type, newval );
    /* callbacks[index] callback var oldval newval */
    lua_getfield( L, -5, "data" );
    /* callbacks[index] callback var oldval newval data */
    lua_remove( L, -6 );
    /* callback var oldval newval data */

    if( lua_pcall( L, 4, 0, 0 ) )
    {
        /* errormessage */
        const char *psz_err = lua_tostring( L, -1 );
        msg_Err( p_this, "Error while runing lua interface callback: %s",
                 psz_err );
        /* empty the stack (should only contain the error message) */
        lua_settop( L, 0 );
        return VLC_EGENERIC;
    }

    /* empty the stack (should already be empty) */
    lua_settop( L, 0 );

    return VLC_SUCCESS;
}

int vlclua_add_callback( lua_State *L )
{
    vlclua_callback_t *p_callback;
    static int i_index = 0;
    vlc_object_t *p_obj = vlclua_checkobject( L, 1, 0 );
    const char *psz_var = luaL_checkstring( L, 2 );
    lua_settop( L, 4 ); /* makes sure that optional data arg is set */
    if( !lua_isfunction( L, 3 ) )
        return vlclua_error( L );
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
    lua_pushlightuserdata( L, p_obj ); /* will be needed in vlclua_del_callback */
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
    p_callback->i_type = var_Type( p_obj, psz_var );
    p_callback->L = lua_newthread( L );

    var_AddCallback( p_obj, psz_var, vlclua_callback, p_callback );
    return 0;
}

int vlclua_del_callback( lua_State *L )
{
    vlclua_callback_t *p_callback;
    vlc_bool_t b_found = VLC_FALSE;
    vlc_object_t *p_obj = vlclua_checkobject( L, 1, 0 );
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
                        if( p_obj2 == p_obj ) /* object is equal */
                        {
                            lua_pop( L, 1 );
                            /* obj var func data callbacks index value */
                            lua_getfield( L, -1, "private3" );
                            /* obj var func data callbacks index value private3 */
                            p_callback = (vlclua_callback_t*)luaL_checklightuserdata( L, -1 );
                            lua_pop( L, 2 );
                            /* obj var func data callbacks index */
                            b_found = VLC_TRUE;
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
    if( b_found == VLC_FALSE )
        /* obj var func data callbacks */
        return luaL_error( L, "Couldn't find matching callback." );
    /* else */
        /* obj var func data callbacks index*/

    var_DelCallback( p_obj, psz_var, vlclua_callback, p_callback );
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
