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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define INITGUID
#define COBJMACROS
#define CONST_VTABLE

#include <stdlib.h>
#include <assert.h>
#include <audioclient.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include "mmdevice.h"

static LARGE_INTEGER freq; /* performance counters frequency */

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID); /* avoid warning */

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved)
{
    (void) dll;
    (void) reserved;

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            if (!QueryPerformanceFrequency(&freq))
                return FALSE;
            break;
    }
    return TRUE;
}

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
    unsigned bytes_per_frame;
    UINT32 written; /**< Frames written to the buffer */
    UINT32 frames; /**< Total buffer size (frames) */
} aout_stream_sys_t;


/*** VLC audio output callbacks ***/
static HRESULT TimeGet(aout_stream_t *s, mtime_t *restrict delay)
{
    aout_stream_sys_t *sys = s->sys;
    void *pv;
    UINT64 pos, qpcpos;
    HRESULT hr;

    hr = IAudioClient_GetService(sys->client, &IID_IAudioClock, &pv);
    if (SUCCEEDED(hr))
    {
        IAudioClock *clock = pv;

        hr = IAudioClock_GetPosition(clock, &pos, &qpcpos);
        if (FAILED(hr))
            msg_Err(s, "cannot get position (error 0x%lx)", hr);
        IAudioClock_Release(clock);
    }
    else
        msg_Err(s, "cannot get clock (error 0x%lx)", hr);

    if (SUCCEEDED(hr))
    {
        if (pos != 0)
        {
            *delay = ((GetQPC() - qpcpos) / (10000000 / CLOCK_FREQ));
            static_assert((10000000 % CLOCK_FREQ) == 0,
                          "Frequency conversion broken");
        }
        else
        {
            *delay = sys->written * CLOCK_FREQ / sys->rate;
            msg_Dbg(s, "extrapolating position: still propagating buffers");
        }
    }
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

        const size_t copy = frames * sys->bytes_per_frame;

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
        msleep(AOUT_MIN_PREPARE_TIME
             + block->i_nb_samples * CLOCK_FREQ / sys->rate);
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
    if (FAILED(hr))
        msg_Warn(s, "cannot reset stream (error 0x%lx)", hr);
    else
        sys->written = 0;
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

        for (unsigned i = 0; chans_in[i]; i++)
            if (wfe->dwChannelMask & chans_in[i])
                audio->i_physical_channels |= pi_vlc_chan_order_wg4[i];
    }

    audio->i_original_channels = audio->i_physical_channels;
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

static HRESULT Start(aout_stream_t *s, audio_sample_format_t *restrict fmt,
                     const GUID *sid)
{
    aout_stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return E_OUTOFMEMORY;
    sys->client = NULL;

    void *pv;
    HRESULT hr = aout_stream_Activate(s, &IID_IAudioClient, NULL, &pv);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot activate client (error 0x%lx)", hr);
        goto error;
    }
    sys->client = pv;

    /* Configure audio stream */
    WAVEFORMATEXTENSIBLE wf;
    WAVEFORMATEX *pwf;

    vlc_ToWave(&wf, fmt);
    hr = IAudioClient_IsFormatSupported(sys->client, AUDCLNT_SHAREMODE_SHARED,
                                        &wf.Format, &pwf);
    if (FAILED(hr))
    {
        msg_Err(s, "cannot negotiate audio format (error 0x%lx)", hr);
        goto error;
    }

    if (hr == S_FALSE)
    {
        assert(pwf != NULL);
        if (vlc_FromWave(pwf, fmt))
        {
            CoTaskMemFree(pwf);
            msg_Err(s, "unsupported audio format");
            hr = E_INVALIDARG;
            goto error;
        }
        msg_Dbg(s, "modified format");
    }
    else
        assert(pwf == NULL);

    sys->chans_to_reorder = vlc_CheckWaveOrder((hr == S_OK) ? &wf.Format : pwf,
                                               sys->chans_table);
    sys->format = fmt->i_format;

    hr = IAudioClient_Initialize(sys->client, AUDCLNT_SHAREMODE_SHARED, 0,
                                 AOUT_MAX_PREPARE_TIME * 10, 0,
                                 (hr == S_OK) ? &wf.Format : pwf, sid);
    CoTaskMemFree(pwf);
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

    sys->rate = fmt->i_rate;
    sys->bytes_per_frame = fmt->i_bytes_per_frame;
    sys->written = 0;
    s->sys = sys;
    s->time_get = TimeGet;
    s->play = Play;
    s->pause = Pause;
    s->flush = Flush;
    return S_OK;
error:
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
}

HRESULT aout_stream_Start(aout_stream_t *s,
                          audio_sample_format_t *restrict fmt, const GUID *sid)
{
    return Start(s, fmt, sid);
}

void aout_stream_Stop(aout_stream_t *s)
{
    Stop(s);
}
