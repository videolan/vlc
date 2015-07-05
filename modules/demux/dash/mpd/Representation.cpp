/*
 * Representation.cpp
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

#include "Representation.h"
#include "AdaptationSet.h"
#include "MPD.h"
#include "TrickModeType.h"
#include "../adaptative/playlist/SegmentTemplate.h"
#include "../adaptative/playlist/SegmentTimeline.h"
#include "../DASHStreamFormat.hpp"

using namespace dash::mpd;

Representation::Representation  ( AdaptationSet *set ) :
                BaseRepresentation( set ),
                qualityRanking  ( -1 ),
                trickModeType   ( NULL )
{
}

Representation::~Representation ()
{
    delete(this->trickModeType);
}

StreamFormat Representation::getStreamFormat() const
{
    if(getMimeType().empty())
        return DASHStreamFormat::mimeToFormat(adaptationSet->getMimeType());
    else
        return DASHStreamFormat::mimeToFormat(getMimeType());
}

TrickModeType*      Representation::getTrickModeType        () const
{
    return this->trickModeType;
}

void                Representation::setTrickMode        (TrickModeType *trickModeType)
{
    this->trickModeType = trickModeType;
}

int Representation::getQualityRanking() const
{
    return this->qualityRanking;
}

void Representation::setQualityRanking( int qualityRanking )
{
    if ( qualityRanking > 0 )
        this->qualityRanking = qualityRanking;
}

const std::list<const Representation*>&     Representation::getDependencies() const
{
    return this->dependencies;
}

void Representation::addDependency(const Representation *dep)
{
    if ( dep != NULL )
        this->dependencies.push_back( dep );
}

std::string Representation::contextualize(size_t index, const std::string &component,
                                          const BaseSegmentTemplate *basetempl) const
{
    std::string ret(component);
    size_t pos;

    const MediaSegmentTemplate *templ = dynamic_cast<const MediaSegmentTemplate *>(basetempl);

    if(templ)
    {
        pos = ret.find("$Time$");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss << getScaledTimeBySegmentNumber(index, templ);
            ret.replace(pos, std::string("$Time$").length(), ss.str());
        }

        pos = ret.find("$Number$");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss << getSegmentNumber(index, templ);
            ret.replace(pos, std::string("$Number$").length(), ss.str());
        }
        else
        {
            pos = ret.find("$Number%");
            size_t tokenlength = std::string("$Number%").length();
            size_t fmtstart = pos + tokenlength;
            if(pos != std::string::npos && fmtstart < ret.length())
            {
                size_t fmtend = ret.find('$', fmtstart);
                if(fmtend != std::string::npos)
                {
                    std::istringstream iss(ret.substr(fmtstart, fmtend - fmtstart + 1));
                    try
                    {
                        size_t width;
                        iss >> width;
                        if (iss.peek() != '$' && iss.peek() != 'd')
                            throw VLC_EGENERIC;
                        std::stringstream oss;
                        oss.width(width); /* set format string length */
                        oss.fill('0');
                        oss << getSegmentNumber(index, templ);
                        ret.replace(pos, fmtend - pos + 1, oss.str());
                    } catch(int) {}
                }
            }
        }
    }

    pos = ret.find("$Bandwidth$");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        ss << getBandwidth();
        ret.replace(pos, std::string("$Bandwidth$").length(), ss.str());
    }

    pos = ret.find("$RepresentationID$");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        ss << getId();
        ret.replace(pos, std::string("$RepresentationID$").length(), ss.str());
    }

    return ret;
}

mtime_t Representation::getScaledTimeBySegmentNumber(size_t index, const MediaSegmentTemplate *templ) const
{
    mtime_t time = 0;
    if(templ->segmentTimeline.Get())
    {
        time = templ->segmentTimeline.Get()->getScaledPlaybackTimeByElementNumber(index);
    }
    else if(templ->duration.Get())
    {
        time = templ->duration.Get() * index;
    }
    return time;
}

size_t Representation::getSegmentNumber(size_t index, const MediaSegmentTemplate *templ) const
{
    index += templ->startNumber.Get();
    /* live streams / templated */
    if(getPlaylist()->isLive())
    {
        if(templ->segmentTimeline.Get())
        {
            // do nothing ?
        }
        else if(templ->duration.Get())
        {
            mtime_t playbackstart = getPlaylist()->playbackStart.Get();
            mtime_t streamstart = getPlaylist()->availabilityStartTime.Get();
            streamstart += getPeriodStart();
            mtime_t duration = templ->duration.Get();
            uint64_t timescale = templ->inheritTimescale();
            if(duration && timescale)
                index += (playbackstart - streamstart) * timescale / duration;
        }
    }
    return index;
}
