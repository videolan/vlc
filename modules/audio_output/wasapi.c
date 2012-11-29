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
# include "config.h"
#endif

#define INITGUID
#define COBJMACROS
#define CONST_VTABLE

#include <stdlib.h>
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

DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd,
   0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);

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
    audio_output_t *aout;
    IMMDeviceEnumerator *it;
    IAudioClient *client;
    IAudioRenderClient *render;
    IAudioClock *clock;

    IAudioSessionControl *control;
    struct IAudioSessionEvents events;
    LONG refs;

    uint8_t chans_table[AOUT_CHAN_MAX];
    uint8_t chans_to_reorder;
    uint8_t bits; /**< Bits per sample */
    unsigned rate; /**< Sample rate */
    unsigned bytes_per_frame;
    UINT32 written; /**< Frames written to the buffer */
    UINT32 frames; /**< Total buffer size (frames) */

    float volume_hack; /**< Deferred volume request */
    int mute_hack; /**< Deferred mute request */

    HANDLE ready; /**< Semaphore from MTA thread */
    HANDLE done; /**< Semaphore to MTA thread */
};


/*** VLC audio output callbacks ***/
static int TimeGet(audio_output_t *aout, mtime_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    UINT64 pos, qpcpos;
    HRESULT hr;

    if (sys->clock == NULL)
        return -1;

    Enter();
    hr = IAudioClock_GetPosition(sys->clock, &pos, &qpcpos);
    Leave();
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get position (error 0x%lx)", hr);
        return -1;
    }

    if (pos == 0)
    {
        *delay = sys->written * CLOCK_FREQ / sys->rate;
        msg_Dbg(aout, "extrapolating position: still propagating buffers");
        return 0;
    }

    *delay = ((GetQPC() - qpcpos) / (10000000 / CLOCK_FREQ));
    static_assert((10000000 % CLOCK_FREQ) == 0, "Frequency conversion broken");
    return 0;
}

static void CheckVolumeHack(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    if (unlikely(sys->volume_hack >= 0.f))
    {   /* Apply volume now, if it failed earlier */
        aout->volume_set(aout, sys->volume_hack);
        sys->volume_hack = -1.f;
    }
    if (unlikely(sys->mute_hack >= 0))
    {   /* Apply volume now, if it failed earlier */
        aout->mute_set(aout, sys->mute_hack);
        sys->mute_hack = -1;
    }
}

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr = S_OK;

    CheckVolumeHack(aout);

    if (sys->chans_to_reorder)
        aout_ChannelReorder(block->p_buffer, block->i_buffer,
                          sys->chans_to_reorder, sys->chans_table, sys->bits);

    Enter();
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

        const size_t copy = frames * sys->bytes_per_frame;

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
        sys->written += frames;
        if (block->i_nb_samples == 0)
            break; /* done */

        /* Out of buffer space, sleep */
        msleep(AOUT_MIN_PREPARE_TIME
             + block->i_nb_samples * CLOCK_FREQ / sys->rate);
    }

    Leave();
    block_Release(block);

    /* Restart on unplug */
    if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED))
        var_TriggerCallback(aout, "audio-device");
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    CheckVolumeHack(aout);

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

    CheckVolumeHack(aout);

    if (wait)
        return; /* Drain not implemented */

    Enter();
    IAudioClient_Stop(sys->client);
    hr = IAudioClient_Reset(sys->client);
    Leave();

    if (FAILED(hr))
        msg_Warn(aout, "cannot reset stream (error 0x%lx)", hr);
    else
        sys->written = 0;
}

static int SimpleVolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    ISimpleAudioVolume *simple;
    HRESULT hr;

    if (TryEnter(aout))
        return -1;
    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume,
                                 (void **)&simple);
    if (SUCCEEDED(hr))
    {
        hr = ISimpleAudioVolume_SetMasterVolume(simple, vol, NULL);
        ISimpleAudioVolume_Release(simple);
    }
    Leave();

    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
        sys->volume_hack = vol;
        return -1;
    }
    sys->volume_hack = -1.f;
    return 0;
}

static int SimpleMuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    ISimpleAudioVolume *simple;
    HRESULT hr;

    if (TryEnter(aout))
        return -1;
    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume,
                                 (void **)&simple);
    if (SUCCEEDED(hr))
    {
        hr = ISimpleAudioVolume_SetMute(simple, mute, NULL);
        ISimpleAudioVolume_Release(simple);
    }
    Leave();

    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set mute (error 0x%lx)", hr);
        sys->mute_hack = mute;
        return -1;
    }
    sys->mute_hack = -1;
    return 0;
}


/*** Audio devices ***/
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

    var_Create (obj, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
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
    {
        msg_Warn (obj, "cannot count audio endpoints (error 0x%lx)", hr);
        n = 0;
    }
    else
        msg_Dbg(obj, "Available Windows Audio devices:");

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
            hr = IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &v);
            if (SUCCEEDED(hr))
                text.psz_string = FromWide(v.pwszVal);
            PropVariantClear(&v);
            IPropertyStore_Release(props);
        }
        IMMDevice_Release(dev);

        msg_Dbg(obj, "%s (%s)", val.psz_string, text.psz_string);
        var_Change(obj, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
        if (likely(text.psz_string != val.psz_string))
            free(text.psz_string);
        free(val.psz_string);
    }
    IMMDeviceCollection_Release(devs);
}


/*** Audio session events ***/
static inline aout_sys_t *vlc_AudioSessionEvents_sys(IAudioSessionEvents *this)
{
    return (aout_sys_t *)(((char *)this) - offsetof(aout_sys_t, events));
}

static STDMETHODIMP
vlc_AudioSessionEvents_QueryInterface(IAudioSessionEvents *this, REFIID riid,
                                      void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown)
     || IsEqualIID(riid, &IID_IAudioSessionEvents))
    {
        *ppv = this;
        IUnknown_AddRef(this);
        return S_OK;
    }
    else
    {
       *ppv = NULL;
        return E_NOINTERFACE;
    }
}

static STDMETHODIMP_(ULONG)
vlc_AudioSessionEvents_AddRef(IAudioSessionEvents *this)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    return InterlockedIncrement(&sys->refs);
}

static STDMETHODIMP_(ULONG)
vlc_AudioSessionEvents_Release(IAudioSessionEvents *this)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    return InterlockedDecrement(&sys->refs);
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnDisplayNameChanged(IAudioSessionEvents *this,
                                            LPCWSTR wname, LPCGUID ctx)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "display name changed: %ls", wname);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnIconPathChanged(IAudioSessionEvents *this,
                                         LPCWSTR wpath, LPCGUID ctx)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "icon path changed: %ls", wpath);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnSimpleVolumeChanged(IAudioSessionEvents *this, float vol,
                                             WINBOOL mute, LPCGUID ctx)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "simple volume changed: %f, muting %sabled", vol,
            mute ? "en" : "dis");
    aout_VolumeReport(aout, vol);
    aout_MuteReport(aout, mute == TRUE);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnChannelVolumeChanged(IAudioSessionEvents *this,
                                              DWORD count, float *vols,
                                              DWORD changed, LPCGUID ctx)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "channel volume %lu of %lu changed: %f", changed, count,
            vols[changed]);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnGroupingParamChanged(IAudioSessionEvents *this,
                                              LPCGUID param, LPCGUID ctx)

{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "grouping parameter changed");
    (void) param;
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnStateChanged(IAudioSessionEvents *this,
                                      AudioSessionState state)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "state changed: %d", state);
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnSessionDisconnected(IAudioSessionEvents *this,
                                             AudioSessionDisconnectReason reason)
{
    aout_sys_t *sys = vlc_AudioSessionEvents_sys(this);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "session disconnected: reason %d", reason);
    return S_OK;
}

static const struct IAudioSessionEventsVtbl vlc_AudioSessionEvents =
{
    vlc_AudioSessionEvents_QueryInterface,
    vlc_AudioSessionEvents_AddRef,
    vlc_AudioSessionEvents_Release,

    vlc_AudioSessionEvents_OnDisplayNameChanged,
    vlc_AudioSessionEvents_OnIconPathChanged,
    vlc_AudioSessionEvents_OnSimpleVolumeChanged,
    vlc_AudioSessionEvents_OnChannelVolumeChanged,
    vlc_AudioSessionEvents_OnGroupingParamChanged,
    vlc_AudioSessionEvents_OnStateChanged,
    vlc_AudioSessionEvents_OnSessionDisconnected,
};


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
    if (sys->clock != NULL)
        IAudioClock_Release(sys->clock);
    IAudioRenderClient_Release(sys->render);
fail:
    Leave();
    ReleaseSemaphore(sys->ready, 1, NULL);
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    sys->client = NULL;
    sys->render = NULL;
    sys->clock = NULL;
    sys->events.lpVtbl = &vlc_AudioSessionEvents;
    sys->refs = 1;
    sys->ready = NULL;
    sys->done = NULL;

    Enter();
retry:
    /* Get audio device according to policy */
    // Without configuration item, the variable must be created explicitly.
    var_Create (aout, "wasapi-audio-device", VLC_VAR_STRING);
    LPWSTR devid = var_InheritWide (aout, "wasapi-audio-device");
    var_Destroy (aout, "wasapi-audio-device");

    IMMDevice *dev = NULL;
    if (devid != NULL)
    {
        msg_Dbg (aout, "using selected device %ls", devid);
        hr = IMMDeviceEnumerator_GetDevice (sys->it, devid, &dev);
        if (FAILED(hr))
            msg_Warn(aout, "cannot get audio endpoint (error 0x%lx)", hr);
        free (devid);
    }
    if (dev == NULL)
    {
        msg_Dbg (aout, "using default device");
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(sys->it, eRender,
                                                         eConsole, &dev);
    }
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
    WAVEFORMATEXTENSIBLE wf;
    WAVEFORMATEX *pwf;

    vlc_ToWave(&wf, fmt);
    hr = IAudioClient_IsFormatSupported(sys->client, AUDCLNT_SHAREMODE_SHARED,
                                        &wf.Format, &pwf);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot negotiate audio format (error 0x%lx)", hr);
        goto error;
    }

    if (hr == S_FALSE)
    {
        assert(pwf != NULL);
        if (vlc_FromWave(pwf, fmt))
        {
            CoTaskMemFree(pwf);
            msg_Err(aout, "unsupported audio format");
            goto error;
        }
        msg_Dbg(aout, "modified format");
    }
    else
        assert(pwf == NULL);

    sys->chans_to_reorder = vlc_CheckWaveOrder((hr == S_OK) ? &wf.Format : pwf,
                                               sys->chans_table);
    sys->bits = fmt->i_bitspersample;

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

    sys->rate = fmt->i_rate;
    sys->bytes_per_frame = fmt->i_bytes_per_frame;
    sys->written = 0;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    if (likely(sys->control != NULL))
       IAudioSessionControl_RegisterAudioSessionNotification(sys->control,
                                                             &sys->events);
    var_AddCallback (aout, "audio-device", DeviceChanged, NULL);

    return VLC_SUCCESS;
error:
    if (sys->done != NULL)
        CloseHandle(sys->done);
    if (sys->ready != NULL)
        CloseHandle(sys->done);
    if (sys->client != NULL)
        IAudioClient_Release(sys->client);
    if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
    {
        var_SetString(aout, "audio-device", "");
        msg_Warn(aout, "device invalidated, retrying");
        goto retry;
    }
    Leave();
    return VLC_EGENERIC;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    Enter();
    if (likely(sys->control != NULL))
       IAudioSessionControl_UnregisterAudioSessionNotification(sys->control,
                                                               &sys->events);
    ReleaseSemaphore(sys->done, 1, NULL); /* tell MTA thread to finish */
    WaitForSingleObject(sys->ready, INFINITE); /* wait for that ^ */
    IAudioClient_Stop(sys->client); /* should not be needed */
    IAudioClient_Release(sys->client);
    Leave();

    var_DelCallback (aout, "audio-device", DeviceChanged, NULL);

    CloseHandle(sys->done);
    CloseHandle(sys->ready);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    void *pv;
    HRESULT hr;

    if (!aout->b_force && var_InheritBool(aout, "spdif"))
        /* Fallback to other plugin until pass-through is implemented */
        return VLC_EGENERIC;

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->aout = aout;

    if (TryEnter(aout))
        goto error;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, &pv);
    if (FAILED(hr))
    {
        msg_Dbg(aout, "cannot create device enumerator (error 0x%lx)", hr);
        Leave();
        goto error;
    }
    sys->it = pv;
    GetDevices(obj, sys->it);
    Leave();

    sys->volume_hack = -1.f;
    sys->mute_hack = -1;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = SimpleVolumeSet; /* FIXME */
    aout->mute_set = SimpleMuteSet;
    return VLC_SUCCESS;
error:
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    var_Destroy (aout, "audio-device");

    Enter();
    IMMDeviceEnumerator_Release(sys->it);
    Leave();

    free(sys);
}
