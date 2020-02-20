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

#define AMEM_SAMPLE_RATE_MAX 384000
#define AMEM_CHAN_MAX 8
#define AMEM_NB_FORMATS 3

/* Forward declaration */
static const char *const format_list[AMEM_NB_FORMATS];

vlc_module_begin ()
    set_shortname (N_("Audio memory"))
    set_description (N_("Audio memory output"))
    set_capability ("audio output", 0)
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AOUT)
    set_callbacks (Open, Close)

    add_string ("amem-format", "S16N",
                N_("Sample format"), N_("Sample format"), false)
        change_string_list( format_list, format_list )
        change_private()
    add_integer ("amem-rate", 44100,
                 N_("Sample rate"), N_("Sample rate"), false)
        change_integer_range (1, AMEM_SAMPLE_RATE_MAX)
        change_private()
    add_integer ("amem-channels", 2,
                 N_("Channels count"), N_("Channels count"), false)
        change_integer_range (1, AMEM_CHAN_MAX)
        change_private()

vlc_module_end ()

static const char *const format_list[AMEM_NB_FORMATS] = {
    "S16N",
    "S32N",
    "FL32",
};

static const vlc_fourcc_t format_list_fourcc[AMEM_NB_FORMATS] = {
    VLC_CODEC_S16N,
    VLC_CODEC_S32N,
    VLC_CODEC_FL32,
};

typedef struct
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
             unsigned rate;
             uint8_t channels;
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
    vlc_mutex_t lock;
} aout_sys_t;

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock(&sys->lock);
    sys->play(sys->opaque, block->p_buffer, block->i_nb_samples, date);
    vlc_mutex_unlock(&sys->lock);
    block_Release (block);
}

static void Pause (audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    void (*cb) (void *, int64_t) = paused ? sys->pause : sys->resume;

    if (cb != NULL)
    {
        vlc_mutex_lock(&sys->lock);
        cb (sys->opaque, date);
        vlc_mutex_unlock(&sys->lock);
    }
}

static void Flush (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    if (sys->flush != NULL)
    {
        vlc_mutex_lock(&sys->lock);
        sys->flush (sys->opaque);
        vlc_mutex_unlock(&sys->lock);
    }
}

static void Drain (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock(&sys->lock);
    sys->drain (sys->opaque);
    vlc_mutex_unlock(&sys->lock);
}

static int VolumeSet (audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    int val;

    sys->volume = vol;

    vlc_mutex_lock(&sys->lock);
    if (sys->ready)
        val = sys->set_volume(sys->opaque, vol, sys->mute);
    else
        val = 0; /* sys->opaque is not yet defined... */
    vlc_mutex_unlock(&sys->lock);

    return val ? -1 : 0;
}

static int MuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    int val;

    sys->mute = mute;

    vlc_mutex_lock(&sys->lock);
    if (sys->ready)
        val = sys->set_volume(sys->opaque, sys->volume, mute);
    else
        val = 0; /* sys->opaque is not yet defined... */
    vlc_mutex_unlock(&sys->lock);

    return val ? -1 : 0;
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

    vlc_mutex_lock(&sys->lock);
    if (sys->cleanup != NULL)
        sys->cleanup (sys->opaque);

    sys->ready = false;
    vlc_mutex_unlock(&sys->lock);
}

static int Start (audio_output_t *aout, audio_sample_format_t *fmt)
{
    aout_sys_t *sys = aout->sys;
    char format[5] = "S16N";
    unsigned channels;
    int i_idx;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    vlc_mutex_lock(&sys->lock);
    if (sys->setup != NULL)
    {
        channels = aout_FormatNbChannels(fmt);

        sys->opaque = sys->setup_opaque;
        if (sys->setup (&sys->opaque, format, &fmt->i_rate, &channels))
        {
            vlc_mutex_unlock(&sys->lock);
            return VLC_EGENERIC;
        }
    }
    else
    {
        char *psz_format;

        psz_format = var_InheritString (aout, "amem-format");
        if (psz_format == NULL)
        {
            vlc_mutex_unlock(&sys->lock);
            return VLC_ENOMEM;
        }

        if (strlen(psz_format) != 4) /* fourcc string length */
        {
            msg_Err (aout, "Invalid paramameter for amem-format: '%s'",
                     psz_format);
            free(psz_format);
            vlc_mutex_unlock(&sys->lock);

            return VLC_EGENERIC;
        }

        strcpy(format, psz_format);
        free(psz_format);
        fmt->i_rate = sys->rate;
        channels = sys->channels;
    }

    /* Initialize volume (in case the UI changed volume before setup) */
    sys->ready = true;
    if (sys->set_volume != NULL)
        sys->set_volume(sys->opaque, sys->volume, sys->mute);
    vlc_mutex_unlock(&sys->lock);

    /* amem-format: string to fourcc */
    for (i_idx = 0; i_idx < AMEM_NB_FORMATS; i_idx++)
    {
        if (strncmp(format,
                    format_list[i_idx],
                    strlen(format_list[i_idx])) == 0)
        {
            fmt->i_format = format_list_fourcc[i_idx];

            break;
        }
    }

    /* Ensure that format is supported */
    if (fmt->i_rate == 0 || fmt->i_rate > AMEM_SAMPLE_RATE_MAX
     || channels == 0 || channels > AMEM_CHAN_MAX
     || i_idx == AMEM_NB_FORMATS)
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
            vlc_assert_unreachable();
    }

    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
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
    vlc_mutex_init(&sys->lock);

    if (sys->play == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = aout_TimeGetDefault;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->drain = sys->drain ? Drain : NULL;
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
