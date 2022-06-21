/*****************************************************************************
 * src/test/pcr_delayer.c
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

#undef NDEBUG

#include <assert.h>

#include <vlc_common.h>

#include <vlc_arrays.h>
#include <vlc_frame.h>
#include <vlc_tick.h>
#include <vlc_vector.h>

#include "../modules/stream_out/transcode/pcr_sync.h"

typedef struct
{
    enum test_element_type
    {
        TEST_ELEM_TYPE_FRAME,
        TEST_ELEM_TYPE_PCR,
    } type;
    union
    {
        struct
        {
            vlc_tick_t dts;
            unsigned int track_number;
            bool discontinuity;
        };
        vlc_tick_t pcr;
    };
} test_element;

typedef struct
{
    size_t input_count;
    const test_element *input;
    size_t output_count;
    const test_element *output;

    size_t track_count;
} test_context;

#undef FRAME
#define FRAME(id, val)                                                                             \
    ((test_element){.type = TEST_ELEM_TYPE_FRAME, .dts = val, .track_number = id})
#undef DISCONTINUITY
#define DISCONTINUITY(id, val)                                                                     \
    ((test_element){                                                                               \
        .type = TEST_ELEM_TYPE_FRAME, .dts = val, .track_number = id, .discontinuity = true})
#undef PCR
#define PCR(val) ((test_element){.type = TEST_ELEM_TYPE_PCR, .pcr = val})

static void test_Sync(vlc_pcr_sync_t *sync, const test_context context)
{
    // Add all tracks to the synchronizer and keep their id.
    struct VLC_VECTOR(unsigned int) track_ids_vec = VLC_VECTOR_INITIALIZER;
    bool alloc_success = vlc_vector_reserve(&track_ids_vec, context.track_count);
    assert(alloc_success);
    for (size_t i = 0; i < context.track_count; ++i)
    {
        unsigned int id;
        alloc_success = vlc_pcr_sync_NewESID(sync, &id) == VLC_SUCCESS;
        assert(alloc_success);
        alloc_success = vlc_vector_push(&track_ids_vec, id);
        assert(alloc_success);
    }

    // Queue PCR events in the synchronizer and signal all the frames.
    for (size_t i = 0; i < context.input_count; ++i)
    {
        const test_element *elem = &context.input[i];
        if (elem->type == TEST_ELEM_TYPE_PCR)
            vlc_pcr_sync_SignalPCR(sync, elem->pcr);
        else
        {
            const vlc_frame_t mock = {.i_dts = elem->dts,
                                      .i_flags =
                                          elem->discontinuity ? VLC_FRAME_FLAG_DISCONTINUITY : 0};
            vlc_pcr_sync_SignalFrame(sync, track_ids_vec.data[elem->track_number], &mock);
        }
    }

    // Signal all the frame leaving and check that PCR events are properly aligned.
    vlc_tick_t last_pcr;
    for (size_t i = 0; i < context.output_count; ++i)
    {
        const test_element *elem = &context.output[i];
        if (elem->type == TEST_ELEM_TYPE_FRAME)
        {
            const vlc_frame_t mock = {.i_dts = elem->dts};
            last_pcr =
                vlc_pcr_sync_SignalFrameOutput(sync, track_ids_vec.data[elem->track_number], &mock);
        }
        else
        {
            assert(elem->pcr == last_pcr);
        }
    }

    vlc_vector_destroy(&track_ids_vec);
}

static void test_Run(void (*test)(vlc_pcr_sync_t *))
{
    vlc_pcr_sync_t *sync = vlc_pcr_sync_New();
    assert(sync != NULL);

    test(sync);

    vlc_pcr_sync_Delete(sync);
}

typedef enum
{
    VIDEO_TRACK,
    AUDIO_TRACK,
    SUB_TRACK,

    MAX_TRACKS
} track_ids;

static void test_Simple(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 1), PCR(10), FRAME(VIDEO_TRACK, 20)};
    test_Sync(sync, (test_context){
                        .input = input,
                        .input_count = ARRAY_SIZE(input),
                        .output = input,
                        .output_count = ARRAY_SIZE(input),
                        .track_count = 1,
                    });
}

static void test_MultipleTracks(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {
        FRAME(VIDEO_TRACK, 1),  FRAME(AUDIO_TRACK, 1),  PCR(10),
        FRAME(VIDEO_TRACK, 15), FRAME(AUDIO_TRACK, 15), FRAME(SUB_TRACK, 20)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = input,
                                   .output_count = ARRAY_SIZE(input),
                                   .track_count = MAX_TRACKS});
}

static void test_MultipleTracksButOnlyOneSends(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 1), PCR(10), FRAME(VIDEO_TRACK, 15)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = input,
                                   .output_count = ARRAY_SIZE(input),
                                   .track_count = MAX_TRACKS});
}

static void test_InvalidDTS(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 1), FRAME(VIDEO_TRACK, VLC_TICK_INVALID),
                                  FRAME(VIDEO_TRACK, VLC_TICK_INVALID), PCR(5),
                                  FRAME(VIDEO_TRACK, 10)};
    const test_element output[] = {FRAME(VIDEO_TRACK, 1), PCR(5), FRAME(VIDEO_TRACK, 10)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = output,
                                   .output_count = ARRAY_SIZE(output),
                                   .track_count = MAX_TRACKS});
}

static void test_LowDiscontinuity(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 200), DISCONTINUITY(VIDEO_TRACK, 1), PCR(5),
                                  FRAME(VIDEO_TRACK, 10)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = input,
                                   .output_count = ARRAY_SIZE(input),
                                   .track_count = MAX_TRACKS});
}

static void test_HighDiscontinuity(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 1), DISCONTINUITY(VIDEO_TRACK, 200), PCR(210),
                                  FRAME(VIDEO_TRACK, 220)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = input,
                                   .output_count = ARRAY_SIZE(input),
                                   .track_count = MAX_TRACKS});
}
static void test_TwoDiscontinuities(vlc_pcr_sync_t *sync)
{
    const test_element input[] = {FRAME(VIDEO_TRACK, 200), DISCONTINUITY(VIDEO_TRACK, 1),
                                  DISCONTINUITY(VIDEO_TRACK, 210), PCR(210),
                                  FRAME(VIDEO_TRACK, 220)};
    test_Sync(sync, (test_context){.input = input,
                                   .input_count = ARRAY_SIZE(input),
                                   .output = input,
                                   .output_count = ARRAY_SIZE(input),
                                   .track_count = MAX_TRACKS});
}

int main()
{
    test_Run(test_Simple);
    test_Run(test_MultipleTracks);
    test_Run(test_MultipleTracksButOnlyOneSends);
    test_Run(test_InvalidDTS);
    test_Run(test_LowDiscontinuity);
    test_Run(test_HighDiscontinuity);
    test_Run(test_TwoDiscontinuities);
}
