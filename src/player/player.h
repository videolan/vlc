/*****************************************************************************
 * player.h: Player internal interface
 *****************************************************************************
 * Copyright © 2018-2019 VLC authors and VideoLAN
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

#ifndef VLC_PLAYER_INTERNAL_H
#define VLC_PLAYER_INTERNAL_H

#include <vlc_player.h>
#include <vlc_list.h>
#include <vlc_vector.h>
#include <vlc_atomic.h>
#include <vlc_media_library.h>

#include "input/input_internal.h"

struct vlc_player_track_priv
{
    struct vlc_player_track t;
    vout_thread_t *vout; /* weak reference */
    vlc_tick_t delay;
    /* only valid if selected and if category is VIDEO_ES or SPU_ES */
    enum vlc_vout_order vout_order;
    /* Used to save or not the track selection */
    bool selected_by_user;
};

typedef struct VLC_VECTOR(struct vlc_player_program *)
    vlc_player_program_vector;

typedef struct VLC_VECTOR(struct vlc_player_track_priv *)
    vlc_player_track_vector;

struct vlc_player_title_list
{
    vlc_atomic_rc_t rc;
    size_t count;
    struct vlc_player_title array[];
};

struct vlc_player_input
{
    input_thread_t *thread;
    vlc_player_t *player;
    bool started;

    enum vlc_player_state state;
    enum vlc_player_error error;
    float rate;
    int capabilities;
    vlc_tick_t length;

    float position;
    vlc_tick_t time;
    vlc_tick_t normal_time;

    vlc_tick_t pause_date;

    bool recording;

    float signal_quality;
    float signal_strength;
    float cache;

    struct input_stats_t stats;

    vlc_tick_t cat_delays[DATA_ES];

    vlc_player_program_vector program_vector;
    vlc_player_track_vector video_track_vector;
    vlc_player_track_vector audio_track_vector;
    vlc_player_track_vector spu_track_vector;
    const struct vlc_player_track_priv *teletext_source;

    struct vlc_player_title_list *titles;

    size_t title_selected;
    size_t chapter_selected;

    struct vlc_list node;

    bool teletext_enabled;
    bool teletext_transparent;
    unsigned teletext_page;

    struct
    {
        vlc_tick_t time;
        float pos;
        bool set;
    } abloop_state[2];

    struct
    {
        vlc_ml_playback_states_all states;
        enum
        {
            VLC_RESTOREPOINT_TITLE,
            VLC_RESTOREPOINT_POSITION,
            VLC_RESTOREPOINT_NONE,
        } restore;
        bool restore_states;
        bool delay_restore;
    } ml;
};

struct vlc_player_listener_id
{
    const struct vlc_player_cbs *cbs;
    void *cbs_data;
    struct vlc_list node;
};

struct vlc_player_vout_listener_id
{
    const struct vlc_player_vout_cbs *cbs;
    void *cbs_data;
    struct vlc_list node;
};

struct vlc_player_aout_listener_id
{
    const struct vlc_player_aout_cbs *cbs;
    void *cbs_data;
    struct vlc_list node;
};

enum vlc_player_timer_source_type
{
    VLC_PLAYER_TIMER_TYPE_BEST,
    VLC_PLAYER_TIMER_TYPE_SMPTE,
    VLC_PLAYER_TIMER_TYPE_COUNT
};

struct vlc_player_timer_id
{
    vlc_tick_t period;
    vlc_tick_t last_update_date;

    union
    {
        const struct vlc_player_timer_cbs *cbs;
        const struct vlc_player_timer_smpte_cbs *smpte_cbs;
    };
    void *data;

    struct vlc_list node;
};

struct vlc_player_timer_source
{
    struct vlc_list listeners; /* list of struct vlc_player_timer_id */
    vlc_es_id_t *es; /* weak reference */
    struct vlc_player_timer_point point;
    union
    {
        struct {
            unsigned long last_framenum;
            unsigned frame_rate;
            unsigned frame_rate_base;
            unsigned frame_resolution;
            unsigned df_fps;
            int df;
            int frames_per_10mins;
        } smpte;
    };
};

enum vlc_player_timer_state
{
    VLC_PLAYER_TIMER_STATE_PLAYING,
    VLC_PLAYER_TIMER_STATE_PAUSED,
    VLC_PLAYER_TIMER_STATE_DISCONTINUITY,
};

struct vlc_player_timer
{
    vlc_mutex_t lock;

    enum vlc_player_timer_state state;
    bool seeking;

    vlc_tick_t input_length;
    vlc_tick_t input_normal_time;
    vlc_tick_t last_ts;
    float input_position;

    struct vlc_player_timer_source sources[VLC_PLAYER_TIMER_TYPE_COUNT];
#define best_source sources[VLC_PLAYER_TIMER_TYPE_BEST]
#define smpte_source sources[VLC_PLAYER_TIMER_TYPE_SMPTE]
};

struct vlc_player_t
{
    struct vlc_object_t obj;
    vlc_mutex_t lock;
    vlc_mutex_t aout_listeners_lock;
    vlc_mutex_t vout_listeners_lock;
    vlc_cond_t start_delay_cond;

    enum vlc_player_media_stopped_action media_stopped_action;
    bool start_paused;

    const struct vlc_player_media_provider *media_provider;
    void *media_provider_data;

    bool pause_on_cork;
    bool corked;

    struct vlc_list listeners;
    struct vlc_list aout_listeners;
    struct vlc_list vout_listeners;

    input_resource_t *resource;
    vlc_renderer_item_t *renderer;

    input_item_t *media;
    struct vlc_player_input *input;

    bool releasing_media;
    bool next_media_requested;
    input_item_t *next_media;

    char *video_string_ids;
    char *audio_string_ids;
    char *sub_string_ids;

    enum vlc_player_state global_state;
    bool started;

    unsigned error_count;

    bool deleting;
    struct
    {
        vlc_thread_t thread;
        vlc_cond_t wait;
        vlc_cond_t notify;
        struct vlc_list inputs;
        struct vlc_list stopping_inputs;
        struct vlc_list joinable_inputs;
    } destructor;

    struct vlc_player_timer timer;
};

#ifndef NDEBUG
/*
 * Assert that the player mutex is locked.
 *
 * This is exposed in this internal header because the playlist and its
 * associated player share the lock to avoid lock-order inversion issues.
 */
static inline void
vlc_player_assert_locked(vlc_player_t *player)
{
    assert(player);
    vlc_mutex_assert(&player->lock);
}
#else
#define vlc_player_assert_locked(x) ((void) (0))
#endif

static inline struct vlc_player_input *
vlc_player_get_input_locked(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    return player->input;
}

#define vlc_player_SendEvent(player, event, ...) do { \
    vlc_player_listener_id *listener; \
    vlc_list_foreach(listener, &player->listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(player, ##__VA_ARGS__, listener->cbs_data); \
    } \
} while(0)

static inline const char *
es_format_category_to_string(enum es_format_category_e cat)
{
    switch (cat)
    {
        case VIDEO_ES: return "Video";
        case AUDIO_ES: return "Audio";
        case SPU_ES: return "Subtitle";
        default: return NULL;
    }
}

/*
 * player.c
 */

vlc_object_t *
vlc_player_GetObject(vlc_player_t *player);

int
vlc_player_OpenNextMedia(vlc_player_t *player);

void
vlc_player_PrepareNextMedia(vlc_player_t *player);

void
vlc_player_destructor_AddStoppingInput(vlc_player_t *player,
                                       struct vlc_player_input *input);

void
vlc_player_destructor_AddJoinableInput(vlc_player_t *player,
                                       struct vlc_player_input *input);

/*
 * player_track.c
 */

struct vlc_player_program *
vlc_player_program_New(int id, const char *name);

int
vlc_player_program_Update(struct vlc_player_program *prgm, int id,
                          const char *name);

struct vlc_player_program *
vlc_player_program_vector_FindById(vlc_player_program_vector *vec, int id,
                                   size_t *idx);

struct vlc_player_track_priv *
vlc_player_track_priv_New(vlc_es_id_t *id, const char *name, const es_format_t *fmt);

void
vlc_player_track_priv_Delete(struct vlc_player_track_priv *trackpriv);

int
vlc_player_track_priv_Update(struct vlc_player_track_priv *trackpriv,
                             const char *name, const es_format_t *fmt);

struct vlc_player_track_priv *
vlc_player_track_vector_FindById(vlc_player_track_vector *vec, vlc_es_id_t *id,
                                 size_t *idx);

int
vlc_player_GetFirstSelectedTrackId(const vlc_player_track_vector* tracks);

/*
 * player_title.c
 */

struct vlc_player_title_list *
vlc_player_title_list_Create(input_title_t *const *array, size_t count,
                             int title_offset, int chapter_offset);

/*
 * player_input.c
 */

static inline vlc_player_track_vector *
vlc_player_input_GetTrackVector(struct vlc_player_input *input,
                                enum es_format_category_e cat)
{
    switch (cat)
    {
        case VIDEO_ES:
            return &input->video_track_vector;
        case AUDIO_ES:
            return &input->audio_track_vector;
        case SPU_ES:
            return &input->spu_track_vector;
        default:
            return NULL;
    }
}

struct vlc_player_track_priv *
vlc_player_input_FindTrackById(struct vlc_player_input *input, vlc_es_id_t *id,
                               size_t *idx);

struct vlc_player_input *
vlc_player_input_New(vlc_player_t *player, input_item_t *item);

void
vlc_player_input_Delete(struct vlc_player_input *input);

void
vlc_player_input_SelectTracksByStringIds(struct vlc_player_input *input,
                                         enum es_format_category_e cat,
                                         const char *str_ids);

char *
vlc_player_input_GetSelectedTrackStringIds(struct vlc_player_input *input,
                                           enum es_format_category_e cat);

vlc_tick_t
vlc_player_input_GetTime(struct vlc_player_input *input);

float
vlc_player_input_GetPos(struct vlc_player_input *input);

int
vlc_player_input_Start(struct vlc_player_input *input);

void
vlc_player_input_HandleState(struct vlc_player_input *, enum vlc_player_state,
                             vlc_tick_t state_date);

struct vlc_player_timer_point
vlc_player_input_GetTimerValue(struct vlc_player_input *input);

/*
 * player_timer.c
*/

void
vlc_player_InitTimer(vlc_player_t *player);

void
vlc_player_DestroyTimer(vlc_player_t *player);

void
vlc_player_ResetTimer(vlc_player_t *player);

void
vlc_player_UpdateTimerState(vlc_player_t *player, vlc_es_id_t *es_source,
                            enum vlc_player_timer_state state,
                            vlc_tick_t system_date);

void
vlc_player_UpdateTimer(vlc_player_t *player, vlc_es_id_t *es_source,
                       bool es_source_is_master,
                       const struct vlc_player_timer_point *point,
                       vlc_tick_t normal_time,
                       unsigned frame_rate, unsigned frame_rate_base);

void
vlc_player_RemoveTimerSource(vlc_player_t *player, vlc_es_id_t *es_source);

int
vlc_player_GetTimerPoint(vlc_player_t *player, vlc_tick_t system_now,
                         vlc_tick_t *out_ts, float *out_pos);

/*
 * player_vout.c
 */

void
vlc_player_vout_AddCallbacks(vlc_player_t *player, vout_thread_t *vout);

void
vlc_player_vout_DelCallbacks(vlc_player_t *player, vout_thread_t *vout);

/*
 * player_aout.c
 */

audio_output_t *
vlc_player_aout_Init(vlc_player_t *player);

void
vlc_player_aout_Deinit(vlc_player_t *player);

/*
 * player_osd.c
 */

void
vlc_player_osd_Message(vlc_player_t *player, const char *fmt, ...);

void
vlc_player_osd_Icon(vlc_player_t *player, short type);

void
vlc_player_osd_Position(vlc_player_t *player,
                        struct vlc_player_input *input, vlc_tick_t time,
                        float position, enum vlc_player_whence whence);
void
vlc_player_osd_Volume(vlc_player_t *player, bool mute_action);

int
vlc_player_vout_OSDCallback(vlc_object_t *this, const char *var,
                            vlc_value_t oldval, vlc_value_t newval, void *data);

void
vlc_player_osd_Track(vlc_player_t *player, vlc_es_id_t *id, bool select);

void
vlc_player_osd_Program(vlc_player_t *player, const char *name);

/*
 * player/medialib.c
 */

void
vlc_player_input_RestoreMlStates(struct vlc_player_input* input, bool force_pos);

void
vlc_player_UpdateMLStates(vlc_player_t *player, struct vlc_player_input* input);

#endif
