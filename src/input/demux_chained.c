/*****************************************************************************
 * demux_chained.c
 *****************************************************************************
 * Copyright (C) 1999-2016 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Rémi Denis-Courmont
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_demux.h>
#include "demux.h"

struct vlc_demux_chained_t
{
    vlc_stream_fifo_t *writer;
    stream_t *reader;

    vlc_thread_t thread;
    vlc_mutex_t  lock;

    struct
    {
        double  position;
        vlc_tick_t length;
        vlc_tick_t time;
    } stats;

    es_out_t *out;
    char name[];
};

static void *vlc_demux_chained_Thread(void *data)
{
    vlc_demux_chained_t *dc = data;
    demux_t *demux = demux_New(VLC_OBJECT(dc->reader), dc->name, dc->reader,
                               dc->out);
    if (demux == NULL)
    {
        vlc_stream_Delete(dc->reader);
        return NULL;
    }

    /* Stream FIFO cannot apply DVB filters.
     * Get all programs and let the E/S output sort them out. */
    demux_Control(demux, DEMUX_SET_GROUP_ALL);

    /* Main loop */
    vlc_tick_t next_update = 0;

    do
        if (demux_TestAndClearFlags(demux, UINT_MAX) || vlc_tick_now() >= next_update)
        {
            double newpos;
            vlc_tick_t newlen;
            vlc_tick_t newtime;

            if (demux_Control(demux, DEMUX_GET_POSITION, &newpos))
                newpos = 0.;
            if (demux_Control(demux, DEMUX_GET_LENGTH, &newlen))
                newlen = 0;
            if (demux_Control(demux, DEMUX_GET_TIME, &newtime))
                newtime = 0;

            vlc_mutex_lock(&dc->lock);
            dc->stats.position = newpos;
            dc->stats.length = newlen;
            dc->stats.time = newtime;
            vlc_mutex_unlock(&dc->lock);

            next_update = vlc_tick_now() + VLC_TICK_FROM_MS(250);
        }
    while (demux_Demux(demux) > 0);

    demux_Delete(demux);
    return NULL;
}

vlc_demux_chained_t *vlc_demux_chained_New(vlc_object_t *parent,
                                           const char *name, es_out_t *out)
{
    vlc_demux_chained_t *dc = malloc(sizeof (*dc) + strlen(name) + 1);
    if (unlikely(dc == NULL))
        return NULL;

    dc->writer = vlc_stream_fifo_New(parent, &dc->reader);
    if (dc->writer == NULL)
    {
        free(dc);
        return NULL;
    }

    dc->stats.position = 0.;
    dc->stats.length = 0;
    dc->stats.time = 0;
    dc->out = out;
    strcpy(dc->name, name);

    vlc_mutex_init(&dc->lock);

    if (vlc_clone(&dc->thread, vlc_demux_chained_Thread, dc,
                  VLC_THREAD_PRIORITY_INPUT))
    {
        vlc_stream_Delete(dc->reader);
        vlc_stream_fifo_Close(dc->writer);
        free(dc);
        dc = NULL;
    }
    return dc;
}

void vlc_demux_chained_Send(vlc_demux_chained_t *dc, block_t *block)
{
    vlc_stream_fifo_Queue(dc->writer, block);
}

int vlc_demux_chained_ControlVa(vlc_demux_chained_t *dc, int query, va_list ap)
{
    switch (query)
    {
        case DEMUX_GET_POSITION:
            vlc_mutex_lock(&dc->lock);
            *va_arg(ap, double *) = dc->stats.position;
            vlc_mutex_unlock(&dc->lock);
            break;
        case DEMUX_GET_LENGTH:
            vlc_mutex_lock(&dc->lock);
            *va_arg(ap, vlc_tick_t *) = dc->stats.length;
            vlc_mutex_unlock(&dc->lock);
            break;
        case DEMUX_GET_TIME:
            vlc_mutex_lock(&dc->lock);
            *va_arg(ap, vlc_tick_t *) = dc->stats.time;
            vlc_mutex_unlock(&dc->lock);
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

void vlc_demux_chained_Delete(vlc_demux_chained_t *dc)
{
    vlc_stream_fifo_Close(dc->writer);
    vlc_join(dc->thread, NULL);
    free(dc);
}
