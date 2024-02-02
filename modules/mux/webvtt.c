/*****************************************************************************
 * webvtt.c: muxer for raw WEBVTT
 *****************************************************************************
 * Copyright (C) 2024 VideoLabs, VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_codec.h>
#include <vlc_memstream.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include "../codec/webvtt/webvtt.h"
#include "../demux/mp4/minibox.h"

typedef struct
{
    bool header_done;
    const sout_input_t *input;
} sout_mux_sys_t;

static void OutputTime(struct vlc_memstream *ms, vlc_tick_t time)
{
    const vlc_tick_t secs = SEC_FROM_VLC_TICK(time);
    vlc_memstream_printf(ms,
                         "%02" PRIi64 ":%02" PRIi64 ":%02" PRIi64 ".%03" PRIi64,
                         secs / 3600,
                         secs % 3600 / 60,
                         secs % 60,
                         (time % CLOCK_FREQ) / 1000);
}

static block_t *FormatCue(const webvtt_cue_t *cue)
{
    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms))
        return NULL;

    if (cue->psz_id != NULL)
        vlc_memstream_printf(&ms, "%s\n", cue->psz_id);

    OutputTime(&ms, cue->i_start);
    vlc_memstream_printf(&ms, " --> ");
    OutputTime(&ms, cue->i_stop);

    if (cue->psz_attrs != NULL)
        vlc_memstream_printf(&ms, " %s\n", cue->psz_attrs);
    else
        vlc_memstream_putc(&ms, '\n');

    vlc_memstream_printf(&ms, "%s\n\n", cue->psz_text);

    if (vlc_memstream_close(&ms))
        return NULL;

    block_t *formatted = block_heap_Alloc(ms.ptr, ms.length);
    formatted->i_length = cue->i_stop - cue->i_start;
    formatted->i_pts = formatted->i_dts = cue->i_stop - cue->i_start;
    return formatted;
}

static void UnpackISOBMFF(const vlc_frame_t *packed, webvtt_cue_t *out)
{
    mp4_box_iterator_t it;
    mp4_box_iterator_Init(&it, packed->p_buffer, packed->i_buffer);
    while (mp4_box_iterator_Next(&it))
    {
        if (it.i_type != ATOM_vttc && it.i_type != ATOM_vttx)
            continue;

        mp4_box_iterator_t vtcc;
        mp4_box_iterator_Init(&vtcc, it.p_payload, it.i_payload);
        while (mp4_box_iterator_Next(&vtcc))
        {
            switch (vtcc.i_type)
            {
                case ATOM_iden:
                    if (out->psz_id == NULL)
                        out->psz_id =
                            strndup((char *)vtcc.p_payload, vtcc.i_payload);
                    break;
                case ATOM_sttg:
                    if (out->psz_attrs)
                    {
                        char *dup = strndup((char *)vtcc.p_payload, vtcc.i_payload);
                        if (dup)
                        {
                            char *psz;
                            if (asprintf(&psz, "%s %s", out->psz_attrs, dup) >= 0)
                            {
                                free(out->psz_attrs);
                                out->psz_attrs = psz;
                            }
                            free(dup);
                        }
                    }
                    else
                        out->psz_attrs =
                            strndup((char *)vtcc.p_payload, vtcc.i_payload);
                    break;
                case ATOM_payl:
                    if (!out->psz_text)
                        out->psz_text =
                            strndup((char *)vtcc.p_payload, vtcc.i_payload);
                    break;
            }
        }
    }
}

static int Control(sout_mux_t *mux, int query, va_list args)
{
    switch (query)
    {
        case MUX_CAN_ADD_STREAM_WHILE_MUXING:
            *(va_arg(args, bool *)) = false;
            return VLC_SUCCESS;
        default:
            return VLC_ENOTSUP;
    }
    (void)mux;
}

static int AddStream(sout_mux_t *mux, sout_input_t *input)
{
    sout_mux_sys_t *sys = mux->p_sys;
    if ((input->fmt.i_codec != VLC_CODEC_WEBVTT &&
         input->fmt.i_codec != VLC_CODEC_TEXT) ||
        sys->input)
        return VLC_ENOTSUP;
    sys->input = input;
    return VLC_SUCCESS;
}

static void DelStream(sout_mux_t *mux, sout_input_t *input)
{
    sout_mux_sys_t *sys = mux->p_sys;
    if (input == sys->input)
        sys->input = NULL;
}

static int Mux(sout_mux_t *mux)
{
    sout_mux_sys_t *sys = mux->p_sys;

    if (mux->i_nb_inputs == 0)
        return VLC_SUCCESS;

    sout_input_t *input = mux->pp_inputs[0];

    if (!sys->header_done)
    {
        block_t *data = NULL;
        if (input->fmt.i_extra > 8 && !memcmp(input->fmt.p_extra, "WEBVTT", 6))
        {
            data = block_Alloc(input->fmt.i_extra + 2);

            if (unlikely(data == NULL))
                return VLC_ENOMEM;
            memcpy(data->p_buffer, input->fmt.p_extra, input->fmt.i_extra);
            data->p_buffer[data->i_buffer - 2] = '\n';
            data->p_buffer[data->i_buffer - 1] = '\n';
        }
        else
        {
            data = block_Alloc(8);
            if (unlikely(data == NULL))
                return VLC_ENOMEM;
            memcpy(data->p_buffer, "WEBVTT\n\n", 8);
        }

        if (data)
        {
            data->i_flags |= BLOCK_FLAG_HEADER;
            sout_AccessOutWrite(mux->p_access, data);
        }
        sys->header_done = true;
    }

    vlc_fifo_Lock(input->p_fifo);
    vlc_frame_t *chain = vlc_fifo_DequeueAllUnlocked(input->p_fifo);
    int status = VLC_SUCCESS;
    while (chain != NULL)
    {
        // Ephemer subtitles stop being displayed at the next SPU display time.
        // We need to delay if subsequent SPU aren't available yet.
        const bool is_ephemer = (chain->i_length == VLC_TICK_INVALID);
        if (is_ephemer && chain->p_next == NULL)
        {
            vlc_fifo_QueueUnlocked(input->p_fifo, chain);
            chain = NULL;
            break;
        }

        webvtt_cue_t cue = {};

        if (sys->input->fmt.i_codec != VLC_CODEC_WEBVTT)
            cue.psz_text = strndup((char *)chain->p_buffer, chain->i_buffer);
        else
            UnpackISOBMFF(chain, &cue);

        if (unlikely(cue.psz_text == NULL))
        {
            webvtt_cue_Clean(&cue);
            status = VLC_ENOMEM;
            break;
        }

        cue.i_start = chain->i_pts - VLC_TICK_0;
        if(is_ephemer)
            cue.i_stop = chain->p_next->i_pts - VLC_TICK_0;
        else
            cue.i_stop = cue.i_start + chain->i_length;
        block_t *to_write = FormatCue(&cue);
        webvtt_cue_Clean(&cue);

        ssize_t written = -1;
        if (likely(to_write != NULL))
            written = sout_AccessOutWrite(mux->p_access, to_write);
        if (written == -1)
        {
            status = VLC_EGENERIC;
            break;
        }

        vlc_frame_t *next = chain->p_next;
        vlc_frame_Release(chain);
        chain = next;
    }

    vlc_frame_ChainRelease(chain);
    vlc_fifo_Unlock(input->p_fifo);
    return status;
}
/*****************************************************************************
 * Open:
 *****************************************************************************/
int webvtt_OpenMuxer(vlc_object_t *this)
{
    sout_mux_t *mux = (sout_mux_t *)this;
    sout_mux_sys_t *sys;

    mux->p_sys = sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->header_done = false;
    sys->input = NULL;

    mux->pf_control = Control;
    mux->pf_addstream = AddStream;
    mux->pf_delstream = DelStream;
    mux->pf_mux = Mux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
void webvtt_CloseMuxer(vlc_object_t *this)
{
    sout_mux_t *mux = (sout_mux_t *)this;
    free(mux->p_sys);
}
