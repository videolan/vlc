/*
 * Demuxer.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#ifndef DEMUXER_HPP
#define DEMUXER_HPP

#include <vlc_common.h>
#include <string>

namespace adaptive
{
    class AbstractSourceStream;
    class DemuxerFactoryInterface;
    class StreamFormat;

    class AbstractDemuxer
    {
        public:
            AbstractDemuxer();
            virtual ~AbstractDemuxer();
            virtual int demux(mtime_t) = 0;
            virtual void drain() = 0;
            virtual bool create() = 0;
            virtual void destroy() = 0;
            bool alwaysStartsFromZero() const;
            bool needsRestartOnSeek() const;
            bool needsRestartOnSwitch() const;
            bool needsRestartOnEachSegment() const;
            void setCanDetectSwitches(bool);
            void setRestartsOnEachSegment(bool);

        protected:
            bool b_startsfromzero;
            bool b_reinitsonseek;
            bool b_alwaysrestarts;
            bool b_candetectswitches;
    };

    class MimeDemuxer : public AbstractDemuxer
    {
        public:
            MimeDemuxer(demux_t *, const DemuxerFactoryInterface *,
                        es_out_t *, AbstractSourceStream *);
            virtual ~MimeDemuxer();
            virtual int demux(mtime_t); /* impl */
            virtual void drain(); /* impl */
            virtual bool create(); /* impl */
            virtual void destroy(); /* impl */

        protected:
            AbstractSourceStream *sourcestream;
            demux_t *p_realdemux;
            AbstractDemuxer *demuxer;
            const DemuxerFactoryInterface *factory;
            es_out_t *p_es_out;
    };

    class Demuxer : public AbstractDemuxer
    {
        public:
            Demuxer(demux_t *, const std::string &, es_out_t *, AbstractSourceStream *);
            virtual ~Demuxer();
            virtual int demux(mtime_t); /* impl */
            virtual void drain(); /* impl */
            virtual bool create(); /* impl */
            virtual void destroy(); /* impl */

        protected:
            AbstractSourceStream *sourcestream;
            demux_t *p_realdemux;
            demux_t *p_demux;
            std::string name;
            es_out_t *p_es_out;
            bool b_eof;
    };

    class SlaveDemuxer : public Demuxer
    {
        public:
            SlaveDemuxer(demux_t *, const std::string &, es_out_t *, AbstractSourceStream *);
            virtual ~SlaveDemuxer();
            virtual bool create(); /* reimpl */
            virtual int demux(mtime_t); /* reimpl */

        private:
            mtime_t length;
    };

    class DemuxerFactoryInterface
    {
        public:
            virtual AbstractDemuxer * newDemux(demux_t *, const StreamFormat &,
                                               es_out_t *, AbstractSourceStream *) const = 0;
    };
}

#endif // DEMUXER_HPP
