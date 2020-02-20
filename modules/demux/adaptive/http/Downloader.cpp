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
    vlc_mutex_init(&lock);
    vlc_cond_init(&waitcond);
    killed = false;
    thread_handle_valid = false;
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
    vlc_mutex_lock( &lock );
    killed = true;
    vlc_cond_signal(&waitcond);
    vlc_mutex_unlock( &lock );

    if(thread_handle_valid)
        vlc_join(thread_handle, NULL);
}
void Downloader::schedule(HTTPChunkBufferedSource *source)
{
    vlc_mutex_lock(&lock);
    source->hold();
    chunks.push_back(source);
    vlc_cond_signal(&waitcond);
    vlc_mutex_unlock(&lock);
}

void Downloader::cancel(HTTPChunkBufferedSource *source)
{
    vlc_mutex_lock(&lock);
    source->release();
    chunks.remove(source);
    vlc_mutex_unlock(&lock);
}

void * Downloader::downloaderThread(void *opaque)
{
    Downloader *instance = static_cast<Downloader *>(opaque);
    int canc = vlc_savecancel();
    instance->Run();
    vlc_restorecancel( canc );
    return NULL;
}

void Downloader::DownloadSource(HTTPChunkBufferedSource *source)
{
    if(!source->isDone())
        source->bufferize(HTTPChunkSource::CHUNK_SIZE);
}

void Downloader::Run()
{
    vlc_mutex_lock(&lock);
    while(1)
    {
        while(chunks.empty() && !killed)
            vlc_cond_wait(&waitcond, &lock);

        if(killed)
            break;

        if(!chunks.empty())
        {
            HTTPChunkBufferedSource *source = chunks.front();
            DownloadSource(source);
            if(source->isDone())
            {
                chunks.pop_front();
                source->release();
            }
        }
    }
    vlc_mutex_unlock(&lock);
}
