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
#include "ConnectionParams.hpp"
#include "../ID.hpp"
#include <vector>
#include <string>
#include <stdint.h>
#include <vlc_cxx_helpers.hpp>

typedef struct block_t block_t;

namespace adaptive
{
    namespace http
    {
        class AbstractConnection;
        class AbstractConnectionManager;
        class AbstractChunk;

        enum class ChunkType
        {
            Segment,
            Init,
            Index,
            Playlist,
            Key,
        };

        class AbstractChunkSource
        {
            public:
                AbstractChunkSource();
                virtual ~AbstractChunkSource();
                virtual block_t *   readBlock       () = 0;
                virtual block_t *   read            (size_t) = 0;
                virtual bool        hasMoreData     () const = 0;
                void                setBytesRange   (const BytesRange &);
                const BytesRange &  getBytesRange   () const;
                virtual std::string getContentType  () const;
                RequestStatus       getRequestStatus() const;

            protected:
                RequestStatus       requeststatus;
                size_t              contentLength;
                BytesRange          bytesRange;
        };

        class AbstractChunk
        {
            public:
                virtual ~AbstractChunk();

                std::string         getContentType          ();
                RequestStatus       getRequestStatus        () const;
                size_t              getBytesRead            () const;
                uint64_t            getStartByteInFile      () const;
                bool                isEmpty                 () const;

                virtual block_t *   readBlock       ();
                virtual block_t *   read            (size_t);

            protected:
                AbstractChunk(AbstractChunkSource *);
                AbstractChunkSource *source;
                virtual void        onDownload      (block_t **) = 0;

            private:
                size_t              bytesRead;
                block_t *           doRead(size_t, bool);
        };

        class HTTPChunkSource : public AbstractChunkSource,
                                public BackendPrefInterface
        {
            public:
                HTTPChunkSource(const std::string &url, AbstractConnectionManager *,
                                const ID &, ChunkType, bool = false);
                virtual ~HTTPChunkSource();

                virtual block_t *   readBlock       ()  override;
                virtual block_t *   read            (size_t)  override;
                virtual bool        hasMoreData     () const  override;
                virtual std::string getContentType  () const  override;

                static const size_t CHUNK_SIZE = 32768;

            protected:
                virtual bool        prepare();
                AbstractConnection    *connection;
                AbstractConnectionManager *connManager;
                mutable vlc::threads::mutex lock;
                size_t              consumed; /* read pointer */
                bool                prepared;
                bool                eof;
                ID                  sourceid;
                ChunkType           type;

            private:
                bool init(const std::string &);
                ConnectionParams    params;
        };

        class HTTPChunkBufferedSource : public HTTPChunkSource
        {
            friend class Downloader;

            public:
                HTTPChunkBufferedSource(const std::string &url, AbstractConnectionManager *,
                                        const ID &, ChunkType, bool = false);
                virtual ~HTTPChunkBufferedSource();
                virtual block_t *  readBlock       ()  override;
                virtual block_t *  read            (size_t)  override;
                virtual bool       hasMoreData     () const  override;
                void               hold();
                void               release();

            protected:
                virtual bool       prepare()  override;
                void               bufferize(size_t);
                bool               isDone() const;

            private:
                block_t            *p_head; /* read cache buffer */
                block_t           **pp_tail;
                size_t              buffered; /* read cache size */
                bool                done;
                bool                eof;
                vlc_tick_t          downloadstart;
                vlc::threads::condition_variable avail;
                bool                held;
        };

        class HTTPChunk : public AbstractChunk
        {
            public:
                HTTPChunk(const std::string &url, AbstractConnectionManager *,
                          const ID &, ChunkType, bool = false);
                virtual ~HTTPChunk();

            protected:
                virtual void        onDownload      (block_t **)  override {}
        };
    }
}

#endif /* CHUNK_H_ */
