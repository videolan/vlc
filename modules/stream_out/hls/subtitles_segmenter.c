/*****************************************************************************
 * subtitle_segmenter.c: Create subtitle segments
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>

#include <vlc_block.h>
#include <vlc_frame.h>
#include <vlc_sout.h>
#include <vlc_tick.h>

#include "hls.h"

/**
 * The hls_sub_segmenter is a meta-muxer used to create subtitles segments. It
 * handles subtitle frames splitting, re-creation of the subtitles muxer and
 * empty segments creation when no data is available.
 */
struct hls_sub_segmenter
{
    sout_mux_t owner;

    const struct hls_config *config;
    sout_mux_t *spu_muxer;
    sout_input_t *spu_muxer_input;
    bool segment_empty;
    vlc_tick_t last_segment;
    vlc_tick_t last_stream_update;
};

static int hls_sub_segmenter_ResetMuxer(struct hls_sub_segmenter *segmenter,
                                        sout_input_t *owner_input)
{
    if (segmenter->spu_muxer_input != NULL)
        sout_MuxDeleteStream(segmenter->spu_muxer, segmenter->spu_muxer_input);
    if (segmenter->spu_muxer != NULL)
        sout_MuxDelete(segmenter->spu_muxer);

    segmenter->segment_empty = true;

    segmenter->spu_muxer = sout_MuxNew(segmenter->owner.p_access, "webvtt");
    if (segmenter->spu_muxer == NULL)
        return VLC_EGENERIC;

    segmenter->spu_muxer_input =
        sout_MuxAddStream(segmenter->spu_muxer, &owner_input->fmt);

    // Disable mux caching.
    segmenter->spu_muxer->b_waiting_stream = false;
    return VLC_SUCCESS;
}

static int hls_sub_segmenter_EndSegment(struct hls_sub_segmenter *segmenter,
                                        sout_input_t *input)
{
    segmenter->last_segment += segmenter->config->segment_length;
    return hls_sub_segmenter_ResetMuxer(segmenter, input);
}

static int hls_sub_segmenter_EndEmptySegment(
    struct hls_sub_segmenter *segmenter, sout_input_t *input, vlc_tick_t len)
{
    char *vtt_header = strdup("WEBVTT\n\n");
    if (unlikely(vtt_header == NULL))
        return VLC_ENOMEM;

    block_t *empty_segment = block_heap_Alloc(vtt_header, strlen(vtt_header));
    if (unlikely(empty_segment == NULL))
        return VLC_ENOMEM;

    empty_segment->i_flags |= BLOCK_FLAG_HEADER;
    empty_segment->i_length = len;
    if (sout_AccessOutWrite(segmenter->spu_muxer->p_access, empty_segment) < 0)
        return VLC_EGENERIC;

    return hls_sub_segmenter_EndSegment(segmenter, input);
}

static int hls_sub_segmenter_Add(sout_mux_t *mux, sout_input_t *input)
{
    struct hls_sub_segmenter *segmenter = mux->p_sys;
    hls_sub_segmenter_ResetMuxer(segmenter, input);
    return VLC_SUCCESS;
}

static int hls_sub_segmenter_MuxSend(struct hls_sub_segmenter *segmenter,
                                     vlc_frame_t *spu)
{
    segmenter->segment_empty = false;
    return sout_MuxSendBuffer(
        segmenter->spu_muxer, segmenter->spu_muxer_input, spu);
}

static void hls_sub_segmenter_Del(sout_mux_t *mux, sout_input_t *input)
{
    struct hls_sub_segmenter *segmenter = mux->p_sys;

    if (segmenter->last_segment != segmenter->last_stream_update)
    {
        const vlc_tick_t seglen =
            segmenter->last_stream_update - segmenter->last_segment;

        if (!vlc_fifo_IsEmpty(input->p_fifo))
        {
            /* Drain the last ephemeral SPU. */
            vlc_frame_t *ephemeral = vlc_fifo_Get(input->p_fifo);
            assert(ephemeral->i_length == VLC_TICK_INVALID);
            ephemeral->i_length = seglen;
            hls_sub_segmenter_MuxSend(segmenter, ephemeral);
        }
        else if (segmenter->segment_empty)
        {
            /* Drain an empty last segment. */
            hls_sub_segmenter_EndEmptySegment(segmenter, input, seglen);
        }
    }

    if (segmenter->spu_muxer_input != NULL)
        sout_MuxDeleteStream(segmenter->spu_muxer, segmenter->spu_muxer_input);
    if (segmenter->spu_muxer != NULL)
        sout_MuxDelete(segmenter->spu_muxer);
}

static vlc_frame_t* hls_sub_segmenter_CutEphemeral(vlc_frame_t *ephemeral, vlc_tick_t length)
{
    vlc_frame_t *cut = vlc_frame_Duplicate(ephemeral);
    if (unlikely(cut == NULL))
        return NULL;

    cut->i_length = length;
    ephemeral->i_pts += cut->i_length;
    return cut;
}

/**
 * Should be called at frequent PCR interval.
 *
 * Creates empty segments when no data is sent to the muxer for a time superior
 * to segment length. 
 */
void hls_sub_segmenter_SignalStreamUpdate(sout_mux_t *mux,
                                          vlc_tick_t stream_time)
{
    if (mux->i_nb_inputs == 0)
        return;

    sout_input_t *owner_input = mux->pp_inputs[0];

    struct hls_sub_segmenter *segmenter = mux->p_sys;
    segmenter->last_stream_update = stream_time;
    if (stream_time - segmenter->last_segment <
        segmenter->config->segment_length)
        return;

    if (!vlc_fifo_IsEmpty(owner_input->p_fifo))
    {
        vlc_frame_t *ephemeral = vlc_fifo_Show(owner_input->p_fifo);

        /* The segmenter should only keep ephemeral SPUs in queue. */
        assert(ephemeral->i_length == VLC_TICK_INVALID);

        const vlc_tick_t eph_len =
            (segmenter->last_segment + segmenter->config->segment_length) -
            ephemeral->i_pts;
        vlc_frame_t *cut = hls_sub_segmenter_CutEphemeral(ephemeral, eph_len);
        if (likely(cut != NULL))
            hls_sub_segmenter_MuxSend(segmenter, cut);
    }

    if (!segmenter->segment_empty)
    {
        hls_sub_segmenter_EndSegment(segmenter, owner_input);
        return;
    }

    hls_sub_segmenter_EndEmptySegment(
        segmenter, owner_input, segmenter->config->segment_length);
}

static int
hls_sub_segmenter_ProcessSingleFrame(struct hls_sub_segmenter *segmenter,
                                     sout_input_t *owner_input,
                                     vlc_frame_t *spu)
{
    const vlc_tick_t seglen = segmenter->config->segment_length;
    if (spu->i_pts >= segmenter->last_segment + seglen)
        hls_sub_segmenter_EndSegment(segmenter, owner_input);

    // If the subtitle overlaps between segments, we have to split it.
    while (spu->i_pts + spu->i_length > segmenter->last_segment + seglen)
    {
        vlc_frame_t *capped_spu = vlc_frame_Duplicate(spu);
        if (unlikely(capped_spu == NULL))
        {
            vlc_frame_Release(spu);
            return VLC_ENOMEM;
        }

        // Extra time not fitting in the current segment.
        const vlc_tick_t extra_time =
            (spu->i_pts - VLC_TICK_0 + spu->i_length) -
            segmenter->last_segment - seglen;

        // Shorten the copy length so it fits the current segment.
        capped_spu->i_length -= extra_time;

        if (spu->i_dts != VLC_TICK_INVALID)
            spu->i_dts += capped_spu->i_length;
        spu->i_pts += capped_spu->i_length;
        spu->i_length = extra_time;

        const int status = hls_sub_segmenter_MuxSend(segmenter, capped_spu);
        if (status != VLC_SUCCESS)
        {
            vlc_frame_Release(spu);
            return status;
        }
        hls_sub_segmenter_EndSegment(segmenter, owner_input);
    }
    return hls_sub_segmenter_MuxSend(segmenter, spu);
}

static int hls_sub_segmenter_Process(sout_mux_t *mux)
{
    struct hls_sub_segmenter *segmenter = mux->p_sys;

    if (mux->i_nb_inputs == 0)
        return VLC_SUCCESS;

    sout_input_t *input = mux->pp_inputs[0];

    vlc_fifo_Lock(input->p_fifo);
    vlc_frame_t *chain = vlc_fifo_DequeueAllUnlocked(input->p_fifo);

    int status = VLC_SUCCESS;
    while (chain != NULL)
    {
        if (chain->i_length == VLC_TICK_INVALID)
        {
            /* Ephemeral SPUs length depends on the next SPUs timestamps. */
            if (chain->p_next == NULL)
            {
                vlc_fifo_QueueUnlocked(input->p_fifo, chain);
                break;
            }
            else
            {
                chain->i_length = chain->p_next->i_pts - chain->i_pts;
            }
        }

        vlc_frame_t *next = chain->p_next;
        chain->p_next = NULL;
        status =
            hls_sub_segmenter_ProcessSingleFrame(segmenter, input, chain);
        if (status != VLC_SUCCESS)
        {
            vlc_frame_ChainRelease(next);
            break;
        }
        chain = next;
    }
    vlc_fifo_Unlock(input->p_fifo);
    return status;
}

static int hls_sub_segmenter_Control(sout_mux_t *mux, int query, va_list args)
{
    if (query != MUX_CAN_ADD_STREAM_WHILE_MUXING)
        return VLC_ENOTSUP;

    *(va_arg(args, bool *)) = false;
    (void)mux;
    return VLC_SUCCESS;
}

sout_mux_t *CreateSubtitleSegmenter(sout_access_out_t *access,
                                    const struct hls_config *config)
{
    struct hls_sub_segmenter *segmenter =
        vlc_object_create(access, sizeof(*segmenter));
    if (unlikely(segmenter == NULL))
        return NULL;

    sout_mux_t *mux = &segmenter->owner;
    mux->psz_mux = strdup("hls-sub-segmenter");
    if (unlikely(mux->psz_mux == NULL))
    {
        vlc_object_delete(mux);
        return NULL;
    }

    mux->p_module = NULL;
    mux->p_cfg = NULL;
    mux->i_nb_inputs = 0;
    mux->pp_inputs = NULL;
    mux->b_add_stream_any_time = false;
    mux->b_waiting_stream = true;
    mux->i_add_stream_start = VLC_TICK_INVALID;
    mux->p_sys = segmenter;
    mux->p_access = access;

    mux->pf_addstream = hls_sub_segmenter_Add;
    mux->pf_delstream = hls_sub_segmenter_Del;
    mux->pf_mux = hls_sub_segmenter_Process;
    mux->pf_control = hls_sub_segmenter_Control;

    segmenter->config = config;
    segmenter->spu_muxer = NULL;
    segmenter->segment_empty = true;
    segmenter->last_segment = VLC_TICK_0;
    segmenter->last_stream_update = VLC_TICK_INVALID;
    return &segmenter->owner;
}
