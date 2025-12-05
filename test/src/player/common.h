// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * common.h: common helpers for the player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_player.h>
#include <vlc_vector.h>
#include <vlc_filter.h>

#if defined(ZVBI_COMPILED)
# define TELETEXT_DECODER "zvbi,"
#elif defined(TELX_COMPILED)
# define TELETEXT_DECODER "telx,"
#else
# define TELETEXT_DECODER ""
#endif

static const char *state_to_string(enum vlc_player_state state)
{
    switch (state)
    {
        case VLC_PLAYER_STATE_STOPPED:      return "stopped";
        case VLC_PLAYER_STATE_STARTED:      return "started";
        case VLC_PLAYER_STATE_PLAYING:      return "playing";
        case VLC_PLAYER_STATE_PAUSED:       return "paused";
        case VLC_PLAYER_STATE_STOPPING:     return "stopping";
    }
    vlc_assert_unreachable();
};

struct report_capabilities
{
    int old_caps;
    int new_caps;
};

struct report_position
{
    vlc_tick_t time;
    double pos;
};

struct report_track_list
{
    enum vlc_player_list_action action;
    struct vlc_player_track *track;
};

struct report_track_selection
{
    vlc_es_id_t *unselected_id;
    vlc_es_id_t *selected_id;
};

struct report_program_list
{
    enum vlc_player_list_action action;
    struct vlc_player_program *prgm;
};

struct report_program_selection
{
    int unselected_id;
    int selected_id;
};

struct report_chapter_selection
{
    size_t title_idx;
    size_t chapter_idx;
};

struct report_category_delay
{
    enum es_format_category_e cat;
    vlc_tick_t delay;
};

struct report_signal
{
    float quality;
    float strength;
};

struct report_vout
{
    enum vlc_player_vout_action action;
    vout_thread_t *vout;
    enum vlc_vout_order order;
    vlc_es_id_t *es_id;
};

struct report_media_subitems
{
    size_t count;
    input_item_t **items;
};

struct report_media_attachments
{
    input_attachment_t **array;
    size_t count;
};

#define PLAYER_REPORT_LIST \
    X(input_item_t *, on_current_media_changed) \
    X(enum vlc_player_state, on_state_changed) \
    X(enum vlc_player_error, on_error_changed) \
    X(float, on_buffering_changed) \
    X(float, on_rate_changed) \
    X(struct report_capabilities, on_capabilities_changed) \
    X(struct report_position, on_position_changed) \
    X(vlc_tick_t, on_length_changed) \
    X(struct report_track_list, on_track_list_changed) \
    X(struct report_track_selection, on_track_selection_changed) \
    X(struct report_program_list, on_program_list_changed) \
    X(struct report_program_selection, on_program_selection_changed) \
    X(vlc_player_title_list *, on_titles_changed) \
    X(size_t, on_title_selection_changed) \
    X(struct report_chapter_selection, on_chapter_selection_changed) \
    X(bool, on_teletext_menu_changed) \
    X(bool, on_teletext_enabled_changed) \
    X(unsigned, on_teletext_page_changed) \
    X(bool, on_teletext_transparency_changed) \
    X(struct report_category_delay, on_category_delay_changed) \
    X(bool, on_recording_changed) \
    X(struct report_signal, on_signal_changed) \
    X(struct input_stats_t, on_statistics_changed) \
    X(struct report_vout, on_vout_changed) \
    X(input_item_t *, on_media_meta_changed) \
    X(input_item_t *, on_media_epg_changed) \
    X(struct report_media_subitems, on_media_subitems_changed) \
    X(struct report_media_attachments, on_media_attachments_added) \
    X(int, on_next_frame_status) \
    X(int, on_prev_frame_status)

struct report_aout_first_pts
{
    vlc_tick_t first_pts;
};

#define REPORT_LIST \
    X(vlc_tick_t, on_aout_first_pts) \

#define X(type, name) typedef struct VLC_VECTOR(type) vec_##name;
PLAYER_REPORT_LIST
REPORT_LIST
#undef X

#define X(type, name) vec_##name name;
struct reports
{
PLAYER_REPORT_LIST
REPORT_LIST
};
#undef X

static inline void
reports_init(struct reports *report)
{
#define X(type, name) vlc_vector_init(&report->name);
PLAYER_REPORT_LIST
REPORT_LIST
#undef X
}

struct media_params
{
    vlc_tick_t length;
    vlc_tick_t audio_sample_length;
    size_t track_count[DATA_ES];
    size_t program_count;

    bool video_packetized, audio_packetized, sub_packetized;

    unsigned video_frame_rate;
    unsigned video_frame_rate_base;

    size_t title_count;
    size_t chapter_count;
    size_t attachment_count;

    bool can_seek;
    bool can_pause;
    bool error;
    bool null_names;
    bool report_length;
    vlc_tick_t pts_delay;

    const char *config;
    const char *discontinuities;
};

#define DEFAULT_MEDIA_PARAMS(param_length) { \
    .length = param_length, \
    .audio_sample_length = VLC_TICK_FROM_MS(100), \
    .track_count = { \
        [VIDEO_ES] = 1, \
        [AUDIO_ES] = 1, \
        [SPU_ES] = 1, \
    }, \
    .program_count = 0, \
    .video_packetized = true, .audio_packetized = true, .sub_packetized = true,\
    .video_frame_rate = 25, \
    .video_frame_rate_base = 1, \
    .title_count = 0, \
    .chapter_count = 0, \
    .attachment_count = 0, \
    .can_seek = true, \
    .can_pause = true, \
    .error = false, \
    .null_names = false, \
    .report_length = true, \
    .pts_delay = DEFAULT_PTS_DELAY, \
    .config = NULL, \
    .discontinuities = NULL, \
}

#define DISABLE_VIDEO_OUTPUT (1 << 0)
#define DISABLE_AUDIO_OUTPUT (1 << 1)
#define DISABLE_VIDEO        (1 << 2)
#define DISABLE_AUDIO        (1 << 3)
#define AUDIO_INSTANT_DRAIN  (1 << 4)
#define CLOCK_MASTER_MONOTONIC (1 << 5)

struct ctx
{
    int flags;

    libvlc_instance_t *vlc;
    vlc_player_t *player;
    vlc_player_listener_id *listener;
    struct VLC_VECTOR(input_item_t *) next_medias;
    struct VLC_VECTOR(input_item_t *) added_medias;
    struct VLC_VECTOR(input_item_t *) played_medias;

    size_t program_switch_count;
    size_t extra_start_count;
    struct media_params params;
    float rate;

    size_t last_state_idx;

    vlc_cond_t wait;
    struct reports report;
};

static inline struct ctx *
get_ctx(vlc_player_t *player, void *data)
{
    assert(data);
    struct ctx *ctx = data;
    assert(player == ctx->player);
    return ctx;
}

#define VEC_PUSH(vec, item) do { \
    bool success = vlc_vector_push(&ctx->report.vec, item); \
    assert(success); \
    vlc_cond_signal(&ctx->wait); \
} while(0)

static inline void
player_on_current_media_changed(vlc_player_t *player,
                                input_item_t *new_media, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    if (new_media)
        input_item_Hold(new_media);
    VEC_PUSH(on_current_media_changed, new_media);

    if (ctx->next_medias.size == 0)
        return;

    input_item_t *next_media = ctx->next_medias.data[0];
    vlc_vector_remove(&ctx->next_medias, 0);

    bool success = vlc_vector_push(&ctx->added_medias, next_media);
    assert(success);
    success = vlc_vector_push(&ctx->played_medias, next_media);
    assert(success);
    vlc_player_SetNextMedia(player, next_media);
}

static inline void
player_on_state_changed(vlc_player_t *player, enum vlc_player_state state,
                        void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_state_changed, state);
}

static inline void
player_on_error_changed(vlc_player_t *player, enum vlc_player_error error,
                        void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_error_changed, error);
}

static inline void
player_on_buffering_changed(vlc_player_t *player, float new_buffering,
                            void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_buffering_changed, new_buffering);
}

static inline void
player_on_rate_changed(vlc_player_t *player, float new_rate, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_rate_changed, new_rate);
}

static inline void
player_on_capabilities_changed(vlc_player_t *player, int old_caps, int new_caps,
                               void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_capabilities report = {
        .old_caps = old_caps,
        .new_caps = new_caps,
    };
    VEC_PUSH(on_capabilities_changed, report);
}

static inline void
player_on_position_changed(vlc_player_t *player, vlc_tick_t time,
                           double pos, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_position report = {
        .time = time,
        .pos = pos,
    };
    VEC_PUSH(on_position_changed, report);
}

static inline void
player_on_length_changed(vlc_player_t *player, vlc_tick_t new_length,
                         void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_length_changed, new_length);
}

static inline void
player_on_track_list_changed(vlc_player_t *player,
                             enum vlc_player_list_action action,
                             const struct vlc_player_track *track,
                             void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_track_list report = {
        .action = action,
        .track = vlc_player_track_Dup(track),
    };
    assert(report.track);
    VEC_PUSH(on_track_list_changed, report);
}

static inline void
player_on_track_selection_changed(vlc_player_t *player,
                                  vlc_es_id_t *unselected_id,
                                  vlc_es_id_t *selected_id, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_track_selection report = {
        .unselected_id = unselected_id ? vlc_es_id_Hold(unselected_id) : NULL,
        .selected_id = selected_id ? vlc_es_id_Hold(selected_id) : NULL,
    };
    assert(!!unselected_id == !!report.unselected_id);
    assert(!!selected_id == !!report.selected_id);
    VEC_PUSH(on_track_selection_changed, report);
}

static inline void
player_on_program_list_changed(vlc_player_t *player,
                               enum vlc_player_list_action action,
                               const struct vlc_player_program *prgm,
                               void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_program_list report = {
        .action = action,
        .prgm = vlc_player_program_Dup(prgm)
    };
    assert(report.prgm);
    VEC_PUSH(on_program_list_changed, report);
}

static inline void
player_on_program_selection_changed(vlc_player_t *player,
                                    int unselected_id, int selected_id,
                                    void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_program_selection report = {
        .unselected_id = unselected_id,
        .selected_id = selected_id,
    };
    VEC_PUSH(on_program_selection_changed, report);
}

static inline void
player_on_titles_changed(vlc_player_t *player,
                         vlc_player_title_list *titles, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    if (titles)
        vlc_player_title_list_Hold(titles);
    VEC_PUSH(on_titles_changed, titles);
}

static inline void
player_on_title_selection_changed(vlc_player_t *player,
                                  const struct vlc_player_title *new_title,
                                  size_t new_idx, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_title_selection_changed, new_idx);
    (void) new_title;
}

static inline void
player_on_chapter_selection_changed(vlc_player_t *player,
                                    const struct vlc_player_title *title,
                                    size_t title_idx,
                                    const struct vlc_player_chapter *chapter,
                                    size_t chapter_idx, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_chapter_selection report = {
        .title_idx = title_idx,
        .chapter_idx = chapter_idx,
    };
    VEC_PUSH(on_chapter_selection_changed, report);
    (void) title;
    (void) chapter;
}

static inline void
player_on_teletext_menu_changed(vlc_player_t *player,
                                bool has_teletext_menu, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_teletext_menu_changed, has_teletext_menu);
}

static inline void
player_on_teletext_enabled_changed(vlc_player_t *player,
                                   bool enabled, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_teletext_enabled_changed, enabled);
}

static inline void
player_on_teletext_page_changed(vlc_player_t *player,
                                unsigned new_page, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_teletext_page_changed, new_page);
}

static inline void
player_on_teletext_transparency_changed(vlc_player_t *player,
                                        bool enabled, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_teletext_transparency_changed, enabled);
}

static inline void
player_on_category_delay_changed(vlc_player_t *player,
                                 enum es_format_category_e cat, vlc_tick_t new_delay,
                                 void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_category_delay report = {
        .cat = cat,
        .delay = new_delay,
    };
    VEC_PUSH(on_category_delay_changed, report);
}

static inline void
player_on_recording_changed(vlc_player_t *player, bool recording, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_recording_changed, recording);
}

static inline void
player_on_signal_changed(vlc_player_t *player,
                         float quality, float strength, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_signal report = {
        .quality = quality,
        .strength = strength,
    };
    VEC_PUSH(on_signal_changed, report);
}

static inline void
player_on_statistics_changed(vlc_player_t *player,
                        const struct input_stats_t *stats, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct input_stats_t dup = *stats;
    VEC_PUSH(on_statistics_changed, dup);
}

static inline void
player_on_vout_changed(vlc_player_t *player,
                       enum vlc_player_vout_action action,
                       vout_thread_t *vout, enum vlc_vout_order order,
                       vlc_es_id_t *es_id, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    struct report_vout report = {
        .action = action,
        .vout = vout_Hold(vout),
        .order = order,
        .es_id = vlc_es_id_Hold(es_id),
    };
    assert(report.es_id);
    VEC_PUSH(on_vout_changed, report);
}

static inline void
player_on_media_meta_changed(vlc_player_t *player, input_item_t *media,
                             void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    input_item_Hold(media);
    VEC_PUSH(on_media_meta_changed, media);
}

static inline void
player_on_media_epg_changed(vlc_player_t *player, input_item_t *media,
                            void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    input_item_Hold(media);
    VEC_PUSH(on_media_epg_changed, media);
}

static inline void
player_on_media_subitems_changed(vlc_player_t *player, input_item_t *media,
                           const input_item_node_t *subitems, void *data)
{
    (void) media;
    struct ctx *ctx = get_ctx(player, data);

    struct report_media_subitems report = {
        .count = subitems->i_children,
        .items = vlc_alloc(subitems->i_children, sizeof(input_item_t)),
    };
    assert(report.items);
    for (int i = 0; i < subitems->i_children; ++i)
        report.items[i] = input_item_Hold(subitems->pp_children[i]->p_item);
    VEC_PUSH(on_media_subitems_changed, report);
}

static inline void
player_on_media_attachments_added(vlc_player_t *player,
                                  input_item_t *media,
                                  input_attachment_t *const *array, size_t count,
                                  void *data)
{
    (void) media;
    struct ctx *ctx = get_ctx(player, data);

    struct report_media_attachments report = {
        .array = vlc_alloc(count, sizeof(input_attachment_t *)),
        .count = count,
    };
    assert(report.array);
    for (size_t i = 0; i < count; ++i)
        report.array[i] = vlc_input_attachment_Hold(array[i]);
    VEC_PUSH(on_media_attachments_added, report);
}

static inline void
player_on_next_frame_status(vlc_player_t *player, int status, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_next_frame_status, status);
}

static inline void
player_on_prev_frame_status(vlc_player_t *player, int status, void *data)
{
    struct ctx *ctx = get_ctx(player, data);
    VEC_PUSH(on_prev_frame_status, status);
}

#define VEC_LAST(vec) (vec)->data[(vec)->size - 1]
#define assert_position(ctx, report) do { \
    assert(fabs((report)->pos - (report)->time / (float) ctx->params.length) < 0.001); \
} while (0)

/* Wait for the next state event */
static inline void
wait_state(struct ctx *ctx, enum vlc_player_state state)
{
    vec_on_state_changed *vec = &ctx->report.on_state_changed;
    for (;;)
    {
        while (vec->size <= ctx->last_state_idx)
            vlc_player_CondWait(ctx->player, &ctx->wait);
        for (size_t i = ctx->last_state_idx; i < vec->size; ++i)
            if ((vec)->data[i] == state)
            {
                ctx->last_state_idx = i + 1;
                return;
            }
        ctx->last_state_idx = vec->size;
    }
}

#define assert_state(ctx, state) do { \
    vec_on_state_changed *vec = &ctx->report.on_state_changed; \
    assert(VEC_LAST(vec) == state); \
} while(0)

static inline void state_list_dump(vec_on_state_changed *vec)
{
    fprintf(stderr, "Dumping state:\n");
    for (size_t i = 0; i < vec->size; ++i)
        fprintf(stderr, "state[%zu] = %s\n",
                i, state_to_string(vec->data[i]));
}

static inline bool
state_equal(vec_on_state_changed *vec, int location, enum vlc_player_state state)
{
    assert(location < (int)vec->size && -location < (int)(vec->size + 1));
    size_t index = location < 0 ? vec->size + location : (size_t)location;
    const char *str_state_vec = state_to_string(vec->data[index]);
    const char *str_state_check = state_to_string(state);

    fprintf(stderr, "Checking state[%d] == '%s', is '%s'\n",
            location, str_state_vec, str_state_check);
    return vec->data[index] == state;
}

#define state_equal(ctx, index, state) \
    (state_equal)(&ctx->report.on_state_changed, index, state)

#define assert_normal_state(ctx) do { \
    vec_on_state_changed *vec = &ctx->report.on_state_changed; \
    state_list_dump(vec); \
    assert(vec->size >= 4); \
    assert(state_equal(ctx, -4, VLC_PLAYER_STATE_STARTED)); \
    assert(state_equal(ctx, -3, VLC_PLAYER_STATE_PLAYING)); \
    assert(state_equal(ctx, -2, VLC_PLAYER_STATE_STOPPING)); \
    assert(state_equal(ctx, -1, VLC_PLAYER_STATE_STOPPED)); \
} while(0)


static inline size_t
get_buffering_count(struct ctx *ctx)
{
    vec_on_buffering_changed *vec = &ctx->report.on_buffering_changed;
    size_t count = 0;
    for (size_t i = 0; i < vec->size; ++i)
        if (vec->data[i] == 1.0f)
            count++;
    return count;
}

static inline void
ctx_reset(struct ctx *ctx)
{
#define FOREACH_VEC(item, vec) vlc_vector_foreach(item, &ctx->report.vec)
#define CLEAN_MEDIA_VEC(vec) do { \
    input_item_t *media; \
    FOREACH_VEC(media, vec) { \
        if (media) \
            input_item_Release(media); \
    } \
} while(0)


    CLEAN_MEDIA_VEC(on_current_media_changed);
    CLEAN_MEDIA_VEC(on_media_meta_changed);
    CLEAN_MEDIA_VEC(on_media_epg_changed);

    {
        struct report_track_list report;
        FOREACH_VEC(report, on_track_list_changed)
            vlc_player_track_Delete(report.track);
    }

    {
        struct report_track_selection report;
        FOREACH_VEC(report, on_track_selection_changed)
        {
            if (report.unselected_id)
                vlc_es_id_Release(report.unselected_id);
            if (report.selected_id)
                vlc_es_id_Release(report.selected_id);
        }
    }

    {
        struct report_program_list report;
        FOREACH_VEC(report, on_program_list_changed)
            vlc_player_program_Delete(report.prgm);
    }

    {
        vlc_player_title_list *titles;
        FOREACH_VEC(titles, on_titles_changed)
        {
            if (titles)
                vlc_player_title_list_Release(titles);
        }
    }

    {
        struct report_vout report;
        FOREACH_VEC(report, on_vout_changed)
        {
            vout_Release(report.vout);
            vlc_es_id_Release(report.es_id);
        }
    }

    {
        struct report_media_subitems report;
        FOREACH_VEC(report, on_media_subitems_changed)
        {
            for (size_t i = 0; i < report.count; ++i)
                input_item_Release(report.items[i]);
            free(report.items);
        }
    }

    {
        struct report_media_attachments report;
        FOREACH_VEC(report, on_media_attachments_added)
        {
            for (size_t i = 0; i < report.count; ++i)
                vlc_input_attachment_Release(report.array[i]);
            free(report.array);
        }
    }
#undef CLEAN_MEDIA_VEC
#undef FOREACH_VEC

#define X(type, name) vlc_vector_clear(&ctx->report.name);
PLAYER_REPORT_LIST
REPORT_LIST
#undef X

    input_item_t *media;
    vlc_vector_foreach(media, &ctx->next_medias)
    {
        assert(media);
        input_item_Release(media);
    }
    vlc_vector_clear(&ctx->next_medias);

    vlc_vector_foreach(media, &ctx->added_medias)
        if (media)
            input_item_Release(media);
    vlc_vector_clear(&ctx->added_medias);

    vlc_vector_clear(&ctx->played_medias);

    ctx->extra_start_count = 0;
    ctx->program_switch_count = 1;
    ctx->rate = 1.f;

    ctx->last_state_idx = 0;
};

static inline input_item_t *
create_mock_media(const char *name, const struct media_params *params)
{
    assert(params);
    char *url;
    int ret = asprintf(&url,
        "mock://video_width=4;video_height=4;"
        "video_track_count=%zu;audio_track_count=%zu;sub_track_count=%zu;"
        "program_count=%zu;video_packetized=%d;audio_packetized=%d;"
        "sub_packetized=%d;length=%"PRId64";audio_sample_length=%"PRId64";"
        "video_frame_rate=%u;video_frame_rate_base=%u;"
        "title_count=%zu;chapter_count=%zu;"
        "can_seek=%d;can_pause=%d;error=%d;null_names=%d;"
        "report_length=%d;pts_delay=%"PRId64";"
        "config=%s;discontinuities=%s;attachment_count=%zu",
        params->track_count[VIDEO_ES], params->track_count[AUDIO_ES],
        params->track_count[SPU_ES], params->program_count,
        params->video_packetized, params->audio_packetized,
        params->sub_packetized, params->length, params->audio_sample_length,
        params->video_frame_rate, params->video_frame_rate_base,
        params->title_count, params->chapter_count,
        params->can_seek, params->can_pause, params->error, params->null_names,
        params->report_length, params->pts_delay,
        params->config ? params->config : "",
        params->discontinuities ? params->discontinuities : "",
        params->attachment_count);
    assert(ret != -1);
    input_item_t *item = input_item_New(url, name);
    assert(item);
    free(url);
    return item;
}

static inline input_item_t *
player_create_mock_media(struct ctx *ctx, const char *name,
                         const struct media_params *params)
{
    assert(params);

    input_item_t *media = create_mock_media(name, params);
    assert(media != NULL);

    ctx->params = *params;
    if (ctx->params.chapter_count > 0 && ctx->params.title_count == 0)
        ctx->params.title_count = 1;
    if (ctx->params.program_count == 0)
        ctx->params.program_count = 1;
    return media;
}

static inline void
player_set_current_mock_media(struct ctx *ctx, const char *name,
                              const struct media_params *params, bool ignored)
{
    input_item_t *media;
    if (name)
        media = player_create_mock_media(ctx, name, params);
    else
        media = NULL;
    int ret = vlc_player_SetCurrentMedia(ctx->player, media);
    assert(ret == VLC_SUCCESS);

    bool success = vlc_vector_push(&ctx->added_medias, media);
    assert(success);

    if (!ignored)
    {
        success = vlc_vector_push(&ctx->played_medias, media);
        assert(success);
    }
}

static inline void
player_set_next_mock_media(struct ctx *ctx, const char *name,
                           const struct media_params *params)
{
    assert(name != NULL);
    if (ctx->added_medias.size == 0)
    {
        input_item_t *media = player_create_mock_media(ctx, name, params);
        vlc_player_SetNextMedia(ctx->player, media);
        bool success = vlc_vector_push(&ctx->added_medias, media);
        assert(success);
        success = vlc_vector_push(&ctx->played_medias, media);
        assert(success);
    }
    else
    {
        input_item_t *media = create_mock_media(name, params);
        assert(media);

        assert(ctx->added_medias.size > 0);
        bool success = vlc_vector_push(&ctx->next_medias, media);
        assert(success);
    }
}

static inline void
player_set_rate(struct ctx *ctx, float rate)
{
    vlc_player_ChangeRate(ctx->player, rate);
    ctx->rate = rate;
}

static inline void
player_start(struct ctx *ctx)
{
    int ret = vlc_player_Start(ctx->player);
    assert(ret == VLC_SUCCESS);
}

static inline void
test_end_prestop_rate(struct ctx *ctx)
{
    if (ctx->rate != 1.0f)
    {
        vec_on_rate_changed *vec = &ctx->report.on_rate_changed;
        while (vec->size == 0)
            vlc_player_CondWait(ctx->player, &ctx->wait);
        assert(vec->size > 0);
        assert(VEC_LAST(vec) == ctx->rate);
    }

}

static inline void
test_end_prestop_length(struct ctx *ctx)
{
    vlc_player_t *player = ctx->player;
    vec_on_length_changed *vec = &ctx->report.on_length_changed;
    while (vec->size != ctx->played_medias.size)
        vlc_player_CondWait(ctx->player, &ctx->wait);
    for (size_t i = 0; i < vec->size; ++i)
        assert(vec->data[i] == ctx->params.length);
    assert(ctx->params.length == vlc_player_GetLength(player));
}

static inline void
test_end_prestop_capabilities(struct ctx *ctx)
{
    vlc_player_t *player = ctx->player;
    vec_on_capabilities_changed *vec = &ctx->report.on_capabilities_changed;
    while (vec->size == 0)
        vlc_player_CondWait(ctx->player, &ctx->wait);
    int new_caps = VEC_LAST(vec).new_caps;
    assert(vlc_player_CanSeek(player) == ctx->params.can_seek
        && !!(new_caps & VLC_PLAYER_CAP_SEEK) == ctx->params.can_seek);
    assert(vlc_player_CanPause(player) == ctx->params.can_pause
        && !!(new_caps & VLC_PLAYER_CAP_PAUSE) == ctx->params.can_pause);
}

static inline void
test_end_prestop_buffering(struct ctx *ctx)
{
    vec_on_buffering_changed *vec = &ctx->report.on_buffering_changed;
    while (vec->size == 0 || VEC_LAST(vec) != 1.0f)
        vlc_player_CondWait(ctx->player, &ctx->wait);
    assert(vec->size >= 2);
    assert(vec->data[0] == 0.0f);
}


static inline void
test_end_poststop_state(struct ctx *ctx)
{
    vec_on_state_changed *vec = &ctx->report.on_state_changed;
    assert(vec->size > 1);
    assert(vec->data[0] == VLC_PLAYER_STATE_STARTED);
    assert(VEC_LAST(vec) == VLC_PLAYER_STATE_STOPPED);
}

static inline void
test_end_poststop_tracks(struct ctx *ctx)
{
    vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
    struct {
        size_t added;
        size_t removed;
    } tracks[] = {
        [VIDEO_ES] = { 0, 0 },
        [AUDIO_ES] = { 0, 0 },
        [SPU_ES] = { 0, 0 },
    };
    struct report_track_list report;
    vlc_vector_foreach(report, vec)
    {
        assert(report.track->fmt.i_cat == VIDEO_ES
            || report.track->fmt.i_cat == AUDIO_ES
            || report.track->fmt.i_cat == SPU_ES);
        if (report.action == VLC_PLAYER_LIST_ADDED)
            tracks[report.track->fmt.i_cat].added++;
        else if (report.action == VLC_PLAYER_LIST_REMOVED)
            tracks[report.track->fmt.i_cat].removed++;
    }

    static const enum es_format_category_e cats[] = {
        VIDEO_ES, AUDIO_ES, SPU_ES,
    };
    for (size_t i = 0; i < ARRAY_SIZE(cats); ++i)
    {
        enum es_format_category_e cat = cats[i];

        assert(tracks[cat].added == tracks[cat].removed);

        /* The next check doesn't work if we selected new programs and started
         * more than one time */
        assert(ctx->program_switch_count == 1 || ctx->extra_start_count == 0);

        const size_t track_count =
            ctx->params.track_count[cat] * ctx->program_switch_count *
            (ctx->played_medias.size + ctx->extra_start_count);
        assert(tracks[cat].added == track_count);
    }
}

static inline void
test_end_poststop_programs(struct ctx *ctx)
{
    vec_on_program_list_changed *vec = &ctx->report.on_program_list_changed;
    size_t program_added = 0, program_removed = 0;
    struct report_program_list report;
    vlc_vector_foreach(report, vec)
    {
        if (report.action == VLC_PLAYER_LIST_ADDED)
            program_added++;
        else if (report.action == VLC_PLAYER_LIST_REMOVED)
            program_removed++;
    }

    assert(program_added == program_removed);
    size_t program_count = ctx->params.program_count *
        (ctx->played_medias.size + ctx->extra_start_count);
    assert(program_added == program_count);
}

static inline void
test_end_poststop_titles(struct ctx *ctx)
{
    if (!ctx->params.chapter_count && !ctx->params.title_count)
        return;

    vec_on_titles_changed *vec = &ctx->report.on_titles_changed;
    assert(vec->size == 2);
    assert(vec->data[0] != NULL);
    assert(vec->data[1] == NULL);

    vlc_player_title_list *titles = vec->data[0];
    size_t title_count = vlc_player_title_list_GetCount(titles);
    assert(title_count == ctx->params.title_count);

    for (size_t title_idx = 0; title_idx < title_count; ++title_idx)
    {
        const struct vlc_player_title *title =
            vlc_player_title_list_GetAt(titles, title_idx);
        assert(title);
        assert(title->name && title->name[strlen(title->name)] == 0);
        assert(title->chapter_count == ctx->params.chapter_count);
        assert(title->length == ctx->params.length);

        for (size_t chapter_idx = 0; chapter_idx < title->chapter_count;
             ++chapter_idx)
        {
            const struct vlc_player_chapter *chapter =
                &title->chapters[chapter_idx];
            assert(chapter->name && chapter->name[strlen(chapter->name)] == 0);
            assert(chapter->time < ctx->params.length);
            if (chapter_idx != 0)
                assert(chapter->time > 0);
        }
    }
}

static inline void
test_end_poststop_vouts(struct ctx *ctx)
{
    vec_on_vout_changed *vec = &ctx->report.on_vout_changed;

    size_t vout_started = 0, vout_stopped = 0;

    struct report_vout report;
    vlc_vector_foreach(report, vec)
    {
        if (report.action == VLC_PLAYER_VOUT_STARTED)
            vout_started++;
        else if (report.action == VLC_PLAYER_VOUT_STOPPED)
            vout_stopped++;
        else
            vlc_assert_unreachable();
    }
    assert(vout_started == vout_stopped);
}

static inline void
test_end_poststop_medias(struct ctx *ctx)
{
    vlc_player_t *player = ctx->player;
    vec_on_current_media_changed *vec = &ctx->report.on_current_media_changed;

    assert(vec->size > 0);
    assert(vlc_player_GetCurrentMedia(player) != NULL);
    assert(VEC_LAST(vec) == vlc_player_GetCurrentMedia(player));
    const size_t oldsize = vec->size;

    player_set_current_mock_media(ctx, NULL, NULL, false);

    while (vec->size == oldsize)
        vlc_player_CondWait(player, &ctx->wait);

    assert(vec->size == ctx->added_medias.size);
    for (size_t i  = 0; i < vec->size; ++i)
        assert(vec->data[i] == ctx->added_medias.data[i]);

    assert(VEC_LAST(vec) == NULL);
    assert(vlc_player_GetCurrentMedia(player) == NULL);
}

static inline void
test_end_poststop_capabilities(struct ctx *ctx)
{
    vlc_player_t *player = ctx->player;
    vec_on_capabilities_changed *vec = &ctx->report.on_capabilities_changed;
    int new_caps = VEC_LAST(vec).new_caps;
    assert(vlc_player_CanSeek(player) == !!(new_caps & VLC_PLAYER_CAP_SEEK));
    assert(vlc_player_CanPause(player) == !!(new_caps & VLC_PLAYER_CAP_PAUSE));
}

static inline void
test_prestop(struct ctx *ctx)
{
    test_end_prestop_rate(ctx);
    test_end_prestop_length(ctx);
    test_end_prestop_capabilities(ctx);
    test_end_prestop_buffering(ctx);
}

static inline void
test_end(struct ctx *ctx)
{
    vlc_player_t *player = ctx->player;

    /* Don't wait if we already stopped or waited for a stop */
    const bool wait_stopped =
        VEC_LAST(&ctx->report.on_state_changed) != VLC_PLAYER_STATE_STOPPED;
    /* Can be no-op */
    vlc_player_Stop(player);
    assert(vlc_player_GetCurrentMedia(player) != NULL);
    if (wait_stopped)
        wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    if (!ctx->params.error)
    {
        test_end_poststop_state(ctx);
        test_end_poststop_tracks(ctx);
        test_end_poststop_programs(ctx);
        test_end_poststop_titles(ctx);
        test_end_poststop_vouts(ctx);
    }
    test_end_poststop_capabilities(ctx);
    test_end_poststop_medias(ctx);

    player_set_rate(ctx, 1.0f);
    vlc_player_SetStartPaused(player, false);
    vlc_player_SetPlayAndPause(player, false);
    vlc_player_SetRepeatCount(player, 0);

    ctx_reset(ctx);
}

static inline size_t
vec_on_program_list_get_action_count(vec_on_program_list_changed *vec,
                                    enum vlc_player_list_action action)
{
    size_t count = 0;
    struct report_program_list report;
    vlc_vector_foreach(report, vec)
        if (report.action == action)
            count++;
    return count;
}

static inline size_t
vec_on_program_selection_has_event(vec_on_program_selection_changed *vec,
                                   size_t from_idx,
                                   int unselected_id, int selected_id)
{
    assert(vec->size >= from_idx);
    bool has_unselected_id = false, has_selected_id = false;
    for (size_t i = from_idx; i < vec->size; ++i)
    {
        struct report_program_selection report = vec->data[i];
        if (unselected_id != -1 && report.unselected_id == unselected_id)
        {
            assert(!has_unselected_id);
            has_unselected_id = true;
        }
        if (selected_id != -1 && report.selected_id == selected_id)
        {
            assert(!has_selected_id);
            has_selected_id = true;
        }
    }
    if (unselected_id != -1 && selected_id != -1)
        return has_unselected_id && has_selected_id;
    else if (unselected_id)
    {
        assert(!has_selected_id);
        return has_unselected_id;
    }
    else
    {
        assert(selected_id != -1);
        assert(!has_unselected_id);
        return has_selected_id;
    }
}

static inline size_t
vec_on_track_list_get_action_count(vec_on_track_list_changed *vec,
                                   enum vlc_player_list_action action)
{
    size_t count = 0;
    struct report_track_list report;
    vlc_vector_foreach(report, vec)
        if (report.action == action)
            count++;
    return count;
}

static inline bool
vec_on_track_selection_has_event(vec_on_track_selection_changed *vec,
                                 size_t from_idx, vlc_es_id_t *unselected_id,
                                 vlc_es_id_t *selected_id)
{
    assert(vec->size >= from_idx);
    bool has_unselected_id = false, has_selected_id = false;
    for (size_t i = from_idx; i < vec->size; ++i)
    {
        struct report_track_selection report = vec->data[i];
        if (unselected_id && report.unselected_id == unselected_id)
        {
            assert(!has_unselected_id);
            has_unselected_id = true;
        }
        if (selected_id && report.selected_id == selected_id)
        {
            assert(!has_selected_id);
            has_selected_id = true;
        }
    }
    if (unselected_id && selected_id)
        return has_unselected_id && has_selected_id;
    else if (unselected_id)
    {
        assert(!has_selected_id);
        return has_unselected_id;
    }
    else
    {
        assert(selected_id);
        assert(!has_unselected_id);
        return has_selected_id;
    }
}

static inline void
ctx_destroy(struct ctx *ctx)
{
#define X(type, name) vlc_vector_destroy(&ctx->report.name);
PLAYER_REPORT_LIST
REPORT_LIST
#undef X
    vlc_player_RemoveListener(ctx->player, ctx->listener);
    vlc_player_Unlock(ctx->player);
    vlc_player_Delete(ctx->player);

    libvlc_release(ctx->vlc);
}

static inline void
ctx_init(struct ctx *ctx, int flags)
{
    static bool test_initialized = false;
    if (!test_initialized)
    {
        test_init();
        test_initialized = true;
    }

    const char * argv[] = {
        "-v",
        "--ignore-config",
        "-Idummy",
        "--no-media-library",
        "--no-drop-late-frames",
        /* Avoid leaks from various dlopen... */
        "--codec=araw,rawvideo,subsdec,"TELETEXT_DECODER"none",
        "--dec-dev=none",
        (flags & DISABLE_VIDEO_OUTPUT) ? "--vout=none" : "--vout=dummy,none",
        (flags & DISABLE_AUDIO_OUTPUT) ? "--aout=none" : "--aout=test_src_player,none",
        (flags & DISABLE_VIDEO) ? "--no-video" : "--video",
        (flags & DISABLE_AUDIO) ? "--no-audio" : "--audio",
        "--text-renderer=tdummy,none",
        (flags & CLOCK_MASTER_MONOTONIC) ?
            "--clock-master=monotonic" : "--clock-master=auto",
    };
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(argv), argv);
    assert(vlc);

#define X(type, name) .name = player_##name,
    static const struct vlc_player_cbs cbs = {
PLAYER_REPORT_LIST
    };
#undef X

    *ctx = (struct ctx) {
        .flags = flags,
        .vlc = vlc,
        .next_medias = VLC_VECTOR_INITIALIZER,
        .added_medias = VLC_VECTOR_INITIALIZER,
        .played_medias = VLC_VECTOR_INITIALIZER,
        .program_switch_count = 1,
        .extra_start_count = 0,
        .rate = 1.f,
    };
    vlc_cond_init(&ctx->wait);
    reports_init(&ctx->report);

    /* Force wdummy window */
    int ret = var_SetString(vlc->p_libvlc_int, "window", "wdummy");
    assert(ret == VLC_SUCCESS);

    ret = var_Create(vlc->p_libvlc_int, "test-ctx", VLC_VAR_ADDRESS);
    assert(ret == VLC_SUCCESS);
    ret = var_SetAddress(vlc->p_libvlc_int, "test-ctx", ctx);
    assert(ret == VLC_SUCCESS);

    ctx->player = vlc_player_New(VLC_OBJECT(vlc->p_libvlc_int),
                                 VLC_PLAYER_LOCK_NORMAL);
    assert(ctx->player);

    vlc_player_Lock(ctx->player);
    ctx->listener = vlc_player_AddListener(ctx->player, &cbs, ctx);
    assert(ctx->listener);
}