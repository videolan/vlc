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

Url::Component::Component(const std::string & str, const SegmentTemplate *templ_)
{
    component = str;
    templ = templ_;
}

std::string Url::Component::contextualize(size_t index, const Representation *rep) const
{
    std::string ret(component);

    if(!rep || !templ)
        return ret;

    size_t pos = ret.find("$Time$");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        ss << (templ->duration.Get() * index);
        ret.replace(pos, std::string("$Time$").length(), ss.str());
    }


    pos = ret.find("$Number$");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        /* live streams / templated */
        if(templ && rep->getMPD()->isLive() && templ->duration.Get())
        {
            mtime_t playbackstart = rep->getMPD()->playbackStart.Get();
            mtime_t streamstart = rep->getMPD()->getAvailabilityStartTime();
            streamstart += rep->getPeriodStart();
            mtime_t duration = templ->duration.Get();
            uint64_t timescale = templ->timescale.Get() ?
                                 templ->timescale.Get() :
                                 rep->getTimescale();
            if(duration && timescale)
                index += (playbackstart - streamstart) * timescale / duration;
        }
        ss << index;
        ret.replace(pos, std::string("$Number$").length(), ss.str());
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
