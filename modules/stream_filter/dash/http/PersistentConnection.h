/*
 * PersistentConnection.h
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
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

#ifndef PERSISTENTCONNECTION_H_
#define PERSISTENTCONNECTION_H_

#include "HTTPConnection.h"
#include <deque>

namespace dash
{
    namespace http
    {
        class PersistentConnection : public HTTPConnection
        {
            public:
                PersistentConnection            (stream_t *stream);
                virtual ~PersistentConnection   ();

                virtual int         read        (void *p_buffer, size_t len);
                virtual bool        init        (Chunk *chunk);
                bool                addChunk    (Chunk *chunk);
                const std::string&  getHostname () const;
                bool                isConnected () const;

            private:
                std::deque<Chunk *>  chunkQueue;
                bool                isInit;
                std::string         hostname;

                static const int RETRY;

            protected:
                virtual std::string prepareRequest      (Chunk *chunk);
                bool                initChunk           (Chunk *chunk);
                bool                reconnect           (Chunk *chunk);
                bool                resendAllRequests   ();
        };
    }
}

#endif /* PERSISTENTCONNECTION_H_ */
