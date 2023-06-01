/*****************************************************************************
 * variant_maps.c
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

#include <ctype.h>

#include <vlc_common.h>

#include <vlc_memstream.h>
#include <vlc_vector.h>

#include "variant_maps.h"

enum hls_map_parser_state
{
    STATE_MAP_BEGIN,
    STATE_NEW_ESID,
    STATE_BUILD_ESID,
    STATE_MAP_END,
};

static int parser_NewESID(struct vlc_memstream *esid,
                          struct hls_variant_stream_map *map)
{
    vlc_memstream_putc(esid, '\0');
    if (vlc_memstream_close(esid))
        return VLC_ENOMEM;

    if (!vlc_vector_push(&map->es_list, esid->ptr))
    {
        free(esid->ptr);
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static inline bool is_valid_char(char c) { return isalnum(c) || c == '/'; }

static void hls_variant_map_Destroy(struct hls_variant_stream_map *map)
{
    char *esid;
    vlc_vector_foreach (esid, &map->es_list)
    {
        free(esid);
    }
    vlc_vector_destroy(&map->es_list);
    free(map);
}

int hls_variant_maps_Parse(const char *s, hls_variant_stream_maps_t *out)
{
    int error = VLC_ENOMEM;

    struct vlc_memstream current_esid;
    struct hls_variant_stream_map *current_map = NULL;
    enum hls_map_parser_state state = STATE_MAP_BEGIN;

    for (char c = *s; c != '\0'; c = *++s)
    {
        if (c == ' ' || c == '\t')
            continue;

        switch (state)
        {
            case STATE_MAP_BEGIN:
                if (c != '{')
                    goto einval;

                current_map = malloc(sizeof(*current_map));
                if (unlikely(current_map == NULL))
                    goto enomem;

                vlc_vector_init(&current_map->es_list);
                current_map->playlist_ref = NULL;

                state = STATE_NEW_ESID;
                break;
            case STATE_NEW_ESID:
                if (!is_valid_char(c))
                    goto einval;

                vlc_memstream_open(&current_esid);
                vlc_memstream_putc(&current_esid, c);
                state = STATE_BUILD_ESID;
                break;
            case STATE_BUILD_ESID:
                if (c == ',')
                {
                    if (parser_NewESID(&current_esid, current_map) !=
                        VLC_SUCCESS)
                        goto enomem;
                    state = STATE_NEW_ESID;
                }
                else if (c == '}')
                {
                    if (parser_NewESID(&current_esid, current_map) !=
                        VLC_SUCCESS)
                        goto enomem;

                    if (!vlc_vector_push(out, current_map))
                        goto enomem;
                    current_map = NULL;

                    state = STATE_MAP_END;
                }
                else if (is_valid_char(c))
                    vlc_memstream_putc(&current_esid, c);
                else
                {
                    if (!vlc_memstream_close(&current_esid))
                        free(current_esid.ptr);
                    goto einval;
                }
                break;
            case STATE_MAP_END:
                if (c == ',')
                    state = STATE_MAP_BEGIN;
                else
                    goto einval;
                break;
        }
    }

    if (state == STATE_MAP_END)
        return VLC_SUCCESS;

einval:
    error = VLC_EINVAL;
enomem:
    hls_variant_maps_Destroy(out);
    if (current_map != NULL)
        hls_variant_map_Destroy(current_map);
    return error;
}

void hls_variant_maps_Destroy(hls_variant_stream_maps_t *maps)
{
    struct hls_variant_stream_map *map;
    vlc_vector_foreach (map, maps)
    {
        char *esid;
        vlc_vector_foreach (esid, &map->es_list)
            free(esid);
        vlc_vector_destroy(&map->es_list);
        free(map);
    }
    vlc_vector_destroy(maps);
}

struct hls_variant_stream_map *
hls_variant_map_FromESID(hls_variant_stream_maps_t *maps, const char *es_id)
{
    struct hls_variant_stream_map *it;
    vlc_vector_foreach (it, maps)
    {
        const char *es_id_it;
        vlc_vector_foreach (es_id_it, &it->es_list)
        {
            if (!strcmp(es_id_it, es_id))
                return it;
        }
    }
    return NULL;
}

struct hls_variant_stream_map *
hls_variant_map_FromPlaylist(hls_variant_stream_maps_t *maps,
                             struct hls_playlist *playlist)
{
    struct hls_variant_stream_map *it;
    vlc_vector_foreach (it, maps)
    {
        if (it->playlist_ref == playlist)
            return it;
    }
    return NULL;
}
