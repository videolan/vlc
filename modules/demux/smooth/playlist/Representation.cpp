/*
 * Representation.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN Authors
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

#include "Representation.hpp"
#include "Manifest.hpp"
#include "../../adaptive/playlist/SegmentTemplate.h"
#include "../../adaptive/playlist/SegmentTimeline.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"

using namespace smooth::playlist;

Representation::Representation  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
}

Representation::~Representation ()
{
}

StreamFormat Representation::getStreamFormat() const
{
    return StreamFormat(StreamFormat::MP4);
}

std::size_t Representation::getSegments(SegmentInfoType type, std::vector<ISegment *> &retSegments) const
{
    if(type == INFOTYPE_INIT && initialisationSegment.Get())
    {
        retSegments.push_back(initialisationSegment.Get());
        return retSegments.size();
    }
    return BaseRepresentation::getSegments(type, retSegments);
}

std::string Representation::contextualize(size_t number, const std::string &component,
                                          const BaseSegmentTemplate *basetempl) const
{
    std::string ret(component);
    size_t pos;

    if(!basetempl)
        return ret;

    const MediaSegmentTemplate *templ = dynamic_cast<const MediaSegmentTemplate *>(basetempl);

    if(templ)
    {
        pos = ret.find("{start time}");
        if(pos == std::string::npos)
            pos = ret.find("{start_time}");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss.imbue(std::locale("C"));
            const SegmentTimeline *tl = templ->inheritSegmentTimeline();
            if(tl)
            {
                ss << tl->getScaledPlaybackTimeByElementNumber(number);
                ret.replace(pos, std::string("{start_time}").length(), ss.str());
            }
        }
    }

    pos = ret.find("{bitrate}");
    if(pos == std::string::npos)
        pos = ret.find("{Bitrate}");
    if(pos != std::string::npos)
    {
        std::stringstream ss;
        ss.imbue(std::locale("C"));
        ss << getBandwidth();
        ret.replace(pos, std::string("{bitrate}").length(), ss.str());
    }

    return ret;
}
