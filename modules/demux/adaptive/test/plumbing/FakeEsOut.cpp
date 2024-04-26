/*****************************************************************************
 *
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../../plumbing/FakeESOut.hpp"
#include "../../plumbing/FakeESOutID.hpp"
#include "../../plumbing/CommandsQueue.hpp"

#include "../test.hpp"

#include <vlc_block.h>
#include <vlc_es_out.h>

#include <limits>
#include <list>
#include <algorithm>
#include <cassert>

using namespace adaptive;

using OutputVal = std::pair<const AbstractFakeESOutID *, block_t *>;
const Times drainTimes(SegmentTimes(),std::numeric_limits<vlc_tick_t>::max());

#define DT(t) Times(SegmentTimes(), (t))

class DummyEsOut
{
    public:
        DummyEsOut();
        ~DummyEsOut();

        void reset();

        static es_out_id_t *callback_add( es_out_t *, input_source_t *, const es_format_t * );
        static int callback_send( es_out_t *, es_out_id_t *, block_t * );
        static void callback_del( es_out_t *, es_out_id_t * );
        static int callback_control( es_out_t *, input_source_t *, int, va_list );
        static void callback_destroy( es_out_t * );

        vlc_tick_t dts;
        vlc_tick_t pts;
        vlc_tick_t pcr;

        class ES
        {
            public:
                ES(const es_format_t *);
                ~ES();
                es_format_t fmt;
                bool b_selected;
        };

        std::list<ES *> eslist;
};

DummyEsOut::DummyEsOut()
{
    reset();
}

DummyEsOut::~DummyEsOut()
{

}

void DummyEsOut::reset()
{
    dts = VLC_TICK_INVALID;
    pts = VLC_TICK_INVALID;
    pcr = VLC_TICK_INVALID;
    while(!eslist.empty())
    {
        delete eslist.front();
        eslist.pop_front();
    }
}

struct dropesout
{
    DummyEsOut *dummyesout;
    es_out_t esout;
};

es_out_id_t *DummyEsOut::callback_add(es_out_t *out, input_source_t *, const es_format_t *fmt)
{
    DummyEsOut *dummyesout = container_of(out, dropesout, esout)->dummyesout;
    ES *es = new ES(fmt);
    dummyesout->eslist.push_back(es);
    return (es_out_id_t *) es;
}

int DummyEsOut::callback_send(es_out_t *out, es_out_id_t *, block_t *b)
{
    DummyEsOut *dummyesout = container_of(out, dropesout, esout)->dummyesout;
    dummyesout->dts = b->i_dts;
    dummyesout->pts = b->i_pts;
    block_Release(b);
    return VLC_SUCCESS;
}

void DummyEsOut::callback_del(es_out_t *out, es_out_id_t *id)
{
    DummyEsOut *dummyesout = container_of(out, dropesout, esout)->dummyesout;
    ES *es = (ES *) id;
    auto it = std::find(dummyesout->eslist.begin(), dummyesout->eslist.end(), es);
    assert(it != dummyesout->eslist.end());
    if(it != dummyesout->eslist.end())
        dummyesout->eslist.erase(it);
    delete es;
}

int DummyEsOut::callback_control(es_out_t *out, input_source_t *, int i_query, va_list args)
{
    DummyEsOut *dummyesout = container_of(out, dropesout, esout)->dummyesout;

    switch( i_query )
    {
        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            if( i_query == ES_OUT_SET_GROUP_PCR )
                (void) va_arg( args, int );
            dummyesout->pcr = va_arg( args, vlc_tick_t );
            break;
        }
        case ES_OUT_SET_ES:
        {
            ES *es = (ES *) va_arg( args, es_out_id_t * );
            /* emulate reselection */
            for( ES *e : dummyesout->eslist )
            {
                if( e->fmt.i_cat == es->fmt.i_cat )
                    e->b_selected = (e == es);
            }
            return VLC_SUCCESS;
        }
        case ES_OUT_SET_ES_STATE:
        {
            /* emulate selection override */
            ES *es = (ES *) va_arg( args, es_out_id_t * );
            bool b = va_arg( args, int );
            auto it = std::find(dummyesout->eslist.begin(), dummyesout->eslist.end(), es);
            if(it == dummyesout->eslist.end())
                return VLC_EGENERIC;
            (*it)->b_selected = b;
            return VLC_SUCCESS;
        }
        case ES_OUT_GET_ES_STATE:
        {
            ES *es = (ES *) va_arg( args, es_out_id_t * );
            auto it = std::find(dummyesout->eslist.begin(), dummyesout->eslist.end(), es);
            if(it == dummyesout->eslist.end())
                return VLC_EGENERIC;
            *va_arg( args, bool * ) = (*it)->b_selected;
            return VLC_SUCCESS;
        }
        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void DummyEsOut::callback_destroy(es_out_t *)
{

}

static const struct es_out_callbacks dummycbs = []() constexpr
{
    es_out_callbacks cbs = {};
    cbs.add = DummyEsOut::callback_add;
    cbs.send = DummyEsOut::callback_send;
    cbs.del = DummyEsOut::callback_del;
    cbs.control = DummyEsOut::callback_control;
    cbs.destroy = DummyEsOut::callback_destroy;
    return cbs;
}();

DummyEsOut::ES::ES(const es_format_t *src)
{
    b_selected = false;
    es_format_Init(&fmt, src->i_cat, src->i_codec);
    es_format_Copy(&fmt, src);
}

DummyEsOut::ES::~ES()
{
    es_format_Clean(&fmt);
}

static void enqueue(es_out_t *out, es_out_id_t *id, vlc_tick_t dts, vlc_tick_t pts)
{
    block_t *b = block_Alloc(1);
    if(b)
    {
        b->i_dts = dts;
        b->i_pts = pts;
        es_out_Send(out, id, b);
    }
}

#define TMS(t) (VLC_TICK_0 + VLC_TICK_FROM_MS(t))
#define DMS(t) (VLC_TICK_FROM_MS(t))
#define SEND(t) enqueue(out, id, t, t)
#define PCR(t) es_out_SetPCR(out, t)
#define FROM_MPEGTS(x) (INT64_C(x) * 100 / 9)

static int check3(es_out_t *out, DummyEsOut *dummy, FakeESOut *fakees)
{
    es_format_t fmt;

    /* few ES reusability checks */
    try
    {
        es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
        FakeESOutID fakeid0(fakees, &fmt);
        fmt.audio.i_rate = 48000;
        FakeESOutID fakeid1(fakees, &fmt);
        fmt.i_original_fourcc = 0xbeef;
        FakeESOutID fakeid2(fakees, &fmt);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
        FakeESOutID fakeid3(fakees, &fmt);
        fmt.i_codec = VLC_CODEC_MPGV;
        FakeESOutID fakeid4(fakees, &fmt);
        fakees->setSrcID(SrcID::make()); // change source sequence id
        FakeESOutID fakeid0b(fakees, fakeid0.getFmt());
        FakeESOutID fakeid4b(fakees, fakeid4.getFmt());
        Expect(fakeid0.isCompatible(&fakeid0) == true); // aac without rate, same source
        Expect(fakeid0.isCompatible(&fakeid1) == false); // aac rate/unknown mix
        Expect(fakeid1.isCompatible(&fakeid1) == true);  // aac with same rate
        Expect(fakeid0.isCompatible(&fakeid3) == false); // different codecs
        Expect(fakeid1.isCompatible(&fakeid2) == false); // different original fourcc
        Expect(fakeid2.isCompatible(&fakeid2) == true);  // same original fourcc
        Expect(fakeid3.isCompatible(&fakeid3) == false);  // same video with extra codecs
        Expect(fakeid0.isCompatible(&fakeid0b) == false); // aac without rate, different source
        Expect(fakeid4.isCompatible(&fakeid4) == true);  // same codec, same sequence
        Expect(fakeid4.isCompatible(&fakeid4b) == false);  // same codec, different sequence
        es_format_Clean(&fmt);
    } catch (...) {
        return 1;
    }

    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
    es_out_id_t *id = es_out_Add(out, &fmt);
    try
    {
        Expect(id != nullptr);

        /* single ES should be allocated */
        Expect(dummy->eslist.size() == 0);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 1);
        Expect(dummy->eslist.front()->fmt.i_codec == VLC_CODEC_H264);
        dummy->eslist.front()->b_selected = true; /* fake selection */

        /* subsequent ES should be allocated */
        es_format_Clean(&fmt);
        es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 2);
        Expect(dummy->eslist.front()->fmt.i_codec == VLC_CODEC_H264);
        Expect(dummy->eslist.front()->b_selected == true);
        Expect(dummy->eslist.back()->fmt.i_codec == VLC_CODEC_MP4A);

        /* on restart / new segment, unused ES should be reclaimed */
        fakees->recycleAll();
        es_format_Clean(&fmt);
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_MPGV);
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 3);
        fakees->gc();
        Expect(dummy->eslist.size() == 1);
        Expect(dummy->eslist.front()->fmt.i_codec == VLC_CODEC_MPGV);
        Expect(dummy->eslist.front()->b_selected == true);

        /* on restart / new segment, ES MUST be reused */
        fakees->recycleAll();
        Expect(dummy->eslist.size() == 1);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_MPGV);
        /* check ID signaling so we don't blame FakeEsOut */
        {
            FakeESOutID fakeid(fakees, &fmt);
            Expect(fakeid.isCompatible(&fakeid));
        }
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 1);
        fakees->gc();
        Expect(dummy->eslist.size() == 1);
        Expect(dummy->eslist.front()->b_selected == true);

        /* on restart / new segment, different codec, ES MUST NOT be reused */
        fakees->recycleAll();
        Expect(dummy->eslist.size() == 1);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
        id = es_out_Add(out, &fmt);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 3);
        fakees->gc();
        Expect(dummy->eslist.size() == 2);
        for( DummyEsOut::ES *e : dummy->eslist ) /* selection state must have been kept */
            if( e->fmt.i_cat == VIDEO_ES )
                Expect(e->b_selected == true);

        /* on restart / new segment, incompatible codec parameters, ES MUST NOT be reused */
        fakees->setSrcID(SrcID::make());
        fakees->recycleAll();
        Expect(dummy->eslist.size() == 2);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
        id = es_out_Add(out, &fmt);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 4);
        fakees->gc();
        Expect(dummy->eslist.size() == 2);
        for( DummyEsOut::ES *e : dummy->eslist ) /* selection state must have been kept */
            if( e->fmt.i_cat == VIDEO_ES )
                Expect(e->b_selected == true);

        /* on restart / new segment, with compatible codec parameters, ES MUST be reused */
        fakees->recycleAll();
        Expect(dummy->eslist.size() == 2);
        es_format_Clean(&fmt);
        es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
        id = es_out_Add(out, &fmt);
        fakees->commandsQueue()->Commit();
        fakees->commandsQueue()->Process(drainTimes);
        Expect(dummy->eslist.size() == 2); // mp4a reused
        fakees->gc(); // should drop video
        Expect(dummy->eslist.size() == 1);
        Expect(dummy->eslist.front()->fmt.i_codec == fmt.i_codec);
    } catch (...) {
        return 1;
    }

    es_format_Clean(&fmt);

    return 0;
}


static int check2(es_out_t *out, DummyEsOut *, FakeESOut *fakees)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
    es_out_id_t *id = es_out_Add(out, &fmt);
    try
    {
        vlc_tick_t mediaref = TMS(10000);
        SegmentTimes segmentTimes(VLC_TICK_INVALID, mediaref, mediaref);

        /* setExpectedTimestamp check starting from zero every segment (smooth streaming) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setExpectedTimestamp(TMS(60000));

        PCR(TMS(0));
        SEND(TMS(0));
        PCR(TMS(5000));

        Times first = fakees->commandsQueue()->getFirstTimes();
        Expect(first.continuous == TMS(60000));
        Expect(first.segment.demux == TMS(60000));
        Expect(first.segment.media == mediaref);

        fakees->resetTimestamps();
        fakees->commandsQueue()->Abort(true);

        /* setExpectedTimestamp check it does not apply when not starting from zero (smooth streaming) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setExpectedTimestamp(TMS(30000));

        PCR(TMS(100000));
        SEND(TMS(100000));

        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first.continuous %" PRId64 "\n", first.continuous);
        Expect(first.continuous == TMS(100000));
        fprintf(stderr,"first.segment.demux %" PRId64 "\n", first.segment.demux);
        Expect(first.segment.demux == TMS(100000));

        fakees->resetTimestamps();
        fakees->commandsQueue()->Abort(true);

        /* setAssociatedTimestamp check explicit first timestamp mapping (AAC) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setAssociatedTimestamp(TMS(60000));

        PCR(TMS(100000));
        SEND(TMS(100000 + 5000));
        PCR(TMS(100000 + 5000));
        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first %" PRId64 "\n", first.continuous);
        Expect(first.continuous == TMS(60000));

        fakees->resetTimestamps();
        fakees->commandsQueue()->Abort(true);

        /* setAssociatedTimestamp check explicit MPEGTS timestamp mapping (WebVTT) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setAssociatedTimestamp(TMS(60000), TMS(500000));

        SEND(TMS(500000));
        PCR(TMS(500000));
        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first %" PRId64 "\n", first.continuous);
        Expect(first.continuous == TMS(60000));

        /* setAssociatedTimestamp check explicit rolled MPEGTS timestamp mapping (WebVTT) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setAssociatedTimestamp(TMS(60000), TMS(500000) + FROM_MPEGTS(0x1FFFFFFFF));

        SEND(TMS(500000));
        PCR(TMS(500000));
        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first %" PRId64 "\n", first.continuous);
        Expect(first.continuous == TMS(60000));

    } catch (...) {
        return 1;
    }

    return 0;
}

static int check1(es_out_t *out, DummyEsOut *ctx, FakeESOut *fakees)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
    es_out_id_t *id = es_out_Add(out, &fmt);

    /* ensure ES is created */
    const Times drainTimes(SegmentTimes(),std::numeric_limits<vlc_tick_t>::max());

    fakees->commandsQueue()->Commit();
    fakees->commandsQueue()->Process(drainTimes);

    try
    {
        vlc_tick_t mediaref = TMS(10000);
        SegmentTimes segmentTimes(VLC_TICK_INVALID, mediaref, mediaref);
        fakees->setSegmentStartTimes(segmentTimes);

        PCR(TMS(0));
        for(int i=0; i<=5000; i += 1000)
            SEND(TMS(i));
        PCR(TMS(5000));

        Times first = fakees->commandsQueue()->getFirstTimes();
        Expect(first.continuous != VLC_TICK_INVALID);
        Expect(first.continuous == TMS(0));
        Expect(first.segment.media == mediaref);

        Expect(mediaref + DMS(5000) == fakees->commandsQueue()->getBufferingLevel().segment.media);

        /* Reference has local timestamp < rolled ts */
        vlc_tick_t reference = TMS(0);
        fakees->resetTimestamps();
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setSynchronizationReference(SynchronizationReference());
        SEND(reference);
        PCR(reference);
        Expect(fakees->commandsQueue()->getBufferingLevel().continuous == reference);
        vlc_tick_t ts = TMS(1000) + FROM_MPEGTS(0x1FFFFFFFF);
        ts = fakees->applyTimestampContinuity(ts);
        fprintf(stderr, "timestamp %" PRId64 "\n", ts);
        Expect(ts == reference + DMS(1000));

        /* Reference has local multiple rolled timestamp < multiple rolled ts */
        reference = VLC_TICK_0 + FROM_MPEGTS(0x1FFFFFFFF) * 2;
        fakees->resetTimestamps();
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setSynchronizationReference(SynchronizationReference());
        SEND(reference);
        PCR(reference);
        ts = TMS(1000) + FROM_MPEGTS(0x1FFFFFFFF) * 5;
        ts = fakees->applyTimestampContinuity(ts);
        fprintf(stderr, "timestamp %" PRId64 "\n", ts);
        Expect(ts == reference + DMS(1000));


        /* Reference has local timestamp rolled > ts */
        reference = VLC_TICK_0 + FROM_MPEGTS(0x1FFFFFFFF);
        fakees->resetTimestamps();
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setSynchronizationReference(SynchronizationReference());
        SEND(reference);
        PCR(reference);
        Expect(fakees->commandsQueue()->getBufferingLevel().continuous == reference);
        ts = VLC_TICK_0 + 1;
        ts = fakees->applyTimestampContinuity(ts);
        fprintf(stderr, "timestamp %" PRId64 "\n", ts);
        Expect(ts == reference + 1);

        /* Reference has local timestamp mutiple rolled > multiple rolled ts */
        reference = VLC_TICK_0 + FROM_MPEGTS(0x1FFFFFFFF) * 5;
        fakees->resetTimestamps();
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setSynchronizationReference(SynchronizationReference());
        SEND(reference);
        PCR(reference);
        Expect(fakees->commandsQueue()->getBufferingLevel().continuous == reference);
        ts = VLC_TICK_0 + 1 + FROM_MPEGTS(0x1FFFFFFFF) * 2;
        ts = fakees->applyTimestampContinuity(ts);
        fprintf(stderr, "timestamp %" PRId64 "\n", ts);
        Expect(ts == reference + 1);

        /* Do not trigger unwanted roll on long playbacks due to
         * initial reference value */
        reference = VLC_TICK_0 + FROM_MPEGTS(0x00000FFFF);
        fakees->resetTimestamps();
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setSynchronizationReference(SynchronizationReference());
        SEND(reference);
        PCR(reference);
        fakees->commandsQueue()->Process(drainTimes);
        Expect(fakees->commandsQueue()->getBufferingLevel().continuous == reference);
        Expect(fakees->hasSynchronizationReference());
        Expect(ctx->dts == reference);
        ts = reference + FROM_MPEGTS(0x000FFFFFF);
        SEND(ts);
        ts = reference + FROM_MPEGTS(0x00FFFFFFF);
        SEND(ts);
        ts = reference + FROM_MPEGTS(0x0FFFFFFFF);
        SEND(ts);
        ts = reference + FROM_MPEGTS(0x1FF000000); /* elapse enough time from ref */
        SEND(ts);
        PCR(ts);
        fakees->commandsQueue()->Process(drainTimes);
        ts = VLC_TICK_0 + 100; /* next ts has rolled */
        SEND(ts);
        PCR(ts);
        fakees->commandsQueue()->Process(drainTimes);
        Expect(ctx->dts == ts + FROM_MPEGTS(0x1FFFFFFFF));
        SEND(ts + FROM_MPEGTS(0x1FFFFFFFF) * 2); /* next ts has rolled */
        PCR(ts + FROM_MPEGTS(0x1FFFFFFFF) * 2);
        fakees->commandsQueue()->Process(drainTimes);
        Expect(ctx->dts == ts + FROM_MPEGTS(0x1FFFFFFFF));

    } catch (...) {
        return 1;
    }

    return 0;
}

static int check0(es_out_t *out, DummyEsOut *, FakeESOut *fakees)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
    es_out_id_t *id = es_out_Add(out, &fmt);
    try
    {

        vlc_tick_t mediaref = TMS(10000);
        SegmentTimes segmentTimes(VLC_TICK_INVALID, mediaref, mediaref);
        fakees->setSegmentStartTimes(segmentTimes);

        Expect(fakees->commandsQueue()->getBufferingLevel().segment.media == VLC_TICK_INVALID);

        PCR(TMS(0));
        for(int i=0; i<=5000; i += 1000)
            SEND(TMS(i));
        PCR(TMS(5000));

        Times first = fakees->commandsQueue()->getFirstTimes();
        Expect(first.continuous != VLC_TICK_INVALID);
        Expect(first.continuous == TMS(0));
        Expect(first.segment.media == mediaref);
        Expect(mediaref + DMS(5000) == fakees->commandsQueue()->getBufferingLevel().segment.media);
        //    SynchronizationReference r(0, first);

        //    fakees->createDiscontinuityResumePoint(true, 1);

        //    SEND(TMS(0));
        //    PCR(TMS(0));
        //    assert(fakees->commandsQueue()->getBufferingLevel().continuous == TMS(6000));

        //    first = fakees->commandsQueue()->getFirstTimes();
        //    assert(first.continuous != VLC_TICK_INVALID);
        //    assert(first.continuous == TMS(0));
        //    assert(first.segment.media == mediaref);

    } catch (...) {
        return 1;
    }

    return 0;
}

int FakeEsOut_test()
{
    DummyEsOut dummyEsOut;
    struct dropesout dummy = {
            .dummyesout = &dummyEsOut,
            .esout = { .cbs = &dummycbs }
    };

    int(* const tests[4])(es_out_t *, DummyEsOut *, FakeESOut *)
            = { check0, check1, check2, check3 };
    for(size_t i=0; i<4; i++)
    {
        CommandsFactory *factory = new CommandsFactory();
        CommandsQueue *queue = new CommandsQueue();
        FakeESOut *fakees = new FakeESOut(&dummy.esout, queue, factory);
        es_out_t *out = *fakees;
        int ret = tests[i](out, &dummyEsOut, fakees);
        delete fakees;
        if (ret)
            return ret;
        dummyEsOut.reset();
    }

    return 0;
}
