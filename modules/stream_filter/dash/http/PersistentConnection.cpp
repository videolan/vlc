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

#include <vlc_network.h>

using namespace dash::http;

const int PersistentConnection::RETRY = 5;

PersistentConnection::PersistentConnection  (stream_t *stream) :
                      HTTPConnection        (stream),
                      isInit                (false)
{
}
PersistentConnection::~PersistentConnection ()
{
}

int                 PersistentConnection::read              (void *p_buffer, size_t len)
{
    if(this->chunkQueue.size() == 0)
        return -1;

    Chunk *readChunk = this->chunkQueue.front();

    if(readChunk->getBytesRead() == 0)
    {
        if(!this->initChunk(readChunk))
        {
            this->chunkQueue.pop_front();
            return -1;
        }
    }

    if(readChunk->getBytesToRead() == 0)
    {
        this->chunkQueue.pop_front();
        return 0;
    }

    int ret = 0;
    if(len > readChunk->getBytesToRead())
        ret = HTTPConnection::read(p_buffer, readChunk->getBytesToRead());
    else
        ret = HTTPConnection::read(p_buffer, len);

    if(ret <= 0)
    {
        readChunk->setStartByte(readChunk->getStartByte() + readChunk->getBytesRead());
        readChunk->setBytesRead(0);
        if(!this->reconnect(readChunk))
        {
            this->chunkQueue.pop_front();
            return -1;
        }

        return this->read(p_buffer, len);
    }

    readChunk->setBytesRead(readChunk->getBytesRead() + ret);

    return ret;
}

bool                PersistentConnection::init              (Chunk *chunk)
{
    if(isInit)
        return true;

    if (IHTTPConnection::init(chunk))
    {
        isInit = true;
        chunkQueue.push_back(chunk);
        hostname = chunk->getHostname();
    }

    return isInit;
}
bool                PersistentConnection::addChunk          (Chunk *chunk)
{
    if(chunk == NULL)
        return false;

    if(!this->isInit)
        return this->init(chunk);

    if(!chunk->hasHostname())
    {
        chunk->setUrl(getUrlRelative(chunk));
        if(!chunk->hasHostname())
            return false;
    }

    if(chunk->getHostname().compare(this->hostname))
        return false;

    if(send(getRequestHeader(chunk).append("\r\n")))
    {
        this->chunkQueue.push_back(chunk);
        return true;
    }

    return false;
}
bool                PersistentConnection::initChunk         (Chunk *chunk)
{
    HeaderReply reply;
    if(parseHeader(&reply))
    {
        chunk->setLength(reply.contentLength);
        return true;
    }

    if(!reconnect(chunk))
        return false;

    if(parseHeader(&reply))
    {
        chunk->setLength(reply.contentLength);
        return true;
    }

    return false;
}
bool                PersistentConnection::reconnect         (Chunk *chunk)
{
    int         count   = 0;
    std::string request = getRequestHeader(chunk).append("\r\n");

    while(count < this->RETRY)
    {
        this->httpSocket = net_ConnectTCP(this->stream, chunk->getHostname().c_str(), chunk->getPort());
        if(this->httpSocket != -1)
            if(this->resendAllRequests())
                return true;

        count++;
    }

    return false;
}
const std::string&  PersistentConnection::getHostname       () const
{
    return this->hostname;
}
bool                PersistentConnection::isConnected       () const
{
    return this->isInit;
}
bool                PersistentConnection::resendAllRequests ()
{
    for(size_t i = 0; i < chunkQueue.size(); i++)
        if(!send(getRequestHeader(chunkQueue.at(i)).append("\r\n")))
            return false;

    return true;
}

std::string PersistentConnection::getRequestHeader(const Chunk *chunk) const
{
    /* can clearly see here that inheritance is reversed :/ */
    return IHTTPConnection::getRequestHeader(chunk);
}
