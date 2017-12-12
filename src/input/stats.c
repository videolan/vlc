/*****************************************************************************
 * stats.c: Statistics handling
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
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
    rate->samples[0].date = VLC_TS_INVALID;
    rate->samples[1].date = VLC_TS_INVALID;
}

static float stats_GetRate(const input_rate_t *rate)
{
    if (rate->samples[1].date == VLC_TS_INVALID)
        return 0.;

    return (rate->samples[0].value - rate->samples[1].value)
        / (float)(rate->samples[0].date - rate->samples[1].date);
}

struct input_stats *input_stats_Create(void)
{
    struct input_stats *stats = malloc(sizeof (*stats));
    if (unlikely(stats == NULL))
        return NULL;

    memset(stats, 0, sizeof (*stats));
    input_rate_Init(&stats->input_bitrate);
    input_rate_Init(&stats->demux_bitrate);
    vlc_mutex_init(&stats->lock);
    return stats;
}

void input_stats_Destroy(struct input_stats *stats)
{
    vlc_mutex_destroy(&stats->lock);
    free(stats);
}

void input_stats_Compute(struct input_stats *stats, input_stats_t *st)
{
    vlc_mutex_lock(&stats->lock);

    /* Input */
    st->i_read_packets = stats->read_packets;
    st->i_read_bytes = stats->read_bytes;
    st->f_input_bitrate = stats_GetRate(&stats->input_bitrate);
    st->i_demux_read_bytes = stats->demux_read;
    st->f_demux_bitrate = stats_GetRate(&stats->demux_bitrate);
    st->i_demux_corrupted = stats->demux_corrupted;
    st->i_demux_discontinuity = stats->demux_discontinuity;

    /* Decoders */
    st->i_decoded_video = stats->decoded_video;
    st->i_decoded_audio = stats->decoded_audio;

    /* Aout */
    st->i_played_abuffers = stats->played_abuffers;
    st->i_lost_abuffers = stats->lost_abuffers;

    /* Vouts */
    st->i_displayed_pictures = stats->displayed_pictures;
    st->i_lost_pictures = stats->lost_pictures;

    vlc_mutex_unlock(&stats->lock);
}

/** Update a counter element with new values
 * \param p_counter the counter to update
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see stats_Create
 */
void input_rate_Update(input_rate_t *counter, uintmax_t val)
{
    if (counter == NULL)
        return;

    /* Ignore samples within a second of another */
    mtime_t now = mdate();
    if (counter->samples[0].date != VLC_TS_INVALID
     && (now - counter->samples[0].date) < CLOCK_FREQ)
        return;

    memcpy(counter->samples + 1, counter->samples,
           sizeof (counter->samples[0]));

    counter->samples[0].value = val;
    counter->samples[0].date = now;
}
