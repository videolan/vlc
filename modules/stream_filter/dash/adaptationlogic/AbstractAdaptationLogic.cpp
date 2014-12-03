/*
 * AbstractAdaptationLogic.cpp
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

#include "AbstractAdaptationLogic.h"

using namespace dash::logic;
using namespace dash::mpd;
using namespace dash::http;

AbstractAdaptationLogic::AbstractAdaptationLogic    (MPD *mpd_) :
                         mpd                        (mpd_),
                         currentPeriod              (mpd->getFirstPeriod()),
                         count                      (0),
                         bpsAvg                     (0),
                         bpsLastChunk               (0),
                         bufferedMicroSec           (0),
                         bufferedPercent            (0)
{
}

AbstractAdaptationLogic::~AbstractAdaptationLogic   ()
{
}

Chunk*  AbstractAdaptationLogic::getNextChunk(Streams::Type type)
{
    if(!currentPeriod)
        return NULL;

    const Representation *rep = getCurrentRepresentation(type);
    if ( rep == NULL )
            return NULL;

    std::vector<ISegment *> segments = rep->getSegments();
    if ( count == segments.size() )
    {
        currentPeriod = mpd->getNextPeriod(currentPeriod);
        count = 0;
        return getNextChunk(type);
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

void AbstractAdaptationLogic::bufferLevelChanged     (mtime_t bufferedMicroSec, int bufferedPercent)
{
    this->bufferedMicroSec = bufferedMicroSec;
    this->bufferedPercent  = bufferedPercent;
}
void AbstractAdaptationLogic::downloadRateChanged    (uint64_t bpsAvg, uint64_t bpsLastChunk)
{
    this->bpsAvg        = bpsAvg;
    this->bpsLastChunk  = bpsLastChunk;
}
uint64_t AbstractAdaptationLogic::getBpsAvg          () const
{
    return this->bpsAvg;
}
uint64_t AbstractAdaptationLogic::getBpsLastChunk    () const
{
    return this->bpsLastChunk;
}
int AbstractAdaptationLogic::getBufferPercent        () const
{
    return this->bufferedPercent;
}
