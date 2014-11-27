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

AlwaysBestAdaptationLogic::AlwaysBestAdaptationLogic    (MPDManager *mpdManager, stream_t *stream) :
                           AbstractAdaptationLogic      (mpdManager, stream)
{
    this->count         = 0;
    this->initSchedule();
}
AlwaysBestAdaptationLogic::~AlwaysBestAdaptationLogic   ()
{
}

Chunk*  AlwaysBestAdaptationLogic::getNextChunk()
{
    if ( this->count < this->schedule.size() )
    {
        Chunk *chunk = new Chunk();
        chunk->setUrl(this->schedule.at( this->count )->getUrlSegment());
        this->count++;
        return chunk;
    }
    return NULL;
}

const Representation *AlwaysBestAdaptationLogic::getCurrentRepresentation() const
{
    if ( this->count < this->schedule.size() )
        return this->schedule.at( this->count )->getRepresentation();
    return NULL;
}

void    AlwaysBestAdaptationLogic::initSchedule ()
{
    if(mpdManager)
    {
        std::vector<Period *> periods = mpdManager->getPeriods();
        std::vector<Period *>::const_iterator it;
        RepresentationSelector selector;

        for(it=periods.begin(); it!=periods.end(); it++)
        {
            Representation *best = selector.select(*it);
            if(best)
            {
                std::vector<ISegment *> segments = best->getSegments();
                std::vector<ISegment *>::const_iterator segIt;
                for(segIt=segments.begin(); segIt!=segments.end(); segIt++)
                {
                    schedule.push_back(*segIt);
                }
            }
        }
    }
}
