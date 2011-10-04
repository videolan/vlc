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
        change_integer_range (1, 192000)
        change_private()
    add_integer ("amem-channels", 2,
                 N_("Channels count"), N_("Channels count"), false)
        change_integer_range (1, AOUT_CHAN_MAX)
        change_private()

vlc_module_end ()

struct aout_sys_t
{
    void *opaque;
    void (*play) (void *opaque, const void *data, unsigned count, int64_t pts);
    void (*pause) (void *opaque, int64_t pts);
    void (*resume) (void *opaque, int64_t pts);
    void (*flush) (void *opaque);
    void (*drain) (void *opaque);
    int (*set_volume) (void *opaque, float vol, bool mute);
    void (*cleanup) (void *opaque);
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

static int VolumeSet (audio_output_t *aout, float vol, bool mute)
{
    aout_sys_t *sys = aout->sys;

    return sys->set_volume (sys->opaque, vol, mute) ? -1 : 0;
}

typedef int (*vlc_audio_format_cb) (void **, char *, unsigned *, unsigned *);

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    sys->opaque = var_InheritAddress (obj, "amem-data");
    sys->play = var_InheritAddress (obj, "amem-play");
    sys->pause = var_InheritAddress (obj, "amem-pause");
    sys->resume = var_InheritAddress (obj, "amem-resume");
    sys->flush = var_InheritAddress (obj, "amem-flush");
    sys->drain = var_InheritAddress (obj, "amem-drain");
    sys->set_volume = var_InheritAddress (obj, "amem-set-volume");
    sys->cleanup = NULL; /* defer */
    if (sys->play == NULL)
        goto error;

    vlc_audio_format_cb setup = var_InheritAddress (obj, "amem-setup");
    char format[5] = "S16N";
    unsigned rate, channels;

    if (setup != NULL)
    {
        rate = aout->format.i_rate;
        channels = aout_FormatNbChannels(&aout->format);

        if (setup (&sys->opaque, format, &rate, &channels))
            goto error;
        /* Only call this callback if setup succeeded */
        sys->cleanup = var_InheritAddress (obj, "amem-cleanup");
    }
    else
    {
        rate = var_InheritInteger (obj, "amem-rate");
        channels = var_InheritInteger (obj, "amem-channels");
    }

    if (rate == 0 || rate > 192000
     || channels == 0 || channels > AOUT_CHAN_MAX)
        goto error;

    /* TODO: amem-format */
    if (strcmp(format, "S16N"))
    {
        msg_Err (aout, "format not supported");
        goto error;
    }

    /* channel mapping */
    switch (channels)
    {
        case 1:
            aout->format.i_physical_channels = AOUT_CHAN_CENTER;
            break;
        case 2:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 3:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
            break;
        case 4:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 5:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 6:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;
        case 7:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT |
                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
            break;
        case 8:
            aout->format.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
            break;
        default:
            assert(0);
    }

    aout->format.i_format = VLC_CODEC_S16N;
    aout->format.i_rate = rate;
    aout->format.i_original_channels = aout->format.i_physical_channels;

    aout->pf_play = Play;
    aout->pf_pause = Pause;
    aout->pf_flush = Flush;
    if (sys->set_volume != NULL)
        aout->pf_volume_set = VolumeSet;
    else
        aout_VolumeSoftInit (aout);
    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    if (sys->cleanup != NULL)
        sys->cleanup (sys->opaque);
    free (sys);
}
