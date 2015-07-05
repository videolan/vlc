/*
 * Representation.cpp
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

#include <cstdlib>

#include "Representation.hpp"
#include "M3U8.hpp"
#include "../adaptative/playlist/BasePeriod.h"
#include "../adaptative/playlist/BaseAdaptationSet.h"
#include "../adaptative/playlist/SegmentList.h"
#include "../HLSStreamFormat.hpp"

using namespace hls;
using namespace hls::playlist;

Representation::Representation  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
    b_live = true;
}

Representation::~Representation ()
{
}

StreamFormat Representation::getStreamFormat() const
{
    return StreamFormat(HLSStreamFormat::MPEG2TS);
}

bool Representation::isLive() const
{
    return b_live;
}

void Representation::localMergeWithPlaylist(M3U8 *updated, mtime_t prunebarrier)
{
    BasePeriod *period = updated->getFirstPeriod();
    if(!period)
        return;

    BaseAdaptationSet *adapt = period->getAdaptationSets().front();
    if(!adapt)
        return;

    BaseRepresentation *rep = adapt->getRepresentations().front();
    if(!rep)
        return;

    this->mergeWith( rep, prunebarrier );
}

void Representation::mergeWith(SegmentInformation *seginfo, mtime_t prunebarrier)
{
    BaseRepresentation::mergeWith(seginfo, prunebarrier);
}
