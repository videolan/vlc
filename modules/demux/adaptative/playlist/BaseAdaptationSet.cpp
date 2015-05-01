/*
 * BaseAdaptationSet.cpp
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

#include "BaseAdaptationSet.h"
#include "BaseRepresentation.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

#include "SegmentTemplate.h"
#include "BasePeriod.h"

using namespace adaptative::playlist;

BaseAdaptationSet::BaseAdaptationSet(BasePeriod *period) :
    CommonAttributesElements(),
    SegmentInformation( period ),
    isBitstreamSwitching( false )
{
}

BaseAdaptationSet::~BaseAdaptationSet   ()
{
    vlc_delete_all( representations );
    childs.clear();
}

const std::string& BaseAdaptationSet::getMimeType() const
{
    if (mimeType.empty() && !representations.empty())
        return representations.front()->getMimeType();
    else
        return mimeType;
}

std::vector<BaseRepresentation*>& BaseAdaptationSet::getRepresentations()
{
    return representations;
}

void BaseAdaptationSet::addRepresentation(BaseRepresentation *rep)
{
    representations.push_back(rep);
    childs.push_back(rep);
}

void BaseAdaptationSet::setBitstreamSwitching  (bool value)
{
    this->isBitstreamSwitching = value;
}

bool BaseAdaptationSet::getBitstreamSwitching  () const
{
    return this->isBitstreamSwitching;
}

std::vector<std::string> BaseAdaptationSet::toString(int indent) const
{
    std::vector<std::string> ret;
    std::string text(indent, ' ');
    text.append("BaseAdaptationSet");
    ret.push_back(text);
    std::vector<BaseRepresentation *>::const_iterator k;
    for(k = representations.begin(); k != representations.end(); ++k)
    {
        std::vector<std::string> debug = (*k)->toString(indent + 1);
        ret.insert(ret.end(), debug.begin(), debug.end());
    }
    return ret;
}
