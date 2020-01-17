/*****************************************************************************
 * ts_pes.c: MPEG PES assembly tests
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_block.h>

#include "../../../modules/demux/mpeg/ts_streams.h"
#include "../../../modules/demux/mpeg/ts_pid_fwd.h"
#include "../../../modules/demux/mpeg/ts_streams_private.h"
#include "../../../modules/demux/mpeg/ts_pes.h"

#include "../../libvlc/test.h"

static void Parse(vlc_object_t *obj, void *priv, block_t *data)
{
    VLC_UNUSED(obj);
    block_t **pp_append = (block_t **) priv;
    fprintf(stderr, "recv: ");
    data = block_ChainGather(data);
    for(size_t i=0; i<data->i_buffer; i++)
        fprintf(stderr, "%2.2x ", data->p_buffer[i]);
    fprintf(stderr, "\n");
    block_ChainAppend(pp_append, data);
}

#define RESET do {\
    block_ChainRelease(output);\
    output = NULL;\
    block_ChainRelease(pes.gather.p_data);\
    memset(&pes, 0, sizeof(pes));\
    pes.transport = TS_TRANSPORT_PES;\
    pes.gather.pp_last = &pes.gather.p_data;\
    } while(0)

#define ASSERT(a) do {\
    if(!(a)) { RESET; \
        fprintf(stderr, "failed line %d\n", __LINE__); \
        return 1; } \
    } while(0)

#define PKT_FROMSZ(a, b) do {\
    pkt = block_Alloc(sizeof(a) + b);\
    ASSERT(pkt);\
    memcpy(pkt->p_buffer, a, sizeof(a));\
    for(size_t i=1; i<1+b;i++)\
        pkt->p_buffer[sizeof(a) + i] = i % 0xFF;\
} while(0)

#define PKT_FROM(a) PKT_FROMSZ(a, 0)

int main()
{
    block_t *pkt;
    block_t *output = NULL;
    int outputcount = 0;
    size_t outputsize = 0;

    test_init();

    ts_pes_parse_callback cb =
    {
        .p_obj = NULL,
        .priv = &output,
        .pf_parse = Parse
    };

    ts_stream_t pes;
    memset(&pes, 0, sizeof(pes));
    pes.transport = TS_TRANSPORT_PES;
    pes.gather.pp_last = &pes.gather.p_data;

    /* General case, aligned payloads */
    /* payload == 0 */
    const uint8_t aligned0[] = {
        0x00, 0x00, 0x01, 0xe0, 0x00, 0x03, 0x80, 0x00, 0x00,
    };
    PKT_FROM(aligned0);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1);
    ASSERT(outputsize == 6+3);
    ASSERT(!memcmp(aligned0, output->p_buffer, outputsize));
    RESET;
    /* no output if not unit start */
    PKT_FROM(aligned0);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, false, true));
    ASSERT(!output);
    RESET;
    /* no output if not unit start */
    PKT_FROM(aligned0);
    pkt->i_buffer = 1;
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, false, true));
    ASSERT(!output);
    RESET;

    /* payload == 6 */
    const uint8_t aligned1[] = {
        0x00, 0x00, 0x01, 0xe0, 0x00, 0x09, 0x80, 0x00, 0x00,
        0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB,
    };
    PKT_FROM(aligned1);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1);
    ASSERT(outputsize == 6+3+6);
    ASSERT(!memcmp(aligned1, output->p_buffer, outputsize));
    RESET;
    /* no output if not unit start */
    PKT_FROM(aligned1);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, false, true));
    ASSERT(!output);
    RESET;

    /* payload == 30, uncomplete */
    PKT_FROM(aligned1);
    SetWBE(&pkt->p_buffer[4], 30);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    RESET;

    /* packets assembly, payload > 188 - 6 - 4 */
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 250);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    ASSERT(pes.gather.i_data_size == 256);
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, false, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1);
    ASSERT(outputsize == 256);
    RESET;

    /* no packets assembly from unit start */
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 250);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    ASSERT(pes.gather.i_data_size == 256);
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 2);
    RESET;

    /* packets assembly, payload undef, use next sync code from another payload undef */
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 0);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    ASSERT(pes.gather.i_data_size == 0);
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 0);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1);
    ASSERT(outputsize == 188);
    RESET;

    /* packets assembly, payload undef, use next sync code from fixed size */
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 0);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    ASSERT(pes.gather.i_data_size == 0);
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 2); /* secondary */
    RESET;

    /* packets assembly, payload undef, use next sync code from fixed size but uncomplete */
    PKT_FROMSZ(aligned1, 188-sizeof(aligned1));
    SetWBE(&pkt->p_buffer[4], 0);
    ASSERT(!ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(!output);
    ASSERT(pes.gather.i_data_size == 0);
    PKT_FROM(aligned1);
    pkt->i_buffer = 6;
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1); /* can't output */
    PKT_FROM(aligned1);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, false, true)); /* add data for last output */
    ASSERT(output); /* output */
    RESET;

    const uint8_t aligned2[] = {
        0x00, 0x00, 0x01, 0xe0, 0x00, 0x03, 0x80, 0x00, 0x00,

        0x00, 0x00, 0x01, 0xe0, 0x00, 0x07, 0x80, 0x00, 0x00,  /* PES 0xdb header */
        0x00, 0x01, 0x02, 0x03,                                /* PES payload */

        0x00, 0x00, 0x01, 0xe0, 0x00, 0x07, 0x80, 0x00, 0x00,  /* PES 0xdb header */
        0xAA, 0xBB, 0xCC, 0xDD,                                /* PES payload */
    };

    /* If the payload_unit_start_indicator is set to '1', then one and only one
     * PES packet starts in this transport stream packet. */
    PKT_FROM(aligned2);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 1);
    RESET;

    /* Broken PUSI tests */
    pes.b_broken_PUSI_conformance = true;
    PKT_FROM(aligned2);
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 3);
    RESET;

    pes.b_broken_PUSI_conformance = true;
    PKT_FROM(aligned2);
    pkt->p_buffer[0] = 0xFF;
    ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
    ASSERT(output);
    block_ChainProperties(output, &outputcount, &outputsize, NULL);
    ASSERT(outputcount == 2);
    RESET;

    for(int split=12; split>9; split--)
    {
        pes.b_broken_PUSI_conformance = true;
        PKT_FROM(aligned2);
        pkt->i_buffer = split;
        ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
        ASSERT(output);

        PKT_FROM(aligned2);
        pkt->p_buffer += split;
        pkt->i_buffer -= split;
        ASSERT(ts_pes_Gather(&cb, &pes, pkt, true, true));
        ASSERT(output);

        RESET;
    }

    return 0;
}
