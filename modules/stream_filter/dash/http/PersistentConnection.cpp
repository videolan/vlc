/*
 * PersistentConnection.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
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

#include "PersistentConnection.h"
#include "Chunk.h"

#include <vlc_network.h>

using namespace dash::http;

PersistentConnection::PersistentConnection  (stream_t *stream, Chunk *chunk) :
                      HTTPConnection        (stream, chunk)
{
    queryOk = false;
    retries = 0;
}
#include <cassert>
ssize_t PersistentConnection::read(void *p_buffer, size_t len)
{
    if(!chunk)
        return -1;

    if(len == 0)
        return 0;

    if(chunk->getBytesRead() == 0 && !queryOk)
    {
        if(!query(chunk->getPath()))
            return -1;
    }

    assert(connected() && queryOk);

    if(chunk->getBytesToRead() == 0)
        return 0;

    if(len > chunk->getBytesToRead())
        len = chunk->getBytesToRead();

    ssize_t ret = IHTTPConnection::read(p_buffer, len);
    if(ret <= 0)
    {
        chunk->setStartByte(chunk->getStartByte() + chunk->getBytesRead());
        chunk->setBytesRead(0);
        disconnect();
        if(retries++ == retryCount || !query(chunk->getPath()))
            return -1;

        return read(p_buffer, len);
    }

    retries = 0;
    chunk->setBytesRead(chunk->getBytesRead() + ret);

    return ret;
}

bool PersistentConnection::query(const std::string &path)
{
    if(!connected() &&
       !connect(chunk->getHostname(), chunk->getPort()))
        return false;

    queryOk = IHTTPConnection::query(path);
    return queryOk;
}

bool PersistentConnection::connect(const std::string &hostname, int port)
{
    assert(!connected());
    assert(!queryOk);
    return IHTTPConnection::connect(hostname, port);
}

void PersistentConnection::releaseChunk()
{
    if(!chunk)
        return;
    if(toRead > 0 && connected()) /* We can't resend request if we haven't finished reading */
        disconnect();
    HTTPConnection::releaseChunk();
}

void PersistentConnection::disconnect()
{
    queryOk = false;
    IHTTPConnection::disconnect();
}

const std::string& PersistentConnection::getHostname() const
{
    return hostname;
}

std::string PersistentConnection::buildRequestHeader(const std::string &path) const
{
    return IHTTPConnection::buildRequestHeader(path);
}
