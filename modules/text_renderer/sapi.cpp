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
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>

#include <initguid.h>

#include <memory>

// not available in standard libraries and used in inline functions without __uuidof()
DEFINE_GUID(CLSID_SpObjectTokenCategory, 0xa910187f, 0x0c7a, 0x45ac, 0x92,0xcc, 0x59,0xed,0xaf,0xb7,0x7b,0x53);

extern "C" {
static int Create (filter_t *);
static void Destroy(filter_t *);
static subpicture_region_t *RenderText(filter_t *,
                      const subpicture_region_t *,
                      const vlc_fourcc_t *);
}

vlc_module_begin ()
 set_description(N_("Speech synthesis for Windows"))

 set_subcategory(SUBCAT_VIDEO_SUBPIC)

 set_callback_text_renderer(Create, 0)
 /* Note: Skip label translation - too technical */
 add_integer("sapi-voice", -1, "Voice Index", "Voice Index")
vlc_module_end ()

namespace {

enum class FilterCommand {
    CMD_RENDER_TEXT,
    CMD_EXIT,
};

struct filter_sapi
{
    /* We need a MTA thread, so we ensure it's possible by creating a
     * dedicated thread for that mode. */
    vlc_thread_t thread;

    ISpVoice* cpVoice;
    char* lastString;

    bool initialized;
    vlc::threads::semaphore sem_ready;
    vlc::threads::semaphore cmd_available;
    vlc::threads::semaphore cmd_ready;

    struct {
        FilterCommand query;
        union {
            struct {
                int result;
                const subpicture_region_t *region;
            } render_text;
        };
    } cmd;
};

struct MTAGuard
{
    HRESULT result_mta;

    MTAGuard()
    {
        this->result_mta = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    }

    ~MTAGuard()
    {
        if (SUCCEEDED(this->result_mta))
            CoUninitialize();
    }

};
}

static int RenderTextMTA(filter_t *p_filter,
        const subpicture_region_t * p_region_in)
{
    struct filter_sapi *p_sys = static_cast<struct filter_sapi *>( p_filter->p_sys );
    const text_segment_t *p_segment = p_region_in->p_text;

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
        if (p_sys->lastString == nullptr)
            continue;

        msg_Dbg(p_filter, "Speaking '%s'", s->psz_text);

        wchar_t* wideText = ToWide(s->psz_text);
        HRESULT hr = p_sys->cpVoice->Speak(wideText, SPF_ASYNC, NULL);
        free(wideText);
        if (!SUCCEEDED(hr))
            msg_Err(p_filter, "Speak() error");
    }

    return VLC_SUCCESS;
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

static subpicture_region_t *RenderText(filter_t *p_filter,
        const subpicture_region_t *region_in,
        const vlc_fourcc_t *)
{
    auto *sys = static_cast<struct filter_sapi *>( p_filter->p_sys );
    sys->cmd.query = FilterCommand::CMD_RENDER_TEXT;
    sys->cmd.render_text.region = region_in;
    sys->cmd_available.post();
    sys->cmd_ready.wait();
    return NULL; /* We don't generate output region. */
}

static const struct vlc_filter_operations filter_ops = []{
    struct vlc_filter_operations ops {};
    ops.render = RenderText;
    ops.close  = Destroy;
    return ops;
}();

static void *Run(void *opaque)
{
    filter_t *filter = static_cast<filter_t *>(opaque);
    filter_sapi *sys = static_cast<filter_sapi *>(filter->p_sys);

    MTAGuard guard {};
    if (FAILED(guard.result_mta))
    {
        sys->sem_ready.post();
        return NULL;
    }


    HRESULT hr;
    hr = CoCreateInstance(__uuidof(SpVoice), NULL, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&sys->cpVoice));
    if (!SUCCEEDED(hr))
    {
        msg_Err(filter, "Could not create SpVoice");
        sys->sem_ready.post();
        return NULL;
    }

    int ret = SelectVoice(filter, sys->cpVoice);
    (void) ret; /* TODO: we can detect whether we set the voice or not */

    sys->initialized = true;
    sys->sem_ready.post();

    for (;;)
    {
        sys->cmd_available.wait();
        switch (sys->cmd.query)
        {
            case FilterCommand::CMD_EXIT:
                return NULL;
            case FilterCommand::CMD_RENDER_TEXT:
                sys->cmd.render_text.result =
                    RenderTextMTA(filter, sys->cmd.render_text.region);
                sys->cmd_ready.post();
                break;
            default:
                vlc_assert_unreachable();
        }
    }

    return NULL;
}

static int Create (filter_t *p_filter)
{
    std::unique_ptr<filter_sapi> sys {
        new (std::nothrow) filter_sapi {}
    };

    if (sys == nullptr)
        return VLC_ENOMEM;

    p_filter->ops = &filter_ops;
    p_filter->p_sys = sys.get();
    std::unique_ptr<filter_t, void(*)(filter_t*)> guard {
        p_filter, [](filter_t *filter)
    {
        filter->p_sys = nullptr;
        filter->ops = nullptr;
    }};


    int ret = vlc_clone(&sys->thread, Run, p_filter);

    if (ret != VLC_SUCCESS)
        return VLC_ENOMEM;

    sys->sem_ready.wait();
    if (!sys->initialized)
    {
        vlc_join(sys->thread, NULL);
        return VLC_ENOTSUP;
    }

    guard.release(); /* No need to clean filter. */
    sys.release(); /* leak to p_filter */
    return VLC_SUCCESS;
}

static void Destroy(filter_t *p_filter)
{
    std::unique_ptr<filter_sapi> sys {
        static_cast<filter_sapi *>(p_filter->p_sys)
    };

    if (sys->cpVoice)
        sys->cpVoice->Release();
    free(sys->lastString);

    sys->cmd.query = FilterCommand::CMD_EXIT;
    sys->cmd_available.post();
    vlc_join(sys->thread, NULL);
}
