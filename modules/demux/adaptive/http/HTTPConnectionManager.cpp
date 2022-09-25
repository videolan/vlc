/*
 * HTTPConnectionManager.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnectionManager.h"
#include "HTTPConnection.hpp"
#include "ConnectionParams.hpp"
#include "Downloader.hpp"
#include "../tools/Debug.hpp"
#include <vlc_url.h>
#include <vlc_http.h>

#include <cassert>

using namespace adaptive::http;

AbstractConnectionManager::AbstractConnectionManager(vlc_object_t *p_object_)
    : IDownloadRateObserver()
{
    p_object = p_object_;
    rateObserver = nullptr;
}

AbstractConnectionManager::~AbstractConnectionManager()
{

}

void AbstractConnectionManager::updateDownloadRate(const adaptive::ID &sourceid, size_t size,
                                                   vlc_tick_t time, vlc_tick_t latency)
{
    if(rateObserver)
    {
        BwDebug(msg_Dbg(p_object,
                "%" PRId64 "Kbps downloaded %zuKBytes in %" PRId64 "ms latency %" PRId64 "ms [%s]",
                1000 * size * 8 / (time ? time : 1), size / 1024, MS_FROM_VLC_TICK(time),
                latency / 1000, sourceid.str().c_str()));
        rateObserver->updateDownloadRate(sourceid, size, time, latency);
    }
}

void AbstractConnectionManager::setDownloadRateObserver(IDownloadRateObserver *obs)
{
    rateObserver = obs;
}

void AbstractConnectionManager::deleteSource(AbstractChunkSource *source)
{
    delete source;
}

HTTPConnectionManager::HTTPConnectionManager    (vlc_object_t *p_object_)
    : AbstractConnectionManager( p_object_ ),
      localAllowed(false)
{
    vlc_mutex_init(&lock);
    downloader = new Downloader();
    downloaderhp = new Downloader();
    downloader->start();
    downloaderhp->start();
    cache_total = 0;
    cache_max = 1 << 19;
}

HTTPConnectionManager::~HTTPConnectionManager   ()
{
    while(!cache.empty())
    {
        HTTPChunkBufferedSource *purged = cache.back();
        cache.pop_back();
        assert(cache_total >= purged->contentLength);
        cache_total -= purged->contentLength;
        CacheDebug(msg_Dbg(p_object, "Cache DEL '%s' usage %u bytes",
                            purged->getStorageID().c_str(), cache_total));
        deleteSource(purged);
    }
    delete downloader;
    delete downloaderhp;
    this->closeAllConnections();
    while(!factories.empty())
    {
        delete factories.front();
        factories.pop_front();
    }
}

void HTTPConnectionManager::closeAllConnections      ()
{
    vlc_mutex_lock(&lock);
    releaseAllConnections();
    vlc_delete_all(this->connectionPool);
    vlc_mutex_unlock(&lock);
}

void HTTPConnectionManager::releaseAllConnections()
{
    std::vector<AbstractConnection *>::iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
        (*it)->setUsed(false);
}

AbstractConnection * HTTPConnectionManager::reuseConnection(ConnectionParams &params)
{
    std::vector<AbstractConnection *>::const_iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
    {
        AbstractConnection *conn = *it;
        if(conn->canReuse(params))
            return conn;
    }
    return nullptr;
}

AbstractConnection * HTTPConnectionManager::getConnection(ConnectionParams &params)
{
    if(unlikely(factories.empty() || !downloader || !downloaderhp))
        return nullptr;

    if(params.isLocal())
    {
        if(!localAllowed)
            return nullptr;
    }

    vlc_mutex_lock(&lock);
    AbstractConnection *conn = reuseConnection(params);
    if(!conn)
    {
        for(auto it = factories.begin(); it != factories.end() && !conn; ++it)
            conn = (*it)->createConnection(p_object, params);

        if(!conn)
        {
            vlc_mutex_unlock(&lock);
            return nullptr;
        }

        connectionPool.push_back(conn);

        if (!conn->prepare(params))
        {
            vlc_mutex_unlock(&lock);
            return nullptr;
        }
    }

    conn->setUsed(true);
    vlc_mutex_unlock(&lock);
    return conn;
}

AbstractChunkSource *HTTPConnectionManager::makeSource(const std::string &url,
                                                       const ID &id, ChunkType type,
                                                       const BytesRange &range)
{
    StorageID storageid = HTTPChunkSource::makeStorageID(url, range);
    switch(type)
    {
        case ChunkType::Init:
        case ChunkType::Index:
            for(HTTPChunkBufferedSource *s : cache)
            {
                if(s->getStorageID() == storageid)
                {
                    cache.remove(s);
                    assert(cache_total >= s->contentLength);
                    cache_total -= s->contentLength;
                    CacheDebug(msg_Dbg(p_object, "Cache GET '%s' usage %u bytes",
                                       storageid.c_str(), cache_total));
                    return s;
                }
            }
            // fallthrough
        case ChunkType::Segment:
        case ChunkType::Key:
        case ChunkType::Playlist:
        default:
            return new HTTPChunkBufferedSource(url, this, id, type, range);
    }
}

void HTTPConnectionManager::recycleSource(AbstractChunkSource *source)
{
    bool b_cacheable;
    switch(source->getChunkType())
    {
        case ChunkType::Index:
        case ChunkType::Init:
            b_cacheable = true;
            break;
        case ChunkType::Key:
        case ChunkType::Playlist:
        case ChunkType::Segment:
        default:
            b_cacheable = false;
            break;
    }

    HTTPChunkBufferedSource *buf = dynamic_cast<HTTPChunkBufferedSource *>(source);
    if(buf && b_cacheable && !buf->getStorageID().empty() &&
       buf->contentLength < cache_max)
    {
        while(cache_max < cache_total + buf->contentLength)
        {
            HTTPChunkBufferedSource *purged = cache.back();
            cache.pop_back();
            assert(cache_total >= purged->contentLength);
            cache_total -= purged->contentLength;
            CacheDebug(msg_Dbg(p_object, "Cache DEL '%s' usage %u bytes",
                               purged->getStorageID().c_str(), cache_total));
            deleteSource(purged);
        }
        cache.push_front(buf);
        cache_total += buf->contentLength;
        CacheDebug(msg_Dbg(p_object, "Cache PUT '%s' usage %u bytes",
                           buf->getStorageID().c_str(), cache_total));
    }
    else
        deleteSource(source);
}

Downloader * HTTPConnectionManager::getDownloadQueue(const AbstractChunkSource *source) const
{
    switch(source->getChunkType())
    {
        case ChunkType::Init:
        case ChunkType::Index:
        case ChunkType::Segment:
            return downloader;
        case ChunkType::Key:
        case ChunkType::Playlist:
        default:
            return downloaderhp;
    }
}

void HTTPConnectionManager::start(AbstractChunkSource *source)
{
    HTTPChunkBufferedSource *src = dynamic_cast<HTTPChunkBufferedSource *>(source);
    if(src && !src->isDone())
        getDownloadQueue(src)->schedule(src);
}

void HTTPConnectionManager::cancel(AbstractChunkSource *source)
{
    HTTPChunkBufferedSource *src = dynamic_cast<HTTPChunkBufferedSource *>(source);
    if(src)
        getDownloadQueue(src)->cancel(src);
}

void HTTPConnectionManager::setLocalConnectionsAllowed()
{
    localAllowed = true;
}

void HTTPConnectionManager::addFactory(AbstractConnectionFactory *factory)
{
    factories.push_back(factory);
}
