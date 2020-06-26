/*****************************************************************************
 * winstore.c : Windows Multimedia Device API audio output plugin for VLC
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

#include <audiopolicy.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h> // ToWide
#include <vlc_modules.h>
#include "audio_output/mmdevice.h"

#include <audioclient.h>
#include <mmdeviceapi.h>

DEFINE_GUID (GUID_VLC_AUD_OUT, 0x4533f59d, 0x59ee, 0x00c6,
   0xad, 0xb2, 0xc6, 0x8b, 0x50, 0x1a, 0x66, 0x55);

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

typedef struct
{
    aout_stream_t *stream; /**< Underlying audio output stream */
    module_t *module;
    IAudioClient *client;
    wchar_t* acquired_device;
    wchar_t* requested_device;
    wchar_t* default_device; // read once on open

    // IActivateAudioInterfaceCompletionHandler interface
    IActivateAudioInterfaceCompletionHandler client_locator;
    vlc_sem_t async_completed;
    LONG refs;
    CRITICAL_SECTION lock;
} aout_sys_t;


/* MMDeviceLocator IUnknown methods */
static STDMETHODIMP_(ULONG) MMDeviceLocator_AddRef(IActivateAudioInterfaceCompletionHandler *This)
{
    aout_sys_t *sys = container_of(This, aout_sys_t, client_locator);
    return InterlockedIncrement(&sys->refs);
}

static STDMETHODIMP_(ULONG) MMDeviceLocator_Release(IActivateAudioInterfaceCompletionHandler *This)
{
    aout_sys_t *sys = container_of(This, aout_sys_t, client_locator);
    return InterlockedDecrement(&sys->refs);
}

static STDMETHODIMP MMDeviceLocator_QueryInterface(IActivateAudioInterfaceCompletionHandler *This,
                                                   REFIID riid, void **ppv)
{
    if( IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IActivateAudioInterfaceCompletionHandler) )
    {
        MMDeviceLocator_AddRef(This);
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    if( IsEqualIID(riid, &IID_IAgileObject) )
    {
        return S_OK;
    }
    return ResultFromScode( E_NOINTERFACE );
}

/* MMDeviceLocator IActivateAudioInterfaceCompletionHandler methods */
static HRESULT MMDeviceLocator_ActivateCompleted(IActivateAudioInterfaceCompletionHandler *This,
                                                 IActivateAudioInterfaceAsyncOperation *operation)
{
    (void)operation;
    aout_sys_t *sys = container_of(This, aout_sys_t, client_locator);
    vlc_sem_post( &sys->async_completed );
    return S_OK;
}

/* MMDeviceLocator vtable */
static const struct IActivateAudioInterfaceCompletionHandlerVtbl MMDeviceLocator_vtable =
{
    MMDeviceLocator_QueryInterface,
    MMDeviceLocator_AddRef,
    MMDeviceLocator_Release,

    MMDeviceLocator_ActivateCompleted,
};

static void WaitForAudioClient(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    IActivateAudioInterfaceAsyncOperation* asyncOp = NULL;

    const wchar_t* devId = sys->requested_device ? sys->requested_device : sys->default_device;

    assert(sys->refs == 0);
    sys->refs = 0;
    assert(sys->client == NULL);
    sys->client = NULL;
    free(sys->acquired_device);
    sys->acquired_device = NULL;
    ActivateAudioInterfaceAsync(devId, &IID_IAudioClient, NULL, &sys->client_locator, &asyncOp);

    vlc_sem_wait( &sys->async_completed );

    if (asyncOp)
    {
        HRESULT hr;
        HRESULT hrActivateResult;
        IUnknown *audioInterface;

        hr = IActivateAudioInterfaceAsyncOperation_GetActivateResult(asyncOp, &hrActivateResult, &audioInterface);
        IActivateAudioInterfaceAsyncOperation_Release(asyncOp);
        if (unlikely(FAILED(hr)))
            msg_Dbg(aout, "Failed to get the activation result. (hr=0x%lX)", hr);
        else if (FAILED(hrActivateResult))
            msg_Dbg(aout, "Failed to activate the device. (hr=0x%lX)", hr);
        else if (unlikely(audioInterface == NULL))
            msg_Dbg(aout, "Failed to get the device instance.");
        else
        {
            hr = IUnknown_QueryInterface(audioInterface, &IID_IAudioClient, (void**)&sys->client);
            IUnknown_Release(audioInterface);
            if (unlikely(FAILED(hr)))
                msg_Warn(aout, "The received interface is not a IAudioClient. (hr=0x%lX)", hr);
            else
            {
                sys->acquired_device = wcsdup(devId);

                char *report = FromWide(devId);
                if (likely(report))
                {
                    aout_DeviceReport(aout, report);
                    free(report);
                }

                IAudioClient2 *audioClient2;
                if (SUCCEEDED(IAudioClient_QueryInterface(sys->client, &IID_IAudioClient2, (void**)&audioClient2))
                    && audioClient2)
                {
                    // "BackgroundCapableMedia" does not work in UWP
                    AudioClientProperties props = (AudioClientProperties) {
                        .cbSize = sizeof(props),
                        .bIsOffload = FALSE,
                        .eCategory = AudioCategory_Movie,
                        .Options = AUDCLNT_STREAMOPTIONS_NONE
                    };
                    if (FAILED(IAudioClient2_SetClientProperties(audioClient2, &props))) {
                        msg_Dbg(aout, "Failed to set audio client properties");
                    }
                    IAudioClient2_Release(audioClient2);
                }
            }
        }
    }
}

static bool SetRequestedDevice(audio_output_t *aout, wchar_t *id)
{
    aout_sys_t* sys = aout->sys;
    if (sys->requested_device != id)
    {
        if (sys->requested_device != sys->default_device)
            free(sys->requested_device);
        sys->requested_device = id;
        return true;
    }
    return false;
}

static int DeviceRequestLocked(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    assert(sys->requested_device);

    WaitForAudioClient(aout);

    if (sys->stream != NULL && sys->client != NULL)
        /* Request restart of stream with the new device */
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
    return (sys->client != NULL) ? 0 : -1;
}

static int DeviceRestartLocked(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    if (sys->client)
    {
        assert(sys->acquired_device);
        free(sys->acquired_device);
        sys->acquired_device = NULL;
        IAudioClient_Release(sys->client);
        sys->client = NULL;
    }

    return DeviceRequestLocked(aout);
}

static int DeviceSelectLocked(audio_output_t *aout, const char* id)
{
    aout_sys_t *sys = aout->sys;
    bool changed;
    if( id == NULL )
    {
        changed = SetRequestedDevice(aout, sys->default_device);
    }
    else
    {
        wchar_t *requested_device = ToWide(id);
        if (unlikely(requested_device == NULL))
            return VLC_ENOMEM;
        changed = SetRequestedDevice(aout, requested_device);
    }
    if (!changed)
        return VLC_EGENERIC;
    return DeviceRestartLocked(aout);
}

static int DeviceSelect(audio_output_t *aout, const char* id)
{
    aout_sys_t *sys = aout->sys;
    EnterCriticalSection(&sys->lock);
    int ret = DeviceSelectLocked(aout, id);
    LeaveCriticalSection(&sys->lock);
    return ret;
}

static void ResetInvalidatedClient(audio_output_t *aout, HRESULT hr)
{
    /* Select the default device (and restart) on unplug */
    if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                 hr == AUDCLNT_E_RESOURCES_INVALIDATED))
    {
        // Select the default device (and restart) on unplug
        DeviceSelect(aout, NULL);
    }
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return VLC_EGENERIC;
    HRESULT hr;
    ISimpleAudioVolume *pc_AudioVolume = NULL;
    float gain = 1.f;

    vol = vol * vol * vol; /* ISimpleAudioVolume is tapered linearly. */

    if (vol > 1.f)
    {
        gain = vol;
        vol = 1.f;
    }

    aout_GainRequest(aout, gain);

    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume, (void**)&pc_AudioVolume);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get volume service (error 0x%lX)", hr);
        goto done;
    }

    hr = ISimpleAudioVolume_SetMasterVolume(pc_AudioVolume, vol, NULL);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set volume (error 0x%lX)", hr);
        goto done;
    }

done:
    ISimpleAudioVolume_Release(pc_AudioVolume);

    return SUCCEEDED(hr) ? 0 : -1;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return VLC_EGENERIC;
    HRESULT hr;
    ISimpleAudioVolume *pc_AudioVolume = NULL;

    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume, (void**)&pc_AudioVolume);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get volume service (error 0x%lX)", hr);
        goto done;
    }

    hr = ISimpleAudioVolume_SetMute(pc_AudioVolume, mute, NULL);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set mute (error 0x%lX)", hr);
        goto done;
    }

done:
    ISimpleAudioVolume_Release(pc_AudioVolume);

    return SUCCEEDED(hr) ? 0 : -1;
}

static int TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return VLC_EGENERIC;
    HRESULT hr;

    EnterMTA();
    hr = aout_stream_TimeGet(sys->stream, delay);
    LeaveMTA();

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return;

    EnterMTA();
    HRESULT hr = aout_stream_Play(sys->stream, block, date);
    LeaveMTA();

    ResetInvalidatedClient(aout, hr);
    (void) date;
}

static void Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return;

    EnterMTA();
    HRESULT hr = aout_stream_Pause(sys->stream, paused);
    LeaveMTA();

    (void) date;
    ResetInvalidatedClient(aout, hr);
}

static void Flush(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return;

    EnterMTA();
    HRESULT hr = aout_stream_Flush(sys->stream);
    LeaveMTA();

    ResetInvalidatedClient(aout, hr);
}

static HRESULT ActivateDevice(void *opaque, REFIID iid, PROPVARIANT *actparms,
                              void **restrict pv)
{
    IAudioClient *client = opaque;

    if (!IsEqualIID(iid, &IID_IAudioClient))
        return E_NOINTERFACE;
    if (actparms != NULL || client == NULL )
        return E_INVALIDARG;

    IAudioClient_AddRef(client); // as would IMMDevice_Activate do
    *pv = opaque;

    return S_OK;
}

static int aout_stream_Start(void *func, bool forced, va_list ap)
{
    aout_stream_start_t start = func;
    aout_stream_t *s = va_arg(ap, aout_stream_t *);
    audio_sample_format_t *fmt = va_arg(ap, audio_sample_format_t *);
    HRESULT *hr = va_arg(ap, HRESULT *);

    (void) forced;
    *hr = start(s, fmt, &GUID_VLC_AUD_OUT);
    return SUCCEEDED(*hr) ? VLC_SUCCESS : VLC_EGENERIC;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    aout_stream_t *s = vlc_object_create(aout, sizeof (*s));
    if (unlikely(s == NULL))
        return -1;

    if (sys->requested_device != NULL)
    {
        if (sys->acquired_device == NULL || wcscmp(sys->acquired_device, sys->requested_device))
        {
            // we have a pending request for a new device
            DeviceRestartLocked(aout);
            if (sys->client == NULL)
            {
                vlc_object_delete(&s->obj);
                return -1;
            }
        }
    }

    // Load the "out stream" for the requested device
    EnterMTA();
    EnterCriticalSection(&sys->lock);

    s->owner.activate = ActivateDevice;
    for (;;)
    {
        s->owner.device = sys->client;
        sys->module = vlc_module_load(s, "aout stream", NULL, false,
                                      aout_stream_Start, s, fmt, &hr);

        int ret = -1;
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
        {
            // the requested device is not usable, try the default device
            ret = DeviceSelectLocked(aout, NULL);
        }
        else if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
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
        if (ret != VLC_SUCCESS)
            break;

        if (sys->client == NULL || sys->module != NULL)
            break;
    }

    LeaveCriticalSection(&sys->lock);
    LeaveMTA();

    if (sys->module == NULL)
    {
        vlc_object_delete(s);
        return -1;
    }

    if (sys->client)
    {
        // the requested device has been used, reset it
        // we keep the corresponding sys->client until a new request is started
        SetRequestedDevice(aout, NULL);
    }

    assert (sys->stream == NULL);
    sys->stream = s;
    return 0;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->stream != NULL);

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

    if (unlikely(FAILED(StringFromIID(&DEVINTERFACE_AUDIO_RENDER, &sys->default_device))))
    {
        msg_Dbg(obj, "Failed to get the default renderer string");
        free(sys);
        return VLC_EGENERIC;
    }

    InitializeCriticalSection(&sys->lock);

    vlc_sem_init(&sys->async_completed, 0);
    sys->refs = 0;
    sys->requested_device = sys->default_device;
    sys->acquired_device = NULL;
    sys->client_locator = (IActivateAudioInterfaceCompletionHandler) { &MMDeviceLocator_vtable };

    aout->sys = sys;
    sys->stream = NULL;
    sys->client = NULL;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->device_select = DeviceSelect;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    if(sys->client != NULL)
        IAudioClient_Release(sys->client);

    assert(sys->refs == 0);

    free(sys->acquired_device);
    free(sys->requested_device);
    CoTaskMemFree(sys->default_device);
    DeleteCriticalSection(&sys->lock);

    free(sys);
}

vlc_module_begin()
    set_shortname("winstore")
    set_description("Windows Store audio output")
    set_capability("audio output", 0)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
