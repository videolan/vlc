/*****************************************************************************
 * pcr_helper.c:
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
#include <stdint.h>

#include "pcr_helper.h"

#include <vlc_list.h>
#include <vlc_tick.h>

struct transcode_track_pcr_helper
{
    vlc_tick_t max_delay;
    vlc_tick_t current_media_time;
    vlc_tick_t last_dts_output;

    struct vlc_list delayed_frames_data;

    vlc_pcr_sync_t *sync_ref;
    unsigned int pcr_sync_es_id;
};

typedef struct
{
    vlc_tick_t length;
    vlc_tick_t dts;
    struct vlc_list node;
} delayed_frame_data_t;

transcode_track_pcr_helper_t *transcode_track_pcr_helper_New(vlc_pcr_sync_t *sync_ref,
                                                             vlc_tick_t max_delay)
{
    transcode_track_pcr_helper_t *ret = malloc(sizeof(*ret));
    if (unlikely(ret == NULL))
        return NULL;

    if (vlc_pcr_sync_NewESID(sync_ref, &ret->pcr_sync_es_id) != VLC_SUCCESS)
    {
        free(ret);
        return NULL;
    }

    ret->max_delay = max_delay;
    ret->current_media_time = 0;
    ret->last_dts_output = VLC_TICK_INVALID;
    vlc_list_init(&ret->delayed_frames_data);
    ret->sync_ref = sync_ref;

    return ret;
}

void transcode_track_pcr_helper_Delete(transcode_track_pcr_helper_t *pcr_helper)
{
    delayed_frame_data_t *it;
    vlc_list_foreach(it, &pcr_helper->delayed_frames_data, node)
    {
        vlc_list_remove(&it->node);
        free(it);
    }

    vlc_pcr_sync_DelESID(pcr_helper->sync_ref, pcr_helper->pcr_sync_es_id);

    free(pcr_helper);
}

static inline vlc_tick_t
transcode_track_pcr_helper_GetFramePCR(transcode_track_pcr_helper_t *pcr_helper,
                                       vlc_tick_t frame_dts)
{
    vlc_tick_t pcr = VLC_TICK_INVALID;
    vlc_tick_t it = VLC_TICK_INVALID;

    // XXX: `vlc_pcr_sync_SignalFrameOutput` only needs DTS for now. Passing a frame by only filling
    // the DTS is enough.
    const vlc_frame_t fake_frame = {.i_dts = frame_dts};

    while ((it = vlc_pcr_sync_SignalFrameOutput(pcr_helper->sync_ref, pcr_helper->pcr_sync_es_id,
                                                &fake_frame)) != VLC_TICK_INVALID)
    {
        pcr = it;
    }
    return pcr;
}

int transcode_track_pcr_helper_SignalEnteringFrame(transcode_track_pcr_helper_t *pcr_helper,
                                                   const vlc_frame_t *frame,
                                                   vlc_tick_t *dropped_frame_ts)
{
    delayed_frame_data_t *bdata = malloc(sizeof(*bdata));
    if (unlikely(bdata == NULL))
        return VLC_ENOMEM;

    bdata->length = frame->i_length;
    bdata->dts = frame->i_dts;

    pcr_helper->current_media_time += bdata->length;

    vlc_pcr_sync_SignalFrame(pcr_helper->sync_ref, pcr_helper->pcr_sync_es_id, frame);

    vlc_list_append(&bdata->node, &pcr_helper->delayed_frames_data);

    // Something went wrong in the delaying unit.
    // Exceeding this limit usually means the frame was dropped. So in our case, act like it went
    // through.
    // TODO needs to be properly unit-tested.
    if (pcr_helper->current_media_time > pcr_helper->max_delay)
    {
        delayed_frame_data_t *first_bdata = vlc_list_first_entry_or_null(
            &pcr_helper->delayed_frames_data, delayed_frame_data_t, node);
        assert(first_bdata != NULL);

        const vlc_tick_t pcr = transcode_track_pcr_helper_GetFramePCR(pcr_helper, first_bdata->dts);
        *dropped_frame_ts = pcr_helper->last_dts_output == VLC_TICK_INVALID
                                ? pcr
                                : __MIN(pcr_helper->last_dts_output, first_bdata->dts);

        pcr_helper->current_media_time -= first_bdata->length;

        vlc_list_remove(&first_bdata->node);
        free(first_bdata);
    }
    else
    {
        *dropped_frame_ts = VLC_TICK_INVALID;
    }
    return VLC_SUCCESS;
}

vlc_tick_t transcode_track_pcr_helper_SignalLeavingFrame(transcode_track_pcr_helper_t *pcr_helper,
                                                         const vlc_frame_t *frame)
{
    delayed_frame_data_t *frame_data =
        vlc_list_first_entry_or_null(&pcr_helper->delayed_frames_data, delayed_frame_data_t, node);

    assert(frame_data != NULL);
    pcr_helper->last_dts_output = frame->i_dts;
    pcr_helper->current_media_time -= frame_data->length;

    const vlc_tick_t pcr = transcode_track_pcr_helper_GetFramePCR(pcr_helper, frame_data->dts);

    vlc_list_remove(&frame_data->node);
    free(frame_data);

    if (pcr == VLC_TICK_INVALID)
    {
        return VLC_TICK_INVALID;
    }
    return __MIN(frame->i_dts, pcr);
}
