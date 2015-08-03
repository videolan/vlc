/*
 * PlaylistManager.h
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *             2015 VideoLAN and VLC Authors
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

#ifndef PLAYLISTMANAGER_H_
#define PLAYLISTMANAGER_H_

#include "logic/AbstractAdaptationLogic.h"
#include "Streams.hpp"
#include <vector>

namespace adaptative
{
    namespace playlist
    {
        class AbstractPlaylist;
        class BasePeriod;
    }

    namespace http
    {
        class HTTPConnectionManager;
    }

    using namespace playlist;
    using namespace logic;
    using namespace http;

    class AbstractStreamFactory;
    class AbstractStreamOutputFactory;

    class PlaylistManager
    {
        public:
            PlaylistManager( demux_t *, AbstractPlaylist *,
                             AbstractStreamOutputFactory *,
                             AbstractAdaptationLogic::LogicType type );
            virtual ~PlaylistManager    ();

            bool    start();

            Stream::status demux(mtime_t, bool);
            mtime_t getDuration() const;
            mtime_t getPCR() const;
            mtime_t getFirstDTS() const;
            int     esCount() const;
            bool    setPosition(mtime_t);
            bool    seekAble() const;
            virtual bool updatePlaylist();

            /* static callbacks */
            static int control_callback(demux_t *, int, va_list);
            static int demux_callback(demux_t *);

        protected:
            /* Demux calls */
            virtual int doControl(int, va_list);
            virtual int doDemux(int64_t);

            bool setupPeriod();
            void unsetPeriod();
            /* local factories */
            virtual AbstractAdaptationLogic *createLogic(AbstractAdaptationLogic::LogicType);

            HTTPConnectionManager              *conManager;
            AbstractAdaptationLogic::LogicType  logicType;
            AbstractPlaylist                    *playlist;
            AbstractStreamOutputFactory         *streamOutputFactory;
            demux_t                             *p_demux;
            std::vector<Stream *>                streams;
            mtime_t                              nextPlaylistupdate;
            mtime_t                              i_nzpcr;
            BasePeriod                          *currentPeriod;
    };

}

#endif /* PLAYLISTMANAGER_H_ */
