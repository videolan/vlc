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
    struct aout_stream_owner *stream; /**< Underlying audio output stream */
    module_t *module;
    IAudioClient *client;
    wchar_t* acquired_device;
    wchar_t* requested_device;
    wchar_t* default_device; // read once on open

    float gain;

    // IActivateAudioInterfaceCompletionHandler interface
    IActivateAudioInterfaceCompletionHandler client_locator;
    vlc_sem_t async_completed;
    LONG refs;
    vlc_mutex_t lock;
    vlc_thread_t thread;
    bool stopping;

    HANDLE work_event;
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
static STDMETHODIMP MMDeviceLocator_ActivateCompleted(IActivateAudioInterfaceCompletionHandler *This,
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
            void *pv;
            hr = IUnknown_QueryInterface(audioInterface, &IID_IAudioClient, &pv);
            IUnknown_Release(audioInterface);
            if (unlikely(FAILED(hr)))
                msg_Warn(aout, "The received interface is not a IAudioClient. (hr=0x%lX)", hr);
            else
            {
                sys->client = pv;
                sys->acquired_device = wcsdup(devId);

                char *report = FromWide(devId);
                if (likely(report))
                {
                    aout_DeviceReport(aout, report);
                    free(report);
                }

                if (SUCCEEDED(IAudioClient_QueryInterface(sys->client, &IID_IAudioClient2, &pv)))
                {
                    IAudioClient2 *audioClient2 = pv;
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
    vlc_mutex_lock(&sys->lock);
    int ret = DeviceSelectLocked(aout, id);
    vlc_mutex_unlock(&sys->lock);
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
    void *pv = NULL;
    ISimpleAudioVolume *pc_AudioVolume = NULL;

    float linear_vol = vol * vol * vol; /* ISimpleAudioVolume is tapered linearly. */

    if (linear_vol > 1.f)
    {
        sys->gain = linear_vol;
        linear_vol = 1.f;
    }
    else
        sys->gain = 1.f;

    aout_GainRequest(aout, sys->gain);

    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume, &pv);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get volume service (error 0x%lX)", hr);
        goto done;
    }
    pc_AudioVolume = pv;

    hr = ISimpleAudioVolume_SetMasterVolume(pc_AudioVolume, linear_vol, NULL);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set volume (error 0x%lX)", hr);
        goto done;
    }

    aout_VolumeReport(aout, vol);

done:
    if (pc_AudioVolume)
        ISimpleAudioVolume_Release(pc_AudioVolume);

    return SUCCEEDED(hr) ? 0 : -1;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    if( unlikely( sys->client == NULL ) )
        return VLC_EGENERIC;
    HRESULT hr;
    void *pv = NULL;
    ISimpleAudioVolume *pc_AudioVolume = NULL;

    hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume, &pv);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot get volume service (error 0x%lX)", hr);
        goto done;
    }
    pc_AudioVolume = pv;

    hr = ISimpleAudioVolume_SetMute(pc_AudioVolume, mute, NULL);
    if (FAILED(hr))
    {
        msg_Err(aout, "cannot set mute (error 0x%lX)", hr);
        goto done;
    }

    aout_MuteReport(aout, mute);

done:
    if (pc_AudioVolume)
        ISimpleAudioVolume_Release(pc_AudioVolume);

    return SUCCEEDED(hr) ? 0 : -1;
}

static int TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    EnterMTA();
    vlc_mutex_lock(&sys->lock);
    if (unlikely(sys->client == NULL))
    {
        vlc_mutex_unlock(&sys->lock);
        LeaveMTA();
        return -1;
    }

    hr = aout_stream_owner_TimeGet(sys->stream, delay);

    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();

    return SUCCEEDED(hr) ? 0 : -1;
}

static void Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock(&sys->lock);
    if (unlikely(sys->client == NULL))
    {
        block_Release(block);
        vlc_mutex_unlock(&sys->lock);
        return;
    }

    aout_stream_owner_AppendBlock(sys->stream, block, date);

    vlc_mutex_unlock(&sys->lock);
    SetEvent(sys->work_event);
}

static void Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    EnterMTA();
    vlc_mutex_lock(&sys->lock);
    if (unlikely(sys->client == NULL))
    {
        vlc_mutex_unlock(&sys->lock);
        LeaveMTA();
        return;
    }

    HRESULT hr = aout_stream_owner_Pause(sys->stream, paused);

    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();

    (void) date;
    ResetInvalidatedClient(aout, hr);
}

static void Flush(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    EnterMTA();
    vlc_mutex_lock(&sys->lock);
    if (unlikely(sys->client == NULL))
    {
        vlc_mutex_unlock(&sys->lock);
        LeaveMTA();
        return;
    }

    HRESULT hr = aout_stream_owner_Flush(sys->stream);

    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();

    ResetInvalidatedClient(aout, hr);
}

static void *PlaybackThread(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;
    struct aout_stream_owner *owner = sys->stream;

    vlc_thread_set_name("vlc-winstore");

    EnterMTA();
    vlc_mutex_lock(&sys->lock);

    while (true)
    {
        DWORD wait_ms = INFINITE;
        DWORD ev_count = 1;
        HANDLE events[2] = {
            sys->work_event,
            NULL
        };

        if (sys->stream != NULL)
        {
            wait_ms = aout_stream_owner_ProcessTimer(sys->stream);

            /* Don't listen to the stream event if the block fifo is empty */
            if (sys->stream->chain != NULL)
                events[ev_count++] = sys->stream->buffer_ready_event;
        }

        vlc_mutex_unlock(&sys->lock);
        WaitForMultipleObjects(ev_count, events, FALSE, wait_ms);
        vlc_mutex_lock(&sys->lock);

        if (sys->stopping)
            break;

        if (likely(sys->client != NULL))
        {
            HRESULT hr = aout_stream_owner_PlayAll(sys->stream);

            /* Don't call ResetInvalidatedClient here since this function lock
             * the current mutex */

            if (unlikely(hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                         hr == AUDCLNT_E_RESOURCES_INVALIDATED))
            {
                DeviceSelectLocked(aout, NULL);
                if (sys->client == NULL)
                {
                    /* Impossible to recover */
                    block_ChainRelease(owner->chain);
                    owner->chain = NULL;
                    owner->last = &owner->chain;
                }
            }
        }
    }

    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();

    return NULL;
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

    struct aout_stream_owner *owner =
        aout_stream_owner_New(aout, sizeof (*owner), ActivateDevice);
    if (unlikely(owner == NULL))
        return -1;
    aout_stream_t *s = &owner->s;

    // Load the "out stream" for the requested device
    EnterMTA();
    vlc_mutex_lock(&sys->lock);

    if (sys->requested_device != NULL)
    {
        if (sys->acquired_device == NULL || wcscmp(sys->acquired_device, sys->requested_device))
        {
            // we have a pending request for a new device
            DeviceRestartLocked(aout);
            if (sys->client == NULL)
            {
                vlc_mutex_unlock(&sys->lock);
                LeaveMTA();
                aout_stream_owner_Delete(owner);
                return -1;
            }
        }
    }

    sys->work_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (unlikely(sys->work_event == NULL))
    {
        vlc_mutex_unlock(&sys->lock);
        LeaveMTA();
        aout_stream_owner_Delete(owner);
        return -1;
    }

    for (;;)
    {
        owner->device = sys->client;
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

    // Report the initial volume and mute status to the core
    if (sys->client != NULL)
    {
        ISimpleAudioVolume *pc_AudioVolume = NULL;
        void *pv;

        hr = IAudioClient_GetService(sys->client, &IID_ISimpleAudioVolume, &pv);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get volume service (error 0x%lx)", hr);
            goto done;
        }
        pc_AudioVolume = pv;

        float vol;
        hr = ISimpleAudioVolume_GetMasterVolume(pc_AudioVolume, &vol);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get initial volume (error 0x%lX)", hr);
            goto done;
        }

        WINBOOL mute;
        hr = ISimpleAudioVolume_GetMute(pc_AudioVolume, &mute);
        if (FAILED(hr))
        {
            msg_Err(aout, "cannot get initial mute (error 0x%lX)", hr);
            goto done;
        }

        aout_VolumeReport(aout, cbrtf(vol * sys->gain));
        aout_MuteReport(aout, mute != 0);

    done:
        if (pc_AudioVolume)
            ISimpleAudioVolume_Release(pc_AudioVolume);
    }

    if (sys->module == NULL)
        goto error;

    assert (sys->stream == NULL);
    sys->stream = owner;
    sys->stopping = false;

    if (vlc_clone(&sys->thread, PlaybackThread, aout))
    {
        aout_stream_owner_Stop(sys->stream);
        sys->stream = NULL;
        goto error;
    }

    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();

    if (sys->client)
    {
        // the requested device has been used, reset it
        // we keep the corresponding sys->client until a new request is started
        SetRequestedDevice(aout, NULL);
    }

    return 0;

error:
    CloseHandle(sys->work_event);
    aout_stream_owner_Delete(owner);
    vlc_mutex_unlock(&sys->lock);
    LeaveMTA();
    return -1;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    assert (sys->stream != NULL);

    vlc_mutex_lock(&sys->lock);
    sys->stopping = true;
    vlc_mutex_unlock(&sys->lock);
    SetEvent(sys->work_event);
    vlc_join(sys->thread, NULL);

    EnterMTA();
    aout_stream_owner_Stop(sys->stream);
    LeaveMTA();

    CloseHandle(sys->work_event);
    aout_stream_owner_Delete(sys->stream);
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

    char *psz_default = FromWide(sys->default_device);
    if (likely(psz_default != NULL))
    {
        aout_HotplugReport(aout, psz_default, _("Default"));
        free(psz_default);
    }

    vlc_mutex_init(&sys->lock);

    vlc_sem_init(&sys->async_completed, 0);
    sys->refs = 0;
    sys->requested_device = sys->default_device;
    sys->acquired_device = NULL;
    sys->client_locator = (IActivateAudioInterfaceCompletionHandler) { &MMDeviceLocator_vtable };
    sys->gain = 1.f;

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
    if (sys->requested_device != sys->default_device)
        free(sys->requested_device);
    CoTaskMemFree(sys->default_device);

    free(sys);
}

vlc_module_begin()
    set_shortname("winstore")
    set_description("Windows Store audio output")
    set_capability("audio output", 0)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
