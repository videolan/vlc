/*
 * Representationselectors.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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
#include "Representationselectors.hpp"
#include "../playlist/BaseRepresentation.h"
#include "../playlist/BaseAdaptationSet.h"
#include "../playlist/BasePeriod.h"
#include <limits>

using namespace adaptative::logic;

RepresentationSelector::RepresentationSelector()
{
}

BaseRepresentation * RepresentationSelector::select(BaseAdaptationSet *adaptSet) const
{
    return select(adaptSet, std::numeric_limits<uint64_t>::max());
}
BaseRepresentation * RepresentationSelector::select(BaseAdaptationSet *adaptSet, uint64_t bitrate) const
{
    if (adaptSet == NULL)
        return NULL;

    BaseRepresentation *best = NULL;
    std::vector<BaseRepresentation *> reps = adaptSet->getRepresentations();
    BaseRepresentation *candidate = select(reps, (best)?best->getBandwidth():0, bitrate);
    if (candidate)
    {
        if (candidate->getBandwidth() > bitrate) /* none matched, returned lowest */
            return candidate;
        best = candidate;
    }

    return best;
}

BaseRepresentation * RepresentationSelector::select(BaseAdaptationSet *adaptSet, uint64_t bitrate,
                                                int width, int height) const
{
    if(adaptSet == NULL)
        return NULL;

    std::vector<BaseRepresentation *> resMatchReps;

    /* subset matching WxH */
    std::vector<BaseRepresentation *> reps = adaptSet->getRepresentations();
    std::vector<BaseRepresentation *>::const_iterator repIt;
    for(repIt=reps.begin(); repIt!=reps.end(); ++repIt)
    {
        if((*repIt)->getWidth() == width && (*repIt)->getHeight() == height)
            resMatchReps.push_back(*repIt);
    }

    if(resMatchReps.empty())
        return select(adaptSet, bitrate);
    else
        return select(resMatchReps, 0, bitrate);
}

BaseRepresentation * RepresentationSelector::select(std::vector<BaseRepresentation *>& reps,
                                                uint64_t minbitrate, uint64_t maxbitrate) const
{
    BaseRepresentation  *candidate = NULL, *lowest = NULL;
    std::vector<BaseRepresentation *>::const_iterator repIt;
    for(repIt=reps.begin(); repIt!=reps.end(); ++repIt)
    {
        if ( !lowest || (*repIt)->getBandwidth() < lowest->getBandwidth())
            lowest = *repIt;

        if ( (*repIt)->getBandwidth() < maxbitrate &&
             (*repIt)->getBandwidth() > minbitrate )
        {
            candidate = (*repIt);
            minbitrate = (*repIt)->getBandwidth();
        }
    }

    if (!candidate)
        return candidate = lowest;

    return candidate;
}
