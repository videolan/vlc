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
    aout_stream_t *stream; /**< Underlying audio output stream */

    IMMDeviceEnumerator *it; /**< Device enumerator, NULL when exiting */
    /*TODO: IMMNotificationClient*/

    IMMDevice *dev; /**< Selected output device, NULL if none */
    IAudioSessionManager *manager; /**< Session for the output device */
    struct IAudioSessionEvents session_events;
    ISimpleAudioVolume *volume; /**< Volume setter */

    LONG refs;
    HANDLE device_changed; /**< Event to reset thread */
    HANDLE device_ready; /**< Event when thread is reset */
    vlc_thread_t thread; /**< Thread for audio session control */
};

/* NOTE: The Core Audio API documentation totally fails to specify the thread
 * safety (or lack thereof) of the interfaces. This code takes the most
 * restrictive assumption, no thread safety: The background thread (MMThread)
 * only runs at specified times, namely between the device_ready and
 * device_changed events (effectively, a thread barrier but only Windows 8
 * provides thread barriers natively).
 *
 * The audio output owner (i.e. the audio output core) is responsible for
 * serializing callbacks. This code only needs to be concerned with
 * synchronization between the set of audio output callbacks, MMThread()
 * and (trivially) the device and session notifications. */

static int vlc_FromHR(audio_output_t *aout, HRESULT hr)
{
    /* Restart on unplug */
    if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED))
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    return SUCCEEDED(hr) ? 0 : -1;
}

/*** VLC audio output callbacks ***/
static int TimeGet(audio_output_t *aout, mtime_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_TimeGet(sys->stream, delay);
    LeaveMTA();

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_Play(sys->stream, block);
    LeaveMTA();

    vlc_FromHR(aout, hr);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_Pause(sys->stream, paused);
    LeaveMTA();

    vlc_FromHR(aout, hr);
    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;

    EnterMTA();

    if (wait)
    {   /* Loosy drain emulation */
        mtime_t delay;

        if (SUCCEEDED(aout_stream_TimeGet(sys->stream, &delay)))
            Sleep((delay / (CLOCK_FREQ / 1000)) + 1);
    }
    else
        aout_stream_Flush(sys->stream);

    LeaveMTA();

}

static int VolumeSet(audio_output_t *aout, float vol)
{
    ISimpleAudioVolume *volume = aout->sys->volume;
    if (volume == NULL)
        return -1;

    if (TryEnterMTA(aout))
        return -1;

    HRESULT hr = ISimpleAudioVolume_SetMasterVolume(volume, vol, NULL);
    if (FAILED(hr))
        msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
    LeaveMTA();

    return FAILED(hr) ? -1 : 0;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    ISimpleAudioVolume *volume = aout->sys->volume;
    if (volume == NULL)
        return -1;

    if (TryEnterMTA(aout))
        return -1;

    HRESULT hr = ISimpleAudioVolume_SetMute(volume, mute ? TRUE : FALSE, NULL);
    if (FAILED(hr))
        msg_Err(aout, "cannot set volume (error 0x%lx)", hr);
    LeaveMTA();

    return FAILED(hr) ? -1 : 0;
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

    switch (reason)
    {
        case DisconnectReasonDeviceRemoval:
            msg_Warn(aout, "session disconnected: %s", "device removed");
            break;
        case DisconnectReasonServerShutdown:
            msg_Err(aout, "session disconnected: %s", "service stopped");
            return S_OK;
        case DisconnectReasonFormatChanged:
            msg_Warn(aout, "session disconnected: %s", "format changed");
            break;
        case DisconnectReasonSessionLogoff:
            msg_Err(aout, "session disconnected: %s", "user logged off");
            return S_OK;
        case DisconnectReasonSessionDisconnected:
            msg_Err(aout, "session disconnected: %s", "session disconnected");
            return S_OK;
        case DisconnectReasonExclusiveModeOverride:
            msg_Err(aout, "session disconnected: %s", "stream overriden");
            return S_OK;
        default:
            msg_Warn(aout, "session disconnected: unknown reason %d", reason);
            return S_OK;
    }
    /* NOTE: audio decoder thread should get invalidated device and restart */
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
    if (sys->manager != NULL)
    {
        hr = IAudioSessionManager_GetSimpleAudioVolume(sys->manager,
                                                       &GUID_VLC_AUD_OUT,
                                                       FALSE, &sys->volume);
        if (FAILED(hr))
            msg_Err(aout, "cannot get simple volume (error 0x%lx)", hr);

        hr = IAudioSessionManager_GetAudioSessionControl(sys->manager,
                                                         &GUID_VLC_AUD_OUT, 0,
                                                         &control);
        if (FAILED(hr))
            msg_Err(aout, "cannot get session control (error 0x%lx)", hr);
    }
    else
    {
        sys->volume = NULL;
        control = NULL;
    }

    if (control != NULL)
    {
        wchar_t *ua = var_InheritWide(aout, "user-agent");
        IAudioSessionControl_SetDisplayName(control, ua, NULL);
        free(ua);

        IAudioSessionControl_RegisterAudioSessionNotification(control,
                                                         &sys->session_events);
    }

    if (sys->volume != NULL)
    {   /* Get current values (_after_ changes notification registration) */
        BOOL mute;
        float level;

        hr = ISimpleAudioVolume_GetMute(sys->volume, &mute);
        if (FAILED(hr))
            msg_Err(aout, "cannot get mute (error 0x%lx)", hr);
        else
            aout_MuteReport(aout, mute != FALSE);

        hr = ISimpleAudioVolume_GetMasterVolume(sys->volume, &level);
        if (FAILED(hr))
            msg_Err(aout, "cannot get mute (error 0x%lx)", hr);
        else
            aout_VolumeReport(aout, level);
    }

    SetEvent(sys->device_ready);
    /* Wait until device change or exit */
    WaitForSingleObject(sys->device_changed, INFINITE);

    /* Deregister session control */
    if (control != NULL)
    {
        IAudioSessionControl_UnregisterAudioSessionNotification(control,
                                                         &sys->session_events);
        IAudioSessionControl_Release(control);
    }

    if (sys->volume != NULL)
        ISimpleAudioVolume_Release(sys->volume);
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
    while (sys->it != NULL)
        MMSession(aout, sys);
    LeaveMTA();
    return NULL;
}

/*** Audio devices ***/
static int DevicesEnum(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;
    IMMDeviceCollection *devs;

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(sys->it, eRender,
                                                DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr))
    {
        msg_Warn(aout, "cannot enumerate audio endpoints (error 0x%lx)", hr);
        return -1;
    }

    UINT count;
    hr = IMMDeviceCollection_GetCount(devs, &count);
    if (FAILED(hr))
    {
        msg_Warn(aout, "cannot count audio endpoints (error 0x%lx)", hr);
        count = 0;
    }

    unsigned n = 0;

    for (UINT i = 0; i < count; i++)
    {
        IMMDevice *dev;
        char *id, *name = NULL;

        hr = IMMDeviceCollection_Item(devs, i, &dev);
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
        id = FromWide(devid);
        CoTaskMemFree(devid);

        /* User-readable device name */
        IPropertyStore *props;
        hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, &props);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT v;

            PropVariantInit(&v);
            hr = IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &v);
            if (SUCCEEDED(hr))
                name = FromWide(v.pwszVal);
            PropVariantClear(&v);
            IPropertyStore_Release(props);
        }
        IMMDevice_Release(dev);

        aout_HotplugReport(aout, id, (name != NULL) ? name : id);
        free(name);
        free(id);
        n++;
    }
    IMMDeviceCollection_Release(devs);
    return n;
}

/**
 * Opens the selected audio output device.
 */
static HRESULT OpenDevice(audio_output_t *aout, const char *devid)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    assert(sys->dev == NULL);

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
    assert(sys->manager == NULL);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get device (error 0x%lx)", hr);
        goto out;
    }

    /* Create session manager (for controls even w/o active audio client) */
    void *pv;
    hr = IMMDevice_Activate(sys->dev, &IID_IAudioSessionManager,
                            CLSCTX_ALL, NULL, &pv);
    if (FAILED(hr))
        msg_Err(aout, "cannot activate session manager (error 0x%lx)", hr);
    else
        sys->manager = pv;

    /* Report actual device */
    LPWSTR wdevid;
    hr = IMMDevice_GetId(sys->dev, &wdevid);
    if (SUCCEEDED(hr))
    {
        char *id = FromWide(wdevid);
        CoTaskMemFree(wdevid);
        if (likely(id != NULL))
        {
            aout_DeviceReport(aout, id);
            free(id);
        }
    }
out:
    SetEvent(sys->device_changed);
    WaitForSingleObject(sys->device_ready, INFINITE);
    return hr;
}

/**
 * Closes the opened audio output device (if any).
 */
static void CloseDevice(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert(sys->dev != NULL);

    if (sys->manager != NULL)
    {
        IAudioSessionManager_Release(sys->manager);
        sys->manager = NULL;
    }

    IMMDevice_Release(sys->dev);
    sys->dev = NULL;
}

static int DeviceSelect(audio_output_t *aout, const char *id)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    if (TryEnterMTA(aout))
        return -1;

    if (sys->dev != NULL)
        CloseDevice(aout);

    hr = OpenDevice(aout, id);
    while (hr == AUDCLNT_E_DEVICE_INVALIDATED)
        hr = OpenDevice(aout, NULL); /* Fallback to default device */
    LeaveMTA();

    if (sys->stream != NULL)
        /* Request restart of stream with the new device */
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    return FAILED(hr) ? -1 : 0;
}

/**
 * Callback for aout_stream_t to create a stream on the device.
 * This can instantiate an IAudioClient or IDirectSound(8) object.
 */
static HRESULT ActivateDevice(void *opaque, REFIID iid, PROPVARIANT *actparms,
                              void **restrict pv)
{
    IMMDevice *dev = opaque;

    return IMMDevice_Activate(dev, iid, CLSCTX_ALL, actparms, pv);
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    assert (sys->stream == NULL);
    /* Open the default device if required (to deal with restarts) */
    if (sys->dev == NULL && FAILED(DeviceSelect(aout, NULL)))
        return -1;

    aout_stream_t *s = vlc_object_create(aout, sizeof (*s));
    if (unlikely(s == NULL))
        return -1;

    s->owner.device = sys->dev;
    s->owner.activate = ActivateDevice;

    EnterMTA();
    hr = aout_stream_Start(s, fmt, &GUID_VLC_AUD_OUT);
    if (SUCCEEDED(hr))
        sys->stream = s;
    else
        vlc_object_release(s);
    LeaveMTA();

    return vlc_FromHR(aout, hr);
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->stream != NULL);

    EnterMTA();
    aout_stream_Stop(sys->stream);
    LeaveMTA();

    vlc_object_release(sys->stream);
    sys->stream = NULL;
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
    sys->stream = NULL;
    sys->it = NULL;
    sys->dev = NULL;
    sys->manager = NULL;
    sys->session_events.lpVtbl = &vlc_AudioSessionEvents;
    sys->refs = 1;

    sys->device_changed = CreateEvent(NULL, FALSE, FALSE, NULL);
    sys->device_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (unlikely(sys->device_changed == NULL || sys->device_ready == NULL))
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

    if (vlc_clone(&sys->thread, MMThread, aout, VLC_THREAD_PRIORITY_LOW))
        goto error;
    WaitForSingleObject(sys->device_ready, INFINITE);

    DeviceSelect(aout, NULL); /* Get a device to start with */
    LeaveMTA(); /* leave MTA after thread has entered MTA */

    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = DeviceSelect;
    DevicesEnum(aout);
    return VLC_SUCCESS;

error:
    if (sys->it != NULL)
    {
        IMMDeviceEnumerator_Release(sys->it);
        LeaveMTA();
    }
    if (sys->device_ready != NULL)
        CloseHandle(sys->device_ready);
    if (sys->device_changed != NULL)
        CloseHandle(sys->device_changed);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    EnterMTA(); /* enter MTA before thread leaves MTA */
    if (sys->dev != NULL)
        CloseDevice(aout);

    IMMDeviceEnumerator_Release(sys->it);
    sys->it = NULL;

    SetEvent(sys->device_changed);
    vlc_join(sys->thread, NULL);
    LeaveMTA();

    CloseHandle(sys->device_ready);
    CloseHandle(sys->device_changed);
    free(sys);
}

vlc_module_begin()
    set_shortname("MMDevice")
    set_description(N_("Windows Multimedia Device output"))
    set_capability("audio output", /*150*/0)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
