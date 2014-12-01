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
#include <vlc_common.h>
#include <vlc_stream.h>

using namespace dash::mpd;

MPD::MPD (stream_t *stream_, Profile profile_) :
    ICanonicalUrl(),
    stream(stream_),
    profile( profile_ ),
    availabilityStartTime( -1 ),
    availabilityEndTime( -1 ),
    duration( -1 ),
    minUpdatePeriod( -1 ),
    minBufferTime( -1 ),
    timeShiftBufferDepth( -1 ),
    programInfo( NULL )
{
}

MPD::~MPD   ()
{
    for(size_t i = 0; i < this->periods.size(); i++)
        delete(this->periods.at(i));

    for(size_t i = 0; i < this->baseUrls.size(); i++)
        delete(this->baseUrls.at(i));

    delete(this->programInfo);
}

const std::vector<Period*>&    MPD::getPeriods             () const
{
    return this->periods;
}

time_t      MPD::getDuration() const
{
    return this->duration;
}

void MPD::setDuration(time_t duration)
{
    if ( duration >= 0 )
        this->duration = duration;
}

time_t MPD::getMinUpdatePeriod() const
{
    return this->minUpdatePeriod;
}

void MPD::setMinUpdatePeriod(time_t period)
{
    if ( period >= 0 )
        this->minUpdatePeriod = period;
}

time_t MPD::getMinBufferTime() const
{
    return this->minBufferTime;
}

void MPD::setMinBufferTime(time_t time)
{
    if ( time >= 0 )
        this->minBufferTime = time;
}

time_t MPD::getTimeShiftBufferDepth() const
{
    return this->timeShiftBufferDepth;
}

void MPD::setTimeShiftBufferDepth(time_t depth)
{
    if ( depth >= 0 )
        this->timeShiftBufferDepth = depth;
}

const ProgramInformation*     MPD::getProgramInformation  () const
{
    return this->programInfo;
}

void                    MPD::addBaseUrl             (BaseUrl *url)
{
    this->baseUrls.push_back(url);
}
void                    MPD::addPeriod              (Period *period)
{
    this->periods.push_back(period);
}
void                    MPD::setProgramInformation  (ProgramInformation *progInfo)
{
    this->programInfo = progInfo;
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

time_t MPD::getAvailabilityStartTime() const
{
    return this->availabilityStartTime;
}

void MPD::setAvailabilityStartTime(time_t time)
{
    if ( time >=0 )
        this->availabilityStartTime = time;
}

time_t MPD::getAvailabilityEndTime() const
{
    return this->availabilityEndTime;
}

void MPD::setAvailabilityEndTime(time_t time)
{
    if ( time >= 0 )
        this->availabilityEndTime = time;
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

Period* MPD::getFirstPeriod() const
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
