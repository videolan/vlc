/*
 * M3U8.cpp
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

#include "M3U8.hpp"
#include "Representation.hpp"
#include "../adaptative/playlist/BasePeriod.h"
#include "../adaptative/playlist/BaseAdaptationSet.h"

#include <vlc_common.h>
#include <vlc_stream.h>

using namespace hls::playlist;

M3U8::M3U8 (stream_t *stream_) :
    AbstractPlaylist(stream_)
{
    minUpdatePeriod.Set( 5 );
}

M3U8::~M3U8()
{

}

bool M3U8::isLive() const
{
    std::vector<BasePeriod *>::const_iterator itp;
    for(itp = periods.begin(); itp != periods.end(); ++itp)
    {
        const BasePeriod *period = *itp;
        std::vector<BaseAdaptationSet *>::const_iterator ita;
        for(ita = period->getAdaptationSets().begin(); ita != period->getAdaptationSets().end(); ++ita)
        {
            BaseAdaptationSet *adaptSet = *ita;
            std::vector<BaseRepresentation *>::iterator itr;
            for(itr = adaptSet->getRepresentations().begin(); itr != adaptSet->getRepresentations().end(); ++itr)
            {
                const Representation *rep = dynamic_cast<const Representation *>(*itr);
                if(rep->isLive())
                    return true;
            }
        }
    }

    return false;
}

void M3U8::debug()
{
    std::vector<BasePeriod *>::const_iterator i;
    for(i = periods.begin(); i != periods.end(); ++i)
        (*i)->debug(VLC_OBJECT(stream));
}

