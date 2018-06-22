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

        class ChunkInterface
        {
            public:
                virtual ~ChunkInterface() = default;
                virtual std::string getContentType  () const = 0;
                virtual RequestStatus getRequestStatus() const = 0;

                virtual block_t *   readBlock       () = 0;
                virtual block_t *   read            (size_t) = 0;
                virtual bool        hasMoreData     () const = 0;
                virtual size_t      getBytesRead    () const = 0;
        };

        using StorageID = std::string;

        class AbstractChunkSource : public ChunkInterface
        {
            friend class AbstractConnectionManager;

            public:
                const BytesRange &  getBytesRange   () const;
                ChunkType           getChunkType    () const;
                const StorageID &   getStorageID    () const;
                virtual std::string getContentType  () const override;
                virtual RequestStatus getRequestStatus() const override;
                virtual void        recycle() = 0;

            protected:
                AbstractChunkSource(ChunkType, const BytesRange & = BytesRange());
                virtual ~AbstractChunkSource();
                StorageID           storeid;
                ChunkType           type;
                RequestStatus       requeststatus;
                size_t              contentLength;
                BytesRange          bytesRange;
        };

        class AbstractChunk : public ChunkInterface
        {
            public:
                virtual ~AbstractChunk();

                virtual std::string   getContentType        () const override;
                virtual RequestStatus getRequestStatus      () const override;
                virtual size_t        getBytesRead          () const override;
                virtual bool          hasMoreData           () const override;
                uint64_t              getStartByteInFile    () const;

                virtual block_t *   readBlock       () override;
                virtual block_t *   read            (size_t) override;

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
            friend class HTTPConnectionManager;

            public:
                virtual ~HTTPChunkSource();

                virtual block_t *   readBlock       ()  override;
                virtual block_t *   read            (size_t)  override;
                virtual bool        hasMoreData     () const  override;
                virtual size_t      getBytesRead    () const  override;
                virtual std::string getContentType  () const  override;
                virtual void        recycle() override;

                static const size_t CHUNK_SIZE = 32768;
                static StorageID makeStorageID(const std::string &, const BytesRange &);

            protected:
                HTTPChunkSource(const std::string &url, AbstractConnectionManager *,
                                const ID &, ChunkType, const BytesRange &,
                                bool = false);

                virtual bool        prepare();
                void                setIdentifier(const std::string &, const BytesRange &);
                AbstractConnection    *connection;
                AbstractConnectionManager *connManager;
                mutable vlc_mutex_t lock;
                size_t              consumed; /* read pointer */
                bool                prepared;
                bool                eof;
                ID                  sourceid;
                vlc_tick_t          requestStartTime;
                vlc_tick_t          responseTime;
                vlc_tick_t          downloadEndTime;

            private:
                bool init(const std::string &);
                ConnectionParams    params;
        };

        class HTTPChunkBufferedSource : public HTTPChunkSource
        {
            friend class HTTPConnectionManager;
            friend class Downloader;

            public:
                virtual ~HTTPChunkBufferedSource();
                virtual block_t *  readBlock       ()  override;
                virtual block_t *  read            (size_t)  override;
                virtual bool       hasMoreData     () const  override;
                virtual void        recycle() override;

            protected:
                HTTPChunkBufferedSource(const std::string &url, AbstractConnectionManager *,
                                        const ID &, ChunkType, const BytesRange &,
                                        bool = false);
                void               bufferize(size_t);
                bool               isDone() const;
                void               hold();
                void               release();

            private:
                block_t            *p_head; /* read cache buffer */
                block_t           **pp_tail;
                const block_t      *p_read;
                size_t              inblockreadoffset;
                size_t              buffered; /* read cache size */
                bool                done;
                bool                eof;
                vlc_cond_t          avail;
                bool                held;
        };

        class HTTPChunk : public AbstractChunk
        {
            public:
                HTTPChunk(const std::string &url, AbstractConnectionManager *,
                          const ID &, ChunkType, const BytesRange &);
                virtual ~HTTPChunk();

            protected:
                virtual void        onDownload      (block_t **)  override {}
        };

        class ProbeableChunk : public ChunkInterface
        {
            public:
                ProbeableChunk(ChunkInterface *);
                virtual ~ProbeableChunk();

                virtual std::string getContentType  () const override;
                virtual RequestStatus getRequestStatus() const override;

                virtual block_t *   readBlock       () override;
                virtual block_t *   read            (size_t) override;
                virtual bool        hasMoreData     () const override;
                virtual size_t      getBytesRead    () const override;

                size_t peek(const uint8_t **);

            private:
                ChunkInterface *source;
                block_t *peekblock;
        };
    }
}

#endif /* CHUNK_H_ */
