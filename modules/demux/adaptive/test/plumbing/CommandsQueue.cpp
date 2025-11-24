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

#include <limits>
#include <list>
#include <algorithm>

using namespace adaptive;

using OutputVal = std::pair<const AbstractFakeESOutID *, block_t *>;

class TestEsOut : public AbstractFakeEsOut
{
    public:
        TestEsOut() {}
        virtual ~TestEsOut() { cleanup(); }
        void milestoneReached() override {}
        void recycle(AbstractFakeESOutID *) override {}
        void createOrRecycleRealEsID(AbstractFakeESOutID *) override {}
        void setPriority(int) override {}
        void sendData(AbstractFakeESOutID *id, block_t *b) override
        {
            output.push_back(OutputVal(id, b));
        }
        void sendMeta(int, const vlc_meta_t *) override {}

        std::list<OutputVal> output;
        void cleanup()
        {
            while(!output.empty())
            {
                block_Release(output.front().second);
                output.pop_front();
            }
        }

    private:
        es_out_id_t *esOutAdd(const es_format_t *) override { return nullptr; }
        int esOutSend(es_out_id_t *, block_t *) override { return VLC_SUCCESS; }
        void esOutDel(es_out_id_t *) override {}
        int esOutControl(int, va_list) override { return VLC_SUCCESS; }
        void esOutDestroy() override {}
};

class TestEsOutID : public AbstractFakeESOutID
{
    public:
        TestEsOutID(TestEsOut *out) { this->out = out; }
        virtual ~TestEsOutID() {}
        es_out_id_t * realESID() const override { return nullptr; }
        void create() override {}
        void release() override {}
        void sendData(block_t *b) override
        {
            out->sendData(this, b);
        }
        EsType esType() const override { return EsType::Other; }

    private:
        TestEsOut *out;
};

#define DT(t) Times(SegmentTimes(), (t))

int CommandsQueue_test()
{
    TestEsOut esout;
    TestEsOutID *id0 = nullptr;
    TestEsOutID *id1 = nullptr;
    try
    {
        CommandsFactory factory;
        CommandsQueue queue;

        id0 = new TestEsOutID(&esout);
        AbstractCommand *cmd = nullptr;

        Expect(queue.isEOF() == false);
        Expect(queue.isDraining() == false);
        Expect(queue.isEmpty() == true);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == 0);
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_INVALID);
        Expect(queue.getFirstTimes().continuous == VLC_TICK_INVALID);
        Expect(queue.getPCR().continuous == VLC_TICK_INVALID);
        cmd = factory.createEsOutAddCommand(id0);
        queue.Schedule(cmd);
        cmd = factory.createEsOutDelCommand(id0);
        queue.Schedule(cmd);
        for(size_t i=0; i<3; i++) /* Add / Del will return in between */
            queue.Process(Times(SegmentTimes(), std::numeric_limits<vlc_tick_t>::max()));
        Expect(queue.isEmpty() == false); /* no PCR issued nor commit */
        queue.Commit();
        for(size_t i=0; i<3; i++)
            queue.Process(Times(SegmentTimes(), std::numeric_limits<vlc_tick_t>::max()));
        Expect(queue.isEOF() == false);
        Expect(queue.isDraining() == false);
        Expect(queue.isEmpty() == true);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == 0);
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_INVALID);
        Expect(queue.getPCR().continuous == std::numeric_limits<vlc_tick_t>::max());

        queue.Abort(true);
        esout.cleanup();

        /* Feed data in */
        for(size_t i=0; i<10; i++)
        {
            block_t *data = block_Alloc(0);
            Expect(data);
            data->i_dts = VLC_TICK_0 + vlc_tick_from_sec(i);
            cmd = factory.createEsOutSendCommand(id0, SegmentTimes(), data);
            queue.Schedule(cmd);
        }
        Expect(queue.getPCR().continuous == VLC_TICK_INVALID);
        Expect(queue.getFirstTimes().continuous == VLC_TICK_INVALID);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == 0);
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_INVALID);
        /* commit some */
        cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(), VLC_TICK_0 + vlc_tick_from_sec(8));
        queue.Schedule(cmd);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == vlc_tick_from_sec(8)); /* PCR committed data up to 8s */
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_0 + vlc_tick_from_sec(8));
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0 + vlc_tick_from_sec(8))).continuous == 0);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0 + vlc_tick_from_sec(7))).continuous == vlc_tick_from_sec(1));
        Expect(queue.getPCR().continuous == VLC_TICK_INVALID);
        /* extend through PCR */
        cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(), VLC_TICK_0 + vlc_tick_from_sec(10));
        queue.Schedule(cmd);
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_0 + vlc_tick_from_sec(10));
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == vlc_tick_from_sec(10));

        /* dequeue */
        queue.Process(Times(SegmentTimes(), VLC_TICK_0 + vlc_tick_from_sec(3)));
        Expect(queue.getPCR().continuous == VLC_TICK_0 + vlc_tick_from_sec(3));
        Expect(queue.getFirstTimes().continuous == VLC_TICK_0 + vlc_tick_from_sec(3));
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0 + vlc_tick_from_sec(3))).continuous == vlc_tick_from_sec(7));
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0 + vlc_tick_from_sec(4))).continuous == vlc_tick_from_sec(6));

        /* drop */
        queue.setDrop(true);
        do
        {
            block_t *data = block_Alloc(0);
            Expect(data);
            data->i_dts = VLC_TICK_0 + vlc_tick_from_sec(11);
            cmd = factory.createEsOutSendCommand(id0, SegmentTimes(), data);
            queue.Schedule(cmd);
        } while(0);
        cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(), VLC_TICK_0 + vlc_tick_from_sec(11));
        queue.Schedule(cmd);
        Expect(queue.getPCR().continuous == VLC_TICK_0 + vlc_tick_from_sec(3)); /* should be unchanged */
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0 + vlc_tick_from_sec(3))).continuous == vlc_tick_from_sec(7));
        queue.setDrop(false);

        /* empty */
        Expect(queue.getPCR().continuous == VLC_TICK_0 + vlc_tick_from_sec(3));
        queue.Process(DT(VLC_TICK_0 + vlc_tick_from_sec(13)));
        Expect(queue.isEmpty());
        Expect(queue.getPCR().continuous == VLC_TICK_0 + vlc_tick_from_sec(9));

        queue.Abort(true);
        esout.cleanup();

        /* reordering */
        id1 = new TestEsOutID(&esout);
        const vlc_tick_t OFFSET = vlc_tick_from_sec(100);
        for(size_t j=0; j<2; j++)
        {
            TestEsOutID *id = (j % 2) ? id1 : id0;
            for(size_t i=0; i<5; i++)
            {
                block_t *data = block_Alloc(0);
                Expect(data);
                data->i_dts = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(i);
                cmd = factory.createEsOutSendCommand(id, SegmentTimes(), data);
                queue.Schedule(cmd);
            }
        }

        cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(),
                                                   VLC_TICK_0 + OFFSET + vlc_tick_from_sec(10));
        queue.Schedule(cmd);
        Expect(esout.output.empty());
        queue.Process(DT(VLC_TICK_0 + OFFSET - 1));
        Expect(esout.output.empty());
        queue.Process(DT(VLC_TICK_0 + OFFSET + vlc_tick_from_sec(10)));
        Expect(esout.output.size() == 10);
        for(size_t i=0; i<5; i++)
        {
            const vlc_tick_t now = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(i);
            OutputVal val = esout.output.front();
            Expect(val.first == id0);
            Expect(val.second->i_dts == now);
            block_Release(val.second);
            esout.output.pop_front();
            val = esout.output.front();
            Expect(val.first == id1);
            Expect(val.second->i_dts == now);
            block_Release(val.second);
            esout.output.pop_front();
        }
        Expect(esout.output.empty());
        queue.Abort(true);

        /* reordering with PCR */
        for(size_t k=0; k<3; k++)
        {
            for(size_t j=0; j<2; j++)
            {
                TestEsOutID *id = (j % 2) ? id1 : id0;
                for(size_t i=0; i<2; i++)
                {
                    block_t *data = block_Alloc(0);
                    Expect(data);
                    data->i_dts = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(k * 2 + i);
                    cmd = factory.createEsOutSendCommand(id, SegmentTimes(), data);
                    queue.Schedule(cmd);
                }
            }
            cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(),
                    VLC_TICK_0 + OFFSET + vlc_tick_from_sec( (k*2)+1 ));
            queue.Schedule(cmd);
        }
        queue.Process(Times(SegmentTimes(), std::numeric_limits<vlc_tick_t>::max()));
        Expect(esout.output.size() == 12);
        for(size_t i=0; i<12; i++)
        {
            TestEsOutID *id = (i % 2) ? id1 : id0;
            const vlc_tick_t now = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(i / 2);
            OutputVal &val = esout.output.front();
            Expect(val.first == id);
            Expect(val.second->i_dts == now);
            block_Release(val.second);
            esout.output.pop_front();
        }
        Expect(esout.output.empty());
        queue.Abort(true);

        /* reordering with PCR & INVALID */
        for(size_t k=0; k<3; k++)
        {
            for(size_t j=0; j<2; j++)
            {
                TestEsOutID *id = (j % 2) ? id1 : id0;
                for(size_t i=0; i<2; i++)
                {
                    block_t *data = block_Alloc(0);
                    Expect(data);
                    if(i==0)
                        data->i_dts = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(k);
                    cmd = factory.createEsOutSendCommand(id, SegmentTimes(), data);
                    queue.Schedule(cmd);
                }
            }
            cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(),
                    VLC_TICK_0 + OFFSET + vlc_tick_from_sec(k));
            queue.Schedule(cmd);
        }
        queue.Process(Times(SegmentTimes(), std::numeric_limits<vlc_tick_t>::max()));
        Expect(esout.output.size() == 12);
        for(size_t i=0; i<6; i++)
        {
            TestEsOutID *id = (i % 2) ? id1 : id0;
            const vlc_tick_t now = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(i/2);
            OutputVal val = esout.output.front();
            Expect(val.first == id);
            Expect(val.second->i_dts == now);
            block_Release(val.second);
            esout.output.pop_front();
            val = esout.output.front();
            Expect(val.first == id);
            Expect(val.second->i_dts == VLC_TICK_INVALID);
            block_Release(val.second);
            esout.output.pop_front();
        }
        Expect(esout.output.empty());
        queue.Abort(true);

        /* reordering PCR before PTS */
        for(size_t i=0; i<2; i++)
        {
            const vlc_tick_t now = VLC_TICK_0 + OFFSET + vlc_tick_from_sec(i);
            cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(), now);
            queue.Schedule(cmd);
            block_t *data = block_Alloc(0);
            Expect(data);
            data->i_dts = now;
            cmd = factory.createEsOutSendCommand(id0, SegmentTimes(), data);
            queue.Schedule(cmd);
        }
        queue.Process(DT(VLC_TICK_0 + OFFSET + vlc_tick_from_sec(0)));
        Expect(esout.output.size() == 1);
        queue.Abort(true);
        esout.cleanup();

        /* PCR handling when set to INVALID */
        for(size_t i=0; i<3; i++)
        {
            block_t *data = block_Alloc(0);
            Expect(data);
            data->i_dts = VLC_TICK_0 + vlc_tick_from_sec(i);
            cmd = factory.createEsOutSendCommand(id0, SegmentTimes(), data);
            queue.Schedule(cmd);
        }
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == 0);
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_INVALID);
        cmd = factory.createEsOutControlPCRCommand(0, SegmentTimes(), VLC_TICK_INVALID);
        queue.Schedule(cmd);
        Expect(queue.getDemuxedAmount(DT(VLC_TICK_0)).continuous == vlc_tick_from_sec(2));
        Expect(queue.getBufferingLevel().continuous == VLC_TICK_0 + vlc_tick_from_sec(2));
        queue.Process(DT(VLC_TICK_0 + vlc_tick_from_sec(5)));
        Expect(esout.output.size() == 3);


    } catch(...) {
        delete id0;
        delete id1;
        return 1;
    }

    delete id0;
    delete id1;

    return 0;
}
