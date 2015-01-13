/*
 * Streams.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include <vlc_common.h>
#include "StreamsType.hpp"
#include "adaptationlogic/AbstractAdaptationLogic.h"
#include "http/HTTPConnectionManager.h"
#include "http/Chunk.h"

namespace dash
{
    class SegmentTracker;

    namespace Streams
    {
        class AbstractStreamOutput;

        class Stream
        {
            public:
                Stream(const std::string &mime);
                Stream(const Type, const Format);
                ~Stream();
                bool operator==(const Stream &) const;
                static Type mimeToType(const std::string &mime);
                static Format mimeToFormat(const std::string &mime);
                void create(demux_t *, logic::AbstractAdaptationLogic *, SegmentTracker *);
                bool isEOF() const;
                mtime_t getPCR() const;
                int getGroup() const;
                int esCount() const;
                bool seekAble() const;
                size_t read(http::HTTPConnectionManager *);
                bool setPosition(mtime_t, bool);
                mtime_t getPosition() const;

            private:
                http::Chunk *getChunk();
                void init(const Type, const Format);
                Type type;
                Format format;
                AbstractStreamOutput *output;
                logic::AbstractAdaptationLogic *adaptationLogic;
                SegmentTracker *segmentTracker;
                http::Chunk *currentChunk;
                bool eof;
        };

        class AbstractStreamOutput
        {
            public:
                AbstractStreamOutput(demux_t *);
                virtual ~AbstractStreamOutput();

                virtual void pushBlock(block_t *);
                mtime_t getPCR() const;
                int getGroup() const;
                int esCount() const;
                bool seekAble() const;
                void setPosition(mtime_t);

            protected:
                mtime_t   pcr;
                int       group;
                int       escount;
                es_out_t *fakeesout; /* to intercept/proxy what is sent from demuxstream */
                stream_t *demuxstream;
                bool      seekable;

            private:
                demux_t  *realdemux;
                static es_out_id_t *esOutAdd(es_out_t *, const es_format_t *);
                static int esOutSend(es_out_t *, es_out_id_t *, block_t *);
                static void esOutDel(es_out_t *, es_out_id_t *);
                static int esOutControl(es_out_t *, int, va_list);
                static void esOutDestroy(es_out_t *);
        };

        class MP4StreamOutput : public AbstractStreamOutput
        {
            public:
                MP4StreamOutput(demux_t *);
                virtual ~MP4StreamOutput(){}
        };

        class MPEG2TSStreamOutput : public AbstractStreamOutput
        {
            public:
                MPEG2TSStreamOutput(demux_t *);
                virtual ~MPEG2TSStreamOutput(){}
        };
    }
}
#endif // STREAMS_HPP
