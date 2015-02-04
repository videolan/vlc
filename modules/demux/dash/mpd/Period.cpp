/*
 * Period.cpp
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

#include "Period.h"
#include "MPD.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

using namespace dash::mpd;

Period::Period(MPD *mpd) :
    SegmentInformation( mpd )
{
    duration.Set(0);
    startTime.Set(0);
    baseUrl.Set(NULL);
}

Period::~Period ()
{
    vlc_delete_all( this->adaptationSets );
    delete baseUrl.Get();
    childs.clear();
}

const std::vector<AdaptationSet*>&  Period::getAdaptationSets() const
{
    return this->adaptationSets;
}

const std::vector<AdaptationSet*>   Period::getAdaptationSets(dash::Streams::Type type) const
{
    std::vector<AdaptationSet*> list;
    std::vector<AdaptationSet*>::const_iterator it;
    for(it = adaptationSets.begin(); it!= adaptationSets.end(); it++)
    {
        if( Streams::Stream::mimeToType((*it)->getMimeType()) == type )
            list.push_back(*it);
    }
    return list;
}

void                                Period::addAdaptationSet(AdaptationSet *adaptationSet)
{
    if ( adaptationSet != NULL )
    {
        this->adaptationSets.push_back(adaptationSet);
        childs.push_back(adaptationSet);
    }
}

AdaptationSet * Period::getAdaptationSet(dash::Streams::Type type) const
{
    std::vector<AdaptationSet *>::const_iterator it;
    for(it = adaptationSets.begin(); it != adaptationSets.end(); it++)
    {
        if ( Streams::Stream::mimeToType((*it)->getMimeType()) == type )
            return *it;
    }
    return NULL;
}

Url Period::getUrlSegment() const
{
    if( baseUrl.Get() )
        return *(baseUrl.Get());
    else
        return getParentUrlSegment();
}

std::vector<std::string> Period::toString(int indent) const
{
    std::vector<std::string> ret;
    std::string text(indent, ' ');
    text.append("Period");
    ret.push_back(text);
    std::vector<AdaptationSet *>::const_iterator k;
    for(k = adaptationSets.begin(); k != adaptationSets.end(); k++)
    {
        std::vector<std::string> debug = (*k)->toString(indent + 1);
        ret.insert(ret.end(), debug.begin(), debug.end());
    }
    return ret;
}

mtime_t Period::getPeriodStart() const
{
    return startTime.Get();
}
