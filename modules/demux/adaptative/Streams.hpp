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
#include <vlc_es.h>
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
    class AbstractStreamOutputFactory;

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
        void create(demux_t *, AbstractAdaptationLogic *,
                    SegmentTracker *, AbstractStreamOutputFactory &);
        bool isEOF() const;
        mtime_t getPCR() const;
        mtime_t getFirstDTS() const;
        int getGroup() const;
        int esCount() const;
        bool seekAble() const;
        typedef enum {status_eof, status_buffering, status_demuxed} status;
        status demux(HTTPConnectionManager *, mtime_t, bool);
        bool setPosition(mtime_t, bool);
        mtime_t getPosition() const;
        void prune();

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

        virtual void pushBlock(block_t *) = 0;
        virtual mtime_t getPCR() const;
        virtual mtime_t getFirstDTS() const = 0;
        virtual int getGroup() const;
        virtual int esCount() const = 0;
        virtual bool seekAble() const = 0;
        virtual void setPosition(mtime_t) = 0;
        virtual void sendToDecoder(mtime_t) = 0;
        virtual bool reinitsOnSeek() const = 0;
        virtual bool switchAllowed() const = 0;

    protected:
        demux_t  *realdemux;
        mtime_t   pcr;
        int       group;
    };

    class AbstractStreamOutputFactory
    {
        public:
            virtual AbstractStreamOutput *create(demux_t*, int streamType) const = 0;
    };

    class DefaultStreamOutputFactory : public AbstractStreamOutputFactory
    {
        public:
            virtual AbstractStreamOutput *create(demux_t*, int streamType) const;
    };

    class BaseStreamOutput : public AbstractStreamOutput
    {
    public:
        BaseStreamOutput(demux_t *, const std::string &);
        virtual ~BaseStreamOutput();
        virtual void pushBlock(block_t *); /* reimpl */
        virtual mtime_t getFirstDTS() const; /* reimpl */
        virtual int esCount() const; /* reimpl */
        virtual bool seekAble() const; /* reimpl */
        virtual void setPosition(mtime_t); /* reimpl */
        virtual void sendToDecoder(mtime_t); /* reimpl */
        virtual bool reinitsOnSeek() const; /* reimpl */
        virtual bool switchAllowed() const; /* reimpl */

    protected:
        es_out_t *fakeesout; /* to intercept/proxy what is sent from demuxstream */
        stream_t *demuxstream;
        bool      seekable;
        std::string name;
        bool      restarting;

    private:
        static es_out_id_t *esOutAdd(es_out_t *, const es_format_t *);
        static int esOutSend(es_out_t *, es_out_id_t *, block_t *);
        static void esOutDel(es_out_t *, es_out_id_t *);
        static int esOutControl(es_out_t *, int, va_list);
        static void esOutDestroy(es_out_t *);

        class Demuxed
        {
            friend class BaseStreamOutput;
            Demuxed(es_out_id_t *, const es_format_t *);
            ~Demuxed();
            void drop();
            es_out_id_t *es_id;
            block_t  *p_queue;
            block_t **pp_queue_last;
            bool recycle;
            es_format_t fmtcpy;
        };
        std::list<Demuxed *> queues;
        bool b_drop;
        vlc_mutex_t lock;
        void sendToDecoderUnlocked(mtime_t);
        bool restart();
    };

}
#endif // STREAMS_HPP
