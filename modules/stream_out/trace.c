/*****************************************************************************
 * trace.c: Trace frame timestamps and PCRs
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#include <vlc_configuration.h>
#include <vlc_frame.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tracer.h>

typedef struct
{
    const char *es_id;
    void *next_id;
} sout_stream_id_sys_t;

static void *
Add(sout_stream_t *stream, const es_format_t *fmt, const char *es_id)
{
    sout_stream_id_sys_t *id = malloc(sizeof(*id));
    if (unlikely(id == NULL))
        return NULL;

    id->es_id = es_id;
    id->next_id = sout_StreamIdAdd(stream->p_next, fmt, es_id);
    if (id->next_id == NULL)
        FREENULL(id);
    return id;
}

static int Send(sout_stream_t *stream, void *id, vlc_frame_t *frame)
{
    sout_stream_id_sys_t *sys_id = id;

    struct vlc_tracer *tracer = vlc_object_get_tracer(VLC_OBJECT(stream));
    if (tracer != NULL)
    {
        if (frame->i_dts != VLC_TICK_INVALID)
        {
            vlc_tracer_TraceStreamDTS(tracer,
                                      stream->p_sys,
                                      sys_id->es_id,
                                      "IN",
                                      frame->i_pts,
                                      frame->i_dts);
        }
        else
        {
            vlc_tracer_TraceStreamPTS(
                tracer, stream->p_sys, sys_id->es_id, "IN", frame->i_pts);
        }
    }

    return sout_StreamIdSend(stream->p_next, sys_id->next_id, frame);
}

static void Del(sout_stream_t *stream, void *id)
{
    sout_stream_id_sys_t *sys_id = id;
    sout_StreamIdDel(stream->p_next, sys_id->next_id);
    free(sys_id);
}

static void SetPCR(sout_stream_t *stream, vlc_tick_t pcr)
{
    struct vlc_tracer *tracer = vlc_object_get_tracer(VLC_OBJECT(stream));
    if (tracer != NULL)
        vlc_tracer_TracePCR(tracer, stream->p_sys, "PCR", pcr);

    sout_StreamSetPCR(stream->p_next, pcr);
}

static int Control(sout_stream_t *stream, int query, va_list args)
{
    if (query == SOUT_STREAM_ID_SPU_HIGHLIGHT)
    {
        sout_stream_id_sys_t *sys_id = va_arg(args, void *);
        return sout_StreamControl(
            stream->p_next, query, sys_id->next_id, va_arg(args, void *));
    }
    return sout_StreamControlVa(stream->p_next, query, args);
}

static void Flush(sout_stream_t *stream, void *id)
{
    sout_stream_id_sys_t *sys_id = id;
    struct vlc_tracer *tracer = vlc_object_get_tracer(VLC_OBJECT(stream));
    if (tracer != NULL)
    {
        vlc_tracer_TraceEvent(tracer, stream->p_sys, sys_id->es_id, "FLUSH");
    }
    sout_StreamFlush(stream->p_next, sys_id->next_id);
}

static void Close(sout_stream_t *stream)
{
    free(stream->p_sys);
}

#define SOUT_CFG_PREFIX "sout-trace-"

static int Open(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;
    static const char *sout_options[] = {"name", NULL};
    config_ChainParse(stream, SOUT_CFG_PREFIX, sout_options, stream->p_cfg);

    char *name_in_cfg = var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "name");
    stream->p_sys =
        (name_in_cfg != NULL) ? name_in_cfg : strdup(stream->p_next->psz_name);
    if (unlikely(stream->p_sys == NULL))
        return VLC_ENOMEM;

    static const struct sout_stream_operations ops = {
        .add = Add,
        .del = Del,
        .send = Send,
        .set_pcr = SetPCR,
        .control = Control,
        .flush = Flush,
        .close = Close,
    };
    stream->ops = &ops;

    return VLC_SUCCESS;
}

#define HELP_TEXT                                                              \
    N_("This filter module traces all frames and timestamps passing through "  \
       "it using the VLC tracer."                                              \
       "Here's a stream output chain example:\n"                               \
       "  #autodel:trace:transcode{...}:trace:file{...}\n "                    \
       "The name of the traced output defaults to the next module's name. A "  \
       "custom name can be passed for complex `duplicate` chains or VLM "      \
       "scenario where mutliple stages of the chain are traced:\n"             \
       "  #duplicate{dst=trace{name=dup1}:dummy,dst=trace{name=dup2}:dummy}")

#define NAME_TEXT N_("Name of the traced output")
#define NAME_LONGTEXT                                                          \
    N_("Give meaning and context to the traced output. Defaults to the next "  \
       "stream name")

vlc_module_begin()
    set_shortname(N_("Trace"))
    set_description(N_("Trace frame timestamps and PCR events"))
    set_capability("sout filter", 0)
    set_help(HELP_TEXT)
    add_shortcut("trace")
    set_callback(Open)

    add_string(SOUT_CFG_PREFIX "name", NULL, NAME_TEXT, NAME_LONGTEXT)
vlc_module_end()
