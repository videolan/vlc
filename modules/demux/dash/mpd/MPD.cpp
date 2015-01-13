/*
 * MPD.cpp
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

#include "MPD.h"
#include "Helper.h"
#include "dash.hpp"
#include "SegmentTimeline.h"
#include <vlc_common.h>
#include <vlc_stream.h>

using namespace dash::mpd;

MPD::MPD (stream_t *stream_, Profile profile_) :
    ICanonicalUrl(),
    stream(stream_),
    profile( profile_ )
{
    playbackStart.Set(0);
    availabilityStartTime.Set( 0 );
    availabilityEndTime.Set( 0 );
    duration.Set( 0 );
    minUpdatePeriod.Set( 0 );
    maxSegmentDuration.Set( 0 );
    minBufferTime.Set( 0 );
    timeShiftBufferDepth.Set( 0 );
    programInfo.Set( NULL );
}

MPD::~MPD   ()
{
    for(size_t i = 0; i < this->periods.size(); i++)
        delete(this->periods.at(i));

    for(size_t i = 0; i < this->baseUrls.size(); i++)
        delete(this->baseUrls.at(i));

    delete(programInfo.Get());
}

const std::vector<Period*>&    MPD::getPeriods             ()
{
    return this->periods;
}

void                    MPD::addBaseUrl             (BaseUrl *url)
{
    this->baseUrls.push_back(url);
}
void                    MPD::addPeriod              (Period *period)
{
    this->periods.push_back(period);
}

bool                    MPD::isLive() const
{
    if(type.empty())
    {
        Profile live(Profile::ISOLive);
        return profile == live;
    }
    else
        return (type != "static");
}

void MPD::setType(const std::string &type_)
{
    type = type_;
}

Profile MPD::getProfile() const
{
    return profile;
}
Url MPD::getUrlSegment() const
{
    if (!baseUrls.empty())
        return Url(baseUrls.front()->getUrl());
    else
    {
        std::stringstream ss;
        ss << stream->psz_access << "://" << Helper::getDirectoryPath(stream->psz_path) << "/";
        return Url(ss.str());
    }
}

vlc_object_t * MPD::getVLCObject() const
{
    return VLC_OBJECT(stream);
}

Period* MPD::getFirstPeriod()
{
    std::vector<Period *> periods = getPeriods();

    if( !periods.empty() )
        return periods.front();
    else
        return NULL;
}

Period* MPD::getNextPeriod(Period *period)
{
    std::vector<Period *> periods = getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

void MPD::getTimeLinesBoundaries(mtime_t *min, mtime_t *max) const
{
    *min = *max = 0;
    for(size_t i = 0; i < periods.size(); i++)
    {
        std::vector<SegmentTimeline *> timelines;
        periods.at(i)->collectTimelines(&timelines);

        for(size_t j = 0; j < timelines.size(); j++)
        {
            const SegmentTimeline *timeline = timelines.at(j);
            if(timeline->start() > *min)
                *min = timeline->start();
            if(!*max || timeline->end() < *max)
                *max = timeline->end();
        }
    }
}

void MPD::mergeWith(MPD *updatedMPD, mtime_t prunebarrier)
{
    availabilityEndTime.Set(updatedMPD->availabilityEndTime.Get());
    /* Only merge timelines for now */
    for(size_t i = 0; i < periods.size() && i < updatedMPD->periods.size(); i++)
    {
        std::vector<SegmentTimeline *> timelines;
        std::vector<SegmentTimeline *> timelinesUpdate;
        periods.at(i)->collectTimelines(&timelines);
        updatedMPD->periods.at(i)->collectTimelines(&timelinesUpdate);

        for(size_t j = 0; j < timelines.size() && j < timelinesUpdate.size(); j++)
        {
            timelines.at(j)->mergeWith(*timelinesUpdate.at(j));
            if(prunebarrier)
                timelines.at(j)->prune(prunebarrier);
        }
    }
}
