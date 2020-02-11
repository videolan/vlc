/*****************************************************************************
 * es_out_source.c: Es Out Source handle
 *****************************************************************************
 * Copyright (C) 2020 VLC authors, VideoLAN and Videolabs SAS
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <vlc_common.h>

#include <vlc_atomic.h>
#include <vlc_es_out.h>
#include <vlc_block.h>

#include "input_internal.h"
#include "es_out.h"

typedef struct
{
    es_out_t out;
    input_source_t *in;
    es_out_t *parent_out;
} es_out_sys_t;

static es_out_id_t *EsOutSourceAdd(es_out_t *out, input_source_t *in,
                                    const es_format_t *fmt)
{
    assert(in == NULL);
    es_out_sys_t *sys = container_of(out, es_out_sys_t, out);
    return sys->parent_out->cbs->add(sys->parent_out, sys->in, fmt);
}

static int EsOutSourceSend(es_out_t *out, es_out_id_t *es, block_t *block)
{
    es_out_sys_t *sys = container_of(out, es_out_sys_t, out);
    return es_out_Send(sys->parent_out, es, block);
}

static void EsOutSourceDel(es_out_t *out, es_out_id_t *es)
{
    es_out_sys_t *sys = container_of(out, es_out_sys_t, out);
    es_out_Del(sys->parent_out, es);
}

static int EsOutSourceControl(es_out_t *out, input_source_t *in, int query,
                               va_list args)
{
    assert(in == NULL);
    es_out_sys_t *sys = container_of(out, es_out_sys_t, out);
    return sys->parent_out->cbs->control(sys->parent_out, sys->in, query, args);
}

static void EsOutSourceDestroy(es_out_t *out)
{
    es_out_sys_t *sys = container_of(out, es_out_sys_t, out);
    free(sys);
}

es_out_t *input_EsOutSourceNew(es_out_t *parent_out, input_source_t *in)
{
    assert(parent_out && in);

    static const struct es_out_callbacks es_out_cbs =
    {
        .add = EsOutSourceAdd,
        .send = EsOutSourceSend,
        .del = EsOutSourceDel,
        .control = EsOutSourceControl,
        .destroy = EsOutSourceDestroy,
    };

    es_out_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return NULL;

    sys->in = in;
    sys->out.cbs = &es_out_cbs;
    sys->parent_out = parent_out;

    return &sys->out;
}
