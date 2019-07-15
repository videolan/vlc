/*
 * Manifest.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Manifest.hpp"
#include "../../adaptive/playlist/BasePeriod.h"

#include <vlc_common.h>

using namespace smooth::playlist;

Manifest::Manifest (vlc_object_t *p_object) :
    AbstractPlaylist(p_object), TimescaleAble()
{
    minUpdatePeriod.Set( VLC_TICK_FROM_SEC(5) );
    setTimescale( 10000000 ); // 100ns
    b_live = false;
}

Manifest::~Manifest()
{

}

bool Manifest::isLive() const
{
    return b_live;
}

void Manifest::debug()
{
    std::vector<BasePeriod *>::const_iterator i;
    for(i = periods.begin(); i != periods.end(); ++i)
        (*i)->debug(VLC_OBJECT(p_object));
}
