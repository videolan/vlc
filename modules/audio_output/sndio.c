/*****************************************************************************
 * sndio.c : sndio plugin for VLC
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
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
    set_callbacks (Open, Close )
vlc_module_end ()

static void Play  (audio_output_t *, block_t *);
static void Pause (audio_output_t *, bool, mtime_t);

/** Initializes an sndio playback stream */
static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    struct sio_hdl *sio = sio_open (NULL, SIO_PLAY, 0 /* blocking */);
    if (sio == NULL)
    {
        msg_Err (obj, "cannot create audio playback stream");
        return VLC_EGENERIC;
    }

    struct sio_par par;
    sio_initpar (&par);
    par.bits = 16;
    par.bps = par.bits >> 3;
    par.sig = 1;
    par.le = SIO_LE_NATIVE;
    par.pchan = aout_FormatNbChannels (&aout->format);
    par.rate = aout->format.i_rate;
    par.xrun = SIO_SYNC;

    if (!sio_setpar (sio, &par) || !sio_getpar (sio, &par))
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

    audio_format_t f;

    switch (par.bps)
    {
        case 8:
            f.i_format = par.sig ? VLC_CODEC_S8 : VLC_CODEC_U8;
            break;
        case 16:
            f.i_format = par.sig ? (par.le ? VLC_CODEC_S16L : VLC_CODEC_S16B)
                                 : (par.le ? VLC_CODEC_U16L : VLC_CODEC_U16B);
            break;
        case 24:
            f.i_format = par.sig ? (par.le ? VLC_CODEC_S24L : VLC_CODEC_S24B)
                                 : (par.le ? VLC_CODEC_U24L : VLC_CODEC_U24B);
            break;
        case 32:
            f.i_format = par.sig ? (par.le ? VLC_CODEC_S32L : VLC_CODEC_S32B)
                                 : (par.le ? VLC_CODEC_U32L : VLC_CODEC_U32B);
            break;
        default:
            msg_Err (obj, "unsupported audio sample format (%u bits)",
                     par.bits);
            goto error;
    }

    f.i_rate = par.rate;

    /* Channel map */
    unsigned chans;
    switch (par.pchan)
    {
        case 1:
            chans = AOUT_CHAN_CENTER;
            break;
        case 2:
            chans = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 4:
            chans = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                  | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 6:
            chans = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                  | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                  | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
            break;
        default:
            msg_Err (aout, "unknown %u channels map", par.pchan);
            goto error;
    }

    f.i_original_channels = f.i_physical_channels = chans;
    aout_FormatPrepare (&f);

    aout->format = f;
    aout->sys = (void *)sio;
    aout->pf_play = Play;
    aout->pf_pause = Pause;
    aout->pf_flush  = NULL; /* sndio sucks! */
    aout_VolumeSoftInit (aout); /* TODO: sio_onvol() */

    sio_start (sio);
    return VLC_SUCCESS;

error:
    sio_close (sio);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    struct sio_hdl *sio = (void *)aout->sys;

    sio_close (sio);
}

static void Play (audio_output_t *aout, block_t *block)
{
    struct sio_hdl *sio = (void *)aout->sys;
    struct sio_par par;

    if (sio_getpar (sio, &par) == 0)
    {
        mtime_t delay = par.bufsz * CLOCK_FREQ / aout->format.i_rate;

        delay = block->i_pts - (mdate () - delay);
        if (delay > 0)
        {
            size_t frames = (delay * aout->format.i_rate) / CLOCK_FREQ;
            msg_Dbg (aout, "prepending %zu zeroes", frames);

            void *pad = calloc (frames, aout->format.i_bytes_per_frame);
            if (likely(pad != NULL))
            {
                sio_write (sio, pad, frames * aout->format.i_bytes_per_frame);
                free (pad);
            }
        }
        else
            aout_TimeReport (aout, block->i_pts - delay);
    }

    while (block->i_buffer > 0 && !sio_eof (sio))
    {
        size_t bytes = sio_write (sio, block->p_buffer, block->i_buffer);

        block->p_buffer += bytes;
        block->i_buffer -= bytes;
        /* Note that i_nb_samples and i_pts are corrupted here. */
    }
    block_Release (block);
}

static void Pause (audio_output_t *aout, bool pause, mtime_t date)
{
    struct sio_hdl *sio = (void *)aout->sys;

    if (pause)
        sio_stop (sio);
    else
        sio_start (sio);
    (void) date;
}
