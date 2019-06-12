/**
 * \file wasapi.c
 * \brief Windows Audio Session API capture plugin for VLC
 */
/*****************************************************************************
 * Copyright (C) 2014-2015 Rémi Denis-Courmont
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

#include <assert.h>
#include <stdlib.h>

#define _DECL_DLLMAIN
#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

static LARGE_INTEGER freq; /* performance counters frequency */

BOOL WINAPI DllMain(HANDLE dll, DWORD reason, LPVOID reserved)
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

static msftime_t GetQPC(void)
{
    LARGE_INTEGER counter;

    if (!QueryPerformanceCounter(&counter))
        abort();

    lldiv_t d = lldiv(counter.QuadPart, freq.QuadPart);
    return (d.quot * 10000000) + ((d.rem * 10000000) / freq.QuadPart);
}

static_assert(CLOCK_FREQ * 10 == 10000000,
              "REFERENCE_TIME conversion broken");

static EDataFlow GetDeviceFlow(IMMDevice *dev)
{
    void *pv;

    if (FAILED(IMMDevice_QueryInterface(dev, &IID_IMMEndpoint, &pv)))
        return false;

    IMMEndpoint *ep = pv;
    EDataFlow flow;

    if (SUCCEEDED(IMMEndpoint_GetDataFlow(ep, &flow)))
        flow = eAll;
    IMMEndpoint_Release(ep);
    return flow;
}

static IAudioClient *GetClient(demux_t *demux, bool *restrict loopbackp)
{
    IMMDeviceEnumerator *e;
    IMMDevice *dev;
    void *pv;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, &pv);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot create device enumerator (error 0x%lX)", hr);
        return NULL;
    }
    e = pv;

    bool loopback = var_InheritBool(demux, "wasapi-loopback");
    EDataFlow flow = loopback ? eRender : eCapture;
    ERole role = loopback ? eConsole : eCommunications;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, flow, role, &dev);
    IMMDeviceEnumerator_Release(e);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot get default device (error 0x%lX)", hr);
        return NULL;
    }

    hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, &pv);
    *loopbackp = GetDeviceFlow(dev) == eRender;
    IMMDevice_Release(dev);
    if (FAILED(hr))
        msg_Err(demux, "cannot activate device (error 0x%lX)", hr);
    return pv;
}

static int vlc_FromWave(const WAVEFORMATEX *restrict wf,
                        audio_sample_format_t *restrict fmt)
{
    fmt->i_rate = wf->nSamplesPerSec;

    /* As per MSDN, IAudioClient::GetMixFormat() always uses this format. */
    assert(wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE);

    const WAVEFORMATEXTENSIBLE *wfe = (void *)wf;

    fmt->i_physical_channels = 0;
    if (wfe->dwChannelMask & SPEAKER_FRONT_LEFT)
        fmt->i_physical_channels |= AOUT_CHAN_LEFT;
    if (wfe->dwChannelMask & SPEAKER_FRONT_RIGHT)
        fmt->i_physical_channels |= AOUT_CHAN_RIGHT;
    if (wfe->dwChannelMask & SPEAKER_FRONT_CENTER)
        fmt->i_physical_channels |= AOUT_CHAN_CENTER;
    if (wfe->dwChannelMask & SPEAKER_LOW_FREQUENCY)
        fmt->i_physical_channels |= AOUT_CHAN_LFE;

    assert(vlc_popcount(wfe->dwChannelMask) == wf->nChannels);

    if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM))
    {
        switch (wf->wBitsPerSample)
        {
            case 32:
                switch (wfe->Samples.wValidBitsPerSample)
                {
                    case 32:
                        fmt->i_format = VLC_CODEC_S32N;
                        break;
                    case 24:
#ifdef WORDS_BIGENDIAN
                        fmt->i_format = VLC_CODEC_S24B32;
#else
                        fmt->i_format = VLC_CODEC_S24L32;
#endif
                        break;
                    default:
                        return -1;
                }
                break;
            case 24:
                if (wfe->Samples.wValidBitsPerSample == 24)
                    fmt->i_format = VLC_CODEC_S24N;
                else
                    return -1;
                break;
            case 16:
                if (wfe->Samples.wValidBitsPerSample == 16)
                    fmt->i_format = VLC_CODEC_S16N;
                else
                    return -1;
                break;
            case 8:
                if (wfe->Samples.wValidBitsPerSample == 8)
                    fmt->i_format = VLC_CODEC_S8;
                else
                    return -1;
                break;
            default:
                return -1;
        }
    }
    else if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
    {
        if (wf->wBitsPerSample != wfe->Samples.wValidBitsPerSample)
            return -1;

        switch (wf->wBitsPerSample)
        {
            case 64:
                fmt->i_format = VLC_CODEC_FL64;
                break;
            case 32:
                fmt->i_format = VLC_CODEC_FL32;
                break;
            default:
                return -1;
        }
    }
    /*else if (IsEqualIID(&wfe->Subformat, &KSDATAFORMAT_SUBTYPE_DRM)) {} */
    else if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_ALAW))
        fmt->i_format = VLC_CODEC_ALAW;
    else if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_MULAW))
        fmt->i_format = VLC_CODEC_MULAW;
    else if (IsEqualIID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_ADPCM))
        fmt->i_format = VLC_CODEC_ADPCM_MS;
    else
        return -1;

    aout_FormatPrepare(fmt);
    if (wf->nChannels != fmt->i_channels)
        return -1;

    return 0;
}

static es_out_id_t *CreateES(demux_t *demux, IAudioClient *client, bool loop,
                             vlc_tick_t caching, size_t *restrict frame_size)
{
    es_format_t fmt;
    WAVEFORMATEX *pwf;
    HRESULT hr;

    hr = IAudioClient_GetMixFormat(client, &pwf);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot get mix format (error 0x%lX)", hr);
        return NULL;
    }

    es_format_Init(&fmt, AUDIO_ES, 0);
    if (vlc_FromWave(pwf, &fmt.audio))
    {
        msg_Err(demux, "unsupported mix format");
        CoTaskMemFree(pwf);
        return NULL;
    }

    fmt.i_codec = fmt.audio.i_format;
    fmt.i_bitrate = fmt.audio.i_bitspersample * fmt.audio.i_channels
                                              * fmt.audio.i_rate;
    *frame_size = fmt.audio.i_bitspersample * fmt.audio.i_channels / 8;

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (loop)
        flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

    /* Request at least thrice the PTS delay */
    REFERENCE_TIME bufsize = MSFTIME_FROM_VLC_TICK( caching ) * 3;

    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, flags,
                                 bufsize, 0, pwf, NULL);
    CoTaskMemFree(pwf);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot initialize audio client (error 0x%lX)", hr);
        return NULL;
    }
    return es_out_Add(demux->out, &fmt);
}

typedef struct
{
    IAudioClient *client;
    es_out_id_t *es;

    size_t frame_size;
    vlc_tick_t caching;
    vlc_tick_t start_time;

    HANDLE events[2];
    union {
        HANDLE thread;
        HANDLE ready;
    };
} demux_sys_t;

static unsigned __stdcall Thread(void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    IAudioCaptureClient *capture = NULL;
    void *pv;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr)); /* COM already allocated by parent thread */
    SetEvent(sys->ready);

    hr = IAudioClient_GetService(sys->client, &IID_IAudioCaptureClient, &pv);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot get capture client (error 0x%lX)", hr);
        goto out;
    }
    capture = pv;

    hr = IAudioClient_Start(sys->client);
    if (FAILED(hr))
    {
        msg_Err(demux, "cannot start client (error 0x%lX)", hr);
        IAudioCaptureClient_Release(capture);
        goto out;
    }

    while (WaitForMultipleObjects(2, sys->events, FALSE, INFINITE)
            != WAIT_OBJECT_0)
    {
        BYTE *buf;
        UINT32 frames;
        DWORD flags;
        UINT64 qpc;
        vlc_tick_t pts;

        hr = IAudioCaptureClient_GetBuffer(capture, &buf, &frames, &flags,
                                           NULL, &qpc);
        if (hr != S_OK)
            continue;

        pts = vlc_tick_now() - VLC_TICK_FROM_MSFTIME(GetQPC() - qpc);

        es_out_SetPCR(demux->out, pts);

        size_t bytes = frames * sys->frame_size;
        block_t *block = block_Alloc(bytes);

        if (likely(block != NULL)) {
            memcpy(block->p_buffer, buf, bytes);
            block->i_nb_samples = frames;
            block->i_pts = block->i_dts = pts;
            es_out_Send(demux->out, sys->es, block);
        }

        IAudioCaptureClient_ReleaseBuffer(capture, frames);
    }

    IAudioClient_Stop(sys->client);
    IAudioCaptureClient_Release(capture);
out:
    CoUninitialize();
    return 0;
}

static int Control(demux_t *demux, int query, va_list ap)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_TIME:
            *(va_arg(ap, vlc_tick_t *)) = vlc_tick_now() - sys->start_time;
            break;

        case DEMUX_GET_PTS_DELAY:
            *(va_arg(ap, vlc_tick_t *)) = sys->caching;
            break;

        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
            *(va_arg(ap, bool *)) = false;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    HRESULT hr;

    if (demux->out == NULL)
        return VLC_EGENERIC;

    if (demux->psz_location != NULL && *demux->psz_location != '\0')
        return VLC_EGENERIC; /* TODO non-default device */

    demux_sys_t *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->client = NULL;
    sys->es = NULL;
    sys->caching = VLC_TICK_FROM_MS( var_InheritInteger(obj, "live-caching") );
    sys->start_time = vlc_tick_now();
    for (unsigned i = 0; i < 2; i++)
        sys->events[i] = NULL;

    for (unsigned i = 0; i < 2; i++) {
        sys->events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (sys->events[i] == NULL)
            goto error;
    }

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (unlikely(FAILED(hr))) {
        msg_Err(demux, "cannot initialize COM (error 0x%lX)", hr);
        goto error;
    }

    bool loopback;
    sys->client = GetClient(demux, &loopback);
    if (sys->client == NULL) {
        CoUninitialize();
        goto error;
    }

    sys->es = CreateES(demux, sys->client, loopback, sys->caching,
                       &sys->frame_size);
    if (sys->es == NULL)
        goto error;

    hr = IAudioClient_SetEventHandle(sys->client, sys->events[1]);
    if (FAILED(hr)) {
        msg_Err(demux, "cannot set event handle (error 0x%lX)", hr);
        goto error;
    }

    demux->p_sys = sys;

    sys->ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (sys->ready == NULL)
        goto error;

    uintptr_t h = _beginthreadex(NULL, 0, Thread, demux, 0, NULL);
    if (h != 0)
        WaitForSingleObject(sys->ready, INFINITE);
    CloseHandle(sys->ready);

    sys->thread = (HANDLE)h;
    if (sys->thread == NULL)
        goto error;
    CoUninitialize();

    demux->pf_demux = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    if (sys->es != NULL)
        es_out_Del(demux->out, sys->es);
    if (sys->client != NULL)
    {
        IAudioClient_Release(sys->client);
        CoUninitialize();
    }
    for (unsigned i = 0; i < 2; i++)
        if (sys->events[i] != NULL)
            CloseHandle(sys->events[i]);
    return VLC_ENOMEM;
}

static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr));

    SetEvent(sys->events[0]);
    WaitForSingleObject(sys->thread, INFINITE);
    CloseHandle(sys->thread);

    es_out_Del(demux->out, sys->es);
    IAudioClient_Release(sys->client);
    CoUninitialize();
    for (unsigned i = 0; i < 2; i++)
        CloseHandle(sys->events[i]);
}

#define LOOPBACK_TEXT N_("Loopback mode")
#define LOOPBACK_LONGTEXT N_("Record an audio rendering endpoint.")

vlc_module_begin()
    set_shortname(N_("WASAPI"))
    set_description(N_("Windows Audio Session API input"))
    set_capability("access", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    add_bool("wasapi-loopback", false, LOOPBACK_TEXT, LOOPBACK_LONGTEXT, true)

    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
