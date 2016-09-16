/*****************************************************************************
 * Inheritables.cpp
 *****************************************************************************
 * Copyright (C) 1998-2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "Inheritables.hpp"
#include "SegmentTimeline.h"

using namespace adaptive::playlist;
using namespace adaptive;

Timelineable::Timelineable()
{
    segmentTimeline.Set(NULL);
}

Timelineable::~Timelineable()
{
    delete segmentTimeline.Get();
}

TimescaleAble::TimescaleAble(TimescaleAble *parent)
{
    parentTimescaleAble = parent;
}

TimescaleAble::~TimescaleAble()
{
}

void TimescaleAble::setParentTimescaleAble(TimescaleAble *parent)
{
    parentTimescaleAble = parent;
}

Timescale TimescaleAble::inheritTimescale() const
{
    if(timescale.isValid())
        return timescale;
    else if(parentTimescaleAble)
        return parentTimescaleAble->inheritTimescale();
    else
        return Timescale(1);
}

void TimescaleAble::setTimescale(const Timescale & t)
{
    timescale = t;
}

void TimescaleAble::setTimescale(uint64_t t)
{
    timescale = Timescale(t);
}

const Timescale & TimescaleAble::getTimescale() const
{
    return timescale;
}

const ID & Unique::getID() const
{
    return id;
}

void Unique::setID(const ID &id_)
{
    id = id_;
}
