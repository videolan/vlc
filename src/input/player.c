/*****************************************************************************
 * player.c: Player interface
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include "player.h"
#include <vlc_aout.h>
#include <vlc_interface.h>
#include <vlc_renderer_discovery.h>
#include <vlc_list.h>
#include <vlc_vector.h>
#include <vlc_atomic.h>
#include <vlc_tick.h>

#include "libvlc.h"
#include "input_internal.h"
#include "resource.h"
#include "../audio_output/aout_internal.h"

#define RETRY_TIMEOUT_BASE VLC_TICK_FROM_MS(100)
#define RETRY_TIMEOUT_MAX VLC_TICK_FROM_MS(3200)

static_assert(VLC_PLAYER_CAP_SEEK == VLC_INPUT_CAPABILITIES_SEEKABLE &&
              VLC_PLAYER_CAP_PAUSE == VLC_INPUT_CAPABILITIES_PAUSEABLE &&
              VLC_PLAYER_CAP_CHANGE_RATE == VLC_INPUT_CAPABILITIES_CHANGE_RATE &&
              VLC_PLAYER_CAP_REWIND == VLC_INPUT_CAPABILITIES_REWINDABLE,
              "player/input capabilities mismatch");

static_assert(VLC_PLAYER_TITLE_MENU == INPUT_TITLE_MENU &&
              VLC_PLAYER_TITLE_INTERACTIVE == INPUT_TITLE_INTERACTIVE,
              "player/input title flag mismatch");

typedef struct VLC_VECTOR(struct vlc_player_program *)
    vlc_player_program_vector;

typedef struct VLC_VECTOR(struct vlc_player_track *)
    vlc_player_track_vector;

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

    vlc_tick_t time;
    float position;

    bool recording;

    float signal_quality;
    float signal_strength;
    float cache;

    struct input_stats_t stats;

    vlc_tick_t audio_delay;
    vlc_tick_t subtitle_delay;

    struct
    {
        vlc_tick_t audio_time;
        vlc_tick_t subtitle_time;
    } subsync;

    vlc_player_program_vector program_vector;
    vlc_player_track_vector video_track_vector;
    vlc_player_track_vector audio_track_vector;
    vlc_player_track_vector spu_track_vector;
    struct vlc_player_track *teletext_menu;

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
};

struct vlc_player_t
{
    struct vlc_common_members obj;
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
};

#define vlc_player_SendEvent(player, event, ...) do { \
    vlc_player_listener_id *listener; \
    vlc_list_foreach(listener, &player->listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(player, ##__VA_ARGS__, listener->cbs_data); \
    } \
} while(0)

#define vlc_player_aout_SendEvent(player, event, ...) do { \
    vlc_mutex_lock(&player->aout_listeners_lock); \
    vlc_player_aout_listener_id *listener; \
    vlc_list_foreach(listener, &player->aout_listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(player, ##__VA_ARGS__, listener->cbs_data); \
    } \
    vlc_mutex_unlock(&player->aout_listeners_lock); \
} while(0)

#define vlc_player_vout_SendEvent(player, event, ...) do { \
    vlc_mutex_lock(&player->vout_listeners_lock); \
    vlc_player_vout_listener_id *listener; \
    vlc_list_foreach(listener, &player->vout_listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(player, ##__VA_ARGS__, listener->cbs_data); \
    } \
    vlc_mutex_unlock(&player->vout_listeners_lock); \
} while(0)

#define vlc_player_foreach_inputs(it) \
    for (struct vlc_player_input *it = player->input; it != NULL; it = NULL)

static void
input_thread_Events(input_thread_t *, const struct vlc_input_event *, void *);
static void
vlc_player_input_HandleState(struct vlc_player_input *, enum vlc_player_state);
static int
vlc_player_VoutCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data);
static int
vlc_player_VoutOSDCallback(vlc_object_t *this, const char *var,
                           vlc_value_t oldval, vlc_value_t newval, void *data);

void
vlc_player_assert_locked(vlc_player_t *player)
{
    assert(player);
    vlc_mutex_assert(&player->lock);
}

static inline struct vlc_player_input *
vlc_player_get_input_locked(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    return player->input;
}

static vout_thread_t **
vlc_player_vout_OSDHoldAll(vlc_player_t *player, size_t *count)
{
    vout_thread_t **vouts = vlc_player_vout_HoldAll(player, count);

    for (size_t i = 0; i < *count; ++i)
    {
        vout_FlushSubpictureChannel(vouts[i], VOUT_SPU_CHANNEL_OSD);
        vout_FlushSubpictureChannel(vouts[i], VOUT_SPU_CHANNEL_OSD_HSLIDER);
        vout_FlushSubpictureChannel(vouts[i], VOUT_SPU_CHANNEL_OSD_HSLIDER);
    }
    return vouts;
}

static void
vlc_player_vout_OSDReleaseAll(vlc_player_t *player, vout_thread_t **vouts,
                            size_t count)
{
    for (size_t i = 0; i < count; ++i)
        vout_Release(vouts[i]);
    free(vouts);
    (void) player;
}

static inline void
vouts_osd_Message(vout_thread_t **vouts, size_t count, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    for (size_t i = 0; i < count; ++i)
        vout_OSDMessageVa(vouts[i], VOUT_SPU_CHANNEL_OSD, fmt, args);
    va_end(args);
}

static inline void
vouts_osd_Icon(vout_thread_t **vouts, size_t count, short type)
{
    for (size_t i = 0; i < count; ++i)
        vout_OSDIcon(vouts[i], VOUT_SPU_CHANNEL_OSD, type);
}

static inline void
vouts_osd_Slider(vout_thread_t **vouts, size_t count, int position, short type)
{
    int channel = type == OSD_HOR_SLIDER ?
        VOUT_SPU_CHANNEL_OSD_HSLIDER : VOUT_SPU_CHANNEL_OSD_VSLIDER;
    for (size_t i = 0; i < count; ++i)
        vout_OSDSlider(vouts[i], channel, position, type);
}

void
vlc_player_vout_OSDMessage(vlc_player_t *player, const char *fmt, ...)
{
    size_t count;
    vout_thread_t **vouts = vlc_player_vout_OSDHoldAll(player, &count);

    va_list args;
    va_start(args, fmt);
    for (size_t i = 0; i < count; ++i)
        vout_OSDMessageVa(vouts[i], VOUT_SPU_CHANNEL_OSD, fmt, args);
    va_end(args);

    vlc_player_vout_OSDReleaseAll(player, vouts, count);
}

static void
vlc_player_vout_OSDIcon(vlc_player_t *player, short type)
{
    size_t count;
    vout_thread_t **vouts = vlc_player_vout_OSDHoldAll(player, &count);

    vouts_osd_Icon(vouts, count, type);

    vlc_player_vout_OSDReleaseAll(player, vouts, count);
}

static char *
vlc_player_program_DupTitle(int id, const char *title)
{
    char *dup;
    if (title)
        dup = strdup(title);
    else if (asprintf(&dup, "%d", id) == -1)
        dup = NULL;
    return dup;
}

static struct vlc_player_program *
vlc_player_program_New(int id, const char *name)
{
    struct vlc_player_program *prgm = malloc(sizeof(*prgm));
    if (!prgm)
        return NULL;
    prgm->name = vlc_player_program_DupTitle(id, name);
    if (!prgm->name)
    {
        free(prgm);
        return NULL;
    }
    prgm->group_id = id;
    prgm->selected = prgm->scrambled = false;

    return prgm;
}

static int
vlc_player_program_Update(struct vlc_player_program *prgm, int id,
                          const char *name)
{
    free((char *)prgm->name);
    prgm->name = vlc_player_program_DupTitle(id, name);
    return prgm->name != NULL ? VLC_SUCCESS : VLC_ENOMEM;
}

struct vlc_player_program *
vlc_player_program_Dup(const struct vlc_player_program *src)
{
    struct vlc_player_program *dup =
        vlc_player_program_New(src->group_id, src->name);

    if (!dup)
        return NULL;
    dup->selected = src->selected;
    dup->scrambled = src->scrambled;
    return dup;
}

void
vlc_player_program_Delete(struct vlc_player_program *prgm)
{
    free((char *)prgm->name);
    free(prgm);
}

static struct vlc_player_program *
vlc_player_program_vector_FindById(vlc_player_program_vector *vec, int id,
                                   size_t *idx)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct vlc_player_program *prgm = vec->data[i];
        if (prgm->group_id == id)
        {
            if (idx)
                *idx = i;
            return prgm;
        }
    }
    return NULL;
}

static struct vlc_player_track *
vlc_player_track_New(vlc_es_id_t *id, const char *name, const es_format_t *fmt)
{
    struct vlc_player_track *track = malloc(sizeof(*track));
    if (!track)
        return NULL;
    track->name = strdup(name);
    if (!track->name)
    {
        free(track);
        return NULL;
    }

    int ret = es_format_Copy(&track->fmt, fmt);
    if (ret != VLC_SUCCESS)
    {
        free((char *)track->name);
        free(track);
        return NULL;
    }
    track->es_id = vlc_es_id_Hold(id);
    track->selected = false;

    return track;
}

struct vlc_player_track *
vlc_player_track_Dup(const struct vlc_player_track *src)
{
    struct vlc_player_track *dup =
        vlc_player_track_New(src->es_id, src->name, &src->fmt);

    if (!dup)
        return NULL;
    dup->selected = src->selected;
    return dup;
}

void
vlc_player_track_Delete(struct vlc_player_track *track)
{
    es_format_Clean(&track->fmt);
    free((char *)track->name);
    vlc_es_id_Release(track->es_id);
    free(track);
}

static int
vlc_player_track_Update(struct vlc_player_track *track,
                        const char *name, const es_format_t *fmt)
{
    if (strcmp(name, track->name) != 0)
    {
        char *dup = strdup(name);
        if (!dup)
            return VLC_ENOMEM;
        free((char *)track->name);
        track->name = dup;
    }

    es_format_t fmtdup;
    int ret = es_format_Copy(&fmtdup, fmt);
    if (ret != VLC_SUCCESS)
        return ret;

    es_format_Clean(&track->fmt);
    track->fmt = fmtdup;
    return VLC_SUCCESS;
}

struct vlc_player_title_list *
vlc_player_title_list_Hold(struct vlc_player_title_list *titles)
{
    vlc_atomic_rc_inc(&titles->rc);
    return titles;
}

void
vlc_player_title_list_Release(struct vlc_player_title_list *titles)
{
    if (!vlc_atomic_rc_dec(&titles->rc))
        return;
    for (size_t title_idx = 0; title_idx < titles->count; ++title_idx)
    {
        struct vlc_player_title *title = &titles->array[title_idx];
        free((char *)title->name);
        for (size_t chapter_idx = 0; chapter_idx < title->chapter_count;
             ++chapter_idx)
        {
            const struct vlc_player_chapter *chapter =
                &title->chapters[chapter_idx];
            free((char *)chapter->name);
        }
        free((void *)title->chapters);
    }
    free(titles);
}

static char *
input_title_GetName(const struct input_title_t *input_title, int idx,
                    int title_offset)
{
    int ret;
    char length_str[MSTRTIME_MAX_SIZE + sizeof(" []")];

    if (input_title->i_length > 0)
    {
        strcpy(length_str, " [");
        secstotimestr(&length_str[2], SEC_FROM_VLC_TICK(input_title->i_length));
        strcat(length_str, "]");
    }
    else
        length_str[0] = '\0';

    char *dup;
    if (input_title->psz_name && input_title->psz_name[0] != '\0')
        ret = asprintf(&dup, "%s%s", input_title->psz_name, length_str);
    else
        ret = asprintf(&dup, _("Title %i%s"), idx + title_offset, length_str);
    if (ret == -1)
        return NULL;
    return dup;
}

static char *
seekpoint_GetName(seekpoint_t *seekpoint, int idx, int chapter_offset)
{
    if (seekpoint->psz_name && seekpoint->psz_name[0] != '\0' )
        return strdup(seekpoint->psz_name);

    char *dup;
    int ret = asprintf(&dup, _("Chapter %i"), idx + chapter_offset);
    if (ret == -1)
        return NULL;
    return dup;
}

static struct vlc_player_title_list *
vlc_player_title_list_Create(input_title_t *const *array, size_t count,
                             int title_offset, int chapter_offset)
{
    if (count == 0)
        return NULL;

    /* Allocate the struct + the whole list */
    size_t size;
    if (mul_overflow(count, sizeof(struct vlc_player_title), &size))
        return NULL;
    if (add_overflow(size, sizeof(struct vlc_player_title_list), &size))
        return NULL;
    struct vlc_player_title_list *titles = malloc(size);
    if (!titles)
        return NULL;

    vlc_atomic_rc_init(&titles->rc);
    titles->count = count;

    for (size_t title_idx = 0; title_idx < titles->count; ++title_idx)
    {
        const struct input_title_t *input_title = array[title_idx];
        struct vlc_player_title *title = &titles->array[title_idx];

        title->name = input_title_GetName(input_title, title_idx, title_offset);
        title->length = input_title->i_length;
        title->flags = input_title->i_flags;
        const size_t seekpoint_count = input_title->i_seekpoint > 0 ?
                                       input_title->i_seekpoint : 0;
        title->chapter_count = seekpoint_count;

        struct vlc_player_chapter *chapters = title->chapter_count == 0 ? NULL :
            vlc_alloc(title->chapter_count, sizeof(*chapters));

        if (chapters)
        {
            for (size_t chapter_idx = 0; chapter_idx < title->chapter_count;
                 ++chapter_idx)
            {
                struct vlc_player_chapter *chapter = &chapters[chapter_idx];
                seekpoint_t *seekpoint = input_title->seekpoint[chapter_idx];

                chapter->name = seekpoint_GetName(seekpoint, chapter_idx,
                                                  chapter_offset);
                chapter->time = seekpoint->i_time_offset;
                if (!chapter->name) /* Will trigger the error path */
                    title->chapter_count = chapter_idx;
            }
        }
        else if (seekpoint_count > 0) /* Will trigger the error path */
            title->chapter_count = 0;

        title->chapters = chapters;

        if (!title->name || seekpoint_count != title->chapter_count)
        {
            /* Release titles up to title_idx */
            titles->count = title_idx;
            vlc_player_title_list_Release(titles);
            return NULL;
        }
    }
    return titles;
}

const struct vlc_player_title *
vlc_player_title_list_GetAt(struct vlc_player_title_list *titles, size_t idx)
{
    assert(idx < titles->count);
    return &titles->array[idx];
}

size_t
vlc_player_title_list_GetCount(struct vlc_player_title_list *titles)
{
    return titles->count;
}

static struct vlc_player_input *
vlc_player_input_New(vlc_player_t *player, input_item_t *item)
{
    struct vlc_player_input *input = malloc(sizeof(*input));
    if (!input)
        return NULL;

    input->player = player;
    input->started = false;

    input->state = VLC_PLAYER_STATE_STOPPED;
    input->error = VLC_PLAYER_ERROR_NONE;
    input->rate = 1.f;
    input->capabilities = 0;
    input->length = input->time = VLC_TICK_INVALID;
    input->position = 0.f;

    input->recording = false;

    input->cache = 0.f;
    input->signal_quality = input->signal_strength = -1.f;

    memset(&input->stats, 0, sizeof(input->stats));

    input->audio_delay = input->subtitle_delay = 0;

    input->subsync.audio_time =
    input->subsync.subtitle_time = VLC_TICK_INVALID;

    vlc_vector_init(&input->program_vector);
    vlc_vector_init(&input->video_track_vector);
    vlc_vector_init(&input->audio_track_vector);
    vlc_vector_init(&input->spu_track_vector);
    input->teletext_menu = NULL;

    input->titles = NULL;
    input->title_selected = input->chapter_selected = 0;

    input->teletext_enabled = input->teletext_transparent = false;
    input->teletext_page = 0;

    input->abloop_state[0].set = input->abloop_state[1].set = false;

    input->thread = input_Create(player, input_thread_Events, input, item,
                                 player->resource, player->renderer);
    if (!input->thread)
    {
        free(input);
        return NULL;
    }
    return input;
}

static void
vlc_player_input_Delete(struct vlc_player_input *input)
{
    assert(input->titles == NULL);
    assert(input->program_vector.size == 0);
    assert(input->video_track_vector.size == 0);
    assert(input->audio_track_vector.size == 0);
    assert(input->spu_track_vector.size == 0);
    assert(input->teletext_menu == NULL);

    vlc_vector_destroy(&input->program_vector);
    vlc_vector_destroy(&input->video_track_vector);
    vlc_vector_destroy(&input->audio_track_vector);
    vlc_vector_destroy(&input->spu_track_vector);

    input_Close(input->thread);
    free(input);
}

static int
vlc_player_input_Start(struct vlc_player_input *input)
{
    int ret = input_Start(input->thread);
    if (ret != VLC_SUCCESS)
        return ret;
    input->started = true;
    return ret;
}

static void
vlc_player_PrepareNextMedia(vlc_player_t *player)
{
    vlc_player_assert_locked(player);

    if (!player->media_provider 
     || player->media_stopped_action != VLC_PLAYER_MEDIA_STOPPED_CONTINUE
     || player->next_media_requested)
        return;

    assert(player->next_media == NULL);
    player->next_media =
        player->media_provider->get_next(player, player->media_provider_data);
    player->next_media_requested = true;
}

static int
vlc_player_OpenNextMedia(vlc_player_t *player)
{
    assert(player->input == NULL);

    player->next_media_requested = false;

    int ret = VLC_SUCCESS;
    if (player->releasing_media)
    {
        assert(player->media);
        input_item_Release(player->media);
        player->media = NULL;
        player->releasing_media = false;
    }
    else
    {
        if (!player->next_media)
            return VLC_EGENERIC;

        if (player->media)
            input_item_Release(player->media);
        player->media = player->next_media;
        player->next_media = NULL;

        player->input = vlc_player_input_New(player, player->media);
        if (!player->input)
        {
            input_item_Release(player->media);
            player->media = NULL;
            ret = VLC_ENOMEM;
        }
    }
    vlc_player_SendEvent(player, on_current_media_changed, player->media);
    return ret;
}

static void
vlc_player_CancelWaitError(vlc_player_t *player)
{
    if (player->error_count != 0)
    {
        player->error_count = 0;
        vlc_cond_signal(&player->start_delay_cond);
    }
}

static bool
vlc_list_HasInput(struct vlc_list *list, struct vlc_player_input *input)
{
    struct vlc_player_input *other_input;
    vlc_list_foreach(other_input, list, node)
    {
        if (other_input == input)
            return true;
    }
    return false;
}

static void
vlc_player_destructor_AddInput(vlc_player_t *player,
                               struct vlc_player_input *input)
{
    if (input->started)
    {
        input->started = false;
        /* Add this input to the stop list: it will be stopped by the
         * destructor thread */
        assert(!vlc_list_HasInput(&player->destructor.stopping_inputs, input));
        assert(!vlc_list_HasInput(&player->destructor.joinable_inputs, input));
        vlc_list_append(&input->node, &player->destructor.inputs);
    }
    else
    {
        /* Add this input to the joinable list: it will be deleted by the
         * destructor thread */
        assert(!vlc_list_HasInput(&player->destructor.inputs, input));
        assert(!vlc_list_HasInput(&player->destructor.joinable_inputs, input));
        vlc_list_append(&input->node, &player->destructor.joinable_inputs);
    }

    vlc_cond_signal(&input->player->destructor.wait);
}

static void
vlc_player_destructor_AddStoppingInput(vlc_player_t *player,
                                       struct vlc_player_input *input)
{
    /* Add this input to the stopping list */
    if (vlc_list_HasInput(&player->destructor.inputs, input))
        vlc_list_remove(&input->node);
    if (!vlc_list_HasInput(&player->destructor.stopping_inputs, input))
    {
        vlc_list_append(&input->node, &player->destructor.stopping_inputs);
        vlc_cond_signal(&input->player->destructor.wait);
    }
}

static void
vlc_player_destructor_AddJoinableInput(vlc_player_t *player,
                                       struct vlc_player_input *input)
{
    if (vlc_list_HasInput(&player->destructor.stopping_inputs, input))
        vlc_list_remove(&input->node);

    assert(!input->started);
    vlc_player_destructor_AddInput(player, input);
}

static bool vlc_player_destructor_IsEmpty(vlc_player_t *player)
{
    return vlc_list_is_empty(&player->destructor.inputs)
        && vlc_list_is_empty(&player->destructor.stopping_inputs)
        && vlc_list_is_empty(&player->destructor.joinable_inputs);
}

static void *
vlc_player_destructor_Thread(void *data)
{
    vlc_player_t *player = data;

    vlc_mutex_lock(&player->lock);

    /* Terminate this thread when the player is deleting (vlc_player_Delete()
     * was called) and when all input_thread_t all stopped and released. */
    while (!player->deleting
        || !vlc_player_destructor_IsEmpty(player))
    {
        /* Wait for an input to stop or close. No while loop here since we want
         * to leave this code path when the player is deleting. */
        if (vlc_list_is_empty(&player->destructor.inputs)
         && vlc_list_is_empty(&player->destructor.joinable_inputs))
            vlc_cond_wait(&player->destructor.wait, &player->lock);

        struct vlc_player_input *input;
        vlc_list_foreach(input, &player->destructor.inputs, node)
        {
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING);
            vlc_player_destructor_AddStoppingInput(player, input);

            input_Stop(input->thread);
        }

        bool keep_sout = true;
        const bool inputs_changed =
            !vlc_list_is_empty(&player->destructor.joinable_inputs);
        vlc_list_foreach(input, &player->destructor.joinable_inputs, node)
        {
            keep_sout = var_GetBool(input->thread, "sout-keep");

            if (input->state == VLC_PLAYER_STATE_STOPPING)
                vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPED);

            vlc_list_remove(&input->node);
            vlc_player_input_Delete(input);
        }

        if (inputs_changed)
        {
            const bool started = player->started;
            vlc_player_Unlock(player);
            if (!started)
                input_resource_TerminateVout(player->resource);
            if (!keep_sout)
                input_resource_TerminateSout(player->resource);
            vlc_player_Lock(player);
        }
    }
    vlc_mutex_unlock(&player->lock);
    return NULL;
}

static bool
vlc_player_WaitRetryDelay(vlc_player_t *player)
{
    if (player->error_count)
    {
        /* Delay the next opening in case of error to avoid busy loops */
        vlc_tick_t delay = RETRY_TIMEOUT_BASE;
        for (unsigned i = 1; i < player->error_count
          && delay < RETRY_TIMEOUT_MAX; ++i)
            delay *= 2; /* Wait 100, 200, 400, 800, 1600 and finally 3200ms */
        delay += vlc_tick_now();

        while (player->error_count > 0
            && vlc_cond_timedwait(&player->start_delay_cond, &player->lock,
                                  delay) == 0);
        if (player->error_count == 0)
            return false; /* canceled */
    }
    return true;
}

static void
vlc_player_input_HandleState(struct vlc_player_input *input,
                             enum vlc_player_state state)
{
    vlc_player_t *player = input->player;

    /* The STOPPING state can be set earlier by the player. In that case,
     * ignore all future events except the STOPPED one */
    if (input->state == VLC_PLAYER_STATE_STOPPING
     && state != VLC_PLAYER_STATE_STOPPED)
        return;

    input->state = state;

    /* Override the global state if the player is still playing and has a next
     * media to play */
    bool send_event = player->global_state != state;
    switch (input->state)
    {
        case VLC_PLAYER_STATE_STOPPED:
            assert(!input->started);
            assert(input != player->input);

            if (input->titles)
            {
                vlc_player_title_list_Release(input->titles);
                input->titles = NULL;
                vlc_player_SendEvent(player, on_titles_changed, NULL);
            }

            if (input->error != VLC_PLAYER_ERROR_NONE)
                player->error_count++;
            else
                player->error_count = 0;

            vlc_player_WaitRetryDelay(player);

            if (!player->deleting)
                vlc_player_OpenNextMedia(player);
            if (!player->input)
                player->started = false;

            switch (player->media_stopped_action)
            {
                case VLC_PLAYER_MEDIA_STOPPED_EXIT:
                    if (player->input && player->started)
                        vlc_player_input_Start(player->input);
                    else
                        libvlc_Quit(vlc_object_instance(player));
                    break;
                case VLC_PLAYER_MEDIA_STOPPED_CONTINUE:
                    if (player->input && player->started)
                        vlc_player_input_Start(player->input);
                    break;
                default:
                    break;
            }

            send_event = !player->started;
            break;
        case VLC_PLAYER_STATE_STOPPING:
            input->started = false;
            if (input == player->input)
                player->input = NULL;

            if (player->started)
            {
                vlc_player_PrepareNextMedia(player);
                if (!player->next_media)
                    player->started = false;
            }
            send_event = !player->started;
            break;
        case VLC_PLAYER_STATE_STARTED:
        case VLC_PLAYER_STATE_PLAYING:
            if (player->started &&
                player->global_state == VLC_PLAYER_STATE_PLAYING)
                send_event = false;
            break;

        case VLC_PLAYER_STATE_PAUSED:
            assert(player->started && input->started);
            break;
        default:
            vlc_assert_unreachable();
    }

    if (send_event)
    {
        player->global_state = input->state;
        vlc_player_SendEvent(player, on_state_changed, player->global_state);
    }
}

size_t
vlc_player_GetProgramCount(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    return input ? input->program_vector.size : 0;
}

const struct vlc_player_program *
vlc_player_GetProgramAt(vlc_player_t *player, size_t index)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return NULL;

    assert(index < input->program_vector.size);
    return input->program_vector.data[index];
}

const struct vlc_player_program *
vlc_player_GetProgram(vlc_player_t *player, int id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return NULL;

    struct vlc_player_program *prgm =
        vlc_player_program_vector_FindById(&input->program_vector, id, NULL);
    return prgm;
}

static inline void
vlc_player_vout_OSDProgram(vlc_player_t *player, const char *name)
{
    vlc_player_vout_OSDMessage(player, _("Program Service ID: %s"), name);
}

void
vlc_player_SelectProgram(vlc_player_t *player, int id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    const struct vlc_player_program *prgm =
        vlc_player_program_vector_FindById(&input->program_vector,
                                           id, NULL);
    if (prgm)
    {
        input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_PROGRAM,
                                &(vlc_value_t) { .i_int = id });
        vlc_player_vout_OSDProgram(player, prgm->name);
    }
}

static void
vlc_player_CycleProgram(vlc_player_t *player, bool next)
{
    size_t count = vlc_player_GetProgramCount(player);
    if (!count)
        return;
    size_t index = 0;
    bool selected = false;
    for (size_t i = 0; i < count; ++i)
    {
        const struct vlc_player_program *prgm =
            vlc_player_GetProgramAt(player, i);
        if (prgm->selected)
        {
            /* Only one program can be selected at a time */
            assert(!selected);
            index = i;
            selected = true;
        }
    }
    assert(selected);
    if (next && index + 1 == count) /* First program */
        index = 0;
    else if (!next && index == 0) /* Last program */
        index = count - 1;
    else /* Next or Previous program */
        index = index + (next ? 1 : -1);

    const struct vlc_player_program *prgm =
        vlc_player_GetProgramAt(player, index);
    assert(prgm);
    vlc_player_SelectProgram(player, prgm->group_id);
}

void
vlc_player_SelectNextProgram(vlc_player_t *player)
{
    vlc_player_CycleProgram(player, true);
}

void
vlc_player_SelectPrevProgram(vlc_player_t *player)
{
    vlc_player_CycleProgram(player, false);
}

static void
vlc_player_input_HandleProgramEvent(struct vlc_player_input *input,
                                    const struct vlc_input_event_program *ev)
{
    vlc_player_t *player = input->player;
    struct vlc_player_program *prgm;
    vlc_player_program_vector *vec = &input->program_vector;

    switch (ev->action)
    {
        case VLC_INPUT_PROGRAM_ADDED:
            prgm = vlc_player_program_New(ev->id, ev->title);
            if (!prgm)
                break;

            if (!vlc_vector_push(vec, prgm))
            {
                vlc_player_program_Delete(prgm);
                break;
            }
            vlc_player_SendEvent(player, on_program_list_changed,
                                 VLC_PLAYER_LIST_ADDED, prgm);
            break;
        case VLC_INPUT_PROGRAM_DELETED:
        {
            size_t idx;
            prgm = vlc_player_program_vector_FindById(vec, ev->id, &idx);
            if (prgm)
            {
                vlc_player_SendEvent(player, on_program_list_changed,
                                     VLC_PLAYER_LIST_REMOVED, prgm);
                vlc_vector_remove(vec, idx);
                vlc_player_program_Delete(prgm);
            }
            break;
        }
        case VLC_INPUT_PROGRAM_UPDATED:
        case VLC_INPUT_PROGRAM_SCRAMBLED:
            prgm = vlc_player_program_vector_FindById(vec, ev->id, NULL);
            if (!prgm)
                break;
            if (ev->action == VLC_INPUT_PROGRAM_UPDATED)
            {
                if (vlc_player_program_Update(prgm, ev->id, ev->title) != 0)
                    break;
            }
            else
                prgm->scrambled = ev->scrambled;
            vlc_player_SendEvent(player, on_program_list_changed,
                                 VLC_PLAYER_LIST_UPDATED, prgm);
            break;
        case VLC_INPUT_PROGRAM_SELECTED:
        {
            int unselected_id = -1, selected_id = -1;
            vlc_vector_foreach(prgm, vec)
            {
                if (prgm->group_id == ev->id)
                {
                    if (!prgm->selected)
                    {
                        assert(selected_id == -1);
                        prgm->selected = true;
                        selected_id = prgm->group_id;
                    }
                }
                else
                {
                    if (prgm->selected)
                    {
                        assert(unselected_id == -1);
                        prgm->selected = false;
                        unselected_id = prgm->group_id;
                    }
                }
            }
            if (unselected_id != -1 || selected_id != -1)
                vlc_player_SendEvent(player, on_program_selection_changed,
                                     unselected_id, selected_id);
            break;
        }
        default:
            vlc_assert_unreachable();
    }
}

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

static struct vlc_player_track *
vlc_player_track_vector_FindById(vlc_player_track_vector *vec, vlc_es_id_t *id,
                                 size_t *idx)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct vlc_player_track *track = vec->data[i];
        if (track->es_id == id)
        {
            if (idx)
                *idx = i;
            return track;
        }
    }
    return NULL;
}

size_t
vlc_player_GetTrackCount(vlc_player_t *player, enum es_format_category_e cat)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return 0;
    vlc_player_track_vector *vec = vlc_player_input_GetTrackVector(input, cat);
    if (!vec)
        return 0;
    return vec->size;
}

const struct vlc_player_track *
vlc_player_GetTrackAt(vlc_player_t *player, enum es_format_category_e cat,
                      size_t index)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return NULL;
    vlc_player_track_vector *vec = vlc_player_input_GetTrackVector(input, cat);
    if (!vec)
        return NULL;
    assert(index < vec->size);
    return vec->data[index];
}

const struct vlc_player_track *
vlc_player_GetTrack(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return NULL;
    vlc_player_track_vector *vec =
        vlc_player_input_GetTrackVector(input, vlc_es_id_GetCat(id));
    if (!vec)
        return NULL;
    return vlc_player_track_vector_FindById(vec, id, NULL);
}

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

static void
vlc_player_vout_OSDTrack(vlc_player_t *player, vlc_es_id_t *id, bool select)
{
    enum es_format_category_e cat = vlc_es_id_GetCat(id);
    const struct vlc_player_track *track = vlc_player_GetTrack(player, id);
    if (!track && select)
        return;

    const char *cat_name = es_format_category_to_string(cat);
    assert(cat_name);
    const char *track_name = select ? track->name : _("N/A");
    vlc_player_vout_OSDMessage(player, _("%s track: %s"), cat_name, track_name);
}

void
vlc_player_SelectTrack(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    input_ControlPushEsHelper(input->thread, INPUT_CONTROL_SET_ES, id);
    vlc_player_vout_OSDTrack(player, id, true);
}

static void
vlc_player_CycleTrack(vlc_player_t *player, enum es_format_category_e cat,
                      bool next)
{
    size_t count = vlc_player_GetTrackCount(player, cat);
    if (!count)
        return;

    size_t index;
    bool selected = false;
    for (size_t i = 0; i < count; ++i)
    {
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt(player, cat, i);
        assert(track);
        if (track->selected)
        {
            if (selected)
            {
                /* Can't cycle through tracks if there are more than one
                 * selected */
                return;
            }
            index = i;
            selected = true;
        }
    }

    if (!selected)
    {
        /* No track selected: select the first or the last track */
        index = next ? 0 : count - 1;
        selected = true;
    }
    else
    {
        /* Unselect if we reach the end of the cycle */
        if ((next && index + 1 == count) || (!next && index == 0))
            selected = false;
        else /* Switch to the next or previous track */
            index = index + (next ? 1 : -1);
    }

    const struct vlc_player_track *track =
        vlc_player_GetTrackAt(player, cat, index);
    if (selected)
        vlc_player_SelectTrack(player, track->es_id);
    else
        vlc_player_UnselectTrack(player, track->es_id);
}

void
vlc_player_SelectNextTrack(vlc_player_t *player,
                           enum es_format_category_e cat)
{
    vlc_player_CycleTrack(player, cat, true);
}

void
vlc_player_SelectPrevTrack(vlc_player_t *player,
                           enum es_format_category_e cat)
{
    vlc_player_CycleTrack(player, cat, false);
}

void
vlc_player_UnselectTrack(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    input_ControlPushEsHelper(input->thread, INPUT_CONTROL_UNSET_ES, id);
    vlc_player_vout_OSDTrack(player, id, false);
}

void
vlc_player_RestartTrack(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (input)
        input_ControlPushEsHelper(input->thread, INPUT_CONTROL_RESTART_ES, id);
}

void
vlc_player_SelectDefaultTrack(vlc_player_t *player,
                              enum es_format_category_e cat, const char *lang)
{
    vlc_player_assert_locked(player);
    /* TODO */ (void) cat; (void) lang;
}

static void
vlc_player_input_HandleTeletextMenu(struct vlc_player_input *input,
                                    const struct vlc_input_event_es *ev)
{
    vlc_player_t *player = input->player;
    switch (ev->action)
    {
        case VLC_INPUT_ES_ADDED:
            if (input->teletext_menu)
            {
                msg_Warn(player, "Can't handle more than one teletext menu "
                         "track. Using the last one.");
                vlc_player_track_Delete(input->teletext_menu);
            }
            input->teletext_menu = vlc_player_track_New(ev->id, ev->title,
                                                        ev->fmt);
            if (!input->teletext_menu)
                return;

            vlc_player_SendEvent(player, on_teletext_menu_changed, true);
            break;
        case VLC_INPUT_ES_DELETED:
        {
            if (input->teletext_menu && input->teletext_menu->es_id == ev->id)
            {
                assert(!input->teletext_enabled);

                vlc_player_track_Delete(input->teletext_menu);
                input->teletext_menu = NULL;
                vlc_player_SendEvent(player, on_teletext_menu_changed, false);
            }
            break;
        }
        case VLC_INPUT_ES_UPDATED:
            break;
        case VLC_INPUT_ES_SELECTED:
        case VLC_INPUT_ES_UNSELECTED:
            if (input->teletext_menu->es_id == ev->id)
            {
                input->teletext_enabled = ev->action == VLC_INPUT_ES_SELECTED;
                vlc_player_SendEvent(player, on_teletext_enabled_changed,
                                     input->teletext_enabled);
            }
            break;
        default:
            vlc_assert_unreachable();
    }
}

void
vlc_player_SetTeletextEnabled(vlc_player_t *player, bool enabled)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_menu)
        return;
    if (enabled)
        vlc_player_SelectTrack(player, input->teletext_menu->es_id);
    else
        vlc_player_UnselectTrack(player, input->teletext_menu->es_id);
}

void
vlc_player_SelectTeletextPage(vlc_player_t *player, unsigned page)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_menu)
        return;

    input_ControlPush(input->thread, INPUT_CONTROL_SET_VBI_PAGE,
        &(input_control_param_t) {
            .vbi_page.id = input->teletext_menu->es_id,
            .vbi_page.page = page,
    });
}

void
vlc_player_SetTeletextTransparency(vlc_player_t *player, bool enabled)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_menu)
        return;

    input_ControlPush(input->thread, INPUT_CONTROL_SET_VBI_TRANSPARENCY,
        &(input_control_param_t) {
            .vbi_transparency.id = input->teletext_menu->es_id,
            .vbi_transparency.enabled = enabled,
    });
}

bool
vlc_player_HasTeletextMenu(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input && input->teletext_menu;
}

bool
vlc_player_IsTeletextEnabled(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input && input->teletext_enabled)
    {
        assert(input->teletext_menu);
        return true;
    }
    return false;
}

unsigned
vlc_player_GetTeletextPage(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return vlc_player_IsTeletextEnabled(player) ? input->teletext_page : 0;
}

bool
vlc_player_IsTeletextTransparent(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return vlc_player_IsTeletextEnabled(player) && input->teletext_transparent;
}

static void
vlc_player_input_HandleEsEvent(struct vlc_player_input *input,
                               const struct vlc_input_event_es *ev)
{
    assert(ev->id && ev->title && ev->fmt);

    if (ev->fmt->i_cat == SPU_ES && ev->fmt->i_codec == VLC_CODEC_TELETEXT
     && (ev->fmt->subs.teletext.i_magazine == 1
      || ev->fmt->subs.teletext.i_magazine == -1))
    {
        vlc_player_input_HandleTeletextMenu(input, ev);
        return;
    }

    vlc_player_track_vector *vec =
        vlc_player_input_GetTrackVector(input, ev->fmt->i_cat);
    if (!vec)
        return; /* UNKNOWN_ES or DATA_ES not handled */

    vlc_player_t *player = input->player;
    struct vlc_player_track *track;
    switch (ev->action)
    {
        case VLC_INPUT_ES_ADDED:
            track = vlc_player_track_New(ev->id, ev->title, ev->fmt);
            if (!track)
                break;

            if (!vlc_vector_push(vec, track))
            {
                vlc_player_track_Delete(track);
                break;
            }
            vlc_player_SendEvent(player, on_track_list_changed,
                                 VLC_PLAYER_LIST_ADDED, track);
            break;
        case VLC_INPUT_ES_DELETED:
        {
            size_t idx;
            track = vlc_player_track_vector_FindById(vec, ev->id, &idx);
            if (track)
            {
                vlc_player_SendEvent(player, on_track_list_changed,
                                     VLC_PLAYER_LIST_REMOVED, track);
                vlc_vector_remove(vec, idx);
                vlc_player_track_Delete(track);
            }
            break;
        }
        case VLC_INPUT_ES_UPDATED:
            track = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (!track)
                break;
            if (vlc_player_track_Update(track, ev->title, ev->fmt) != 0)
                break;
            vlc_player_SendEvent(player, on_track_list_changed,
                                 VLC_PLAYER_LIST_UPDATED, track);
            break;
        case VLC_INPUT_ES_SELECTED:
            track = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (track)
            {
                track->selected = true;
                vlc_player_SendEvent(player, on_track_selection_changed,
                                     NULL, track->es_id);
            }
            break;
        case VLC_INPUT_ES_UNSELECTED:
            track = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (track)
            {
                track->selected = false;
                vlc_player_SendEvent(player, on_track_selection_changed,
                                     track->es_id, NULL);
            }
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
vlc_player_input_HandleTitleEvent(struct vlc_player_input *input,
                                  const struct vlc_input_event_title *ev)
{
    vlc_player_t *player = input->player;
    switch (ev->action)
    {
        case VLC_INPUT_TITLE_NEW_LIST:
        {
            input_thread_private_t *input_th = input_priv(input->thread);
            const int title_offset = input_th->i_title_offset;
            const int chapter_offset = input_th->i_seekpoint_offset;

            if (input->titles)
                vlc_player_title_list_Release(input->titles);
            input->title_selected = input->chapter_selected = 0;
            input->titles =
                vlc_player_title_list_Create(ev->list.array, ev->list.count,
                                             title_offset, chapter_offset);
            vlc_player_SendEvent(player, on_titles_changed, input->titles);
            if (input->titles)
                vlc_player_SendEvent(player, on_title_selection_changed,
                                     &input->titles->array[0], 0);
            break;
        }
        case VLC_INPUT_TITLE_SELECTED:
            if (!input->titles)
                return; /* a previous VLC_INPUT_TITLE_NEW_LIST failed */
            assert(ev->selected_idx < input->titles->count);
            input->title_selected = ev->selected_idx;
            vlc_player_SendEvent(player, on_title_selection_changed,
                                 &input->titles->array[input->title_selected],
                                 input->title_selected);
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
vlc_player_input_HandleChapterEvent(struct vlc_player_input *input,
                                    const struct vlc_input_event_chapter *ev)
{
    vlc_player_t *player = input->player;
    if (!input->titles || ev->title < 0 || ev->seekpoint < 0)
        return; /* a previous VLC_INPUT_TITLE_NEW_LIST failed */

    assert((size_t)ev->title < input->titles->count);
    const struct vlc_player_title *title = &input->titles->array[ev->title];
    if (!title->chapter_count)
        return;

    assert(ev->seekpoint < (int)title->chapter_count);
    input->title_selected = ev->title;
    input->chapter_selected = ev->seekpoint;

    const struct vlc_player_chapter *chapter = &title->chapters[ev->seekpoint];
    vlc_player_SendEvent(player, on_chapter_selection_changed, title, ev->title,
                         chapter, ev->seekpoint);
}

struct vlc_player_title_list *
vlc_player_GetTitleList(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->titles : NULL;
}

ssize_t
vlc_player_GetSelectedTitleIdx(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return -1;
    return input->title_selected;
}

static ssize_t
vlc_player_GetTitleIdx(vlc_player_t *player,
                       const struct vlc_player_title *title)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input && input->titles)
        for (size_t i = 0; i < input->titles->count; ++i)
            if (&input->titles->array[i] == title)
                return i;
    return -1;
}

void
vlc_player_SelectTitleIdx(vlc_player_t *player, size_t index)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input)
        input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_TITLE,
                                &(vlc_value_t){ .i_int = index });
}

void
vlc_player_SelectTitle(vlc_player_t *player,
                       const struct vlc_player_title *title)
{
    ssize_t idx = vlc_player_GetTitleIdx(player, title);
    if (idx != -1)
        vlc_player_SelectTitleIdx(player, idx);
}

void
vlc_player_SelectChapter(vlc_player_t *player,
                         const struct vlc_player_title *title,
                         size_t chapter_idx)
{
    ssize_t idx = vlc_player_GetTitleIdx(player, title);
    if (idx != -1 && idx == vlc_player_GetSelectedTitleIdx(player))
        vlc_player_SelectChapterIdx(player, chapter_idx);
}

void
vlc_player_SelectNextTitle(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPush(input->thread, INPUT_CONTROL_SET_TITLE_NEXT, NULL);
    vlc_player_vout_OSDMessage(player, _("Next title"));
}

void
vlc_player_SelectPrevTitle(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPush(input->thread, INPUT_CONTROL_SET_TITLE_PREV, NULL);
    vlc_player_vout_OSDMessage(player, _("Previous title"));
}

ssize_t
vlc_player_GetSelectedChapterIdx(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return -1;
    return input->chapter_selected;
}

void
vlc_player_SelectChapterIdx(vlc_player_t *player, size_t index)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_SEEKPOINT,
                            &(vlc_value_t){ .i_int = index });
    vlc_player_vout_OSDMessage(player, _("Chapter %ld"), index);
}

void
vlc_player_SelectNextChapter(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPush(input->thread, INPUT_CONTROL_SET_SEEKPOINT_NEXT, NULL);
    vlc_player_vout_OSDMessage(player, _("Next chapter"));
}

void
vlc_player_SelectPrevChapter(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPush(input->thread, INPUT_CONTROL_SET_SEEKPOINT_PREV, NULL);
    vlc_player_vout_OSDMessage(player, _("Previous chapter"));
}

static void
vlc_player_input_HandleVoutEvent(struct vlc_player_input *input,
                                 const struct vlc_input_event_vout *ev)
{
    assert(ev->vout);

    static const char osd_vars[][sizeof("deinterlace-mode")] = {
        "aspect-ratio", "autoscale", "crop", "crop-bottom",
        "crop-top", "crop-left", "crop-right", "deinterlace",
        "deinterlace-mode", "sub-margin", "zoom"
    };

    vlc_player_t *player = input->player;
    switch (ev->action)
    {
        case VLC_INPUT_EVENT_VOUT_ADDED:
            vlc_player_SendEvent(player, on_vout_list_changed,
                                 VLC_PLAYER_LIST_ADDED, ev->vout);

            /* Register vout callbacks after the vout list event */
            var_AddCallback(ev->vout, "fullscreen",
                            vlc_player_VoutCallback, player);
            var_AddCallback(ev->vout, "video-wallpaper",
                            vlc_player_VoutCallback, player);
            for (size_t i = 0; i < ARRAY_SIZE(osd_vars); ++i)
                var_AddCallback(ev->vout, osd_vars[i],
                                vlc_player_VoutOSDCallback, player);
            break;
        case VLC_INPUT_EVENT_VOUT_DELETED:
            /* Un-register vout callbacks before the vout list event */
            var_DelCallback(ev->vout, "fullscreen",
                            vlc_player_VoutCallback, player);
            var_DelCallback(ev->vout, "video-wallpaper",
                            vlc_player_VoutCallback, player);
            for (size_t i = 0; i < ARRAY_SIZE(osd_vars); ++i)
                var_DelCallback(ev->vout, osd_vars[i],
                                vlc_player_VoutOSDCallback, player);

            vlc_player_SendEvent(player, on_vout_list_changed,
                                 VLC_PLAYER_LIST_REMOVED, ev->vout);
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
vlc_player_input_HandleStateEvent(struct vlc_player_input *input,
                                  input_state_e state)
{
    switch (state)
    {
        case OPENING_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STARTED);
            break;
        case PLAYING_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_PLAYING);
            break;
        case PAUSE_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_PAUSED);
            break;
        case END_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING);
            vlc_player_destructor_AddStoppingInput(input->player, input);
            break;
        case ERROR_S:
            /* Don't send errors if the input is stopped by the user */
            if (input->started)
            {
                /* Contrary to the input_thead_t, an error is not a state */
                input->error = VLC_PLAYER_ERROR_GENERIC;
                vlc_player_SendEvent(input->player, on_error_changed, input->error);
            }
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
vlc_player_HandleAtoBLoop(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    assert(input);
    assert(input->abloop_state[0].set && input->abloop_state[1].set);

    if (input->time != VLC_TICK_INVALID
     && input->abloop_state[0].time != VLC_TICK_INVALID
     && input->abloop_state[1].time != VLC_TICK_INVALID)
    {
        if (input->time >= input->abloop_state[1].time)
            vlc_player_SetTime(player, input->abloop_state[0].time);
    }
    else if (input->position >= input->abloop_state[1].pos)
        vlc_player_SetPosition(player, input->abloop_state[0].pos);
}

static void
input_thread_Events(input_thread_t *input_thread,
                    const struct vlc_input_event *event, void *user_data)
{
    struct vlc_player_input *input = user_data;
    vlc_player_t *player = input->player;

    assert(input_thread == input->thread);

    vlc_mutex_lock(&player->lock);

    switch (event->type)
    {
        case INPUT_EVENT_STATE:
            vlc_player_input_HandleStateEvent(input, event->state);
            break;
        case INPUT_EVENT_RATE:
            input->rate = event->rate;
            vlc_player_SendEvent(player, on_rate_changed, input->rate);
            break;
        case INPUT_EVENT_CAPABILITIES:
            input->capabilities = event->capabilities;
            vlc_player_SendEvent(player, on_capabilities_changed,
                                 input->capabilities);
            break;
        case INPUT_EVENT_POSITION:
            if (input->time != event->position.ms ||
                input->position != event->position.percentage)
            {
                input->time = event->position.ms;
                input->position = event->position.percentage;
                vlc_player_SendEvent(player, on_position_changed,
                                     input->time,
                                     input->position);

                if (input->abloop_state[0].set && input->abloop_state[1].set
                 && input == player->input)
                    vlc_player_HandleAtoBLoop(player);
            }
            break;
        case INPUT_EVENT_LENGTH:
            if (input->length != event->length)
            {
                input->length = event->length;
                vlc_player_SendEvent(player, on_length_changed, input->length);
            }
            break;
        case INPUT_EVENT_PROGRAM:
            vlc_player_input_HandleProgramEvent(input, &event->program);
            break;
        case INPUT_EVENT_ES:
            vlc_player_input_HandleEsEvent(input, &event->es);
            break;
        case INPUT_EVENT_TITLE:
            vlc_player_input_HandleTitleEvent(input, &event->title);
            break;
        case INPUT_EVENT_CHAPTER:
            vlc_player_input_HandleChapterEvent(input, &event->chapter);
            break;
        case INPUT_EVENT_RECORD:
            input->recording = event->record;
            vlc_player_SendEvent(player, on_recording_changed, input->recording);
            break;
        case INPUT_EVENT_STATISTICS:
            input->stats = *event->stats;
            vlc_player_SendEvent(player, on_statistics_changed, &input->stats);
            break;
        case INPUT_EVENT_SIGNAL:
            input->signal_quality = event->signal.quality;
            input->signal_strength = event->signal.strength;
            vlc_player_SendEvent(player, on_signal_changed,
                                 input->signal_quality, input->signal_strength);
            break;
        case INPUT_EVENT_AUDIO_DELAY:
            input->audio_delay = event->audio_delay;
            vlc_player_SendEvent(player, on_audio_delay_changed,
                                 input->audio_delay);
            break;
        case INPUT_EVENT_SUBTITLE_DELAY:
            input->subtitle_delay = event->subtitle_delay;
            vlc_player_SendEvent(player, on_subtitle_delay_changed,
                                 input->subtitle_delay);
            break;
        case INPUT_EVENT_CACHE:
            input->cache = event->cache;
            vlc_player_SendEvent(player, on_buffering_changed, event->cache);
            break;
        case INPUT_EVENT_VOUT:
            vlc_player_input_HandleVoutEvent(input, &event->vout);
            break;
        case INPUT_EVENT_ITEM_META:
            vlc_player_SendEvent(player, on_media_meta_changed,
                                 input_GetItem(input->thread));
            break;
        case INPUT_EVENT_ITEM_EPG:
            vlc_player_SendEvent(player, on_media_epg_changed,
                                 input_GetItem(input->thread));
            break;
        case INPUT_EVENT_SUBITEMS:
            vlc_player_SendEvent(player, on_media_subitems_changed,
                                 input_GetItem(input->thread), event->subitems);
            break;
        case INPUT_EVENT_DEAD:
            if (input->started) /* Can happen with early input_thread fails */
                vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING);
            vlc_player_destructor_AddJoinableInput(player, input);
            break;
        case INPUT_EVENT_VBI_PAGE:
            input->teletext_page = event->vbi_page < 999 ? event->vbi_page : 100;
            vlc_player_SendEvent(player, on_teletext_page_changed,
                                 input->teletext_page);
            break;
        case INPUT_EVENT_VBI_TRANSPARENCY:
            input->teletext_transparent = event->vbi_transparent;
            vlc_player_SendEvent(player, on_teletext_transparency_changed,
                                 input->teletext_transparent);
            break;
        default:
            break;
    }

    vlc_mutex_unlock(&player->lock);
}

void
vlc_player_Lock(vlc_player_t *player)
{
    vlc_mutex_lock(&player->lock);
}

void
vlc_player_Unlock(vlc_player_t *player)
{
    vlc_mutex_unlock(&player->lock);
}

void
vlc_player_CondWait(vlc_player_t *player, vlc_cond_t *cond)
{
    vlc_player_assert_locked(player);
    vlc_cond_wait(cond, &player->lock);
}

vlc_player_listener_id *
vlc_player_AddListener(vlc_player_t *player,
                       const struct vlc_player_cbs *cbs, void *cbs_data)
{
    assert(cbs);
    vlc_player_assert_locked(player);

    vlc_player_listener_id *listener = malloc(sizeof(*listener));
    if (!listener)
        return NULL;

    listener->cbs = cbs;
    listener->cbs_data = cbs_data;

    vlc_list_append(&listener->node, &player->listeners);

    return listener;
}

void
vlc_player_RemoveListener(vlc_player_t *player,
                          vlc_player_listener_id *id)
{
    assert(id);
    vlc_player_assert_locked(player);

    vlc_list_remove(&id->node);
    free(id);
}

int
vlc_player_SetCurrentMedia(vlc_player_t *player, input_item_t *media)
{
    vlc_player_assert_locked(player);

    vlc_player_CancelWaitError(player);

    vlc_player_InvalidateNextMedia(player);

    if (media)
    {
        /* Switch to this new media when the current input is stopped */
        player->next_media = input_item_Hold(media);
        player->releasing_media = false;
        player->next_media_requested = true;
    }
    else if (player->media)
    {
        /* The current media will be set to NULL once the current input is
         * stopped */
        player->releasing_media = true;
        player->next_media_requested = false;
    }
    else
        return VLC_SUCCESS;

    if (player->input)
    {
        vlc_player_destructor_AddInput(player, player->input);
        player->input = NULL;
    }

    assert(media == player->next_media);
    if (!vlc_player_destructor_IsEmpty(player))
    {
        /* This media will be opened when the input is finally stopped */
        return VLC_SUCCESS;
    }

    /* We can switch to the next media directly */
    return vlc_player_OpenNextMedia(player);
}

input_item_t *
vlc_player_GetCurrentMedia(vlc_player_t *player)
{
    vlc_player_assert_locked(player);

    return player->media;
}

int
vlc_player_AddAssociatedMedia(vlc_player_t *player,
                              enum es_format_category_e cat, const char *uri,
                              bool select, bool notify, bool check_ext)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return VLC_EGENERIC;

    enum slave_type type;
    switch (cat)
    {
        case AUDIO_ES:
            type = SLAVE_TYPE_AUDIO;
            break;
        case SPU_ES:
            type = SLAVE_TYPE_SPU;
            break;
        default:
            return VLC_EGENERIC;
    }
    return input_AddSlave(input->thread, type, uri, select, notify, check_ext);
}

void
vlc_player_SetAssociatedSubsFPS(vlc_player_t *player, float fps)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    var_SetFloat(player, "sub-fps", fps);
    if (input)
        input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_SUBS_FPS,
                                &(vlc_value_t) { .f_float = fps });
    vlc_player_SendEvent(player, on_associated_subs_fps_changed, fps);
}

float
vlc_player_GetAssociatedSubsFPS(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    return var_GetFloat(player, "sub-fps");
}

void
vlc_player_InvalidateNextMedia(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    if (player->next_media)
    {
        input_item_Release(player->next_media);
        player->next_media = NULL;
    }
    player->next_media_requested = false;

}

int
vlc_player_Start(vlc_player_t *player)
{
    vlc_player_assert_locked(player);

    vlc_player_CancelWaitError(player);

    if (player->started)
        return VLC_SUCCESS;

    if (!vlc_player_destructor_IsEmpty(player))
    {
        if (player->next_media)
        {
            player->started = true;
            return VLC_SUCCESS;
        }
        else
            return VLC_EGENERIC;
    }

    if (!player->media)
        return VLC_EGENERIC;

    if (!player->input)
    {
        /* Possible if the player was stopped by the user */
        player->input = vlc_player_input_New(player, player->media);

        if (!player->input)
            return VLC_ENOMEM;
    }
    assert(!player->input->started);

    if (player->start_paused)
    {
        var_Create(player->input->thread, "start-paused", VLC_VAR_BOOL);
        var_SetBool(player->input->thread, "start-paused", true);
    }

    int ret = vlc_player_input_Start(player->input);
    if (ret == VLC_SUCCESS)
        player->started = true;

    vlc_player_vout_OSDIcon(player, OSD_PLAY_ICON);
    return ret;
}

void
vlc_player_Stop(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    vlc_player_CancelWaitError(player);

    vlc_player_InvalidateNextMedia(player);

    if (!input || !player->started)
        return;
    player->started = false;

    vlc_player_destructor_AddInput(player, input);
    player->input = NULL;

}

void
vlc_player_SetMediaStoppedAction(vlc_player_t *player,
                                 enum vlc_player_media_stopped_action action)
{
    vlc_player_assert_locked(player);
    player->media_stopped_action = action;
    var_SetBool(player, "play-and-pause",
                action == VLC_PLAYER_MEDIA_STOPPED_PAUSE);
    vlc_player_SendEvent(player, on_media_stopped_action_changed, action);
}

void
vlc_player_SetStartPaused(vlc_player_t *player, bool start_paused)
{
    vlc_player_assert_locked(player);
    player->start_paused = start_paused;
}

static void
vlc_player_SetPause(vlc_player_t *player, bool pause)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input || !input->started)
        return;

    vlc_value_t val = { .i_int = pause ? PAUSE_S : PLAYING_S };
    input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_STATE, &val);

    vlc_player_vout_OSDIcon(player, pause ? OSD_PAUSE_ICON : OSD_PLAY_ICON);
}

void
vlc_player_Pause(vlc_player_t *player)
{
    vlc_player_SetPause(player, true);
}

void
vlc_player_Resume(vlc_player_t *player)
{
    vlc_player_SetPause(player, false);
}

void
vlc_player_NextVideoFrame(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_FRAME_NEXT, NULL);
    vlc_player_vout_OSDMessage(player, _("Next frame"));
}

enum vlc_player_state
vlc_player_GetState(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    return player->global_state;
}

enum vlc_player_error
vlc_player_GetError(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->error : VLC_PLAYER_ERROR_NONE;
}

int
vlc_player_GetCapabilities(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->capabilities : 0;
}

float
vlc_player_GetRate(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input)
        return input->rate;
    else
        return var_GetFloat(player, "rate");
}

void
vlc_player_ChangeRate(vlc_player_t *player, float rate)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (rate == 0.0)
        return;

    /* Save rate accross inputs */
    var_SetFloat(player, "rate", rate);

    if (input)
    {
        input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_RATE,
            &(vlc_value_t) { .f_float = rate });
    }
    else
        vlc_player_SendEvent(player, on_rate_changed, rate);

    vlc_player_vout_OSDMessage(player, ("Speed: %.2fx"), rate);
}

static void
vlc_player_ChangeRateOffset(vlc_player_t *player, bool increment)
{
    static const float rates[] = {
        1.0/64, 1.0/32, 1.0/16, 1.0/8, 1.0/4, 1.0/3, 1.0/2, 2.0/3,
        1.0/1,
        3.0/2, 2.0/1, 3.0/1, 4.0/1, 8.0/1, 16.0/1, 32.0/1, 64.0/1,
    };
    float rate = vlc_player_GetRate(player) * (increment ? 1.1f : 0.9f);

    /* find closest rate (if any) in the desired direction */
    for (size_t i = 0; i < ARRAY_SIZE(rates); ++i)
    {
        if ((increment && rates[i] > rate) ||
            (!increment && rates[i] >= rate && i))
        {
            rate = increment ? rates[i] : rates[i-1];
            break;
        }
    }

    vlc_player_ChangeRate(player, rate);
}

void
vlc_player_IncrementRate(vlc_player_t *player)
{
    vlc_player_ChangeRateOffset(player, true);
}

void
vlc_player_DecrementRate(vlc_player_t *player)
{
    vlc_player_ChangeRateOffset(player, false);
}

vlc_tick_t
vlc_player_GetLength(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->length : VLC_TICK_INVALID;
}

vlc_tick_t
vlc_player_GetTime(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input || input->time == VLC_TICK_INVALID)
        return VLC_TICK_INVALID;

    return input->time;
}

float
vlc_player_GetPosition(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    return input ? input->position : -1.f;
}

static inline void
vlc_player_assert_seek_params(enum vlc_player_seek_speed speed,
                              enum vlc_player_whence whence)
{
    assert(speed == VLC_PLAYER_SEEK_PRECISE
        || speed == VLC_PLAYER_SEEK_FAST);
    assert(whence == VLC_PLAYER_WHENCE_ABSOLUTE
        || whence == VLC_PLAYER_WHENCE_RELATIVE);
    (void) speed; (void) whence;
}

static inline void
vlc_player_vout_OSDPosition(vlc_player_t *player,
                            struct vlc_player_input *input, vlc_tick_t time,
                            float position, enum vlc_player_whence whence)
{
    if (input->length != VLC_TICK_INVALID)
    {
        if (time == VLC_TICK_INVALID)
            time = position * input->length;
        else
            position = time / (float) input->length;
    }

    size_t count;
    vout_thread_t **vouts = vlc_player_vout_OSDHoldAll(player, &count);

    if (time != VLC_TICK_INVALID)
    {
        if (whence == VLC_PLAYER_WHENCE_RELATIVE)
        {
            time += input->time; /* XXX: TOCTOU */
            if (time < 0)
                time = 0;
        }

        char time_text[MSTRTIME_MAX_SIZE];
        secstotimestr(time_text, SEC_FROM_VLC_TICK(time));
        if (input->length != VLC_TICK_INVALID)
        {
            char len_text[MSTRTIME_MAX_SIZE];
            secstotimestr(len_text, SEC_FROM_VLC_TICK(input->length));
            vouts_osd_Message(vouts, count, "%s / %s", time_text, len_text);
        }
        else
            vouts_osd_Message(vouts, count, "%s", time_text);
    }

    if (vlc_player_vout_IsFullscreen(player))
    {
        if (whence == VLC_PLAYER_WHENCE_RELATIVE)
        {
            position += input->position; /* XXX: TOCTOU */
            if (position < 0.f)
                position = 0.f;
        }
        vouts_osd_Slider(vouts, count, position * 100, OSD_HOR_SLIDER);
    }
    vlc_player_vout_OSDReleaseAll(player, vouts, count);
}

void
vlc_player_DisplayPosition(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    vlc_player_vout_OSDPosition(player, input, input->time, input->position,
                                   VLC_PLAYER_WHENCE_ABSOLUTE);
}

void
vlc_player_SeekByPos(vlc_player_t *player, float position,
                     enum vlc_player_seek_speed speed,
                     enum vlc_player_whence whence)
{
    vlc_player_assert_seek_params(speed, whence);

    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    const int type =
        whence == VLC_PLAYER_WHENCE_ABSOLUTE ? INPUT_CONTROL_SET_POSITION
                                             : INPUT_CONTROL_JUMP_POSITION;
    input_ControlPush(input->thread, type,
        &(input_control_param_t) {
            .pos.f_val = position,
            .pos.b_fast_seek = speed == VLC_PLAYER_SEEK_FAST,
    });

    vlc_player_vout_OSDPosition(player, input, VLC_TICK_INVALID, position,
                                   whence);
}

void
vlc_player_SeekByTime(vlc_player_t *player, vlc_tick_t time,
                      enum vlc_player_seek_speed speed,
                      enum vlc_player_whence whence)
{
    vlc_player_assert_seek_params(speed, whence);

    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    const int type =
        whence == VLC_PLAYER_WHENCE_ABSOLUTE ? INPUT_CONTROL_SET_TIME
                                             : INPUT_CONTROL_JUMP_TIME;
    input_ControlPush(input->thread, type,
        &(input_control_param_t) {
            .time.i_val = time,
            .time.b_fast_seek = speed == VLC_PLAYER_SEEK_FAST,
    });

    vlc_player_vout_OSDPosition(player, input, time, -1, whence);
}

void
vlc_player_SetRenderer(vlc_player_t *player, vlc_renderer_item_t *renderer)
{
    vlc_player_assert_locked(player);

    if (player->renderer)
        vlc_renderer_item_release(player->renderer);
    player->renderer = renderer ? vlc_renderer_item_hold(renderer) : NULL;

    vlc_player_foreach_inputs(input)
    {
        vlc_value_t val = {
            .p_address = renderer ? vlc_renderer_item_hold(renderer) : NULL
        };
        input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_RENDERER,
                                &val);
    }
    vlc_player_SendEvent(player, on_renderer_changed, player->renderer);
}

vlc_renderer_item_t *
vlc_player_GetRenderer(vlc_player_t *player)
{
    vlc_player_assert_locked(player);
    return player->renderer;
}

int
vlc_player_SetAtoBLoop(vlc_player_t *player, enum vlc_player_abloop abloop)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input || !vlc_player_CanSeek(player))
        return VLC_EGENERIC;

    vlc_tick_t time = vlc_player_GetTime(player);
    float pos = vlc_player_GetPosition(player);
    int ret = VLC_SUCCESS;
    switch (abloop)
    {
        case VLC_PLAYER_ABLOOP_A:
            if (input->abloop_state[1].set)
                return VLC_EGENERIC;
            input->abloop_state[0].time = time;
            input->abloop_state[0].pos = pos;
            input->abloop_state[0].set = true;
            break;
        case VLC_PLAYER_ABLOOP_B:
            if (!input->abloop_state[0].set)
                return VLC_EGENERIC;
            input->abloop_state[1].time = time;
            input->abloop_state[1].pos = pos;
            input->abloop_state[1].set = true;
            if (input->abloop_state[0].time != VLC_TICK_INVALID
             && time != VLC_TICK_INVALID)
            {
                if (time > input->abloop_state[0].time)
                {
                    vlc_player_SetTime(player, input->abloop_state[0].time);
                    break;
                }
            }
            else if (pos > input->abloop_state[0].pos)
            {
                vlc_player_SetPosition(player, input->abloop_state[0].pos);
                break;
            }

            /* Error: A time is superior to B time. */
            abloop = VLC_PLAYER_ABLOOP_NONE;
            ret = VLC_EGENERIC;
            /* fall-through */
        case VLC_PLAYER_ABLOOP_NONE:
            input->abloop_state[0].set = input->abloop_state[1].set = false;
            time = VLC_TICK_INVALID;
            pos = 0.f;
            break;
        default:
            vlc_assert_unreachable();
    }
    vlc_player_SendEvent(player, on_atobloop_changed, abloop, time, pos);
    return ret;
}

enum vlc_player_abloop
vlc_player_GetAtoBLoop(vlc_player_t *player, vlc_tick_t *a_time, float *a_pos,
                       vlc_tick_t *b_time, float *b_pos)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input || !vlc_player_CanSeek(player) || !input->abloop_state[0].set)
        return VLC_PLAYER_ABLOOP_NONE;

    if (a_time)
        *a_time = input->abloop_state[0].time;
    if (a_pos)
        *a_pos = input->abloop_state[0].pos;
    if (!input->abloop_state[1].set)
        return VLC_PLAYER_ABLOOP_A;

    if (b_time)
        *b_time = input->abloop_state[1].time;
    if (b_pos)
        *b_pos = input->abloop_state[1].pos;
    return VLC_PLAYER_ABLOOP_B;
}

void
vlc_player_Navigate(vlc_player_t *player, enum vlc_player_nav nav)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return;

    enum input_control_e control;
    switch (nav)
    {
        case VLC_PLAYER_NAV_ACTIVATE:
            control = INPUT_CONTROL_NAV_ACTIVATE;
            break;
        case VLC_PLAYER_NAV_UP:
            control = INPUT_CONTROL_NAV_UP;
            break;
        case VLC_PLAYER_NAV_DOWN:
            control = INPUT_CONTROL_NAV_DOWN;
            break;
        case VLC_PLAYER_NAV_LEFT:
            control = INPUT_CONTROL_NAV_LEFT;
            break;
        case VLC_PLAYER_NAV_RIGHT:
            control = INPUT_CONTROL_NAV_RIGHT;
            break;
        case VLC_PLAYER_NAV_POPUP:
            control = INPUT_CONTROL_NAV_POPUP;
            break;
        case VLC_PLAYER_NAV_MENU:
            control = INPUT_CONTROL_NAV_MENU;
            break;
        default:
            vlc_assert_unreachable();
    }
    input_ControlPushHelper(input->thread, control, NULL);
}

void
vlc_player_UpdateViewpoint(vlc_player_t *player,
                           const vlc_viewpoint_t *viewpoint,
                           enum vlc_player_whence whence)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input)
        input_UpdateViewpoint(input->thread, viewpoint,
                              whence == VLC_PLAYER_WHENCE_ABSOLUTE);
}

bool
vlc_player_IsRecording(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    return input ? input->recording : false;
}

void
vlc_player_SetRecordingEnabled(vlc_player_t *player, bool enable)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_RECORD_STATE,
                            &(vlc_value_t) { .b_bool = enable });

    vlc_player_vout_OSDMessage(player, enable ?
                               _("Recording") : _("Recording done"));
}

void
vlc_player_SetAudioDelay(vlc_player_t *player, vlc_tick_t delay,
                         enum vlc_player_whence whence)
{
    bool absolute = whence == VLC_PLAYER_WHENCE_ABSOLUTE;
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    input_ControlPush(input->thread, INPUT_CONTROL_SET_AUDIO_DELAY,
        &(input_control_param_t) {
            .delay = {
                .b_absolute = whence == VLC_PLAYER_WHENCE_ABSOLUTE,
                .i_val = delay,
            },
    });

    if (!absolute)
        delay += input->audio_delay;
    vlc_player_vout_OSDMessage(player, _("Audio delay: %i ms"),
                               (int)MS_FROM_VLC_TICK(delay));
}

vlc_tick_t
vlc_player_GetAudioDelay(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->audio_delay : 0;
}

static void
vlc_player_SetSubtitleDelayInternal(vlc_player_t *player, vlc_tick_t delay,
                                    enum vlc_player_whence whence)
{
    bool absolute = whence == VLC_PLAYER_WHENCE_ABSOLUTE;
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    input_ControlPush(input->thread, INPUT_CONTROL_SET_SPU_DELAY,
        &(input_control_param_t) {
            .delay = {
                .b_absolute = absolute,
                .i_val = delay,
            },
    });
}

void
vlc_player_SetSubtitleDelay(vlc_player_t *player, vlc_tick_t delay,
                            enum vlc_player_whence whence)
{
    vlc_player_SetSubtitleDelayInternal(player, delay, whence);
    vlc_player_vout_OSDMessage(player, _("Subtitle delay: %s%i ms"),
                               whence == VLC_PLAYER_WHENCE_ABSOLUTE ? "" : "+",
                               (int)MS_FROM_VLC_TICK(delay));
}

static struct {
    const char var[sizeof("video")];
    const char sout_var[sizeof("sout-video")];
} cat2vars[] = {
    [VIDEO_ES] = { "video", "sout-video" },
    [AUDIO_ES] = { "audio", "sout-audio" },
    [SPU_ES] = { "spu", "sout-spu" },
};

void
vlc_player_SetTrackCategoryEnabled(vlc_player_t *player,
                                   enum es_format_category_e cat, bool enabled)
{
    assert(cat >= UNKNOWN_ES && cat <= DATA_ES);
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    var_SetBool(player, cat2vars[cat].var, enabled);
    var_SetBool(player, cat2vars[cat].sout_var, enabled);

    if (input)
    {
        var_SetBool(input->thread, cat2vars[cat].var, enabled);
        var_SetBool(input->thread, cat2vars[cat].sout_var, enabled);

        if (!enabled)
            vlc_player_UnselectTrackCategory(player, cat);
    }
}

bool
vlc_player_IsTrackCategoryEnabled(vlc_player_t *player,
                                  enum es_format_category_e cat)
{
    assert(cat >= UNKNOWN_ES && cat <= DATA_ES);
    return var_GetBool(player, cat2vars[cat].var);
}

void
vlc_player_SetSubtitleTextScale(vlc_player_t *player, unsigned scale)
{
    assert(scale >= 10 && scale <= 500);
    var_SetInteger(player, "sub-text-scale", scale);
}

unsigned
vlc_player_GetSubtitleTextScale(vlc_player_t *player)
{
    return var_GetInteger(player, "sub-text-scale");
}

static void
vlc_player_SubtitleSyncMarkAudio(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input->subsync.audio_time = vlc_tick_now();
    vlc_player_vout_OSDMessage(player, _("Sub sync: bookmarked audio time"));
}

static void
vlc_player_SubtitleSyncMarkSubtitle(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    input->subsync.subtitle_time = vlc_tick_now();
    vlc_player_vout_OSDMessage(player, _("Sub sync: bookmarked subtitle time"));
}

static void
vlc_player_SubtitleSyncApply(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    if (input->subsync.audio_time == VLC_TICK_INVALID ||
        input->subsync.subtitle_time == VLC_TICK_INVALID)
    {
        vlc_player_vout_OSDMessage(player, _("Sub sync: set bookmarks first!"));
        return;
    }
    vlc_tick_t delay =
        input->subsync.audio_time - input->subsync.subtitle_time;
    input->subsync.audio_time = VLC_TICK_INVALID;
    input->subsync.subtitle_time = VLC_TICK_INVALID;
    vlc_player_SetSubtitleDelayInternal(player, delay,
                                        VLC_PLAYER_WHENCE_RELATIVE);

    long long delay_ms = MS_FROM_VLC_TICK(delay);
    long long totdelay_ms = MS_FROM_VLC_TICK(input->subtitle_delay + delay);
    vlc_player_vout_OSDMessage(player, _("Sub sync: corrected %"PRId64
                               " ms (total delay = %"PRId64" ms)"),
                               delay_ms, totdelay_ms);
}

static void
vlc_player_SubtitleSyncReset(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    vlc_player_SetSubtitleDelayInternal(player, 0, VLC_PLAYER_WHENCE_ABSOLUTE);
    input->subsync.audio_time = VLC_TICK_INVALID;
    input->subsync.subtitle_time = VLC_TICK_INVALID;
    vlc_player_vout_OSDMessage(player, _("Sub sync: delay reset"));
}

void
vlc_player_SetSubtitleSync(vlc_player_t *player,
                           enum vlc_player_subtitle_sync sync)
{
    switch (sync)
    {
        case VLC_PLAYER_SUBTITLE_SYNC_RESET:
            vlc_player_SubtitleSyncReset(player);
            break;
        case VLC_PLAYER_SUBTITLE_SYNC_MARK_AUDIO:
            vlc_player_SubtitleSyncMarkAudio(player);
            break;
        case VLC_PLAYER_SUBTITLE_SYNC_MARK_SUBTITLE:
            vlc_player_SubtitleSyncMarkSubtitle(player);
            break;
        case VLC_PLAYER_SUBTITLE_SYNC_APPLY:
            vlc_player_SubtitleSyncApply(player);
            break;
        default:
            vlc_assert_unreachable();
    }
}

vlc_tick_t
vlc_player_GetSubtitleDelay(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input ? input->subtitle_delay : 0;
}

int
vlc_player_GetSignal(vlc_player_t *player, float *quality, float *strength)
{
    assert(quality && strength);
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (input && input->signal_quality >= 0 && input->signal_strength >= 0)
    {
        *quality = input->signal_quality;
        *strength = input->signal_strength;
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

const struct input_stats_t *
vlc_player_GetStatistics(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    return input ? &input->stats : NULL;
}

void
vlc_player_SetPauseOnCork(vlc_player_t *player, bool enabled)
{
    vlc_player_assert_locked(player);
    player->pause_on_cork = enabled;
}

static int
vlc_player_CorkCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vlc_player_t *player = data;

    if (oldval.i_int == newval.i_int )
        return VLC_SUCCESS;

    vlc_player_Lock(player);

    if (player->pause_on_cork)
    {
        if (newval.i_int)
        {
            player->corked = player->global_state == VLC_PLAYER_STATE_PLAYING
                          || player->global_state == VLC_PLAYER_STATE_STARTED;
            if (player->corked)
                vlc_player_Pause(player);
        }
        else
        {
            if (player->corked)
            {
                vlc_player_Resume(player);
                player->corked = false;
            }
        }
    }
    else
        vlc_player_SendEvent(player, on_cork_changed, newval.i_int);

    vlc_player_Unlock(player);

    return VLC_SUCCESS;
    (void) this; (void) var;
}

audio_output_t *
vlc_player_aout_Hold(vlc_player_t *player)
{
    return input_resource_HoldAout(player->resource);
}

vlc_player_aout_listener_id *
vlc_player_aout_AddListener(vlc_player_t *player,
                            const struct vlc_player_aout_cbs *cbs,
                            void *cbs_data)
{
    assert(cbs);

    vlc_player_aout_listener_id *listener = malloc(sizeof(*listener));
    if (!listener)
        return NULL;

    listener->cbs = cbs;
    listener->cbs_data = cbs_data;

    vlc_mutex_lock(&player->aout_listeners_lock);
    vlc_list_append(&listener->node, &player->aout_listeners);
    vlc_mutex_unlock(&player->aout_listeners_lock);

    return listener;
}

void
vlc_player_aout_RemoveListener(vlc_player_t *player,
                               vlc_player_aout_listener_id *id)
{
    assert(id);

    vlc_mutex_lock(&player->aout_listeners_lock);
    vlc_list_remove(&id->node);
    vlc_mutex_unlock(&player->aout_listeners_lock);
    free(id);
}

static void
vlc_player_vout_OSDVolume(vlc_player_t *player, bool mute_action)
{
    size_t count;
    vout_thread_t **vouts = vlc_player_vout_OSDHoldAll(player, &count);

    bool mute = vlc_player_aout_IsMuted(player);
    int volume = lroundf(vlc_player_aout_GetVolume(player) * 100.f);
    if (mute_action && mute)
        vouts_osd_Icon(vouts, count, OSD_MUTE_ICON);
    else
    {
        if (vlc_player_vout_IsFullscreen(player))
            vouts_osd_Slider(vouts, count, volume, OSD_VERT_SLIDER);
        vouts_osd_Message(vouts, count, _("Volume: %ld%%"), volume);
    }

    vlc_player_vout_OSDReleaseAll(player, vouts, count);
}

static int
vlc_player_AoutCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vlc_player_t *player = data;

    if (strcmp(var, "volume") == 0)
    {
        if (oldval.f_float != newval.f_float)
        {
            vlc_player_aout_SendEvent(player, on_volume_changed, newval.f_float);
            vlc_player_vout_OSDVolume(player, false);
        }
    }
    else if (strcmp(var, "mute") == 0)
    {
        if (oldval.b_bool != newval.b_bool)
        {
            vlc_player_aout_SendEvent(player, on_mute_changed, newval.b_bool);
            vlc_player_vout_OSDVolume(player, true);
        }
    }
    else
        vlc_assert_unreachable();

    return VLC_SUCCESS;
    (void) this;
}

float
vlc_player_aout_GetVolume(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1.f;
    float vol = aout_VolumeGet(aout);
    aout_Release(aout);

    return vol;
}

int
vlc_player_aout_SetVolume(vlc_player_t *player, float volume)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_VolumeSet(aout, volume);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_IncrementVolume(vlc_player_t *player, int steps, float *result)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_VolumeUpdate(aout, steps, result);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_IsMuted(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_MuteGet(aout);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_Mute(vlc_player_t *player, bool mute)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_MuteSet (aout, mute);
    aout_Release(aout);

    return ret;
}


int
vlc_player_aout_EnableFilter(vlc_player_t *player, const char *name, bool add)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    aout_EnableFilter(aout, name, add);
    aout_Release(aout);

    return 0;
}

vout_thread_t *
vlc_player_vout_Hold(vlc_player_t *player)
{
    return input_resource_HoldVout(player->resource);
}

vout_thread_t **
vlc_player_vout_HoldAll(vlc_player_t *player, size_t *count)
{
    vout_thread_t **vouts;
    input_resource_HoldVouts(player->resource, &vouts, count);
    return vouts;
}

vlc_player_vout_listener_id *
vlc_player_vout_AddListener(vlc_player_t *player,
                            const struct vlc_player_vout_cbs *cbs,
                            void *cbs_data)
{
    assert(cbs);

    vlc_player_vout_listener_id *listener = malloc(sizeof(*listener));
    if (!listener)
        return NULL;

    listener->cbs = cbs;
    listener->cbs_data = cbs_data;

    vlc_mutex_lock(&player->vout_listeners_lock);
    vlc_list_append(&listener->node, &player->vout_listeners);
    vlc_mutex_unlock(&player->vout_listeners_lock);

    return listener;
}

void
vlc_player_vout_RemoveListener(vlc_player_t *player,
                               vlc_player_vout_listener_id *id)
{
    assert(id);

    vlc_mutex_lock(&player->vout_listeners_lock);
    vlc_list_remove(&id->node);
    vlc_mutex_unlock(&player->vout_listeners_lock);
    free(id);
}

bool
vlc_player_vout_IsFullscreen(vlc_player_t *player)
{
    return var_GetBool(player, "fullscreen");
}

static int
vlc_player_VoutCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vlc_player_t *player = data;

    if (strcmp(var, "fullscreen") == 0)
    {
        if (oldval.b_bool != newval.b_bool )
            vlc_player_vout_SendEvent(player, on_fullscreen_changed,
                                      (vout_thread_t *)this, newval.b_bool);
    }
    else if (strcmp(var, "video-wallpaper") == 0)
    {
        if (oldval.b_bool != newval.b_bool )
            vlc_player_vout_SendEvent(player, on_wallpaper_mode_changed,
                                      (vout_thread_t *)this, newval.b_bool);
    }
    else
        vlc_assert_unreachable();

    return VLC_SUCCESS;
}

static bool
vout_osd_PrintVariableText(vout_thread_t *vout, const char *varname, int vartype,
                           vlc_value_t varval, const char *osdfmt)
{
    bool found = false;
    bool isvarstring = vartype == VLC_VAR_STRING;
    size_t num_choices;
    vlc_value_t *choices;
    char **choices_text;
    var_Change(vout, varname, VLC_VAR_GETCHOICES,
               &num_choices, &choices, &choices_text);
    for (size_t i = 0; i < num_choices; ++i)
    {
        if (!found)
            if ((isvarstring &&
                 strcmp(choices[i].psz_string, varval.psz_string) == 0) ||
                (!isvarstring && choices[i].f_float == varval.f_float))
            {
                vouts_osd_Message(&vout, 1, osdfmt, choices_text[i]);
                found = true;
            }
        if (isvarstring)
            free(choices[i].psz_string);
        free(choices_text[i]);
    }
    free(choices);
    free(choices_text);
    return found;
}

static int
vlc_player_VoutOSDCallback(vlc_object_t *this, const char *var,
                           vlc_value_t oldval, vlc_value_t newval, void *data)
{
    VLC_UNUSED(oldval);

    vout_thread_t *vout = (vout_thread_t *)this;

    if (strcmp(var, "aspect-ratio") == 0)
        vout_osd_PrintVariableText(vout, var, VLC_VAR_STRING,
                                   newval, _("Aspect ratio: %s"));

    else if (strcmp(var, "autoscale") == 0)
        vouts_osd_Message(&vout, 1, newval.b_bool ?
                          _("Scaled to screen") : _("Original size"));

    else if (strcmp(var, "crop") == 0)
        vout_osd_PrintVariableText(vout, var, VLC_VAR_STRING, newval,
                                   _("Crop: %s"));

    else if (strcmp(var, "crop-bottom") == 0)
        vouts_osd_Message(&vout, 1, _("Bottom crop: %d px"), newval.i_int);

    else if (strcmp(var, "crop-top") == 0)
        vouts_osd_Message(&vout, 1, _("Top crop: %d px"), newval.i_int);

    else if (strcmp(var, "crop-left") == 0)
        vouts_osd_Message(&vout, 1, _("Left crop: %d px"), newval.i_int);

    else if (strcmp(var, "crop-right") == 0)
        vouts_osd_Message(&vout, 1, _("Right crop: %d px"), newval.i_int);

    else if (strcmp(var, "deinterlace") == 0 ||
             strcmp(var, "deinterlace-mode") == 0)
    {
        bool varmode = strcmp(var, "deinterlace-mode") == 0;
        int on = !varmode ?
            newval.i_int : var_GetInteger(vout, "deinterlace");
        char *mode = varmode ?
            newval.psz_string : var_GetString(vout, "deinterlace-mode");
        vouts_osd_Message(&vout, 1, _("Deinterlace %s (%s)"),
                          on == 1 ? _("On") : _("Off"), mode);
        free(mode);
    }

    else if (strcmp(var, "sub-margin") == 0)
        vouts_osd_Message(&vout, 1, _("Subtitle position %d px"), newval.i_int);

    else if (strcmp(var, "sub-text-scale") == 0)
        vouts_osd_Message(&vout, 1, _("Subtitle text scale %d%%"), newval.i_int);

    else if (strcmp(var, "zoom") == 0)
    {
        if (newval.f_float == 1.f)
            vouts_osd_Message(&vout, 1, _("Zooming reset"));
        else
        {
            bool found =  vout_osd_PrintVariableText(vout, var, VLC_VAR_FLOAT,
                                                     newval, _("Zoom mode: %s"));
            if (!found)
                vouts_osd_Message(&vout, 1, _("Zoom: x%f"), newval.f_float);
        }
    }

    (void) data;
    return VLC_SUCCESS;
}

static void
vlc_player_vout_SetVar(vlc_player_t *player, const char *name, int type,
                       vlc_value_t val)
{
    var_SetChecked(player, name, type, val);

    size_t count;
    vout_thread_t **vouts = vlc_player_vout_HoldAll(player, &count);
    for (size_t i = 0; i < count; i++)
    {
        var_SetChecked(vouts[i], name, type, val);
        vout_Release(vouts[i]);
    }
    free(vouts);
}


static void
vlc_player_vout_TriggerOption(vlc_player_t *player, const char *option)
{
    size_t count;
    vout_thread_t **vouts = vlc_player_vout_HoldAll(player, &count);
    for (size_t i = 0; i < count; ++i)
    {
        var_TriggerCallback(vouts[i], option);
        vout_Release(vouts[i]);
    }
    free(vouts);
}

vlc_object_t *
vlc_player_GetV4l2Object(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input && var_Type(input->thread, "controls") != 0 ?
           (vlc_object_t*) input->thread : NULL;
}

void
vlc_player_SetVideoSplitter(vlc_player_t *player, const char *splitter)
{
    if (config_GetType("video-splitter") == 0)
        return;
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    vout_thread_t *vout = vlc_player_vout_Hold(player);
    var_SetString(vout, "video-splitter", splitter);
    vout_Release(vout);

    /* FIXME vout cannot handle live video splitter change, restart the main
     * vout manually by restarting the first video es */
    struct vlc_player_track *track;
    vlc_vector_foreach(track, &input->video_track_vector)
        if (track->selected)
        {
            vlc_player_RestartTrack(player, track->es_id);
            break;
        }
}

void
vlc_player_vout_SetFullscreen(vlc_player_t *player, bool enabled)
{
    vlc_player_vout_SetVar(player, "fullscreen", VLC_VAR_BOOL,
                           (vlc_value_t) { .b_bool = enabled });
    vlc_player_vout_SendEvent(player, on_fullscreen_changed, NULL, enabled);
}

bool
vlc_player_vout_IsWallpaperModeEnabled(vlc_player_t *player)
{
    return var_GetBool(player, "video-wallpaper");
}

void
vlc_player_vout_SetWallpaperModeEnabled(vlc_player_t *player, bool enabled)
{
    vlc_player_vout_SetVar(player, "video-wallpaper", VLC_VAR_BOOL,
                           (vlc_value_t) { .b_bool = enabled });
    vlc_player_vout_SendEvent(player, on_wallpaper_mode_changed, NULL, enabled);
}

static const char *
vlc_vout_filter_type_to_varname(enum vlc_vout_filter_type type)
{
    switch (type)
    {
        case VLC_VOUT_FILTER_VIDEO_FILTER:
            return "video-filter";
        case VLC_VOUT_FILTER_SUB_SOURCE:
            return "sub-source";
        case VLC_VOUT_FILTER_SUB_FILTER:
            return "sub-filter";
        default:
            vlc_assert_unreachable();
    }
}

void
vlc_player_vout_SetFilter(vlc_player_t *player, enum vlc_vout_filter_type type,
                          const char *value)
{
    const char *varname = vlc_vout_filter_type_to_varname(type);
    if (varname)
        vlc_player_vout_SetVar(player, varname, VLC_VAR_STRING,
                               (vlc_value_t) { .psz_string = (char *) value });
}

char *
vlc_player_vout_GetFilter(vlc_player_t *player, enum vlc_vout_filter_type type)
{
    const char *varname = vlc_vout_filter_type_to_varname(type);
    return varname ? var_GetString(player, varname) : NULL;
}

void
vlc_player_vout_Snapshot(vlc_player_t *player)
{
    vlc_player_vout_TriggerOption(player, "video-snapshot");
}

static void
vlc_player_InitLocks(vlc_player_t *player)
{
    vlc_mutex_init(&player->lock);
    vlc_mutex_init(&player->vout_listeners_lock);
    vlc_mutex_init(&player->aout_listeners_lock);
    vlc_cond_init(&player->start_delay_cond);
    vlc_cond_init(&player->destructor.wait);
}

static void
vlc_player_DestroyLocks(vlc_player_t *player)
{
    vlc_mutex_destroy(&player->lock);
    vlc_mutex_destroy(&player->vout_listeners_lock);
    vlc_mutex_destroy(&player->aout_listeners_lock);
    vlc_cond_destroy(&player->start_delay_cond);
    vlc_cond_destroy(&player->destructor.wait);
}

void
vlc_player_Delete(vlc_player_t *player)
{
    vlc_mutex_lock(&player->lock);

    if (player->input)
        vlc_player_destructor_AddInput(player, player->input);

    player->deleting = true;
    vlc_cond_signal(&player->destructor.wait);

    assert(vlc_list_is_empty(&player->listeners));

    vlc_mutex_unlock(&player->lock);

    vlc_join(player->destructor.thread, NULL);

    if (player->media)
        input_item_Release(player->media);
    if (player->next_media)
        input_item_Release(player->next_media);

    vlc_player_DestroyLocks(player);

    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (aout)
    {
        var_DelCallback(aout, "volume", vlc_player_AoutCallback, player);
        var_DelCallback(aout, "mute", vlc_player_AoutCallback, player);
        var_DelCallback(player, "corks", vlc_player_CorkCallback, NULL);
        aout_Release(aout);
    }
    input_resource_Release(player->resource);
    if (player->renderer)
        vlc_renderer_item_release(player->renderer);

    vlc_object_delete(player);
}

vlc_player_t *
vlc_player_New(vlc_object_t *parent,
               const struct vlc_player_media_provider *media_provider,
               void *media_provider_data)
{
    audio_output_t *aout = NULL;
    vlc_player_t *player = vlc_custom_create(parent, sizeof(*player), "player");
    if (!player)
        return NULL;

    assert(!media_provider || media_provider->get_next);

    vlc_list_init(&player->listeners);
    vlc_list_init(&player->vout_listeners);
    vlc_list_init(&player->aout_listeners);
    vlc_list_init(&player->destructor.inputs);
    vlc_list_init(&player->destructor.stopping_inputs);
    vlc_list_init(&player->destructor.joinable_inputs);
    player->media_stopped_action = VLC_PLAYER_MEDIA_STOPPED_CONTINUE;
    player->start_paused = false;
    player->pause_on_cork = false;
    player->corked = false;
    player->renderer = NULL;
    player->media_provider = media_provider;
    player->media_provider_data = media_provider_data;
    player->media = NULL;
    player->input = NULL;
    player->global_state = VLC_PLAYER_STATE_STOPPED;
    player->started = false;

    player->error_count = 0;

    player->releasing_media = false;
    player->next_media_requested = false;
    player->next_media = NULL;

#define VAR_CREATE(var, flag) do { \
    if (var_Create(player, var, flag) != VLC_SUCCESS) \
        goto error; \
} while(0)

    /* player variables */
    VAR_CREATE("rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-text-scale", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    VAR_CREATE("demux-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

    /* vout variables */
    if (config_GetType("video-splitter"))
        VAR_CREATE("video-splitter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("video-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-source", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("fullscreen", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("video-wallpaper", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);

    /* aout variables */
    VAR_CREATE("audio-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("mute", VLC_VAR_BOOL);
    VAR_CREATE("corks", VLC_VAR_INTEGER);

    /* es_out variables */
    VAR_CREATE("sout", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("sout-video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("sout-audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    VAR_CREATE("sout-spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);

    /* TODO: Override these variables since the player handle media ended
     * action itself. */
    VAR_CREATE("start-paused", VLC_VAR_BOOL);
    VAR_CREATE("play-and-pause", VLC_VAR_BOOL);

#undef VAR_CREATE

    player->resource = input_resource_New(VLC_OBJECT(player));

    if (!player->resource)
        goto error;


    aout = input_resource_GetAout(player->resource);
    if (aout != NULL)
    {
        var_AddCallback(aout, "volume", vlc_player_AoutCallback, player);
        var_AddCallback(aout, "mute", vlc_player_AoutCallback, player);
        var_AddCallback(player, "corks", vlc_player_CorkCallback, NULL);
        input_resource_PutAout(player->resource, aout);
    }

    player->deleting = false;
    vlc_player_InitLocks(player);

    if (vlc_clone(&player->destructor.thread, vlc_player_destructor_Thread,
                  player, VLC_THREAD_PRIORITY_LOW) != 0)
    {
        vlc_player_DestroyLocks(player);
        goto error;
    }

    return player;

error:
    if (aout)
    {
        var_DelCallback(aout, "volume", vlc_player_AoutCallback, player);
        var_DelCallback(aout, "mute", vlc_player_AoutCallback, player);
        var_DelCallback(player, "corks", vlc_player_AoutCallback, NULL);
    }
    if (player->resource)
        input_resource_Release(player->resource);

    vlc_object_delete(player);
    return NULL;
}
