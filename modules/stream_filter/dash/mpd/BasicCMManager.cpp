/*
 * BasicCMManager.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
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

#include "BasicCMManager.h"

using namespace dash::mpd;
using namespace dash::exception;

BasicCMManager::BasicCMManager  (MPD *mpd)
{
    this->mpd = mpd;
}
BasicCMManager::~BasicCMManager ()
{
    delete this->mpd;
}

std::vector<Segment*>  BasicCMManager::getSegments             (Representation *rep)
{
    std::vector<Segment *> retSegments;
    try
    {
        SegmentInfo* info = rep->getSegmentInfo();
        Segment*     initSegment = info->getInitSegment();

        retSegments.push_back(initSegment);

        std::vector<Segment *> segments = info->getSegments();

        for(size_t i = 0; i < segments.size(); i++)
            retSegments.push_back(segments.at(i));
    }
    catch(ElementNotPresentException &e)
    {
        /*TODO Debug */
    }

    return retSegments;
}
const std::vector<Period*>&    BasicCMManager::getPeriods              () const
{
    return this->mpd->getPeriods();
}

Representation*         BasicCMManager::getBestRepresentation   (Period *period)
{
    std::vector<Group *> groups = period->getGroups();

    long            bitrate  = 0;
    Representation  *best    = NULL;

    for(size_t i = 0; i < groups.size(); i++)
    {
        std::vector<Representation *> reps = groups.at(i)->getRepresentations();
        for(size_t j = 0; j < reps.size(); j++)
        {
            try
            {
                long currentBitrate = reps.at(j)->getBandwidth();
                if(currentBitrate > bitrate)
                {
                    bitrate = currentBitrate;
                    best    = reps.at(j);
                }
            }
            catch(AttributeNotPresentException &e)
            {
                /* TODO DEBUG */
            }
        }
    }

    return best;
}
Period*                 BasicCMManager::getFirstPeriod          ()
{
    std::vector<Period *> periods = this->mpd->getPeriods();

    if(periods.size() == 0)
        return NULL;

    return periods.at(0);
}
Representation*         BasicCMManager::getRepresentation       (Period *period, long bitrate)
{
    std::vector<Group *> groups = period->getGroups();

    Representation  *best       = NULL;
    long            bestDif  = -1;

    for(size_t i = 0; i < groups.size(); i++)
    {
        std::vector<Representation *> reps = groups.at(i)->getRepresentations();
        for(size_t j = 0; j < reps.size(); j++)
        {
            try
            {
                long currentBitrate = reps.at(j)->getBandwidth();
                long dif = bitrate - currentBitrate;

                if(bestDif == -1)
                {
                    bestDif = dif;
                    best = reps.at(j);
                }
                else
                {
                    if(dif >= 0 && dif < bestDif)
                    {
                        bestDif = dif;
                        best = reps.at(j);
                    }
                }

            }
            catch(AttributeNotPresentException &e)
            {
                /* TODO DEBUG */
            }
        }
    }

    return best;
}
Period*                 BasicCMManager::getNextPeriod           (Period *period)
{
    std::vector<Period *> periods = this->mpd->getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

const MPD*      BasicCMManager::getMPD() const
{
    return this->mpd;
}
