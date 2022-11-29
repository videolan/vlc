/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2010 VLC authors and VideoLAN
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * Authors: Henri Fallon <henri@videolan.org> - Original Author
 *          Jeffrey Baker <jwbaker@acm.org> - Port to ALSA 1.0 API
 *          John Paul Lorenti <jpl31@columbia.edu> - Device selection
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr> - S/PDIF and aout3
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

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>
#include <vlc_fs.h>

#include <alsa/asoundlib.h>
#include <alsa/version.h>

/** Helper for ALSA -> VLC debugging output */
static void DumpPost(struct vlc_logger *log, snd_output_t *output,
                     const char *msg, int val)
{
    char *str;

    if (val)
    {
        vlc_warning(log, "cannot get info: %s", snd_strerror(val));
        return;
    }

    size_t len = snd_output_buffer_string (output, &str);
    if (len > 0 && str[len - 1])
        len--; /* strip trailing newline */
    vlc_debug(log, "%s%.*s", msg, (int)len, str);
    snd_output_close (output);
}

#define Dump(o, m, cb, p) \
    do { \
        snd_output_t *output; \
\
        if (likely(snd_output_buffer_open(&output) == 0)) \
            DumpPost(o, output, m, (cb)(p, output)); \
    } while (0)

static void DumpDevice(struct vlc_logger *log, snd_pcm_t *pcm)
{
    snd_pcm_info_t *info;

    Dump(log, " ", snd_pcm_dump, pcm);
    snd_pcm_info_alloca (&info);
    if (snd_pcm_info (pcm, info) == 0)
    {
        vlc_debug(log, " device name   : %s", snd_pcm_info_get_name (info));
        vlc_debug(log, " device ID     : %s", snd_pcm_info_get_id (info));
        vlc_debug(log, " subdevice name: %s",
                  snd_pcm_info_get_subdevice_name (info));
    }
}

static void DumpDeviceStatus(struct vlc_logger *log, snd_pcm_t *pcm)
{
    snd_pcm_status_t *status;

    snd_pcm_status_alloca (&status);
    snd_pcm_status (pcm, status);
    Dump(log, "current status:\n", snd_pcm_status_dump, status);
}

typedef enum
{
    IDLE,
    PLAYING,
    PAUSED,
} pb_state_t;

/** Private data for an ALSA PCM playback stream */
typedef struct
{
    snd_pcm_t *pcm;
    unsigned rate; /**< Sample rate */
    vlc_fourcc_t format; /**< Sample format */
    uint8_t chans_table[AOUT_CHAN_MAX]; /**< Channels order table */
    uint8_t chans_to_reorder; /**< Number of channels to reorder */

    bool soft_mute;
    float soft_gain;
    char *device;

    vlc_thread_t thread;
    pb_state_t state;
    bool started;
    bool draining;
    bool unrecoverable_error;
    vlc_mutex_t lock;
    vlc_sem_t init_sem;
    int wakefd[2];
    vlc_frame_t *frame_chain;
    vlc_frame_t **frame_last;
    uint64_t queued_samples;
} aout_sys_t;

#include "audio_output/volume.h"

static void wake_poll(aout_sys_t *sys)
{
    uint64_t val = 1;
    ssize_t rd = write(sys->wakefd[1], &val, sizeof(val));
    assert(rd == sizeof(val));
    (void) rd;
}

static int recover_from_pcm_state(snd_pcm_t *pcm)
{
    snd_pcm_state_t state = snd_pcm_state(pcm);
    int err = 0;
    switch (state)
    {
    case SND_PCM_STATE_RUNNING:
    case SND_PCM_STATE_PAUSED:
        return 0;
    case SND_PCM_STATE_XRUN:
        err = -EPIPE;
        break;
    case SND_PCM_STATE_SUSPENDED:
        err = -ESTRPIPE;
        break;
    default:
        err = 0;
    }

    if (err)
        return snd_pcm_recover(pcm, err, -1);

    return -1;
}

static int fill_pfds_from_state_locked(audio_output_t *aout, struct pollfd **pfds, int *pfds_count)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;
    switch (sys->state)
    {
    case IDLE:
    case PAUSED:
        /* We are paused or drained no need to wait for snd pcm*/
        return 1;
    case PLAYING:
    {
        if (sys->frame_chain == NULL && !sys->draining)
            return 1; /* Waiting for data */

        int cnt = snd_pcm_poll_descriptors_count(pcm);
        if (unlikely(cnt < 0))
        {
            if (!recover_from_pcm_state(pcm))
                return 0;

            msg_Err(aout, "Cannot retrieve descriptors' count (%d)", cnt);
            return -1;
        }
        else if (cnt + 1 > *pfds_count)
        {
            struct pollfd * tmp = realloc(*pfds, sizeof(struct pollfd) * (cnt + 1));
            if (tmp == NULL)
            {
                sys->unrecoverable_error = true;
                return -1;
            }
            *pfds = tmp;
            *pfds_count = cnt + 1;
        }

        cnt = snd_pcm_poll_descriptors(pcm, &(*pfds)[1], cnt);
        if (unlikely(cnt < 0))
        {
            if (!recover_from_pcm_state(pcm))
                return 0;

            msg_Err(aout, "snd_pcm_poll_descriptors failed (%d)", cnt);
            return -1;
        }
        return cnt + 1;
    }
    default:
        return -1;
    }
    return -1;
}

static void * InjectionThread(void * data)
{
    audio_output_t *aout = (audio_output_t *) data;
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;

    /* We're expecting at least 2 fds:
     * - one for generic wakeup
     * - one or more for alsa
     */
    struct pollfd * pfds = calloc(2, sizeof(struct pollfd));
    if (pfds == NULL)
    {
      sys->unrecoverable_error = true;
      vlc_sem_post(&sys->init_sem);
      return NULL;
    }
    int pfds_count = 2;

    pfds[0].fd = sys->wakefd[0];
    pfds[0].events = POLLIN;

    vlc_sem_post(&sys->init_sem);

    vlc_mutex_lock(&sys->lock);
    while (sys->started)
    {
        int cnt = fill_pfds_from_state_locked(aout, &pfds, &pfds_count);
        if (unlikely(cnt < 0))
            break;
        else if (unlikely(cnt == 0))
            continue;

        vlc_mutex_unlock(&sys->lock);

        cnt = poll(pfds, cnt, -1);

        vlc_mutex_lock(&sys->lock);
        if (unlikely(cnt < 0))
        {
            if (errno == -EINTR)
                continue;
            msg_Err(aout, "poll failed (%s)", strerror(errno));
            break;
        }

        if (pfds[0].revents & POLLIN)
        {
            uint64_t val;
            ssize_t rd = read(sys->wakefd[0], &val, sizeof(val));
            if (rd != sizeof(val))
            {
                msg_Err(aout, "Invalid read on wakefd got %zd (%s)", rd, strerror(errno));
                break;
            }
            /* We either got data or a state change, let's refill the pfds or abort */
            continue;
        }

        unsigned short revents;
        cnt = snd_pcm_poll_descriptors_revents(pcm, &pfds[1], pfds_count-1, &revents);
        if (cnt != 0)
        {
            if (!recover_from_pcm_state(pcm))
                continue;

            msg_Err(aout, "snd_pcm_poll_descriptors_revents failed (%d)", cnt);
            break;
        }

        if (unlikely(revents & POLLERR))
        {
            if (!recover_from_pcm_state(pcm))
                continue;
            if (sys->draining)
            {
                msg_Warn(aout,"Polling error from drain");
                snd_pcm_prepare(pcm);
                sys->state = IDLE;
                sys->draining = false;
                aout_DrainedReport(aout);
                continue;
            }
            msg_Err(aout, "Unrecoverable polling error");
            break;
        }

        if (!(revents & POLLOUT) ||
            sys->state == PAUSED ||
            sys->state == IDLE)
            continue;

        if (sys->frame_chain == NULL)
        {
            if (sys->draining)
            {
                /* SND_PCM_NONBLOCK makes snd_pcm_drain non blocking so we must
                 * call poll until its completion
                 */
                if (snd_pcm_drain(pcm) == -EAGAIN)
                    continue;
                snd_pcm_prepare(pcm);
                sys->state = IDLE;
                sys->draining = false;
                aout_DrainedReport(aout);
            }
            continue;
        }

        vlc_frame_t * f = sys->frame_chain;
        snd_pcm_sframes_t frames = snd_pcm_writei(pcm, f->p_buffer, f->i_nb_samples);
        if (frames >= 0)
        {
            size_t bytes = snd_pcm_frames_to_bytes(pcm, frames);
            f->i_nb_samples -= frames;
            f->p_buffer += bytes;
            f->i_buffer -= bytes;
            sys->queued_samples -= frames;
            // pts, length
            if (f->i_nb_samples == 0)
            {
                sys->frame_chain = f->p_next;
                if (sys->frame_chain == NULL)
                    sys->frame_last = &sys->frame_chain;
                vlc_frame_Release(f);
            }
        }
        else if (frames == -EAGAIN)
            continue;
        else
        {
            int val = snd_pcm_recover(pcm, frames, 1);
            if (val)
            {
                msg_Err(aout, "cannot recover playback stream: %s",
                        snd_strerror (val));
                DumpDeviceStatus(aout->obj.logger, pcm);
                break;
            }
            msg_Warn(aout, "cannot write samples: %s", snd_strerror(frames));
        }
    }
    free(pfds);
    if (sys->started && !sys->unrecoverable_error)
    {
        msg_Err(aout, "Unhandled error in injection thread, requesting aout restart");
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    }
    vlc_mutex_unlock(&sys->lock);
    return NULL;
}

static int TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_sframes_t frames;

    vlc_mutex_lock(&sys->lock);
    int val = snd_pcm_delay(sys->pcm, &frames);
    if (val)
    {
        msg_Err(aout, "cannot estimate delay: %s", snd_strerror(val));
        vlc_mutex_unlock(&sys->lock);
        return -1;
    }
    *delay = vlc_tick_from_samples(frames + sys->queued_samples, sys->rate);
    vlc_mutex_unlock(&sys->lock);
    return 0;
}


/**
 * Queues one audio buffer to the hardware.
 */
static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    if (sys->chans_to_reorder != 0)
        aout_ChannelReorder(block->p_buffer, block->i_buffer,
                            sys->chans_to_reorder, sys->chans_table,
                            sys->format);

    vlc_mutex_lock(&sys->lock);
    if (unlikely(sys->unrecoverable_error))
    {
        vlc_frame_Release(block);
        vlc_mutex_unlock(&sys->lock);
        return;
    }
    if (sys->frame_chain == NULL)
        wake_poll(sys);
    vlc_frame_ChainLastAppend(&sys->frame_last, block);
    sys->queued_samples += block->i_nb_samples;
    vlc_mutex_unlock(&sys->lock);

    (void) date;
}

static void PauseDummy(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;

    /* Stupid device cannot pause. Discard samples. */
    vlc_mutex_lock(&sys->lock);
    if (pause)
    {
        sys->state = PAUSED;
        snd_pcm_drop(pcm);
    }
    else
    {
        sys->state = PLAYING;
        snd_pcm_prepare(pcm);
    }
    wake_poll(sys);
    vlc_mutex_unlock(&sys->lock);

    (void) date;
}

/**
 * Pauses/resumes the audio playback.
 */
static void Pause(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;

    int val = snd_pcm_pause(pcm, pause);
    if (unlikely(val))
    {
        PauseDummy(aout, pause, date);
        return;
    }
    vlc_mutex_lock(&sys->lock);
    sys->state = pause? PAUSED: PLAYING;
    wake_poll(sys);
    vlc_mutex_unlock(&sys->lock);
}

/**
 * Flushes the audio playback buffer.
 */
static void Flush (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;

    vlc_mutex_lock(&sys->lock);
    vlc_frame_ChainRelease(sys->frame_chain);
    sys->frame_chain = NULL;
    sys->frame_last = &sys->frame_chain;
    sys->queued_samples = 0;
    sys->draining = false;

    if (sys->state == IDLE)
        sys->state = PLAYING;
    wake_poll(sys);
    vlc_mutex_unlock(&sys->lock);

    snd_pcm_drop(pcm);
    snd_pcm_prepare(pcm);
}

/**
 * Drains the audio playback buffer.
 */
static void Drain (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock(&sys->lock);
    sys->draining = true;
    wake_poll(sys);
    vlc_mutex_unlock(&sys->lock);
}

/**
 * Releases the audio output.
 */
static void Stop (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;

    vlc_mutex_lock(&sys->lock);
    sys->started = false;
    sys->state = IDLE;
    sys->draining = false;
    vlc_frame_ChainRelease(sys->frame_chain);
    sys->frame_chain = NULL;
    sys->frame_last = &sys->frame_chain;
    sys->queued_samples = 0;
    wake_poll(sys);
    snd_pcm_drop(pcm);
    vlc_mutex_unlock(&sys->lock);

    vlc_join(sys->thread, NULL);

    snd_pcm_close(pcm);
}

#if (SND_LIB_VERSION >= 0x01001B)
static const uint16_t vlc_chans[] = {
    [SND_CHMAP_MONO] = AOUT_CHAN_CENTER,
    [SND_CHMAP_FL]   = AOUT_CHAN_LEFT,
    [SND_CHMAP_FR]   = AOUT_CHAN_RIGHT,
    [SND_CHMAP_RL]   = AOUT_CHAN_REARLEFT,
    [SND_CHMAP_RR]   = AOUT_CHAN_REARRIGHT,
    [SND_CHMAP_FC]   = AOUT_CHAN_CENTER,
    [SND_CHMAP_LFE]  = AOUT_CHAN_LFE,
    [SND_CHMAP_SL]   = AOUT_CHAN_MIDDLELEFT,
    [SND_CHMAP_SR]   = AOUT_CHAN_MIDDLERIGHT,
    [SND_CHMAP_RC]   = AOUT_CHAN_REARCENTER,
};
static_assert(AOUT_CHAN_MAX == 9, "Missing channel entries");

static int Map2Mask (vlc_object_t *obj, const snd_pcm_chmap_t *restrict map)
{
    uint16_t mask = 0;

    for (unsigned i = 0; i < map->channels; i++)
    {
        const unsigned pos = map->pos[i];
        uint_fast16_t vlc_chan = 0;

        if (pos < sizeof (vlc_chans) / sizeof (vlc_chans[0]))
            vlc_chan = vlc_chans[pos];
        if (vlc_chan == 0)
        {
            msg_Dbg (obj, " %s channel %u position %u", "unsupported", i, pos);
            return -1;
        }
        if (mask & vlc_chan)
        {
            msg_Dbg (obj, " %s channel %u position %u", "duplicate", i, pos);
            return -1;
        }
        mask |= vlc_chan;
    }
    return mask;
}

/**
 * Compares a fixed ALSA channels map with the VLC channels order.
 */
static unsigned SetupChannelsFixed(const snd_pcm_chmap_t *restrict map,
                               uint16_t *restrict maskp, uint8_t *restrict tab)
{
    uint32_t chans_out[AOUT_CHAN_MAX];
    uint16_t mask = 0;

    for (unsigned i = 0; i < map->channels; i++)
    {
        uint_fast16_t vlc_chan = vlc_chans[map->pos[i]];

        chans_out[i] = vlc_chan;
        mask |= vlc_chan;
    }

    *maskp = mask;
    return aout_CheckChannelReorder(NULL, chans_out, mask, tab);
}

/**
 * Negotiate channels mapping.
 */
static unsigned SetupChannels (vlc_object_t *obj, snd_pcm_t *pcm,
                               uint16_t *restrict mask, uint8_t *restrict tab)
{
    snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps (pcm);
    if (maps == NULL)
    {   /* Fallback to default order if unknown */
        msg_Dbg(obj, "channels map not provided");
        return 0;
    }

    /* Find most appropriate available channels map */
    unsigned best_offset, best_score = 0, to_reorder = 0;

    for (snd_pcm_chmap_query_t *const *p = maps; *p != NULL; p++)
    {
        snd_pcm_chmap_query_t *map = *p;

        switch (map->type)
        {
            case SND_CHMAP_TYPE_FIXED:
            case SND_CHMAP_TYPE_PAIRED:
            case SND_CHMAP_TYPE_VAR:
                break;
            default:
                msg_Err (obj, "unknown channels map type %u", map->type);
                continue;
        }

        int chans = Map2Mask (obj, &map->map);
        if (chans == -1)
            continue;

        unsigned score = (vlc_popcount (chans & *mask) << 8)
                       | (255 - vlc_popcount (chans));
        if (score > best_score)
        {
            best_offset = p - maps;
            best_score = score;
        }
    }

    if (best_score == 0)
    {
        msg_Err (obj, "cannot find supported channels map");
        goto out;
    }

    const snd_pcm_chmap_t *map = &maps[best_offset]->map;
    msg_Dbg (obj, "using channels map %u, type %u, %u channel(s)", best_offset,
             maps[best_offset]->type, map->channels);

    /* Setup channels map */
    to_reorder = SetupChannelsFixed(map, mask, tab);

    /* TODO: avoid reordering for PAIRED and VAR types */
    //snd_pcm_set_chmap (pcm, ...)
out:
    snd_pcm_free_chmaps (maps);
    return to_reorder;
}
#else /* (SND_LIB_VERSION < 0x01001B) */
# define SetupChannels(obj, pcm, mask, tab) (0)
#endif

enum {
    PASSTHROUGH_NONE,
    PASSTHROUGH_SPDIF,
    PASSTHROUGH_HDMI,
};

#define A52_FRAME_NB 1536

/** Initializes an ALSA playback stream */
static int Start (audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    struct vlc_logger *log = aout->obj.logger;
    aout_sys_t *sys = aout->sys;
    snd_pcm_format_t pcm_format; /* ALSA sample format */
    unsigned channels;
    int passthrough = PASSTHROUGH_NONE;

    if (aout_FormatNbChannels(fmt) == 0)
        return VLC_EGENERIC;

    switch (fmt->i_format)
    {
        case VLC_CODEC_FL64:
            pcm_format = SND_PCM_FORMAT_FLOAT64;
            break;
        case VLC_CODEC_FL32:
            pcm_format = SND_PCM_FORMAT_FLOAT;
            break;
        case VLC_CODEC_S32N:
            pcm_format = SND_PCM_FORMAT_S32;
            break;
        case VLC_CODEC_S16N:
            pcm_format = SND_PCM_FORMAT_S16;
            break;
        case VLC_CODEC_U8:
            pcm_format = SND_PCM_FORMAT_U8;
            break;
        default:
            if (AOUT_FMT_SPDIF(fmt))
            {
                passthrough = var_InheritInteger(aout, "alsa-passthrough");
                channels = 2;
            }
            if (AOUT_FMT_HDMI(fmt))
            {
                passthrough = var_InheritInteger(aout, "alsa-passthrough");
                if (passthrough == PASSTHROUGH_SPDIF)
                    passthrough = PASSTHROUGH_NONE; /* TODO? convert down */
                channels = 8;
            }

            if (passthrough != PASSTHROUGH_NONE)
            {
                fmt->i_format = VLC_CODEC_SPDIFL;
                pcm_format = SND_PCM_FORMAT_S16;
            }
            else
            if (HAVE_FPU)
            {
                fmt->i_format = VLC_CODEC_FL32;
                pcm_format = SND_PCM_FORMAT_FLOAT;
            }
            else
            {
                fmt->i_format = VLC_CODEC_S16N;
                pcm_format = SND_PCM_FORMAT_S16;
            }
    }

    const char *device = sys->device;

    /* Choose the device for passthrough output */
    char sep = '\0';
    if (passthrough != PASSTHROUGH_NONE)
    {
        const char *opt = NULL;

        if (!strcmp (device, "default"))
            device = (passthrough == PASSTHROUGH_HDMI) ? "hdmi" : "iec958";

        if (!strncmp (device, "iec958", 6))
            opt = device + 6;
        if (!strncmp (device, "hdmi", 4))
            opt = device + 4;

        if (opt != NULL)
            switch (*opt)
            {
                case ':':  sep = ','; break;
                case '\0': sep = ':'; break;
            }
    }

    char *devbuf = NULL;
    if (sep != '\0')
    {
#ifdef IEC958_AES3_CON_FS_22050
        unsigned aes3;

        switch (fmt->i_rate)
        {
#define FS(freq) \
            case freq: aes3 = IEC958_AES3_CON_FS_ ## freq; break;
            FS( 44100) /* def. */ FS( 48000) FS( 32000)
            FS( 22050)            FS( 24000)
            FS( 88200) FS(768000) FS( 96000)
            FS(176400)            FS(192000)
#undef FS
            default:
                aes3 = IEC958_AES3_CON_FS_NOTID;
                break;
        }

        if (asprintf (&devbuf, "%s%cAES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                      device, sep,
                      IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO,
                      IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,
                      0, aes3) == -1)
            return VLC_ENOMEM;
        device = devbuf;
#else
       /* SALSA-lib lacks many AES definitions, but it does not matter much,
        * as it does note support parametric device names either. */
       return VLC_ENOTSUP;
#endif
    }

    /* Open the device */
    snd_pcm_t *pcm;
    /* VLC always has a resampler. No need for ALSA's. */
    const int mode = SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NONBLOCK;

    int val = snd_pcm_open (&pcm, device, SND_PCM_STREAM_PLAYBACK, mode);
    if (val != 0)
    {
        msg_Err (aout, "cannot open ALSA device \"%s\": %s", device,
                 snd_strerror (val));
        vlc_dialog_display_error (aout, _("Audio output failed"),
            _("The audio device \"%s\" could not be used:\n%s."),
            sys->device, snd_strerror (val));
        free (devbuf);
        return VLC_EGENERIC;
    }
    sys->pcm = pcm;

    /* Print some potentially useful debug */
    msg_Dbg (aout, "using ALSA device: %s", device);
    free (devbuf);
    DumpDevice(log, pcm);

    /* Get Initial hardware parameters */
    snd_pcm_hw_params_t *hw;
    unsigned param;

    snd_pcm_hw_params_alloca (&hw);
    snd_pcm_hw_params_any (pcm, hw);
    Dump(log, "initial hardware setup:\n", snd_pcm_hw_params_dump, hw);

    val = snd_pcm_hw_params_set_rate_resample(pcm, hw, 0);
    if (val)
    {
        msg_Err (aout, "cannot disable resampling: %s", snd_strerror (val));
        goto error;
    }

    val = snd_pcm_hw_params_set_access (pcm, hw,
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (val)
    {
        msg_Err (aout, "cannot set access mode: %s", snd_strerror (val));
        goto error;
    }

    /* Set sample format */
    if (snd_pcm_hw_params_test_format (pcm, hw, pcm_format) == 0)
        ;
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_FLOAT) == 0)
    {
        fmt->i_format = VLC_CODEC_FL32;
        pcm_format = SND_PCM_FORMAT_FLOAT;
    }
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_S32) == 0)
    {
        fmt->i_format = VLC_CODEC_S32N;
        pcm_format = SND_PCM_FORMAT_S32;
    }
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_S16) == 0)
    {
        fmt->i_format = VLC_CODEC_S16N;
        pcm_format = SND_PCM_FORMAT_S16;
    }
    else
    {
        msg_Err (aout, "no supported sample format");
        goto error;
    }

    val = snd_pcm_hw_params_set_format (pcm, hw, pcm_format);
    if (val)
    {
        msg_Err (aout, "cannot set sample format: %s", snd_strerror (val));
        goto error;
    }

    /* Set channels count */
    if (passthrough == PASSTHROUGH_NONE)
    {
        uint16_t map = var_InheritInteger (aout, "alsa-audio-channels");

        sys->chans_to_reorder = SetupChannels (VLC_OBJECT(aout), pcm, &map,
                                               sys->chans_table);
        fmt->i_physical_channels = map;
        channels = vlc_popcount (map);
    }
    else
        sys->chans_to_reorder = 0;

    /* By default, ALSA plug will pad missing channels with zeroes, which is
     * usually fine. However, it will also discard extraneous channels, which
     * is not acceptable. Thus the user must configure the physically
     * available channels, and VLC will downmix if needed. */
    val = snd_pcm_hw_params_set_channels (pcm, hw, channels);
    if (val)
    {
        msg_Err (aout, "cannot set %u channels: %s", channels,
                 snd_strerror (val));
        goto error;
    }

    /* Set sample rate */
    val = snd_pcm_hw_params_set_rate_near (pcm, hw, &fmt->i_rate, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set sample rate: %s", snd_strerror (val));
        goto error;
    }
    sys->rate = fmt->i_rate;

#if 1 /* work-around for period-long latency outputs (e.g. PulseAudio): */
    param = AOUT_MIN_PREPARE_TIME;
    val = snd_pcm_hw_params_set_period_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set period: %s", snd_strerror (val));
        goto error;
    }
#endif
    /* Set buffer size */
    param = AOUT_MAX_ADVANCE_TIME;
    val = snd_pcm_hw_params_set_buffer_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set buffer duration: %s", snd_strerror (val));
        goto error;
    }
#if 0
    val = snd_pcm_hw_params_get_buffer_time (hw, &param, NULL);
    if (val)
    {
        msg_Warn (aout, "cannot get buffer time: %s", snd_strerror(val));
        param = AOUT_MIN_PREPARE_TIME;
    }
    else
        param /= 2;
    val = snd_pcm_hw_params_set_period_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set period: %s", snd_strerror (val));
        goto error;
    }
#endif

    /* Commit hardware parameters */
    val = snd_pcm_hw_params (pcm, hw);
    if (val < 0)
    {
        msg_Err (aout, "cannot commit hardware parameters: %s",
                 snd_strerror (val));
        goto error;
    }
    Dump(log, "final HW setup:\n", snd_pcm_hw_params_dump, hw);

    /* Get Initial software parameters */
    snd_pcm_sw_params_t *sw;

    snd_pcm_sw_params_alloca (&sw);
    snd_pcm_sw_params_current (pcm, sw);
    Dump(log, "initial software parameters:\n", snd_pcm_sw_params_dump, sw);

    /* START REVISIT */
    //snd_pcm_sw_params_set_avail_min( pcm, sw, i_period_size );
    // FIXME: useful?
    val = snd_pcm_sw_params_set_start_threshold (pcm, sw, 1);
    if( val < 0 )
    {
        msg_Err( aout, "unable to set start threshold (%s)",
                 snd_strerror( val ) );
        goto error;
    }
    /* END REVISIT */

    /* Commit software parameters. */
    val = snd_pcm_sw_params (pcm, sw);
    if (val)
    {
        msg_Err (aout, "cannot commit software parameters: %s",
                 snd_strerror (val));
        goto error;
    }
    Dump(log, "final software parameters:\n", snd_pcm_sw_params_dump, sw);

    val = snd_pcm_prepare (pcm);
    if (val)
    {
        msg_Err (aout, "cannot prepare device: %s", snd_strerror (val));
        goto error;
    }

    /* Setup audio_output_t */
    if (passthrough != PASSTHROUGH_NONE)
    {
        fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE * (channels / 2);
        fmt->i_frame_length = A52_FRAME_NB;
    }
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    sys->format = fmt->i_format;

    if (snd_pcm_hw_params_can_pause (hw))
        aout->pause = Pause;
    else
    {
        aout->pause = PauseDummy;
        msg_Warn (aout, "device cannot be paused");
    }

    aout_SoftVolumeStart (aout);

    sys->queued_samples = 0;
    sys->started = true;
    sys->draining = false;
    sys->state = PLAYING;
    sys->unrecoverable_error = false;
    if (vlc_clone(&sys->thread, InjectionThread, aout))
        goto error;

    vlc_sem_wait(&sys->init_sem);
    if (sys->unrecoverable_error)
    {
        vlc_join(sys->thread, NULL);
        goto error;
    }
    return 0;

error:
    snd_pcm_close (pcm);
    return VLC_EGENERIC;
}

/**
 * Enumerates ALSA output devices.
 */
static int EnumDevices(char const *varname,
                       char ***restrict idp, char ***restrict namep)
{
    void **hints;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return -1;

    char **ids = NULL, **names = NULL;
    unsigned n = 0;
    bool hinted_default = false;

    for (size_t i = 0; hints[i] != NULL; i++)
    {
        void *hint = hints[i];

        char *name = snd_device_name_get_hint(hint, "NAME");
        if (unlikely(name == NULL))
            continue;

        char *desc = snd_device_name_get_hint(hint, "DESC");
        if (desc == NULL)
            desc = xstrdup (name);
        for (char *lf = strchr(desc, '\n'); lf; lf = strchr(lf, '\n'))
            *lf = ' ';

        ids = xrealloc (ids, (n + 1) * sizeof (*ids));
        names = xrealloc (names, (n + 1) * sizeof (*names));
        ids[n] = name;
        names[n] = desc;
        n++;

        if (!strcmp(name, "default"))
            hinted_default = true;
    }

    snd_device_name_free_hint(hints);

    if (!hinted_default)
    {
        ids = xrealloc (ids, (n + 1) * sizeof (*ids));
        names = xrealloc (names, (n + 1) * sizeof (*names));
        ids[n] = xstrdup ("default");
        names[n] = xstrdup (_("Default"));
        n++;
    }

    *idp = ids;
    *namep = names;
    (void) varname;
    return n;
}

VLC_CONFIG_STRING_ENUM(EnumDevices)

static int DeviceSelect (audio_output_t *aout, const char *id)
{
    aout_sys_t *sys = aout->sys;

    char *device = strdup (id ? id : "default");
    if (unlikely(device == NULL))
        return -1;

    free (sys->device);
    sys->device = device;
    aout_DeviceReport (aout, device);
    aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    return 0;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc (sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

#ifdef HAVE_EVENTFD
    sys->wakefd[0] = eventfd(0, EFD_CLOEXEC);
    sys->wakefd[1] = sys->wakefd[0];
    if (sys->wakefd[0] < 0)
      goto error;
#else
    if (vlc_pipe(sys->wakefd))
    {
      sys->wakefd[0] = sys->wakefd[1] = -1;
      goto error;
    }
#endif

    sys->device = var_InheritString (aout, "alsa-audio-device");
    if (unlikely(sys->device == NULL))
        goto error;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout_SoftVolumeInit (aout);
    aout->device_select = DeviceSelect;
    aout_DeviceReport (aout, sys->device);

    /* ALSA does not support hot-plug events so list devices at startup */
    char **ids, **names;
    int count = EnumDevices(NULL, &ids, &names);
    if (count >= 0)
    {
        msg_Dbg (obj, "Available ALSA PCM devices:");

        for (int i = 0; i < count; i++)
        {
            msg_Dbg(obj, "%s: %s", ids[i], names[i]);
            aout_HotplugReport (aout, ids[i], names[i]);
            free (names[i]);
            free (ids[i]);
        }
        free (names);
        free (ids);
    }

    sys->state = IDLE;
    vlc_mutex_init(&sys->lock);
    sys->frame_chain = NULL;
    sys->frame_last = &sys->frame_chain;
    vlc_sem_init(&sys->init_sem, 0);

    aout->time_get = TimeGet;
    aout->play = Play;
    aout->flush = Flush;
    aout->drain = Drain;

    return VLC_SUCCESS;
error:
    if (sys->wakefd[0] >= 0)
    {
      if (sys->wakefd[1] != sys->wakefd[0])
        vlc_close(sys->wakefd[1]);
      vlc_close(sys->wakefd[0]);
    }
    free (sys);
    return VLC_ENOMEM;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free (sys->device);
    if (sys->wakefd[1] != sys->wakefd[0])
      vlc_close(sys->wakefd[1]);
    vlc_close(sys->wakefd[0]);
    free (sys);
}

#define AUDIO_DEV_TEXT N_("Audio output device")
#define AUDIO_DEV_LONGTEXT N_("Audio output device (using ALSA syntax).")

#define AUDIO_CHAN_TEXT N_("Audio output channels")
#define AUDIO_CHAN_LONGTEXT N_("Channels available for audio output. " \
    "If the input has more channels than the output, it will be down-mixed. " \
    "This parameter is ignored when digital pass-through is active.")
static const int channels[] = {
    AOUT_CHAN_CENTER, AOUT_CHANS_STEREO, AOUT_CHANS_4_0, AOUT_CHANS_4_1,
    AOUT_CHANS_5_0, AOUT_CHANS_5_1, AOUT_CHANS_7_1,
};
static const char *const channels_text[] = {
    N_("Mono"), N_("Stereo"), N_("Surround 4.0"), N_("Surround 4.1"),
    N_("Surround 5.0"), N_("Surround 5.1"), N_("Surround 7.1"),
};

#define PASSTHROUGH_TEXT N_("Audio passthrough mode")
static const int passthrough_modes[] = {
    PASSTHROUGH_NONE, PASSTHROUGH_SPDIF, PASSTHROUGH_HDMI,
};
static const char *const passthrough_modes_text[] = {
    N_("None"), N_("S/PDIF"), N_("HDMI"),
};

vlc_module_begin()
    set_shortname("ALSA")
    set_description(N_("ALSA audio output"))
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_string("alsa-audio-device", "default",
               AUDIO_DEV_TEXT, AUDIO_DEV_LONGTEXT)
    add_integer("alsa-audio-channels", AOUT_CHANS_FRONT,
                AUDIO_CHAN_TEXT, AUDIO_CHAN_LONGTEXT)
        change_integer_list (channels, channels_text)
    add_integer("alsa-passthrough", PASSTHROUGH_NONE, PASSTHROUGH_TEXT,
                NULL)
        change_integer_list(passthrough_modes, passthrough_modes_text)
    add_sw_gain()
    set_capability("audio output", 150)
    set_callbacks(Open, Close)
vlc_module_end()
