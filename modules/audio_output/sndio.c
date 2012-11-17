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
static void Close (vlc_objec_t *);

vlc_module_begin ()
    set_shortname ("sndio")
    set_description (N_("OpenBSD sndio audio output"))
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AOUT)
    set_capability ("audio output", 120)
    set_callbacks (Open, Close)
vlc_module_end ()

static int TimeGet (audio_output, mtime_t *);
static void Play (audio_output_t *, block_t *);
static void Flush (audio_output_t *, bool);
static int VolumeSet (audio_output_t *, float);
static int MuteSet (audio_output_t *, bool);
static void VolumeChanged (void *, unsigned);
static void PositionChanged (void *, int);

struct aout_sys_t
{
    struct sio_hdl *hdl;
    unsigned long long read_offset;
    unsigned long long write_offset;
    unsigned rate;
    unsigned volume;
    bool mute;
};

/** Initializes an sndio playback stream */
static int Start (audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    sys->hdl = sio_open (NULL, SIO_PLAY, 0 /* blocking */);
    if (sys->hdl == NULL)
    {
        msg_Err (obj, "cannot create audio playback stream");
        free (sys);
        return VLC_EGENERIC;
    }
    aout->sys = sys;

    struct sio_par par;
    sio_initpar (&par);
    par.bits = 16;
    par.bps = par.bits >> 3;
    par.sig = 1;
    par.le = SIO_LE_NATIVE;
    par.pchan = aout_FormatNbChannels (fmt);
    par.rate = fmt->i_rate;
    par.xrun = SIO_SYNC;

    if (!sio_setpar (sys->hdl, &par) || !sio_getpar (sys->hdl, &par))
    {
        msg_Err (obj, "cannot negotiate audio playback parameters");
        goto error;
    }

    if (par.bps != par.bits >> 3)
    {
        msg_Err (obj, "unsupported audio sample format (%u bits in %u bytes)",
                 par.bits, par.bps);
        goto error;
    }

    switch (par.bits)
    {
        case 8:
            fmt->i_format = par.sig ? VLC_CODEC_S8 : VLC_CODEC_U8;
            break;
        case 16:
            fmt->i_format = par.sig
                                 ? (par.le ? VLC_CODEC_S16L : VLC_CODEC_S16B)
                                 : (par.le ? VLC_CODEC_U16L : VLC_CODEC_U16B);
            break;
        case 24:
            fmt->i_format = par.sig
                                 ? (par.le ? VLC_CODEC_S24L : VLC_CODEC_S24B)
                                 : (par.le ? VLC_CODEC_U24L : VLC_CODEC_U24B);
            break;
        case 32:
            fmt->i_format = par.sig
                                 ? (par.le ? VLC_CODEC_S32L : VLC_CODEC_S32B)
                                 : (par.le ? VLC_CODEC_U32L : VLC_CODEC_U32B);
            break;
        default:
            msg_Err (obj, "unsupported audio sample format (%u bits)",
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

    fmt->i_original_channels = fmt->i_physical_channels = chans;
    aout_FormatPrepare (fmt);

    aout->sys = sys;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = NULL;
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

    sys->read_offset = 0;
    sys->write_offset = 0;
    sio_onmove (sys->hdl, PositionChanged, aout);
    sio_start (sys->hdl);
    return VLC_SUCCESS;

error:
    sio_close (sys->hdl);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    sio_close (sys->hdl);
}

static void PositionChanged (void *arg, int delta)
{
    audio_output_t *aout = arg;
    aout_sys_t *sys = aout->sys;

    sys->read_offset += delta;
}

static int TimeGet (audio_output_t *aout, mtime_t *restrict pts)
{
    aout_sys_t *sys = aout->sys;
    long long frames = sys->write_offset - sys->read_offset;

    if (frames == 0)
        return -1;

    *pts = mdate () + (frames * CLOCK_FREQ / sys->rate);
    return 0;
}

static void Play (audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;

    sys->write_offset += block->i_nb_samples;

    while (block->i_buffer > 0 && !sio_eof (sys->hdl))
    {
        size_t bytes = sio_write (sys->hdl, block->p_buffer, block->i_buffer);

        block->p_buffer += bytes;
        block->i_buffer -= bytes;
        /* Note that i_nb_samples and i_pts are not updated here. */
    }
    block_Release (block);
}

static void Flush (audio_output_t *aout, bool wait)
{
    if (wait)
    {
        long long frames = sys->write_offset - sys->read_offset;

        if (frames > 0)
            msleep (frames * CLOCK_FREQ / sys->rate);
    }
    else
    {
        sio_stop (sys->hdl);
        sys->read_offset = 0;
        sys->write_offset = 0;
        sio_start (sys->hdl);
    }
}

static void VolumeChanged (void *arg, unsigned volume)
{
    audio_output_t *aout = arg;
    float fvol = (float)volume / (float)SIO_MAXVOL;

    aout_VolumeReport (aout, fvol);
    aout_MuteReport (aout, volume == 0);
    if (volume) /* remember last non-zero volume to unmute later */
        aout->sys->volume = volume;
}

static int VolumeSet (audio_output_t *aout, float fvol)
{
    aout_sys_t *sys = aout->sys;
    unsigned volume = lroundf (fvol * SIO_MAXVOL);

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

static int Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free (sys);
}

