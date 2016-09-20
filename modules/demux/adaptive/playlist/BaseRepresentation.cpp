/*
 * BaseRepresentation.cpp
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

#include <cstdlib>

#include "BaseRepresentation.h"
#include "BaseAdaptationSet.h"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "../ID.hpp"

using namespace adaptive;
using namespace adaptive::playlist;

BaseRepresentation::BaseRepresentation( BaseAdaptationSet *set ) :
                SegmentInformation( set ),
                adaptationSet   ( set ),
                bandwidth       (0)
{
    b_consistent = true;
}

BaseRepresentation::~BaseRepresentation ()
{
}

StreamFormat BaseRepresentation::getStreamFormat() const
{
    return StreamFormat();
}

BaseAdaptationSet * BaseRepresentation::getAdaptationSet()
{
    return adaptationSet;
}

uint64_t     BaseRepresentation::getBandwidth            () const
{
    return bandwidth;
}

void    BaseRepresentation::setBandwidth( uint64_t bandwidth )
{
    this->bandwidth = bandwidth;
}

const std::list<std::string> & BaseRepresentation::getCodecs() const
{
    return codecs;
}

void BaseRepresentation::addCodec(const std::string &codec)
{
    codecs.push_back(codec);
}

bool BaseRepresentation::needsUpdate() const
{
    return false;
}

bool BaseRepresentation::runLocalUpdates(mtime_t, uint64_t, bool)
{
    return false;
}

void BaseRepresentation::scheduleNextUpdate(uint64_t)
{

}

bool BaseRepresentation::consistentSegmentNumber() const
{
    return b_consistent;
}

void BaseRepresentation::pruneByPlaybackTime(mtime_t time)
{
    uint64_t num;
    if(getSegmentNumberByTime(time, &num))
        pruneBySegmentNumber(num);
}

mtime_t BaseRepresentation::getMinAheadTime(uint64_t curnum) const
{
    std::vector<ISegment *> seglist;
    getSegments(INFOTYPE_MEDIA, seglist);

    if(seglist.size() == 1 && seglist.front()->isTemplate())
    {
        const MediaSegmentTemplate *templ = dynamic_cast<MediaSegmentTemplate *>(seglist.front());
        if(templ)
        {
            const Timescale timescale = templ->inheritTimescale();
            stime_t i_length = templ->getMinAheadScaledTime(curnum);
            return timescale.ToTime(i_length);
        }

        /* should not happen */
        return CLOCK_FREQ;
    }

    mtime_t minTime = 0;
    const Timescale timescale = inheritTimescale();
    std::vector<ISegment *>::const_iterator it;
    for(it = seglist.begin(); it != seglist.end(); ++it)
    {
        const ISegment *seg = *it;
        if(seg->getSequenceNumber() > curnum)
            minTime += timescale.ToTime(seg->duration.Get());
    }

    return minTime;
}

void BaseRepresentation::debug(vlc_object_t *obj, int indent) const
{
    std::string text(indent, ' ');
    text.append("Representation ");
    text.append(id.str());
    msg_Dbg(obj, "%s", text.c_str());
    std::vector<ISegment *> list;
    getAllSegments(list);
    std::vector<ISegment *>::const_iterator l;
    for(l = list.begin(); l != list.end(); ++l)
        (*l)->debug(obj, indent + 1);
}

std::string BaseRepresentation::contextualize(size_t, const std::string &component,
                                              const BaseSegmentTemplate *) const
{
    return component;
}

bool BaseRepresentation::bwCompare(const BaseRepresentation *a,
                                   const BaseRepresentation *b)
{
    return a->getBandwidth() < b->getBandwidth();
}

bool BaseRepresentation::validateCodec(const std::string &) const
{
    return true;
}
