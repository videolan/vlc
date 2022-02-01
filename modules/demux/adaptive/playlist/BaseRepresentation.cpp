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
#include "BasePlaylist.hpp"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "SegmentList.h"
#include "SegmentBase.h"
#include "../ID.hpp"
#include "../tools/Helper.h"

#include <limits>

using namespace adaptive;
using namespace adaptive::playlist;

BaseRepresentation::BaseRepresentation( BaseAdaptationSet *set ) :
                CommonAttributesElements( set ),
                SegmentInformation( set ),
                adaptationSet   ( set ),
                bandwidth       (0)
{
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

void BaseRepresentation::addCodecs(const std::string &s)
{
    std::list<std::string> list = Helper::tokenize(s, ',');
    std::list<std::string>::const_iterator it;
    for(it=list.begin(); it!=list.end(); ++it)
        codecs.push_back(*it);
}

void BaseRepresentation::getCodecsDesc(CodecDescriptionList *desc) const
{
    std::list<std::string> codecs = getCodecs();
    if(codecs.empty())
    {
        const StreamFormat format = getStreamFormat();
        switch(format)
        {
            case StreamFormat::Type::TTML:
                codecs.push_front("stpp");
                break;
            case StreamFormat::Type::WebVTT:
                codecs.push_front("wvtt");
                break;
            default:
                break;
        }
    }

    for(auto it = codecs.cbegin(); it != codecs.cend(); ++it)
    {
        CodecDescription *dsc = makeCodecDescription(*it);
        dsc->setDescription(adaptationSet->description.Get());
        dsc->setLanguage(adaptationSet->getLang());
        if(getWidth() > 0 && getHeight() > 0)
            dsc->setDimensions(getWidth(), getHeight());
        desc->push_back(dsc);
    }
}

CodecDescription * BaseRepresentation::makeCodecDescription(const std::string &codec) const
{
    return new CodecDescription(codec);
}

bool BaseRepresentation::needsUpdate(uint64_t) const
{
    return false;
}

bool BaseRepresentation::needsIndex() const
{
    SegmentBase *base = inheritSegmentBase();
    return base && base->subSegments().empty();
}

bool BaseRepresentation::runLocalUpdates(SharedResources *)
{
    return false;
}

void BaseRepresentation::scheduleNextUpdate(uint64_t, bool)
{

}

bool BaseRepresentation::canNoLongerUpdate() const
{
    return false;
}

void BaseRepresentation::pruneByPlaybackTime(vlc_tick_t time)
{
    uint64_t num;
    if(getSegmentNumberByTime(time, &num))
        pruneBySegmentNumber(num);
}

vlc_tick_t BaseRepresentation::getMinAheadTime(uint64_t curnum) const
{
    AbstractSegmentBaseType *profile = inheritSegmentTemplate();
    if(!profile)
        profile = inheritSegmentList();
    if(!profile)
        profile = inheritSegmentBase();

    return profile ? profile->getMinAheadTime(curnum) : 0;
}

void BaseRepresentation::debug(vlc_object_t *obj, int indent) const
{
    std::string text(indent, ' ');
    text.append("Representation ");
    text.append(id.str());
    if(!codecs.empty())
    {
        std::list<std::string>::const_iterator c = codecs.begin();
        text.append(" [" + *c++);
        while(c != codecs.end())
            text.append("," + *c++);
        text.append("]");
    }
    msg_Dbg(obj, "%s", text.c_str());
    const AbstractSegmentBaseType *profile = getProfile();
    if(profile)
        profile->debug(obj, indent + 1);
}

std::string BaseRepresentation::contextualize(size_t, const std::string &component,
                                              const SegmentTemplate *) const
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

uint64_t BaseRepresentation::translateSegmentNumber(uint64_t num, const BaseRepresentation *) const
{
    return num;
}

bool BaseRepresentation::getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const
{
    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    return profile && profile->getSegmentNumberByTime(time, ret);
}

bool BaseRepresentation::getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                                vlc_tick_t *time, vlc_tick_t *duration) const
{
    if(number == std::numeric_limits<uint64_t>::max())
        return false;

    const AbstractSegmentBaseType *profile = inheritSegmentProfile();
    return profile && profile->getPlaybackTimeDurationBySegmentNumber(number, time, duration);
}

bool BaseRepresentation::getMediaPlaybackRange(vlc_tick_t *rangeBegin,
                                               vlc_tick_t *rangeEnd,
                                               vlc_tick_t *rangeLength) const
{
    SegmentTemplate *mediaSegmentTemplate = inheritSegmentTemplate();
    if( mediaSegmentTemplate )
    {
        const Timescale timescale = mediaSegmentTemplate->inheritTimescale();
        const SegmentTimeline *timeline = mediaSegmentTemplate->inheritSegmentTimeline();
        if( timeline )
        {
            stime_t startTime, endTime, duration;
            if(!timeline->getScaledPlaybackTimeDurationBySegmentNumber(timeline->minElementNumber(),
                                                                       &startTime, &duration) ||
               !timeline->getScaledPlaybackTimeDurationBySegmentNumber(timeline->maxElementNumber(),
                                                                       &endTime, &duration))
                return false;

            *rangeBegin = timescale.ToTime(startTime);
            *rangeEnd = timescale.ToTime(endTime+duration);
            *rangeLength = timescale.ToTime(timeline->getTotalLength());
            return true;
        }
        /* Else compute, current time and timeshiftdepth based */
        else if( mediaSegmentTemplate->inheritDuration() )
        {
            *rangeEnd = 0;
            *rangeBegin = -1 * getPlaylist()->timeShiftBufferDepth.Get();
            *rangeLength = getPlaylist()->timeShiftBufferDepth.Get();
            return true;
        }
    }

    SegmentList *segmentList = inheritSegmentList();
    if ( segmentList && !segmentList->getSegments().empty() )
    {
        const Timescale timescale = segmentList->inheritTimescale();
        const std::vector<Segment *> &list = segmentList->getSegments();
        const ISegment *back = list.back();
        const stime_t startTime = list.front()->startTime.Get();
        const stime_t endTime = back->startTime.Get() + back->duration.Get();
        *rangeBegin = timescale.ToTime(startTime);
        *rangeEnd = timescale.ToTime(endTime);
        *rangeLength = timescale.ToTime(segmentList->getTotalLength());
        return true;
    }

    SegmentBase *segmentBase = inheritSegmentBase();
    if( segmentBase )
    {
        const std::vector<Segment *> &list = segmentBase->subSegments();
        if(list.empty())
            return false;

        const Timescale timescale = inheritTimescale();
        const Segment *back = list.back();
        const stime_t startTime = list.front()->startTime.Get();
        const stime_t endTime = back->startTime.Get() + back->duration.Get();
        *rangeBegin = timescale.ToTime(startTime);
        *rangeEnd = timescale.ToTime(endTime);
        *rangeLength = 0;
        return true;
    }

    return false;
}
