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

using namespace adaptive;

using OutputVal = std::pair<const AbstractFakeESOutID *, block_t *>;

#define DT(t) Times(SegmentTimes(), (t))

struct context
{
    vlc_tick_t dts;
    vlc_tick_t pts;
    vlc_tick_t pcr;
};

struct dropesout
{
    struct context *ctx;
    es_out_t esout;
};

static es_out_id_t *dummy_callback_add(es_out_t *, const es_format_t *)
{
    return (es_out_id_t *) 0x01;
}

static int dummy_callback_send(es_out_t *out, es_out_id_t *, block_t *b)
{
    struct context *ctx = container_of(out, dropesout, esout)->ctx;
    ctx->dts = b->i_dts;
    ctx->pts = b->i_pts;
    block_Release(b);
    return VLC_SUCCESS;
}

static void dummy_callback_del(es_out_t *, es_out_id_t *)
{

}

static int dummy_callback_control(es_out_t *out, int i_query, va_list args)
{
    struct context *ctx = container_of(out, dropesout, esout)->ctx;

    switch( i_query )
    {
        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            if( i_query == ES_OUT_SET_GROUP_PCR )
                (void) va_arg( args, int );
            ctx->pcr = va_arg( args, vlc_tick_t );
            break;
        }
        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void dummy_callback_destroy(es_out_t *)
{

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

#define DMS(t) ((t)*INT64_C(1000))
#define TMS(t) (VLC_TICK_0 + DMS(t))
#define SEND(t) enqueue(out, id, t, t)
#define PCR(t) es_out_SetPCR(out, t)
#define FROM_MPEGTS(x) (INT64_C(x) * 100 / 9)

static int check2(es_out_t *out, struct context *, FakeESOut *fakees)
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
        fprintf(stderr,"first.continuous %ld", first.continuous);
        Expect(first.continuous == TMS(100000));
        fprintf(stderr,"first.segment.demux %ld", first.segment.demux);
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
        fprintf(stderr,"first %ld\n", first.continuous);
        Expect(first.continuous == TMS(60000));

        fakees->resetTimestamps();
        fakees->commandsQueue()->Abort(true);

        /* setAssociatedTimestamp check explicit MPEGTS timestamp mapping (WebVTT) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setAssociatedTimestamp(TMS(60000), TMS(500000));

        SEND(TMS(500000));
        PCR(TMS(500000));
        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first %ld\n", first.continuous);
        Expect(first.continuous == TMS(60000));

        /* setAssociatedTimestamp check explicit rolled MPEGTS timestamp mapping (WebVTT) */
        fakees->setSegmentStartTimes(segmentTimes);
        fakees->setAssociatedTimestamp(TMS(60000), TMS(500000) + FROM_MPEGTS(0x1FFFFFFFF));

        SEND(TMS(500000));
        PCR(TMS(500000));
        first = fakees->commandsQueue()->getFirstTimes();
        fprintf(stderr,"first %ld\n", first.continuous);
        Expect(first.continuous == TMS(60000));

    } catch (...) {
        return 1;
    }

    return 0;
}

static int check1(es_out_t *out, struct context *ctx, FakeESOut *fakees)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
    es_out_id_t *id = es_out_Add(out, &fmt);

    /* ensure ES is created */
    const Times drainTimes(SegmentTimes(),std::numeric_limits<mtime_t>::max());
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
        fprintf(stderr, "timestamp %ld\n", ts);
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
        fprintf(stderr, "timestamp %ld\n", ts);
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
        fprintf(stderr, "timestamp %ld\n", ts);
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
        fprintf(stderr, "timestamp %ld\n", ts);
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

static int check0(es_out_t *out, struct context *, FakeESOut *fakees)
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
    struct context ctx = {VLC_TICK_INVALID,VLC_TICK_INVALID,VLC_TICK_INVALID};
    struct dropesout dummy = {
            .ctx = &ctx,
            .esout = {
                .pf_add = dummy_callback_add,
                .pf_send = dummy_callback_send,
                .pf_del = dummy_callback_del,
                .pf_control = dummy_callback_control,
                .pf_destroy = dummy_callback_destroy,
                .p_sys = nullptr,
    } };

    int(* const tests[3])(es_out_t *, struct context *, FakeESOut *)
            = { check0, check1, check2 };
    for(size_t i=0; i<3; i++)
    {
        CommandsFactory *factory = new CommandsFactory();
        CommandsQueue *queue = new CommandsQueue();
        FakeESOut *fakees = new FakeESOut(&dummy.esout, queue, factory);
        es_out_t *out = *fakees;
        int ret = tests[i](out, &ctx, fakees);
        delete fakees;
        if (ret)
            return ret;
    }

    return 0;
}
