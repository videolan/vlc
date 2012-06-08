/*****************************************************************************
 * volume.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

Volume::Volume( intf_thread_t *pIntf ): VarPercent( pIntf )
{
    m_step = (float)config_GetInt( pIntf, "volume-step" ) / AOUT_VOLUME_MAX;
    m_max = 200;
    m_volumeMax = AOUT_VOLUME_DEFAULT * 2;

    // Initial value
    audio_volume_t val = aout_VolumeGet( getIntf()->p_sys->p_playlist );
    set( val, false );
}


void Volume::set( float percentage, bool updateVLC )
{
    // Avoid looping forever...
    if( (int)(get() * AOUT_VOLUME_MAX) !=
        (int)(percentage * AOUT_VOLUME_MAX) )
    {
        VarPercent::set( percentage );
        if( updateVLC )
            aout_VolumeSet( getIntf()->p_sys->p_playlist,
                            (int)(get() * m_volumeMax) );
    }
}


void Volume::set( int val, bool updateVLC )
{
    set( (float)val / m_volumeMax, updateVLC );
}


string Volume::getAsStringPercent() const
{
    int value = (int)(m_max * VarPercent::get());
    // 0 <= value <= 200, so we need 4 chars
    char str[4];
    snprintf( str, 4, "%d", value );
    return string(str);
}

