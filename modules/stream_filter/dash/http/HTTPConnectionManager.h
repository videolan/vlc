/*
 * HTTPConnectionManager.h
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

#ifndef HTTPCONNECTIONMANAGER_H_
#define HTTPCONNECTIONMANAGER_H_

#include <vlc_common.h>

#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <ctime>
#include <limits.h>

#include "http/PersistentConnection.h"
#include "adaptationlogic/IAdaptationLogic.h"

namespace dash
{
    namespace http
    {
        class HTTPConnectionManager
        {
            public:
                HTTPConnectionManager           (logic::IAdaptationLogic *adaptationLogic, stream_t *stream);
                virtual ~HTTPConnectionManager  ();

                void    closeAllConnections ();
                bool    addChunk            (Chunk *chunk);
                int     read                (block_t *block);
                void    attach              (dash::logic::IDownloadRateObserver *observer);
                void    notify              ();

            private:
                std::vector<dash::logic::IDownloadRateObserver *>   rateObservers;
                std::deque<Chunk *>                                 downloadQueue;
                std::vector<PersistentConnection *>                 connectionPool;
                logic::IAdaptationLogic                             *adaptationLogic;
                stream_t                                            *stream;
                int                                                 chunkCount;
                int64_t                                             bpsAvg;
                int64_t                                             bpsLastChunk;
                int64_t                                             bpsCurrentChunk;
                int64_t                                             bytesReadSession;
                int64_t                                             bytesReadChunk;
                double                                              timeSession;
                double                                              timeChunk;

                static const size_t     PIPELINE;
                static const size_t     PIPELINELENGTH;
                static const uint64_t   CHUNKDEFAULTBITRATE;

                std::vector<PersistentConnection *>     getConnectionsForHost   (const std::string &hostname);
                void                                    updateStatistics        (int bytes, double time);

        };
    }
}

#endif /* HTTPCONNECTIONMANAGER_H_ */
