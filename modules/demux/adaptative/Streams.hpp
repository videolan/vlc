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

#include <vlc_common.h>
#include "StreamFormat.hpp"
#include "ChunksSource.hpp"

#include "plumbing/Demuxer.hpp"
#include "plumbing/SourceStream.hpp"
#include "plumbing/FakeESOut.hpp"

#include <string>

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

    using namespace http;
    using namespace logic;
    using namespace playlist;

    class AbstractStream : public ChunksSource,
                           public ExtraFMTInfoInterface
    {
    public:
        AbstractStream(demux_t *, const StreamFormat &);
        virtual ~AbstractStream();
        void bind(AbstractAdaptationLogic *, SegmentTracker *,
                  HTTPConnectionManager *);

        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        bool isEOF() const;
        mtime_t getPCR() const;
        mtime_t getBufferingLevel() const;
        mtime_t getFirstDTS() const;
        int esCount() const;
        bool seekAble() const;
        bool isSelected() const;
        bool reactivate(mtime_t);
        bool isDisabled() const;
        typedef enum {status_eof, status_eop, status_dis, status_buffering, status_demuxed} status;
        status demux(mtime_t, bool);
        virtual bool setPosition(mtime_t, bool);
        mtime_t getPosition() const;
        void prune();
        void runUpdates();

        virtual block_t *readNextBlock(size_t); /* impl */

        virtual void fillExtraFMTInfo( es_format_t * ) const; /* impl */

    protected:
        virtual block_t *checkBlock(block_t *, bool) = 0;
        virtual AbstractDemuxer * createDemux(const StreamFormat &) = 0;
        virtual bool startDemux();
        virtual bool restartDemux();

        virtual void prepareFormatChange();

        bool restarting_output;
        bool discontinuity;
        SegmentChunk *getChunk();

        Demuxer *syncdemux;

        demux_t *p_realdemux;
        StreamFormat format;

        AbstractAdaptationLogic *adaptationLogic;
        HTTPConnectionManager *connManager; /* not owned */
        SegmentTracker *segmentTracker;

        SegmentChunk *currentChunk;
        bool disabled;
        bool eof;
        bool dead;
        bool flushing;
        mtime_t pcr;
        std::string language;
        std::string description;

        AbstractDemuxer *demuxer;
        AbstractSourceStream *demuxersource;
        FakeESOut *fakeesout; /* to intercept/proxy what is sent from demuxstream */
    };

    class AbstractStreamFactory
    {
        public:
            virtual ~AbstractStreamFactory() {}
            virtual AbstractStream *create(demux_t*, const StreamFormat &,
                                   AbstractAdaptationLogic *, SegmentTracker *,
                                   HTTPConnectionManager *) const = 0;
    };
}
#endif // STREAMS_HPP
