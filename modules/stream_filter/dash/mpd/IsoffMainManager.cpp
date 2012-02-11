/*
 * IsoffMainManager.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "IsoffMainManager.h"

using namespace dash::mpd;

IsoffMainManager::IsoffMainManager  (MPD *mpd)
{
    this->mpd = mpd;
}
IsoffMainManager::~IsoffMainManager ()
{
    delete this->mpd;
}

std::vector<Segment*>       IsoffMainManager::getSegments           (const Representation *rep)
{
    std::vector<Segment *>  retSegments;
    SegmentList*            list= rep->getSegmentList();
    Segment*                initSegment = rep->getSegmentBase()->getInitSegment();

    if(initSegment)
        retSegments.push_back(initSegment);

    retSegments.insert(retSegments.end(), list->getSegments().begin(), list->getSegments().end());
    return retSegments;
}
const std::vector<Period*>& IsoffMainManager::getPeriods            () const
{
    return this->mpd->getPeriods();
}
Representation*             IsoffMainManager::getBestRepresentation (Period *period)
{
    std::vector<AdaptationSet *> adaptationSets = period->getAdaptationSets();

    int             bitrate  = 0;
    Representation  *best    = NULL;

    for(size_t i = 0; i < adaptationSets.size(); i++)
    {
        std::vector<Representation *> reps = adaptationSets.at(i)->getRepresentations();
        for(size_t j = 0; j < reps.size(); j++)
        {
            int currentBitrate = reps.at(j)->getBandwidth();

            if(currentBitrate > bitrate)
            {
                bitrate = currentBitrate;
                best    = reps.at(j);
            }
        }
    }
    return best;
}
Period*                     IsoffMainManager::getFirstPeriod        ()
{
    std::vector<Period *> periods = this->mpd->getPeriods();

    if(periods.size() == 0)
        return NULL;

    return periods.at(0);
}
Representation*             IsoffMainManager::getRepresentation     (Period *period, int bitrate) const
{
    if(period == NULL)
        return NULL;

    std::vector<AdaptationSet *> adaptationSets = period->getAdaptationSets();

    Representation  *best = NULL;
    std::cout << "Searching for best representation with bitrate: " << bitrate << std::endl;

    for(size_t i = 0; i < adaptationSets.size(); i++)
    {
        std::vector<Representation *> reps = adaptationSets.at(i)->getRepresentations();
        for( size_t j = 0; j < reps.size(); j++ )
        {
            int currentBitrate = reps.at(j)->getBandwidth();

            if(best == NULL || (currentBitrate > best->getBandwidth() && currentBitrate < bitrate))
            {
                best = reps.at( j );
            }
        }
    }
    std::cout << "Found Representation bandwidth=" << best->getBandwidth() << std::endl;
    return best;
}
Period*                     IsoffMainManager::getNextPeriod         (Period *period)
{
    std::vector<Period *> periods = this->mpd->getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}
const MPD*                  IsoffMainManager::getMPD                () const
{
    return this->mpd;
}
Representation*             IsoffMainManager::getRepresentation     (Period *period, int bitrate, int width, int height) const
{
    if(period == NULL)
        return NULL;

    std::vector<AdaptationSet *> adaptationSets = period->getAdaptationSets();

    std::cout << "Searching for best representation with bitrate: " << bitrate << " and resolution: " << width << "x" << height << std::endl;

    std::vector<Representation *> resMatchReps;

    int lowerWidth  = 0;
    int lowerHeight = 0;

    for(size_t i = 0; i < adaptationSets.size(); i++)
    {
        std::vector<Representation *> reps = adaptationSets.at(i)->getRepresentations();
        for( size_t j = 0; j < reps.size(); j++ )
        {
            if(reps.at(j)->getWidth() == width && reps.at(j)->getHeight() == height)
                resMatchReps.push_back(reps.at(j));

            if(reps.at(j)->getHeight() < height)
            {
                lowerWidth  = reps.at(j)->getWidth();
                lowerHeight = reps.at(j)->getHeight();
            }
        }
    }

    if(resMatchReps.size() == 0)
        return this->getRepresentation(period, bitrate, lowerWidth, lowerHeight);

    Representation  *best = NULL;
    for( size_t j = 0; j < resMatchReps.size(); j++ )
    {
        int currentBitrate = resMatchReps.at(j)->getBandwidth();

        if(best == NULL || (currentBitrate > best->getBandwidth() && currentBitrate < bitrate))
        {
            best = resMatchReps.at(j);
        }
    }
    std::cout << "Found a better Representation bandwidth=" << best->getBandwidth()
              << " and resolution: " << best->getWidth() << "x" << best->getHeight() << std::endl;
    return best;
}
