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
#include "HLSRepresentation.hpp"
#include "../../adaptive/playlist/BasePeriod.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"

using namespace hls::playlist;

M3U8::M3U8 (vlc_object_t *p_object) :
    BasePlaylist(p_object)
{
    minUpdatePeriod.Set( VLC_TICK_FROM_SEC(5) );
}

M3U8::~M3U8()
{
}

bool M3U8::isLive() const
{
    bool b_live = false;
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
                const HLSRepresentation *rep = dynamic_cast<const HLSRepresentation *>(*itr);
                if(rep->initialized())
                {
                    if(rep->isLive())
                        b_live = true;
                    else
                        return false; /* Any non live has higher priority */
                }
            }
        }
    }

    return b_live;
}

