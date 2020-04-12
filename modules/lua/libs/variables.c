/*****************************************************************************
 * variables.c: Generic lua<->vlc variables interface
 *****************************************************************************
 * Copyright (C) 2007-2010 the VideoLAN team
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

#include <math.h>

#include <vlc_common.h>

#include "../vlc.h"
#include "../libs.h"
#include "variables.h"

/*****************************************************************************
 * Variables handling
 *****************************************************************************/
static int vlclua_pushvalue( lua_State *L, int i_type, vlc_value_t val )
{
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_BOOL:
            lua_pushboolean( L, val.b_bool );
            break;
        case VLC_VAR_INTEGER:
            /* Lua may only support 32-bit integers. If so, and the
             * value requires a higher range, push it as a float. We
             * lose some precision, but object variables are not a
             * recommended API for lua scripts: functionality requiring
             * high precision should be provided with a dedicated lua
             * binding instead of object variables.
             */
            // TODO: check using LUA_MININTEGER and LUA_MAXINTEGER macros
            // if and when we require lua >= 5.3
            if( sizeof( lua_Integer ) < sizeof( val.i_int ) &&
                ( val.i_int < INT32_MIN || INT32_MAX < val.i_int ) )
                lua_pushnumber( L, (lua_Number)val.i_int );
            else
                lua_pushinteger( L, val.i_int );
            break;
        case VLC_VAR_STRING:
            lua_pushstring( L, val.psz_string );
            break;
        case VLC_VAR_FLOAT:
            lua_pushnumber( L, val.f_float );
            break;
        case VLC_VAR_ADDRESS:
            vlclua_error( L );
            break;
        case VLC_VAR_VOID:
        default:
            vlclua_error( L );
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
            /* Lua may only support 32-bit integers. If so, we need to
             * get the value as a float instead so we can even know if
             * there would be an overflow.
             */
            // TODO: check using LUA_MININTEGER and LUA_MAXINTEGER macros
            // if and when we require lua >= 5.3
            if( sizeof( lua_Integer ) < sizeof( val->i_int ) )
            {
                lua_Number f = luaL_checknumber( L, -1 );
                // Calling vlc.var.set() on integer object variables with
                // an out-of-range float value is not handled.
                val->i_int = (int64_t)llround( f );
                if( INT32_MIN < val->i_int && val->i_int < INT32_MAX )
                    val->i_int = luaL_checkinteger( L, -1 );
            }
            else
                val->i_int = luaL_checkinteger( L, -1 );
            break;
        case VLC_VAR_STRING:
            val->psz_string = (char*)luaL_checkstring( L, -1 ); /* XXX: Beware, this only stays valid as long as (L,-1) stays in the stack */
            break;
        case VLC_VAR_FLOAT:
            val->f_float = luaL_checknumber( L, -1 );
            break;
        case VLC_VAR_ADDRESS:
            vlclua_error( L );
            break;
        default:
            vlclua_error( L );
    }
    return 1;
}

static int vlclua_var_inherit( lua_State *L )
{
    vlc_value_t val;
    vlc_object_t *p_obj;
    if( lua_type( L, 1 ) == LUA_TNIL )
        p_obj = vlclua_get_this( L );
    else
    {
        vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
        p_obj = *pp_obj;
    }
    const char *psz_var = luaL_checkstring( L, 2 );

    int i_type = config_GetType( psz_var );
    if( var_Inherit( p_obj, psz_var, i_type, &val ) != VLC_SUCCESS )
        return 0;

    lua_pop( L, 2 );
    vlclua_pushvalue( L, i_type, val );
    if( (i_type & VLC_VAR_CLASS) == VLC_VAR_STRING )
        free( val.psz_string );
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
    vlclua_pushvalue( L, i_type, val );
    if( (i_type & VLC_VAR_CLASS) == VLC_VAR_STRING )
        free( val.psz_string );
    return 1;
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
    vlc_value_t *val;
    char **text;
    size_t count;
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    int i_ret = var_Change( *pp_obj, psz_var, VLC_VAR_GETCHOICES,
                            &count, &val, &text );
    if( i_ret < 0 )
        return vlclua_push_ret( L, i_ret );

    int type = var_Type( *pp_obj, psz_var );

    lua_createtable( L, count, 0 );
    for( size_t i = 0; i < count; i++ )
    {
        lua_pushinteger( L, i+1 );
        if( !vlclua_pushvalue( L, type, val[i] ) )
            lua_pushnil( L );
        lua_settable( L, -3 );
        if( (type & VLC_VAR_CLASS) == VLC_VAR_STRING )
            free(val[i].psz_string);
    }

    lua_createtable( L, count, 0 );
    for( size_t i = 0; i < count; i++ )
    {
        lua_pushinteger( L, i + 1 );
        lua_pushstring( L, text[i] );
        lua_settable( L, -3 );
        free(text[i]);
    }

    free(text);
    free(val);
    return 2;
}

static int vlclua_libvlc_command( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(p_this));
    vlc_value_t val_arg;

    const char *psz_cmd = luaL_checkstring( L, 1 );
    val_arg.psz_string = (char*)luaL_optstring( L, 2, "" );

    int i_type = var_Type( vlc, psz_cmd );
    if( ! (i_type & VLC_VAR_ISCOMMAND) )
    {
        return luaL_error( L, "libvlc's \"%s\" is not a command",
                           psz_cmd );
    }

    int i_ret = var_Set( vlc, psz_cmd, val_arg );
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
    {
        b_bool = var_ToggleBool( p_obj, psz_name );
        goto end;
    }

    /* lua_gettop( L ) == 1 */
    const char *s = luaL_checkstring( L, -1 );
    lua_pop( L, 1 );

    if( s && !strcmp(s, "on") )
        b_bool = true;
    else if( s && !strcmp(s, "off") )
        b_bool = false;
    else
    {
        b_bool = var_GetBool( p_obj, psz_name );
        goto end;
    }

    if( b_bool != var_GetBool( p_obj, psz_name ) )
        var_SetBool( p_obj, psz_name, b_bool );

end:
    lua_pushboolean( L, b_bool );
    return 1;
}

static int vlclua_trigger_callback( lua_State *L )
{
    vlc_object_t **pp_obj = luaL_checkudata( L, 1, "vlc_object" );
    const char *psz_var = luaL_checkstring( L, 2 );

    var_TriggerCallback( *pp_obj, psz_var );
    return vlclua_push_ret( L, 0 );
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
    { "inherit", vlclua_var_inherit },
    { "get", vlclua_var_get },
    { "get_list", vlclua_var_get_list },
    { "set", vlclua_var_set },
    { "create", vlclua_var_create },
    { "trigger_callback", vlclua_trigger_callback },
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
