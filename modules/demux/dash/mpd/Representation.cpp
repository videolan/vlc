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
#include "../adaptive/playlist/SegmentTemplate.h"
#include "../adaptive/playlist/SegmentTimeline.h"

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
        return StreamFormat(adaptationSet->getMimeType());
    else
        return StreamFormat(getMimeType());
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

std::string Representation::contextualize(size_t number, const std::string &component,
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
            ss.imbue(std::locale("C"));
            ss << getScaledTimeBySegmentNumber(number, templ);
            ret.replace(pos, std::string("$Time$").length(), ss.str());
        }

        pos = ret.find("$Number$");
        if(pos != std::string::npos)
        {
            std::stringstream ss;
            ss.imbue(std::locale("C"));
            ss << number;
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
                    iss.imbue(std::locale("C"));
                    try
                    {
                        size_t width;
                        iss >> width;
                        if (iss.peek() != '$' && iss.peek() != 'd')
                            throw VLC_EGENERIC;
                        std::stringstream oss;
                        oss.imbue(std::locale("C"));
                        oss.width(width); /* set format string length */
                        oss.fill('0');
                        oss << number;
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
        ss.imbue(std::locale("C"));
        ss << getBandwidth();
        ret.replace(pos, std::string("$Bandwidth$").length(), ss.str());
    }

    pos = ret.find("$RepresentationID$");
    if(pos != std::string::npos)
        ret.replace(pos, std::string("$RepresentationID$").length(), id.str());

    return ret;
}

mtime_t Representation::getScaledTimeBySegmentNumber(uint64_t index, const MediaSegmentTemplate *templ) const
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

