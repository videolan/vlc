/*
 * Period.h
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
#ifndef BASEPERIOD_H_
#define BASEPERIOD_H_

#include <vector>

#include "BaseAdaptationSet.h"
#include "SegmentInformation.hpp"
#include "../tools/Properties.hpp"

namespace adaptive
{
    namespace playlist
    {
        class BasePeriod : public SegmentInformation
        {
            public:
                BasePeriod(AbstractPlaylist *);
                virtual ~BasePeriod ();

                const std::vector<BaseAdaptationSet *>& getAdaptationSets   () const;
                BaseAdaptationSet *                 getAdaptationSetByID(const ID &);
                void                                addAdaptationSet    (BaseAdaptationSet *AdaptationSet);
                void                                debug               (vlc_object_t *,int = 0) const;

                virtual vlc_tick_t getPeriodStart() const; /* reimpl */
                virtual vlc_tick_t getPeriodDuration() const;
                virtual AbstractPlaylist *getPlaylist() const; /* reimpl */

                Property<vlc_tick_t> duration;
                Property<vlc_tick_t> startTime;

            private:
                std::vector<BaseAdaptationSet *>    adaptationSets;
                AbstractPlaylist *playlist;
        };
    }
}

#endif /* BASEPERIOD_H_ */
