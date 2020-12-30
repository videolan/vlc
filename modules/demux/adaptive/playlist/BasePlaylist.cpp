/*
 * BasePlaylist.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 * Copyright (C) 2015 - 2020 VideoLabs, VideoLAN and VLC authors
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

#include "BasePlaylist.hpp"
#include "../tools/Helper.h"
#include "BasePeriod.h"
#include "SegmentTimeline.h"
#include <vlc_common.h>
#include <vlc_stream.h>

#include <algorithm>

using namespace adaptive::playlist;

BasePlaylist::BasePlaylist (vlc_object_t *p_object_) :
    ICanonicalUrl(), AttrsNode(Type::PLAYLIST),
    p_object(p_object_)
{
    playbackStart.Set(0);
    availabilityStartTime.Set( 0 );
    availabilityEndTime.Set( 0 );
    duration.Set( 0 );
    minUpdatePeriod.Set( VLC_TICK_FROM_SEC(2) );
    maxSegmentDuration.Set( 0 );
    minBufferTime = 0;
    maxBufferTime = 0;
    timeShiftBufferDepth.Set( 0 );
    suggestedPresentationDelay.Set( 0 );
    b_needsUpdates = true;
}

BasePlaylist::~BasePlaylist()
{
    for(size_t i = 0; i < this->periods.size(); i++)
        delete(this->periods.at(i));
}

const std::vector<BasePeriod *>& BasePlaylist::getPeriods()
{
    return periods;
}

void BasePlaylist::addBaseUrl(const std::string &url)
{
    baseUrls.push_back(url);
}

void BasePlaylist::setPlaylistUrl(const std::string &url)
{
    playlistUrl = url;
}

void BasePlaylist::addPeriod(BasePeriod *period)
{
    period->setParentNode(this);
    periods.push_back(period);
}

bool BasePlaylist::isLive() const
{
    return false;
}

bool BasePlaylist::isLowLatency() const
{
    return false;
}

void BasePlaylist::setType(const std::string &type_)
{
    type = type_;
}

void BasePlaylist::setMinBuffering( vlc_tick_t min )
{
    minBufferTime = min;
}

void BasePlaylist::setMaxBuffering( vlc_tick_t max )
{
    maxBufferTime = max;
}

vlc_tick_t BasePlaylist::getMinBuffering() const
{
    return minBufferTime;
}

vlc_tick_t BasePlaylist::getMaxBuffering() const
{
    return maxBufferTime;
}

Url BasePlaylist::getUrlSegment() const
{
    Url ret;

    if (!baseUrls.empty())
        ret = Url(baseUrls.front());

    if( !ret.hasScheme() && !playlistUrl.empty() )
        ret.prepend( Url(playlistUrl) );

    return ret;
}

vlc_object_t * BasePlaylist::getVLCObject() const
{
    return p_object;
}

BasePeriod* BasePlaylist::getFirstPeriod()
{
    std::vector<BasePeriod *> periods = getPeriods();

    if( !periods.empty() )
        return periods.front();
    else
        return NULL;
}

BasePeriod* BasePlaylist::getNextPeriod(BasePeriod *period)
{
    std::vector<BasePeriod *> periods = getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

bool BasePlaylist::needsUpdates() const
{
    return b_needsUpdates;
}

void BasePlaylist::updateWith(BasePlaylist *updatedPlaylist)
{
    availabilityEndTime.Set(updatedPlaylist->availabilityEndTime.Get());

    for(size_t i = 0; i < periods.size() && i < updatedPlaylist->periods.size(); i++)
        periods.at(i)->updateWith(updatedPlaylist->periods.at(i));
}

void BasePlaylist::debug() const
{
    std::vector<BasePeriod *>::const_iterator i;
    for(i = periods.begin(); i != periods.end(); ++i)
        (*i)->debug(VLC_OBJECT(p_object));
}
