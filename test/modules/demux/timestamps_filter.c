/*****************************************************************************
 * timestamps_filter.c:
 *****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_block.h>
#include "../../../modules/demux/moving_avg.h"
#define DEBUG_TIMESTAMPS_FILTER
#include "../../../modules/demux/timestamps_filter.h"

#define TRASH_ID 0xfeca1
#define ESID(n) ((void *)(INT64_C(0)+TRASH_ID+n))

static int trash_es_out_Control(es_out_t *out, input_source_t *in, int i_query, va_list va_list)
{
    VLC_UNUSED(i_query);
    VLC_UNUSED(out);
    VLC_UNUSED(in);
    VLC_UNUSED(va_list);
    return VLC_EGENERIC;
}

static int trash_es_out_Send(es_out_t *out, es_out_id_t *id, block_t *p_block)
{
    VLC_UNUSED(out);
    VLC_UNUSED(id);
    VLC_UNUSED(p_block); /* We will reuse it ! */
    return VLC_SUCCESS;
}

static void trash_es_out_Delete(es_out_t *out)
{
    VLC_UNUSED(out);
}

static es_out_id_t *trash_es_out_Add(es_out_t *out, input_source_t *in, const es_format_t *fmt)
{
    VLC_UNUSED(out);
    VLC_UNUSED(in);
    VLC_UNUSED(fmt);
    return ESID(fmt->i_id);
}

static void trash_es_out_Del(es_out_t *out, es_out_id_t *id)
{
    VLC_UNUSED(out);
    VLC_UNUSED(id);
}

static const struct es_out_callbacks trash_es_out_cbs =
{
    trash_es_out_Add,
    trash_es_out_Send,
    trash_es_out_Del,
    trash_es_out_Control,
    trash_es_out_Delete,
    NULL,
};

#define TIMELINE_0 (CLOCK_FREQ)
#define TIMELINE_1 (CLOCK_FREQ * 10)
#define TIMELINE_2 (CLOCK_FREQ * 5)

int main(void)
{
    es_out_t trash_es_out = { .cbs = &trash_es_out_cbs };
    es_out_t *out = timestamps_filter_es_out_New(&trash_es_out);
    if(!out)
        return 1;
    block_t *p_block = block_Alloc(0);
    if(!p_block)
    {
        timestamps_filter_es_out_Delete(out);
        return 1;
    }
    es_format_t fmt;
    int64_t i_pcrtime;

    es_format_Init(&fmt, VIDEO_ES, VLC_FOURCC('V','I','D','E'));
    fmt.i_id = 0;
    assert(es_out_Add(out, &fmt) == ESID(0));
    p_block->i_dts = TIMELINE_0 + 0;
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 0);
    p_block->i_dts = TIMELINE_0 + 100;
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 100);
    p_block->i_dts = TIMELINE_0 +200;
    es_out_Send(out, ESID(0), p_block);
    p_block->i_dts = TIMELINE_0 +300;
    es_out_Send(out, ESID(0), p_block);
    es_out_SetPCR(out, TIMELINE_0 +300);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 + 300);

    /* New Timeline: Handle PTS jump in the future */
    p_block->i_dts = TIMELINE_1 + 0;
    es_out_SetPCR(out, p_block->i_dts);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 + 300);

    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 400);
    p_block->i_dts = TIMELINE_1 + 100;
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 500);

    /* New Timeline: Handle PTS jump in the past */
    p_block->i_dts = TIMELINE_2 + 0;
    es_out_SetPCR(out, p_block->i_dts);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 + 300);
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 600);

    es_format_Init(&fmt, VIDEO_ES, VLC_FOURCC('V','I','D','2'));
    fmt.i_id = 1;
    assert(es_out_Add(out, &fmt) == ESID(1));
    p_block->i_dts = TIMELINE_2 + 333;
    es_out_Send(out, ESID(1), p_block);
    assert(p_block->i_dts == i_pcrtime + 333);

    for(int i=1; i<MVA_PACKETS + 2; i++)
    {
        p_block->i_dts = TIMELINE_2 + i * 333;
        es_out_Send(out, ESID(1), p_block);
        assert(p_block->i_dts == i_pcrtime + i * 333);
    }

    es_format_Init(&fmt, SPU_ES, VLC_FOURCC('S','P','U','0'));
    fmt.i_id = 2;
    assert(es_out_Add(out, &fmt) == ESID(2));
    p_block->i_dts = TIMELINE_2;
    es_out_Send(out, ESID(2), p_block);
    assert(p_block->i_dts == i_pcrtime);
    p_block->i_dts = TIMELINE_2 + CLOCK_FREQ;
    es_out_Send(out, ESID(2), p_block);
    assert(p_block->i_dts == i_pcrtime + CLOCK_FREQ);

    /* Test Reset */
    assert(es_out_Control(out, ES_OUT_TF_FILTER_RESET) == VLC_SUCCESS);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == VLC_TICK_INVALID);

    /* Test PCR interpolation */
    es_format_Init(&fmt, VIDEO_ES, VLC_FOURCC('V','I','D','E'));
    fmt.i_id = 0;
    assert(es_out_Add(out, &fmt) == ESID(0));
    p_block->i_dts = TIMELINE_0 + 0;
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 0);
    es_out_SetPCR(out, TIMELINE_0 +0);
    p_block->i_dts = TIMELINE_0 + 100;
    es_out_Send(out, ESID(0), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 100);
    es_out_SetPCR(out, TIMELINE_0 +100);
    es_out_SetPCR(out, TIMELINE_0 +200);
    es_out_SetPCR(out, TIMELINE_0 +300);
    es_out_SetPCR(out, TIMELINE_0 +400);
    es_out_SetPCR(out, TIMELINE_0 +500);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 +500);

    assert(es_out_Control(out, ES_OUT_TF_FILTER_DISCONTINUITY) == VLC_SUCCESS);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 +500);

    es_out_SetPCR(out, TIMELINE_1 +0);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 +600);

    es_format_Init(&fmt, SPU_ES, VLC_FOURCC('S','P','U','0'));
    fmt.i_id = 2;
    assert(es_out_Add(out, &fmt) == ESID(2));
    p_block->i_dts = TIMELINE_1 + 100;
    es_out_Send(out, ESID(2), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 700);

    es_out_SetPCR(out, TIMELINE_2 +0);
    assert(es_out_Control(out, ES_OUT_TF_FILTER_GET_TIME, &i_pcrtime) == VLC_SUCCESS);
    assert(i_pcrtime == TIMELINE_0 +700);
    p_block->i_dts = TIMELINE_2 + 300;
    es_out_Send(out, ESID(2), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 1000);
    p_block->i_dts = TIMELINE_2 + 5300;
    es_out_Send(out, ESID(2), p_block);
    assert(p_block->i_dts == TIMELINE_0 + 6000);

    block_Release(p_block);
    es_out_Delete(out);
    return 0;
}
