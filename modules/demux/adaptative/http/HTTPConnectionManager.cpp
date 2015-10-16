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
#include "Chunk.h"
#include "Sockets.hpp"

using namespace adaptative::http;

HTTPConnectionManager::HTTPConnectionManager    (vlc_object_t *stream) :
                       stream                   (stream)
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

bool HTTPConnectionManager::connectChunk(Chunk *chunk)
{
    if(chunk == NULL)
        return false;
    if(chunk->getConnection())
        return true;

    msg_Dbg(stream, "Retrieving %s @%zu", chunk->getUrl().c_str(),
            chunk->getBytesRange().isValid() ? chunk->getBytesRange().getStartByte() : 0);

    const int sockettype = (chunk->getScheme() == "https") ? TLSSocket::TLS : Socket::REGULAR;
    HTTPConnection *conn = getConnection(chunk->getHostname(), chunk->getPort(), sockettype);
    if(!conn)
    {
        Socket *socket = (sockettype == TLSSocket::TLS) ? new (std::nothrow) TLSSocket()
                                                        : new (std::nothrow) Socket();
        if(!socket)
            return false;
        /* disable pipelined tls until we have ticket/resume session support */
        conn = new (std::nothrow) HTTPConnection(stream, socket, sockettype != TLSSocket::TLS);
        if(!conn)
        {
            delete socket;
            return false;
        }

        connectionPool.push_back(conn);

        if (!conn->connect(chunk->getHostname(), chunk->getPort()))
        {
            return false;
        }
    }

    conn->setUsed(true);
    chunk->setConnection(conn);

    return true;
}
