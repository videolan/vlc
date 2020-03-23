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

namespace adaptive
{
    namespace playlist
    {
        class AbstractPlaylist;
        class BasePeriod;
    }

    namespace http
    {
        class AbstractConnectionManager;
    }

    using namespace playlist;
    using namespace logic;

    class PlaylistManager
    {
        public:
            PlaylistManager( demux_t *,
                             SharedResources *,
                             AbstractPlaylist *,
                             AbstractStreamFactory *,
                             AbstractAdaptationLogic::LogicType type );
            virtual ~PlaylistManager    ();

            bool    init();
            bool    start();
            bool    started() const;
            void    stop();

            AbstractStream::buffering_status bufferize(mtime_t, unsigned, unsigned);
            AbstractStream::status dequeue(mtime_t, mtime_t *);
            void drain();

            virtual bool needsUpdate() const;
            virtual bool updatePlaylist();
            virtual void scheduleNextUpdate();

            /* static callbacks */
            static int control_callback(demux_t *, int, va_list);
            static int demux_callback(demux_t *);

        protected:
            /* Demux calls */
            virtual int doControl(int, va_list);
            virtual int doDemux(int64_t);

            virtual bool    setPosition(mtime_t);
            mtime_t getPCR() const;
            mtime_t getFirstDTS() const;

            virtual mtime_t getFirstPlaybackTime() const;
            mtime_t getCurrentDemuxTime() const;

            virtual bool reactivateStream(AbstractStream *);
            bool setupPeriod();
            void unsetPeriod();

            void updateControlsPosition();

            /* local factories */
            virtual AbstractAdaptationLogic *createLogic(AbstractAdaptationLogic::LogicType,
                                                         AbstractConnectionManager *);
            virtual AbstractBufferingLogic *createBufferingLogic() const;

            SharedResources                     *resources;
            AbstractAdaptationLogic::LogicType  logicType;
            AbstractAdaptationLogic             *logic;
            AbstractBufferingLogic              *bufferingLogic;
            AbstractPlaylist                    *playlist;
            AbstractStreamFactory               *streamFactory;
            demux_t                             *p_demux;
            std::vector<AbstractStream *>        streams;
            BasePeriod                          *currentPeriod;

            /* shared with demux/buffering */
            struct
            {
                mtime_t     i_nzpcr;
                mtime_t     i_firstpcr;
                vlc_mutex_t lock;
                vlc_cond_t  cond;
            } demux;

            /* buffering process */
            time_t                               nextPlaylistupdate;
            int                                  failedupdates;

            /* Controls */
            struct
            {
                bool        b_live;
                mtime_t     i_time;
                double      f_position;
                mutable vlc_mutex_t lock;
                mtime_t     playlistStart;
                mtime_t     playlistEnd;
                mtime_t     playlistLength;
                time_t      lastupdate;
            } cached;

        private:
            void setBufferingRunState(bool);
            void Run();
            static void * managerThread(void *);
            vlc_mutex_t  lock;
            vlc_thread_t thread;
            bool         b_thread;
            vlc_cond_t   waitcond;
            bool         b_buffering;
            bool         b_canceled;
    };

}

#endif /* PLAYLISTMANAGER_H_ */
