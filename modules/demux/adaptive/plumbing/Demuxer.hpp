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
            enum Status
            {
                STATUS_SUCCESS,
                STATUS_ERROR,
                STATUS_END_OF_FILE,
            };
            AbstractDemuxer();
            virtual ~AbstractDemuxer();
            virtual Status demux(vlc_tick_t) = 0;
            virtual void drain() = 0;
            virtual bool create() = 0;
            virtual void destroy() = 0;
            bool alwaysStartsFromZero() const;
            bool needsRestartOnSeek() const;
            bool bitstreamSwitchCompatible() const;
            bool needsRestartOnEachSegment() const;
            void setBitstreamSwitchCompatible(bool);
            void setRestartsOnEachSegment(bool);

        protected:
            static Status returnCode(int);
            bool b_startsfromzero;
            bool b_reinitsonseek;
            bool b_alwaysrestarts;
            bool b_candetectswitches;
    };

    class MimeDemuxer : public AbstractDemuxer
    {
        public:
            MimeDemuxer(vlc_object_t *, const DemuxerFactoryInterface *,
                        es_out_t *, AbstractSourceStream *);
            virtual ~MimeDemuxer();
            virtual Status demux(vlc_tick_t); /* impl */
            virtual void drain(); /* impl */
            virtual bool create(); /* impl */
            virtual void destroy(); /* impl */

        protected:
            AbstractSourceStream *sourcestream;
            vlc_object_t *p_obj;
            AbstractDemuxer *demuxer;
            const DemuxerFactoryInterface *factory;
            es_out_t *p_es_out;
    };

    class Demuxer : public AbstractDemuxer
    {
        public:
            Demuxer(vlc_object_t *, const std::string &, es_out_t *, AbstractSourceStream *);
            virtual ~Demuxer();
            virtual Status demux(vlc_tick_t); /* impl */
            virtual void drain(); /* impl */
            virtual bool create(); /* impl */
            virtual void destroy(); /* impl */

        protected:
            AbstractSourceStream *sourcestream;
            vlc_object_t *p_obj;
            demux_t *p_demux;
            std::string name;
            es_out_t *p_es_out;
            bool b_eof;
    };

    class SlaveDemuxer : public Demuxer
    {
        public:
            SlaveDemuxer(vlc_object_t *, const std::string &, es_out_t *, AbstractSourceStream *);
            virtual ~SlaveDemuxer();
            virtual bool create(); /* reimpl */
            virtual Status demux(vlc_tick_t); /* reimpl */

        private:
            vlc_tick_t length;
    };

    class DemuxerFactoryInterface
    {
        public:
            virtual AbstractDemuxer * newDemux(vlc_object_t *, const StreamFormat &,
                                               es_out_t *, AbstractSourceStream *) const = 0;
            virtual ~DemuxerFactoryInterface() = default;
    };
}

#endif // DEMUXER_HPP
