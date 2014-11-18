/*
 * IsoffMainManager.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include "IsoffMainManager.h"

using namespace dash::mpd;

IsoffMainManager::IsoffMainManager(MPD *mpd) :
    IMPDManager( mpd )
{

}

Representation*             IsoffMainManager::getRepresentation     (Period *period, uint64_t bitrate, int width, int height) const
{
    if(period == NULL)
        return NULL;

    std::vector<AdaptationSet *> adaptationSets = period->getAdaptationSets();
    std::vector<Representation *> resMatchReps;

    for(size_t i = 0; i < adaptationSets.size(); i++)
    {
        std::vector<Representation *> reps = adaptationSets.at(i)->getRepresentations();
        for( size_t j = 0; j < reps.size(); j++ )
        {
            if(reps.at(j)->getWidth() == width && reps.at(j)->getHeight() == height)
                resMatchReps.push_back(reps.at(j));
        }
    }

    if(resMatchReps.size() == 0)
        return IMPDManager::getRepresentation(period, bitrate);

    Representation  *best = NULL;
    for( size_t j = 0; j < resMatchReps.size(); j++ )
    {
        uint64_t currentBitrate = resMatchReps.at(j)->getBandwidth();

        if(best == NULL || (currentBitrate > best->getBandwidth() && currentBitrate < bitrate))
        {
            best = resMatchReps.at(j);
        }
    }

    return best;
}
