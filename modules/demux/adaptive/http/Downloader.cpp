/*
 * Downloader.hpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN Authors
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

#include "Downloader.hpp"

#include <vlc_threads.h>

#include <atomic>

using namespace adaptive::http;

Downloader::Downloader()
{
    killed = false;
    thread_handle_valid = false;
    current = NULL;
}

bool Downloader::start()
{
    if(!thread_handle_valid &&
       vlc_clone(&thread_handle, downloaderThread,
                 static_cast<void *>(this), VLC_THREAD_PRIORITY_INPUT))
    {
        return false;
    }
    thread_handle_valid = true;
    return true;
}

Downloader::~Downloader()
{
    kill();

    if(thread_handle_valid)
        vlc_join(thread_handle, NULL);
}

void Downloader::kill()
{
    vlc::threads::mutex_locker locker {lock};
    killed = true;
    wait_cond.signal();
}

void Downloader::schedule(HTTPChunkBufferedSource *source)
{
    vlc::threads::mutex_locker locker {lock};
    source->hold();
    chunks.push_back(source);
    wait_cond.signal();
}

void Downloader::cancel(HTTPChunkBufferedSource *source)
{
    vlc::threads::mutex_locker locker {lock};
    while (current == source)
        updated_cond.wait(lock);

    if(!source->isDone())
    {
        chunks.remove(source);
        source->release();
    }
}

void * Downloader::downloaderThread(void *opaque)
{
    Downloader *instance = static_cast<Downloader *>(opaque);
    instance->Run();
    return NULL;
}

void Downloader::Run()
{
    while(1)
    {
        lock.lock();

        while(chunks.empty() && !killed)
            wait_cond.wait(lock);

        if(killed)
        {
            lock.unlock();
            break;
        }

        current = chunks.front();
        lock.unlock();
        current->bufferize(HTTPChunkSource::CHUNK_SIZE);
        lock.lock();
        if(current->isDone())
        {
            chunks.pop_front();
            current->release();
        }
        current = NULL;
        updated_cond.signal();
        lock.unlock();
    }
}
