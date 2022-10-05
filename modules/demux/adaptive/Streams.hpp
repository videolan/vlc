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

#include <vlc_common.h>
#include "StreamFormat.hpp"
#include "AbstractSource.hpp"
#include "SegmentTracker.hpp"

#include "plumbing/CommandsQueue.hpp"
#include "plumbing/Demuxer.hpp"
#include "plumbing/SourceStream.hpp"
#include "plumbing/FakeESOut.hpp"

#include "Time.hpp"

#include <string>

namespace adaptive
{
    class SegmentTracker;

    namespace http
    {
        class ChunkInterface;
    }

    namespace playlist
    {
        class SegmentChunk;
    }

    using namespace http;
    using namespace playlist;

    class AbstractStream : public AbstractSource,
                           public ExtraFMTInfoInterface,
                           public SegmentTrackerListenerInterface,
                           public DemuxerFactoryInterface
    {
    public:
        AbstractStream(demux_t *);
        virtual ~AbstractStream();
        bool init(const StreamFormat &, SegmentTracker *);

        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        mtime_t getMinAheadTime() const;
        Times getFirstTimes() const;
        int esCount() const;
        bool isSelected() const;
        bool isDisabled() const;
        bool isValid() const;
        void setLivePause(bool);
        enum class Status {
            Eof = 0, /* prioritized */
            Discontinuity,
            Demuxed,
            Buffering,
        };
        enum class BufferingStatus {
            End = 0, /* prioritized */
            Suspended,
            Full,
            Ongoing,
            Lessthanmin,
        };
        BufferingStatus bufferize(Times, mtime_t, mtime_t,
                                  mtime_t, bool = false);
        BufferingStatus getBufferAndStatus(const Times &, mtime_t, mtime_t, mtime_t *);
        mtime_t getDemuxedAmount(Times) const;
        Status dequeue(Times, Times *);
        bool decodersDrained();

        class StreamPosition
        {
            public:
                StreamPosition();
                Times times;
                uint64_t number;
                double pos;
        };
        virtual bool reactivate(const StreamPosition &);
        virtual bool setPosition(const StreamPosition &, bool);
        bool getMediaPlaybackTimes(mtime_t *, mtime_t *, mtime_t *) const;
        bool getMediaAdvanceAmount(mtime_t *) const;
        bool runUpdates();

        /* Used by demuxers fake streams */
        virtual block_t *readNextBlock() override;

        /**/
        virtual void fillExtraFMTInfo( es_format_t * ) const  override;
        virtual void trackerEvent(const TrackerEvent &)  override;

    protected:
        bool seekAble() const;
        void setDisabled(bool);
        virtual block_t *checkBlock(block_t *, bool) = 0;
        AbstractDemuxer * createDemux(const StreamFormat &);
        virtual AbstractDemuxer * newDemux(vlc_object_t *, const StreamFormat &,
                                           es_out_t *, AbstractSourceStream *) const  override;
        virtual bool startDemux();
        virtual bool restartDemux();

        virtual void prepareRestart(bool = true);
        bool resetForNewPosition(mtime_t);

        bool contiguous;
        bool segmentgap;
        bool discontinuity;
        bool needrestart;
        bool inrestart;
        bool demuxfirstchunk;

        bool mightalwaysstartfromzero;

        demux_t *p_realdemux;
        StreamFormat format;

        SegmentTracker *segmentTracker;

        ChunkInterface * getNextChunk() const;
        ChunkInterface *currentChunk;
        bool eof;
        std::string language;
        std::string description;
        struct
        {
            unsigned width;
            unsigned height;
        } currentrep;

        AbstractDemuxer *demuxer;
        AbstractSourceStream *demuxersource;
        FakeESOut::LockedFakeEsOut fakeEsOut();
        FakeESOut::LockedFakeEsOut fakeEsOut() const;
        FakeESOut *fakeesout; /* to intercept/proxy what is sent from demuxstream */
        mutable vlc_mutex_t lock; /* lock for everything accessed by dequeuing */

        SegmentTimes startTimeContext;
        SegmentTimes currentTimeContext;
        SegmentTimes prevEndTimeContext;
        mtime_t currentDuration;
        uint64_t currentSequence;

    private:
        void declaredCodecs();
        BufferingStatus doBufferize(Times, mtime_t, mtime_t,
                                    mtime_t, bool);
        BufferingStatus last_buffer_status;
        bool valid;
        bool disabled;
        unsigned notfound_sequence;
    };

    class AbstractStreamFactory
    {
        public:
            virtual ~AbstractStreamFactory() {}
            virtual AbstractStream *create(demux_t*, const StreamFormat &,
                                           SegmentTracker *) const = 0;
    };
}
#endif // STREAMS_HPP
