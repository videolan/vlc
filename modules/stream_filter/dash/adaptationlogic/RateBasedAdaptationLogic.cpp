/*
 * RateBasedAdaptationLogic.cpp
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

#include "RateBasedAdaptationLogic.h"
#include "Representationselectors.hpp"

using namespace dash::logic;
using namespace dash::xml;
using namespace dash::http;
using namespace dash::mpd;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (MPDManager *mpdManager, stream_t *stream) :
                          AbstractAdaptationLogic   (mpdManager, stream),
                          mpdManager                (mpdManager),
                          count                     (0),
                          currentPeriod             (mpdManager->getFirstPeriod())
{
    width  = var_InheritInteger(stream, "dash-prefwidth");
    height = var_InheritInteger(stream, "dash-prefheight");
}

Chunk*  RateBasedAdaptationLogic::getNextChunk()
{
    if(this->currentPeriod == NULL)
        return NULL;

    const Representation *rep = getCurrentRepresentation();
    if (!rep)
        return NULL;

    std::vector<Segment *> segments = rep->getSegments();

    if ( this->count == segments.size() )
    {
        this->currentPeriod = this->mpdManager->getNextPeriod(this->currentPeriod);
        this->count = 0;
        return this->getNextChunk();
    }

    if ( segments.size() > this->count )
    {
        Segment *seg = segments.at( this->count );
        Chunk *chunk = seg->toChunk();
        //In case of UrlTemplate, we must stay on the same segment.
        if ( seg->isSingleShot() == true )
            this->count++;
        seg->done();
        return chunk;
    }
    return NULL;
}

const Representation *RateBasedAdaptationLogic::getCurrentRepresentation() const
{
    if(currentPeriod == NULL)
        return NULL;

    uint64_t bitrate = this->getBpsAvg();
    if(getBufferPercent() < MINBUFFER)
        bitrate = 0;

    RepresentationSelector selector;
    Representation *rep = selector.select(currentPeriod, bitrate, width, height);
    if ( rep == NULL )
    {
        rep = selector.select(currentPeriod);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}
