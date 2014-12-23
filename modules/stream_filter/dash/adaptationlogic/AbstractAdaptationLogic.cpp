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
                         prevRepresentation         (NULL)
{
}

AbstractAdaptationLogic::~AbstractAdaptationLogic   ()
{
}

Chunk*  AbstractAdaptationLogic::getNextChunk(Streams::Type type)
{
    if(!currentPeriod)
        return NULL;

    Representation *rep;

    if(prevRepresentation && !prevRepresentation->canBitswitch())
        rep = prevRepresentation;
    else
        rep = getCurrentRepresentation(type);

    if ( rep == NULL )
            return NULL;

    bool reinit = count && (rep != prevRepresentation);
    prevRepresentation = rep;

    std::vector<ISegment *> segments = rep->getSegments();
    ISegment *first = segments.empty() ? NULL : segments.front();

    if (reinit && first && first->getClassId() == InitSegment::CLASSID_INITSEGMENT)
        return first->toChunk(count, rep);

    bool b_templated = (first && !first->isSingleShot());

    if (count == segments.size() && !b_templated)
    {
        currentPeriod = mpd->getNextPeriod(currentPeriod);
        prevRepresentation = NULL;
        count = 0;
        return getNextChunk(type);
    }

    ISegment *seg = NULL;
    if ( segments.size() > count )
    {
        seg = segments.at( count );
    }
    else if(b_templated)
    {
        seg = segments.back();
    }

    if(seg)
    {
        Chunk *chunk = seg->toChunk(count, rep);
        count++;
        seg->done();
        return chunk;
    }
    return NULL;
}

void AbstractAdaptationLogic::updateDownloadRate    (size_t, mtime_t)
{
}
