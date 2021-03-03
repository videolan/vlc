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
#include "Transport.hpp"
#include "Downloader.hpp"
#include "tools/Debug.hpp"
#include <vlc_url.h>
#include <vlc_http.h>

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


HTTPConnectionManager::HTTPConnectionManager    (vlc_object_t *p_object_)
    : AbstractConnectionManager( p_object_ ),
      localAllowed(false)
{
    vlc_mutex_init(&lock);
    downloader = new (std::nothrow) Downloader();
    downloader->start();
}

HTTPConnectionManager::~HTTPConnectionManager   ()
{
    delete downloader;
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
    if(unlikely(factories.empty() || !downloader))
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

void HTTPConnectionManager::start(AbstractChunkSource *source)
{
    HTTPChunkBufferedSource *src = dynamic_cast<HTTPChunkBufferedSource *>(source);
    if(src)
        downloader->schedule(src);
}

void HTTPConnectionManager::cancel(AbstractChunkSource *source)
{
    HTTPChunkBufferedSource *src = dynamic_cast<HTTPChunkBufferedSource *>(source);
    if(src)
        downloader->cancel(src);
}

void HTTPConnectionManager::setLocalConnectionsAllowed()
{
    localAllowed = true;
}

void HTTPConnectionManager::addFactory(AbstractConnectionFactory *factory)
{
    factories.push_back(factory);
}
