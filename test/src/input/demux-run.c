/**
 * @file demux-run.c
 */
/*****************************************************************************
 * Copyright © 2016 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Rémi Denis-Courmont reserves the right to redistribute this file under
 * the terms of the GNU Lesser General Public License as published by the
 * the Free Software Foundation; either version 2.1 or the License, or
 * (at his option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_block.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_es_out.h>
#include <vlc_url.h>
#include "../lib/libvlc_internal.h"

#include <vlc/vlc.h>

#include "demux-run.h"
#include "decoder.h"

struct test_es_out_t
{
    struct es_out_t out;
    struct es_out_id_t *ids;
};

struct es_out_id_t
{
    struct es_out_id_t *next;
#ifdef HAVE_DECODERS
    decoder_t *decoder;
#endif
};

static es_out_id_t *EsOutAdd(es_out_t *out, const es_format_t *fmt)
{
    struct test_es_out_t *ctx = (struct test_es_out_t *) out;

    if (fmt->i_group < 0)
        return NULL;

    es_out_id_t *id = malloc(sizeof (*id));
    if (unlikely(id == NULL))
        return NULL;

    id->next = ctx->ids;
    ctx->ids = id;
#ifdef HAVE_DECODERS
    id->decoder = test_decoder_create((void *)out->p_sys, fmt);
#endif

    debug("[%p] Added   ES\n", (void *)id);
    return id;
}

static void EsOutCheckId(es_out_t *out, es_out_id_t *id)
{
    struct test_es_out_t *ctx = (struct test_es_out_t *) out;

    for (es_out_id_t *ids = ctx->ids; ids != NULL; ids = ids->next)
        if (ids == id)
            return;

    abort();
}

static int EsOutSend(es_out_t *out, es_out_id_t *id, block_t *block)
{
    //debug("[%p] Sent    ES: %zu\n", (void *)idd, block->i_buffer);
    EsOutCheckId(out, id);
#ifdef HAVE_DECODERS
    if (id->decoder)
        test_decoder_process(id->decoder, block);
    else
#endif
        block_Release(block);
    return VLC_SUCCESS;
}

static void IdDelete(es_out_id_t *id)
{
#ifdef HAVE_DECODERS
    if (id->decoder)
    {
        /* Drain */
        test_decoder_process(id->decoder, NULL);
        test_decoder_destroy(id->decoder);
    }
#endif
    free(id);
}

static void EsOutDelete(es_out_t *out, es_out_id_t *id)
{
    struct test_es_out_t *ctx = (struct test_es_out_t *) out;
    es_out_id_t **pp = &ctx->ids;

    while (*pp != id)
    {
        if (*pp == NULL)
            abort();
        pp = &((*pp)->next);
    }

    debug("[%p] Deleted ES\n", (void *)id);
    *pp = id->next;
    IdDelete(id);
}

static int EsOutControl(es_out_t *out, int query, va_list args)
{
    switch (query)
    {
        case ES_OUT_SET_ES:
            break;
        case ES_OUT_RESTART_ES:
            abort();
        case ES_OUT_SET_ES_DEFAULT:
        case ES_OUT_SET_ES_STATE:
            break;
        case ES_OUT_GET_ES_STATE:
            EsOutCheckId(out, va_arg(args, es_out_id_t *));
            *va_arg(args, bool *) = true;
            break;
        case ES_OUT_SET_ES_CAT_POLICY:
            break;
        case ES_OUT_SET_GROUP:
            abort();
        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        case ES_OUT_RESET_PCR:
        case ES_OUT_SET_ES_FMT:
        case ES_OUT_SET_NEXT_DISPLAY_TIME:
        case ES_OUT_SET_GROUP_META:
        case ES_OUT_SET_GROUP_EPG:
        case ES_OUT_DEL_GROUP:
        case ES_OUT_SET_ES_SCRAMBLED_STATE:
            break;
        case ES_OUT_GET_EMPTY:
            *va_arg(args, bool *) = true;
            break;
        case ES_OUT_SET_META:
            break;
        case ES_OUT_GET_PCR_SYSTEM:
        case ES_OUT_MODIFY_PCR_SYSTEM:
            abort();
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void EsOutDestroy(es_out_t *out)
{
    struct test_es_out_t *ctx = (struct test_es_out_t *)out;
    es_out_id_t *id;

    while ((id = ctx->ids) != NULL)
    {
        ctx->ids = id->next;
        IdDelete(id);
    }
    free(ctx);
}

static es_out_t *test_es_out_create(vlc_object_t *parent)
{
    struct test_es_out_t *ctx = malloc(sizeof (*ctx));
    if (ctx == NULL)
    {
        fprintf(stderr, "Error: cannot create ES output.\n");
        return NULL;
    }

    ctx->ids = NULL;

    es_out_t *out = &ctx->out;
    out->pf_add = EsOutAdd;
    out->pf_send = EsOutSend;
    out->pf_del = EsOutDelete;
    out->pf_control = EsOutControl;
    out->pf_destroy = EsOutDestroy;
    out->p_sys = (void *)parent;

    return out;
}

static unsigned demux_test_and_clear_flags(demux_t *demux, unsigned flags)
{
    unsigned update;
    if (demux_Control(demux, DEMUX_TEST_AND_CLEAR_FLAGS, &update) == VLC_SUCCESS)
        return update;
    unsigned ret = demux->info.i_update & flags;
    demux->info.i_update &= ~flags;
    return ret;
}

static void demux_get_title_list(demux_t *demux)
{
    int title;
    int title_offset;
    int seekpoint_offset;
    input_title_t **title_list;

    if (demux_Control(demux, DEMUX_GET_TITLE_INFO, &title_list, &title,
                      &title_offset, &seekpoint_offset) == VLC_SUCCESS)
    {
        for (int i = 0; i < title; i++)
            vlc_input_title_Delete(title_list[i]);
    }
}

static void demux_get_meta(demux_t *demux)
{
    vlc_meta_t *p_meta = vlc_meta_New();
    if (unlikely(p_meta == NULL) )
        return;

    input_attachment_t **attachment;
    int i_attachment;

    demux_Control(demux, DEMUX_GET_META, p_meta);
    demux_Control(demux, DEMUX_GET_ATTACHMENTS, &attachment, &i_attachment);

    vlc_meta_Delete(p_meta);
}

static int demux_process_stream(const struct vlc_run_args *args, stream_t *s)
{
    const char *name = args->name;
    if (name == NULL)
        name = "any";

    if (s == NULL)
        return -1;

    es_out_t *out = test_es_out_create(VLC_OBJECT(s));
    if (out == NULL)
        return -1;

    demux_t *demux = demux_New(VLC_OBJECT(s), name, "", s, out);
    if (demux == NULL)
    {
        es_out_Delete(out);
        vlc_stream_Delete(s);
        debug("Error: cannot create demultiplexer: %s\n", name);
        return -1;
    }

    uintmax_t i = 0;
    int val;

    while ((val = demux_Demux(demux)) == VLC_DEMUXER_SUCCESS)
    {
        if (args->test_demux_controls)
        {
            if (demux_test_and_clear_flags(demux, INPUT_UPDATE_TITLE_LIST))
                demux_get_title_list(demux);

            if (demux_test_and_clear_flags(demux, INPUT_UPDATE_META))
                demux_get_meta(demux);

            int seekpoint = 0;
            double position = 0.0;
            mtime_t time = 0;
            mtime_t length = 0;

            /* Call controls for increased code coverage */
            demux_Control(demux, DEMUX_GET_SEEKPOINT, &seekpoint);
            demux_Control(demux, DEMUX_GET_POSITION, &position);
            demux_Control(demux, DEMUX_GET_TIME, &time);
            demux_Control(demux, DEMUX_GET_LENGTH, &length);
        }
        i++;
    }

    demux_Delete(demux);
    es_out_Delete(out);

    debug("Completed with %ju iteration(s).\n", i);

    return val == VLC_DEMUXER_EOF ? 0 : -1;
}

int vlc_demux_process_url(const struct vlc_run_args *args, const char *url)
{
    libvlc_instance_t *vlc = libvlc_create(args);
    if (vlc == NULL)
        return -1;

    stream_t *s = vlc_access_NewMRL(VLC_OBJECT(vlc->p_libvlc_int), url);
    if (s == NULL)
        fprintf(stderr, "Error: cannot create input stream: %s\n", url);

    int ret = demux_process_stream(args, s);
    libvlc_release(vlc);
    return ret;
}

int vlc_demux_process_path(const struct vlc_run_args *args, const char *path)
{
    char *url = vlc_path2uri(path, NULL);
    if (url == NULL)
    {
        fprintf(stderr, "Error: cannot convert path to URL: %s\n", path);
        return -1;
    }

    int ret = vlc_demux_process_url(args, url);
    free(url);
    return ret;
}

int vlc_demux_process_memory(const struct vlc_run_args *args,
                             const unsigned char *buf, size_t length)
{
    libvlc_instance_t *vlc = libvlc_create(args);
    if (vlc == NULL)
        return -1;

    stream_t *s = vlc_stream_MemoryNew(VLC_OBJECT(vlc->p_libvlc_int),
                                       (void *)buf, length, true);
    if (s == NULL)
        fprintf(stderr, "Error: cannot create input stream\n");

    int ret = demux_process_stream(args, s);
    libvlc_release(vlc);
    return ret;
}

#ifdef HAVE_STATIC_MODULES
# include <vlc_plugin.h>

typedef int (*vlc_plugin_cb)(int (*)(void *, void *, int, ...), void *);
extern vlc_plugin_cb vlc_static_modules[];

#ifdef HAVE_DECODERS
#define DECODER_PLUGINS(f) \
    f(adpcm) \
    f(aes3) \
    f(araw) \
    f(g711) \
    f(lpcm) \
    f(uleaddvaudio) \
    f(rawvideo) \
    f(cc) \
    f(cvdsub) \
    f(dvbsub) \
    f(scte18) \
    f(scte27) \
    f(spudec) \
    f(stl) \
    f(subsdec) \
    f(subsusf) \
    f(svcdsub) \
    f(textst) \
    f(substx3g)
#else
#define DECODER_PLUGINS(f)
#endif

#define PLUGINS(f) \
    f(xml) \
    f(console) \
    f(filesystem) \
    f(xml) \
    f(aiff) \
    f(asf) \
    f(au) \
    f(avi) \
    f(caf) \
    f(diracsys) \
    f(es) \
    f(flacsys) \
    f(h26x) \
    f(mjpeg) \
    f(mkv) \
    f(mp4) \
    f(nsc) \
    f(nsv) \
    f(ps) \
    f(pva) \
    f(sap) \
    f(smf) \
    f(subtitle) \
    PLUGIN_TS(f) \
    f(tta) \
    f(ttml) \
    f(ty) \
    f(voc) \
    f(wav) \
    f(webvtt) \
    f(xa) \
    f(a52) \
    f(copy) \
    f(dirac) \
    f(dts) \
    f(flac) \
    f(h264) \
    f(hevc) \
    f(mlp) \
    f(mpeg4audio) \
    f(mpeg4video) \
    f(mpegaudio) \
    f(mpegvideo) \
    f(vc1) \
    f(rawvid) \
    f(rawaud) \
    DECODER_PLUGINS(f)

#ifdef HAVE_DVBPSI
# define PLUGIN_TS(f) f(ts)
#else
# define PLUGIN_TS(f)
#endif

#define DECL_PLUGIN(p) \
    int vlc_entry__##p(int (*)(void *, void *, int, ...), void *);

#define FUNC_PLUGIN(p) \
    vlc_entry__##p,

PLUGINS(DECL_PLUGIN)

__attribute__((visibility("default")))
vlc_plugin_cb vlc_static_modules[] = { PLUGINS(FUNC_PLUGIN) NULL };
#endif /* HAVE_STATIC_MODULES */
