/*****************************************************************************
 * sapi.cpp: Simple text to Speech renderer for Windows, based on SAPI
 *****************************************************************************
 * Copyright (c) 2015 Moti Zilberman
 *
 * Authors: Moti Zilberman
 *          Jean-Baptiste Kempf
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

#define INITGUID

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>

static int Create (vlc_object_t *);
static void Destroy(vlc_object_t *);
static int RenderText(filter_t *,
                      subpicture_region_t *,
                      subpicture_region_t *,
                      const vlc_fourcc_t *);

vlc_module_begin ()
 set_description(N_("Speech synthesis for Windows"))

 set_category(CAT_VIDEO)
 set_subcategory(SUBCAT_VIDEO_SUBPIC)

 set_capability("text renderer", 0)
 set_callbacks(Create, Destroy)
 add_integer("sapi-voice", -1, "Voice Index", "Voice index", false)
vlc_module_end ()

struct filter_sys_t
{
    ISpVoice* cpVoice;
    char* lastString;
};

/* MTA functions */
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

static int Create (vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    HRESULT hr;

    if (TryEnterMTA(p_this))
        return VLC_EGENERIC;

    p_filter->p_sys = p_sys = (filter_sys_t*) malloc(sizeof(filter_sys_t));
    if (!p_sys)
        goto error;

    p_sys->cpVoice = NULL;
    p_sys->lastString = NULL;

    hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void**) &p_sys->cpVoice);
    if (SUCCEEDED(hr)) {
        ISpObjectToken*        cpVoiceToken = NULL;
        IEnumSpObjectTokens*   cpEnum = NULL;
        ULONG ulCount = 0;

        hr = SpEnumTokens(SPCAT_VOICES, NULL, NULL, &cpEnum);
        if (SUCCEEDED(hr))
        {
            // Get the number of voices.
            hr = cpEnum->GetCount(&ulCount);
            if (SUCCEEDED (hr))
            {
                int voiceIndex = var_InheritInteger(p_this, "sapi-voice");
                if (voiceIndex > -1)
                {
                    if ((unsigned)voiceIndex < ulCount) {
                        hr = cpEnum->Item(voiceIndex, &cpVoiceToken);
                        if (SUCCEEDED(hr)) {
                            hr = p_sys->cpVoice->SetVoice(cpVoiceToken);
                            if (SUCCEEDED(hr)) {
                                msg_Dbg(p_this, "Selected voice %d", voiceIndex);
                            }
                            else {
                                msg_Err(p_this, "Failed to set voice %d", voiceIndex);
                            }
                            cpVoiceToken->Release();
                            cpVoiceToken = NULL;
                        }
                    }
                    else
                        msg_Err(p_this, "Voice index exceeds available count");
                }
            }
            cpEnum->Release();

            /* Set Output */
            hr = p_sys->cpVoice->SetOutput(NULL, TRUE);
        }
    }
    else
    {
        msg_Err(p_filter, "Could not create SpVoice");
        goto error;
    }

    LeaveMTA();

    p_filter->pf_render = RenderText;

    return VLC_SUCCESS;

error:
    LeaveMTA();
    free(p_sys);
    return VLC_EGENERIC;
}

static void Destroy(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
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

            EnterMTA();
            wchar_t* wideText = ToWide(s->psz_text);
            HRESULT hr = p_sys->cpVoice->Speak(wideText, SPF_ASYNC, NULL);
            free(wideText);
            if (!SUCCEEDED(hr)) {
                msg_Err(p_filter, "Speak() error");
            }
            LeaveMTA();
        }
    }

    return VLC_SUCCESS;
}
