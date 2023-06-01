/*****************************************************************************
 * hls.c: HLS stream output module
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

#include <vlc_common.h>

#include <vlc_configuration.h>
#include <vlc_frame.h>
#include <vlc_list.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_vector.h>

#include "hls.h"
#include "variant_maps.h"

/**
 * Represent one HLS playlist as in RFC 8216 section 4.
 */
typedef struct hls_playlist
{
    unsigned int id;

    sout_mux_t *mux;
    /** Every ES muxed in this playlist. */
    struct vlc_list tracks;

    struct vlc_list node;
} hls_playlist_t;

/**
 * Represent one ES.
 *
 * Returned from `pf_add` to have both the sout_input context and the owning
 * playlist reference.
 */
typedef struct
{
    sout_input_t *input;
    const char *es_id;
    hls_playlist_t *playlist_ref;
    struct vlc_list node;
} hls_track_t;

typedef struct
{
    /** All the plugin constants. */
    struct hls_config config;

    hls_variant_stream_maps_t variant_stream_maps;

    /**
     * All the created variant streams "EXT-X-STREAM-INF" (As in RFC 8216
     * section 4.3.4.2) playlists.
     */
    struct vlc_list variant_playlists;
    /**
     * All the created alternative renditions "EXT-X-MEDIA" (As in RFC 8216
     * section 4.3.4.1) playlists.
     */
    struct vlc_list media_playlists;

    /**
     * Total number of playlist created by the plugin.
     *
     * Notably used to create unique playlists IDs.
     */
    unsigned int playlist_created_count;
} sout_stream_sys_t;

#define hls_playlists_foreach(it)                                              \
    for (size_t i_##it = 0; i_##it < 2; ++i_##it)                              \
        vlc_list_foreach (                                                     \
            it,                                                                \
            (i_##it == 0 ? &sys->variant_playlists : &sys->media_playlists),   \
            node)

static hls_playlist_t *CreatePlaylist(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;

    hls_playlist_t *playlist = malloc(sizeof(*playlist));
    if (unlikely(playlist == NULL))
        return NULL;

    playlist->mux = sout_MuxNew(
        NULL /* TODO The access out will be introduced in the next commits. */,
        "ts");
    if (unlikely(playlist->mux == NULL))
        goto error;

    playlist->id = sys->playlist_created_count;
    vlc_list_init(&playlist->tracks);

    return playlist;
error:
    free(playlist);
    return NULL;
}

static void DeletePlaylist(hls_playlist_t *playlist)
{
    sout_MuxDelete(playlist->mux);
    vlc_list_remove(&playlist->node);

    free(playlist);
}

static hls_playlist_t *AddPlaylist(sout_stream_t *stream, struct vlc_list *head)
{
    hls_playlist_t *variant = CreatePlaylist(stream);
    if (variant != NULL)
        vlc_list_append(&variant->node, head);
    return variant;
}

static void *
Add(sout_stream_t *stream, const es_format_t *fmt, const char *es_id)
{
    sout_stream_sys_t *sys = stream->p_sys;

    // Either retrieve the already created playlist from the map or create it.
    struct hls_variant_stream_map *map =
        hls_variant_map_FromESID(&sys->variant_stream_maps, es_id);
    hls_playlist_t *playlist;
    if (map != NULL)
    {
        if (map->playlist_ref == NULL)
            map->playlist_ref = AddPlaylist(stream, &sys->variant_playlists);
        playlist = map->playlist_ref;
    }
    else
        playlist = AddPlaylist(stream, &sys->media_playlists);

    if (playlist == NULL)
        return NULL;

    ++sys->playlist_created_count;

    sout_input_t *input = sout_MuxAddStream(playlist->mux, fmt);
    if (input == NULL)
        goto error;

    hls_track_t *track = malloc(sizeof(*track));
    if (unlikely(track == NULL))
        goto error;

    track->input = input;
    track->es_id = es_id;
    track->playlist_ref = playlist;

    vlc_list_append(&track->node, &playlist->tracks);

    if (map != NULL && map->playlist_ref == NULL)
        map->playlist_ref = playlist;

    return track;
error:
    if (input != NULL)
        sout_MuxDeleteStream(playlist->mux, input);
    if (vlc_list_is_empty(&playlist->tracks))
        DeletePlaylist(playlist);
    return NULL;
}

static void Del(sout_stream_t *stream, void *id)
{
    sout_stream_sys_t *sys = stream->p_sys;
    hls_track_t *track = id;

    sout_MuxDeleteStream(track->playlist_ref->mux, track->input);
    vlc_list_remove(&track->node);

    if (vlc_list_is_empty(&track->playlist_ref->tracks))
    {
        struct hls_variant_stream_map *map = hls_variant_map_FromPlaylist(
            &sys->variant_stream_maps, track->playlist_ref);
        if (map != NULL)
            map->playlist_ref = NULL;

        DeletePlaylist(track->playlist_ref);
    }

    free(track);
}

static int Send(sout_stream_t *stream, void *id, vlc_frame_t *frame)
{
    hls_track_t *track = id;
    return sout_MuxSendBuffer(track->playlist_ref->mux, track->input, frame);
    (void)stream;
}

#define SOUT_CFG_PREFIX "sout-hls-"

static int Open(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;

    sout_stream_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    stream->p_sys = sys;

    static const char *const options[] = {
        "base-url", "num-seg", "out-dir", "pace", "seg-len", "variants", NULL};
    config_ChainParse(stream, SOUT_CFG_PREFIX, options, stream->p_cfg);

    sys->config.base_url = var_GetString(stream, SOUT_CFG_PREFIX "base-url");
    sys->config.outdir =
        var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "out-dir");
    sys->config.max_segments =
        var_GetInteger(stream, SOUT_CFG_PREFIX "num-seg");
    sys->config.pace = var_GetBool(stream, SOUT_CFG_PREFIX "pace");
    sys->config.segment_length =
        VLC_TICK_FROM_SEC(var_GetInteger(stream, SOUT_CFG_PREFIX "seg-len"));

    int status = VLC_EINVAL;

    vlc_vector_init(&sys->variant_stream_maps);
    char *variants = var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "variants");
    if (variants == NULL)
    {
        msg_Err(stream,
                "At least one variant mapping needs to be specified with the "
                "\"" SOUT_CFG_PREFIX "variants\" option");
        goto error;
    }
    status = hls_variant_maps_Parse(variants, &sys->variant_stream_maps);
    free(variants);
    if (status != VLC_SUCCESS)
    {
        if (status == VLC_EINVAL)
            msg_Err(stream,
                    "Wrong variant mapping syntax. It should look like: "
                    "\"{id1,id2},{id3,id4},...\"");
        goto error;
    }

    sys->playlist_created_count = 0;

    vlc_list_init(&sys->variant_playlists);
    vlc_list_init(&sys->media_playlists);
    static const struct sout_stream_operations ops = {
        .add = Add,
        .del = Del,
        .send = Send,
    };
    stream->ops = &ops;

    return VLC_SUCCESS;
error:
    hls_variant_maps_Destroy(&sys->variant_stream_maps);
    hls_config_Clean(&sys->config);
    free(sys);
    return status;
}

static void Close(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;
    sout_stream_sys_t *sys = stream->p_sys;

    hls_config_Clean(&sys->config);

    hls_variant_maps_Destroy(&sys->variant_stream_maps);

    free(sys);
}

#define VARIANTS_LONGTEXT                                                      \
    N_("String map ES string IDs into variant streams. The syntax is the "     \
       "following: \"{video/1,audio/2},{video/3,audio/4}\". This example "     \
       "describes two variant streams that contains different audio and "      \
       "video based on their string ES ID. ES that aren't described in the "   \
       "variant stream map will be automatically treated as alternative "      \
       "renditions")
#define VARIANTS_TEXT                                                          \
    N_("Map that group ES string IDs into variant streams (mandatory)")
#define BASEURL_TEXT N_("Base of the URL")
#define NUMSEG_TEXT N_("Number of maximum segment exposed")
#define OUTDIR_TEXT N_("Output directory path")
#define OUTDIR_LONGTEXT                                                        \
    N_("Output directory path. If not specified and HTTP is enabled, the "     \
       "segments will be stored in memory")
#define PACE_LONGTEXT                                                          \
    N_("Enable input pacing, the media will play at playback rate")
#define PACE_TEXT N_("Enable pacing")
#define SEGLEN_LONGTEXT N_("Length of segments in seconds")
#define SEGLEN_TEXT N_("Segment length (sec)")

vlc_module_begin()
    set_shortname("HLS")
    set_description(N_("HLS stream output"))
    set_capability("sout output", 50)
    add_shortcut("hls")
    set_subcategory(SUBCAT_SOUT_STREAM)

    add_string(SOUT_CFG_PREFIX "variants", NULL, VARIANTS_TEXT, VARIANTS_LONGTEXT)

    add_string(SOUT_CFG_PREFIX "base-url", "", BASEURL_TEXT, BASEURL_TEXT)
    add_integer(SOUT_CFG_PREFIX "num-seg", 0, NUMSEG_TEXT, NUMSEG_TEXT)
    add_string(SOUT_CFG_PREFIX "out-dir", NULL, OUTDIR_TEXT, OUTDIR_LONGTEXT)
    add_bool(SOUT_CFG_PREFIX "pace", false, PACE_TEXT, PACE_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "seg-len", 4, SEGLEN_TEXT, SEGLEN_LONGTEXT)

    set_callbacks(Open, Close)
vlc_module_end()
