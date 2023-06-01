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

#include <vlc_block.h>
#include <vlc_configuration.h>
#include <vlc_frame.h>
#include <vlc_list.h>
#include <vlc_memstream.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tick.h>
#include <vlc_vector.h>

#include "hls.h"
#include "segments.h"
#include "storage.h"
#include "variant_maps.h"

typedef struct
{
    block_t *begin;
    block_t **end;
    vlc_tick_t length;
} hls_block_chain_t;

static inline void hls_block_chain_Reset(hls_block_chain_t *chain)
{
    chain->begin = NULL;
    chain->end = &chain->begin;
    chain->length = 0;
}

/**
 * Represent one HLS playlist as in RFC 8216 section 4.
 */
typedef struct hls_playlist
{
    unsigned int id;

    const struct hls_config *config;

    sout_access_out_t *access;
    sout_mux_t *mux;
    /** Every ES muxed in this playlist. */
    struct vlc_list tracks;

    hls_block_chain_t muxed_output;

    /**
     * Completed segments queue.
     *
     * The queue is generally max-sized (configurable by the user). Which means
     * that, when the max size is reached, pushing in the queue erase the first
     * segment.
     */
    hls_segment_queue_t segments;

    char *url;
    const char *name;

    /**
     * Current playlist manifest as in RFC 8216 section 4.3.3.
     */
    struct hls_storage *manifest;

    bool ended;

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

    vlc_tick_t first_pcr;
    vlc_tick_t last_pcr;
    vlc_tick_t last_segment;
} sout_stream_sys_t;

#define hls_playlists_foreach(it)                                              \
    for (size_t i_##it = 0; i_##it < 2; ++i_##it)                              \
        vlc_list_foreach (                                                     \
            it,                                                                \
            (i_##it == 0 ? &sys->variant_playlists : &sys->media_playlists),   \
            node)

static struct hls_storage *
GeneratePlaylistManifest(const hls_playlist_t *playlist)
{
    struct vlc_memstream out;
    vlc_memstream_open(&out);

#define MANIFEST_ADD_TAG(fmt, ...)                                             \
    do                                                                         \
    {                                                                          \
        if (vlc_memstream_printf(&out, fmt "\n", ##__VA_ARGS__) < 0)           \
            goto error;                                                        \
    } while (0)

    MANIFEST_ADD_TAG("#EXTM3U");
    const double seg_duration =
        secf_from_vlc_tick(playlist->config->segment_length);
    MANIFEST_ADD_TAG("#EXT-X-TARGETDURATION:%.0f", seg_duration);
    // First version adding CMAF fragments support.
    MANIFEST_ADD_TAG("#EXT-X-VERSION:7");

    const bool will_destroy_segments = playlist->config->max_segments == 0;
    if (playlist->ended)
        MANIFEST_ADD_TAG("#EXT-X-PLAYLIST-TYPE:VOD");
    else if (!will_destroy_segments)
        MANIFEST_ADD_TAG("#EXT-X-PLAYLIST-TYPE:EVENT");

    const hls_segment_t *first_seg = hls_segment_GetFirst(&playlist->segments);
    MANIFEST_ADD_TAG("#EXT-X-MEDIA-SEQUENCE:%u",
                     (first_seg == NULL) ? 0u : first_seg->id);

    const hls_segment_t *segment;
    hls_segment_queue_Foreach(&playlist->segments, segment)
    {
        MANIFEST_ADD_TAG("#EXTINF:%.2f,", secf_from_vlc_tick(segment->length));
        MANIFEST_ADD_TAG("%s", segment->url);
    }

    if (playlist->ended)
        MANIFEST_ADD_TAG("#EXT-X-ENDLIST");

#undef MANIFEST_ADD_TAG

    if (vlc_memstream_close(&out) != 0)
        return NULL;

    const struct hls_storage_config storage_config = {
        .name = playlist->name};
    return hls_storage_FromBytes(
        out.ptr, out.length, &storage_config, playlist->config);
error:
    if (vlc_memstream_close(&out) != 0)
        return NULL;
    free(out.ptr);
    return NULL;
}

static int UpdatePlaylistManifest(hls_playlist_t *playlist)
{
    struct hls_storage *new_manifest = GeneratePlaylistManifest(playlist);
    if (unlikely(new_manifest == NULL))
        return VLC_EGENERIC;

    if (playlist->manifest != NULL)
        hls_storage_Destroy(playlist->manifest);
    playlist->manifest = new_manifest;
    return VLC_SUCCESS;
}

static ssize_t AccessOutWrite(sout_access_out_t *access, block_t *block)
{
    hls_playlist_t *playlist = access->p_sys;

    size_t size = 0;
    block_ChainProperties(block, NULL, &size, NULL);

    block_ChainLastAppend(&playlist->muxed_output.end, block);
    return size;
}

static sout_access_out_t *CreateAccessOut(sout_stream_t *stream,
                                          hls_playlist_t *sys)
{
    sout_access_out_t *access = vlc_object_create(stream, sizeof(*access));
    if (unlikely(access == NULL))
        return NULL;

    access->psz_access = strdup("hls");
    if (unlikely(access->psz_access == NULL))
    {
        vlc_object_delete(access);
        return NULL;
    }

    access->p_cfg = NULL;
    access->p_module = NULL;
    access->p_sys = sys;
    access->psz_path = NULL;

    access->pf_control = NULL;
    access->pf_read = NULL;
    access->pf_seek = NULL;
    access->pf_write = AccessOutWrite;
    return access;
}

static inline char *FormatPlaylistManifestURL(const hls_playlist_t *playlist)
{
    char *url;
    const int status = asprintf(&url,
                                "%s/playlist-%u-index.m3u8",
                                playlist->config->base_url,
                                playlist->id);
    if (unlikely(status == -1))
        return NULL;
    return url;
}

static hls_playlist_t *CreatePlaylist(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;

    hls_playlist_t *playlist = malloc(sizeof(*playlist));
    if (unlikely(playlist == NULL))
        return NULL;

    playlist->access = CreateAccessOut(stream, playlist);
    if (unlikely(playlist->access == NULL))
        goto access_err;

    playlist->mux = sout_MuxNew(playlist->access, "ts");
    if (unlikely(playlist->mux == NULL))
        goto mux_err;

    playlist->id = sys->playlist_created_count;
    playlist->config = &sys->config;
    playlist->ended = false;

    playlist->url = FormatPlaylistManifestURL(playlist);
    if (unlikely(playlist->url == NULL))
        goto url_err;

    playlist->name = playlist->url + strlen(sys->config.base_url) + 1;

    struct hls_segment_queue_config config = {
        .playlist_id = playlist->id,
    };
    hls_segment_queue_Init(&playlist->segments, &config, &sys->config);

    hls_block_chain_Reset(&playlist->muxed_output);

    playlist->manifest = NULL;
    if (UpdatePlaylistManifest(playlist) != VLC_SUCCESS)
        goto error;

    vlc_list_init(&playlist->tracks);

    return playlist;
error:
    hls_segment_queue_Clear(&playlist->segments);
    free(playlist->url);
url_err:
    sout_MuxDelete(playlist->mux);
mux_err:
    sout_AccessOutDelete(playlist->access);
access_err:
    free(playlist);
    return NULL;
}

static void DeletePlaylist(hls_playlist_t *playlist)
{
    sout_MuxDelete(playlist->mux);

    sout_AccessOutDelete(playlist->access);

    if (playlist->manifest != NULL)
        hls_storage_Destroy(playlist->manifest);

    block_ChainRelease(playlist->muxed_output.begin);
    hls_segment_queue_Clear(&playlist->segments);

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

static hls_block_chain_t ExtractSegment(hls_playlist_t *playlist,
                                        vlc_tick_t max_segment_length)
{
    hls_block_chain_t segment = {.begin = playlist->muxed_output.begin};

    block_t *prev = NULL;
    for (block_t *it = playlist->muxed_output.begin; it != NULL;
         it = it->p_next)
    {
        if (segment.length + it->i_length > max_segment_length)
        {
            playlist->muxed_output.begin = it;

            if (prev != NULL)
                prev->p_next = NULL;
            return segment;
        }
        segment.length += it->i_length;
        prev = it;
    }

    hls_block_chain_Reset(&playlist->muxed_output);
    return segment;
}

static void ExtractAndAddSegment(hls_playlist_t *playlist,
                                 vlc_tick_t last_segment_time)
{
    hls_block_chain_t segment = ExtractSegment(playlist, last_segment_time);

    const int status = hls_segment_queue_NewSegment(
        &playlist->segments, segment.begin, segment.length);
    if (unlikely(status != VLC_SUCCESS))
        return;

    UpdatePlaylistManifest(playlist);
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

        hls_playlist_t *playlist;
        hls_playlists_foreach (playlist)
            ExtractAndAddSegment(playlist, sys->config.segment_length);

        playlist->ended = true;
        UpdatePlaylistManifest(playlist);

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

/**
 * PCR events are used to have a reliable stream time status. Segmenting is done
 * after a PCR testifying that we are above the segment limit arrives.
 */
static void SetPCR(sout_stream_t *stream, vlc_tick_t pcr)
{
    sout_stream_sys_t *sys = stream->p_sys;

    const vlc_tick_t last_pcr = sys->last_pcr;
    sys->last_pcr = pcr;

    if (sys->first_pcr == VLC_TICK_INVALID)
    {
        sys->first_pcr = pcr;
        return;
    }

    const vlc_tick_t stream_time = pcr - sys->first_pcr;
    const vlc_tick_t current_seglen = stream_time - sys->last_segment;

    const vlc_tick_t pcr_gap = pcr - last_pcr;
    /* PCR and segment length aren't necessarily aligned. Testing segment length
     * with a **next** PCR  approximation will avoid piling up data:
     *
     * |------x#|-----x##|----x###| time
     * ^ PCR  ^ Segment end     ^ Buffer expanding
     *
     * The segments are then a little shorter than they could be.
     */
    if (current_seglen + pcr_gap >= sys->config.segment_length)
    {
        hls_playlist_t *playlist;
        hls_playlists_foreach (playlist)
            ExtractAndAddSegment(playlist, sys->config.segment_length);
        sys->last_segment = stream_time;
    }
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

    sys->first_pcr = VLC_TICK_INVALID;
    sys->last_pcr = VLC_TICK_INVALID;
    sys->last_segment = 0;

    static const struct sout_stream_operations ops = {
        .add = Add,
        .del = Del,
        .send = Send,
        .set_pcr = SetPCR,
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
