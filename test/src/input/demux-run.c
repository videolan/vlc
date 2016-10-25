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
#include <vlc_es_out.h>
#include <vlc_url.h>
#include "../lib/libvlc_internal.h"

#include <vlc/vlc.h>

#include "demux-run.h"

#if 0
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) (void)0
#endif

struct test_es_out_t
{
    struct es_out_t out;
    struct es_out_id_t *ids;
};

struct es_out_id_t
{
    struct es_out_id_t *next;
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
    block_Release(block);
    return VLC_SUCCESS;
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
    free(id);
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
        free(id);
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

static int demux_process_stream(const char *name, stream_t *s)
{
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
         i++;

    demux_Delete(demux);
    es_out_Delete(out);

    debug("Completed with %ju iteration(s).\n", i);

    return val == VLC_DEMUXER_EOF ? 0 : -1;
}

static libvlc_instance_t *libvlc_create(void)
{
    const char *argv[] = {
        NULL
    };
    unsigned argc = (sizeof (argv) / sizeof (*argv)) - 1;

#ifdef TOP_BUILDDIR
# ifndef HAVE_STATIC_MODULES
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
# endif
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
#endif

    libvlc_instance_t *vlc = libvlc_new(argc, argv);
    if (vlc == NULL)
        fprintf(stderr, "Error: cannot initialize LibVLC.\n");
    return vlc;
}

int vlc_demux_process_url(const char *demux, const char *url)
{
    libvlc_instance_t *vlc = libvlc_create();
    if (vlc == NULL)
        return -1;

    stream_t *s = vlc_access_NewMRL(VLC_OBJECT(vlc->p_libvlc_int), url);
    if (s == NULL)
        fprintf(stderr, "Error: cannot create input stream: %s\n", url);

    int ret = demux_process_stream(demux, s);
    libvlc_release(vlc);
    return ret;
}

int vlc_demux_process_path(const char *demux, const char *path)
{
    char *url = vlc_path2uri(path, NULL);
    if (url == NULL)
    {
        fprintf(stderr, "Error: cannot convert path to URL: %s\n", path);
        return -1;
    }

    int ret = vlc_demux_process_url(demux, url);
    free(url);
    return ret;
}

int vlc_demux_process_memory(const char *demux,
                             const unsigned char *buf, size_t length)
{
    libvlc_instance_t *vlc = libvlc_create();
    if (vlc == NULL)
        return -1;

    stream_t *s = vlc_stream_MemoryNew(VLC_OBJECT(vlc->p_libvlc_int),
                                       (void *)buf, length, true);
    if (s == NULL)
        fprintf(stderr, "Error: cannot create input stream\n");

    int ret = demux_process_stream(demux, s);
    libvlc_release(vlc);
    return ret;
}

#ifdef HAVE_STATIC_MODULES
# include <vlc_plugin.h>

typedef int (*vlc_plugin_cb)(int (*)(void *, void *, int, ...), void *);
extern vlc_plugin_cb vlc_static_modules[];

#define PLUGINS(f) \
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
    f(xa) \
    f(a52) \
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
    f(vc1)
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
