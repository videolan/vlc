/*****************************************************************************
 * alsa.c: ALSA capture module for VLC
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
# include <config.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <poll.h>
#include <alsa/asoundlib.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>

#define HELP_TEXT N_( \
    "Pass alsa:// to open the default ALSA capture device, " \
    "or alsa://SOURCE to open a specific device named SOURCE.")
#define STEREO_TEXT N_("Stereo")
#define RATE_TEXT N_("Sample rate")

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

static const int rate_values[] = { 192000, 176400,
    96000, 88200, 48000, 44100,
    32000, 22050, 24000, 16000,
    11025, 8000, 4000
};
static const const char *rate_names[] = { N_("192000 Hz"), N_("176400 Hz"),
    N_("96000 Hz"), N_("88200 Hz"), N_("48000 Hz"), N_("44100 Hz"),
    N_("32000 Hz"), N_("22050 Hz"), N_("24000 Hz"), N_("16000 Hz"),
    N_("11025 Hz"), N_("8000 Hz"), N_("4000 Hz")
};

vlc_module_begin ()
    set_shortname (N_("ALSA"))
    set_description (N_("ALSA audio capture"))
    set_capability ("access_demux", 0)
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_help (HELP_TEXT)

    add_obsolete_string ("alsa-format") /* since 2.1.0 */
    add_bool ("alsa-stereo", true, STEREO_TEXT, STEREO_TEXT, true)
    add_integer ("alsa-samplerate", 48000, RATE_TEXT, RATE_TEXT, true)
        change_integer_list (rate_values, rate_names)

    add_shortcut ("alsa")
    set_callbacks (Open, Close)
vlc_module_end ()

/** Helper for ALSA -> VLC debugging output */
/** XXX: duplicated from ALSA output */
static void Dump (vlc_object_t *obj, const char *msg,
                  int (*cb)(void *, snd_output_t *), void *p)
{
    snd_output_t *output;
    char *str;

    if (unlikely(snd_output_buffer_open (&output)))
        return;

    int val = cb (p, output);
    if (val)
    {
        msg_Warn (obj, "cannot get info: %s", snd_strerror (val));
        return;
    }

    size_t len = snd_output_buffer_string (output, &str);
    if (len > 0 && str[len - 1])
        len--; /* strip trailing newline */
    msg_Dbg (obj, "%s%.*s", msg, (int)len, str);
    snd_output_close (output);
}
#define Dump(o, m, cb, p) \
        Dump(VLC_OBJECT(o), m, (int (*)(void *, snd_output_t *))(cb), p)

static void DumpDevice (vlc_object_t *obj, snd_pcm_t *pcm)
{
    snd_pcm_info_t *info;

    Dump (obj, " ", snd_pcm_dump, pcm);
    snd_pcm_info_alloca (&info);
    if (snd_pcm_info (pcm, info) == 0)
    {
        msg_Dbg (obj, " device name   : %s", snd_pcm_info_get_name (info));
        msg_Dbg (obj, " device ID     : %s", snd_pcm_info_get_id (info));
        msg_Dbg (obj, " subdevice name: %s",
                snd_pcm_info_get_subdevice_name (info));
    }
}

static void DumpDeviceStatus (vlc_object_t *obj, snd_pcm_t *pcm)
{
    snd_pcm_status_t *status;

    snd_pcm_status_alloca (&status);
    snd_pcm_status (pcm, status);
    Dump (obj, "current status:\n", snd_pcm_status_dump, status);
}
#define DumpDeviceStatus(o, p) DumpDeviceStatus(VLC_OBJECT(o), p)


struct demux_sys_t
{
    snd_pcm_t *pcm;
    es_out_id_t *es;
    vlc_thread_t thread;

    mtime_t start;
    mtime_t caching;
    snd_pcm_uframes_t period_size;
    unsigned rate;
};

static void Poll (snd_pcm_t *pcm, int canc)
{
    int n = snd_pcm_poll_descriptors_count (pcm);
    struct pollfd ufd[n];
    unsigned short revents;

    snd_pcm_poll_descriptors (pcm, ufd, n);
    do
    {
        vlc_restorecancel (canc);
        poll (ufd, n, -1);
        canc = vlc_savecancel ();
        snd_pcm_poll_descriptors_revents (pcm, ufd, n, &revents);
    }
    while (!revents);
}

static void *Thread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    snd_pcm_t *pcm = sys->pcm;
    size_t bytes;
    int canc, val;

    canc = vlc_savecancel ();
    bytes = snd_pcm_frames_to_bytes (pcm, sys->period_size);
    val = snd_pcm_start (pcm);
    if (val)
    {
        msg_Err (demux, "cannot prepare device: %s", snd_strerror (val));
        return NULL;
    }

    for (;;)
    {
        block_t *block = block_Alloc (bytes);
        if (unlikely(block == NULL))
            break;

        /* Wait for data */
        Poll (pcm, canc);

        /* Read data */
        snd_pcm_sframes_t frames, delay;
        mtime_t pts;

        frames = snd_pcm_readi (pcm, block->p_buffer, sys->period_size);
        pts = mdate ();
        if (frames < 0)
        {
            if (frames == -EAGAIN)
                continue;

            val = snd_pcm_recover (pcm, frames, 1);
            if (val == 0)
            {
                msg_Warn (demux, "cannot read samples: %s",
                          snd_strerror (frames));
                continue;
            }
            msg_Err (demux, "cannot recover record stream: %s",
                     snd_strerror (val));
            DumpDeviceStatus (demux, pcm);
            break;
        }

        /* Compute time stamp */
        if (snd_pcm_delay (pcm, &delay))
            delay = 0;
        delay += frames;
        pts -= (CLOCK_FREQ * delay) / sys->rate;

        block->i_buffer = snd_pcm_frames_to_bytes (pcm, frames);
        block->i_nb_samples = frames;
        block->i_pts = pts;
        block->i_length = (CLOCK_FREQ * frames) / sys->rate;

        es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
        es_out_Send (demux->out, sys->es, block);
    }
    return NULL;
}

static int Control (demux_t *demux, int query, va_list ap)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_TIME:
            *va_arg (ap, int64_t *) = mdate () - sys->start;
            break;

        case DEMUX_GET_PTS_DELAY:
            *va_arg (ap, int64_t *) = sys->caching;
            break;

        //case DEMUX_SET_NEXT_DEMUX_TIME: still needed?

        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
            *va_arg (ap, bool *) = false;
            break;;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static const vlc_fourcc_t formats[] = {
    [SND_PCM_FORMAT_S8]                 = VLC_CODEC_S8,
    [SND_PCM_FORMAT_U8]                 = VLC_CODEC_U8,
    [SND_PCM_FORMAT_S16_LE]             = VLC_CODEC_S16L,
    [SND_PCM_FORMAT_S16_BE]             = VLC_CODEC_S16B,
    [SND_PCM_FORMAT_U16_LE]             = VLC_CODEC_U16L,
    [SND_PCM_FORMAT_U16_BE]             = VLC_CODEC_U16B,
    [SND_PCM_FORMAT_S24_LE]             = VLC_CODEC_S24L32,
    [SND_PCM_FORMAT_S24_BE]             = VLC_CODEC_S24B32,
    [SND_PCM_FORMAT_U24_LE]             = VLC_CODEC_U32L, // TODO: replay gain
    [SND_PCM_FORMAT_U24_BE]             = VLC_CODEC_U32B, // ^
    [SND_PCM_FORMAT_S32_LE]             = VLC_CODEC_S32L,
    [SND_PCM_FORMAT_S32_BE]             = VLC_CODEC_S32B,
    [SND_PCM_FORMAT_U32_LE]             = VLC_CODEC_U32L,
    [SND_PCM_FORMAT_U32_BE]             = VLC_CODEC_U32B,
    [SND_PCM_FORMAT_FLOAT_LE]           = VLC_CODEC_F32L,
    [SND_PCM_FORMAT_FLOAT_BE]           = VLC_CODEC_F32B,
    [SND_PCM_FORMAT_FLOAT64_LE]         = VLC_CODEC_F32L,
    [SND_PCM_FORMAT_FLOAT64_BE]         = VLC_CODEC_F32B,
  //[SND_PCM_FORMAT_IEC958_SUBFRAME_LE] = VLC_CODEC_SPDIFL,
  //[SND_PCM_FORMAT_IEC958_SUBFRAME_BE] = VLC_CODEC_SPDIFB,
    [SND_PCM_FORMAT_MU_LAW]             = VLC_CODEC_MULAW,
    [SND_PCM_FORMAT_A_LAW]              = VLC_CODEC_ALAW,
  //[SND_PCM_FORMAT_IMA_ADPCM]          = VLC_CODEC_ADPCM_?, // XXX: which one?
    [SND_PCM_FORMAT_MPEG]               = VLC_CODEC_MPGA,
    [SND_PCM_FORMAT_GSM]                = VLC_CODEC_GSM,
  //[SND_PCM_FORMAT_SPECIAL]            = VLC_CODEC_?
    [SND_PCM_FORMAT_S24_3LE]            = VLC_CODEC_S24L,
    [SND_PCM_FORMAT_S24_3BE]            = VLC_CODEC_S24B,
    [SND_PCM_FORMAT_U24_3LE]            = VLC_CODEC_U24L,
    [SND_PCM_FORMAT_U24_3BE]            = VLC_CODEC_U24B,
    [SND_PCM_FORMAT_S20_3LE]            = VLC_CODEC_S24L, // TODO: replay gain
    [SND_PCM_FORMAT_S20_3BE]            = VLC_CODEC_S24B, // ^
    [SND_PCM_FORMAT_U20_3LE]            = VLC_CODEC_U24L, // ^
    [SND_PCM_FORMAT_U20_3BE]            = VLC_CODEC_U24B, // ^
    [SND_PCM_FORMAT_S18_3LE]            = VLC_CODEC_S24L, // ^
    [SND_PCM_FORMAT_S18_3BE]            = VLC_CODEC_S24B, // ^
    [SND_PCM_FORMAT_U18_3LE]            = VLC_CODEC_U24L, // ^
    [SND_PCM_FORMAT_U18_3BE]            = VLC_CODEC_U24B, // ^
};

#ifdef WORDS_BIGENDIAN
# define C(f) f##BE, f##LE
#else
# define C(f) f##LE, f##BE
#endif

/* Formats in order of decreasing preference */
static const uint8_t choices[] = {
    C(SND_PCM_FORMAT_FLOAT_),
    C(SND_PCM_FORMAT_S32_),
    C(SND_PCM_FORMAT_U32_),
    C(SND_PCM_FORMAT_S16_),
    C(SND_PCM_FORMAT_U16_),
    C(SND_PCM_FORMAT_FLOAT64_),
    C(SND_PCM_FORMAT_S24_3),
    C(SND_PCM_FORMAT_U24_3),
    SND_PCM_FORMAT_MPEG,
    SND_PCM_FORMAT_GSM,
    SND_PCM_FORMAT_MU_LAW,
    SND_PCM_FORMAT_A_LAW,
    SND_PCM_FORMAT_S8,
    SND_PCM_FORMAT_U8,
};

static uint16_t channel_maps[] = {
    AOUT_CHAN_CENTER, AOUT_CHANS_2_0, AOUT_CHANS_3_0 /* ? */,
    AOUT_CHANS_4_0, AOUT_CHANS_5_0 /* ? */, AOUT_CHANS_5_1,
    /* TODO: support 7-8 channels - need channels reodering */
};

static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = malloc (sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Open the device */
    const char *device = demux->psz_location;
    if (device == NULL || !device[0])
        device = "default";

    const int mode = SND_PCM_NONBLOCK
                 /*| SND_PCM_NO_AUTO_RESAMPLE*/
                   | SND_PCM_NO_AUTO_CHANNELS
                 /*| SND_PCM_NO_AUTO_FORMAT*/;
    snd_pcm_t *pcm;
    int val = snd_pcm_open (&pcm, device, SND_PCM_STREAM_CAPTURE, mode);
    if (val != 0)
    {
        msg_Err (demux, "cannot open ALSA device \"%s\": %s", device,
                 snd_strerror (val));
        free (sys);
        return VLC_EGENERIC;
    }
    sys->pcm = pcm;
    msg_Dbg (demux, "using ALSA device: %s", device);
    DumpDevice (VLC_OBJECT(demux), pcm);

    /* Negotiate capture parameters */
    snd_pcm_hw_params_t *hw;
    es_format_t fmt;
    unsigned param;
    int dir;

    snd_pcm_hw_params_alloca (&hw);
    snd_pcm_hw_params_any (pcm, hw);
    Dump (demux, "initial hardware setup:\n", snd_pcm_hw_params_dump, hw);

    val = snd_pcm_hw_params_set_rate_resample (pcm, hw, 0);
    if (val)
    {
        msg_Err (demux, "cannot disable resampling: %s", snd_strerror (val));
        goto error;
    }

    val = snd_pcm_hw_params_set_access (pcm, hw,
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (val)
    {
        msg_Err (demux, "cannot set access mode: %s", snd_strerror (val));
        goto error;
    }

    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    for (size_t i = 0; i < sizeof (choices) / sizeof (choices[0]); i++)
        if (snd_pcm_hw_params_test_format (pcm, hw, choices[i]) == 0)
        {
            val = snd_pcm_hw_params_set_format (pcm, hw, choices[i]);
            if (val)
            {
                msg_Err (demux, "cannot set sample format: %s",
                         snd_strerror (val));
                goto error;
            }
            format = choices[i];
            break;
        }

    if (format == SND_PCM_FORMAT_UNKNOWN)
    {
        msg_Err (demux, "no supported sample format");
        goto error;
    }

    assert ((size_t)format < (sizeof (formats) / sizeof (formats[0])));
    es_format_Init (&fmt, AUDIO_ES, formats[format]);
    fmt.audio.i_format = fmt.i_codec;

    param = 1 + var_InheritBool (demux, "alsa-stereo");
    val = snd_pcm_hw_params_set_channels_max (pcm, hw, &param);
    if (val)
    {
        msg_Err (demux, "cannot restrict channels count: %s",
                 snd_strerror (val));
        goto error;
    }
    val = snd_pcm_hw_params_set_channels_last (pcm, hw, &param);
    if (val)
    {
        msg_Err (demux, "cannot set channels count: %s", snd_strerror (val));
        goto error;
    }
    assert (param > 0);
    assert (param < (sizeof (channel_maps) / sizeof (channel_maps[0])));
    fmt.audio.i_channels = param;
    fmt.audio.i_original_channels =
    fmt.audio.i_physical_channels = channel_maps[param - 1];

    param = var_InheritInteger (demux, "alsa-samplerate");
    val = snd_pcm_hw_params_set_rate_max (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (demux, "cannot restrict rate to %u Hz or less: %s", 192000,
                 snd_strerror (val));
        goto error;
    }
    val = snd_pcm_hw_params_set_rate_last (pcm, hw, &param, &dir);
    if (val)
    {
        msg_Err (demux, "cannot set sample rate: %s", snd_strerror (val));
        goto error;
    }
    if (dir)
        msg_Warn (demux, "sample rate is not integral");
    fmt.audio.i_rate = param;
    sys->rate = param;

    sys->start = mdate ();
    sys->caching = INT64_C(1000) * var_InheritInteger (demux, "live-caching");
    param = sys->caching;
    val = snd_pcm_hw_params_set_buffer_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (demux, "cannot set buffer duration: %s", snd_strerror (val));
        goto error;
    }

    param /= 4;
    val = snd_pcm_hw_params_set_period_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (demux, "cannot set period: %s", snd_strerror (val));
        goto error;
    }

    val = snd_pcm_hw_params_get_period_size (hw, &sys->period_size, &dir);
    if (val)
    {
        msg_Err (demux, "cannot get period size: %s", snd_strerror (val));
        goto error;
    }
    if (dir > 0)
        sys->period_size++;

    /* Commit hardware parameters */
    val = snd_pcm_hw_params (pcm, hw);
    if (val)
    {
        msg_Err (demux, "cannot commit hardware parameters: %s",
                 snd_strerror (val));
        goto error;
    }
    Dump (demux, "final HW setup:\n", snd_pcm_hw_params_dump, hw);

    /* Kick recording */
    aout_FormatPrepare (&fmt.audio);
    sys->es = es_out_Add (demux->out, &fmt);

    if (vlc_clone (&sys->thread, Thread, demux, VLC_THREAD_PRIORITY_INPUT))
    {
        es_out_Del (demux->out, sys->es);
        goto error;
    }

    demux->p_sys = sys;
    demux->pf_demux = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;
error:
    snd_pcm_close (pcm);
    free (sys);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    vlc_cancel (sys->thread);
    vlc_join (sys->thread, NULL);

    snd_pcm_close (sys->pcm);
    free (sys);
}
