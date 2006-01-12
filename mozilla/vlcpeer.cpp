/*****************************************************************************
 * vlcpeer.cpp: scriptable peer descriptor
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include "config.h"

#include <vlc/vlc.h>

#ifdef DEBUG
/* We do not want to use nsDebug.h */
#   undef DEBUG
#endif

#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif
#include <nsISupports.h>
#include <nsMemory.h>
#include <npapi.h>

#if !defined(XP_MACOSX) && !defined(XP_UNIX) && !defined(XP_WIN)
#define XP_UNIX 1
#elif defined(XP_MACOSX)
#undef XP_UNIX
#endif

#include "vlcpeer.h"
#include "vlcplugin.h"

NS_IMPL_ISUPPORTS2( VlcPeer, VlcIntf, nsIClassInfo )

/*****************************************************************************
 * Scriptable peer constructor and destructor
 *****************************************************************************/
VlcPeer::VlcPeer()
{
    NS_INIT_ISUPPORTS();
}

VlcPeer::VlcPeer( VlcPlugin * plugin )
{
    NS_INIT_ISUPPORTS();
    p_plugin = plugin;
}

VlcPeer::~VlcPeer()
{
    ;
}

/*****************************************************************************
 * Scriptable peer methods
 *****************************************************************************/
void VlcPeer::Disable()
{
    p_plugin = NULL;
}

/*****************************************************************************
 * Scriptable peer plugin methods
 *****************************************************************************/
NS_IMETHODIMP VlcPeer::Play()
{
    if( p_plugin )
    {
        if( !p_plugin->b_stream && p_plugin->psz_target )
        {
            VLC_AddTarget( p_plugin->i_vlc, p_plugin->psz_target, 0, 0,
                           PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            p_plugin->b_stream = 1;
        }

        VLC_Play( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Pause()
{
    if( p_plugin )
    {
        VLC_Pause( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Stop()
{
    if( p_plugin )
    {
        VLC_Stop( p_plugin->i_vlc );
        p_plugin->b_stream = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Fullscreen()
{
    if( p_plugin )
    {
#ifdef XP_MACOSX
#else
        VLC_FullScreen( p_plugin->i_vlc );
#endif
    }
    return NS_OK;
}

/* Set/Get vlc variables */
NS_IMETHODIMP VlcPeer::Set_int_variable(const char *psz_var, PRInt64 value )
{
    vlc_value_t val;
    val.i_int = value;
    if( p_plugin )
    {
        VLC_VariableSet( p_plugin->i_vlc, psz_var, val );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Set_str_variable(const char *psz_var, const char *value )
{
    vlc_value_t val;
    val.psz_string = strdup( value );
    if( p_plugin )
    {
        VLC_VariableSet( p_plugin->i_vlc, psz_var, val );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Set_bool_variable(const char *psz_var, PRBool value )
{
    vlc_value_t val;
    val.b_bool = value >= 1 ? VLC_TRUE : VLC_FALSE;
    if( p_plugin )
    {
        VLC_VariableSet( p_plugin->i_vlc, psz_var, val );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_int_variable( const char *psz_var, PRInt64 *result )
{
    vlc_value_t val;
    if( p_plugin )
    {
        fprintf(stderr, "Choppage de %s\n", psz_var );
        VLC_VariableGet( p_plugin->i_vlc, psz_var, &val );
        fprintf(stderr, "Valeur %i\n", val.i_int );
        *result = (PRInt64)val.i_int;
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_bool_variable(  const char *psz_var,PRBool *result )
{
    vlc_value_t val;
    if( p_plugin )
    {
        VLC_VariableGet( p_plugin->i_vlc, psz_var, &val );
        *result = (PRBool)val.b_bool;
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_str_variable( const char *psz_var, char **result )
{
    vlc_value_t val;
    if( p_plugin )
    {
        fprintf(stderr, "Choppage de %s\n", psz_var );
        VLC_VariableGet( p_plugin->i_vlc, psz_var, &val );
        if( val.psz_string )
        {
            *result = strdup( val.psz_string );
        }
        else
        {
            *result = strdup( "" );
        }
    }
    return NS_OK;
}

/* Playlist control */
NS_IMETHODIMP VlcPeer::Clear_playlist()
{
    if( p_plugin )
    {
        VLC_PlaylistClear( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Add_item( const char *psz_item )
{
     if( p_plugin )
     {
          VLC_AddTarget( p_plugin->i_vlc, psz_item, NULL, 0,
                         PLAYLIST_APPEND, PLAYLIST_END);
     }
     return NS_OK;
}


NS_IMETHODIMP VlcPeer::Isplaying( PRBool *b_playing )
{
    if( p_plugin->i_vlc )
    {
        *b_playing = VLC_IsPlaying( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_position( PRInt64 *i_position )
{
    if( p_plugin->i_vlc )
    {
        *i_position = (PRInt64)VLC_PositionGet( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_time( PRInt64 *i_time )
{
    if( p_plugin->i_vlc )
    {
        *i_time = VLC_TimeGet( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_length( PRInt64 *i_length )
{
    if( p_plugin->i_vlc )
    {
        *i_length = VLC_LengthGet( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Seek( PRInt64 i_secs, PRInt64 b_relative )
{
    if( p_plugin->i_vlc )
    {
        VLC_TimeSet( p_plugin->i_vlc, i_secs, b_relative );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Next()
{
    if( p_plugin->i_vlc )
    {
        VLC_PlaylistNext( p_plugin->i_vlc);
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Previous()
{
    if( p_plugin->i_vlc )
    {
        VLC_PlaylistPrev( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Set_volume( PRInt64 i_volume )
{
    if( p_plugin->i_vlc )
    {
        VLC_VolumeSet( p_plugin->i_vlc, i_volume );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Get_volume( PRInt64 *i_volume )
{
    if( p_plugin->i_vlc )
    {
        *i_volume = VLC_VolumeGet( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Mute()
{
    if( p_plugin->i_vlc )
    {
        VLC_VolumeMute( p_plugin->i_vlc );
    }
    return NS_OK;
}
