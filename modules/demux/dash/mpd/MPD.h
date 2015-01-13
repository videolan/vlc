/*
 * MPD.h
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

#ifndef MPD_H_
#define MPD_H_

#include <vector>
#include <string>
#include <map>

#include "mpd/Period.h"
#include "mpd/BaseUrl.h"
#include "mpd/ICanonicalUrl.hpp"
#include "mpd/ProgramInformation.h"
#include "mpd/Profile.hpp"
#include "Properties.hpp"

namespace dash
{
    namespace mpd
    {
        class MPD : public ICanonicalUrl
        {
            public:
                MPD(stream_t *, Profile);
                virtual ~MPD();

                Profile                         getProfile() const;
                bool                            isLive() const;
                void                            setType(const std::string &);

                void    addPeriod               (Period *period);
                void    addBaseUrl              (BaseUrl *url);

                virtual Url         getUrlSegment() const; /* impl */
                vlc_object_t *      getVLCObject()  const;

                virtual const std::vector<Period *>&    getPeriods();
                virtual Period*                         getFirstPeriod();
                virtual Period*                         getNextPeriod(Period *period);

                void                mergeWith(MPD *, mtime_t = 0);
                void                getTimeLinesBoundaries(mtime_t *, mtime_t *) const;

                Property<time_t>                    duration;
                Property<time_t>                    playbackStart;
                Property<time_t>                    availabilityEndTime;
                Property<time_t>                    availabilityStartTime;
                Property<time_t>                    minUpdatePeriod;
                Property<time_t>                    maxSegmentDuration;
                Property<time_t>                    minBufferTime;
                Property<time_t>                    timeShiftBufferDepth;
                Property<ProgramInformation *>      programInfo;

            private:
                stream_t                           *stream;
                Profile                             profile;
                std::vector<Period *>               periods;
                std::vector<BaseUrl *>              baseUrls;
                std::string                         type;
        };
    }
}
#endif /* MPD_H_ */
