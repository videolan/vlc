/*
 * AlwaysLowestAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include "AlwaysLowestAdaptationLogic.hpp"
#include "Representationselectors.hpp"

using namespace dash::logic;
using namespace dash::http;
using namespace dash::mpd;

AlwaysLowestAdaptationLogic::AlwaysLowestAdaptationLogic(dash::mpd::MPDManager *mpdManager):
    AbstractAdaptationLogic(mpdManager),
    currentPeriod(mpdManager->getFirstPeriod()),
    count(0)
{
}

Chunk*  AlwaysLowestAdaptationLogic::getNextChunk()
{
    if(!currentPeriod)
        return NULL;

    const Representation *rep = getCurrentRepresentation();
    if ( rep == NULL )
            return NULL;

    std::vector<ISegment *> segments = rep->getSegments();
    if ( count == segments.size() )
    {
        currentPeriod = mpdManager->getNextPeriod(currentPeriod);
        count = 0;
        return getNextChunk();
    }

    if ( segments.size() > count )
    {
        ISegment *seg = segments.at( count );
        Chunk *chunk = seg->toChunk();
        //In case of UrlTemplate, we must stay on the same segment.
        if ( seg->isSingleShot() == true )
            count++;
        seg->done();
        return chunk;
    }
    return NULL;
}

const Representation *AlwaysLowestAdaptationLogic::getCurrentRepresentation() const
{
    RepresentationSelector selector;
    return selector.select(currentPeriod, 0);
}
