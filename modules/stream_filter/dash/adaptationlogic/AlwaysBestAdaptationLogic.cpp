/*
 * AlwaysBestAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
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

#include "AlwaysBestAdaptationLogic.h"

using namespace dash::logic;
using namespace dash::xml;
using namespace dash::http;
using namespace dash::mpd;

AlwaysBestAdaptationLogic::AlwaysBestAdaptationLogic    (MPDManager *mpdManager) :
                           AbstractAdaptationLogic      (mpdManager)
{
    initSchedule();
}

AlwaysBestAdaptationLogic::~AlwaysBestAdaptationLogic   ()
{
}

Chunk*  AlwaysBestAdaptationLogic::getNextChunk(Streams::Type type)
{
    if ( streams[type].count < streams[type].schedule.size() )
    {
        Chunk *chunk = new Chunk();
        chunk->setUrl(streams[type].schedule.at( streams[type].count )->getUrlSegment());
        streams[type].count++;
        return chunk;
    }
    return NULL;
}

const Representation *AlwaysBestAdaptationLogic::getCurrentRepresentation(Streams::Type type) const
{
    if ( streams[type].count < streams[type].schedule.size() )
        return streams[type].schedule.at( streams[type].count )->getRepresentation();

    return NULL;
}

void    AlwaysBestAdaptationLogic::initSchedule ()
{
    if(mpdManager)
    {
        std::vector<Period *> periods = mpdManager->getPeriods();
        if (periods.empty())
            return;
        RepresentationSelector selector;

        for(int type=0; type<Streams::count; type++)
        {
            streams[type].count = 0;
            Representation *best = selector.select(periods.front(),
                                                   static_cast<Streams::Type>(type));
            if(best)
            {
                std::vector<ISegment *> segments = best->getSegments();
                std::vector<ISegment *>::const_iterator segIt;
                for(segIt=segments.begin(); segIt!=segments.end(); segIt++)
                {
                    streams[type].schedule.push_back(*segIt);
                }
            }
        }
    }
}
