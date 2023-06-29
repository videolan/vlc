/*****************************************************************************
 * sapi.cpp: Simple text to Speech renderer for Windows, based on SAPI
 *****************************************************************************
 * Copyright (c) 2015 Moti Zilberman
 * Copyright (c) 2023 Videolabs
 *
 * Authors: Moti Zilberman
 *          Jean-Baptiste Kempf
 *          Alexandre Janniaux <ajanni@videolabs.io>
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_charset.h>
#include <vlc_subpicture.h>

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>

#include <initguid.h>
// not available in standard libraries and used in inline functions without __uuidof()
DEFINE_GUID(CLSID_SpObjectTokenCategory, 0xa910187f, 0x0c7a, 0x45ac, 0x92,0xcc, 0x59,0xed,0xaf,0xb7,0x7b,0x53);

extern "C" {
static int Create (filter_t *);
static void Destroy(filter_t *);
static int RenderText(filter_t *,
                      subpicture_region_t *,
                      subpicture_region_t *,
                      const vlc_fourcc_t *);
}

vlc_module_begin ()
 set_description(N_("Speech synthesis for Windows"))

 set_subcategory(SUBCAT_VIDEO_SUBPIC)

 set_callback_text_renderer(Create, 0)
 /* Note: Skip label translation - too technical */
 add_integer("sapi-voice", -1, "Voice Index", nullptr)
vlc_module_end ()

namespace {
struct filter_sys_t
{
    ISpVoice* cpVoice;
    char* lastString;
};

struct MTAGuard
{
    HRESULT result_mta;

    MTAGuard()
    {
        this->result_mta = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }

    ~MTAGuard()
    {
        if (SUCCEEDED(this->result_mta))
            CoUninitialize();
    }

};
}

static int SelectVoice(filter_t *filter, ISpVoice* cpVoice)
{
    HRESULT hr;
    ISpObjectToken*        cpVoiceToken = NULL;
    IEnumSpObjectTokens*   cpEnum = NULL;
    ULONG ulCount = 0;

    int voiceIndex = var_InheritInteger(filter, "sapi-voice");
    if (voiceIndex < 0)
        return 0;

    hr = SpEnumTokens(SPCAT_VOICES, NULL, NULL, &cpEnum);
    if (!SUCCEEDED(hr))
        return -ENOENT;

    // Get the number of voices.
    hr = cpEnum->GetCount(&ulCount);
    if (!SUCCEEDED (hr))
        goto error;

    if ((unsigned)voiceIndex >= ulCount) {
        msg_Err(filter, "Voice index exceeds available count");
        cpEnum->Release();
        return -EINVAL;
    }
    hr = cpEnum->Item(voiceIndex, &cpVoiceToken);
    if (!SUCCEEDED(hr))
        goto error;

    hr = cpVoice->SetVoice(cpVoiceToken);
    if (SUCCEEDED(hr)) {
        msg_Dbg(filter, "Selected voice %d", voiceIndex);
    }
    else {
        msg_Err(filter, "Failed to set voice %d", voiceIndex);
    }
    cpVoiceToken->Release();
    cpVoiceToken = NULL;
    cpEnum->Release();

    return voiceIndex;

error:
    cpEnum->Release();
    return -ENOENT;
}

static const struct vlc_filter_operations filter_ops = []{
    struct vlc_filter_operations ops {};
    ops.render = RenderText;
    ops.close  = Destroy;
    return ops;
}();

static int Create (filter_t *p_filter)
{
    filter_sys_t *p_sys;
    HRESULT hr;

    MTAGuard guard {};
    if (FAILED(guard.result_mta))
        return VLC_EGENERIC;

    p_filter->p_sys = p_sys = (filter_sys_t*) malloc(sizeof(filter_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->cpVoice = NULL;
    p_sys->lastString = NULL;

    hr = CoCreateInstance(__uuidof(SpVoice), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&p_sys->cpVoice));
    if (!SUCCEEDED(hr))
    {
        msg_Err(p_filter, "Could not create SpVoice");
        free(p_sys);
        return VLC_ENOTSUP;
    }

    int ret = SelectVoice(p_filter, p_sys->cpVoice);
    (void) ret; /* TODO: we can detect whether we set the voice or not */

    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

static void Destroy(filter_t *p_filter)
{
    filter_sys_t *p_sys = reinterpret_cast<filter_sys_t *>( p_filter->p_sys );

    if (p_sys->cpVoice)
        p_sys->cpVoice->Release();

    free(p_sys->lastString);
    free(p_sys);
}

static int RenderText(filter_t *p_filter,
        subpicture_region_t *,
        subpicture_region_t *p_region_in,
        const vlc_fourcc_t *)
{
    filter_sys_t *p_sys = reinterpret_cast<filter_sys_t *>( p_filter->p_sys );
    text_segment_t *p_segment = p_region_in->p_text;

    if (!p_segment)
        return VLC_EGENERIC;

    for (const text_segment_t *s = p_segment; s != NULL; s = s->p_next ) {
        if (!s->psz_text)
            continue;

        if (strlen(s->psz_text) == 0)
            continue;

        if (p_sys->lastString && !strcmp(p_sys->lastString, s->psz_text))
            continue;

        if (!strcmp(s->psz_text, "\n"))
            continue;

        /* */
        free(p_sys->lastString);
        p_sys->lastString = strdup(s->psz_text);

        /* */
        if (p_sys->lastString) {
            msg_Dbg(p_filter, "Speaking '%s'", s->psz_text);

            MTAGuard guard {};
            if (FAILED(guard.result_mta))
                abort();
            wchar_t* wideText = ToWide(s->psz_text);
            HRESULT hr = p_sys->cpVoice->Speak(wideText, SPF_ASYNC, NULL);
            free(wideText);
            if (!SUCCEEDED(hr)) {
                msg_Err(p_filter, "Speak() error");
            }
        }
    }

    /* Return an error since we won't render the subtitle into pixmap. */
    return VLC_ENOTSUP;
}
