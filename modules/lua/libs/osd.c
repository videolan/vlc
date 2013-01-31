/*****************************************************************************
 * osd.c: Generic lua interface functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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

#include <vlc_vout.h>
#include <vlc_vout_osd.h>

#include "../vlc.h"
#include "../libs.h"
#include "input.h"

/*****************************************************************************
 * OSD
 *****************************************************************************/
static int vlc_osd_icon_from_string( const char *psz_name )
{
    static const struct
    {
        int i_icon;
        const char *psz_name;
    } pp_icons[] =
        { { OSD_PAUSE_ICON, "pause" },
          { OSD_PLAY_ICON, "play" },
          { OSD_SPEAKER_ICON, "speaker" },
          { OSD_MUTE_ICON, "mute" },
          { 0, NULL } };
    int i;
    for( i = 0; pp_icons[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_icons[i].psz_name ) )
            return pp_icons[i].i_icon;
    }
    return 0;
}

static int vlclua_osd_icon( lua_State *L )
{
    const char *psz_icon = luaL_checkstring( L, 1 );
    int i_icon = vlc_osd_icon_from_string( psz_icon );
    int i_chan = luaL_optint( L, 2, SPU_DEFAULT_CHANNEL );
    if( !i_icon )
        return luaL_error( L, "\"%s\" is not a valid osd icon.", psz_icon );

    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout )
        {
            vout_OSDIcon( p_vout, i_chan, i_icon );
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input );
    }
    return 0;
}

static int vlc_osd_position_from_string( const char *psz_name )
{
    static const struct
    {
        int i_position;
        const char *psz_name;
    } pp_icons[] =
        { { 0,                                              "center"       },
          { SUBPICTURE_ALIGN_LEFT,                          "left"         },
          { SUBPICTURE_ALIGN_RIGHT,                         "right"        },
          { SUBPICTURE_ALIGN_TOP,                           "top"          },
          { SUBPICTURE_ALIGN_BOTTOM,                        "bottom"       },
          { SUBPICTURE_ALIGN_TOP   |SUBPICTURE_ALIGN_LEFT,  "top-left"     },
          { SUBPICTURE_ALIGN_TOP   |SUBPICTURE_ALIGN_RIGHT, "top-right"    },
          { SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT,  "bottom-left"  },
          { SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_RIGHT, "bottom-right" },
          { 0, NULL } };
    int i;
    for( i = 0; pp_icons[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_icons[i].psz_name ) )
            return pp_icons[i].i_position;
    }
    return 0;
}

static int vlclua_osd_message( lua_State *L )
{
    const char *psz_message = luaL_checkstring( L, 1 );
    int i_chan = luaL_optint( L, 2, SPU_DEFAULT_CHANNEL );
    const char *psz_position = luaL_optstring( L, 3, "top-right" );
    mtime_t duration = luaL_optint( L, 4, 1000000 );

    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout )
        {
            vout_OSDText( p_vout, i_chan, vlc_osd_position_from_string( psz_position ),
                          duration, psz_message );
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input );
    }
    return 0;
}

static int vlc_osd_slider_type_from_string( const char *psz_name )
{
    static const struct
    {
        int i_type;
        const char *psz_name;
    } pp_types[] =
        { { OSD_HOR_SLIDER, "horizontal" },
          { OSD_VERT_SLIDER, "vertical" },
          { 0, NULL } };
    int i;
    for( i = 0; pp_types[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_types[i].psz_name ) )
            return pp_types[i].i_type;
    }
    return 0;
}

static int vlclua_osd_slider( lua_State *L )
{
    int i_position = luaL_checkint( L, 1 );
    const char *psz_type = luaL_checkstring( L, 2 );
    int i_type = vlc_osd_slider_type_from_string( psz_type );
    int i_chan = luaL_optint( L, 3, SPU_DEFAULT_CHANNEL );
    if( !i_type )
        return luaL_error( L, "\"%s\" is not a valid slider type.",
                           psz_type );

    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout )
        {
            vout_OSDSlider( p_vout, i_chan, i_position, i_type );
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input );
    }
    return 0;
}

static int vlclua_spu_channel_register( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return luaL_error( L, "Unable to find input." );

    vout_thread_t *p_vout = input_GetVout( p_input );
    if( !p_vout )
    {
        vlc_object_release( p_input );
        return luaL_error( L, "Unable to find vout." );
    }

    int i_chan = vout_RegisterSubpictureChannel( p_vout );
    vlc_object_release( p_vout );
    vlc_object_release( p_input );
    lua_pushinteger( L, i_chan );
    return 1;
}

static int vlclua_spu_channel_clear( lua_State *L )
{
    int i_chan = luaL_checkint( L, 1 );
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        return luaL_error( L, "Unable to find input." );
    vout_thread_t *p_vout = input_GetVout( p_input );
    if( !p_vout )
    {
        vlc_object_release( p_input );
        return luaL_error( L, "Unable to find vout." );
    }

    vout_FlushSubpictureChannel( p_vout, i_chan );
    vlc_object_release( p_vout );
    vlc_object_release( p_input );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_osd_reg[] = {
    { "icon", vlclua_osd_icon },
    { "message", vlclua_osd_message },
    { "slider", vlclua_osd_slider },
    { "channel_register", vlclua_spu_channel_register },
    { "channel_clear", vlclua_spu_channel_clear },
    { NULL, NULL }
};

void luaopen_osd( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_osd_reg );
    lua_setfield( L, -2, "osd" );
}
