/*****************************************************************************
 * variant_maps.h
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
#ifndef HLS_VARIANT_MAPS_H
#define HLS_VARIANT_MAPS_H

/**
 * Variant stream represented by a group of ES string ID. This structure
 * represents the parsed user input and is used to add the incomming ES to the
 * correct playlists.
 */
struct hls_variant_stream_map
{
    struct VLC_VECTOR(char *) es_list;
    /**
     * Reference on the playlist associated with this list of ES. Having a
     * reference here after the playlist creation ease subsequent searches of
     * the playlist.
     */
    struct hls_playlist *playlist_ref;
};

typedef struct VLC_VECTOR(struct hls_variant_stream_map *)
    hls_variant_stream_maps_t;

int hls_variant_maps_Parse(const char *in, hls_variant_stream_maps_t *out);
void hls_variant_maps_Destroy(hls_variant_stream_maps_t *);

struct hls_variant_stream_map *
hls_variant_map_FromESID(hls_variant_stream_maps_t *, const char *);
struct hls_variant_stream_map *
hls_variant_map_FromPlaylist(hls_variant_stream_maps_t *,
                             struct hls_playlist *);

#endif
