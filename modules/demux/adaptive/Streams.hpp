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

#include <string>

namespace adaptive
{
    class SegmentTracker;

    namespace http
    {
        class AbstractConnectionManager;
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
        bool init(const StreamFormat &, SegmentTracker *, AbstractConnectionManager *);

        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        vlc_tick_t getPCR() const;
        vlc_tick_t getMinAheadTime() const;
        vlc_tick_t getFirstDTS() const;
        int esCount() const;
        bool isSelected() const;
        virtual bool reactivate(vlc_tick_t);
        bool isDisabled() const;
        bool isValid() const;
        typedef enum {
            status_eof = 0, /* prioritized */
            status_discontinuity,
            status_demuxed,
            status_buffering,
        } status;
        typedef enum {
            buffering_end = 0, /* prioritized */
            buffering_suspended,
            buffering_full,
            buffering_ongoing,
            buffering_lessthanmin,
        } buffering_status;
        buffering_status bufferize(vlc_tick_t, vlc_tick_t, vlc_tick_t, bool = false);
        buffering_status getLastBufferStatus() const;
        vlc_tick_t getDemuxedAmount() const;
        status dequeue(vlc_tick_t, vlc_tick_t *);
        bool decodersDrained();
        virtual bool setPosition(vlc_tick_t, bool);
        bool getMediaPlaybackTimes(vlc_tick_t *, vlc_tick_t *, vlc_tick_t *,
                                   vlc_tick_t *, vlc_tick_t *) const;
        void runUpdates();

        /* Used by demuxers fake streams */
        virtual std::string getContentType(); /* impl */
        virtual block_t *readNextBlock(); /* impl */

        /**/
        virtual void fillExtraFMTInfo( es_format_t * ) const; /* impl */
        virtual void trackerEvent(const SegmentTrackerEvent &); /* impl */

    protected:
        bool seekAble() const;
        void setDisabled(bool);
        virtual block_t *checkBlock(block_t *, bool) = 0;
        AbstractDemuxer * createDemux(const StreamFormat &);
        virtual AbstractDemuxer * newDemux(vlc_object_t *, const StreamFormat &,
                                           es_out_t *, AbstractSourceStream *) const; /* impl */
        virtual bool startDemux();
        virtual bool restartDemux();

        virtual void prepareRestart(bool = true);

        bool discontinuity;
        bool needrestart;
        bool inrestart;
        bool demuxfirstchunk;

        demux_t *p_realdemux;
        StreamFormat format;

        AbstractConnectionManager *connManager; /* not owned */
        SegmentTracker *segmentTracker;

        SegmentChunk *currentChunk;
        bool eof;
        std::string language;
        std::string description;

        AbstractDemuxer *demuxer;
        AbstractSourceStream *demuxersource;
        FakeESOut::LockedFakeEsOut fakeEsOut();
        FakeESOut::LockedFakeEsOut fakeEsOut() const;
        FakeESOut *fakeesout; /* to intercept/proxy what is sent from demuxstream */
        mutable vlc_mutex_t lock; /* lock for everything accessed by dequeuing */

    private:
        void declaredCodecs();
        buffering_status doBufferize(vlc_tick_t, vlc_tick_t, vlc_tick_t, bool);
        buffering_status last_buffer_status;
        bool valid;
        bool disabled;
        unsigned notfound_sequence;
    };

    class AbstractStreamFactory
    {
        public:
            virtual ~AbstractStreamFactory() {}
            virtual AbstractStream *create(demux_t*, const StreamFormat &,
                                   SegmentTracker *, AbstractConnectionManager *) const = 0;
    };
}
#endif // STREAMS_HPP
