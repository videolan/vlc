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
#include "mpd/ProgramInformation.h"
#include "mpd/IMPDManager.h"

namespace dash
{
    namespace mpd
    {
        class MPD
        {
            public:
                MPD();
                virtual ~MPD();

                Profile                         getProfile() const;
                void                            setProfile( Profile profile );
                bool                            isLive() const;
                void                            setLive( bool live );
                time_t                          getAvailabilityStartTime() const;
                void                            setAvailabilityStartTime( time_t time );
                time_t                          getAvailabilityEndTime() const;
                void                            setAvailabilityEndTime( time_t time );
                time_t                          getDuration() const;
                void                            setDuration( time_t duration );
                time_t                          getMinUpdatePeriod() const;
                void                            setMinUpdatePeriod( time_t period );
                time_t                          getMinBufferTime() const;
                void                            setMinBufferTime( time_t time );
                time_t                          getTimeShiftBufferDepth() const;
                void                            setTimeShiftBufferDepth( time_t depth );
                const std::vector<BaseUrl *>&   getBaseUrls() const;
                const std::vector<Period *>&    getPeriods() const;
                const ProgramInformation*       getProgramInformation() const;

                void    addPeriod               (Period *period);
                void    addBaseUrl              (BaseUrl *url);
                void    setProgramInformation   (ProgramInformation *progInfo);

            private:
                Profile                             profile;
                bool                                live;
                time_t                              availabilityStartTime;
                time_t                              availabilityEndTime;
                time_t                              duration;
                time_t                              minUpdatePeriod;
                time_t                              minBufferTime;
                time_t                              timeShiftBufferDepth;
                std::vector<Period *>               periods;
                std::vector<BaseUrl *>              baseUrls;
                ProgramInformation                  *programInfo;
        };
    }
}
#endif /* MPD_H_ */
