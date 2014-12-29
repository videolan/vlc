/*
 * AdaptationLogicFactory.cpp
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

#include "AdaptationLogicFactory.h"
#include "adaptationlogic/AlwaysBestAdaptationLogic.h"
#include "adaptationlogic/RateBasedAdaptationLogic.h"
#include "adaptationlogic/AlwaysLowestAdaptationLogic.hpp"

#include <new>

using namespace dash::logic;
using namespace dash::mpd;

AbstractAdaptationLogic* AdaptationLogicFactory::create (
        AbstractAdaptationLogic::LogicType logic, MPD *mpd)
{
    switch(logic)
    {
        case AbstractAdaptationLogic::AlwaysBest:
            return new (std::nothrow) AlwaysBestAdaptationLogic(mpd);
        case AbstractAdaptationLogic::AlwaysLowest:
            return new (std::nothrow) AlwaysLowestAdaptationLogic(mpd);
        case AbstractAdaptationLogic::FixedRate:
            return new (std::nothrow) FixedRateAdaptationLogic(mpd);
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
            return new (std::nothrow) RateBasedAdaptationLogic(mpd);
        default:
            return NULL;
    }
}
