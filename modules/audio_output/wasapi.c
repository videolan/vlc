/*****************************************************************************
 * wasapi.c : Windows Audio Session API output plugin for VLC
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

#define INITGUID
#define COBJMACROS

#include <assert.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("WASAPI")
    set_description(N_("Windows Audio Session output") )
    set_capability("audio output", 150)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("was", "audioclient")
    set_callbacks(Open, Close)
vlc_module_end()

struct aout_sys_t
{
    IAudioClient *client;
    IAudioRenderClient *render;
    UINT32 frames; /**< Total buffer size (frames) */
    HANDLE done; /**< Semaphore for MTA thread */
};

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    while (block->i_nb_samples > 0)
    {
        UINT32 frames;
        hr = IAudioClient_GetCurrentPadding(sys->client, &frames);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get current padding (error 0x%lx)", hr);
            break;
        }

        assert(frames <= sys->frames);
        frames = sys->frames - frames;
        if (frames > block->i_nb_samples)
            frames = block->i_nb_samples;

        BYTE *dst;
        hr = IAudioRenderClient_GetBuffer(sys->render, frames, &dst);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get buffer (error 0x%lx)", hr);
            break;
        }

        const size_t copy = frames * (size_t)aout->format.i_bytes_per_frame;

        memcpy(dst, block->p_buffer, copy);
        hr = IAudioRenderClient_ReleaseBuffer(sys->render, frames, 0);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot release buffer (error 0x%lx)", hr);
            break;
        }

        block->p_buffer += copy;
        block->i_buffer -= copy;
        block->i_nb_samples -= frames;

        /* FIXME: implement synchro */
        IAudioClient_Start(sys->client);
        Sleep(AOUT_MIN_PREPARE_TIME / 1000);
    }

    CoUninitialize();
    block_Release(block);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (!paused)
        return;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = IAudioClient_Stop(sys->client);
    if (FAILED(hr))
        msg_Warn(aout, "cannot stop stream (error 0x%lx)", hr);
    CoUninitialize();
    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (wait)
        return; /* Not drain implemented */

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IAudioClient_Stop(sys->client);
    hr = IAudioClient_Reset(sys->client);
    if (FAILED(hr))
        msg_Warn(aout, "cannot reset stream (error 0x%lx)", hr);
    CoUninitialize();
}

/*static int VolumeSet(audio_output_t *aout, float vol, bool mute)
{
    aout_sys_t *sys = aout->sys;

    return 0;
}*/

static void vlc_ToWave(WAVEFORMATEXTENSIBLE *restrict wf,
                       audio_sample_format_t *restrict audio)
{
    switch (audio->i_format)
    {
#if 0
        case VLC_CODEC_FL32:
        case VLC_CODEC_FL64:
            wf->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;

        case VLC_CODEC_S8:
        case VLC_CODEC_S16N:
        case VLC_CODEC_S24N:
        case VLC_CODEC_S32N:
            wf->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
#endif
        default:
            audio->i_format = VLC_CODEC_FL32;
            audio->i_rate = 48000;
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
     if (audio->i_physical_channels & AOUT_CHAN_LEFT)
         wf->dwChannelMask |= SPEAKER_FRONT_LEFT;
     if (audio->i_physical_channels & AOUT_CHAN_RIGHT)
         wf->dwChannelMask |= SPEAKER_FRONT_RIGHT;
     if (audio->i_physical_channels & AOUT_CHAN_CENTER)
         wf->dwChannelMask |= SPEAKER_FRONT_CENTER;
     if (audio->i_physical_channels & AOUT_CHAN_LFE)
         wf->dwChannelMask |= SPEAKER_LOW_FREQUENCY;
     // TODO: reorder
     if (audio->i_physical_channels & AOUT_CHAN_REARLEFT)
         wf->dwChannelMask |= SPEAKER_BACK_LEFT;
     if (audio->i_physical_channels & AOUT_CHAN_REARRIGHT)
         wf->dwChannelMask |= SPEAKER_BACK_RIGHT;
     /* ... */
     if (audio->i_physical_channels & AOUT_CHAN_REARCENTER)
         wf->dwChannelMask |= SPEAKER_BACK_CENTER;
     if (audio->i_physical_channels & AOUT_CHAN_MIDDLELEFT)
         wf->dwChannelMask |= SPEAKER_SIDE_LEFT;
     if (audio->i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
         wf->dwChannelMask |= SPEAKER_SIDE_RIGHT;
     /* ... */
}

static int vlc_FromWave(const WAVEFORMATEX *restrict wf,
                        audio_sample_format_t *restrict audio)
{
    /* FIXME? different sample format? possible? */
    audio->i_rate = wf->nSamplesPerSec;
    /* FIXME */
    if (wf->nChannels != audio->i_channels)
        return -1;

    aout_FormatPrepare(audio);
    return 0;
}

/* Dummy thread to keep COM MTA alive */
static void MTAThread(void *data)
{
    HANDLE done = data;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
        abort();
    WaitForSingleObject(done, INFINITE);
    CoUninitialize();
    CloseHandle(done);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    HRESULT hr;

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->client = NULL;
    sys->render = NULL;
    sys->done = NULL;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        free(sys);
        return VLC_EGENERIC;
    }

    /* Select audio device */
    IMMDeviceEnumerator *devs;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&devs);
    if (FAILED(hr))
    {
        msg_Dbg(aout, "cannot create device enumerator (error 0x%lx)", hr);
        goto error;
    }

    /* TODO: support selecting a device from config? */
    IMMDevice *dev;
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devs, eRender,
                                                     eConsole, &dev);
    IMMDeviceEnumerator_Release(devs);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get audio endpoint (error 0x%lx)", hr);
        goto error;
    }

    LPWSTR str;
    hr = IMMDevice_GetId(dev, &str);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(aout, "using device %ls", str);
        CoTaskMemFree(str);
    }

    hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL,
                            (void **)&sys->client);
    IMMDevice_Release(dev);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot activate audio client (error 0x%lx)", hr);
        goto error;
    }

    /* Configure audio stream */
    audio_sample_format_t format = aout->format;
    WAVEFORMATEXTENSIBLE wf;
    WAVEFORMATEX *pwf;

    vlc_ToWave(&wf, &format);
    hr = IAudioClient_IsFormatSupported(sys->client, AUDCLNT_SHAREMODE_SHARED,
                                        &wf.Format, &pwf);
    // TODO: deal with (hr == AUDCLNT_E_DEVICE_INVALIDATED) ?
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot negotiate audio format (error 0x%lx)", hr);
        goto error;
    }

    if (hr == S_FALSE)
    {
        assert(pwf != NULL);
        if (vlc_FromWave(pwf, &format))
        {
            CoTaskMemFree(pwf);
            msg_Err(aout, "unsupported audio format");
            goto error;
        }
        msg_Dbg(aout, "modified format");
    }
    else
        assert(pwf == NULL);
    hr = IAudioClient_Initialize(sys->client, AUDCLNT_SHAREMODE_SHARED, 0,
                                 AOUT_MAX_PREPARE_TIME * 10, 0,
                                 (hr == S_OK) ? &wf.Format : pwf, NULL);
    CoTaskMemFree(pwf);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot initialize audio client (error 0x%lx)", hr);
        goto error;
    }

    hr = IAudioClient_GetBufferSize(sys->client, &sys->frames);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get buffer size (error 0x%lx)", hr);
        goto error;
    }

    hr = IAudioClient_GetService(sys->client, &IID_IAudioRenderClient,
                                 (void **)&sys->render);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get audio render service (error 0x%lx)", hr);
        goto error;
    }

    sys->done = CreateSemaphore(NULL, 0, 1, NULL);
    if (unlikely(sys->done == NULL))
        goto error;
    /* Note: thread handle released by CRT, ignore it. */
    if (_beginthread(MTAThread, 0, sys->done) == (uintptr_t)-1)
        goto error;

    aout->format = format;
    aout->sys = sys;
    aout->pf_play = Play;
    aout->pf_pause = Pause;
    aout->pf_flush = Flush;
    aout_VolumeNoneInit (aout);
    CoUninitialize();
    return VLC_SUCCESS;
error:
    if (sys->done != NULL)
        CloseHandle(sys->done);
    if (sys->render != NULL)
        IAudioRenderClient_Release(sys->render);
    if (sys->client != NULL)
        IAudioClient_Release(sys->client);
    CoUninitialize();
    free(sys);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IAudioRenderClient_Release(sys->render);
    IAudioClient_Release(sys->client);
    CoUninitialize();

    ReleaseSemaphore(sys->done, 1, NULL); /* MTA thread will exit */
    free(sys);
}
