/*****************************************************************************
 * DBMHelper.cpp: Decklink SDI Helpers
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *             2018-2019 VideoLabs
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

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_decklink.h>

#include "DBMHelper.hpp"

#include <arpa/inet.h>

using namespace Decklink;

IDeckLinkDisplayMode * Helper::MatchDisplayMode(vlc_object_t *p_obj,
                                                IDeckLinkOutput *p_output,
                                                const video_format_t *fmt,
                                                BMDDisplayMode forcedmode)
{
    HRESULT result;
    IDeckLinkDisplayMode *p_selected = NULL;
    IDeckLinkDisplayModeIterator *p_iterator = NULL;

    for(int i=0; i<4 && p_selected==NULL; i++)
    {
        int i_width = (i % 2 == 0) ? fmt->i_width : fmt->i_visible_width;
        int i_height = (i % 2 == 0) ? fmt->i_height : fmt->i_visible_height;
        int i_div = (i > 2) ? 4 : 0;

        result = p_output->GetDisplayModeIterator(&p_iterator);
        if(result == S_OK)
        {
            IDeckLinkDisplayMode *p_mode = NULL;
            while(p_iterator->Next(&p_mode) == S_OK)
            {
                BMDDisplayMode mode_id = p_mode->GetDisplayMode();
                BMDTimeValue frameduration;
                BMDTimeScale timescale;
                const char *psz_mode_name;
                decklink_str_t tmp_name;

                if(p_mode->GetFrameRate(&frameduration, &timescale) == S_OK &&
                        p_mode->GetName(&tmp_name) == S_OK)
                {
                    BMDDisplayMode modenl = htonl(mode_id);
                    psz_mode_name = DECKLINK_STRDUP(tmp_name);
                    DECKLINK_FREE(tmp_name);

                    if(i==0)
                    {
                        BMDFieldDominance field = htonl(p_mode->GetFieldDominance());
                        msg_Dbg(p_obj, "Found mode '%4.4s': %s (%ldx%ld, %4.4s, %.3f fps, scale %ld dur %ld)",
                                (const char*)&modenl, psz_mode_name,
                                p_mode->GetWidth(), p_mode->GetHeight(),
                                (const char *)&field,
                                double(timescale) / frameduration,
                                timescale, frameduration);
                    }
                }
                else
                {
                    p_mode->Release();
                    continue;
                }

                if(forcedmode != bmdModeUnknown && unlikely(!p_selected))
                {
                    BMDDisplayMode modenl = htonl(forcedmode);
                    msg_Dbg(p_obj, "Forced mode '%4.4s'", (char *)&modenl);
                    if(forcedmode == mode_id)
                        p_selected = p_mode;
                    else
                        p_mode->Release();
                    continue;
                }

                if(p_selected == NULL && forcedmode == bmdModeUnknown)
                {
                    if(i_width >> i_div == p_mode->GetWidth() >> i_div &&
                       i_height >> i_div == p_mode->GetHeight() >> i_div)
                    {
                        unsigned int num_deck, den_deck;
                        unsigned int num_stream, den_stream;
                        vlc_ureduce(&num_deck, &den_deck, timescale, frameduration, 0);
                        vlc_ureduce(&num_stream, &den_stream,
                                    fmt->i_frame_rate, fmt->i_frame_rate_base, 0);

                        if (num_deck == num_stream && den_deck == den_stream)
                        {
                            msg_Info(p_obj, "Matches incoming stream");
                            p_selected = p_mode;
                            continue;
                        }
                    }
                }

                p_mode->Release();
            }
            p_iterator->Release();
        }
    }
    return p_selected;
}

const char * Helper::ErrorToString(long i_code)
{
    static struct
    {
        long i_return_code;
        const char * const psz_string;
    } const errors_to_string[] = {
        { E_UNEXPECTED,  "Unexpected error" },
        { E_NOTIMPL,     "Not implemented" },
        { E_OUTOFMEMORY, "Out of memory" },
        { E_INVALIDARG,  "Invalid argument" },
        { E_NOINTERFACE, "No interface" },
        { E_POINTER,     "Invalid pointer" },
        { E_HANDLE,      "Invalid handle" },
        { E_ABORT,       "Aborted" },
        { E_FAIL,        "Failed" },
        { E_ACCESSDENIED,"Access denied" }
    };

    for(size_t i=0; i<ARRAY_SIZE(errors_to_string); i++)
    {
        if(errors_to_string[i].i_return_code == i_code)
            return errors_to_string[i].psz_string;
    }
    return NULL;
}
