/*****************************************************************************
 * volume.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10 aT videolan Dot org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout_intf.h>
#include <vlc_playlist.h>
#include "volume.hpp"
#include <math.h>

Volume::Volume( intf_thread_t *pIntf ): VarPercent( pIntf )
{
    // compute preferred step in [0.,1.] range
    m_step = (float)config_GetInt( pIntf, "volume-step" )
             / (float)AOUT_VOLUME_MAX;

    // set current volume from the playlist
    playlist_t* pPlaylist = pIntf->p_sys->p_playlist;
    int volume = var_GetInteger( pPlaylist, "volume" );
    set( volume, false );
}


void Volume::set( float percentage, bool updateVLC )
{
    VarPercent::set( percentage );
    if( updateVLC )
    {
        playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;
        aout_VolumeSet( pPlaylist, getVolume() );
    }
}


void Volume::set( int volume, bool updateVLC )
{
    // volume is kept by the playlist in [0,AOUT_VOLUME_MAX] range
    // this class keeps it in [0.,1.] range
    set( (float)volume/(float)AOUT_VOLUME_MAX, updateVLC );
}


float Volume::getVolume() const
{
    // translate from [0.,1.] into [0.,AOUT_VOLUME_MAX/AOUT_VOLUME_DEFAULT]
    return get() * AOUT_VOLUME_MAX/AOUT_VOLUME_DEFAULT;
}


string Volume::getAsStringPercent() const
{
    int value = lround( getVolume() * 100. );
    // 0 <= value <= 200, so we need 4 chars
    char str[4];
    snprintf( str, 4, "%i", value );
    return string(str);
}

