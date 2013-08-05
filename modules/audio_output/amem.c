/*****************************************************************************
 * amem.c : virtual LibVLC audio output plugin
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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
#include <assert.h>

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("Audio memory"))
    set_description (N_("Audio memory output"))
    set_capability ("audio output", 0)
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AOUT)
    set_callbacks (Open, Close)

    add_string ("amem-format", "S16N",
                N_("Sample format"), N_("Sample format"), false)
        change_private()
    add_integer ("amem-rate", 44100,
                 N_("Sample rate"), N_("Sample rate"), false)
        change_integer_range (1, 352800)
        change_private()
    add_integer ("amem-channels", 2,
                 N_("Channels count"), N_("Channels count"), false)
        change_integer_range (1, AOUT_CHAN_MAX)
        change_private()

vlc_module_end ()

struct aout_sys_t
{
    void *opaque;
    int (*setup) (void **, char *, unsigned *, unsigned *);
    void (*cleanup) (void *opaque);
    union
    {
        struct
        {
            void *setup_opaque;
        };
        struct
        {
             unsigned rate:18;
             unsigned channels:14;
        };
    };
    void (*play) (void *opaque, const void *data, unsigned count, int64_t pts);
    void (*pause) (void *opaque, int64_t pts);
    void (*resume) (void *opaque, int64_t pts);
    void (*flush) (void *opaque);
    void (*drain) (void *opaque);
    int (*set_volume) (void *opaque, float vol, bool mute);
    float volume;
    bool mute;
    bool ready;
};

static void Play (audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;

    sys->play (sys->opaque, block->p_buffer, block->i_nb_samples,
               block->i_pts);
    block_Release (block);
}

static void Pause (audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    void (*cb) (void *, int64_t) = paused ? sys->pause : sys->resume;

    if (cb != NULL)
        cb (sys->opaque, date);
}

static void Flush (audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    void (*cb) (void *) = wait ? sys->drain : sys->flush;

    if (cb != NULL)
        cb (sys->opaque);
}

static int VolumeSet (audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;

    sys->volume = vol;
    if (sys->ready)
        return 0; /* sys->opaque is not yet defined... */
    return sys->set_volume (sys->opaque, vol, sys->mute) ? -1 : 0;
}

static int MuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    sys->mute = mute;
    if (!sys->ready)
        return 0; /* sys->opaque is not yet defined... */
    return sys->set_volume (sys->opaque, sys->volume, mute) ? -1 : 0;
}

static int SoftVolumeSet (audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;

    vol = vol * vol * vol;
    if (!sys->mute && aout_GainRequest (aout, vol))
        return -1;
    sys->volume = vol;
    return 0;
}

static int SoftMuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    if (aout_GainRequest (aout, mute ? 0.f : sys->volume))
        return -1;
    sys->mute = mute;
    return 0;
}

static void Stop (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    if (sys->cleanup != NULL)
        sys->cleanup (sys->opaque);
    sys->ready = false;
}

static int Start (audio_output_t *aout, audio_sample_format_t *fmt)
{
    aout_sys_t *sys = aout->sys;
    char format[5] = "S16N";
    unsigned channels;

    if (sys->setup != NULL)
    {
        channels = aout_FormatNbChannels(fmt);

        sys->opaque = sys->setup_opaque;
        if (sys->setup (&sys->opaque, format, &fmt->i_rate, &channels))
            return VLC_EGENERIC;
    }
    else
    {
        fmt->i_rate = sys->rate;
        channels = sys->channels;
    }

    /* Initialize volume (in case the UI changed volume before setup) */
    sys->ready = true;
    if (sys->set_volume != NULL)
        sys->set_volume(sys->opaque, sys->volume, sys->mute);

    /* Ensure that format is supported */
    if (fmt->i_rate == 0 || fmt->i_rate > 192000
     || channels == 0 || channels > AOUT_CHAN_MAX
     || strcmp(format, "S16N") /* TODO: amem-format */)
    {
        msg_Err (aout, "format not supported: %s, %u channel(s), %u Hz",
                 format, channels, fmt->i_rate);
        Stop (aout);
        return VLC_EGENERIC;
    }

    /* channel mapping */
    switch (channels)
    {
        case 1:
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
            break;
        case 2:
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            break;
        case 3:
            fmt->i_physical_channels = AOUT_CHANS_2_1;
            break;
        case 4:
            fmt->i_physical_channels = AOUT_CHANS_4_0;
            break;
        case 5:
            fmt->i_physical_channels = AOUT_CHANS_5_0;
            break;
        case 6:
            fmt->i_physical_channels = AOUT_CHANS_5_1;
            break;
        case 7:
            fmt->i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT |
                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
            break;
        case 8:
            fmt->i_physical_channels = AOUT_CHANS_7_1;
            break;
        default:
            assert(0);
    }

    fmt->i_format = VLC_CODEC_S16N;
    fmt->i_original_channels = fmt->i_physical_channels;
    return VLC_SUCCESS;
}

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    void *opaque = var_InheritAddress (obj, "amem-data");
    sys->setup = var_InheritAddress (obj, "amem-setup");
    if (sys->setup != NULL)
    {
        sys->cleanup = var_InheritAddress (obj, "amem-cleanup");
        sys->setup_opaque = opaque;
    }
    else
    {
        sys->cleanup = NULL;
        sys->opaque = opaque;
        sys->rate = var_InheritInteger (obj, "amem-rate");
        sys->channels = var_InheritInteger (obj, "amem-channels");
    }
    sys->play = var_InheritAddress (obj, "amem-play");
    sys->pause = var_InheritAddress (obj, "amem-pause");
    sys->resume = var_InheritAddress (obj, "amem-resume");
    sys->flush = var_InheritAddress (obj, "amem-flush");
    sys->drain = var_InheritAddress (obj, "amem-drain");
    sys->set_volume = var_InheritAddress (obj, "amem-set-volume");
    sys->volume = 1.;
    sys->mute = false;
    sys->ready = false;
    if (sys->play == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = NULL;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    if (sys->set_volume != NULL)
    {
        aout->volume_set = VolumeSet;
        aout->mute_set = MuteSet;
    }
    else
    {
        aout->volume_set = SoftVolumeSet;
        aout->mute_set = SoftMuteSet;
    }
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free (sys);
}
