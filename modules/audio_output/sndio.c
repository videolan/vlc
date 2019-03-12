/*****************************************************************************
 * sndio.c : sndio plugin for VLC
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <math.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <sndio.h>

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname ("sndio")
    set_description (N_("OpenBSD sndio audio output"))
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AOUT)
    set_capability ("audio output", 120)
    set_callbacks (Open, Close)
vlc_module_end ()

static int TimeGet (audio_output_t *, vlc_tick_t *);
static void Play(audio_output_t *, block_t *, vlc_tick_t);
static void Flush (audio_output_t *);
static int VolumeSet (audio_output_t *, float);
static int MuteSet (audio_output_t *, bool);
static void VolumeChanged (void *, unsigned);
static void PositionChanged (void *, int);

typedef struct
{
    struct sio_hdl *hdl;
    int started;
    int delay;
    unsigned rate;
    unsigned volume;
    bool mute;
} aout_sys_t;

/** Initializes an sndio playback stream */
static int Start (audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    sys->hdl = sio_open (NULL, SIO_PLAY, 0 /* blocking */);
    if (sys->hdl == NULL)
    {
        msg_Err (aout, "cannot create audio playback stream");
        return VLC_EGENERIC;
    }

    struct sio_par par;
    sio_initpar (&par);
    switch (fmt->i_format) {
    case VLC_CODEC_U8:
        par.bits = 8;
        par.sig = 0;
        break;
    case VLC_CODEC_S16N:
        par.bits = 16;
        par.sig = 1;
        par.le = SIO_LE_NATIVE;
        break;
    case VLC_CODEC_S32N:
    case VLC_CODEC_FL32:
    case VLC_CODEC_FL64:
        par.bits = 32;
        par.sig = 1;
        par.le = SIO_LE_NATIVE;
        break;
    default:
        /* use a common audio format */
        par.bits = 16;
        par.sig = 1;
        par.le = SIO_LE_NATIVE;
    }
    par.pchan = aout_FormatNbChannels (fmt);
    par.rate = fmt->i_rate;
    par.round = par.rate / 50;
    par.appbufsz = par.rate / 4;

    if (!sio_setpar (sys->hdl, &par) || !sio_getpar (sys->hdl, &par))
    {
        msg_Err (aout, "cannot negotiate audio playback parameters");
        goto error;
    }

    if (par.bps != par.bits >> 3 && !par.msb)
    {
        msg_Err (aout, "unsupported audio sample format (%u bits in %u bytes)",
                 par.bits, par.bps);
        goto error;
    }
    if (par.sig != (par.bits != 8))
    {
        msg_Err (aout, "unsupported audio sample format (%ssigned)",
                 par.sig ? "" : "un");
        goto error;
    }
    if (par.bps > 1 && par.le != SIO_LE_NATIVE)
    {
        msg_Err (aout, "unsupported audio sample format (%s endian)",
                 par.le ? "little" : "big");
        goto error;
    }
    switch (par.bits)
    {
        case 8:
            fmt->i_format = VLC_CODEC_U8;
            break;
        case 16:
            fmt->i_format = VLC_CODEC_S16N;
            break;
        case 32:
            fmt->i_format = VLC_CODEC_S32N;
            break;
        default:
            msg_Err (aout, "unsupported audio sample format (%u bits)",
                     par.bits);
            goto error;
    }

    fmt->i_rate = par.rate;
    sys->rate = par.rate;

    /* Channel map */
    unsigned chans;
    switch (par.pchan)
    {
        case 1:
            chans = AOUT_CHAN_CENTER;
            break;
        case 2:
            chans = AOUT_CHANS_STEREO;
            break;
        case 4:
            chans = AOUT_CHANS_4_0;
            break;
        case 6:
            chans = AOUT_CHANS_5_1;
            break;
        case 8:
            chans = AOUT_CHANS_7_1;
            break;
        default:
            msg_Err (aout, "unknown %u channels map", par.pchan);
            goto error;
    }

    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    fmt->i_physical_channels = chans;
    aout_FormatPrepare (fmt);

    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = aout_PauseDefault;
    aout->flush = Flush;
    if (sio_onvol(sys->hdl, VolumeChanged, aout))
    {
        aout->volume_set = VolumeSet;
        aout->mute_set = MuteSet;
    }
    else
    {
        aout->volume_set = NULL;
        aout->mute_set = NULL;
    }

    sys->started = 0;
    sys->delay = 0;
    sio_onmove (sys->hdl, PositionChanged, aout);
    sio_start (sys->hdl);
    return VLC_SUCCESS;

error:
    sio_close (sys->hdl);
    return VLC_EGENERIC;
}

static void Stop (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    sio_close (sys->hdl);
}

static void PositionChanged (void *arg, int delta)
{
    audio_output_t *aout = arg;
    aout_sys_t *sys = aout->sys;

    sys->delay -= delta;
    sys->started = 1;
}

static int TimeGet (audio_output_t *aout, vlc_tick_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;

    if (!sys->started)
        return -1;
    *delay = vlc_tick_from_samples(sys->delay, sys->rate);
    return 0;
}

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    sio_write (sys->hdl, block->p_buffer, block->i_buffer);
    sys->delay += block->i_nb_samples;
    block_Release (block);
    (void) date;
}

static void Flush (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    sio_stop (sys->hdl);
    sys->started = 0;
    sys->delay = 0;
    sio_start (sys->hdl);
}

static void VolumeChanged (void *arg, unsigned volume)
{
    audio_output_t *aout = arg;
    aout_sys_t *p_sys = aout->sys;
    float fvol = (float)volume / (float)SIO_MAXVOL;

    aout_VolumeReport (aout, fvol);
    aout_MuteReport (aout, volume == 0);
    if (volume) /* remember last non-zero volume to unmute later */
        p_sys->volume = volume;
}

static int VolumeSet (audio_output_t *aout, float fvol)
{
    aout_sys_t *sys = aout->sys;
    unsigned volume;

    if (fvol < 0)
        fvol = 0;
    if (fvol > 1)
        fvol = 1;
    volume = lroundf (fvol * SIO_MAXVOL);
    if (!sys->mute && !sio_setvol (sys->hdl, volume))
        return -1;
    sys->volume = volume;
    return 0;
}

static int MuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    if (!sio_setvol (sys->hdl, mute ? 0 : sys->volume))
        return -1;

    sys->mute = mute;
    return 0;
}

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    /* FIXME: set volume/mute here */
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free (sys);
}

