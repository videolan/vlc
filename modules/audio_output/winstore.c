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

#define INITGUID
#define COBJMACROS

#include <stdlib.h>
#include <assert.h>
#include <audiopolicy.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_modules.h>
#include "audio_output/mmdevice.h"

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

struct aout_sys_t
{
    aout_stream_t *stream; /**< Underlying audio output stream */
    module_t *module;
    IAudioClient *client;
};

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

    EnterMTA();
    aout_stream_Play(sys->stream, block);
    LeaveMTA();
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;

    EnterMTA();
    aout_stream_Pause(sys->stream, paused);
    LeaveMTA();

    (void) date;
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;

    EnterMTA();
    aout_stream_Flush(sys->stream, wait);
    LeaveMTA();
}

static HRESULT ActivateDevice(void *opaque, REFIID iid, PROPVARIANT *actparms,
                              void **restrict pv)
{
    aout_sys_t *sys = opaque;

    (void)iid; (void)actparms;
    *pv = sys->client;
    return S_OK;
}

static int aout_stream_Start(void *func, va_list ap)
{
    aout_stream_start_t start = func;
    aout_stream_t *s = va_arg(ap, aout_stream_t *);
    audio_sample_format_t *fmt = va_arg(ap, audio_sample_format_t *);
    HRESULT *hr = va_arg(ap, HRESULT *);

    *hr = start(s, fmt, &GUID_VLC_AUD_OUT);
    return SUCCEEDED(*hr) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void aout_stream_Stop(void *func, va_list ap)
{
    aout_stream_stop_t stop = func;
    aout_stream_t *s = va_arg(ap, aout_stream_t *);

    stop(s);
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;
    HRESULT hr;

    aout_stream_t *s = vlc_object_create(aout, sizeof (*s));
    if (unlikely(s == NULL))
        return -1;

    s->owner.device = sys->client;
    s->owner.activate = ActivateDevice;

    EnterMTA();
    sys->module = vlc_module_load(s, "aout stream", NULL, false,
                                  aout_stream_Start, s, fmt, &hr);
    LeaveMTA();

    if (sys->module == NULL)
    {
        vlc_object_release(s);
        return -1;
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
    vlc_module_unload(sys->module, aout_stream_Stop, sys->stream);
    LeaveMTA();

    vlc_object_release(sys->stream);
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
    sys->client = var_InheritAddress(aout, "mmdevice-audioclient");
    assert(sys->client != NULL);
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free(sys->client);
    free(sys);
}

vlc_module_begin()
    set_shortname("winstore")
    set_description(N_("Windows Store audio output"))
    set_capability("audio output", 150)
    /* Pointer to the activated AudioClient* */
    add_integer("winstore-audioclient", 0x0, NULL, NULL, true);
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("wasapi")
    set_callbacks(Open, Close)
vlc_module_end()
