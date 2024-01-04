/*****************************************************************************
 * segments.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>

#include <vlc_httpd.h>
#include <vlc_list.h>
#include <vlc_tick.h>

#include "hls.h"
#include "segments.h"
#include "storage.h"

static void hls_segment_Destroy(hls_segment_t *segment)
{
    if (segment->http_url != NULL)
        httpd_UrlDelete(segment->http_url);
    hls_storage_Destroy(segment->storage);
    free(segment->url);
    free(segment);
}

static const char *
hls_segment_queue_GetFileExtension(enum hls_playlist_type type)
{
    switch (type)
    {
        case HLS_PLAYLIST_TYPE_TS:
            return "ts";
        case HLS_PLAYLIST_TYPE_WEBVTT:
            return "vtt";
        default:
            vlc_assert_unreachable();
    }
}

void hls_segment_queue_Init(hls_segment_queue_t *queue,
                            const struct hls_segment_queue_config *config,
                            const struct hls_config *hls_config)
{
    queue->playlist_id = config->playlist_id;
    queue->total_segments = 0;

    queue->httpd_ref = config->httpd_ref;
    queue->httpd_callback = config->httpd_callback;

    queue->file_extension =
        hls_segment_queue_GetFileExtension(config->playlist_type);

    queue->hls_config = hls_config;

    vlc_list_init(&queue->segments);
}

void hls_segment_queue_Clear(hls_segment_queue_t *queue)
{
    hls_segment_t *it;
    hls_segment_queue_Foreach(queue, it) { hls_segment_Destroy(it); }
}

int hls_segment_queue_NewSegment(hls_segment_queue_t *queue,
                                 block_t *content,
                                 vlc_tick_t length)
{
    hls_segment_t *segment = malloc(sizeof(*segment));
    if (unlikely(segment == NULL))
        return VLC_ENOMEM;

    segment->id = queue->total_segments;
    segment->length = length;

    if (asprintf(&segment->url,
                 "%s/playlist-%u-%u.%s",
                 queue->hls_config->base_url,
                 queue->playlist_id,
                 segment->id,
                 queue->file_extension) == -1)
    {
        segment->url = NULL;
        goto nomem;
    }

    const struct hls_storage_config storage_conf = {
        .name = segment->url + strlen(queue->hls_config->base_url) + 1,
        .mime = "video/MP2T",
    };
    segment->storage =
        hls_storage_FromBlocks(content, &storage_conf, queue->hls_config);
    if (unlikely(segment->storage == NULL))
        goto nomem;

    if (queue->httpd_ref != NULL)
    {
        segment->http_url =
            httpd_UrlNew(queue->httpd_ref, segment->url, NULL, NULL);
        if (segment->http_url == NULL)
            goto nomem;

        httpd_UrlCatch(segment->http_url,
                       HTTPD_MSG_GET,
                       queue->httpd_callback,
                       (httpd_callback_sys_t *)segment->storage);
    }
    else
        segment->http_url = NULL;

    if (hls_segment_queue_IsAtMaxCapacity(queue))
    {
        hls_segment_t *old = hls_segment_GetFirst(queue);
        assert(old != NULL);
        vlc_list_remove(&old->priv_node);
        hls_segment_Destroy(old);
    }

    ++queue->total_segments;
    vlc_list_append(&segment->priv_node, &queue->segments);
    return VLC_SUCCESS;
nomem:
    if (segment->storage != NULL)
        hls_storage_Destroy(segment->storage);
    free(segment->url);
    free(segment);
    return VLC_ENOMEM;
}
