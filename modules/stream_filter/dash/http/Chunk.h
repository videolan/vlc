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

#include <vlc_common.h>
#include <vlc_url.h>

#include "IHTTPConnection.h"

#include <vector>
#include <string>
#include <stdint.h>

namespace dash
{
    namespace http
    {
        class Chunk
        {
            public:
                Chunk           ();

                int                 getEndByte              () const;
                int                 getStartByte            () const;
                const std::string&  getUrl                  () const;
                bool                hasHostname             () const;
                const std::string&  getHostname             () const;
                const std::string&  getPath                 () const;
                int                 getPort                 () const;
                uint64_t            getLength               () const;
                uint64_t            getBytesRead            () const;
                uint64_t            getBytesToRead          () const;
                size_t              getPercentDownloaded    () const;
                IHTTPConnection*    getConnection           () const;

                void                setConnection   (IHTTPConnection *connection);
                void                setBytesRead    (uint64_t bytes);
                void                setLength       (uint64_t length);
                void                setEndByte      (int endByte);
                void                setStartByte    (int startByte);
                void                setUrl          (const std::string& url);
                void                addOptionalUrl  (const std::string& url);
                bool                useByteRange    ();
                void                setUseByteRange (bool value);
                void                setBitrate      (uint64_t bitrate);
                int                 getBitrate      ();

            private:
                std::string                 url;
                std::string                 path;
                std::string                 hostname;
                std::vector<std::string>    optionalUrls;
                int                         startByte;
                int                         endByte;
                bool                        hasByteRange;
                int                         bitrate;
                int                         port;
                bool                        isHostname;
                size_t                      length;
                uint64_t                    bytesRead;
                IHTTPConnection             *connection;
        };
    }
}

#endif /* CHUNK_H_ */
