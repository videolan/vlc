/*
 * IMPDManager.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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

#include "IMPDManager.h"

using namespace dash::mpd;

IMPDManager::IMPDManager(MPD *mpd_) :
    mpd(mpd_)
{

}

IMPDManager::~IMPDManager()
{
    delete mpd;
}

const std::vector<Period*>& IMPDManager::getPeriods() const
{
    return mpd->getPeriods();
}

Period* IMPDManager::getFirstPeriod() const
{
    std::vector<Period *> periods = getPeriods();

    if( !periods.empty() )
        return periods.front();
    else
        return NULL;
}

Period* IMPDManager::getNextPeriod(Period *period)
{
    std::vector<Period *> periods = getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

const MPD* IMPDManager::getMPD() const
{
    return mpd;
}

Representation* IMPDManager::getBestRepresentation(Period *period) const
{
    if (period == NULL)
        return NULL;

    std::vector<AdaptationSet *> adaptSet = period->getAdaptationSets();

    uint64_t        bitrate  = 0;
    Representation  *best    = NULL;

    for(size_t i = 0; i < adaptSet.size(); i++)
    {
        std::vector<Representation *> reps = adaptSet.at(i)->getRepresentations();
        for(size_t j = 0; j < reps.size(); j++)
        {
            uint64_t currentBitrate = reps.at(j)->getBandwidth();

            if( currentBitrate > bitrate)
            {
                bitrate = currentBitrate;
                best    = reps.at(j);
            }
        }
    }
    return best;
}

Representation* IMPDManager::getRepresentation(Period *period, uint64_t bitrate ) const
{
    if (period == NULL)
        return NULL;

    std::vector<AdaptationSet *>    adaptSet = period->getAdaptationSets();

    Representation  *best = NULL;

    for(size_t i = 0; i < adaptSet.size(); i++)
    {
        std::vector<Representation *> reps = adaptSet.at(i)->getRepresentations();
        for( size_t j = 0; j < reps.size(); j++ )
        {
            uint64_t currentBitrate = reps.at(j)->getBandwidth();

            if ( best == NULL ||
                 ( currentBitrate > best->getBandwidth() &&
                   currentBitrate < bitrate ) )
            {
                best = reps.at( j );
            }
        }
    }
    return best;
}
