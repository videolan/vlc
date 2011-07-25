/*****************************************************************************
 * amem.c : virtual LibVLC audio output plugin
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

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
    int (*set_volume) (void *opaque, float vol, bool mute);
    void (*cleanup) (void *opaque);
};

static void Play (aout_instance_t *aout)
{
    aout_sys_t *sys = aout->sys;
    block_t *block;

    while ((block = aout_FifoPop(&aout->fifo)) != NULL)
    {
        sys->play (sys->opaque, block->p_buffer, block->i_nb_samples,
                   block->i_pts);
        block_Release (block);
    }
}

static int VolumeSet (aout_instance_t *aout, float vol, bool mute)
{
    aout_sys_t *sys = aout->sys;

    return sys->set_volume (sys->opaque, vol, mute) ? -1 : 0;
}

typedef int (*vlc_audio_format_cb) (void **, char *, unsigned *, unsigned *);

static int Open (vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    sys->opaque = var_InheritAddress (obj, "amem-data");
    sys->play = var_InheritAddress (obj, "amem-play");
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
    /* FIXME/TODO channel mapping */
    if (strcmp(format, "S16N") || aout->format.i_channels != channels)
    {
        msg_Err (aout, "format not supported");
        goto error;
    }
    aout->format.i_format = VLC_CODEC_S16N;
    aout->format.i_rate = rate;

    aout->pf_play = Play;
    aout->pf_pause = NULL;
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
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = aout->sys;

    if (sys->cleanup != NULL)
        sys->cleanup (sys->opaque);
    free (sys);
}
