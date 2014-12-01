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

#include <vlc_common.h>
#include <vlc_variables.h>

using namespace dash::logic;
using namespace dash::xml;
using namespace dash::http;
using namespace dash::mpd;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (MPD *mpd) :
                          AbstractAdaptationLogic   (mpd),
                          count                     (0),
                          currentPeriod             (mpd->getFirstPeriod())
{
    width  = var_InheritInteger(mpd->getVLCObject(), "dash-prefwidth");
    height = var_InheritInteger(mpd->getVLCObject(), "dash-prefheight");
}

Chunk*  RateBasedAdaptationLogic::getNextChunk(Streams::Type type)
{
    if(this->currentPeriod == NULL)
        return NULL;

    const Representation *rep = getCurrentRepresentation(type);
    if (!rep)
        return NULL;

    std::vector<ISegment *> segments = rep->getSegments();

    if ( this->count == segments.size() )
    {
        currentPeriod = mpd->getNextPeriod(currentPeriod);
        this->count = 0;
        return getNextChunk(type);
    }

    if ( segments.size() > this->count )
    {
        ISegment *seg = segments.at( this->count );
        Chunk *chunk = seg->toChunk();
        //In case of UrlTemplate, we must stay on the same segment.
        if ( seg->isSingleShot() == true )
            this->count++;
        seg->done();
        return chunk;
    }
    return NULL;
}

const Representation *RateBasedAdaptationLogic::getCurrentRepresentation(Streams::Type type) const
{
    if(currentPeriod == NULL)
        return NULL;

    uint64_t bitrate = this->getBpsAvg();
    if(getBufferPercent() < MINBUFFER)
        bitrate = 0;

    RepresentationSelector selector;
    Representation *rep = selector.select(currentPeriod, type, bitrate, width, height);
    if ( rep == NULL )
    {
        rep = selector.select(currentPeriod, type);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}
