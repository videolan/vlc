/*
 * Chunk.h
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

#ifndef CHUNK_H_
#define CHUNK_H_

#include <vector>
#include <string>
#include <stdint.h>

typedef struct block_t block_t;

namespace adaptative
{
    namespace http
    {
        class HTTPConnection;

        class Chunk
        {
            public:
                Chunk           (const std::string &url);
                virtual ~Chunk  () {}

                size_t              getEndByte              () const;
                size_t              getStartByte            () const;
                const std::string&  getUrl                  () const;
                const std::string&  getScheme               () const;
                const std::string&  getHostname             () const;
                const std::string&  getPath                 () const;
                int                 getPort                 () const;
                uint64_t            getLength               () const;
                uint64_t            getBytesRead            () const;
                uint64_t            getBytesToRead          () const;
                size_t              getPercentDownloaded    () const;
                HTTPConnection*     getConnection           () const;

                void                setConnection   (HTTPConnection *connection);
                void                setBytesRead    (uint64_t bytes);
                void                setBytesToRead  (uint64_t bytes);
                void                setLength       (uint64_t length);
                void                setEndByte      (size_t endByte);
                void                setStartByte    (size_t startByte);
                void                addOptionalUrl  (const std::string& url);
                bool                usesByteRange   () const;
                void                setBitrate      (uint64_t bitrate);
                int                 getBitrate      ();

                virtual void        onDownload      (block_t **) {}

            private:
                std::string                 url;
                std::string                 scheme;
                std::string                 path;
                std::string                 hostname;
                std::vector<std::string>    optionalUrls;
                size_t                      startByte;
                size_t                      endByte;
                int                         bitrate;
                int                         port;
                uint64_t                    length;
                uint64_t                    bytesRead;
                uint64_t                    bytesToRead;
                HTTPConnection             *connection;
        };
    }
}

#endif /* CHUNK_H_ */
