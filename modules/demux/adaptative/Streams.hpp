/*
 * Streams.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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
#ifndef STREAM_HPP
#define STREAM_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string>
#include <list>
#include <vlc_common.h>
#include <vlc_es.h>
#include "StreamsType.hpp"
#include "StreamFormat.hpp"

namespace adaptative
{
    class SegmentTracker;

    namespace http
    {
        class HTTPConnectionManager;
    }

    namespace logic
    {
        class AbstractAdaptationLogic;
    }

    namespace playlist
    {
        class SegmentChunk;
    }


    class AbstractStreamOutput;
    class AbstractStreamOutputFactory;

    using namespace http;
    using namespace logic;
    using namespace playlist;

    class Stream
    {
    public:
        Stream(demux_t *, const StreamFormat &);
        ~Stream();
        bool operator==(const Stream &) const;
        static StreamType mimeToType(const std::string &mime);
        void create(AbstractAdaptationLogic *, SegmentTracker *,
                    const AbstractStreamOutputFactory *);
        void updateFormat(StreamFormat &);
        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        bool isEOF() const;
        mtime_t getPCR() const;
        mtime_t getFirstDTS() const;
        int esCount() const;
        bool seekAble() const;
        bool isSelected() const;
        bool reactivate(mtime_t);
        bool isDisabled() const;
        typedef enum {status_eof, status_eop, status_buffering, status_demuxed} status;
        status demux(HTTPConnectionManager *, mtime_t, bool);
        bool setPosition(mtime_t, bool);
        mtime_t getPosition() const;
        void prune();

    private:
        SegmentChunk *getChunk();
        size_t read(HTTPConnectionManager *);
        demux_t *p_demux;
        StreamType type;
        StreamFormat format;
        AbstractStreamOutput *output;
        AbstractAdaptationLogic *adaptationLogic;
        SegmentTracker *segmentTracker;
        SegmentChunk *currentChunk;
        bool disabled;
        bool eof;
        std::string language;
        std::string description;

        const AbstractStreamOutputFactory *streamOutputFactory;
    };

}
#endif // STREAMS_HPP
