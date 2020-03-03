/*****************************************************************************
 * cycle.c: cycle stream output module
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_sout.h>

typedef struct sout_cycle sout_cycle_t;

struct sout_cycle
{
    sout_cycle_t *next;
    vlc_tick_t offset;
    char chain[1];
};

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;
struct sout_stream_id_sys_t
{
    sout_stream_id_sys_t *prev;
    sout_stream_id_sys_t *next;
    es_format_t fmt;
    void *id;
};

typedef struct
{
    sout_stream_t *stream; /*< Current output stream */
    sout_stream_id_sys_t *first; /*< First elementary stream */
    sout_stream_id_sys_t *last; /*< Last elementary stream */

    sout_cycle_t *start;
    sout_cycle_t *next;
    vlc_tick_t (*clock)(const block_t *);
    vlc_tick_t period; /*< Total cycle duration */
} sout_stream_sys_t;

static vlc_tick_t get_dts(const block_t *block)
{
    return block->i_dts;
}

static void *Add(sout_stream_t *stream, const es_format_t *fmt)
{
    sout_stream_sys_t *sys = stream->p_sys;
    sout_stream_id_sys_t *id = malloc(sizeof (*id));
    if (unlikely(id == NULL))
        return NULL;

    id->next = NULL;

    if (es_format_Copy(&id->fmt, fmt))
    {
        es_format_Clean(&id->fmt);
        free(id);
        return NULL;
    }

    if (sys->stream != NULL)
        id->id = sout_StreamIdAdd(sys->stream, &id->fmt);

    id->prev = sys->last;
    sys->last = id;
    if (id->prev != NULL)
        id->prev->next = id;
    else
        sys->first = id;
    return id;
}

static void Del(sout_stream_t *stream, void *_id)
{
    sout_stream_sys_t *sys = stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;

    if (id->prev != NULL)
        id->prev->next = id->next;
    else
        sys->first = id->next;

    if (id->next != NULL)
        id->next->prev = id->prev;
    else
        sys->last = id->prev;

    if (sys->stream != NULL)
        sout_StreamIdDel(sys->stream, id->id);

    es_format_Clean(&id->fmt);
    free(id);
}

static int AddStream(sout_stream_t *stream, char *chain)
{
    sout_stream_sys_t *sys = stream->p_sys;

    msg_Dbg(stream, "starting new phase \"%s\"", chain);
    /* TODO format */
    sys->stream = sout_StreamChainNew(stream->p_sout, chain,
                                      stream->p_next, NULL);
    if (sys->stream == NULL)
        return -1;

    for (sout_stream_id_sys_t *id = sys->first; id != NULL; id = id->next)
        id->id = sout_StreamIdAdd(sys->stream, &id->fmt);

    return 0;
}

static void DelStream(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;

    if (sys->stream == NULL)
        return;

    for (sout_stream_id_sys_t *id = sys->first; id != NULL; id = id->next)
        if (id->id != NULL)
            sout_StreamIdDel(sys->stream, id->id);

    sout_StreamChainDelete(sys->stream, NULL);
    sys->stream = NULL;
}

static int Send(sout_stream_t *stream, void *_id, block_t *block)
{
    sout_stream_sys_t *sys = stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;

    for (block_t *next = block->p_next; block != NULL; block = next)
    {
        block->p_next = NULL;

        /* FIXME: deal with key frames properly */
        while (sys->clock(block) >= sys->next->offset)
        {
            DelStream(stream);
            AddStream(stream, sys->next->chain);

            sys->next->offset += sys->period;
            sys->next = sys->next->next;
            if (sys->next == NULL)
                sys->next = sys->start;
        }

        if (sys->stream != NULL)
            sout_StreamIdSend(sys->stream, id->id, block);
        else
            block_Release(block);
    }
    return VLC_SUCCESS;
}

static int AppendPhase(sout_cycle_t ***restrict pp,
                       vlc_tick_t offset, const char *chain)
{

    size_t len = strlen(chain);
    sout_cycle_t *cycle = malloc(sizeof (*cycle) + len);
    if (unlikely(cycle == NULL))
        return -1;

    cycle->next = NULL;
    cycle->offset = offset;
    memcpy(cycle->chain, chain, len + 1);

    **pp = cycle;
    *pp = &cycle->next;
    return 0;
}

static vlc_tick_t ParseTime(const char *str)
{
    char *end;
    unsigned long long u = strtoull(str, &end, 0);

    switch (*end)
    {
        case 'w':
            if (u < ((unsigned long long)INT64_MAX)/ (60 * 60 * 24 * 7 * CLOCK_FREQ))
                return vlc_tick_from_sec( 60LU * 60 * 24 * 7 * u );
            break;
        case 'd':
            if (u < ((unsigned long long)INT64_MAX)/ (60 * 60 * 24 * CLOCK_FREQ))
                return vlc_tick_from_sec( 60LU * 60 * 24 * u );
            break;
        case 'h':
            if (u < ((unsigned long long)INT64_MAX)/ (60 * 60 * CLOCK_FREQ))
                return vlc_tick_from_sec( 60LLU * 60 * u );
            break;
        case 'm':
            if (u < ((unsigned long long)INT64_MAX)/ (60 * CLOCK_FREQ))
                return vlc_tick_from_sec( 60LLU * u );
            break;
        case 's':
        case 0:
            if (u < ((unsigned long long)INT64_MAX)/CLOCK_FREQ)
                return vlc_tick_from_sec( u );
            break;
    }
    return -1;
}

static int Open(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    sout_stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->stream = NULL;
    sys->first = NULL;
    sys->last = NULL;
    sys->start = NULL;
    sys->clock = get_dts;

    vlc_tick_t offset = 0;
    sout_cycle_t **pp = &sys->start;
    const char *chain = "";

    for (const config_chain_t *cfg = stream->p_cfg;
         cfg != NULL;
         cfg = cfg->p_next)
    {
        if (!strcmp(cfg->psz_name, "dst"))
        {
            chain = cfg->psz_value;
        }
        else if (!strcmp(cfg->psz_name, "duration"))
        {
            vlc_tick_t t = ParseTime(cfg->psz_value);

            if (t > 0)
            {
                AppendPhase(&pp, offset, chain);
                offset += t;
            }

            chain = "";
        }
        else if (!strcmp(cfg->psz_name, "offset"))
        {
            vlc_tick_t t = ParseTime(cfg->psz_value);

            if (t > offset)
            {
                AppendPhase(&pp, offset, chain);
                offset = t;
            }

            chain = "";
        }
        else
        {
            msg_Err(stream, "unknown option \"%s\"", cfg->psz_name);
        }
    }

    if (sys->start == NULL || offset == 0)
    {
        free(sys);
        msg_Err(stream, "unknown or invalid cycle specification");
        return VLC_EGENERIC;
    }

    sys->next = sys->start;
    sys->period = offset;

    stream->pf_add = Add;
    stream->pf_del = Del;
    stream->pf_send = Send;
    stream->p_sys = sys;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    sout_stream_sys_t *sys = stream->p_sys;

    assert(sys->first == NULL && sys->last == NULL);

    if (sys->stream != NULL)
        sout_StreamChainDelete(sys->stream, NULL);

    for (sout_cycle_t *cycle = sys->start, *next; cycle != NULL; cycle = next)
    {
        next = cycle->next;
        free(cycle);
    }

    free(sys);
}

vlc_module_begin()
    set_shortname(N_("cycle"))
    set_description(N_("Cyclic stream output"))
    set_capability("sout output", 0)
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)
    add_shortcut("cycle")
    add_submodule()
    add_shortcut("cycle")
    set_capability("sout filter", 0)
    set_callbacks(Open, Close)
vlc_module_end()
