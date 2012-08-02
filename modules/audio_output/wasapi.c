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
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>

DEFINE_GUID (GUID_VLC_AUD_OUT, 0x4533f59d, 0x59ee, 0x00c6,
   0xad, 0xb2, 0xc6, 0x8b, 0x50, 0x1a, 0x66, 0x55);

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

static int TryEnter(vlc_object_t *obj)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
    {
        msg_Err (obj, "cannot initialize COM (error 0x%lx)", hr);
        return -1;
    }
    return 0;
}
#define TryEnter(o) TryEnter(VLC_OBJECT(o))

static void Enter(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
        abort();
}

static void Leave(void)
{
    CoUninitialize();
}

struct aout_sys_t
{
    IAudioClient *client;
    IAudioRenderClient *render;
    IAudioClock *clock;
    union
    {
        ISimpleAudioVolume *simple;
    } volume;
    IAudioSessionControl *control;
    UINT32 frames; /**< Total buffer size (frames) */
    HANDLE ready; /**< Semaphore from MTA thread */
    HANDLE done; /**< Semaphore to MTA thread */
};

static void Play(audio_output_t *aout, block_t *block, mtime_t *restrict drift)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    Enter();
    if (likely(sys->clock != NULL))
    {
        UINT64 pos, qpcpos;

        /* NOTE: this assumes mdate() uses QPC() (which it currently does). */
        hr = IAudioClock_GetPosition(sys->clock, &pos, &qpcpos);
        if (SUCCEEDED(hr))
        {
            qpcpos = (qpcpos + 5) / 10; /* 100ns -> 1µs */
            *drift = mdate() - qpcpos;
        }
        else
            msg_Warn(aout, "cannot get position (error 0x%lx)", hr);
    }

    for (;;)
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
        IAudioClient_Start(sys->client);

        block->p_buffer += copy;
        block->i_buffer -= copy;
        block->i_nb_samples -= frames;
        if (block->i_nb_samples == 0)
            break; /* done */

        /* Out of buffer space, sleep */
        msleep(AOUT_MIN_PREPARE_TIME
             + block->i_nb_samples * CLOCK_FREQ / aout->format.i_rate);
    }

    Leave();
    block_Release(block);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    Enter();
    if (paused)
        hr = IAudioClient_Stop(sys->client);
    else
        hr = IAudioClient_Start(sys->client);
    if (FAILED(hr))
        msg_Warn(aout, "cannot %s stream (error 0x%lx)",
                 paused ? "stop" : "start", hr);
    Leave();
    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (wait)
        return; /* Drain not implemented */

    Enter();
    IAudioClient_Stop(sys->client);
    hr = IAudioClient_Reset(sys->client);
    if (FAILED(hr))
        msg_Warn(aout, "cannot reset stream (error 0x%lx)", hr);
    Leave();
}

static int SimpleVolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (TryEnter(aout))
        return -1;
    hr = ISimpleAudioVolume_SetMasterVolume(sys->volume.simple, vol, NULL);
    if (FAILED(hr))
        msg_Warn(aout, "cannot set session volume (error 0x%lx)", hr);
    Leave();
    return FAILED(hr) ? -1 : 0;
}

static int SimpleMuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (TryEnter(aout))
        return -1;
    hr = ISimpleAudioVolume_SetMute(sys->volume.simple, mute, NULL);
    if (FAILED(hr))
        msg_Warn(aout, "cannot mute session (error 0x%lx)", hr);
    Leave();
    return FAILED(hr) ? -1 : 0;
}

static int DeviceChanged(vlc_object_t *obj, const char *varname,
                         vlc_value_t prev, vlc_value_t cur, void *data)
{
    aout_ChannelsRestart(obj, varname, prev, cur, data);

    if (!var_Type (obj, "wasapi-audio-device"))
        var_Create (obj, "wasapi-audio-device", VLC_VAR_STRING);
    var_SetString (obj, "wasapi-audio-device", cur.psz_string);
    return VLC_SUCCESS;
}

static void GetDevices(vlc_object_t *obj, IMMDeviceEnumerator *it)
{
    HRESULT hr;
    vlc_value_t val, text;

    text.psz_string = _("Audio Device");
    var_Change (obj, "audio-device", VLC_VAR_SETTEXT, &text, NULL);

    IMMDeviceCollection *devs;
    hr = IMMDeviceEnumerator_EnumAudioEndpoints(it, eRender,
                                                DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr))
    {
        msg_Warn (obj, "cannot enumerate audio endpoints (error 0x%lx)", hr);
        return;
    }

    UINT n;
    hr = IMMDeviceCollection_GetCount(devs, &n);
    if (FAILED(hr))
        n = 0;
    while (n > 0)
    {
        IMMDevice *dev;

        hr = IMMDeviceCollection_Item(devs, --n, &dev);
        if (FAILED(hr))
            continue;

        /* Unique device ID */
        LPWSTR devid;
        hr = IMMDevice_GetId(dev, &devid);
        if (FAILED(hr))
        {
            IMMDevice_Release(dev);
            continue;
        }
        val.psz_string = FromWide(devid);
        CoTaskMemFree(devid);
        text.psz_string = val.psz_string;

        /* User-readable device name */
        IPropertyStore *props;
        hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, &props);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT v;

            PropVariantInit(&v);
#ifdef FIXED
            hr = IPropertyStore_GetValue(props, PKEY_Device_FriendlyName, &v);
            if (SUCCEEDED(hr))
                text.psz_string = FromWide(v.pwszVal);
#endif
            PropVariantClear(&v);
            IPropertyStore_Release(props);
        }
        IMMDevice_Release(dev);

        var_Change(obj, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
        if (likely(text.psz_string != val.psz_string))
            free(text.psz_string);
        free(val.psz_string);
    }
    IMMDeviceCollection_Release(devs);
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

        case VLC_CODEC_S8:
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

static wchar_t *var_InheritWide(vlc_object_t *obj, const char *name)
{
    char *v8 = var_InheritString(obj, name);
    if (v8 == NULL)
        return NULL;

    wchar_t *v16 = ToWide(v8);
    free(v8);
    return v16;
}
#define var_InheritWide(o,n) var_InheritWide(VLC_OBJECT(o),n)

static int var_SetWide(vlc_object_t *obj, const char *name, const wchar_t *val)
{
    char *str = FromWide(val);
    if (unlikely(str == NULL))
        return VLC_ENOMEM;

    int ret = var_SetString(obj, name, str);
    free(str);
    return ret;
}
#define var_SetWide(o,n,v) var_SetWide(VLC_OBJECT(o),n,v)

/* Dummy thread to create and release COM interfaces when needed. */
static void MTAThread(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    Enter();

    hr = IAudioClient_GetService(sys->client, &IID_IAudioRenderClient,
                                 (void **)&sys->render);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get audio render service (error 0x%lx)", hr);
        goto fail;
    }

    hr = IAudioClient_GetService(sys->client, &IID_IAudioClock,
                                 (void **)&sys->clock);
    if (FAILED(hr))
        msg_Warn(aout, "cannot get audio clock (error 0x%lx)", hr);

    /*if (AOUT_FMT_LINEAR(&format) && !exclusive)*/
    {
        hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume,
                                     (void **)&sys->volume.simple);
    }

    hr = IAudioClient_GetService(sys->client, &IID_IAudioSessionControl,
                                 (void **)&sys->control);
    if (FAILED(hr))
        msg_Warn(aout, "cannot get audio session control (error 0x%lx)", hr);
    else
    {
        wchar_t *ua = var_InheritWide(aout, "user-agent");
        IAudioSessionControl_SetDisplayName(sys->control, ua, NULL);
        free(ua);
    }

    /* do nothing until the audio session terminates */
    ReleaseSemaphore(sys->ready, 1, NULL);
    WaitForSingleObject(sys->done, INFINITE);

    if (sys->control != NULL)
        IAudioSessionControl_Release(sys->control);
    /*if (AOUT_FMT_LINEAR(&format) && !exclusive)*/
    {
        if (sys->volume.simple != NULL)
            ISimpleAudioVolume_Release(sys->volume.simple);
    }
    if (sys->clock != NULL)
        IAudioClock_Release(sys->clock);
    IAudioRenderClient_Release(sys->render);
fail:
    Leave();
    ReleaseSemaphore(sys->ready, 1, NULL);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    HRESULT hr;

    if (AOUT_FMT_SPDIF(&aout->format) && !aout->b_force
     && var_InheritBool(aout, "spdif"))
        /* Fallback to other plugin until pass-through is implemented */
        return VLC_EGENERIC;

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->client = NULL;
    sys->render = NULL;
    sys->clock = NULL;
    sys->ready = NULL;
    sys->done = NULL;
    aout->sys = sys;

    if (TryEnter(aout))
    {
        free(sys);
        return VLC_EGENERIC;
    }

    /* Get audio device according to policy */
    var_Create (aout, "audio-device", VLC_VAR_STRING|VLC_VAR_HASCHOICE);

    IMMDeviceEnumerator *devs;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&devs);
    if (FAILED(hr))
    {
        msg_Dbg(aout, "cannot create device enumerator (error 0x%lx)", hr);
        goto error;
    }

    // Without configuration item, the variable must be created explicitly.
    var_Create (aout, "wasapi-audio-device", VLC_VAR_STRING);
    LPWSTR devid = var_InheritWide (aout, "wasapi-audio-device");
    var_Destroy (aout, "wasapi-audio-device");

    IMMDevice *dev = NULL;
    if (devid != NULL)
    {
        msg_Dbg (aout, "using selected device %ls", devid);
        hr = IMMDeviceEnumerator_GetDevice (devs, devid, &dev);
        if (FAILED(hr))
            msg_Warn(aout, "cannot get audio endpoint (error 0x%lx)", hr);
        free (devid);
    }
    if (dev == NULL)
    {
        msg_Dbg (aout, "using default device");
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devs, eRender,
                                                         eConsole, &dev);
    }

    GetDevices(VLC_OBJECT(aout), devs);
    IMMDeviceEnumerator_Release(devs);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get audio endpoint (error 0x%lx)", hr);
        goto error;
    }

    hr = IMMDevice_GetId(dev, &devid);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(aout, "using device %ls", devid);
        var_SetWide (aout, "audio-device", devid);
        CoTaskMemFree(devid);
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
                                 (hr == S_OK) ? &wf.Format : pwf,
                                 &GUID_VLC_AUD_OUT);
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

    sys->ready = CreateSemaphore(NULL, 0, 1, NULL);
    sys->done = CreateSemaphore(NULL, 0, 1, NULL);
    if (unlikely(sys->ready == NULL || sys->done == NULL))
        goto error;
    /* Note: thread handle released by CRT, ignore it. */
    if (_beginthread(MTAThread, 0, aout) == (uintptr_t)-1)
        goto error;

    WaitForSingleObject(sys->ready, INFINITE);
    if (sys->render == NULL)
        goto error;

    Leave();

    aout->format = format;
    aout->pf_play = Play;
    aout->pf_pause = Pause;
    aout->pf_flush = Flush;
    /*if (AOUT_FMT_LINEAR(&format) && !exclusive)*/
    {
        aout->volume_set = SimpleVolumeSet;
        aout->mute_set = SimpleMuteSet;
    }
    var_AddCallback (aout, "audio-device", DeviceChanged, NULL);

    return VLC_SUCCESS;
error:
    if (sys->done != NULL)
        CloseHandle(sys->done);
    if (sys->ready != NULL)
        CloseHandle(sys->done);
    if (sys->client != NULL)
        IAudioClient_Release(sys->client);
    Leave();
    var_Destroy (aout, "audio-device");
    free(sys);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    Enter();
    ReleaseSemaphore(sys->done, 1, NULL); /* tell MTA thread to finish */
    WaitForSingleObject(sys->ready, INFINITE); /* wait for that ^ */
    IAudioClient_Stop(sys->client); /* should not be needed */
    IAudioClient_Release(sys->client);
    Leave();

    var_DelCallback (aout, "audio-device", DeviceChanged, NULL);
    var_Destroy (aout, "audio-device");

    CloseHandle(sys->done);
    CloseHandle(sys->ready);
    free(sys);
}
