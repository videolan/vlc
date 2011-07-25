/*****************************************************************************
 * equalizer.c
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Akash Mehrotra < mehrotra <dot> akash <at> gmail <dot> com >
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
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_charset.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "input.h"
#include "../libs.h"

#if !defined WIN32
# include <locale.h>
#else
# include <windows.h>
#endif

#ifdef __APPLE__
#   include <string.h>
#   include <xlocale.h>
#endif


/*****************************************************************************
* Get the preamp level
*****************************************************************************/
static int vlclua_preamp_get( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return 0;

    audio_output_t *p_aout = input_GetAout( p_input );
    vlc_object_release( p_input );

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        vlc_object_release( p_aout );
        return 0;
    }
    free( psz_af );

    lua_pushnumber( L, var_GetFloat( p_aout, "equalizer-preamp") );
    vlc_object_release( p_aout );
    return 1;
}


/*****************************************************************************
* Set the preamp level
*****************************************************************************/
static int vlclua_preamp_set( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return 0;

    audio_output_t *p_aout = input_GetAout( p_input );
    vlc_object_release( p_input );
    if( !p_aout )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        vlc_object_release( p_aout );
        return 0;
    }
    free( psz_af );

    var_SetFloat( p_aout, "equalizer-preamp", luaL_checknumber( L, 1 ) );
    vlc_object_release( p_aout );
    return 1;
}


/*****************************************************************************
Bands:
Band 0:  60 Hz
Band 1: 170 Hz
Band 2: 310 Hz
Band 3: 600 Hz
Band 4:  1 kHz
Band 5:  3 kHz
Band 6:  6 kHz
Band 7: 12 kHz
Band 8: 14 kHz
Band 9: 16 kHz
*****************************************************************************/
/*****************************************************************************
* Get the equalizer level for the specified band
*****************************************************************************/
static int vlclua_equalizer_get( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return 0;

    audio_output_t *p_aout = input_GetAout( p_input );
    vlc_object_release( p_input );
    if( !p_aout )
        return 0;

    float level = 0 ;
    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        vlc_object_release( p_aout );
        return 0;
    }
    free( psz_af );

    int bandid = luaL_checknumber( L, 1 );
    char *psz_bands_origin, *psz_bands;
    psz_bands_origin = psz_bands = var_GetNonEmptyString( p_aout, "equalizer-bands" );
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    while( bandid >= 0 )
    {
        level = strtof( psz_bands, &psz_bands);
        bandid--;
    }
    free( psz_bands_origin );
    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }

    vlc_object_release( p_aout );
    if( bandid == -1 )
    {
        lua_pushnumber( L, level );
        return 1;
    }
    else
        return 0;
}


/*****************************************************************************
* Set the equalizer level for the specified band
*****************************************************************************/
static int vlclua_equalizer_set( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return 0;

    int i_pos = 0 , j = 0;
    audio_output_t *p_aout = input_GetAout( p_input );
    vlc_object_release( p_input );
    if( !p_aout )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        vlc_object_release( p_aout );
        return 0;
    }
    free( psz_af );

    int bandid = luaL_checknumber( L, 1 );
    float level = luaL_checknumber( L, 2 );
    char *bands = var_GetString( p_aout, "equalizer-bands" );
    char newstr[7];
    while( j != bandid )
    {
        i_pos++;
        if( bands[i_pos] == '.' )
        {
            i_pos++;
            j++;
        }
    }
    if( bandid != 0 )
        i_pos++;
    snprintf( newstr, sizeof ( newstr ) , "%6.1f", level);
    for( int i = 0 ; i < 6 ; i++ )
        bands[i_pos+i] = newstr[i];
    var_SetString( p_aout, "equalizer-bands", bands );

    vlc_object_release( p_aout );
    return 1;
}


static const luaL_Reg vlclua_equalizer_reg[] = {
    { "preampget", vlclua_preamp_get },
    { "preampset", vlclua_preamp_set },
    { "equalizerget", vlclua_equalizer_get },
    { "equalizerset", vlclua_equalizer_set },
    { NULL, NULL }
};


void luaopen_equalizer( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_equalizer_reg );
    lua_setfield( L, -2, "equalizer" );
}
