/*****************************************************************************
 * mmdevice.c : Windows Multimedia Device API audio output plugin for VLC
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

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x600 /* Windows Vista */
#define INITGUID
#define COBJMACROS
#define CONST_VTABLE

#include <stdlib.h>
#include <assert.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd,
   0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>
#include "mmdevice.h"

DEFINE_GUID (GUID_VLC_AUD_OUT, 0x4533f59d, 0x59ee, 0x00c6,
   0xad, 0xb2, 0xc6, 0x8b, 0x50, 0x1a, 0x66, 0x55);

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
    aout_api_t *api; /**< Audio output back-end API */
    IMMDeviceEnumerator *it;
    IMMDevice *dev; /**< Selected output device */
    IAudioSessionManager *manager; /**< Session for the output device */

    /*TODO: IMMNotificationClient*/
    struct IAudioSessionEvents session_events;

    LONG refs;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE request_wait;
    CONDITION_VARIABLE reply_wait;

    bool killed; /**< Flag to terminate the thread */
    bool running; /**< Whether the thread is running */
    int8_t mute; /**< Requested mute state or negative value */
    float volume; /**< Requested volume or negative value */
};

static int vlc_FromHR(audio_output_t *aout, HRESULT hr)
{
    /* Restart on unplug */
    if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED))
        var_TriggerCallback(aout, "audio-device");
    return SUCCEEDED(hr) ? 0 : -1;
}

/*** VLC audio output callbacks ***/
static int TimeGet(audio_output_t *aout, mtime_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr = aout_api_TimeGet(sys->api, delay);

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr = aout_api_Play(sys->api, block);

    vlc_FromHR(aout, hr);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr = aout_api_Pause(sys->api, paused);

    vlc_FromHR(aout, hr);
    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;

    if (wait)
        return; /* Drain not implemented */

    aout_api_Flush(sys->api);
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    sys->volume = vol;
    LeaveCriticalSection(&sys->lock);

    WakeConditionVariable(&sys->request_wait);
    return 0;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    sys->mute = mute;
    LeaveCriticalSection(&sys->lock);

    WakeConditionVariable(&sys->request_wait);
    return 0;
}

/*** Audio devices ***/
static int DeviceChanged(vlc_object_t *obj, const char *varname,
                         vlc_value_t prev, vlc_value_t cur, void *data)
{
    /* FIXME: This does not work. sys->dev, sys->manager and sys->api must be
     * recreated. Those pointers are protected by the aout lock, which
     * serializes accesses to the audio_output_t. Unfortunately,
     * aout lock cannot be taken from a variable callback.
     * Solution: add device_change callback to audio_output_t. */
    aout_ChannelsRestart(obj, varname, prev, cur, data);
    return VLC_SUCCESS;
}

static void GetDevices(vlc_object_t *obj, IMMDeviceEnumerator *it)
{
    HRESULT hr;
    vlc_value_t val, text;

    var_Create (obj, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Audio Device");
    var_Change (obj, "audio-device", VLC_VAR_SETTEXT, &text, NULL);

    /* TODO: implement IMMNotificationClient for hotplug devices */
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
    return (void *)(((char *)this) - offsetof(aout_sys_t, session_events));
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
vlc_AudioSessionEvents_OnSimpleVolumeChanged(IAudioSessionEvents *this,
                                             float vol, WINBOOL mute,
                                             LPCGUID ctx)
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

/** MMDevice audio output thread.
 * A number of Core Audio Interfaces must be deleted from the same thread than
 * they were created from... */
static void MMThread(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;
    IAudioSessionControl *control;
    ISimpleAudioVolume *volume;
    HRESULT hr;

    Enter();
    /* Instantiate thread-invariable interfaces */
    hr = IAudioSessionManager_GetAudioSessionControl(sys->manager,
                                                     &GUID_VLC_AUD_OUT, 0,
                                                     &control);
    if (FAILED(hr))
        msg_Warn(aout, "cannot get session control (error 0x%lx)", hr);
    else
    {
        wchar_t *ua = var_InheritWide(aout, "user-agent");
        IAudioSessionControl_SetDisplayName(control, ua, NULL);
        free(ua);

        sys->session_events.lpVtbl = &vlc_AudioSessionEvents;
        IAudioSessionControl_RegisterAudioSessionNotification(control,
                                                         &sys->session_events);
    }

    hr = IAudioSessionManager_GetSimpleAudioVolume(sys->manager,
                                                   &GUID_VLC_AUD_OUT, FALSE,
                                                   &volume);
    if (FAILED(hr))
        msg_Err(aout, "cannot get simple volume (error 0x%lx)", hr);

    EnterCriticalSection(&sys->lock);
    sys->running = true;
    WakeConditionVariable(&sys->reply_wait);

    while (!sys->killed)
    {
        /* Update volume */
        if (sys->volume >= 0.f)
        {
            hr = ISimpleAudioVolume_SetMasterVolume(volume, sys->volume, NULL);
            if (FAILED(hr))
                msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
            sys->volume = -1.f;
        }

        /* Update mute state */
        if (sys->mute >= 0)
        {
            hr = ISimpleAudioVolume_SetMute(volume, sys->mute, NULL);
            if (FAILED(hr))
                msg_Err(aout, "cannot set mute (error 0x%lx)", hr);
            sys->mute = -1;
        }

        SleepConditionVariableCS(&sys->request_wait, &sys->lock, INFINITE);
    }
    LeaveCriticalSection(&sys->lock);

    if (volume != NULL)
        ISimpleAudioVolume_Release(volume);
    if (control != NULL)
    {
        IAudioSessionControl_UnregisterAudioSessionNotification(control,
                                                         &sys->session_events);
        IAudioSessionControl_Release(control);
    }
    Leave();

    EnterCriticalSection(&sys->lock);
    sys->running = false;
    LeaveCriticalSection(&sys->lock);
    WakeConditionVariable(&sys->reply_wait);
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->api == NULL);
    if (sys->dev == NULL)
        return -1;

    sys->api = aout_api_Start(aout, fmt, sys->dev, &GUID_VLC_AUD_OUT);
    return (sys->api != NULL) ? 0 : -1;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->api != NULL);
    aout_api_Stop(sys->api);
    sys->api = NULL;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    void *pv;
    HRESULT hr;

    if (!aout->b_force && var_InheritBool(aout, "spdif"))
        /* Fallback to other plugin until pass-through is implemented */
        return VLC_EGENERIC;

    /* Initialize MMDevice API */
    if (TryEnter(aout))
        return VLC_EGENERIC;

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->aout = aout;
    sys->api = NULL;
    sys->it = NULL;
    sys->dev = NULL;
    sys->manager = NULL;
    sys->refs = 1;
    InitializeCriticalSection(&sys->lock);
    InitializeConditionVariable(&sys->request_wait);
    InitializeConditionVariable(&sys->reply_wait);
    sys->killed = false;
    sys->running = false;
    sys->volume = -1.f;
    sys->mute = -1;

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

    /* Get audio device according to policy */
    wchar_t *devid = var_InheritWide(aout, "audio-device");
    if (devid != NULL)
    {
        msg_Dbg (aout, "using selected device %ls", devid);
        hr = IMMDeviceEnumerator_GetDevice (sys->it, devid, &sys->dev);
        if (FAILED(hr))
            msg_Err(aout, "cannot get device %ls (error 0x%lx)", devid, hr);
        free (devid);
    }
    if (sys->dev == NULL)
    {
        msg_Dbg (aout, "using default device");
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(sys->it, eRender,
                                                         eConsole, &sys->dev);
        if (FAILED(hr))
            msg_Err(aout, "cannot get default device (error 0x%lx)", hr);
    }
    if (sys->dev == NULL)
        /* TODO: VLC should be able to start without devices, so long as
         * a device becomes available before Start() is called. */
        goto error;

    hr = IMMDevice_Activate(sys->dev, &IID_IAudioSessionManager,
                            CLSCTX_ALL, NULL, &pv);
    if (FAILED(hr))
        msg_Err(aout, "cannot activate session manager (error 0x%lx)", hr);
    else
        sys->manager = pv;

    /* Note: thread handle released by CRT, ignore it. */
    if (_beginthread(MMThread, 0, aout) == (uintptr_t)-1)
        goto error;

    EnterCriticalSection(&sys->lock);
    while (!sys->running)
        SleepConditionVariableCS(&sys->reply_wait, &sys->lock, INFINITE);
    LeaveCriticalSection(&sys->lock);
    Leave();

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    var_AddCallback (aout, "audio-device", DeviceChanged, NULL);
    return VLC_SUCCESS;
error:
    if (sys->manager != NULL)
        IAudioSessionManager_Release(sys->manager);
    if (sys->dev != NULL)
        IMMDevice_Release(sys->dev);
    if (sys->it != NULL)
        IMMDeviceEnumerator_Release(sys->it);
    DeleteCriticalSection(&sys->lock);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    sys->killed = true;
    WakeConditionVariable(&sys->request_wait);
    while (sys->running)
        SleepConditionVariableCS(&sys->reply_wait, &sys->lock, INFINITE);
    LeaveCriticalSection(&sys->lock);

    var_DelCallback (aout, "audio-device", DeviceChanged, NULL);
    var_Destroy (aout, "audio-device");

    Enter();

    if (sys->manager != NULL)
        IAudioSessionManager_Release(sys->manager);
    IMMDevice_Release(sys->dev);
    IMMDeviceEnumerator_Release(sys->it);
    Leave();

    free(sys);
}

vlc_module_begin()
    set_shortname("MMDevice")
    set_description(N_("Windows Multimedia Device output"))
    set_capability("audio output", 150)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
