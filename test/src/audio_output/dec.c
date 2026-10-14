/*****************************************************************************
 * audio_output/dec.c: unit test for the audio output
 *****************************************************************************
 * Copyright (C) 2026 Alexandre Janniaux <ajanni@videolabs.io>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_tick.h>
#include <vlc_es.h>
#include <vlc_block.h>
#include <vlc_fourcc.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_modules.h>

#include "../../../src/audio_output/aout_internal.h"
#include "../../../src/clock/clock.h"

#include <vlc/vlc.h>
#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"

#define MODULE_NAME test_audio_output_dec
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>

/* Define a builtin module for the mocked audio output. */
const char vlc_module_name[] = MODULE_STRING;

static void aout_Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    (void) aout; (void) date;
    block_Release(block);
}

static void aout_Flush(audio_output_t *aout)
{
    (void) aout;
}

static int aout_Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    (void) aout;
    return AOUT_FMT_LINEAR(fmt) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void aout_Stop(audio_output_t *aout)
{
    (void) aout;
}

static int aout_Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout->start = aout_Start;
    aout->play = aout_Play;
    aout->pause = NULL;
    aout->flush = aout_Flush;
    aout->stop = aout_Stop;
    aout->drain = NULL;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    aout->device_select = NULL;
    return VLC_SUCCESS;
}

static int resampling;

static block_t *resampler_Resample(filter_t *f, block_t *in)
{
    resampling = (int)f->fmt_in.audio.i_rate - (int)f->fmt_out.audio.i_rate;
    return in;
}

static int resampler_Open(vlc_object_t *obj)
{
    static const struct vlc_filter_operations ops = {
        .filter_audio = resampler_Resample,
    };
    ((filter_t *)obj)->ops = &ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_capability("audio output", 0)
    set_callback(aout_Open)
    add_submodule()
        set_capability("audio resampler", 9999)
        set_callback(resampler_Open)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

/* Stubs to remove vout from linking */
void *vout_Create(void *obj);
void *vout_Create(void *obj) { (void) obj; return NULL; }
int vout_Request(const void *cfg, void *vctx, void *input);
int vout_Request(const void *cfg, void *vctx, void *input)
{ (void) cfg; (void) vctx; (void) input; return -1; }
bool config_IsSafe(const char *name);
bool config_IsSafe(const char *name) { (void) name; return false; }

static vlc_tick_t mock_now = VLC_TICK_0;

vlc_tick_t vlc_tick_now(void)
{
    return mock_now;
}

#define NB_SAMPLES 1024

static block_t *make_frame(const audio_sample_format_t *fmt, vlc_tick_t pts)
{
    block_t *block = block_Alloc(NB_SAMPLES * fmt->i_bytes_per_frame);
    assert(block != NULL);
    block->i_nb_samples = NB_SAMPLES;
    block->i_pts = block->i_dts = pts;
    return block;
}

static void start_output(vlc_clock_t *clock, vlc_tick_t system_ts,
                         vlc_tick_t ts)
{
    vlc_clock_Lock(clock);
    vlc_clock_Start(clock, system_ts, ts);
    vlc_clock_Unlock(clock);
}

static void test_flush_stops_resampling(libvlc_int_t *vlc)
{
    struct vlc_logger *logger = vlc->obj.logger;

    audio_sample_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_format = VLC_CODEC_S16N;
    fmt.i_rate = 48000;
    fmt.i_physical_channels = AOUT_CHANS_STEREO;
    aout_FormatPrepare(&fmt);

    const vlc_tick_t frame_len = vlc_tick_from_samples(NB_SAMPLES, fmt.i_rate);

    vlc_clock_main_t *clockbus = vlc_clock_main_New(logger, NULL);
    assert(clockbus != NULL);

    vlc_clock_main_Lock(clockbus);
    vlc_clock_main_SetInputDejitter(clockbus, 0);
    vlc_clock_main_SetDejitter(clockbus, 0);
    vlc_clock_t *input = vlc_clock_main_CreateInputMaster(clockbus);
    vlc_clock_t *aout_clock =
        vlc_clock_main_CreateSlave(clockbus, "audio", AUDIO_ES, NULL, NULL);
    assert(input != NULL && aout_clock != NULL);

    const vlc_tick_t sys0 = VLC_TICK_0 + VLC_TICK_FROM_MS(1000);
    const vlc_tick_t stream0 = VLC_TICK_0 + VLC_TICK_FROM_MS(1000);
    mock_now = sys0;
    vlc_clock_Start(input, sys0, stream0);
    vlc_clock_main_Unlock(clockbus);

    var_Create(vlc, "aout", VLC_VAR_STRING);
    var_SetString(vlc, "aout", MODULE_STRING);

    audio_output_t *aout = aout_New(vlc);
    assert(aout != NULL);

    const struct vlc_aout_stream_cfg cfg = {
        .fmt = &fmt,
        .profile = 0,
        .clock = aout_clock,
        .str_id = "audio",
        .replay_gain = NULL,
    };
    vlc_aout_stream *stream = vlc_aout_stream_New(aout, &cfg);
    assert(stream != NULL);

    start_output(aout_clock, sys0, stream0);
    resampling = 0;
    vlc_aout_stream_Play(stream, make_frame(&fmt, stream0));
    assert(resampling == 0);

    /*      output               clock
     *      reports              start 
     *      playing              time
     * --------|-------------------|----------------------> time
     *        -100ms               0ms
     *
     * The output reports playing 100ms behind the clock, which will
     * trigger resampling as it's less than the "too late" but more
     * than the AOUT_MAX_PTS_DELAY. */

    aout_TimingReport(aout, mock_now, stream0 - VLC_TICK_FROM_MS(100));
    vlc_aout_stream_Play(stream, make_frame(&fmt, stream0 + frame_len));
    vlc_aout_stream_Play(stream, make_frame(&fmt, stream0 + 2 * frame_len));
    assert(resampling > 0);

    /* In case of seek, the audio output is flushed, and we should then
     * re-evaluate whether we still need to resample. */
    vlc_aout_stream_Flush(stream);

    const vlc_tick_t sys1 = sys0 + VLC_TICK_FROM_MS(500);
    const vlc_tick_t stream1 = stream0 + VLC_TICK_FROM_SEC(30);
    mock_now = sys1;
    vlc_clock_main_Lock(clockbus);
    vlc_clock_Reset(input);
    vlc_clock_Start(input, sys1, stream1);
    vlc_clock_main_Unlock(clockbus);
    start_output(aout_clock, sys1, stream1);

    vlc_aout_stream_Play(stream, make_frame(&fmt, stream1));
    assert(resampling == 0);

    vlc_aout_stream_Delete(stream);

    aout_Release(aout);

    vlc_clock_Delete(aout_clock);
    vlc_clock_Delete(input);
    vlc_clock_main_Delete(clockbus);
}

int main(void)
{
    test_init();

    libvlc_instance_t *vlc = libvlc_new(0, NULL);
    assert(vlc != NULL);

    test_flush_stops_resampling(vlc->p_libvlc_int);

    libvlc_release(vlc);
    return 0;
}
