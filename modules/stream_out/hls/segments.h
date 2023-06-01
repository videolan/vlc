/*****************************************************************************
 * segments.h
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
#ifndef HLS_SEGMENTS_H
#define HLS_SEGMENTS_H

struct hls_storage;
struct hls_config;

typedef struct hls_segment
{
    char *url;
    unsigned int id;
    vlc_tick_t length;

    struct hls_storage *storage;

    struct vlc_list priv_node;
} hls_segment_t;

struct hls_segment_queue_config
{
    unsigned int playlist_id;
};

typedef struct
{
    struct hls_segment_queue_config config;
    unsigned int total_segments;

    const struct hls_config *hls_config;

    struct vlc_list segments;
} hls_segment_queue_t;

#define hls_segment_queue_Foreach(queue, it)                                   \
    vlc_list_foreach (it, &(queue)->segments, priv_node)
#define hls_segment_GetFirst(queue)                                            \
    vlc_list_first_entry_or_null(&(queue)->segments, hls_segment_t, priv_node);

void hls_segment_queue_Init(hls_segment_queue_t *,
                            const struct hls_segment_queue_config *,
                            const struct hls_config *);
void hls_segment_queue_Clear(hls_segment_queue_t *);

/**
 * Add a new segment to the queue.
 *
 * If the queue is at max capacity, the first inserted segment will also be
 * popped and destroyed.
 *
 * \param content A chain of block containing segment's data.
 * \param length The media time size of the segment.
 *
 * \retval VLC_SUCCESS on success.
 * \retval VLC_ENOMEM on internal allocation failure.
 */
int hls_segment_queue_NewSegment(hls_segment_queue_t *,
                                 block_t *content,
                                 vlc_tick_t length);

#endif
