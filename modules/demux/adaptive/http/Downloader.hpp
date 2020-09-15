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
#ifndef DOWNLOADER_HPP
#define DOWNLOADER_HPP

#include "Chunk.h"

#include <vlc_common.h>
#include <vlc_cxx_helpers.hpp>
#include <list>

namespace adaptive
{

    namespace http
    {

        class Downloader
        {
            public:
                Downloader();
                ~Downloader();
                bool start();
                void schedule(HTTPChunkBufferedSource *);
                void cancel(HTTPChunkBufferedSource *);

            private:
                static void * downloaderThread(void *);
                void Run();
                void DownloadSource(HTTPChunkBufferedSource *);
                void kill();
                vlc_thread_t thread_handle;
                vlc::threads::mutex lock;
                vlc::threads::condition_variable wait_cond;
                bool         thread_handle_valid;
                bool         killed;
                std::list<HTTPChunkBufferedSource *> chunks;
        };

    }

}

#endif // DOWNLOADER_HPP
