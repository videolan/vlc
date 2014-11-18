/*
 * MPDManager.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "MPDManager.hpp"
#include <limits>

using namespace dash::mpd;

MPDManager::MPDManager(MPD *mpd_) :
    mpd(mpd_)
{

}

MPDManager::~MPDManager()
{
    delete mpd;
}

const std::vector<Period*>& MPDManager::getPeriods() const
{
    return mpd->getPeriods();
}

Period* MPDManager::getFirstPeriod() const
{
    std::vector<Period *> periods = getPeriods();

    if( !periods.empty() )
        return periods.front();
    else
        return NULL;
}

Period* MPDManager::getNextPeriod(Period *period)
{
    std::vector<Period *> periods = getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

const MPD* MPDManager::getMPD() const
{
    return mpd;
}
