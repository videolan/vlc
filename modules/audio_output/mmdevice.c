/*****************************************************************************
 * mmdevice.c : Windows Multimedia Device API audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2012-2017 Rémi Denis-Courmont
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

#include <stdatomic.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd,
   0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>
#include <vlc_modules.h>
#include "audio_output/mmdevice.h"

DEFINE_GUID (GUID_VLC_AUD_OUT, 0x4533f59d, 0x59ee, 0x00c6,
   0xad, 0xb2, 0xc6, 0x8b, 0x50, 0x1a, 0x66, 0x55);

static int TryEnterMTA(vlc_object_t *obj)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr)))
    {
        msg_Err (obj, "cannot initialize COM (error 0x%lX)", hr);
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

static wchar_t default_device[1] = L"";
static char default_device_b[1] = "";

typedef struct
{
    aout_stream_t *stream; /**< Underlying audio output stream */
    audio_output_t *aout;
    IMMDeviceEnumerator *it; /**< Device enumerator, NULL when exiting */
    IMMDevice *dev; /**< Selected output device, NULL if none */

    struct IMMNotificationClient device_events;
    struct IAudioSessionEvents session_events;
    struct IAudioVolumeDuckNotification duck;

    LONG refs;
    unsigned ducks;
    float gain; /**< Current software gain volume */

    wchar_t *requested_device; /**< Requested device identifier, NULL if none */
    float requested_volume; /**< Requested volume, negative if none */
    signed char requested_mute; /**< Requested mute, negative if none */
    wchar_t *acquired_device; /**< Acquired device identifier, NULL if none */
    bool request_device_restart;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE work;
    CONDITION_VARIABLE ready;
    vlc_thread_t thread; /**< Thread for audio session control */
} aout_sys_t;

/* NOTE: The Core Audio API documentation totally fails to specify the thread
 * safety (or lack thereof) of the interfaces. This code takes the most
 * restrictive assumption: no thread safety. The background thread (MMThread)
 * only runs at specified times, namely between the device_ready and
 * device_changed events (effectively a thread synchronization barrier, but
 * only Windows 8 natively provides such a primitive).
 *
 * The audio output owner (i.e. the audio output core) is responsible for
 * serializing callbacks. This code only needs to be concerned with
 * synchronization between the set of audio output callbacks, MMThread()
 * and (trivially) the device and session notifications. */

static int DeviceSelect(audio_output_t *, const char *);
static int vlc_FromHR(audio_output_t *aout, HRESULT hr)
{
    /* Select the default device (and restart) on unplug */
    if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                 hr == AUDCLNT_E_RESOURCES_INVALIDATED))
        DeviceSelect(aout, NULL);
    return SUCCEEDED(hr) ? 0 : -1;
}

/*** VLC audio output callbacks ***/
static int TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_TimeGet(sys->stream, delay);
    LeaveMTA();

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_Play(sys->stream, block, date);
    LeaveMTA();

    vlc_FromHR(aout, hr);
}

static void Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_Pause(sys->stream, paused);
    LeaveMTA();

    vlc_FromHR(aout, hr);
    (void) date;
}

static void Flush(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_Flush(sys->stream);
    LeaveMTA();

    vlc_FromHR(aout, hr);
}

static int VolumeSetLocked(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    float gain = 1.f;

    vol = vol * vol * vol; /* ISimpleAudioVolume is tapered linearly. */

    if (vol > 1.f)
    {
        gain = vol;
        vol = 1.f;
    }

    aout_GainRequest(aout, gain);

    sys->gain = gain;
    sys->requested_volume = vol;
    return 0;
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    int ret = VolumeSetLocked(aout, vol);
    WakeConditionVariable(&sys->work);
    LeaveCriticalSection(&sys->lock);
    return ret;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    sys->requested_mute = mute;
    WakeConditionVariable(&sys->work);
    LeaveCriticalSection(&sys->lock);
    return 0;
}

/*** Audio session events ***/
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
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    return InterlockedIncrement(&sys->refs);
}

static STDMETHODIMP_(ULONG)
vlc_AudioSessionEvents_Release(IAudioSessionEvents *this)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    return InterlockedDecrement(&sys->refs);
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnDisplayNameChanged(IAudioSessionEvents *this,
                                            LPCWSTR wname, LPCGUID ctx)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "display name changed: %ls", wname);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnIconPathChanged(IAudioSessionEvents *this,
                                         LPCWSTR wpath, LPCGUID ctx)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "icon path changed: %ls", wpath);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnSimpleVolumeChanged(IAudioSessionEvents *this,
                                             float vol, BOOL mute,
                                             LPCGUID ctx)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "simple volume changed: %f, muting %sabled", vol,
            mute ? "en" : "dis");
    EnterCriticalSection(&sys->lock);
    WakeConditionVariable(&sys->work); /* implicit state: vol & mute */
    LeaveCriticalSection(&sys->lock);
    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnChannelVolumeChanged(IAudioSessionEvents *this,
                                              DWORD count, float *vols,
                                              DWORD changed, LPCGUID ctx)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    audio_output_t *aout = sys->aout;

    if (changed != (DWORD)-1)
        msg_Dbg(aout, "channel volume %lu of %lu changed: %f", changed, count,
                vols[changed]);
    else
        msg_Dbg(aout, "%lu channels volume changed", count);

    (void) ctx;
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnGroupingParamChanged(IAudioSessionEvents *this,
                                              LPCGUID param, LPCGUID ctx)

{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
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
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "state changed: %d", state);
    return S_OK;
}

static STDMETHODIMP
vlc_AudioSessionEvents_OnSessionDisconnected(IAudioSessionEvents *this,
                                           AudioSessionDisconnectReason reason)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, session_events);
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

static STDMETHODIMP
vlc_AudioVolumeDuckNotification_QueryInterface(
    IAudioVolumeDuckNotification *this, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown)
     || IsEqualIID(riid, &IID_IAudioVolumeDuckNotification))
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
vlc_AudioVolumeDuckNotification_AddRef(IAudioVolumeDuckNotification *this)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, duck);
    return InterlockedIncrement(&sys->refs);
}

static STDMETHODIMP_(ULONG)
vlc_AudioVolumeDuckNotification_Release(IAudioVolumeDuckNotification *this)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, duck);
    return InterlockedDecrement(&sys->refs);
}

static STDMETHODIMP
vlc_AudioVolumeDuckNotification_OnVolumeDuckNotification(
    IAudioVolumeDuckNotification *this, LPCWSTR sid, UINT32 count)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, duck);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "volume ducked by %ls of %u sessions", sid, count);
    sys->ducks++;
    aout_PolicyReport(aout, true);
    return S_OK;
}

static STDMETHODIMP
vlc_AudioVolumeDuckNotification_OnVolumeUnduckNotification(
    IAudioVolumeDuckNotification *this, LPCWSTR sid)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, duck);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "volume unducked by %ls", sid);
    sys->ducks--;
    aout_PolicyReport(aout, sys->ducks != 0);
    return S_OK;
}

static const struct IAudioVolumeDuckNotificationVtbl vlc_AudioVolumeDuckNotification =
{
    vlc_AudioVolumeDuckNotification_QueryInterface,
    vlc_AudioVolumeDuckNotification_AddRef,
    vlc_AudioVolumeDuckNotification_Release,

    vlc_AudioVolumeDuckNotification_OnVolumeDuckNotification,
    vlc_AudioVolumeDuckNotification_OnVolumeUnduckNotification,
};


/*** Audio devices ***/

/** Gets the user-readable device name */
static char *DeviceGetFriendlyName(IMMDevice *dev)
{
    IPropertyStore *props;
    PROPVARIANT v;
    HRESULT hr;

    hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, &props);
    if (FAILED(hr))
        return NULL;

    char *name = NULL;
    PropVariantInit(&v);
    hr = IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &v);
    if (SUCCEEDED(hr))
    {
        name = FromWide(v.pwszVal);
        PropVariantClear(&v);
    }

    IPropertyStore_Release(props);

    return name;
}

static int DeviceHotplugReport(audio_output_t *aout, LPCWSTR wid,
                               IMMDevice *dev)
{
    char *id = FromWide(wid);
    if (!id)
        return VLC_EGENERIC;

    char *name = DeviceGetFriendlyName(dev);
    if (name == NULL)
        name = id;

    aout_HotplugReport(aout, id, name);

    free(id);
    if (id != name)
        free(name);
    return VLC_SUCCESS;
}

/** Checks that a device is an output device */
static bool DeviceIsRender(IMMDevice *dev)
{
    void *pv;

    if (FAILED(IMMDevice_QueryInterface(dev, &IID_IMMEndpoint, &pv)))
        return false;

    IMMEndpoint *ep = pv;
    EDataFlow flow;
    HRESULT hr = IMMEndpoint_GetDataFlow(ep, &flow);

    IMMEndpoint_Release(ep);
    if (FAILED(hr) || flow != eRender)
        return false;

    DWORD pdwState;
    hr = IMMDevice_GetState(dev, &pdwState);
    return !FAILED(hr) && pdwState == DEVICE_STATE_ACTIVE;
}

static HRESULT DeviceUpdated(audio_output_t *aout, LPCWSTR wid)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    IMMDevice *dev;
    hr = IMMDeviceEnumerator_GetDevice(sys->it, wid, &dev);
    if (FAILED(hr))
        return hr;

    if (!DeviceIsRender(dev))
    {
        IMMDevice_Release(dev);
        return S_OK;
    }

    DeviceHotplugReport(aout, wid, dev);
    IMMDevice_Release(dev);
    return S_OK;
}

static STDMETHODIMP
vlc_MMNotificationClient_QueryInterface(IMMNotificationClient *this,
                                        REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown)
     || IsEqualIID(riid, &IID_IMMNotificationClient))
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
vlc_MMNotificationClient_AddRef(IMMNotificationClient *this)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    return InterlockedIncrement(&sys->refs);
}

static STDMETHODIMP_(ULONG)
vlc_MMNotificationClient_Release(IMMNotificationClient *this)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    return InterlockedDecrement(&sys->refs);
}

static STDMETHODIMP
vlc_MMNotificationClient_OnDefaultDeviceChange(IMMNotificationClient *this,
                                               EDataFlow flow, ERole role,
                                               LPCWSTR wid)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    audio_output_t *aout = sys->aout;

    if (flow != eRender)
        return S_OK;
    if (role != eConsole)
        return S_OK;

    EnterCriticalSection(&sys->lock);
    if (sys->acquired_device == NULL || sys->acquired_device == default_device)
    {
        msg_Dbg(aout, "default device changed: %ls", wid);
        sys->request_device_restart = true;
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    }
    LeaveCriticalSection(&sys->lock);

    return S_OK;
}

static STDMETHODIMP
vlc_MMNotificationClient_OnDeviceAdded(IMMNotificationClient *this,
                                       LPCWSTR wid)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    audio_output_t *aout = sys->aout;

    msg_Dbg(aout, "device %ls added", wid);
    return DeviceUpdated(aout, wid);
}

static STDMETHODIMP
vlc_MMNotificationClient_OnDeviceRemoved(IMMNotificationClient *this,
                                         LPCWSTR wid)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    audio_output_t *aout = sys->aout;
    char *id = FromWide(wid);

    msg_Dbg(aout, "device %ls removed", wid);
    if (unlikely(id == NULL))
        return E_OUTOFMEMORY;

    aout_HotplugReport(aout, id, NULL);
    free(id);
    return S_OK;
}

static STDMETHODIMP
vlc_MMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *this,
                                              LPCWSTR wid, DWORD state)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    audio_output_t *aout = sys->aout;

    switch (state) {
        case DEVICE_STATE_UNPLUGGED:
            msg_Dbg(aout, "device %ls state changed: unplugged", wid);
            break;
        case DEVICE_STATE_ACTIVE:
            msg_Dbg(aout, "device %ls state changed: active", wid);
            return DeviceUpdated(aout, wid);
        case DEVICE_STATE_DISABLED:
            msg_Dbg(aout, "device %ls state changed: disabled", wid);
            break;
        case DEVICE_STATE_NOTPRESENT:
            msg_Dbg(aout, "device %ls state changed: not present", wid);
            break;
        default:
            msg_Dbg(aout, "device %ls state changed: unknown: %08lx", wid, state);
            return E_FAIL;
    }

    /* Unplugged, disabled or notpresent */
    char *id = FromWide(wid);
    if (unlikely(id == NULL))
        return E_OUTOFMEMORY;
    aout_HotplugReport(aout, id, NULL);
    free(id);

    return S_OK;
}

static STDMETHODIMP
vlc_MMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *this,
                                                LPCWSTR wid,
                                                const PROPERTYKEY key)
{
    aout_sys_t *sys = container_of(this, aout_sys_t, device_events);
    audio_output_t *aout = sys->aout;

    if (key.pid == PKEY_Device_FriendlyName.pid)
    {
        msg_Dbg(aout, "device %ls name changed", wid);
        return DeviceUpdated(aout, wid);
    }
    return S_OK;
}

static const struct IMMNotificationClientVtbl vlc_MMNotificationClient =
{
    vlc_MMNotificationClient_QueryInterface,
    vlc_MMNotificationClient_AddRef,
    vlc_MMNotificationClient_Release,

    vlc_MMNotificationClient_OnDeviceStateChanged,
    vlc_MMNotificationClient_OnDeviceAdded,
    vlc_MMNotificationClient_OnDeviceRemoved,
    vlc_MMNotificationClient_OnDefaultDeviceChange,
    vlc_MMNotificationClient_OnPropertyValueChanged,
};

static HRESULT DevicesEnum(IMMDeviceEnumerator *it,
                           void (*added_cb)(void *data, LPCWSTR wid, IMMDevice *dev),
                           void *added_cb_data)
{
    HRESULT hr;
    IMMDeviceCollection *devs;
    assert(added_cb != NULL);

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(it, eRender,
                                                DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr))
        return hr;

    UINT count;
    hr = IMMDeviceCollection_GetCount(devs, &count);
    if (FAILED(hr))
        return hr;

    for (UINT i = 0; i < count; i++)
    {
        IMMDevice *dev;

        hr = IMMDeviceCollection_Item(devs, i, &dev);
        if (FAILED(hr) || !DeviceIsRender(dev))
            continue;

        /* Unique device ID */
        LPWSTR devid;
        hr = IMMDevice_GetId(dev, &devid);
        if (FAILED(hr))
        {
            IMMDevice_Release(dev);
            continue;
        }

        added_cb(added_cb_data, devid, dev);
        IMMDevice_Release(dev);
        CoTaskMemFree(devid);
    }
    IMMDeviceCollection_Release(devs);
    return S_OK;
}

static int DeviceRequestLocked(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    assert(sys->requested_device);

    sys->request_device_restart = false;

    WakeConditionVariable(&sys->work);
    while (sys->requested_device != NULL)
        SleepConditionVariableCS(&sys->ready, &sys->lock, INFINITE);

    if (sys->stream != NULL && sys->dev != NULL)
        /* Request restart of stream with the new device */
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    return (sys->dev != NULL) ? 0 : -1;
}

static int DeviceSelectLocked(audio_output_t *aout, const char *id)
{
    aout_sys_t *sys = aout->sys;
    assert(sys->requested_device == NULL);

    if (id != NULL && strcmp(id, default_device_b) != 0)
    {
        sys->requested_device = ToWide(id); /* FIXME leak */
        if (unlikely(sys->requested_device == NULL))
            return -1;
    }
    else
        sys->requested_device = default_device;

    return DeviceRequestLocked(aout);
}

static int DeviceRestartLocked(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    assert(sys->requested_device == NULL);
    sys->requested_device = sys->acquired_device ? sys->acquired_device
                                                 : default_device;
    return DeviceRequestLocked(aout);
}

static int DeviceSelect(audio_output_t *aout, const char *id)
{
    aout_sys_t *sys = aout->sys;
    EnterCriticalSection(&sys->lock);
    int ret = DeviceSelectLocked(aout, id);
    LeaveCriticalSection(&sys->lock);
    return ret;
}

/*** Initialization / deinitialization **/
/** MMDevice audio output thread.
 * This thread takes cares of the audio session control. Inconveniently enough,
 * the audio session control interface must:
 *  - be created and destroyed from the same thread, and
 *  - survive across VLC audio output calls.
 * The only way to reconcile both requirements is a custom thread.
 * The thread also ensure that the COM Multi-Thread Apartment is continuously
 * referenced so that MMDevice objects are not destroyed early.
 * Furthermore, VolumeSet() and MuteSet() may be called from a thread with a
 * COM STA, so that it cannot access the COM MTA for audio controls.
 */
static HRESULT MMSession(audio_output_t *aout, IMMDeviceEnumerator *it)
{
    aout_sys_t *sys = aout->sys;
    IAudioSessionManager *manager;
    IAudioSessionControl *control;
    ISimpleAudioVolume *volume;
    IAudioEndpointVolume *endpoint;
    void *pv;
    HRESULT hr;

    assert(sys->requested_device != NULL);
    assert(sys->dev == NULL);

    /* Yes, it's perfectly valid to request the same device, see Start()
     * comments. */
    if (sys->acquired_device != sys->requested_device
     && sys->acquired_device != default_device)
        free(sys->acquired_device);
    if (sys->requested_device != default_device) /* Device selected explicitly */
    {
        msg_Dbg(aout, "using selected device %ls", sys->requested_device);
        hr = IMMDeviceEnumerator_GetDevice(it, sys->requested_device, &sys->dev);
        if (FAILED(hr))
            msg_Err(aout, "cannot get selected device %ls (error 0x%lX)",
                    sys->requested_device, hr);
        sys->acquired_device = sys->requested_device;
    }
    else
        hr = AUDCLNT_E_DEVICE_INVALIDATED;

    while (hr == AUDCLNT_E_DEVICE_INVALIDATED)
    {   /* Default device selected by policy and with stream routing.
         * "Do not use eMultimedia" says MSDN. */
        msg_Dbg(aout, "using default device");
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(it, eRender,
                                                         eConsole, &sys->dev);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get default device (error 0x%lX)", hr);
            sys->acquired_device = NULL;
        }
        else
            sys->acquired_device = default_device;
    }

    sys->requested_device = NULL;
    WakeConditionVariable(&sys->ready);

    if (SUCCEEDED(hr))
    {   /* Report actual device */
        LPWSTR wdevid;

        if (sys->acquired_device == default_device)
            aout_DeviceReport(aout, default_device_b);
        else
        {
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
        }
    }
    else
    {
        msg_Err(aout, "cannot get device identifier (error 0x%lX)", hr);
        return hr;
    }

    /* Create session manager (for controls even w/o active audio client) */
    hr = IMMDevice_Activate(sys->dev, &IID_IAudioSessionManager,
                            CLSCTX_ALL, NULL, &pv);
    manager = pv;
    if (SUCCEEDED(hr))
    {
        LPCGUID guid = var_GetBool(aout, "volume-save") ? &GUID_VLC_AUD_OUT : NULL;

        /* Register session control */
        hr = IAudioSessionManager_GetAudioSessionControl(manager, guid, 0,
                                                         &control);
        if (SUCCEEDED(hr))
        {
            char *ua = var_InheritString(aout, "user-agent");
            if (ua != NULL)
            {
                wchar_t *wua = ToWide(ua);
                if (likely(wua != NULL))
                {
                    IAudioSessionControl_SetDisplayName(control, wua, NULL);
                    free(wua);
                }
                free(ua);
            }

            IAudioSessionControl_RegisterAudioSessionNotification(control,
                                                         &sys->session_events);
        }
        else
            msg_Err(aout, "cannot get session control (error 0x%lX)", hr);

        hr = IAudioSessionManager_GetSimpleAudioVolume(manager, guid, FALSE,
                                                       &volume);
        if (FAILED(hr))
            msg_Err(aout, "cannot get simple volume (error 0x%lX)", hr);

        /* Try to get version 2 (Windows 7) of the manager & control */
        wchar_t *siid = NULL;

        hr = IAudioSessionManager_QueryInterface(manager,
                                              &IID_IAudioSessionControl2, &pv);
        if (SUCCEEDED(hr))
        {
            IAudioSessionControl2 *c2 = pv;

            IAudioSessionControl2_SetDuckingPreference(c2, FALSE);
            hr = IAudioSessionControl2_GetSessionInstanceIdentifier(c2, &siid);
            if (FAILED(hr))
                siid = NULL;
            IAudioSessionControl2_Release(c2);
        }
        else
            msg_Dbg(aout, "version 2 session control unavailable");

        hr = IAudioSessionManager_QueryInterface(manager,
                                              &IID_IAudioSessionManager2, &pv);
        if (SUCCEEDED(hr))
        {
            IAudioSessionManager2 *m2 = pv;

            IAudioSessionManager2_RegisterDuckNotification(m2, siid,
                                                           &sys->duck);
            IAudioSessionManager2_Release(m2);
        }
        else
            msg_Dbg(aout, "version 2 session management unavailable");

        CoTaskMemFree(siid);
    }
    else
    {
        msg_Err(aout, "cannot activate session manager (error 0x%lX)", hr);
        control = NULL;
        volume = NULL;
    }

    hr = IMMDevice_Activate(sys->dev, &IID_IAudioEndpointVolume,
                            CLSCTX_ALL, NULL, &pv);
    endpoint = pv;
    if (SUCCEEDED(hr))
    {
        float min, max, inc;

        hr = IAudioEndpointVolume_GetVolumeRange(endpoint, &min, &max, &inc);
        if (SUCCEEDED(hr))
            msg_Dbg(aout, "volume from %+f dB to %+f dB with %f dB increments",
                    min, max, inc);
        else
            msg_Err(aout, "cannot get volume range (error 0x%lX)", hr);
    }
    else
        msg_Err(aout, "cannot activate endpoint volume (error 0x%lX)", hr);

    /* Main loop (adjust volume as long as device is unchanged) */
    while (sys->requested_device == NULL)
    {
        if (volume != NULL)
        {
            float level;

            level = sys->requested_volume;
            if (level >= 0.f)
            {
                hr = ISimpleAudioVolume_SetMasterVolume(volume, level, NULL);
                if (FAILED(hr))
                    msg_Err(aout, "cannot set master volume (error 0x%lX)",
                            hr);
            }
            sys->requested_volume = -1.f;

            hr = ISimpleAudioVolume_GetMasterVolume(volume, &level);
            if (SUCCEEDED(hr))
                aout_VolumeReport(aout, cbrtf(level * sys->gain));
            else
                msg_Err(aout, "cannot get master volume (error 0x%lX)", hr);

            BOOL mute;

            hr = ISimpleAudioVolume_GetMute(volume, &mute);
            if (SUCCEEDED(hr))
                aout_MuteReport(aout, mute != FALSE);
            else
                msg_Err(aout, "cannot get mute (error 0x%lX)", hr);

            if (sys->requested_mute >= 0)
            {
                mute = sys->requested_mute ? TRUE : FALSE;

                hr = ISimpleAudioVolume_SetMute(volume, mute, NULL);
                if (FAILED(hr))
                    msg_Err(aout, "cannot set mute (error 0x%lX)", hr);
            }
            sys->requested_mute = -1;
        }

        SleepConditionVariableCS(&sys->work, &sys->lock, INFINITE);
    }
    LeaveCriticalSection(&sys->lock);

    if (endpoint != NULL)
        IAudioEndpointVolume_Release(endpoint);

    if (manager != NULL)
    {   /* Deregister callbacks *without* the lock */
        hr = IAudioSessionManager_QueryInterface(manager,
                                              &IID_IAudioSessionManager2, &pv);
        if (SUCCEEDED(hr))
        {
            IAudioSessionManager2 *m2 = pv;

            IAudioSessionManager2_UnregisterDuckNotification(m2, &sys->duck);
            IAudioSessionManager2_Release(m2);
        }

        if (volume != NULL)
            ISimpleAudioVolume_Release(volume);

        if (control != NULL)
        {
            IAudioSessionControl_UnregisterAudioSessionNotification(control,
                                                         &sys->session_events);
            IAudioSessionControl_Release(control);
        }

        IAudioSessionManager_Release(manager);
    }

    EnterCriticalSection(&sys->lock);
    IMMDevice_Release(sys->dev);
    sys->dev = NULL;
    return S_OK;
}

static void MMThread_DevicesEnum_Added(void *data, LPCWSTR wid, IMMDevice *dev)
{
    audio_output_t *aout = data;

    DeviceHotplugReport(aout, wid, dev);
}

static void *MMThread(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;
    IMMDeviceEnumerator *it = sys->it;

    EnterMTA();
    IMMDeviceEnumerator_RegisterEndpointNotificationCallback(it,
                                                          &sys->device_events);
    HRESULT hr = DevicesEnum(it, MMThread_DevicesEnum_Added, aout);
    if (FAILED(hr))
        msg_Warn(aout, "cannot enumerate audio endpoints (error 0x%lX)", hr);

    EnterCriticalSection(&sys->lock);

    do
        if (sys->requested_device == NULL || FAILED(MMSession(aout, it)))
            SleepConditionVariableCS(&sys->work, &sys->lock, INFINITE);
    while (sys->it != NULL);

    LeaveCriticalSection(&sys->lock);

    IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(it,
                                                          &sys->device_events);
    IMMDeviceEnumerator_Release(it);
    LeaveMTA();
    return NULL;
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

static int aout_stream_Start(void *func, bool forced, va_list ap)
{
    aout_stream_start_t start = func;
    aout_stream_t *s = va_arg(ap, aout_stream_t *);
    audio_sample_format_t *fmt = va_arg(ap, audio_sample_format_t *);
    HRESULT *hr = va_arg(ap, HRESULT *);
    LPCGUID sid = var_InheritBool(s, "volume-save") ? &GUID_VLC_AUD_OUT : NULL;

    (void) forced;
    *hr = start(s, fmt, sid);
    if (*hr == AUDCLNT_E_DEVICE_INVALIDATED)
        return VLC_ETIMEOUT;
    return SUCCEEDED(*hr) ? VLC_SUCCESS : VLC_EGENERIC;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    const bool b_spdif = AOUT_FMT_SPDIF(fmt);
    const bool b_hdmi = AOUT_FMT_HDMI(fmt);
    if (b_spdif || b_hdmi)
    {
        switch (var_InheritInteger(aout, "mmdevice-passthrough"))
        {
            case MM_PASSTHROUGH_DISABLED:
                return -1;
            case MM_PASSTHROUGH_ENABLED:
                if (b_hdmi)
                    return -1;
                /* falltrough */
            case MM_PASSTHROUGH_ENABLED_HD:
                break;
        }
    }

    aout_stream_t *s = vlc_object_create(aout, sizeof (*s));
    if (unlikely(s == NULL))
        return -1;

    s->owner.activate = ActivateDevice;

    EnterMTA();
    EnterCriticalSection(&sys->lock);

    if ((sys->request_device_restart && DeviceRestartLocked(aout) != 0)
      || sys->dev == NULL)
    {
        /* Error if the device restart failed or if a request previously
         * failed. */
        LeaveCriticalSection(&sys->lock);
        LeaveMTA();
        vlc_object_delete(s);
        return -1;
    }

    module_t *module;

    for (;;)
    {
        char *modlist = var_InheritString(aout, "mmdevice-backend");
        HRESULT hr;
        s->owner.device = sys->dev;

        module = vlc_module_load(s, "aout stream", modlist,
                                 false, aout_stream_Start, s, fmt, &hr);
        free(modlist);

        int ret = -1;
        if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
        {
            /* From MSDN: "If the initial call to Initialize fails, subsequent
             * Initialize calls might fail and return error code
             * E_ALREADY_INITIALIZED, even though the interface has not been
             * initialized. If this occurs, release the IAudioClient interface
             * and obtain a new IAudioClient interface from the MMDevice API
             * before calling Initialize again."
             *
             * Therefore, request to MMThread the same device and try again. */

            ret = DeviceRestartLocked(aout);
        }
        else if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
        {
            /* The audio endpoint device has been unplugged, request to
             * MMThread the default device and try again. */

            ret = DeviceSelectLocked(aout, NULL);
        }
        if (ret != 0)
            break;
    }

    if (module != NULL)
    {
        IPropertyStore *props;
        HRESULT hr = IMMDevice_OpenPropertyStore(sys->dev, STGM_READ, &props);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT v;
            PropVariantInit(&v);
            hr = IPropertyStore_GetValue(props, &PKEY_AudioEndpoint_FormFactor, &v);
            if (SUCCEEDED(hr))
            {
                switch (v.uintVal)
                {
                    case Headphones:
                    case Headset:
                        aout->current_sink_info.headphones = true;
                        break;
                }
                PropVariantClear(&v);
            }
            IPropertyStore_Release(props);
        }
    }

    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    if (module == NULL)
    {
        vlc_object_delete(s);
        return -1;
    }

    assert (sys->stream == NULL);
    sys->stream = s;
    aout_GainRequest(aout, sys->gain);
    return 0;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert(sys->stream != NULL);

    EnterMTA();
    aout_stream_Stop(sys->stream);
    LeaveMTA();

    vlc_object_delete(sys->stream);
    sys->stream = NULL;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    aout->sys = sys;
    sys->stream = NULL;
    sys->aout = aout;
    sys->it = NULL;
    sys->dev = NULL;
    sys->device_events.lpVtbl = &vlc_MMNotificationClient;
    sys->session_events.lpVtbl = &vlc_AudioSessionEvents;
    sys->duck.lpVtbl = &vlc_AudioVolumeDuckNotification;
    sys->refs = 1;
    sys->ducks = 0;

    sys->gain = 1.f;
    sys->requested_volume = -1.f;
    sys->requested_mute = -1;
    sys->acquired_device = NULL;
    sys->request_device_restart = false;

    if (!var_CreateGetBool(aout, "volume-save"))
        VolumeSetLocked(aout, var_InheritFloat(aout, "mmdevice-volume"));

    InitializeCriticalSection(&sys->lock);
    InitializeConditionVariable(&sys->work);
    InitializeConditionVariable(&sys->ready);

    aout_HotplugReport(aout, default_device_b, _("Default"));

    char *saved_device_b = var_InheritString(aout, "mmdevice-audio-device");
    if (saved_device_b != NULL && strcmp(saved_device_b, default_device_b) != 0)
    {
        sys->requested_device = ToWide(saved_device_b); /* FIXME leak */
        free(saved_device_b);

        if (unlikely(sys->requested_device == NULL))
            goto error;
    }
    else
    {
        free(saved_device_b);
        sys->requested_device = default_device;
    }

    /* Initialize MMDevice API */
    if (TryEnterMTA(aout))
        goto error;

    void *pv;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  &IID_IMMDeviceEnumerator, &pv);
    if (FAILED(hr))
    {
        LeaveMTA();
        msg_Dbg(aout, "cannot create device enumerator (error 0x%lX)", hr);
        goto error;
    }
    sys->it = pv;

    if (vlc_clone(&sys->thread, MMThread, aout, VLC_THREAD_PRIORITY_LOW))
    {
        IMMDeviceEnumerator_Release(sys->it);
        LeaveMTA();
        goto error;
    }

    EnterCriticalSection(&sys->lock);
    while (sys->requested_device != NULL)
        SleepConditionVariableCS(&sys->ready, &sys->lock, INFINITE);
    LeaveCriticalSection(&sys->lock);
    LeaveMTA(); /* Leave MTA after thread has entered MTA */

    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = DeviceSelect;

    return VLC_SUCCESS;

error:
    DeleteCriticalSection(&sys->lock);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    EnterCriticalSection(&sys->lock);
    sys->requested_device = default_device; /* break out of MMSession() loop */
    sys->it = NULL; /* break out of MMThread() loop */
    WakeConditionVariable(&sys->work);
    LeaveCriticalSection(&sys->lock);

    vlc_join(sys->thread, NULL);
    DeleteCriticalSection(&sys->lock);

    free(sys);
}

struct mm_list
{
    size_t count;
    char **ids;
    char **names;
};

static void Reload_DevicesEnum_Added(void *data, LPCWSTR wid, IMMDevice *dev)
{
    struct mm_list *list = data;

    size_t new_count = list->count + 1;
    list->ids = realloc_or_free(list->ids, new_count * sizeof(char *));
    list->names = realloc_or_free(list->names, new_count * sizeof(char *));
    if (!list->ids || !list->names)
    {
        free(list->ids);
        return;
    }

    char *id = FromWide(wid);
    if (!id)
        return;

    char *name = DeviceGetFriendlyName(dev);
    if (!name && !(name = strdup(id)))
    {
        free(id);
        return;
    }
    list->ids[list->count] = id;
    list->names[list->count] = name;

    list->count = new_count;
}

static int ReloadAudioDevices(char const *name, char ***values, char ***descs)
{
    bool in_mta = true;
    HRESULT hr;

    (void) name;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        if (hr != RPC_E_CHANGED_MODE)
            return -1;

        in_mta = false;
    }

    struct mm_list list = { .count = 0 };
    void *it;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, &it);
    if (FAILED(hr))
        goto error;

    list.ids = malloc(sizeof (char *));
    list.names = malloc(sizeof (char *));
    if (!list.ids || !list.names)
    {
        free(list.ids);
        goto error;
    }

    list.ids[0] = strdup("");
    list.names[0] = strdup(_("Default"));
    if (!list.ids[0] || !list.names[0])
    {
        free(list.ids[0]);
        free(list.ids);
        free(list.names);
        goto error;
    }
    list.count++;

    DevicesEnum(it, Reload_DevicesEnum_Added, &list);

error:
    IMMDeviceEnumerator_Release((IMMDeviceEnumerator *)it);
    if (in_mta)
        CoUninitialize();

    if (list.count > 0)
    {
        *values = list.ids;
        *descs = list.names;
    }

    return list.count;
}

VLC_CONFIG_STRING_ENUM(ReloadAudioDevices)

#define MM_PASSTHROUGH_TEXT N_( \
    "HDMI/SPDIF audio passthrough")
#define MM_PASSTHROUGH_LONGTEXT N_( \
    "Change this value if you have issue with HD codecs when using a HDMI receiver.")
static const int pi_mmdevice_passthrough_values[] = {
    MM_PASSTHROUGH_DISABLED,
    MM_PASSTHROUGH_ENABLED,
    MM_PASSTHROUGH_ENABLED_HD,
};
static const char *const ppsz_mmdevice_passthrough_texts[] = {
    N_("Disabled"),
    N_("Enabled (AC3/DTS only)"),
    N_("Enabled"),
};

#define DEVICE_TEXT N_("Output device")
#define DEVICE_LONGTEXT N_("Select your audio output device")

#define VOLUME_TEXT N_("Audio volume")
#define VOLUME_LONGTEXT N_("Audio volume in hundredths of decibels (dB).")

vlc_module_begin()
    set_shortname("MMDevice")
    set_description(N_("Windows Multimedia Device output"))
    set_capability("audio output", 150)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
    add_module("mmdevice-backend", "aout stream", "any",
               N_("Output back-end"), N_("Audio output back-end interface."))
    add_integer( "mmdevice-passthrough", MM_PASSTHROUGH_DEFAULT,
                 MM_PASSTHROUGH_TEXT, MM_PASSTHROUGH_LONGTEXT, false )
        change_integer_list( pi_mmdevice_passthrough_values,
                             ppsz_mmdevice_passthrough_texts )
    add_string("mmdevice-audio-device", NULL, DEVICE_TEXT, DEVICE_LONGTEXT, false)
    add_float("mmdevice-volume", 1.f, VOLUME_TEXT, VOLUME_LONGTEXT, true)
        change_float_range( 0.f, 1.25f )
vlc_module_end()
