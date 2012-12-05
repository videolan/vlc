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

static int TryEnterMTA(vlc_object_t *obj)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
    {
        msg_Err (obj, "cannot initialize COM (error 0x%lx)", hr);
        return -1;
    }
    return 0;
}
#define TryEnterMTA(o) TryEnterMTA(VLC_OBJECT(o))

static void EnterMTA(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
        abort();
}

static void LeaveMTA(void)
{
    CoUninitialize();
}

struct aout_sys_t
{
    audio_output_t *aout;
    aout_api_t *api; /**< Audio output back-end API */

    IMMDeviceEnumerator *it; /**< Device enumerator, NULL when exiting */
    /*TODO: IMMNotificationClient*/

    IMMDevice *dev; /**< Selected output device, NULL if none */
    IAudioSessionManager *manager; /**< Session for the output device */
    struct IAudioSessionEvents session_events;

    LONG refs;
    CRITICAL_SECTION lock; /**< Lock to protect Core Audio API state */
    HANDLE device_changed; /**< Event to reset thread */
    vlc_thread_t thread; /**< Thread for audio session control */
};

/* NOTE: The Core Audio API documentation totally fails to specify the thread
 * safety (or lack thereof) of the interfaces. This code is most pessimistic
 * and assumes that the API is not thread-safe at all.
 *
 * The audio output owner (i.e. the audio output core) is responsible for
 * serializing callbacks. This code only needs to be concerned with
 * synchronization between the set of audio output callbacks, the thread
 * and (trivially) the device and session notifications. */

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
    HRESULT hr;

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    hr = aout_api_TimeGet(sys->api, delay);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    hr = aout_api_Play(sys->api, block);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    vlc_FromHR(aout, hr);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    hr = aout_api_Pause(sys->api, paused);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    vlc_FromHR(aout, hr);
    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    mtime_t delay = VLC_TS_INVALID;

    EnterMTA();
    EnterCriticalSection(&sys->lock);

    if (wait)
    {   /* Loosy drain emulation */
        if (FAILED(aout_api_TimeGet(sys->api, &delay)))
            delay = VLC_TS_INVALID;
    }
    else
        aout_api_Flush(sys->api);

    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    if (delay != VLC_TS_INVALID)
        Sleep((delay / (CLOCK_FREQ / 1000)) + 1);
}

static ISimpleAudioVolume *GetSimpleVolume(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    ISimpleAudioVolume *volume;
    HRESULT hr;

    if (sys->manager == NULL)
        return NULL;

    if (TryEnterMTA(aout))
        return NULL;
    EnterCriticalSection(&sys->lock);
    hr = IAudioSessionManager_GetSimpleAudioVolume(sys->manager,
                                                   &GUID_VLC_AUD_OUT,
                                                   FALSE, &volume);
    if (FAILED(hr))
    {
        LeaveCriticalSection(&sys->lock);
        LeaveMTA();
        msg_Err(aout, "cannot get simple volume (error 0x%lx)", hr);
        return NULL;
    }
    return volume;
}

static void PutSimpleVolume(audio_output_t *aout, ISimpleAudioVolume *volume)
{
    aout_sys_t *sys = aout->sys;

    ISimpleAudioVolume_Release(volume);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    ISimpleAudioVolume *volume = GetSimpleVolume(aout);
    if (volume == NULL)
        return -1;

    HRESULT hr = ISimpleAudioVolume_SetMasterVolume(volume, vol, NULL);
    if (FAILED(hr))
        msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
    PutSimpleVolume(aout, volume);

    return FAILED(hr) ? -1 : 0;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    ISimpleAudioVolume *volume = GetSimpleVolume(aout);
    if (volume == NULL)
        return -1;

    HRESULT hr = ISimpleAudioVolume_SetMute(volume, mute ? TRUE : FALSE, NULL);
    if (FAILED(hr))
        msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
    PutSimpleVolume(aout, volume);

    return FAILED(hr) ? -1 : 0;
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

static void MMSession(audio_output_t *aout, aout_sys_t *sys)
{
    IAudioSessionControl *control;
    HRESULT hr;

    /* Register session control */
    msg_Err(aout, "HERE: %p", sys->manager);
    if (sys->manager != NULL)
    {
        hr = IAudioSessionManager_GetAudioSessionControl(sys->manager,
                                                         &GUID_VLC_AUD_OUT, 0,
                                                         &control);
        if (FAILED(hr))
            msg_Err(aout, "cannot get session control (error 0x%lx)", hr);
    }
    else
        control = NULL;

    msg_Err(aout, "THERE");
    if (control != NULL)
    {
        wchar_t *ua = var_InheritWide(aout, "user-agent");
        IAudioSessionControl_SetDisplayName(control, ua, NULL);
        free(ua);

        IAudioSessionControl_RegisterAudioSessionNotification(control,
                                                         &sys->session_events);
    }

    LeaveCriticalSection(&sys->lock);
    WaitForSingleObject(sys->device_changed, INFINITE);
    EnterCriticalSection(&sys->lock);

    /* Deregister session control */
    if (control != NULL)
    {
        IAudioSessionControl_UnregisterAudioSessionNotification(control,
                                                         &sys->session_events);
        IAudioSessionControl_Release(control);
    }
}

/** MMDevice audio output thread.
 * This thread takes cares of the audio session control. Inconveniently enough,
 * the audio session control interface must:
 *  - be created and destroyed from the same thread, and
 *  - survive across VLC audio output calls.
 * The only way to reconcile both requirements is a custom thread.
 * The thread also ensure that the COM Multi-Thread Apartment is continuously
 * referenced so that MMDevice objects are not destroyed early.
 */
static void *MMThread(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    while (sys->it != NULL)
        MMSession(aout, sys);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();
    return NULL;
}

/**
 * Opens the selected audio output device.
 */
static HRESULT OpenDevice(audio_output_t *aout, const char *devid)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterCriticalSection(&sys->lock);
    if (devid != NULL) /* Device selected explicitly */
    {
        msg_Dbg(aout, "using selected device %s", devid);

        wchar_t *wdevid = ToWide(devid);
        if (likely(wdevid != NULL))
        {
            hr = IMMDeviceEnumerator_GetDevice(sys->it, wdevid, &sys->dev);
            free (wdevid);
        }
        else
            hr = E_OUTOFMEMORY;
    }
    else /* Default device selected by policy */
    {
        msg_Dbg(aout, "using default device");
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(sys->it, eRender,
                                                         eConsole, &sys->dev);
    }

    /* Create session manager (for controls even w/o active audio client) */
    if (FAILED(hr))
        msg_Err(aout, "cannot get device (error 0x%lx)", hr);
    else
    {
        void *pv;
        hr = IMMDevice_Activate(sys->dev, &IID_IAudioSessionManager,
                                CLSCTX_ALL, NULL, &pv);
        if (FAILED(hr))
            msg_Err(aout, "cannot activate session manager (error 0x%lx)", hr);
        else
            sys->manager = pv;
    }
    LeaveCriticalSection(&sys->lock);

    SetEvent(sys->device_changed);
    return hr;
}

/**
 * Closes the opened audio output device (if any).
 */
static void CloseDevice(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert(sys->dev != NULL);

    EnterCriticalSection(&sys->lock);
    if (sys->manager != NULL)
    {
        IAudioSessionManager_Release(sys->manager);
        sys->manager = NULL;
    }

    IMMDevice_Release(sys->dev);
    sys->dev = NULL;
    LeaveCriticalSection(&sys->lock);
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->api == NULL);
    if (sys->dev == NULL)
        return -1;

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    sys->api = aout_api_Start(aout, fmt, sys->dev, &GUID_VLC_AUD_OUT);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    return (sys->api != NULL) ? 0 : -1;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->api != NULL);

    EnterMTA();
    EnterCriticalSection(&sys->lock);
    aout_api_Stop(sys->api);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

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

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    sys->aout = aout;
    sys->api = NULL;
    sys->it = NULL;
    sys->session_events.lpVtbl = &vlc_AudioSessionEvents;
    sys->refs = 1;

    InitializeCriticalSection(&sys->lock);
    sys->device_changed = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (unlikely(sys->device_changed == NULL))
        goto error;

    /* Initialize MMDevice API */
    if (TryEnterMTA(aout))
        goto error;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, &pv);
    if (FAILED(hr))
    {
        LeaveMTA();
        msg_Dbg(aout, "cannot create device enumerator (error 0x%lx)", hr);
        goto error;
    }
    sys->it = pv;
    GetDevices(obj, sys->it);

    if (vlc_clone(&sys->thread, MMThread, aout, VLC_THREAD_PRIORITY_LOW))
        goto error;

    /* Get a device to start with */
    do
        hr = OpenDevice(aout, NULL);
    while (hr == AUDCLNT_E_DEVICE_INVALIDATED);
    LeaveMTA(); /* leave MTA after thread has entered MTA */

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
    if (sys->it != NULL)
    {
        IMMDeviceEnumerator_Release(sys->it);
        LeaveMTA();
    }
    if (sys->device_changed != NULL)
        CloseHandle(sys->device_changed);
    DeleteCriticalSection(&sys->lock);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    var_DelCallback (aout, "audio-device", DeviceChanged, NULL);
    var_Destroy (aout, "audio-device");

    EnterMTA(); /* enter MTA before thread leaves MTA */
    EnterCriticalSection(&sys->lock);
    if (sys->dev != NULL)
        CloseDevice(aout);

    IMMDeviceEnumerator_Release(sys->it);
    sys->it = NULL;
    LeaveCriticalSection(&sys->lock);

    SetEvent(sys->device_changed);
    vlc_join(sys->thread, NULL);
    LeaveMTA();

    CloseHandle(sys->device_changed);
    DeleteCriticalSection(&sys->lock);
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
