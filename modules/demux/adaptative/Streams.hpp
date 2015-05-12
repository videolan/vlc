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
#include <list>
#include <vlc_common.h>
#include "StreamsType.hpp"

namespace adaptative
{
    class SegmentTracker;

    namespace http
    {
        class HTTPConnectionManager;
        class Chunk;
    }

    namespace logic
    {
        class AbstractAdaptationLogic;
    }


    class AbstractStreamOutput;

    using namespace http;
    using namespace logic;

    class Stream
    {
    public:
        Stream(const std::string &mime);
        Stream(const StreamType, const StreamFormat);
        ~Stream();
        bool operator==(const Stream &) const;
        static StreamType mimeToType(const std::string &mime);
        static StreamFormat mimeToFormat(const std::string &mime);
        void create(demux_t *, AbstractAdaptationLogic *, SegmentTracker *);
        bool isEOF() const;
        mtime_t getPCR() const;
        int getGroup() const;
        int esCount() const;
        bool seekAble() const;
        typedef enum {status_eof, status_buffering, status_demuxed} status;
        status demux(HTTPConnectionManager *, mtime_t);
        bool setPosition(mtime_t, bool);
        mtime_t getPosition() const;

    private:
        Chunk *getChunk();
        void init(const StreamType, const StreamFormat);
        size_t read(HTTPConnectionManager *);
        StreamType type;
        StreamFormat format;
        AbstractStreamOutput *output;
        AbstractAdaptationLogic *adaptationLogic;
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
        void sendToDecoder(mtime_t);

    protected:
        mtime_t   pcr;
        int       group;
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

        class Demuxed
        {
            friend class AbstractStreamOutput;
            Demuxed();
            ~Demuxed();
            void drop();
            es_out_id_t *es_id;
            block_t  *p_queue;
            block_t **pp_queue_last;
        };
        std::list<Demuxed *> queues;
        vlc_mutex_t lock;
        void sendToDecoderUnlocked(mtime_t);
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
#endif // STREAMS_HPP
