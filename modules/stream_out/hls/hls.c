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
#include <vlc_httpd.h>
#include <vlc_iso_lang.h>
#include <vlc_list.h>
#include <vlc_memstream.h>
#include <vlc_messages.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tick.h>
#include <vlc_vector.h>

#include "codecs.h"
#include "hls.h"
#include "segments.h"
#include "storage.h"
#include "variant_maps.h"

typedef struct
{
    block_t *begin;
    block_t **end;
    vlc_tick_t length;
    block_t *last_header;
} hls_block_chain_t;

static inline void hls_block_chain_Reset(hls_block_chain_t *chain)
{
    chain->begin = NULL;
    chain->end = &chain->begin;
    chain->length = 0;
    chain->last_header = NULL;
}

/**
 * Represent one HLS playlist as in RFC 8216 section 4.
 */
typedef struct hls_playlist
{
    unsigned int id;

    const struct hls_config *config;
    enum hls_playlist_type type;

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
    struct vlc_logger *logger;

    /**
     * Current playlist manifest as in RFC 8216 section 4.3.3.
     */
    struct hls_storage *manifest;
    httpd_url_t *http_manifest;

    bool ended;

    /**
     * Total duration of the muxed data.
     */
    vlc_tick_t muxed_duration;

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

    httpd_host_t *http_host;

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

    /**
     * Current "Master" Playlist manifest (As in RFC 8216 4.3.4).
     */
    struct hls_storage *manifest;
    httpd_url_t *http_manifest;

    /**
     * Global advancement of the stream in media time.
     */
    vlc_tick_t elapsed_stream_time;
    vlc_tick_t first_pcr;

    size_t current_memory_cached;
} sout_stream_sys_t;

#define hls_playlists_foreach(it)                                              \
    for (size_t i_##it = 0; i_##it < 2; ++i_##it)                              \
        vlc_list_foreach (                                                     \
            it,                                                                \
            (i_##it == 0 ? &sys->variant_playlists : &sys->media_playlists),   \
            node)

static int HTTPCallback(httpd_callback_sys_t *sys,
                        httpd_client_t *client,
                        httpd_message_t *answer,
                        const httpd_message_t *query)
{
    if (answer == NULL || query == NULL || client == NULL)
        return VLC_SUCCESS;

    struct hls_storage *storage = (struct hls_storage *)sys;

    httpd_MsgAdd(answer, "Content-Type", "%s", storage->mime);
    httpd_MsgAdd(answer, "Cache-Control", "no-cache");

    answer->i_proto = HTTPD_PROTO_HTTP;
    answer->i_version = 0;
    answer->i_type = HTTPD_MSG_ANSWER;

    const ssize_t size = storage->get_content(storage, &answer->p_body);
    if (size != -1)
    {
        answer->i_body = size;
        answer->i_status = 200;
    }
    else
        answer->i_status = 500;

    if (httpd_MsgGet(query, "Connection") != NULL)
        httpd_MsgAdd(answer, "Connection", "close");
    httpd_MsgAdd(answer, "Content-Length", "%zu", answer->i_body);

    return VLC_SUCCESS;
}

typedef struct VLC_VECTOR(const es_format_t *) es_format_vec_t;

static inline bool IsCodecAlreadyDescribed(const es_format_vec_t *vec,
                                           const es_format_t *fmt)
{
    const es_format_t *it;
    vlc_vector_foreach (it, vec)
    {
        if (es_format_IsSimilar(it, (fmt)))
            return true;
    }
    return false;
}

static inline hls_track_t *MediaGetTrack(const hls_playlist_t *media_playlist)
{
    hls_track_t *track = vlc_list_first_entry_or_null(
        &media_playlist->tracks, hls_track_t, node);
    assert(track != NULL);
    return track;
}

VLC_MALLOC static char *GeneratePlaylistCodecInfo(const struct vlc_list *media_list,
                                                  const hls_playlist_t *playlist)
{
    es_format_vec_t already_described = VLC_VECTOR_INITIALIZER;

    bool is_stream_empty = true;
    struct vlc_memstream out;
    vlc_memstream_open(&out);

    /* Describe codecs from the playlist. */
    const hls_track_t *track;
    vlc_list_foreach_const (track, &playlist->tracks, node)
    {
        if (IsCodecAlreadyDescribed(&already_described, &track->input->fmt))
            continue;

        if (!is_stream_empty)
            vlc_memstream_putc(&out, ',');
        if (hls_codec_Format(&out, &track->input->fmt) != VLC_SUCCESS)
            goto error;
        is_stream_empty = false;
        vlc_vector_push(&already_described, &track->input->fmt);
    }

    /* Describe codecs from all the EXT-X-MEDIA tracks. */
    const hls_playlist_t *media;
    vlc_list_foreach_const (media, media_list, node)
    {
        track = MediaGetTrack(media);

        if (IsCodecAlreadyDescribed(&already_described, &track->input->fmt))
            continue;

        if (!is_stream_empty)
            vlc_memstream_putc(&out, ',');
        if (hls_codec_Format(&out, &track->input->fmt) != VLC_SUCCESS)
            goto error;
        is_stream_empty = false;
        vlc_vector_push(&already_described, &track->input->fmt);
    }

    vlc_vector_destroy(&already_described);

    vlc_memstream_putc(&out, '\0');
    if (vlc_memstream_close(&out) != 0)
        return NULL;
    return out.ptr;
error:
    vlc_vector_destroy(&already_described);
    if (vlc_memstream_close(&out) != 0)
        return NULL;
    free(out.ptr);
    return NULL;
}

static struct hls_storage *GenerateMainManifest(const sout_stream_sys_t *sys)
{
    struct vlc_memstream out;
    vlc_memstream_open(&out);

#define MANIFEST_START_TAG(tag)                                                \
    do                                                                         \
    {                                                                          \
        bool first_attribute = true;                                           \
        vlc_memstream_puts(&out, tag);

#define MANIFEST_ADD_ATTRIBUTE(attribute, ...)                                 \
    do                                                                         \
    {                                                                          \
        if (vlc_memstream_printf(&out,                                         \
                                 "%s" attribute,                               \
                                 first_attribute ? ":" : ",",                  \
                                 ##__VA_ARGS__) < 0)                           \
            goto error;                                                        \
        first_attribute = false;                                               \
    } while (0)

#define MANIFEST_END_TAG                                                       \
    vlc_memstream_putc(&out, '\n');                                            \
    }                                                                          \
    while (0)                                                                  \
        ;

    vlc_memstream_puts(&out, "#EXTM3U\n");

    static const char *const TRACK_TYPES[] = {
        [VIDEO_ES] = "VIDEO",
        [AUDIO_ES] = "AUDIO",
        [SPU_ES] = "SUBTITLES",
    };
    static const char *const GROUP_IDS[] = {
        [VIDEO_ES] = "video",
        [AUDIO_ES] = "audio",
        [SPU_ES] = "subtitles",
    };

    const hls_playlist_t *playlist;
    vlc_list_foreach_const (playlist, &sys->media_playlists, node)
    {
        const hls_track_t *track = MediaGetTrack(playlist);
        const es_format_t *fmt = &track->input->fmt;
        assert(fmt->i_cat == VIDEO_ES || fmt->i_cat == AUDIO_ES ||
               fmt->i_cat == SPU_ES);

        MANIFEST_START_TAG("#EXT-X-MEDIA")
            const char *track_type = TRACK_TYPES[fmt->i_cat];
            MANIFEST_ADD_ATTRIBUTE("TYPE=%s", track_type);

            const char *group_id = GROUP_IDS[fmt->i_cat];
            MANIFEST_ADD_ATTRIBUTE("GROUP-ID=\"%s\"", group_id);

            const iso639_lang_t *lang =
                (fmt->psz_language != NULL)
                    ? vlc_find_iso639(fmt->psz_language, false)
                    : NULL;

            if (lang != NULL)
            {
                MANIFEST_ADD_ATTRIBUTE("NAME=\"%s\"", lang->psz_eng_name);
                MANIFEST_ADD_ATTRIBUTE("LANGUAGE=\"%3.3s\"",
                                       lang->psz_iso639_2T);
            }
            else
            {
                MANIFEST_ADD_ATTRIBUTE("NAME=\"%s\"", track->es_id);
            }

            MANIFEST_ADD_ATTRIBUTE("URI=\"%s\"", playlist->url);
        MANIFEST_END_TAG
    }

    /* Format EXT-X-STREAM-INF */
    vlc_list_foreach_const (playlist, &sys->variant_playlists, node)
    {
        MANIFEST_START_TAG("#EXT-X-STREAM-INF")
            unsigned int bandwidth = 0;
            const hls_track_t *track;
            vlc_list_foreach_const (track, &playlist->tracks, node)
                bandwidth += track->input->fmt.i_bitrate;
            MANIFEST_ADD_ATTRIBUTE("BANDWIDTH=%u", bandwidth);

            char *codecs =
                GeneratePlaylistCodecInfo(&sys->media_playlists, playlist);
            if (unlikely(codecs == NULL))
                goto error;
            MANIFEST_ADD_ATTRIBUTE("CODECS=\"%s\"", codecs);
            free(codecs);

            MANIFEST_ADD_ATTRIBUTE("VIDEO=\"%s\"", GROUP_IDS[VIDEO_ES]);
            MANIFEST_ADD_ATTRIBUTE("AUDIO=\"%s\"", GROUP_IDS[AUDIO_ES]);
            MANIFEST_ADD_ATTRIBUTE("SUBTITLES=\"%s\"", GROUP_IDS[SPU_ES]);
        MANIFEST_END_TAG

        if (vlc_memstream_printf(&out, "%s\n", playlist->url) < 0)
            goto error;
    }

#undef MANIFEST_START_TAG
#undef MANIFEST_ADD_ATTRIBUTE
#undef MANIFEST_END_TAG

    if (vlc_memstream_close(&out) != 0)
        return NULL;

    const struct hls_storage_config storage_conf = {
        .name = "index.m3u8",
        .mime = "application/vnd.apple.mpegurl",
    };
    return hls_storage_FromBytes(
        out.ptr, out.length, &storage_conf, &sys->config);
error:
    if (vlc_memstream_close(&out) != 0)
        return NULL;
    free(out.ptr);
    return NULL;
}

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
    hls_segment_queue_Foreach_const(&playlist->segments, segment)
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
        .name = playlist->name, .mime = "application/vnd.apple.mpegurl"};
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

    if (playlist->http_manifest != NULL)
    {
        httpd_UrlCatch(playlist->http_manifest,
                       HTTPD_MSG_GET,
                       HTTPCallback,
                       (httpd_callback_sys_t *)new_manifest);
    }

    if (playlist->manifest != NULL)
        hls_storage_Destroy(playlist->manifest);
    playlist->manifest = new_manifest;
    return VLC_SUCCESS;
}

static hls_block_chain_t ExtractCommonSegment(hls_block_chain_t *muxed_output,
                                              vlc_tick_t max_segment_length)
{
    hls_block_chain_t segment = {.begin = muxed_output->begin};

    vlc_tick_t gop_length = 0;
    block_t *prev = NULL;
    block_t *segment_end = NULL;
    for (block_t *it = muxed_output->begin; it != NULL; it = it->p_next)
    {
        if (it->i_flags & BLOCK_FLAG_HEADER)
        {
            segment_end = prev;
            segment.length += gop_length;
            gop_length = 0;
        }
        if (segment.length + gop_length + it->i_length > max_segment_length)
        {
            if (segment_end == NULL)
            {
                segment_end = prev;
                segment.length += gop_length;
                gop_length = 0;
            }
            break;
        }
        gop_length += it->i_length;
        prev = it;
    }

    if (segment_end != NULL)
    {
        muxed_output->begin = segment_end->p_next;
        segment_end->p_next = NULL;
        muxed_output->length -= segment.length;
    }
    else
    {
        segment.length = gop_length;
        hls_block_chain_Reset(muxed_output);
    }
    return segment;
}

static hls_block_chain_t ExtractSubtitleSegment(hls_block_chain_t *muxed_output,
                                                vlc_tick_t segment_length)
{
    hls_block_chain_t segment = {.begin = muxed_output->begin,
                                 .length = segment_length};
    for (block_t *it = muxed_output->begin; it != NULL; it = it->p_next)
    {
        /* Subtitle segments are segmented at mux level by the
         * hls_sub_segmenter. They have varying length so we use the header flag
         * to extract them properly. */
        if (it->p_next != NULL && it->p_next->i_flags & BLOCK_FLAG_HEADER)
        {
            muxed_output->begin = it->p_next;
            muxed_output->last_header = it->p_next;
            it->p_next = NULL;
            return segment;
        }
        muxed_output->length -= it->i_length;
    }
    hls_block_chain_Reset(muxed_output);
    return segment;
}

static hls_block_chain_t ExtractSegment(hls_playlist_t *playlist)
{
    const vlc_tick_t seglen = playlist->config->segment_length;
    if (playlist->type == HLS_PLAYLIST_TYPE_WEBVTT)
        return ExtractSubtitleSegment(&playlist->muxed_output, seglen);
    return ExtractCommonSegment(&playlist->muxed_output, seglen);
}

static bool IsSegmentSelfDecodable(const hls_block_chain_t *segment)
{
    if (segment->begin == NULL)
        return false;

    return segment->begin->i_flags & BLOCK_FLAG_HEADER;
}

static int ExtractAndAddSegment(hls_playlist_t *playlist,
                                sout_stream_sys_t *sys)
{
    hls_block_chain_t segment = ExtractSegment(playlist);

    if (hls_config_IsMemStorageEnabled(&sys->config) &&
        hls_segment_queue_IsAtMaxCapacity(&playlist->segments))
    {
        const hls_segment_t *to_be_removed =
            hls_segment_GetFirst(&playlist->segments);
        sys->current_memory_cached -=
            hls_storage_GetSize(to_be_removed->storage);
    }

    const bool self_decodable = IsSegmentSelfDecodable(&segment);
    const vlc_tick_t length = segment.length;
    const int status = hls_segment_queue_NewSegment(
        &playlist->segments, segment.begin, segment.length);
    if (unlikely(status != VLC_SUCCESS))
    {
        vlc_error(playlist->logger,
                  "Segment '%u' creation failed",
                  playlist->segments.total_segments + 1);
        return status;
    }
    playlist->muxed_duration += length;

    if (!self_decodable)
    {
        vlc_warning(
            playlist->logger,
            "Segment '%u' does not start with a synchronization frame. It will "
            "not be decodable on its own and will likely fail as a seek point.",
            playlist->segments.total_segments);
        if (playlist->type != HLS_PLAYLIST_TYPE_WEBVTT)
        {
            vlc_warning(
                playlist->logger,
                "It is probably due to a GOP being too large to fit in %" PRIi64
                "s segments. Please adjust your encoding parameters or set a "
                "larger segment size.",
                SEC_FROM_VLC_TICK(playlist->config->segment_length));
        }
    }

    vlc_debug(playlist->logger,
              "Segment '%u' created",
              playlist->segments.total_segments);

    return UpdatePlaylistManifest(playlist);
}

static bool IsSegmentReady(enum hls_playlist_type type,
                           hls_block_chain_t *buffer,
                           vlc_tick_t seglen)
{
    /* The subtitle header outputs one header per segment.  Let's wait until we
     * received the next header before considering the current segment
     * finished. */
    if( type == HLS_PLAYLIST_TYPE_WEBVTT)
        return buffer->begin != buffer->last_header;

    /* Only consider full segments as ready for now. */
    return buffer->length >= seglen;
}

static ssize_t AccessOutWrite(sout_access_out_t *access, block_t *block)
{
    sout_stream_sys_t *sys = access->p_sys;

    size_t size = 0;
    vlc_tick_t length;
    block_ChainProperties(block, NULL, &size, &length);

    if (hls_config_IsMemStorageEnabled(&sys->config))
    {
        sys->current_memory_cached += size;
        if (sys->current_memory_cached >= sys->config.max_memory)
        {
            msg_Err(access,
                    "Maximum memory capacity (%zuKb) for segment storage was "
                    "reached. The HLS server will stop creating segments. "
                    "Please refer to the max-memory option for more info.",
                    BYTES_TO_KB(sys->config.max_memory));
            block_ChainRelease(block);
            return -1;
        }
    }

    bool segments_ready = true;
    hls_playlist_t *it;
    hls_playlists_foreach(it)
    {
        /* Append the muxed output to the playlist tied to this access call. */
        if (it->access == access)
        {
            block_ChainLastAppend(&it->muxed_output.end, block);
            it->muxed_output.length += length;
            if (block->i_flags & BLOCK_FLAG_HEADER)
                it->muxed_output.last_header = block;
        }

        if (!IsSegmentReady(
                it->type, &it->muxed_output, sys->config.segment_length))
            segments_ready = false;
    }


    if (segments_ready)
    {
        hls_playlists_foreach (it)
        {
            while (IsSegmentReady(it->type,
                                  &it->muxed_output,
                                  sys->config.segment_length) &&
                   it->muxed_duration < sys->elapsed_stream_time)
            {
                if (ExtractAndAddSegment(it, sys) != VLC_SUCCESS)
                    return -1;
            }
        }
    }
    return size;
}

static sout_access_out_t *CreateAccessOut(sout_stream_t *stream)
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
    access->p_sys = stream->p_sys;
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

static sout_mux_t *CreatePlaylistMuxer(sout_access_out_t *access,
                                       enum hls_playlist_type type,
                                       const struct hls_config *config)
{
    switch(type)
    {
        case HLS_PLAYLIST_TYPE_TS:
            return sout_MuxNew(access, "ts{use-key-frames}");
        case HLS_PLAYLIST_TYPE_WEBVTT:
            return CreateSubtitleSegmenter(access, config);
    }
    return NULL;
}

static hls_playlist_t *CreatePlaylist(sout_stream_t *stream,
                                      enum hls_playlist_type type)
{
    sout_stream_sys_t *sys = stream->p_sys;

    hls_playlist_t *playlist = malloc(sizeof(*playlist));
    if (unlikely(playlist == NULL))
        return NULL;

    playlist->access = CreateAccessOut(stream);
    if (unlikely(playlist->access == NULL))
        goto access_err;

    playlist->mux = CreatePlaylistMuxer(playlist->access, type, &sys->config);
    if (unlikely(playlist->mux == NULL))
        goto mux_err;

    playlist->id = sys->playlist_created_count;
    playlist->type = type;
    playlist->config = &sys->config;
    playlist->ended = false;
    playlist->muxed_duration = 0;

    playlist->url = FormatPlaylistManifestURL(playlist);
    if (unlikely(playlist->url == NULL))
        goto url_err;

    playlist->name = playlist->url + strlen(sys->config.base_url) + 1;

    playlist->logger = vlc_LogHeaderCreate(stream->obj.logger, playlist->name);
    if (unlikely(playlist->logger == NULL))
        goto log_err;

    struct hls_segment_queue_config config = {
        .playlist_id = playlist->id,
        .playlist_type = type,
        .httpd_ref = sys->http_host,
        .httpd_callback = HTTPCallback,
    };
    hls_segment_queue_Init(&playlist->segments, &config, &sys->config);

    hls_block_chain_Reset(&playlist->muxed_output);

    playlist->manifest = NULL;
    if (sys->http_host != NULL)
    {
        playlist->http_manifest =
            httpd_UrlNew(sys->http_host, playlist->url, NULL, NULL);
        if (playlist->http_manifest == NULL)
            goto manifest_err;
    }
    else
        playlist->http_manifest = NULL;

    if (UpdatePlaylistManifest(playlist) != VLC_SUCCESS)
        goto error;

    vlc_list_init(&playlist->tracks);

    vlc_info(playlist->logger, "Playlist created");

    return playlist;
error:
    if (playlist->http_manifest != NULL)
        httpd_UrlDelete(playlist->http_manifest);
manifest_err:
    hls_segment_queue_Clear(&playlist->segments);
    vlc_LogDestroy(playlist->logger);
log_err:
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

    if (playlist->http_manifest != NULL)
        httpd_UrlDelete(playlist->http_manifest);

    if (playlist->manifest != NULL)
        hls_storage_Destroy(playlist->manifest);

    block_ChainRelease(playlist->muxed_output.begin);
    hls_segment_queue_Clear(&playlist->segments);

    vlc_list_remove(&playlist->node);

    vlc_LogDestroy(playlist->logger);
    free(playlist->url);

    free(playlist);
}

static hls_playlist_t *AddPlaylist(sout_stream_t *stream,
                                   enum hls_playlist_type type,
                                   struct vlc_list *head)
{
    hls_playlist_t *variant = CreatePlaylist(stream, type);
    if (variant == NULL)
        return NULL;

    vlc_list_append(&variant->node, head);

    sout_stream_sys_t *sys = stream->p_sys;
    ++sys->playlist_created_count;
    return variant;
}

static void *
Add(sout_stream_t *stream, const es_format_t *fmt, const char *es_id)
{
    if (!hls_codec_IsSupported(fmt))
        return NULL;

    sout_stream_sys_t *sys = stream->p_sys;

    // Either retrieve the already created playlist from the map or create it.
    struct hls_variant_stream_map *map =
        hls_variant_map_FromESID(&sys->variant_stream_maps, es_id);
    hls_playlist_t *playlist;
    if (map != NULL)
    {
        playlist = map->playlist_ref;
        if (playlist == NULL)
            playlist = AddPlaylist(stream, HLS_PLAYLIST_TYPE_TS, &sys->variant_playlists);
    }
    else if (fmt->i_cat == SPU_ES)
        playlist = AddPlaylist(
            stream, HLS_PLAYLIST_TYPE_WEBVTT, &sys->media_playlists);
    else
        playlist =
            AddPlaylist(stream, HLS_PLAYLIST_TYPE_TS, &sys->media_playlists);

    if (playlist == NULL)
        return NULL;

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

    struct hls_storage *new_manifest = GenerateMainManifest(sys);
    if (unlikely(new_manifest == NULL))
    {
        vlc_list_remove(&track->node);
        free(track);
        goto error;
    }

    if (sys->http_host != NULL)
    {
        httpd_UrlCatch(sys->http_manifest,
                       HTTPD_MSG_GET,
                       HTTPCallback,
                       (httpd_callback_sys_t *)new_manifest);
    }

    if (sys->manifest != NULL)
        hls_storage_Destroy(sys->manifest);
    sys->manifest = new_manifest;

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

        track->playlist_ref->ended = true;
        ExtractAndAddSegment(track->playlist_ref, sys);
        UpdatePlaylistManifest(track->playlist_ref);

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

/** PCR events are used to have a reliable stream time status. */
static void SetPCR(sout_stream_t *stream, vlc_tick_t pcr)
{
    sout_stream_sys_t *sys = stream->p_sys;

    if (sys->first_pcr == VLC_TICK_INVALID)
    {
        sys->first_pcr = pcr;
        return;
    }

    sys->elapsed_stream_time = pcr - sys->first_pcr;
    const hls_playlist_t *playlist;
    vlc_list_foreach_const (playlist, &sys->media_playlists, node)
    {
        if (playlist->type != HLS_PLAYLIST_TYPE_WEBVTT)
            continue;

        hls_sub_segmenter_SignalStreamUpdate(playlist->mux,
                                             sys->elapsed_stream_time);
    }
}

static int Control(sout_stream_t *stream, int query, va_list args)
{
    const sout_stream_sys_t *sys = stream->p_sys;
    switch (query)
    {
        case SOUT_STREAM_IS_SYNCHRONOUS:
            *va_arg(args, bool *) = sys->config.pace;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int InitHTTP(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;
    sys->http_host = vlc_http_HostNew(VLC_OBJECT(stream));
    if (sys->http_host == NULL)
        return VLC_EGENERIC;

    char *mainfest_url;
    if (asprintf(&mainfest_url, "%s/stream.m3u8", sys->config.base_url) == -1)
        goto error;

    sys->http_manifest = httpd_UrlNew(sys->http_host, mainfest_url, NULL, NULL);
    free(mainfest_url);
    if (sys->http_manifest == NULL)
        goto error;
    return VLC_SUCCESS;
error:
    httpd_HostDelete(sys->http_host);
    return VLC_EGENERIC;
}

static void Close(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;

    if (sys->http_host != NULL)
    {
        httpd_UrlDelete(sys->http_manifest);
        httpd_HostDelete(sys->http_host);
    }

    if (sys->manifest != NULL)
        hls_storage_Destroy(sys->manifest);

    hls_config_Clean(&sys->config);

    hls_variant_maps_Destroy(&sys->variant_stream_maps);

    free(sys);
}

#define SOUT_CFG_PREFIX "sout-hls-"

static int Open(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;

    sout_stream_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    stream->p_sys = sys;

    static const char *const options[] = {"base-url",
                                          "host-http",
                                          "max-memory",
                                          "num-seg",
                                          "out-dir",
                                          "pace",
                                          "seg-len",
                                          "variants",
                                          NULL};
    config_ChainParse(stream, SOUT_CFG_PREFIX, options, stream->p_cfg);

    sys->config.base_url = var_GetString(stream, SOUT_CFG_PREFIX "base-url");
    sys->config.outdir =
        var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "out-dir");
    sys->config.max_segments =
        var_GetInteger(stream, SOUT_CFG_PREFIX "num-seg");
    sys->config.pace = var_GetBool(stream, SOUT_CFG_PREFIX "pace");
    sys->config.segment_length =
        VLC_TICK_FROM_SEC(var_GetInteger(stream, SOUT_CFG_PREFIX "seg-len"));
    sys->config.max_memory =
        BYTES_FROM_KB(var_GetInteger(stream, SOUT_CFG_PREFIX "max-memory"));

    int status = VLC_EINVAL;

    vlc_vector_init(&sys->variant_stream_maps);
    char *variants = var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "variants");
    if (variants == NULL)
    {
        msg_Err(stream,
                "At least one variant mapping needs to be specified with the "
                "\"" SOUT_CFG_PREFIX "variants\" option");
        goto variant_error;
    }
    status = hls_variant_maps_Parse(variants, &sys->variant_stream_maps);
    free(variants);
    if (status != VLC_SUCCESS)
    {
        if (status == VLC_EINVAL)
            msg_Err(stream,
                    "Wrong variant mapping syntax. It should look like: "
                    "\"{id1,id2},{id3,id4},...\"");
        goto variant_error;
    }

    if (var_GetBool(stream, SOUT_CFG_PREFIX "host-http"))
    {
        status = InitHTTP(stream);
        if (status != VLC_SUCCESS)
            goto error;
    }
    else if (sys->config.outdir != NULL)
    {
        sys->http_host = NULL;
        sys->http_manifest = NULL;
    }
    else
    {
        msg_Err(stream,
                "No output directory specified."
                " See \"" SOUT_CFG_PREFIX "out-dir\"");
        status = VLC_EINVAL;
        goto error;
    }

    sys->manifest = NULL;

    sys->playlist_created_count = 0;

    vlc_list_init(&sys->variant_playlists);
    vlc_list_init(&sys->media_playlists);

    sys->elapsed_stream_time = 0;
    sys->first_pcr = VLC_TICK_INVALID;

    sys->current_memory_cached = 0;

    static const struct sout_stream_operations ops = {
        .add = Add,
        .del = Del,
        .send = Send,
        .set_pcr = SetPCR,
        .control = Control,
        .close = Close,
    };
    stream->ops = &ops;

    return VLC_SUCCESS;
error:
    hls_variant_maps_Destroy(&sys->variant_stream_maps);
variant_error:
    hls_config_Clean(&sys->config);
    free(sys);
    return status;
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
#define HOSTHTTP_LONGTEXT                                                      \
    N_("The internal HTTP server will share the HLS output. This is "          \
       "unadvised for the common use case where an external HTTP server "      \
       "implementation will be way more efficient. This can be useful for "    \
       "quick testing on networks with a small load")
#define HOSTHTTP_TEXT                                                          \
    N_("Enable hosting the HLS output on the internal HTTP server")
#define MAXMEMORY_LONGTEXT                                                     \
    N_("Maximum allowed memory for segment storage in Kb. This option is "     \
       "only relevant when segments are stored in internal memory. If the "    \
       "value is bypassed, the HLS server will stop with an error")
#define MAXMEMORY_TEXT N_("Maximum allowed memory for segment storage in Kb")
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
    add_bool(SOUT_CFG_PREFIX "host-http", false, HOSTHTTP_TEXT, HOSTHTTP_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "max-memory", 20000, MAXMEMORY_TEXT, MAXMEMORY_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "num-seg", 0, NUMSEG_TEXT, NUMSEG_TEXT)
    add_string(SOUT_CFG_PREFIX "out-dir", NULL, OUTDIR_TEXT, OUTDIR_LONGTEXT)
    add_bool(SOUT_CFG_PREFIX "pace", false, PACE_TEXT, PACE_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "seg-len", 4, SEGLEN_TEXT, SEGLEN_LONGTEXT)

    set_callback(Open)
vlc_module_end()
