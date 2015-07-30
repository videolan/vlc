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
#include "SegmentTemplate.h"
#include "ID.hpp"

using namespace adaptative;
using namespace adaptative::playlist;

BaseRepresentation::BaseRepresentation( BaseAdaptationSet *set ) :
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

void BaseRepresentation::addCodec(const std::string &codec)
{
    codecs.push_back(codec);
}

void BaseRepresentation::debug(vlc_object_t *obj, int indent) const
{
    std::string text(indent, ' ');
    text.append("Representation ");
    text.append(id.str());
    msg_Dbg(obj, "%s", text.c_str());
    std::vector<ISegment *> list;
    getAllSegments(list);
    std::vector<ISegment *>::const_iterator l;
    for(l = list.begin(); l != list.end(); ++l)
        (*l)->debug(obj, indent + 1);
}

std::string BaseRepresentation::contextualize(size_t, const std::string &component,
                                              const BaseSegmentTemplate *) const
{
    return component;
}

bool BaseRepresentation::validateCodec(const std::string &) const
{
    return true;
}
