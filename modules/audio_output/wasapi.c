/*****************************************************************************
 * wasapi.c : Windows Audio Session API output plugin for VLC
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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_VISTA
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define INITGUID
#define COBJMACROS
#define CONST_VTABLE
#define NONEWWAVE

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_codecs.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>

#include <audioclient.h>
#include "audio_output/mmdevice.h"

/* 00000092-0000-0010-8000-00aa00389b71 */
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL,
            WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

/* 00000008-0000-0010-8000-00aa00389b71 */
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEC61937_DTS,
            WAVE_FORMAT_DTS_MS, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

/* 0000000b-0cea-0010-8000-00aa00389b71 */
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD,
            0x000b, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

/* 0000000a-0cea-0010-8000-00aa00389b71 */
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS,
            0x000a, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

/* 0000000c-0cea-0010-8000-00aa00389b71 */
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP,
            0x000c, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

static BOOL CALLBACK InitFreq(INIT_ONCE *once, void *param, void **context)
{
    (void) once; (void) context;
    return QueryPerformanceFrequency(param);
}

static LARGE_INTEGER freq; /* performance counters frequency */

static UINT64 GetQPC(void)
{
    LARGE_INTEGER counter;

    if (!QueryPerformanceCounter(&counter))
        abort();

    lldiv_t d = lldiv(counter.QuadPart, freq.QuadPart);
    return (d.quot * 10000000) + ((d.rem * 10000000) / freq.QuadPart);
}

typedef struct aout_stream_sys
{
    IAudioClient *client;

    uint8_t chans_table[AOUT_CHAN_MAX];
    uint8_t chans_to_reorder;

    vlc_fourcc_t format; /**< Sample format */
    unsigned rate; /**< Sample rate */
    unsigned block_align;
    UINT64 written; /**< Frames written to the buffer */
    UINT32 frames; /**< Total buffer size (frames) */
} aout_stream_sys_t;


/*** VLC audio output callbacks ***/
static HRESULT TimeGet(aout_stream_t *s, mtime_t *restrict delay)
{
    aout_stream_sys_t *sys = s->sys;
    void *pv;
    UINT64 pos, qpcpos, freq;
    HRESULT hr;

    hr = IAudioClient_GetService(sys->client, &IID_IAudioClock, &pv);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot get clock (error 0x%lx)", hr);
        return hr;
    }

    IAudioClock *clock = pv;

    hr = IAudioClock_GetPosition(clock, &pos, &qpcpos);
    if (SUCCEEDED(hr))
        hr = IAudioClock_GetFrequency(clock, &freq);
    IAudioClock_Release(clock);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot get position (error 0x%lx)", hr);
        return hr;
    }

    lldiv_t w = lldiv(sys->written, sys->rate);
    lldiv_t r = lldiv(pos, freq);

    static_assert((10000000 % CLOCK_FREQ) == 0, "Frequency conversion broken");

    *delay = ((w.quot - r.quot) * CLOCK_FREQ)
           + ((w.rem * CLOCK_FREQ) / sys->rate)
           - ((r.rem * CLOCK_FREQ) / freq)
           - ((GetQPC() - qpcpos) / (10000000 / CLOCK_FREQ));

    return hr;
}

static HRESULT Play(aout_stream_t *s, block_t *block)
{
    aout_stream_sys_t *sys = s->sys;
    void *pv;
    HRESULT hr;

    if (sys->chans_to_reorder)
        aout_ChannelReorder(block->p_buffer, block->i_buffer,
                          sys->chans_to_reorder, sys->chans_table, sys->format);

    hr = IAudioClient_GetService(sys->client, &IID_IAudioRenderClient, &pv);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot get render client (error 0x%lx)", hr);
        goto out;
    }

    IAudioRenderClient *render = pv;
    for (;;)
    {
        UINT32 frames;
        hr = IAudioClient_GetCurrentPadding(sys->client, &frames);
        if (FAILED(hr))
        {
            msg_Err(s, "cannot get current padding (error 0x%lx)", hr);
            break;
        }

        assert(frames <= sys->frames);
        frames = sys->frames - frames;
        if (frames > block->i_nb_samples)
            frames = block->i_nb_samples;

        BYTE *dst;
        hr = IAudioRenderClient_GetBuffer(render, frames, &dst);
        if (FAILED(hr))
        {
            msg_Err(s, "cannot get buffer (error 0x%lx)", hr);
            break;
        }

        const size_t copy = frames * sys->block_align;

        memcpy(dst, block->p_buffer, copy);
        hr = IAudioRenderClient_ReleaseBuffer(render, frames, 0);
        if (FAILED(hr))
        {
            msg_Err(s, "cannot release buffer (error 0x%lx)", hr);
            break;
        }
        IAudioClient_Start(sys->client);

        block->p_buffer += copy;
        block->i_buffer -= copy;
        block->i_nb_samples -= frames;
        sys->written += frames;
        if (block->i_nb_samples == 0)
            break; /* done */

        /* Out of buffer space, sleep */
        msleep(sys->frames * (CLOCK_FREQ / 2) / sys->rate);
    }
    IAudioRenderClient_Release(render);
out:
    block_Release(block);

    return hr;
}

static HRESULT Pause(aout_stream_t *s, bool paused)
{
    aout_stream_sys_t *sys = s->sys;
    HRESULT hr;

    if (paused)
        hr = IAudioClient_Stop(sys->client);
    else
        hr = IAudioClient_Start(sys->client);
    if (FAILED(hr))
        msg_Warn(s, "cannot %s stream (error 0x%lx)",
                 paused ? "stop" : "start", hr);
    return hr;
}

static HRESULT Flush(aout_stream_t *s)
{
    aout_stream_sys_t *sys = s->sys;
    HRESULT hr;

    IAudioClient_Stop(sys->client);

    hr = IAudioClient_Reset(sys->client);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(s, "reset");
        sys->written = 0;
    }
    else
        msg_Warn(s, "cannot reset stream (error 0x%lx)", hr);
    return hr;
}


/*** Initialization / deinitialization **/
static const uint32_t chans_out[] = {
    SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
    SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY,
    SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT, SPEAKER_BACK_CENTER,
    SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT, 0
};
static const uint32_t chans_in[] = {
    SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
    SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT,
    SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT, SPEAKER_BACK_CENTER,
    SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY, 0
};

static void vlc_HdmiToWave(WAVEFORMATEXTENSIBLE_IEC61937 *restrict wf_iec61937,
                           audio_sample_format_t *restrict audio)
{
    WAVEFORMATEXTENSIBLE *wf = &wf_iec61937->FormatExt;

    switch (audio->i_format)
    {
    case VLC_CODEC_DTS:
        wf->SubFormat = _KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD;
        wf->Format.nChannels = 8;
        wf->dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
        audio->i_rate = 768000;
        break;
    case VLC_CODEC_EAC3:
        wf->SubFormat = _KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS;
        wf->Format.nChannels = 2;
        wf->dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
        break;
    case VLC_CODEC_TRUEHD:
    case VLC_CODEC_MLP:
        wf->SubFormat = _KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP;
        wf->Format.nChannels = 8;
        wf->dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
        audio->i_rate = 768000;
        break;
    default:
        vlc_assert_unreachable();
    }
    wf->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf->Format.nSamplesPerSec = 192000;
    wf->Format.wBitsPerSample = 16;
    wf->Format.nBlockAlign = wf->Format.wBitsPerSample / 8 * wf->Format.nChannels;
    wf->Format.nAvgBytesPerSec = wf->Format.nSamplesPerSec * wf->Format.nBlockAlign;
    wf->Format.cbSize = sizeof (*wf_iec61937) - sizeof (wf->Format);

    wf->Samples.wValidBitsPerSample = wf->Format.wBitsPerSample;

    wf_iec61937->dwEncodedSamplesPerSec = audio->i_rate;
    wf_iec61937->dwEncodedChannelCount = audio->i_channels;
    wf_iec61937->dwAverageBytesPerSec = 0;

    audio->i_format = VLC_CODEC_SPDIFL;
    audio->i_bytes_per_frame = wf->Format.nBlockAlign;
    audio->i_frame_length = 1;
}

static void vlc_SpdifToWave(WAVEFORMATEXTENSIBLE *restrict wf,
                            audio_sample_format_t *restrict audio)
{
    switch (audio->i_format)
    {
    case VLC_CODEC_DTS:
        wf->SubFormat = _KSDATAFORMAT_SUBTYPE_IEC61937_DTS;
        break;
    case VLC_CODEC_SPDIFL:
    case VLC_CODEC_SPDIFB:
    case VLC_CODEC_A52:
        wf->SubFormat = _KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL;
        break;
    default:
        vlc_assert_unreachable();
    }

    wf->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf->Format.nChannels = 2; /* To prevent channel re-ordering */
    wf->Format.nSamplesPerSec = audio->i_rate;
    wf->Format.wBitsPerSample = 16;
    wf->Format.nBlockAlign = 4; /* wf->Format.wBitsPerSample / 8 * wf->Format.nChannels  */
    wf->Format.nAvgBytesPerSec = wf->Format.nSamplesPerSec * wf->Format.nBlockAlign;
    wf->Format.cbSize = sizeof (*wf) - sizeof (wf->Format);

    wf->Samples.wValidBitsPerSample = wf->Format.wBitsPerSample;

    wf->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

    audio->i_format = VLC_CODEC_SPDIFL;
    audio->i_bytes_per_frame = wf->Format.nBlockAlign;
    audio->i_frame_length = 1;
}

static void vlc_ToWave(WAVEFORMATEXTENSIBLE *restrict wf,
                       audio_sample_format_t *restrict audio)
{
    switch (audio->i_format)
    {
        case VLC_CODEC_FL64:
            audio->i_format = VLC_CODEC_FL32;
        case VLC_CODEC_FL32:
            wf->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;

        case VLC_CODEC_U8:
            audio->i_format = VLC_CODEC_S16N;
        case VLC_CODEC_S16N:
            wf->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;

        default:
            audio->i_format = VLC_CODEC_FL32;
            wf->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
    }
    aout_FormatPrepare (audio);

    wf->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf->Format.nChannels = audio->i_channels;
    wf->Format.nSamplesPerSec = audio->i_rate;
    wf->Format.nAvgBytesPerSec = audio->i_bytes_per_frame * audio->i_rate;
    wf->Format.nBlockAlign = audio->i_bytes_per_frame;
    wf->Format.wBitsPerSample = audio->i_bitspersample;
    wf->Format.cbSize = sizeof (*wf) - sizeof (wf->Format);

    wf->Samples.wValidBitsPerSample = audio->i_bitspersample;

    wf->dwChannelMask = 0;
    for (unsigned i = 0; pi_vlc_chan_order_wg4[i]; i++)
        if (audio->i_physical_channels & pi_vlc_chan_order_wg4[i])
            wf->dwChannelMask |= chans_in[i];
}

static int vlc_FromWave(const WAVEFORMATEX *restrict wf,
                        audio_sample_format_t *restrict audio)
{
    audio->i_rate = wf->nSamplesPerSec;
    audio->i_physical_channels = 0;

    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        const WAVEFORMATEXTENSIBLE *wfe = (void *)wf;

        if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
        {
            switch (wf->wBitsPerSample)
            {
                case 64:
                    audio->i_format = VLC_CODEC_FL64;
                    break;
                case 32:
                    audio->i_format = VLC_CODEC_FL32;
                    break;
                default:
                    return -1;
            }
        }
        else if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM))
        {
            switch (wf->wBitsPerSample)
            {
                case 32:
                    audio->i_format = VLC_CODEC_S32N;
                    break;
                case 16:
                    audio->i_format = VLC_CODEC_S16N;
                    break;
                default:
                    return -1;
            }
        }

        if (wfe->Samples.wValidBitsPerSample != wf->wBitsPerSample)
            return -1;

        for (unsigned i = 0; chans_in[i]; i++)
            if (wfe->dwChannelMask & chans_in[i])
                audio->i_physical_channels |= pi_vlc_chan_order_wg4[i];
    }
    else
        return -1;

    aout_FormatPrepare (audio);

    if (wf->nChannels != audio->i_channels)
        return -1;
    return 0;
}

static unsigned vlc_CheckWaveOrder (const WAVEFORMATEX *restrict wf,
                                    uint8_t *restrict table)
{
    uint32_t mask = 0;

    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        const WAVEFORMATEXTENSIBLE *wfe = (void *)wf;

        mask = wfe->dwChannelMask;
    }
    return aout_CheckChannelReorder(chans_in, chans_out, mask, table);
}


static HRESULT Start(aout_stream_t *s, audio_sample_format_t *restrict pfmt,
                     const GUID *sid)
{
    static INIT_ONCE freq_once = INIT_ONCE_STATIC_INIT;

    if (!InitOnceExecuteOnce(&freq_once, InitFreq, &freq, NULL))
        return E_FAIL;

    aout_stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return E_OUTOFMEMORY;
    sys->client = NULL;

    /* Configure audio stream */
    WAVEFORMATEXTENSIBLE_IEC61937 wf_iec61937;
    WAVEFORMATEXTENSIBLE *pwfe = &wf_iec61937.FormatExt;
    WAVEFORMATEX *pwf = &pwfe->Format, *pwf_closest, *pwf_mix = NULL;
    AUDCLNT_SHAREMODE shared_mode;
    REFERENCE_TIME buffer_duration;
    audio_sample_format_t fmt = *pfmt;
    bool b_spdif = AOUT_FMT_SPDIF(&fmt);
    bool b_hdmi = AOUT_FMT_HDMI(&fmt);
    bool b_dtshd = false;

    if (fmt.i_format == VLC_CODEC_DTS)
    {
        b_dtshd = var_GetBool(s->obj.parent, "dtshd");
        if (b_dtshd)
        {
            b_hdmi = true;
            b_spdif = false;
        }
    }

    void *pv;
    HRESULT hr = aout_stream_Activate(s, &IID_IAudioClient, NULL, &pv);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot activate client (error 0x%lx)", hr);
        goto error;
    }
    sys->client = pv;

    if (b_spdif)
    {
        vlc_SpdifToWave(pwfe, &fmt);
        shared_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
        /* The max buffer duration in exclusive mode is 2 seconds */
        buffer_duration = AOUT_MAX_PREPARE_TIME;
    }
    else if (b_hdmi)
    {
        vlc_HdmiToWave(&wf_iec61937, &fmt);
        shared_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
        /* The max buffer duration in exclusive mode is 2 seconds */
        buffer_duration = AOUT_MAX_PREPARE_TIME;
    }
    else if (AOUT_FMT_LINEAR(&fmt))
    {
        shared_mode = AUDCLNT_SHAREMODE_SHARED;

        if (fmt.channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
        {
            fmt.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

            /* Render Ambisonics on the native mix format */
            hr = IAudioClient_GetMixFormat(sys->client, &pwf_mix);
            if (FAILED(hr) || vlc_FromWave(pwf_mix, &fmt))
                vlc_ToWave(pwfe, &fmt); /* failed, fallback to default */
            else
                pwf = pwf_mix;

            /* Setup low latency in order to quickly react to ambisonics filters
             * viewpoint changes. */
            buffer_duration = AOUT_MIN_PREPARE_TIME;
        }
        else
        {
            vlc_ToWave(pwfe, &fmt);
            buffer_duration = AOUT_MAX_PREPARE_TIME * 10;
        }
    }
    else
    {
        hr = E_FAIL;
        goto error;
    }

    hr = IAudioClient_IsFormatSupported(sys->client, shared_mode,
                                        pwf, &pwf_closest);

    if (FAILED(hr))
    {
        if (pfmt->i_format == VLC_CODEC_DTS && b_hdmi)
        {
            msg_Warn(s, "cannot negotiate DTS at 768khz IEC958 rate (HDMI), "
                     "fallback to 48kHz (S/PDIF) (error 0x%lx)", hr);
            IAudioClient_Release(sys->client);
            free(sys);
            var_SetBool(s->obj.parent, "dtshd", false);
            return Start(s, pfmt, sid);
        }
        msg_Err(s, "cannot negotiate audio format (error 0x%lx)%s", hr,
                hr == AUDCLNT_E_UNSUPPORTED_FORMAT
                && fmt.i_format == VLC_CODEC_SPDIFL ?
                ": digital pass-through not supported" : "");
        goto error;
    }

    if (hr == S_FALSE)
    {
        assert(pwf_closest != NULL);
        if (vlc_FromWave(pwf_closest, &fmt))
        {
            CoTaskMemFree(pwf_closest);
            msg_Err(s, "unsupported audio format");
            hr = E_INVALIDARG;
            goto error;
        }
        shared_mode = AUDCLNT_SHAREMODE_SHARED;
        msg_Dbg(s, "modified format");
        pwf = pwf_closest;
    }
    else
        assert(pwf_closest == NULL);

    sys->chans_to_reorder = fmt.i_format != VLC_CODEC_SPDIFL ?
                            vlc_CheckWaveOrder(pwf, sys->chans_table) : 0;
    sys->format = fmt.i_format;
    sys->block_align = pwf->nBlockAlign;
    sys->rate = pwf->nSamplesPerSec;

    hr = IAudioClient_Initialize(sys->client, shared_mode, 0, buffer_duration,
                                 0, pwf, sid);
    CoTaskMemFree(pwf_closest);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot initialize audio client (error 0x%lx)", hr);
        goto error;
    }

    hr = IAudioClient_GetBufferSize(sys->client, &sys->frames);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot get buffer size (error 0x%lx)", hr);
        goto error;
    }
    msg_Dbg(s, "buffer size    : %"PRIu32" frames", sys->frames);

    REFERENCE_TIME latT, defT, minT;
    if (SUCCEEDED(IAudioClient_GetStreamLatency(sys->client, &latT))
     && SUCCEEDED(IAudioClient_GetDevicePeriod(sys->client, &defT, &minT)))
    {
        msg_Dbg(s, "maximum latency: %"PRIu64"00 ns", latT);
        msg_Dbg(s, "default period : %"PRIu64"00 ns", defT);
        msg_Dbg(s, "minimum period : %"PRIu64"00 ns", minT);
    }

    CoTaskMemFree(pwf_mix);
    *pfmt = fmt;
    sys->written = 0;
    s->sys = sys;
    s->time_get = TimeGet;
    s->play = Play;
    s->pause = Pause;
    s->flush = Flush;
    return S_OK;
error:
    CoTaskMemFree(pwf_mix);
    if (sys->client != NULL)
        IAudioClient_Release(sys->client);
    free(sys);
    return hr;
}

static void Stop(aout_stream_t *s)
{
    aout_stream_sys_t *sys = s->sys;

    IAudioClient_Stop(sys->client); /* should not be needed */
    IAudioClient_Release(sys->client);

    free(sys);
}

vlc_module_begin()
    set_shortname("WASAPI")
    set_description(N_("Windows Audio Session API output"))
    set_capability("aout stream", 50)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Start, Stop)
vlc_module_end()
