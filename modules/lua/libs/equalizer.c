/*****************************************************************************
 * equalizer.c
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN and VLC authors
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
#include <vlc_charset.h>
#include <vlc_playlist.h>
#include <vlc_player.h>

#include "input.h"
#include "../libs.h"
#include "../vlc.h"
#include "../../audio_filter/equalizer_presets.h"

#if !defined _WIN32
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
    audio_output_t *p_aout = vlclua_get_aout_internal(L);
    if( p_aout == NULL )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( !psz_af || strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        aout_Release(p_aout);
        return 0;
    }
    free( psz_af );

    lua_pushnumber( L, var_GetFloat( p_aout, "equalizer-preamp") );
    aout_Release(p_aout);
    return 1;
}


/*****************************************************************************
* Set the preamp level
*****************************************************************************/
static int vlclua_preamp_set( lua_State *L )
{
    audio_output_t *p_aout = vlclua_get_aout_internal(L);
    if( p_aout == NULL )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( !psz_af || strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        aout_Release(p_aout);
        return 0;
    }
    free( psz_af );

    var_SetFloat( p_aout, "equalizer-preamp", luaL_checknumber( L, 1 ) );
    aout_Release(p_aout);
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
* Return EQ level for all bands as a Table
*****************************************************************************/
static int vlclua_equalizer_get( lua_State *L )
{
    const unsigned bands = 10;

    audio_output_t *p_aout = vlclua_get_aout_internal(L);
    if( p_aout == NULL )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( !psz_af || strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        aout_Release(p_aout);
        return 0;
    }
    free( psz_af );

    char *psz_bands_origin, *psz_bands;
    psz_bands_origin = psz_bands = var_GetNonEmptyString( p_aout, "equalizer-bands" );
    if( !psz_bands )
    {
        aout_Release(p_aout);
        return 0;
    }

    bool error = false;
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    lua_newtable( L );
    for( unsigned i = 0; i < bands; i++ )
    {
        float level = strtof( psz_bands, &psz_bands );
        char *str;
        if( asprintf( &str , "%f" , level ) == -1 )
        {
            error = true;
            break;
        }
        lua_pushstring( L, str );
        free(str);
        if( asprintf( &str , "band id=\"%u\"", i ) == -1 )
        {
            error = true;
            break;
        }
        lua_setfield( L , -2 , str );
        free( str );
    }

    free( psz_bands_origin );
    if( loc != (locale_t)0 )
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    aout_Release(p_aout);
    return error ? 0 : 1;
}


/*****************************************************************************
* Set the equalizer level for the specified band
*****************************************************************************/
static int vlclua_equalizer_set( lua_State *L )
{
    int bandid = luaL_checknumber( L, 1 );
    if( bandid < 0 || bandid > 9)
        return 0;

    audio_output_t *p_aout = vlclua_get_aout_internal(L);
    if( p_aout == NULL )
        return 0;

    char *psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    if( !psz_af || strstr ( psz_af, "equalizer" ) == NULL )
    {
        free( psz_af );
        aout_Release(p_aout);
        return 0;
    }
    free( psz_af );

    float level = luaL_checknumber( L, 2 );
    char *bands = var_GetString( p_aout, "equalizer-bands" );

    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    char *b = bands;
    while( bandid > 0 )
    {
        float dummy = strtof( b, &b );
        (void)dummy;
        bandid--;
    }
    if( *b != '\0' )
        *b++ = '\0';
    float dummy = strtof( b, &b );
    (void)dummy;

    char *newstr;
    if( asprintf( &newstr, "%s %.1f%s", bands, level, b ) != -1 )
    {
        var_SetString( p_aout, "equalizer-bands", newstr );
        free( newstr );
    }

    if( loc != (locale_t)0 )
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    free( bands );
    aout_Release(p_aout);
    return 0;
}

/*****************************************************************************
* Set the preset specified by preset id
*****************************************************************************/
static int vlclua_equalizer_setpreset( lua_State *L )
{
    int presetid = luaL_checknumber( L, 1 );
    if( presetid >= NB_PRESETS || presetid < 0 )
        return 0;

    audio_output_t *p_aout = vlclua_get_aout_internal(L);
    if( p_aout == NULL )
        return 0;

    int ret = 0;
    char *psz_af = var_InheritString( p_aout, "audio-filter" );
    if( psz_af != NULL && strstr ( psz_af, "equalizer" ) != NULL )
    {
        var_SetString( p_aout , "equalizer-preset" , preset_list[presetid] );
        ret = 1;
    }
    free( psz_af );
    aout_Release(p_aout);
    return ret;
}
 
/****************************************************************************
* Enable/disable Equalizer
*****************************************************************************/
static int vlclua_equalizer_enable(lua_State *L)
{
    bool state = luaL_checkboolean(L , 1);
    audio_output_t *aout = vlclua_get_aout_internal(L);
    if (aout)
    {
        aout_EnableFilter(aout, "equalizer", state);
        aout_Release (aout);
    }
    return 0;
}
/*****************************************************************************
* Get preset names
*****************************************************************************/
static int vlclua_equalizer_get_presets( lua_State *L )
{
    lua_newtable( L );
    char *str;
    for( int i = 0 ; i < NB_PRESETS ; i++ )
    {
        lua_pushstring( L, preset_list_text[i] );
        if( asprintf( &str , "preset id=\"%d\"",i ) == -1 )
            return 0;
        lua_setfield( L , -2 , str );
        free(str);
    }
    return 1;
}
static const luaL_Reg vlclua_equalizer_reg[] = {
    { "preampget", vlclua_preamp_get },
    { "preampset", vlclua_preamp_set },
    { "equalizerget", vlclua_equalizer_get },
    { "equalizerset", vlclua_equalizer_set },
    { "enable", vlclua_equalizer_enable },
    { "presets",vlclua_equalizer_get_presets },
    { "setpreset", vlclua_equalizer_setpreset },
    { NULL, NULL }
};


void luaopen_equalizer( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_equalizer_reg );
    lua_setfield( L, -2, "equalizer" );
}
