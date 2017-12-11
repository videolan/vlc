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

struct counter_t
{
    struct
    {
        uintmax_t value;
        mtime_t   date;
    } samples[2];
};

/**
 * Create a statistics counter
 */
counter_t * stats_CounterCreate( void )
{
    counter_t *counter = malloc(sizeof (*counter)) ;
    if (unlikely(counter == NULL))
        return NULL;

    counter->samples[0].date = VLC_TS_INVALID;
    counter->samples[1].date = VLC_TS_INVALID;
    return counter;
}

static float stats_GetRate(const counter_t *counter)
{
    if (counter == NULL || counter->samples[1].date == VLC_TS_INVALID)
        return 0.;

    return (counter->samples[0].value - counter->samples[1].value)
        / (float)(counter->samples[0].date - counter->samples[1].date);
}

void stats_ComputeInputStats(input_thread_t *input, input_stats_t *st)
{
    input_thread_private_t *priv = input_priv(input);

    if (!libvlc_stats(input))
        return;

    vlc_mutex_lock(&priv->counters.counters_lock);

    /* Input */
    st->i_read_packets = priv->counters.read_packets;
    st->i_read_bytes = priv->counters.read_bytes;
    st->f_input_bitrate = stats_GetRate(priv->counters.p_input_bitrate);
    st->i_demux_read_bytes = priv->counters.demux_read;
    st->f_demux_bitrate = stats_GetRate(priv->counters.p_demux_bitrate);
    st->i_demux_corrupted = priv->counters.demux_corrupted;
    st->i_demux_discontinuity = priv->counters.demux_discontinuity;

    /* Decoders */
    st->i_decoded_video = priv->counters.decoded_video;
    st->i_decoded_audio = priv->counters.decoded_audio;

    /* Sout */
    if (priv->counters.p_sout_send_bitrate)
    {
        st->i_sent_packets = priv->counters.sout_sent_packets;
        st->i_sent_bytes = priv->counters.sout_sent_bytes;
        st->f_send_bitrate = stats_GetRate(priv->counters.p_sout_send_bitrate);
    }

    /* Aout */
    st->i_played_abuffers = priv->counters.played_abuffers;
    st->i_lost_abuffers = priv->counters.lost_abuffers;

    /* Vouts */
    st->i_displayed_pictures = priv->counters.displayed_pictures;
    st->i_lost_pictures = priv->counters.lost_pictures;

    vlc_mutex_unlock(&priv->counters.counters_lock);
}

void stats_CounterClean(counter_t *counter)
{
    free(counter);
}


/** Update a counter element with new values
 * \param p_counter the counter to update
 * \param val the vlc_value union containing the new value to aggregate. For
 * more information on how data is aggregated, \see stats_Create
 */
void stats_Update(counter_t *counter, uint64_t val)
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
