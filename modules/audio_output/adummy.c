/*****************************************************************************
 * adummy.c : dummy audio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy audio output") )
    set_capability( "audio output", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "dummy" )
vlc_module_end ()

#define A52_FRAME_NB 1536

struct aout_sys
{
    vlc_tick_t first_play_date;
    vlc_tick_t length;
};

static int TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    struct aout_sys *sys = aout->sys;

    if (unlikely(sys->first_play_date == VLC_TICK_INVALID))
    {
        *delay = 0;
        return 0;
    }

    vlc_tick_t time_since_first_play = vlc_tick_now() - sys->first_play_date;
    assert(time_since_first_play >= 0);

    if (likely(sys->length > time_since_first_play))
    {
        *delay = sys->length - time_since_first_play;
        return 0;
    }

    msg_Warn(aout, "underflow");
    return -1;
}

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    struct aout_sys *sys = aout->sys;

    if (unlikely(sys->first_play_date == VLC_TICK_INVALID))
        sys->first_play_date = vlc_tick_now();
    sys->length += block->i_length;

    block_Release( block );
    (void) date;
}

static void Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    (void) aout; (void) paused; (void) date;
}

static void Flush(audio_output_t *aout)
{
    struct aout_sys *sys = aout->sys;

    sys->first_play_date = VLC_TICK_INVALID;
    sys->length = 0;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    (void) aout;

    switch (fmt->i_format)
    {
        case VLC_CODEC_A52:
        case VLC_CODEC_EAC3:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            break;
        case VLC_CODEC_DTS:
        case VLC_CODEC_TRUEHD:
        case VLC_CODEC_MLP:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_rate = 768000;
            fmt->i_bytes_per_frame = 16;
            fmt->i_frame_length = 1;
            break;
        default:
            assert(AOUT_FMT_LINEAR(fmt));
            assert(aout_FormatNbChannels(fmt) > 0);
            fmt->i_format = HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;
            fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
            break;
    }

    return VLC_SUCCESS;
}

static void Stop(audio_output_t *aout)
{
    (void) aout;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    free(aout->sys);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    struct aout_sys *sys = aout->sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->first_play_date = VLC_TICK_INVALID;
    sys->length = 0;

    aout->start = Start;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->stop = Stop;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    return VLC_SUCCESS;
}
