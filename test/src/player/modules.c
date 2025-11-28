// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * modules.c: modules used for the player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#define MODULE_NAME test_src_player
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>
/* Define a builtin module for mocked parts */
const char vlc_module_name[] = MODULE_STRING;

struct aout_sys
{
    vlc_tick_t first_pts;
    vlc_tick_t first_play_date;
    vlc_tick_t pos;

    struct ctx *ctx;
};

static void aout_Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    struct aout_sys *sys = aout->sys;

    if (sys->first_play_date == VLC_TICK_INVALID)
    {
        assert(sys->first_pts == VLC_TICK_INVALID);
        sys->first_play_date = date;
        sys->first_pts = block->i_pts;

        struct ctx *ctx = sys->ctx;
        vlc_player_Lock(ctx->player);
        VEC_PUSH(on_aout_first_pts, sys->first_pts);
        vlc_player_Unlock(ctx->player);
    }

    aout_TimingReport(aout, sys->first_play_date + sys->pos - VLC_TICK_0,
                      sys->first_pts + sys->pos);
    sys->pos += block->i_length;
    block_Release(block);
}

static void aout_Flush(audio_output_t *aout)
{
    struct aout_sys *sys = aout->sys;
    sys->pos = 0;
    sys->first_pts = sys->first_play_date = VLC_TICK_INVALID;
}

static void aout_InstantDrain(audio_output_t *aout)
{
    aout_DrainedReport(aout);
}

static int aout_Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    (void) aout;
    return AOUT_FMT_LINEAR(fmt) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
aout_Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    free(aout->sys);
}

static int aout_Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout->start = aout_Start;
    aout->play = aout_Play;
    aout->pause = NULL;
    aout->flush = aout_Flush;
    aout->stop = aout_Flush;
    aout->volume_set = NULL;
    aout->mute_set = NULL;

    struct aout_sys *sys = aout->sys = malloc(sizeof(*sys));
    assert(sys != NULL);

    sys->ctx = var_InheritAddress(aout, "test-ctx");
    assert(sys->ctx != NULL);

    if (sys->ctx->flags & AUDIO_INSTANT_DRAIN)
        aout->drain = aout_InstantDrain;

    aout_Flush(aout);

    return VLC_SUCCESS;
}

static block_t *resampler_Resample(filter_t *filter, block_t *in)
{
    VLC_UNUSED(filter);
    return in;
}

static void resampler_Close(filter_t *filter)
{
    VLC_UNUSED(filter);
}

static int resampler_Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    static const struct vlc_filter_operations filter_ops =
    { .filter_audio = resampler_Resample, .close = resampler_Close, };
    filter->ops = &filter_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    /* This aout module will report audio timings perfectly, but without any
     * delay, in order to be usable for player tests. Indeed, this aout will
     * report timings immediately from Play(), but points will be in the
     * future (like when aout->time_get() is used). */
    set_capability("audio output", 0)
    set_callbacks(aout_Open, aout_Close)
    add_submodule ()
    /* aout will insert a resampler that can have samples delay, even for 1:1
     * Insert our own resampler that keeps blocks and pts untouched. */
    set_capability ("audio resampler", 9999)
    set_callback (resampler_Open)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};