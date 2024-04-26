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

struct es_out_source
{
    struct vlc_input_es_out out;
    input_source_t *in;
    struct vlc_input_es_out *parent_out;
} ;

static struct es_out_source *
PRIV(es_out_t *out)
{
    struct vlc_input_es_out *parent = container_of(out, struct vlc_input_es_out, out);
    struct es_out_source *source = container_of(parent, struct es_out_source, out);
    return source;
}

static es_out_id_t *EsOutSourceAdd(es_out_t *out, input_source_t *in,
                                    const es_format_t *fmt)
{
    assert(in == NULL);
    struct es_out_source *sys = PRIV(out);
    return sys->parent_out->out.cbs->add(&sys->parent_out->out, sys->in, fmt);
}

static int EsOutSourceSend(es_out_t *out, es_out_id_t *es, block_t *block)
{
    struct es_out_source *sys = PRIV(out);
    return es_out_Send(&sys->parent_out->out, es, block);
}

static void EsOutSourceDel(es_out_t *out, es_out_id_t *es)
{
    struct es_out_source *sys = PRIV(out);
    es_out_Del(&sys->parent_out->out, es);
}

static int EsOutSourceControl(es_out_t *out, input_source_t *in, int query,
                               va_list args)
{
    assert(in == NULL);
    struct es_out_source *sys = PRIV(out);
    return sys->parent_out->out.cbs->control(&sys->parent_out->out, sys->in, query, args);
}

static int EsOutSourcePrivControl(struct vlc_input_es_out *out, input_source_t *in, int query,
                                  va_list args)
{
    assert(in == NULL);
    struct es_out_source *sys = PRIV(&out->out);
    return sys->parent_out->ops->priv_control(sys->parent_out, sys->in, query, args);
}

static void EsOutSourceDestroy(es_out_t *out)
{
    struct es_out_source *sys = PRIV(out);
    free(sys);
}

struct vlc_input_es_out *
input_EsOutSourceNew(struct vlc_input_es_out *parent_out, input_source_t *in)
{
    assert(parent_out && in);

    struct es_out_source *sys = malloc(sizeof(*sys));
    if (!sys)
        return NULL;

    sys->in = in;
    sys->parent_out = parent_out;

    static const struct es_out_callbacks es_out_cbs =
    {
        .add = EsOutSourceAdd,
        .send = EsOutSourceSend,
        .del = EsOutSourceDel,
        .control = EsOutSourceControl,
        .destroy = EsOutSourceDestroy,
    };
    sys->out.out.cbs = &es_out_cbs;

    static const struct vlc_input_es_out_ops ops =
    {
        .priv_control = EsOutSourcePrivControl,
    };

    sys->in = in;
    sys->out.ops = &ops;
    sys->parent_out = parent_out;

    return &sys->out;
}
