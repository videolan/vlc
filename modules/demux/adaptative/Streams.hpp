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

    class AbstractStreamOutput
    {
    public:
        AbstractStreamOutput(demux_t *, const StreamFormat &);
        virtual ~AbstractStreamOutput();

        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        const StreamFormat & getStreamFormat() const;
        virtual void pushBlock(block_t *, bool) = 0;
        virtual mtime_t getPCR() const;
        virtual mtime_t getFirstDTS() const = 0;
        virtual int esCount() const = 0;
        virtual bool seekAble() const = 0;
        virtual void setPosition(mtime_t) = 0;
        virtual void sendToDecoder(mtime_t) = 0;
        virtual bool isEmpty() const = 0;
        virtual bool reinitsOnSeek() const = 0;
        virtual bool switchAllowed() const = 0;
        virtual bool isSelected() const = 0;

    protected:
        demux_t  *realdemux;
        mtime_t   pcr;
        std::string language;
        std::string description;

    private:
        StreamFormat format;
    };

    class AbstractStreamOutputFactory
    {
        public:
            virtual ~AbstractStreamOutputFactory() {}
            virtual AbstractStreamOutput *create(demux_t*, const StreamFormat &) const = 0;
    };

    class BaseStreamOutput : public AbstractStreamOutput
    {
    public:
        BaseStreamOutput(demux_t *, const StreamFormat &, const std::string &);
        virtual ~BaseStreamOutput();
        virtual void pushBlock(block_t *, bool); /* reimpl */
        virtual mtime_t getFirstDTS() const; /* reimpl */
        virtual int esCount() const; /* reimpl */
        virtual bool seekAble() const; /* reimpl */
        virtual void setPosition(mtime_t); /* reimpl */
        virtual void sendToDecoder(mtime_t); /* reimpl */
        virtual bool isEmpty() const; /* reimpl */
        virtual bool reinitsOnSeek() const; /* reimpl */
        virtual bool switchAllowed() const; /* reimpl */
        virtual bool isSelected() const; /* reimpl */
        void setTimestampOffset(mtime_t);

    protected:
        es_out_t *fakeesout; /* to intercept/proxy what is sent from demuxstream */
        stream_t *demuxstream;
        bool      seekable;
        std::string name;
        bool      restarting;
        mtime_t   timestamps_offset;

        virtual es_out_id_t *esOutAdd(const es_format_t *);
        virtual int esOutSend(es_out_id_t *, block_t *);
        virtual void esOutDel(es_out_id_t *);
        virtual int esOutControl(int, va_list);
        virtual void esOutDestroy();

    private:
        /* static callbacks for demuxer */
        static es_out_id_t *esOutAdd_Callback(es_out_t *, const es_format_t *);
        static int esOutSend_Callback(es_out_t *, es_out_id_t *, block_t *);
        static void esOutDel_Callback(es_out_t *, es_out_id_t *);
        static int esOutControl_Callback(es_out_t *, int, va_list);
        static void esOutDestroy_Callback(es_out_t *);

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
