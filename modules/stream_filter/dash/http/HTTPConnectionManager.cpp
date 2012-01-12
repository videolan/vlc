/*
 * HTTPConnectionManager.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnectionManager.h"
#include "mpd/Segment.h"

using namespace dash::http;
using namespace dash::logic;

HTTPConnectionManager::HTTPConnectionManager    (stream_t *stream)
{
    this->timeSecSession    = 0;
    this->bytesReadSession  = 0;
    this->timeSecChunk      = 0;
    this->bytesReadChunk    = 0;
    this->bpsAvg            = 0;
    this->bpsLastChunk      = 0;
    this->chunkCount        = 0;
    this->stream            = stream;
}
HTTPConnectionManager::~HTTPConnectionManager   ()
{
    this->closeAllConnections();
}

bool                HTTPConnectionManager::closeConnection( IHTTPConnection *con )
{
    for(std::map<Chunk*, HTTPConnection *>::iterator it = this->chunkMap.begin();
        it != this->chunkMap.end(); ++it)
    {
        if( it->second == con )
        {
            delete con;
            this->chunkMap.erase( it );
            return true;
        }
    }
    return false;
}

bool                HTTPConnectionManager::closeConnection( Chunk *chunk )
{
    HTTPConnection *con = this->chunkMap[chunk];
    bool ret = this->closeConnection(con);
    this->chunkMap.erase(chunk);
    delete(chunk);
    return ret;
}

void                HTTPConnectionManager::closeAllConnections      ()
{
    std::map<Chunk *, HTTPConnection *>::iterator it;

    for(it = this->chunkMap.begin(); it != this->chunkMap.end(); ++it)
        delete(it->second);

    this->chunkMap.clear();
}

int                 HTTPConnectionManager::read( Chunk *chunk, void *p_buffer, size_t len )
{
    if(this->chunkMap.find(chunk) == this->chunkMap.end())
    {
        this->bytesReadChunk    = 0;
        this->timeSecChunk      = 0;

        if ( this->initConnection( chunk ) == NULL )
            return -1;
    }

    mtime_t start = mdate();
    int ret = this->chunkMap[chunk]->read(p_buffer, len);
    mtime_t end = mdate();

    if( ret <= 0 )
        this->closeConnection( chunk );
    else
    {
        double time = ((double)(end - start)) / 1000000;

        this->bytesReadSession += ret;
        this->bytesReadChunk   += ret;
        this->timeSecSession   += time;
        this->timeSecChunk     += time;


        if(this->timeSecSession > 0)
            this->bpsAvg = (this->bytesReadSession / this->timeSecSession) * 8;

        if(this->timeSecChunk > 0)
            this->bpsLastChunk = (this->bytesReadChunk / this->timeSecChunk) * 8;

        if(this->bpsAvg < 0 || this->chunkCount < 2)
            this->bpsAvg = 0;

        if(this->bpsLastChunk < 0 || this->chunkCount < 2)
            this->bpsLastChunk = 0;

        this->notify();
    }
    return ret;
}

int                 HTTPConnectionManager::peek                     (Chunk *chunk, const uint8_t **pp_peek, size_t i_peek)
{
    if(this->chunkMap.find(chunk) == this->chunkMap.end())
    {
        if ( this->initConnection(chunk) == NULL )
            return -1;
    }
    return this->chunkMap[chunk]->peek(pp_peek, i_peek);
}

IHTTPConnection*     HTTPConnectionManager::initConnection(Chunk *chunk)
{
    HTTPConnection *con = new HTTPConnection(chunk->getUrl(), this->stream);
    if ( con->init() == false )
        return NULL;
    this->chunkMap[chunk] = con;
    this->chunkCount++;
    return con;
}
void                HTTPConnectionManager::attach                   (IDownloadRateObserver *observer)
{
    this->rateObservers.push_back(observer);
}
void                HTTPConnectionManager::notify                   ()
{
    if ( this->bpsAvg <= 0 )
        return ;
    for(size_t i = 0; i < this->rateObservers.size(); i++)
        this->rateObservers.at(i)->downloadRateChanged(this->bpsAvg, this->bpsLastChunk);
}
