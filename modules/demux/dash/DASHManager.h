/*
 * DASHManager.h
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
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

#ifndef DASHMANAGER_H_
#define DASHMANAGER_H_

#include "http/HTTPConnectionManager.h"
#include "adaptationlogic/AbstractAdaptationLogic.h"
#include "mpd/MPD.h"

namespace dash
{
    class DASHManager
    {
        public:
            DASHManager( mpd::MPD *mpd,
                         logic::AbstractAdaptationLogic::LogicType type, stream_t *stream);
            virtual ~DASHManager    ();

            bool    start         (demux_t *);
            size_t  read();
            mtime_t getDuration() const;
            mtime_t getPCR() const;
            int     getGroup() const;
            int     esCount() const;
            bool    setPosition(mtime_t);
            bool    seekAble() const;
            bool    updateMPD();

        private:
            http::HTTPConnectionManager         *conManager;
            logic::AbstractAdaptationLogic::LogicType  logicType;
            mpd::MPD                            *mpd;
            stream_t                            *stream;
            Streams::Stream                     *streams[Streams::count];
            mtime_t                              nextMPDupdate;
    };

}

#endif /* DASHMANAGER_H_ */
