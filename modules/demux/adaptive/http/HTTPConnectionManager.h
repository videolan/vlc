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

#include "../logic/IDownloadRateObserver.h"
#include "BytesRange.hpp"

#include <vlc_common.h>

#include <vector>
#include <list>
#include <string>

namespace adaptive
{
    namespace http
    {
        class ConnectionParams;
        class AbstractConnectionFactory;
        class AbstractConnection;
        class Downloader;
        class AbstractChunkSource;
        class HTTPChunkBufferedSource;
        enum class ChunkType;

        class AbstractConnectionManager : public IDownloadRateObserver
        {
            public:
                AbstractConnectionManager(vlc_object_t *);
                ~AbstractConnectionManager();
                virtual void    closeAllConnections () = 0;
                virtual AbstractConnection * getConnection(ConnectionParams &) = 0;
                virtual AbstractChunkSource *makeSource(const std::string &,
                                                        const ID &, ChunkType,
                                                        const BytesRange &) = 0;
                virtual void recycleSource(AbstractChunkSource *) = 0;

                virtual void start(AbstractChunkSource *) = 0;
                virtual void cancel(AbstractChunkSource *) = 0;

                virtual void updateDownloadRate(const ID &, size_t,
                                                mtime_t, mtime_t) override;
                void setDownloadRateObserver(IDownloadRateObserver *);

            protected:
                void deleteSource(AbstractChunkSource *);
                vlc_object_t                                       *p_object;

            private:
                IDownloadRateObserver                              *rateObserver;
        };

        class HTTPConnectionManager : public AbstractConnectionManager
        {
            public:
                HTTPConnectionManager           (vlc_object_t *p_object);
                virtual ~HTTPConnectionManager  ();

                virtual void    closeAllConnections ()  override;
                virtual AbstractConnection * getConnection(ConnectionParams &)  override;
                virtual AbstractChunkSource *makeSource(const std::string &,
                                                        const ID &, ChunkType,
                                                        const BytesRange &) override;
                virtual void recycleSource(AbstractChunkSource *) override;

                virtual void start(AbstractChunkSource *)  override;
                virtual void cancel(AbstractChunkSource *)  override;
                void         setLocalConnectionsAllowed();
                void         addFactory(AbstractConnectionFactory *);

            private:
                void    releaseAllConnections ();
                Downloader                                         *downloader;
                Downloader                                         *downloaderhp;
                vlc_mutex_t                                         lock;
                std::vector<AbstractConnection *>                   connectionPool;
                std::list<AbstractConnectionFactory *>              factories;
                bool                                                localAllowed;
                AbstractConnection * reuseConnection(ConnectionParams &);
                Downloader * getDownloadQueue(const AbstractChunkSource *) const;
                std::list<HTTPChunkBufferedSource *> cache;
                unsigned cache_total;
                unsigned cache_max;
        };
    }
}

#endif /* HTTPCONNECTIONMANAGER_H_ */
