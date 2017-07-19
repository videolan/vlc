/*****************************************************************************
 * oss.c: Open Sound System audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2002 VLC authors and VideoLAN
 * Copyright (C) 2007-2012 Rémi Denis-Courmont
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef HAVE_SOUNDCARD_H
# include <soundcard.h>
#else
# include <sys/soundcard.h>
#endif

#ifndef SNDCTL_DSP_HALT
# define SNDCTL_DSP_HALT SNDCTL_DSP_RESET
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_cpu.h>
#include <vlc_aout.h>

#define A52_FRAME_NB 1536

struct aout_sys_t
{
    int fd;
    audio_sample_format_t format;
    bool starting;
    bool soft_mute;
    float soft_gain;
    char *device;
};

#include "volume.h"

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

#define AUDIO_DEV_TEXT N_("Audio output device")
#define AUDIO_DEV_LONGTEXT N_("OSS device node path.")

vlc_module_begin ()
    set_shortname( "OSS" )
    set_description (N_("Open Sound System audio output"))
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_string ("oss-audio-device", "",
                AUDIO_DEV_TEXT, AUDIO_DEV_LONGTEXT, false)
    add_sw_gain ()
    set_capability( "audio output", 100 )
    set_callbacks (Open, Close)
vlc_module_end ()

static int TimeGet (audio_output_t *, mtime_t *);
static void Play (audio_output_t *, block_t *);
static void Pause (audio_output_t *, bool, mtime_t);
static void Flush (audio_output_t *, bool);

static int Start (audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t* sys = aout->sys;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    /* Open the device */
    const char *device = sys->device;
    if (device == NULL)
        device = getenv ("OSS_AUDIODEV");
    if (device == NULL)
        device = "/dev/dsp";

    int fd = vlc_open (device, O_WRONLY);
    if (fd == -1)
    {
        msg_Err (aout, "cannot open OSS device %s: %s", device,
                 vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }
    msg_Dbg (aout, "using OSS device: %s", device);

    /* Select audio format */
    int format;
    bool spdif = false;

    switch (fmt->i_format)
    {
#ifdef AFMT_FLOAT
        case VLC_CODEC_FL64:
        case VLC_CODEC_FL32:
            format = AFMT_FLOAT;
            break;
#endif
        case VLC_CODEC_S32N:
            format = AFMT_S32_NE;
            break;
        case VLC_CODEC_S16N:
            format = AFMT_S16_NE;
            break;
        case VLC_CODEC_U8:
            format = AFMT_U8;
            break;
        default:
            if (AOUT_FMT_SPDIF(fmt))
                spdif = var_InheritBool (aout, "spdif");
            if (spdif)
                format = AFMT_AC3;
#ifdef AFMT_FLOAT
            else if (HAVE_FPU)
                format = AFMT_FLOAT;
#endif
            else
                format = AFMT_S16_NE;
    }

    if (ioctl (fd, SNDCTL_DSP_SETFMT, &format) < 0)
    {
        msg_Err (aout, "cannot set audio format 0x%X: %s", format,
                 vlc_strerror_c(errno));
        goto error;
    }

    switch (format)
    {
        case AFMT_U8:     fmt->i_format = VLC_CODEC_U8;   break;
        case AFMT_S16_NE: fmt->i_format = VLC_CODEC_S16N; break;
        case AFMT_S32_NE: fmt->i_format = VLC_CODEC_S32N; break;
#ifdef AFMT_FLOAT
        case AFMT_FLOAT:  fmt->i_format = VLC_CODEC_FL32; break;
#endif
        case AFMT_AC3:
            if (spdif)
            {
                fmt->i_format = VLC_CODEC_SPDIFL;
                break;
            }
        default:
            msg_Err (aout, "unsupported audio format 0x%X", format);
            goto error;
    }

    /* Select channels count */
    int channels = spdif ? 2 : aout_FormatNbChannels (fmt);
    if (ioctl (fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
    {
        msg_Err (aout, "cannot set %d channels: %s", channels,
                 vlc_strerror_c(errno));
        goto error;
    }

    switch (channels)
    {
        case 1: channels = AOUT_CHAN_CENTER;  break;
        case 2: channels = AOUT_CHANS_STEREO; break;
        case 4: channels = AOUT_CHANS_4_0;    break;
        case 6: channels = AOUT_CHANS_5_1;    break;
        case 8: channels = AOUT_CHANS_7_1;    break;
        default:
            msg_Err (aout, "unsupported channels count %d", channels);
            goto error;
    }

    /* Select sample rate */
    int rate = spdif ? 48000 : fmt->i_rate;
    if (ioctl (fd, SNDCTL_DSP_SPEED, &rate) < 0)
    {
        msg_Err (aout, "cannot set %d Hz sample rate: %s", rate,
                 vlc_strerror_c(errno));
        goto error;
    }

    /* Setup audio_output_t */
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;

    if (spdif)
    {
        fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE;
        fmt->i_frame_length = A52_FRAME_NB;
    }
    else
    {
        fmt->i_rate = rate;
        fmt->i_physical_channels = channels;
    }
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    aout_FormatPrepare (fmt);

    /* Select timing */
    unsigned bytes;
    if (spdif)
        bytes = AOUT_SPDIF_SIZE;
    else
        bytes = fmt->i_rate / (CLOCK_FREQ / AOUT_MIN_PREPARE_TIME)
                * fmt->i_bytes_per_frame;
    if (unlikely(bytes < 16))
        bytes = 16;

    int frag = (AOUT_MAX_ADVANCE_TIME / AOUT_MIN_PREPARE_TIME) << 16
             | (32 - clz32(bytes - 1));
    if (ioctl (fd, SNDCTL_DSP_SETFRAGMENT, &frag) < 0)
        msg_Err (aout, "cannot set 0x%08x fragment: %s", frag,
                 vlc_strerror_c(errno));

    sys->fd = fd;
    aout_SoftVolumeStart (aout);
    sys->starting = true;
    sys->format = *fmt;
    return VLC_SUCCESS;
error:
    vlc_close (fd);
    return VLC_EGENERIC;
}

static int TimeGet (audio_output_t *aout, mtime_t *restrict pts)
{
    aout_sys_t *sys = aout->sys;
    int delay;

    if (ioctl (sys->fd, SNDCTL_DSP_GETODELAY, &delay) < 0)
    {
        msg_Warn (aout, "cannot get delay: %s", vlc_strerror_c(errno));
        return -1;
    }

    *pts = (delay * CLOCK_FREQ * sys->format.i_frame_length)
                        / (sys->format.i_rate * sys->format.i_bytes_per_frame);
    return 0;
}

/**
 * Queues one audio buffer to the hardware.
 */
static void Play (audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    while (block->i_buffer > 0)
    {
        ssize_t bytes = write (fd, block->p_buffer, block->i_buffer);
        if (bytes >= 0)
        {
            block->p_buffer += bytes;
            block->i_buffer -= bytes;
        }
        else
            msg_Err (aout, "cannot write samples: %s", vlc_strerror_c(errno));
    }
    block_Release (block);
}

/**
 * Pauses/resumes the audio playback.
 */
static void Pause (audio_output_t *aout, bool pause, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    (void) date;
    ioctl (fd, pause ? SNDCTL_DSP_SILENCE : SNDCTL_DSP_SKIP, NULL);
}

/**
 * Flushes/drains the audio playback buffer.
 */
static void Flush (audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    if (wait)
        return; /* drain is implicit with OSS */
    ioctl (fd, SNDCTL_DSP_HALT, NULL);
}

/**
 * Releases the audio output device.
 */
static void Stop (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    ioctl (fd, SNDCTL_DSP_HALT, NULL);
    vlc_close (fd);
    sys->fd = -1;
}

static int DevicesEnum (audio_output_t *aout)
{
    int fd = vlc_open ("/dev/dsp", O_WRONLY);
    if (fd == -1)
        return -1;

    oss_sysinfo si;
    int n = -1;

    if (ioctl (fd, SNDCTL_SYSINFO, &si) < 0)
    {
        msg_Err (aout, "cannot get system infos: %s", vlc_strerror(errno));
        goto out;
    }

    msg_Dbg (aout, "using %s version %s (0x%06X) under %s", si.product,
             si.version, si.versionnum, si.license);

    for (int i = 0; i < si.numaudios; i++)
    {
        oss_audioinfo ai = { .dev = i };

        if (ioctl (fd, SNDCTL_AUDIOINFO, &ai) < 0)
        {
            msg_Warn (aout, "cannot get device %d infos: %s", i,
                      vlc_strerror_c(errno));
            continue;
        }
        if (ai.caps & (PCM_CAP_HIDDEN|PCM_CAP_MODEM))
            continue;
        if (!(ai.caps & PCM_CAP_OUTPUT))
            continue;
        if (!ai.enabled)
            continue;

        aout_HotplugReport (aout, ai.devnode, ai.name);
        n++;
    }
out:
    vlc_close (fd);
    return n;
}

static int DeviceSelect (audio_output_t *aout, const char *id)
{
    aout_sys_t *sys = aout->sys;
    char *path = NULL;

    if (id != NULL)
    {
        path = strdup (id);
        if (unlikely(path == NULL))
            return -1;
    }

    free (sys->device);
    sys->device = path;
    aout_DeviceReport (aout, path);
    aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    return 0;
}

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout_sys_t *sys = malloc (sizeof (*sys));
    if(unlikely( sys == NULL ))
        return VLC_ENOMEM;

    sys->fd = -1;
    sys->device = var_InheritString (aout, "oss-audio-device");

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->device_select = DeviceSelect;
    aout_DeviceReport (aout, sys->device);
    aout_SoftVolumeInit (aout);

    DevicesEnum (aout);
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free (sys->device);
    free (sys);
}
