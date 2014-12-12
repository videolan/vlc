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
#include "mpd/Segment.h"

#include <vlc_block.h>

#include <vlc_stream.h>

using namespace dash::http;

const uint64_t  HTTPConnectionManager::CHUNKDEFAULTBITRATE    = 1;

HTTPConnectionManager::HTTPConnectionManager    (logic::IAdaptationLogic *adaptationLogic, stream_t *stream) :
                       currentChunk             (NULL),
                       adaptationLogic          (adaptationLogic),
                       stream                   (stream),
                       chunkCount               (0)
{
}
HTTPConnectionManager::~HTTPConnectionManager   ()
{
    this->closeAllConnections();
}

void                                HTTPConnectionManager::closeAllConnections      ()
{
    vlc_delete_all(this->connectionPool);
    for(int i=0; i<Streams::count; i++)
        vlc_delete_all(downloadQueue[i]);
    delete currentChunk;
}

ssize_t HTTPConnectionManager::read(Streams::Type type, block_t **pp_block)
{
    Chunk *chunk;

    if(downloadQueue[type].empty())
    {
        chunk = adaptationLogic->getNextChunk(type);
        if(!connectChunk(chunk))
            return -1;
        else
            downloadQueue[type].push_back(chunk);
    }

    chunk = downloadQueue[type].front();

    if(chunk->getBytesRead() == 0)
    {
        if (!chunk->getConnection()->query(chunk->getPath()))
            return -1;
    }

    /* chunk length should be set at connect/query reply time */
    size_t readsize = chunk->getBytesToRead();
    if (readsize > 128000)
        readsize = 32768;

    block_t *block = block_Alloc(readsize);
    if(!block)
        return -1;

    mtime_t time = mdate();
    ssize_t ret = chunk->getConnection()->read(block->p_buffer, readsize);
    time = mdate() - time;

    block->i_length = (mtime_t)((ret * 8) / ((float)chunk->getBitrate() / CLOCK_FREQ));

    if(ret <= 0)
    {
        block_Release(block);
        *pp_block = NULL;

        delete(chunk);
        downloadQueue[type].pop_front();

        return read(type, pp_block);
    }
    else
    {
        block->i_buffer = ret;
        adaptationLogic->updateDownloadRate(block->i_buffer, time);

        if (chunk->getBytesToRead() == 0)
        {
            chunk->onDownload(block->p_buffer, block->i_buffer);
            delete chunk;
            downloadQueue[type].pop_front();
        }
    }

    *pp_block = block;

    return ret;
}

PersistentConnection * HTTPConnectionManager::getConnectionForHost(const std::string &hostname)
{
    std::vector<PersistentConnection *>::const_iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); it++)
    {
        if(!(*it)->getHostname().compare(hostname))
            return *it;
    }
    return NULL;
}

bool HTTPConnectionManager::connectChunk(Chunk *chunk)
{
    if(chunk == NULL)
        return false;

    msg_Dbg(stream, "Retrieving %s", chunk->getUrl().c_str());

    PersistentConnection *conn = getConnectionForHost(chunk->getHostname());
    if(!conn)
    {
        conn = new PersistentConnection(stream, chunk);
        if(!conn)
            return false;
        if (!chunk->getConnection()->connect(chunk->getHostname(), chunk->getPort()))
            return false;
        connectionPool.push_back(conn);
    }

    conn->bindChunk(chunk);

    chunkCount++;

    if(chunk->getBitrate() <= 0)
        chunk->setBitrate(HTTPConnectionManager::CHUNKDEFAULTBITRATE);

    return true;
}
