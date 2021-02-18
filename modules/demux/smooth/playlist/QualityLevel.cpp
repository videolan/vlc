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

#include "QualityLevel.hpp"
#include "Manifest.hpp"
#include "../../adaptive/playlist/SegmentTemplate.h"
#include "../../adaptive/playlist/SegmentTimeline.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"

using namespace smooth::playlist;

SmoothCodecDescription::SmoothCodecDescription(const CodecParameters &params)
{
    params.initAndFillEsFmt(&fmt);
}

SmoothCodecDescription::~SmoothCodecDescription()
{

}

QualityLevel::QualityLevel  ( BaseAdaptationSet *set ) :
                BaseRepresentation( set )
{
}

QualityLevel::~QualityLevel ()
{
}

StreamFormat QualityLevel::getStreamFormat() const
{
    return StreamFormat(StreamFormat::MP4);
}

CodecDescription * QualityLevel::makeCodecDescription(const std::string &) const
{
    return new SmoothCodecDescription(codecParameters);
}

InitSegment * QualityLevel::getInitSegment() const
{
    if(initialisationSegment.Get())
        return initialisationSegment.Get();
    else
        return BaseRepresentation::getInitSegment();
}

std::string QualityLevel::contextualize(size_t number, const std::string &component,
                                          const SegmentTemplate *templ) const
{
    std::string ret(component);
    size_t pos;

    if(!templ)
        return ret;

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

const CodecParameters & QualityLevel::getCodecParameters() const
{
    return codecParameters;
}

void QualityLevel::setCodecParameters(const CodecParameters &c)
{
    codecParameters = c;
}
