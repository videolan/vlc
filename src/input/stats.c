/*****************************************************************************
 * stats.c: Statistics handling
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include "input/input_internal.h"

/**
 * Create a statistics counter
 */
static void input_rate_Init(input_rate_t *rate)
{
    vlc_mutex_init(&rate->lock);
    rate->updates = 0;
    rate->value = 0;
    rate->samples[0].date = VLC_TICK_INVALID;
    rate->samples[1].date = VLC_TICK_INVALID;
}

static float stats_GetRate(const input_rate_t *rate)
{
    if (rate->samples[1].date == VLC_TICK_INVALID)
        return 0.;

    return (rate->samples[0].value - rate->samples[1].value)
        / (float)(rate->samples[0].date - rate->samples[1].date);
}

struct input_stats *input_stats_Create(void)
{
    struct input_stats *stats = malloc(sizeof (*stats));
    if (unlikely(stats == NULL))
        return NULL;

    input_rate_Init(&stats->input_bitrate);
    input_rate_Init(&stats->demux_bitrate);
    atomic_init(&stats->demux_corrupted, 0);
    atomic_init(&stats->demux_discontinuity, 0);
    atomic_init(&stats->decoded_audio, 0);
    atomic_init(&stats->decoded_video, 0);
    atomic_init(&stats->played_abuffers, 0);
    atomic_init(&stats->lost_abuffers, 0);
    atomic_init(&stats->displayed_pictures, 0);
    atomic_init(&stats->lost_pictures, 0);
    return stats;
}

void input_stats_Destroy(struct input_stats *stats)
{
    free(stats);
}

void input_stats_Compute(struct input_stats *stats, input_stats_t *st)
{
    /* Input */
    vlc_mutex_lock(&stats->input_bitrate.lock);
    st->i_read_packets = stats->input_bitrate.updates;
    st->i_read_bytes = stats->input_bitrate.value;
    st->f_input_bitrate = stats_GetRate(&stats->input_bitrate);
    vlc_mutex_unlock(&stats->input_bitrate.lock);

    vlc_mutex_lock(&stats->demux_bitrate.lock);
    st->i_demux_read_bytes = stats->demux_bitrate.value;
    st->f_demux_bitrate = stats_GetRate(&stats->demux_bitrate);
    vlc_mutex_unlock(&stats->demux_bitrate.lock);
    st->i_demux_corrupted = atomic_load_explicit(&stats->demux_corrupted,
                                                 memory_order_relaxed);
    st->i_demux_discontinuity = atomic_load_explicit(
                    &stats->demux_discontinuity, memory_order_relaxed);

    /* Aout */
    st->i_decoded_audio = atomic_load_explicit(&stats->decoded_audio,
                                               memory_order_relaxed);
    st->i_played_abuffers = atomic_load_explicit(&stats->played_abuffers,
                                                 memory_order_relaxed);
    st->i_lost_abuffers = atomic_load_explicit(&stats->lost_abuffers,
                                               memory_order_relaxed);

    /* Vouts */
    st->i_decoded_video = atomic_load_explicit(&stats->decoded_video,
                                               memory_order_relaxed);
    st->i_displayed_pictures = atomic_load_explicit(&stats->displayed_pictures,
                                                    memory_order_relaxed);
    st->i_lost_pictures = atomic_load_explicit(&stats->lost_pictures,
                                               memory_order_relaxed);
}

/** Update a counter element with new values
 * \param p_counter the counter to update
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see stats_Create
 */
void input_rate_Add(input_rate_t *counter, uintmax_t val)
{
    vlc_mutex_lock(&counter->lock);
    counter->updates++;
    counter->value += val;

    /* Ignore samples within a second of another */
    vlc_tick_t now = vlc_tick_now();
    if (counter->samples[0].date != VLC_TICK_INVALID
     && (now - counter->samples[0].date) < VLC_TICK_FROM_SEC(1))
    {
        vlc_mutex_unlock(&counter->lock);
        return;
    }

    memcpy(counter->samples + 1, counter->samples,
           sizeof (counter->samples[0]));

    counter->samples[0].value = counter->value;
    counter->samples[0].date = now;
    vlc_mutex_unlock(&counter->lock);
}
