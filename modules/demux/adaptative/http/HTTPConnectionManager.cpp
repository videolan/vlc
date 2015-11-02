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
#include "Sockets.hpp"
#include <vlc_url.h>

using namespace adaptative::http;

HTTPConnectionManager::HTTPConnectionManager    (vlc_object_t *stream) :
                       stream                   (stream),
                       rateObserver             (NULL)
{
}
HTTPConnectionManager::~HTTPConnectionManager   ()
{
    this->closeAllConnections();
}

void HTTPConnectionManager::closeAllConnections      ()
{
    releaseAllConnections();
    vlc_delete_all(this->connectionPool);
}

void HTTPConnectionManager::releaseAllConnections()
{
    std::vector<HTTPConnection *>::iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
        (*it)->setUsed(false);
}

HTTPConnection * HTTPConnectionManager::getConnection(const std::string &hostname, uint16_t port, int sockettype)
{
    std::vector<HTTPConnection *>::const_iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
    {
        HTTPConnection *conn = *it;
        if(conn->isAvailable() && conn->compare(hostname, port, sockettype))
            return conn;
    }
    return NULL;
}

HTTPConnection * HTTPConnectionManager::getConnection(const std::string &scheme,
                                                      const std::string &hostname,
                                                      uint16_t port)
{
    if((scheme != "http" && scheme != "https") || hostname.empty())
        return NULL;

    const int sockettype = (scheme == "https") ? TLSSocket::TLS : Socket::REGULAR;
    HTTPConnection *conn = getConnection(hostname, port, sockettype);
    if(!conn)
    {
        Socket *socket = (sockettype == TLSSocket::TLS) ? new (std::nothrow) TLSSocket()
                                                        : new (std::nothrow) Socket();
        if(!socket)
            return NULL;
        /* disable pipelined tls until we have ticket/resume session support */
        conn = new (std::nothrow) HTTPConnection(stream, socket, sockettype != TLSSocket::TLS);
        if(!conn)
        {
            delete socket;
            return NULL;
        }

        connectionPool.push_back(conn);

        if (!conn->connect(hostname, port))
        {
            return NULL;
        }
    }

    conn->setUsed(true);
    return conn;
}

void HTTPConnectionManager::updateDownloadRate(size_t size, mtime_t time)
{
    if(rateObserver)
        rateObserver->updateDownloadRate(size, time);
}

void HTTPConnectionManager::setDownloadRateObserver(IDownloadRateObserver *obs)
{
    rateObserver = obs;
}
