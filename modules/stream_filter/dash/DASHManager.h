/*
 * DASHManager.h
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
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

#ifndef DASHMANAGER_H_
#define DASHMANAGER_H_

#include "http/HTTPConnectionManager.h"
#include "xml/Node.h"
#include "adaptationlogic/IAdaptationLogic.h"
#include "adaptationlogic/AdaptationLogicFactory.h"
#include "mpd/IMPDManager.h"
#include "mpd/MPDManagerFactory.h"
#include "buffer/BlockBuffer.h"
#include "DASHDownloader.h"
#include "mpd/MPD.h"

namespace dash
{
    class DASHManager
    {
        public:
            DASHManager( mpd::MPD *mpd,
                         logic::IAdaptationLogic::LogicType type, stream_t *stream);
            virtual ~DASHManager    ();

            bool    start         ();
            int     read          ( void *p_buffer, size_t len );
            int     peek          ( const uint8_t **pp_peek, size_t i_peek );
            int     seekBackwards ( unsigned len );

            const mpd::IMPDManager*         getMpdManager   () const;
            const logic::IAdaptationLogic*  getAdaptionLogic() const;
            const http::Chunk *getCurrentChunk() const;

        private:
            http::HTTPConnectionManager         *conManager;
            http::Chunk                         *currentChunk;
            logic::IAdaptationLogic             *adaptationLogic;
            logic::IAdaptationLogic::LogicType  logicType;
            mpd::IMPDManager                    *mpdManager;
            mpd::MPD                            *mpd;
            stream_t                            *stream;
            DASHDownloader                      *downloader;
            buffer::BlockBuffer                 *buffer;
    };
}

#endif /* DASHMANAGER_H_ */
