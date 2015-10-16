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

#include "BytesRange.hpp"
#include <vector>
#include <string>
#include <stdint.h>

typedef struct block_t block_t;

namespace adaptative
{
    namespace http
    {
        class HTTPConnection;
        class HTTPConnectionManager;
        class AbstractChunk;

        class AbstractChunkSource
        {
            public:
                AbstractChunkSource();
                virtual ~AbstractChunkSource();
                virtual block_t * read(size_t) = 0;
                void setParentChunk(AbstractChunk *);

            protected:
                AbstractChunk      *parentChunk;
        };

        class AbstractChunk
        {
            public:
                virtual ~AbstractChunk();

                size_t              getBytesRead            () const;
                size_t              getBytesToRead          () const;
                const BytesRange &  getBytesRange           () const;

                void                setBytesRead    (size_t bytes);
                void                setLength       (size_t length);
                void                setBytesRange   (const BytesRange &);

                virtual block_t *   read            (size_t, mtime_t *);
                virtual void        onDownload      (block_t **) = 0;

            protected:
                AbstractChunk(AbstractChunkSource *);
                AbstractChunkSource *source;

            private:
                size_t              length;
                size_t              bytesRead;
                BytesRange          bytesRange;
        };

        class HTTPChunkSource : public AbstractChunkSource
        {
            public:
                HTTPChunkSource(const std::string &url, HTTPConnectionManager *);
                virtual ~HTTPChunkSource();

                virtual block_t * read(size_t); /* impl */

            private:
                bool init(const std::string &);
                std::string         url;
                std::string         scheme;
                std::string         path;
                std::string         hostname;
                uint16_t            port;
                HTTPConnection     *connection;
                HTTPConnectionManager *connManager;
        };

        class HTTPChunk : public AbstractChunk
        {
            public:
                HTTPChunk(const std::string &url, HTTPConnectionManager *);
                virtual ~HTTPChunk();

                virtual void        onDownload      (block_t **) {} /* impl */
        };

    }
}

#endif /* CHUNK_H_ */
