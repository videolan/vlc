/*****************************************************************************
 * oss.c: Open Sound System audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2002 the VideoLAN team
 * Copyright (C) 2007-2012 Rémi Denis-Courmont
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <math.h>
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
    uint8_t level;
    bool mute;
    bool starting;
};

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
    set_capability( "audio output", 100 )
    set_callbacks( Open, Close )
vlc_module_end ()

static void Play (audio_output_t *, block_t *, mtime_t *);
static void Pause (audio_output_t *, bool, mtime_t);
static void Flush (audio_output_t *, bool);
static int VolumeSync (audio_output_t *);
static int VolumeSet (audio_output_t *, float);
static int MuteSet (audio_output_t *, bool);

static int DeviceChanged (vlc_object_t *obj, const char *varname,
                          vlc_value_t prev, vlc_value_t cur, void *data)
{
    aout_ChannelsRestart (obj, varname, prev, cur, data);

    if (!var_Type (obj, "oss-audio-device"))
        var_Create (obj, "oss-audio-device", VLC_VAR_STRING);
    var_SetString (obj, "oss-audio-device", cur.psz_string);
    return VLC_SUCCESS;
}

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    /* Open the device */
    const char *device;
    char *devicebuf = var_InheritString (aout, "oss-audio-device");
    device = devicebuf;
    if (device == NULL)
        device = getenv ("OSS_AUDIODEV");
    if (device == NULL)
        device = "/dev/dsp";

    msg_Dbg (aout, "using OSS device: %s", device);

    int fd = vlc_open (device, O_WRONLY);
    if (fd == -1)
    {
        msg_Err (aout, "cannot open OSS device %s: %m", device);
        free (devicebuf);
        return VLC_EGENERIC;
    }

    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;
    aout->sys = sys;
    sys->fd = fd;

    /* Select audio format */
    int format;
    vlc_fourcc_t fourcc = aout->format.i_format;
    bool spdif = false;

    switch (fourcc)
    {
#ifdef AFMT_FLOAT
        case VLC_CODEC_F64B:
        case VLC_CODEC_F64L:
        case VLC_CODEC_F32B:
        case VLC_CODEC_F32L:
            format = AFMT_FLOAT;
            break;
#endif
        case VLC_CODEC_S32B:
            format = AFMT_S32_BE;
            break;
        case VLC_CODEC_S32L:
            format = AFMT_S32_LE;
            break;
        case VLC_CODEC_S16B:
            format = AFMT_S16_BE;
            break;
        case VLC_CODEC_S16L:
            format = AFMT_S16_LE;
            break;
        case VLC_CODEC_S8:
        case VLC_CODEC_U8:
            format = AFMT_U8;
            break;
        default:
            if (AOUT_FMT_SPDIF(&aout->format))
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
        msg_Err (aout, "cannot set audio format 0x%X: %m", format);
        goto error;
    }

    switch (format)
    {
        case AFMT_S8:     fourcc = VLC_CODEC_S8;   break;
        case AFMT_U8:     fourcc = VLC_CODEC_U8;   break;
        case AFMT_S16_BE: fourcc = VLC_CODEC_S16B; break;
        case AFMT_S16_LE: fourcc = VLC_CODEC_S16L; break;
        //case AFMT_S24_BE:
        //case AFMT_S24_LE:
        case AFMT_S32_BE: fourcc = VLC_CODEC_S32B; break;
        case AFMT_S32_LE: fourcc = VLC_CODEC_S32L; break;
#ifdef AFMT_FLOAT
        case AFMT_FLOAT:  fourcc = VLC_CODEC_FL32; break;
#endif
        case AFMT_AC3:
            if (spdif)
            {
                fourcc = VLC_CODEC_SPDIFL;
                break;
            }
        default:
            msg_Err (aout, "unsupported audio format 0x%X", format);
            goto error;
    }

    /* Select channels count */
    int channels = spdif ? 2 : aout_FormatNbChannels (&aout->format);
    if (ioctl (fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
    {
        msg_Err (aout, "cannot set %d channels: %m", channels);
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
    int rate = spdif ? 48000 : aout->format.i_rate;
    if (ioctl (fd, SNDCTL_DSP_SPEED, &rate) < 0)
    {
        msg_Err (aout, "cannot set %d Hz sample rate: %m", rate);
        goto error;
    }

    /* Setup audio_output_t */
    aout->format.i_format = fourcc;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = NULL;
    aout->mute_set = NULL;

    if (spdif)
    {
        aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        aout->format.i_frame_length = A52_FRAME_NB;
    }
    else
    {
        aout->format.i_rate = rate;
        aout->format.i_original_channels =
        aout->format.i_physical_channels = channels;

        sys->level = 100;
        sys->mute = false;
        if (VolumeSync (aout) == 0)
        {
            aout->volume_set = VolumeSet;
            aout->mute_set = MuteSet;
        }
    }
    sys->starting = true;

    /* Build the devices list */
    var_Create (aout, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    var_SetString (aout, "audio-device", device);
    var_AddCallback (aout, "audio-device", DeviceChanged, NULL);

    oss_sysinfo si;
    if (ioctl (fd, SNDCTL_SYSINFO, &si) >= 0)
    {
        vlc_value_t val, text;

        text.psz_string = _("Audio Device");
        var_Change (aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL);

        msg_Dbg (aout, "using %s version %s (0x%06X) under %s", si.product,
                 si.version, si.versionnum, si.license);

        for (int i = 0; i < si.numaudios; i++)
        {
            oss_audioinfo ai = { .dev = i };

            if (ioctl (fd, SNDCTL_AUDIOINFO, &ai) < 0)
            {
                msg_Warn (aout, "cannot get device %d infos: %m", i);
                continue;
            }
            if (ai.caps & (PCM_CAP_HIDDEN|PCM_CAP_MODEM))
                continue;
            if (!(ai.caps & PCM_CAP_OUTPUT))
                continue;
            if (!ai.enabled)
                continue;

            val.psz_string = ai.devnode;
            text.psz_string = ai.name;
            var_Change (aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
        }
    }

    free (devicebuf);
    return 0;
error:
    free (sys);
    close (fd);
    free (devicebuf);
    return VLC_EGENERIC;
}

/**
 * Releases the audio output.
 */
static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    var_DelCallback (obj, "audio-device", DeviceChanged, NULL);
    var_Destroy (obj, "audio-device");

    ioctl (fd, SNDCTL_DSP_HALT, NULL);
    close (fd);
    free (sys);
}

/**
 * Queues one audio buffer to the hardware.
 */
static void Play (audio_output_t *aout, block_t *block,
                  mtime_t *restrict drift)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    int delay;
    if (ioctl (sys->fd, SNDCTL_DSP_GETODELAY, &delay) >= 0)
    {
        mtime_t latency = (delay * CLOCK_FREQ * aout->format.i_frame_length)
                      / (aout->format.i_rate * aout->format.i_bytes_per_frame);
        *drift = mdate () + latency - block->i_pts;
    }
    else
        msg_Warn (aout, "cannot get delay: %m");

    if (sys->starting)
    {   /* Start on time */
        /* TODO: resync on pause resumption and underflow recovery */
        mtime_t delta = -*drift;
        if (delta > 0) {
            msg_Dbg(aout, "deferring start (%"PRId64" us)", delta);
            msleep(delta);
            *drift = 0;
        } else
            msg_Warn(aout, "starting late (%"PRId64" us)", delta);
        sys->starting = false;
    }

    while (block->i_buffer > 0)
    {
        ssize_t bytes = write (fd, block->p_buffer, block->i_buffer);
        if (bytes >= 0)
        {
            block->p_buffer += bytes;
            block->i_buffer -= bytes;
        }
        else
            msg_Err (aout, "cannot write samples: %m");
    }
    block_Release (block);

    /* Dumb OSS cannot send any kind of events for this... */
    VolumeSync (aout);
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
    ioctl (fd, SNDCTL_DSP_HALT_OUTPUT, NULL);
}

static int VolumeSync (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    int level;
    if (ioctl (fd, SNDCTL_DSP_GETPLAYVOL, &level) < 0)
        return -1;

    sys->mute = !level;
    if (level) /* try to keep last volume before mute */
        sys->level = level;
    aout_MuteReport (aout, !level);
    aout_VolumeReport (aout, (float)(level & 0xFF) / 100.f);
    return 0;
}

static int VolumeSet (audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    int level = lroundf (vol * 100.f);
    if (level > 0xFF)
        level = 0xFFFF;
    else
        level |= level << 8;
    if (!sys->mute && ioctl (fd, SNDCTL_DSP_SETPLAYVOL, &level) < 0)
    {
        msg_Err (aout, "cannot set volume: %m");
        return -1;
    }

    sys->level = level;
    aout_VolumeReport (aout, (float)(level & 0xFF) / 100.f);
    return 0;
}

static int MuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    int fd = sys->fd;

    int level = mute ? 0 : (sys->level | (sys->level << 8));
    if (ioctl (fd, SNDCTL_DSP_SETPLAYVOL, &level) < 0)
    {
        msg_Err (aout, "cannot mute: %m");
        return -1;
    }

    sys->mute = mute;
    aout_MuteReport (aout, mute);
    return 0;
}
