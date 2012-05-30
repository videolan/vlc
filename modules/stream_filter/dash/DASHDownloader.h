/*
 * DASHDownloader.h
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

#ifndef DASHDOWNLOADER_H_
#define DASHDOWNLOADER_H_

#include "http/HTTPConnectionManager.h"
#include "adaptationlogic/IAdaptationLogic.h"
#include "buffer/BlockBuffer.h"

#define BLOCKSIZE           32768
#define CHUNKDEFAULTBITRATE 1

#include <iostream>

namespace dash
{
    struct thread_sys_t
    {
        dash::http::HTTPConnectionManager   *conManager;
        buffer::BlockBuffer                 *buffer;
    };

    class DASHDownloader
    {
        public:
            DASHDownloader          (http::HTTPConnectionManager *conManager, buffer::BlockBuffer *buffer);
            virtual ~DASHDownloader ();

            bool            start       ();
            static void*    download    (void *);

        private:
            thread_sys_t    *t_sys;
            vlc_thread_t    dashDLThread;
    };
}

#endif /* DASHDOWNLOADER_H_ */
