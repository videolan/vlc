/*****************************************************************************
 * variables.c: Generic lua<->vlc variables inteface
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 * Variables handling
 *****************************************************************************/
int vlclua_pushvalue( lua_State *L, int i_type, vlc_value_t val )
{
    switch( i_type &= 0xf0 )
    {
        case VLC_VAR_VOID:
            vlclua_error( L );
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
        case VLC_VAR_LIST:
            {
                int i_count = val.p_list->i_count;
                int i;
                lua_createtable( L, i_count, 0 );
                for( i = 0; i < i_count; i++ )
                {
                    lua_pushinteger( L, i+1 );
                    if( !vlclua_pushvalue( L, val.p_list->pi_types[i],
                                           val.p_list->p_values[i] ) )
                        lua_pushnil( L );
                    lua_settable( L, -3 );
                }
            }
            break;
        default:
            vlclua_error( L );
    }
    return 1;
}

static int vlclua_tovalue( lua_State *L, int i_type, vlc_value_t *val )
{
    switch( i_type & 0xf0 )
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
                val->i_time = (vlc_int64_t)(f*1000000.);
            }
            break;
        case VLC_VAR_ADDRESS:
            vlclua_error( L );
            break;
        case VLC_VAR_MUTEX:
            vlclua_error( L );
            break;
        case VLC_VAR_LIST:
            vlclua_error( L );
            break;
        default:
            vlclua_error( L );
    }
    return 1;
}

int vlclua_var_get( lua_State *L )
{
    int i_type;
    vlc_value_t val;
    vlc_object_t *p_obj = vlclua_checkobject( L, 1, 0 );
    const char *psz_var = luaL_checkstring( L, 2 );
    i_type = var_Type( p_obj, psz_var );
    var_Get( p_obj, psz_var, &val );
    lua_pop( L, 2 );
    return vlclua_pushvalue( L, i_type, val );
}

int vlclua_var_set( lua_State *L )
{
    int i_type;
    vlc_value_t val;
    vlc_object_t *p_obj = vlclua_checkobject( L, 1, 0 );
    const char *psz_var = luaL_checkstring( L, 2 );
    int i_ret;
    i_type = var_Type( p_obj, psz_var );
    vlclua_tovalue( L, i_type, &val );
    i_ret = var_Set( p_obj, psz_var, val );
    lua_pop( L, 3 );
    return vlclua_push_ret( L, i_ret );
}

int vlclua_var_get_list( lua_State *L )
{
    vlc_value_t val;
    vlc_value_t text;
    vlc_object_t *p_obj = vlclua_checkobject( L, 1, 0 );
    const char *psz_var = luaL_checkstring( L, 2 );
    int i_ret = var_Change( p_obj, psz_var, VLC_VAR_GETLIST, &val, &text );
    if( i_ret < 0 ) return vlclua_push_ret( L, i_ret );
    vlclua_pushvalue( L, VLC_VAR_LIST, val );
    vlclua_pushvalue( L, VLC_VAR_LIST, text );
    var_Change( p_obj, psz_var, VLC_VAR_FREELIST, &val, &text );
    return 2;
}

int vlclua_module_command( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    const char *psz_name;
    const char *psz_cmd;
    const char *psz_arg;
    char *psz_msg;
    psz_name = luaL_checkstring( L, 1 );
    psz_cmd = luaL_checkstring( L, 2 );
    psz_arg = luaL_checkstring( L, 3 );
    lua_pop( L, 3 );
    var_Command( p_this, psz_name, psz_cmd, psz_arg, &psz_msg );
    if( psz_msg )
    {
        lua_pushstring( L, psz_msg );
        free( psz_msg );
    }
    else
    {
        lua_pushstring( L, "" );
    }
    return 1;
}

int vlclua_libvlc_command( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    const char *psz_cmd;
    vlc_value_t val_arg;
    psz_cmd = luaL_checkstring( L, 1 );
    val_arg.psz_string = strdup( luaL_optstring( L, 2, "" ) );
    lua_pop( L, 2 );
    if( !var_Type( p_this->p_libvlc, psz_cmd ) & VLC_VAR_ISCOMMAND )
    {
        free( val_arg.psz_string );
        return luaL_error( L, "libvlc's \"%s\" is not a command",
                           psz_cmd );
    }

    return vlclua_push_ret( L,
                            var_Set( p_this->p_libvlc, psz_cmd, val_arg ) );
}
