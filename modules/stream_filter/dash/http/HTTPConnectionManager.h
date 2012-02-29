/*
 * HTTPConnectionManager.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef HTTPCONNECTIONMANAGER_H_
#define HTTPCONNECTIONMANAGER_H_

#include <vlc_common.h>

#include <string>
#include <vector>
#include <iostream>
#include <ctime>
#include <map>
#include <limits.h>

#include "http/HTTPConnection.h"
#include "http/Chunk.h"
#include "adaptationlogic/IDownloadRateObserver.h"

namespace dash
{
    namespace http
    {
        class HTTPConnectionManager
        {
            public:
                HTTPConnectionManager           (stream_t *stream);
                virtual ~HTTPConnectionManager  ();

                void                closeAllConnections ();
                bool                closeConnection     (IHTTPConnection *con);
                int                 read                (Chunk *chunk, void *p_buffer, size_t len);
                int                 peek                (Chunk *chunk, const uint8_t **pp_peek, size_t i_peek);
                void                attach              (dash::logic::IDownloadRateObserver *observer);
                void                notify              ();

            private:
                std::map<Chunk *, HTTPConnection *>                 chunkMap;
                std::map<std::string, HTTPConnection *>             urlMap;
                std::vector<dash::logic::IDownloadRateObserver *>   rateObservers;
                uint64_t                                            bpsAvg;
                uint64_t                                            bpsLastChunk;
                long                                                bytesReadSession;
                double                                              timeSecSession;
                long                                                bytesReadChunk;
                double                                              timeSecChunk;
                stream_t                                            *stream;
                int                                                 chunkCount;

                bool                closeConnection( Chunk *chunk );
                IHTTPConnection*    initConnection( Chunk *chunk );

        };
    }
}

#endif /* HTTPCONNECTIONMANAGER_H_ */
