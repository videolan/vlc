/*
 * Url.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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
#include "Url.hpp"
#include "Representation.h"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"
#include "MPD.h"

#include <sstream>
using namespace dash::mpd;

Url::Url()
{
}

Url::Url(const Component & comp)
{
    prepend(comp);
}

Url::Url(const std::string &str)
{
    prepend(Component(str));
}

Url & Url::prepend(const Component & comp)
{
    components.insert(components.begin(), comp);
    return *this;
}

Url & Url::append(const Component & comp)
{
    components.push_back(comp);
    return *this;
}

Url & Url::prepend(const Url &url)
{
    components.insert(components.begin(), url.components.begin(), url.components.end());
    return *this;
}

Url & Url::append(const Url &url)
{
    components.insert(components.end(), url.components.begin(), url.components.end());
    return *this;
}

std::string Url::toString() const
{
    return toString(0, NULL);
}

std::string Url::toString(size_t index, const Representation *rep) const
{
    std::string ret;
    std::vector<Component>::const_iterator it;
    for(it = components.begin(); it != components.end(); it++)
    {
        ret.append((*it).contextualize(index, rep));
    }
    return ret;
}

Url::Component::Component(const std::string & str, const MediaSegmentTemplate *templ_)
{
    component = str;
    templ = templ_;
}

mtime_t Url::Component::getScaledTimeBySegmentNumber(size_t index, const Representation *) const
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

size_t Url::Component::getSegmentNumber(size_t index, const Representation *rep) const
{
    index += templ->startNumber.Get();
    /* live streams / templated */
    if(rep->getMPD()->isLive())
    {
        if(templ->segmentTimeline.Get())
        {
            // do nothing ?
        }
        else if(templ->duration.Get())
        {
            mtime_t playbackstart = rep->getMPD()->playbackStart.Get();
            mtime_t streamstart = rep->getMPD()->availabilityStartTime.Get();
            streamstart += rep->getPeriodStart();
            mtime_t duration = templ->duration.Get();
            uint64_t timescale = templ->inheritTimescale();
            if(duration && timescale)
                index += (playbackstart - streamstart) * timescale / duration;
        }
    }
    return index;
}

std::string Url::Component::contextualize(size_t index, const Representation *rep) const
{
    std::string ret(component);
    size_t pos;

    if(!rep)
        return ret;

    if(templ)
    {
        pos = ret.find("$Time$");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss << getScaledTimeBySegmentNumber(index, rep);
            ret.replace(pos, std::string("$Time$").length(), ss.str());
        }

        pos = ret.find("$Number$");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss << getSegmentNumber(index, rep);
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
                        if (iss.peek() != '$')
                            throw VLC_EGENERIC;
                        std::stringstream oss;
                        oss.width(width); /* set format string length */
                        oss.fill('0');
                        oss << getSegmentNumber(index, rep);
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
        ss << rep->getBandwidth();
        ret.replace(pos, std::string("$Bandwidth$").length(), ss.str());
    }

    pos = ret.find("$RepresentationID$");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        ss << rep->getId();
        ret.replace(pos, std::string("$RepresentationID$").length(), ss.str());
    }

    return ret;
}
