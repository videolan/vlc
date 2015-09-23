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

namespace adaptative
{

    class AbstractDemuxer
    {
        public:
            AbstractDemuxer();
            virtual ~AbstractDemuxer();
            virtual bool restart() = 0;
            virtual bool feed(block_t *, bool) = 0;
            bool alwaysStartsFromZero() const;
            bool reinitsOnSeek() const;

        protected:
            bool b_startsfromzero;
            bool b_reinitsonseek;
    };

    class StreamDemux : public AbstractDemuxer
    {
        public:
            StreamDemux(demux_t *, const std::string &, es_out_t *);
            virtual ~StreamDemux();
            virtual bool restart(); /* impl */
            virtual bool feed(block_t *, bool); /* impl */

        private:
            stream_t *demuxstream;
            demux_t *p_realdemux;
            std::string name;
            es_out_t *p_es_out;
    };

}

#endif // DEMUXER_HPP
