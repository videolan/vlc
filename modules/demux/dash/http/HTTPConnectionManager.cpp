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
#include "http/Chunk.h"

#include <vlc_stream.h>

using namespace dash::http;

const uint64_t  HTTPConnectionManager::CHUNKDEFAULTBITRATE    = 1;

HTTPConnectionManager::HTTPConnectionManager    (stream_t *stream) :
                       stream                   (stream)
{
}
HTTPConnectionManager::~HTTPConnectionManager   ()
{
    this->closeAllConnections();
}

void                                HTTPConnectionManager::closeAllConnections      ()
{
    releaseAllConnections();
    vlc_delete_all(this->connectionPool);
}

void HTTPConnectionManager::releaseAllConnections()
{
    std::vector<PersistentConnection *>::iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); it++)
        (*it)->releaseChunk();
}

PersistentConnection * HTTPConnectionManager::getConnectionForHost(const std::string &hostname)
{
    std::vector<PersistentConnection *>::const_iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); it++)
    {
        if(!(*it)->getHostname().compare(hostname) && (*it)->isAvailable())
            return *it;
    }
    return NULL;
}

bool HTTPConnectionManager::connectChunk(Chunk *chunk)
{
    if(chunk == NULL)
        return false;
    if(chunk->getConnection())
        return true;

    msg_Dbg(stream, "Retrieving %s", chunk->getUrl().c_str());

    PersistentConnection *conn = getConnectionForHost(chunk->getHostname());
    if(!conn)
    {
        conn = new PersistentConnection(stream, chunk);
        if(!conn)
            return false;
        connectionPool.push_back(conn);
        if (!chunk->getConnection()->connect(chunk->getHostname(), chunk->getPort()))
            return false;
    }

    conn->bindChunk(chunk);

    if(chunk->getBitrate() <= 0)
        chunk->setBitrate(HTTPConnectionManager::CHUNKDEFAULTBITRATE);

    return true;
}
