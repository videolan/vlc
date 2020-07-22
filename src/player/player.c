/*****************************************************************************
 * player.c: Player implementation
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>

#include <vlc_common.h>
#include "player.h"
#include <vlc_aout.h>
#include <vlc_renderer_discovery.h>
#include <vlc_tick.h>
#include <vlc_decoder.h>
#include <vlc_memstream.h>

#include "libvlc.h"
#include "input/resource.h"
#include "audio_output/aout_internal.h"

static_assert(VLC_PLAYER_CAP_SEEK == VLC_INPUT_CAPABILITIES_SEEKABLE &&
              VLC_PLAYER_CAP_PAUSE == VLC_INPUT_CAPABILITIES_PAUSEABLE &&
              VLC_PLAYER_CAP_CHANGE_RATE == VLC_INPUT_CAPABILITIES_CHANGE_RATE &&
              VLC_PLAYER_CAP_REWIND == VLC_INPUT_CAPABILITIES_REWINDABLE,
              "player/input capabilities mismatch");

static_assert(VLC_PLAYER_TITLE_MENU == INPUT_TITLE_MENU &&
              VLC_PLAYER_TITLE_INTERACTIVE == INPUT_TITLE_INTERACTIVE,
              "player/input title flag mismatch");

#define vlc_player_foreach_inputs(it) \
    for (struct vlc_player_input *it = player->input; it != NULL; it = NULL)

void
vlc_player_PrepareNextMedia(vlc_player_t *player)
{
    vlc_player_assert_locked(player);

    if (!player->media_provider
     || player->media_stopped_action == VLC_PLAYER_MEDIA_STOPPED_STOP
     || player->next_media_requested)
        return;

    assert(player->next_media == NULL);
    player->next_media =
        player->media_provider->get_next(player, player->media_provider_data);
    player->next_media_requested = true;
}

int
vlc_player_OpenNextMedia(vlc_player_t *player)
{
    assert(player->input == NULL);

    player->next_media_requested = false;

    /* Tracks string ids are only remembered for one media */
    free(player->video_string_ids);
    free(player->audio_string_ids);
    free(player->sub_string_ids);
    player->video_string_ids = player->audio_string_ids =
    player->sub_string_ids = NULL;

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

        struct vlc_player_input *input = player->input =
            vlc_player_input_New(player, player->media);
        if (!input)
        {
            input_item_Release(player->media);
            player->media = NULL;
            ret = VLC_ENOMEM;
        }
    }
    vlc_player_SendEvent(player, on_current_media_changed, player->media);
    if (player->input && player->input->ml.delay_restore)
    {
        vlc_player_SendEvent(player, on_playback_restore_queried);
        player->input->ml.delay_restore = false;
    }
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

void
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

void
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
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING,
                                         VLC_TICK_INVALID);
            vlc_player_destructor_AddStoppingInput(player, input);

            vlc_player_UpdateMLStates(player, input);
            input_Stop(input->thread);
        }

        bool keep_sout = true;
        const bool inputs_changed =
            !vlc_list_is_empty(&player->destructor.joinable_inputs);
        vlc_list_foreach(input, &player->destructor.joinable_inputs, node)
        {
            keep_sout = var_GetBool(input->thread, "sout-keep");

            if (input->state == VLC_PLAYER_STATE_STOPPING)
                vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPED,
                                             VLC_TICK_INVALID);

            vlc_list_remove(&input->node);
            vlc_player_input_Delete(input);
        }

        if (inputs_changed)
        {
            const bool started = player->started;
            vlc_player_Unlock(player);
            if (!started)
                input_resource_StopFreeVout(player->resource);
            if (!keep_sout)
                input_resource_TerminateSout(player->resource);
            vlc_player_Lock(player);
        }
    }
    vlc_mutex_unlock(&player->lock);
    return NULL;
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

void
vlc_player_SelectProgram(vlc_player_t *player, int id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    const struct vlc_player_program *prgm =
        vlc_player_program_vector_FindById(&input->program_vector,
                                           id, NULL);
    if (!prgm)
        return;
    int ret = input_ControlPushHelper(input->thread,
                                      INPUT_CONTROL_SET_PROGRAM,
                                      &(vlc_value_t) { .i_int = id });
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Program(player, prgm->name);
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
    return &vec->data[index]->t;
}

static struct vlc_player_track_priv *
vlc_player_GetPrivTrack(vlc_player_t *player, vlc_es_id_t *id)

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

const struct vlc_player_track *
vlc_player_GetTrack(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_track_priv *trackpriv =
        vlc_player_GetPrivTrack(player, id);
    return trackpriv ? &trackpriv->t : NULL;
}

vout_thread_t *
vlc_player_GetEsIdVout(vlc_player_t *player, vlc_es_id_t *es_id,
                       enum vlc_vout_order *order)
{
    struct vlc_player_track_priv *trackpriv =
        vlc_player_GetPrivTrack(player, es_id);
    if (trackpriv)
    {
        if (order)
            *order = trackpriv->vout_order;
        return trackpriv->vout;
    }
    return NULL;
}

vlc_es_id_t *
vlc_player_GetEsIdFromVout(vlc_player_t *player, vout_thread_t *vout)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (!input)
        return NULL;

    static const enum es_format_category_e cats[] = {
        VIDEO_ES, SPU_ES, AUDIO_ES /* for visualisation filters */
    };
    for (size_t i = 0; i < ARRAY_SIZE(cats); ++i)
    {
        enum es_format_category_e cat = cats[i];
        vlc_player_track_vector *vec =
            vlc_player_input_GetTrackVector(input, cat);
        for (size_t j = 0; j < vec->size; ++j)
        {
            struct vlc_player_track_priv *trackpriv = vec->data[j];
            if (trackpriv->vout == vout)
                return trackpriv->t.es_id;
        }
    }
    return NULL;
}

unsigned
vlc_player_SelectEsIdList(vlc_player_t *player,
                          enum es_format_category_e cat,
                          vlc_es_id_t *const es_id_list[])
{
    static const size_t max_tracks_by_cat[] = {
        [UNKNOWN_ES] = 0,
        [VIDEO_ES] = UINT_MAX,
        [AUDIO_ES] = 1,
        [SPU_ES] = 2,
        [DATA_ES] = 0,
    };

    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return 0;

    const size_t max_tracks = max_tracks_by_cat[cat];

    if (max_tracks == 0)
        return 0;

    /* First, count and hold all the ES Ids.
       Ids will be released in input.c:ControlRelease */
    size_t track_count = 0;
    for (size_t i = 0; es_id_list[i] != NULL && track_count < max_tracks; i++)
        if (vlc_es_id_GetCat(es_id_list[i]) == cat)
            track_count++;

    /* Copy es_id_list into an allocated list so that it remains in memory until
       selection completes. The list will be freed in input.c:ControlRelease */
    struct vlc_es_id_t **allocated_ids =
        vlc_alloc(track_count + 1, sizeof(vlc_es_id_t *));

    if (allocated_ids == NULL)
        return 0;

    track_count = 0;
    for (size_t i = 0; es_id_list[i] != NULL && track_count < max_tracks; i++)
    {
        vlc_es_id_t *es_id = es_id_list[i];
        if (vlc_es_id_GetCat(es_id_list[i]) == cat)
        {
            vlc_es_id_Hold(es_id);
            allocated_ids[track_count++] = es_id;
        }
    }
    allocated_ids[track_count] = NULL;

    /* Attempt to select all the requested tracks */
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_ES_LIST,
        &(input_control_param_t) {
            .list.cat = cat,
            .list.ids = allocated_ids,
        });
    if (ret != VLC_SUCCESS)
        return 0;

    /* Display track selection message */
    const char *cat_name = es_format_category_to_string(cat);
    if (track_count == 0)
        vlc_player_osd_Message(player, _("%s track: %s"), cat_name, _("N/A"));
    else if (track_count == 1)
        vlc_player_osd_Track(player, es_id_list[0], true);
    else
    {
        struct vlc_memstream stream;
        vlc_memstream_open(&stream);
        for (size_t i = 0; i < track_count; i++)
        {
            const struct vlc_player_track *track =
                vlc_player_GetTrack(player, es_id_list[i]);

            if (track)
            {
                if (i != 0)
                    vlc_memstream_puts(&stream, ", ");
                vlc_memstream_puts(&stream, track->name);
            }
        }
        if (vlc_memstream_close(&stream) == 0)
        {
            vlc_player_osd_Message(player, _("%s tracks: %s"), cat_name,
                                   stream.ptr);
            free(stream.ptr);
        }
    }
    return track_count;
}

/* Returns an array of selected tracks, putting id in first position (if any).
 * */
static vlc_es_id_t **
vlc_player_GetEsIdList(vlc_player_t *player,
                       const enum es_format_category_e cat,
                       vlc_es_id_t *id)
{
    const size_t track_count = vlc_player_GetTrackCount(player, cat);
    if (track_count == 0)
        return NULL;

    size_t selected_track_count = id ? 1 : 0;
    for (size_t i = 0; i < track_count; ++i)
    {
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt(player, cat, i);
        if (track->selected && track->es_id != id)
            selected_track_count++;
    }

    vlc_es_id_t **es_id_list =
        vlc_alloc(selected_track_count + 1 /* NULL */, sizeof(vlc_es_id_t*));
    if (!es_id_list)
        return NULL;

    size_t es_id_list_idx = 0;
    /* Assure to select the requested track */
    if (id)
        es_id_list[es_id_list_idx++] = id;

    for (size_t i = 0; i < track_count; ++i)
    {
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt(player, cat, i);
        if (track->selected && track->es_id != id)
            es_id_list[es_id_list_idx++] = track->es_id;
    }
    es_id_list[selected_track_count] = NULL;

    return es_id_list;
}

unsigned
vlc_player_SelectEsId(vlc_player_t *player, vlc_es_id_t *id,
                      enum vlc_player_select_policy policy)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return 0;

    if (policy == VLC_PLAYER_SELECT_EXCLUSIVE)
    {
        if (input_ControlPushEsHelper(input->thread, INPUT_CONTROL_SET_ES, id)
         == VLC_SUCCESS)
            vlc_player_osd_Track(player, id, true);
        return 1;
    }

    /* VLC_PLAYER_SELECT_SIMULTANEOUS */
    const enum es_format_category_e cat = vlc_es_id_GetCat(id);
    vlc_es_id_t **es_id_list = vlc_player_GetEsIdList(player, cat, id);
    if (!es_id_list)
        return 0;

    unsigned ret = vlc_player_SelectEsIdList(player, cat, es_id_list);
    free(es_id_list);
    return ret;
}

void
vlc_player_SelectTracksByStringIds(vlc_player_t *player,
                                   enum es_format_category_e cat,
                                   const char *str_ids)
{
    vlc_player_assert_locked(player);
    char **cat_str_ids;

    switch (cat)
    {
        case VIDEO_ES: cat_str_ids = &player->video_string_ids; break;
        case AUDIO_ES: cat_str_ids = &player->audio_string_ids; break;
        case SPU_ES:   cat_str_ids = &player->sub_string_ids; break;
        default: return;
    }

    free(*cat_str_ids);
    *cat_str_ids = str_ids ? strdup(str_ids) : NULL;

    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input)
        vlc_player_input_SelectTracksByStringIds(input, cat, str_ids);
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
        vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
    else
        vlc_player_UnselectTrack(player, track);
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
vlc_player_UnselectEsId(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;

    int ret = input_ControlPushEsHelper(input->thread, INPUT_CONTROL_UNSET_ES,
                                        id);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Track(player, id, false);
}

void
vlc_player_RestartEsId(vlc_player_t *player, vlc_es_id_t *id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    if (input)
        input_ControlPushEsHelper(input->thread, INPUT_CONTROL_RESTART_ES, id);
}

void
vlc_player_SelectCategoryLanguage(vlc_player_t *player,
                                  enum es_format_category_e cat,
                                  const char *lang)
{
    vlc_player_assert_locked(player);
    switch (cat)
    {
        case AUDIO_ES:
            var_SetString(player, "audio-language", lang);
            break;
        case SPU_ES:
            var_SetString(player, "sub-language", lang);
            break;
        default:
            vlc_assert_unreachable();
    }
}

char *
vlc_player_GetCategoryLanguage(vlc_player_t *player,
                               enum es_format_category_e cat)
{
    vlc_player_assert_locked(player);
    switch (cat)
    {
        case AUDIO_ES:
            return var_GetString(player, "audio-language");
        case SPU_ES:
            return var_GetString(player, "sub-language");
        default:
            vlc_assert_unreachable();
    }
}

void
vlc_player_SetTeletextEnabled(vlc_player_t *player, bool enabled)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_source)
        return;
    if (enabled)
        vlc_player_SelectEsId(player, input->teletext_source->t.es_id,
                              VLC_PLAYER_SELECT_EXCLUSIVE);
    else
        vlc_player_UnselectEsId(player, input->teletext_source->t.es_id);
}

void
vlc_player_SelectTeletextPage(vlc_player_t *player, unsigned page)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_source)
        return;
    input_ControlPush(input->thread, INPUT_CONTROL_SET_VBI_PAGE,
        &(input_control_param_t) {
            .vbi_page.id = input->teletext_source->t.es_id,
            .vbi_page.page = page,
    });
}

void
vlc_player_SetTeletextTransparency(vlc_player_t *player, bool enabled)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input || !input->teletext_source)
        return;

    input_ControlPush(input->thread, INPUT_CONTROL_SET_VBI_TRANSPARENCY,
        &(input_control_param_t) {
            .vbi_transparency.id = input->teletext_source->t.es_id,
            .vbi_transparency.enabled = enabled,
    });
}

bool
vlc_player_HasTeletextMenu(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input && input->teletext_source;
}

bool
vlc_player_IsTeletextEnabled(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (input && input->teletext_enabled)
    {
        assert(input->teletext_source);
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
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_TITLE_NEXT,
                                NULL);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Next title"));
}

void
vlc_player_SelectPrevTitle(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_TITLE_PREV,
                                NULL);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Previous title"));
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
    int ret = input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_SEEKPOINT,
                                      &(vlc_value_t){ .i_int = index });
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Chapter %ld"), index);
}

void
vlc_player_SelectNextChapter(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_SEEKPOINT_NEXT,
                                NULL);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Next chapter"));
}

void
vlc_player_SelectPrevChapter(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_SEEKPOINT_PREV,
                                NULL);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Previous chapter"));
}

void
vlc_player_Lock(vlc_player_t *player)
{
    /* Vout and aout locks should not be held, cf. vlc_player_vout_cbs and
     * vlc_player_aout_cbs documentation */
    assert(!vlc_mutex_held(&player->vout_listeners_lock));
    assert(!vlc_mutex_held(&player->aout_listeners_lock));
    /* The timer lock should not be held (possible lock-order-inversion), cf.
     * vlc_player_timer_cbs.on_update documentation */
    assert(!vlc_mutex_held(&player->timer.lock));

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

    if (!input || !uri)
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

    if (check_ext && type == SLAVE_TYPE_SPU && !subtitles_Filter(uri))
        return VLC_EGENERIC;

    input_item_slave_t *slave =
        input_item_slave_New(uri, type, SLAVE_PRIORITY_USER);
    if (!slave)
        return VLC_ENOMEM;
    slave->b_forced = select;

    vlc_value_t val = { .p_address = slave };
    int ret = input_ControlPushHelper(input->thread, INPUT_CONTROL_ADD_SLAVE,
                                      &val);
    if (ret != VLC_SUCCESS)
        return ret;

    if (notify)
    {
        switch( type )
        {
            case SLAVE_TYPE_AUDIO:
                vlc_player_osd_Message(player, "%s",
                                       vlc_gettext("Audio track added"));
                break;
            case SLAVE_TYPE_SPU:
                vlc_player_osd_Message(player, "%s",
                                       vlc_gettext("Subtitle track added"));
                break;
        }
    }
    return VLC_SUCCESS;
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
    {
        player->started = true;
        vlc_player_osd_Icon(player, OSD_PLAY_ICON);
    }
    return ret;
}

int
vlc_player_Stop(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    vlc_player_CancelWaitError(player);

    vlc_player_InvalidateNextMedia(player);

    if (!input || !player->started)
        return VLC_EGENERIC;
    player->started = false;

    vlc_player_destructor_AddInput(player, input);
    player->input = NULL;
    return VLC_SUCCESS;
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
    int ret = input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_STATE,
                                      &val);

    if (ret == VLC_SUCCESS)
        vlc_player_osd_Icon(player, pause ? OSD_PAUSE_ICON : OSD_PLAY_ICON);
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
    int ret = input_ControlPushHelper(input->thread,
                                      INPUT_CONTROL_SET_FRAME_NEXT, NULL);
    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, _("Next frame"));
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

    /* The event is sent from the thread processing the control */
    if (input
     && input_ControlPushHelper(input->thread, INPUT_CONTROL_SET_RATE,
                                &(vlc_value_t) { .f_float = rate }) == VLC_SUCCESS)
        vlc_player_osd_Message(player, ("Speed: %.2fx"), rate);
    else /* Send the event anyway since it's a global state */
        vlc_player_SendEvent(player, on_rate_changed, rate);

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

    if (!input)
        return VLC_TICK_INVALID;

    return vlc_player_input_GetTime(input);
}

float
vlc_player_GetPosition(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);

    return input ? vlc_player_input_GetPos(input) : -1.f;
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

void
vlc_player_DisplayPosition(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return;
    vlc_player_osd_Position(player, input,
                                vlc_player_input_GetTime(input),
                                vlc_player_input_GetPos(input),
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
    int ret = input_ControlPush(input->thread, type,
        &(input_control_param_t) {
            .pos.f_val = position,
            .pos.b_fast_seek = speed == VLC_PLAYER_SEEK_FAST,
    });

    if (ret == VLC_SUCCESS)
        vlc_player_osd_Position(player, input, VLC_TICK_INVALID, position,
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
    int ret = input_ControlPush(input->thread, type,
        &(input_control_param_t) {
            .time.i_val = time,
            .time.b_fast_seek = speed == VLC_PLAYER_SEEK_FAST,
    });

    if (ret == VLC_SUCCESS)
        vlc_player_osd_Position(player, input, time, -1, whence);
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
    {
        input_control_param_t param = { .viewpoint = *viewpoint };
        if (whence == VLC_PLAYER_WHENCE_ABSOLUTE)
            input_ControlPush(input->thread, INPUT_CONTROL_SET_VIEWPOINT,
                              &param);
        else
            input_ControlPush(input->thread, INPUT_CONTROL_UPDATE_VIEWPOINT,
                              &param);
    }
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
    int ret = input_ControlPushHelper(input->thread,
                                      INPUT_CONTROL_SET_RECORD_STATE,
                                      &(vlc_value_t) { .b_bool = enable });

    if (ret == VLC_SUCCESS)
        vlc_player_osd_Message(player, enable ?
                               _("Recording") : _("Recording done"));
}

int
vlc_player_SetCategoryDelay(vlc_player_t *player, enum es_format_category_e cat,
                            vlc_tick_t delay, enum vlc_player_whence whence)
{
    bool absolute = whence == VLC_PLAYER_WHENCE_ABSOLUTE;
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return VLC_EGENERIC;

    if (cat != AUDIO_ES && cat != SPU_ES)
        return VLC_EGENERIC;
    vlc_tick_t *cat_delay = &input->cat_delays[cat];

    if (absolute)
        *cat_delay = delay;
    else
    {
        *cat_delay += delay;
        delay = *cat_delay;
    }

    const input_control_param_t param = { .cat_delay = { cat, delay } };
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_CATEGORY_DELAY,
                                &param);
    if (ret == VLC_SUCCESS)
    {
        vlc_player_osd_Message(player, _("%s delay: %i ms"),
                               es_format_category_to_string(cat),
                               (int)MS_FROM_VLC_TICK(delay));
        vlc_player_SendEvent(player, on_category_delay_changed, cat, delay);
    }
    return VLC_SUCCESS;
}

vlc_tick_t
vlc_player_GetCategoryDelay(vlc_player_t *player, enum es_format_category_e cat)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return 0;

    if (cat != AUDIO_ES && cat != SPU_ES)
        return 0;

    return input->cat_delays[cat];
}

int
vlc_player_SetEsIdDelay(vlc_player_t *player, vlc_es_id_t *es_id,
                        vlc_tick_t delay, enum vlc_player_whence whence)
{
    bool absolute = whence == VLC_PLAYER_WHENCE_ABSOLUTE;
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return VLC_EGENERIC;

    struct vlc_player_track_priv *trackpriv =
        vlc_player_input_FindTrackById(input, es_id, NULL);
    if (trackpriv == NULL ||
        (trackpriv->t.fmt.i_cat != AUDIO_ES && trackpriv->t.fmt.i_cat != SPU_ES))
        return VLC_EGENERIC;

    if (absolute)
        trackpriv->delay = delay;
    else
    {
        if (trackpriv->delay == INT64_MAX)
            trackpriv->delay = 0;
        trackpriv->delay += delay;
        delay = trackpriv->delay;
    }

    const input_control_param_t param = { .es_delay = { es_id, delay } };
    int ret = input_ControlPush(input->thread, INPUT_CONTROL_SET_ES_DELAY,
                                &param);
    if (ret == VLC_SUCCESS)
    {
        if (delay != INT64_MAX)
            vlc_player_osd_Message(player, _("%s delay: %i ms"),
                                   trackpriv->t.name,
                                   (int)MS_FROM_VLC_TICK(delay));
        vlc_player_SendEvent(player, on_track_delay_changed, es_id, delay);
    }

    return VLC_SUCCESS;
}

vlc_tick_t
vlc_player_GetEsIdDelay(vlc_player_t *player, vlc_es_id_t *es_id)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    if (!input)
        return 0;

    struct vlc_player_track_priv *trackpriv =
        vlc_player_input_FindTrackById(input, es_id, NULL);
    return trackpriv ? trackpriv->delay : INT64_MAX;
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

vlc_object_t *
vlc_player_GetV4l2Object(vlc_player_t *player)
{
    struct vlc_player_input *input = vlc_player_get_input_locked(player);
    return input && var_Type(input->thread, "controls") != 0 ?
           (vlc_object_t*) input->thread : NULL;
}

static void
vlc_player_InitLocks(vlc_player_t *player, enum vlc_player_lock_type lock_type)
{
    if (lock_type == VLC_PLAYER_LOCK_REENTRANT)
        vlc_mutex_init_recursive(&player->lock);
    else
        vlc_mutex_init(&player->lock);

    vlc_mutex_init(&player->vout_listeners_lock);
    vlc_mutex_init(&player->aout_listeners_lock);
    vlc_cond_init(&player->start_delay_cond);
    vlc_cond_init(&player->destructor.wait);
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
    assert(vlc_list_is_empty(&player->vout_listeners));
    assert(vlc_list_is_empty(&player->aout_listeners));

    vlc_mutex_unlock(&player->lock);

    vlc_join(player->destructor.thread, NULL);

    if (player->media)
        input_item_Release(player->media);
    if (player->next_media)
        input_item_Release(player->next_media);

    free(player->video_string_ids);
    free(player->audio_string_ids);
    free(player->sub_string_ids);

    vlc_player_DestroyTimer(player);

    vlc_player_aout_Deinit(player);
    var_DelCallback(player, "corks", vlc_player_CorkCallback, player);

    input_resource_Release(player->resource);
    if (player->renderer)
        vlc_renderer_item_release(player->renderer);

    vlc_object_delete(player);
}

vlc_player_t *
vlc_player_New(vlc_object_t *parent, enum vlc_player_lock_type lock_type,
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

    player->video_string_ids = player->audio_string_ids =
    player->sub_string_ids = NULL;

#define VAR_CREATE(var, flag) do { \
    if (var_Create(player, var, flag) != VLC_SUCCESS) \
        goto error; \
} while(0)

    /* player variables */
    VAR_CREATE("rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-text-scale", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    VAR_CREATE("demux-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

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
    VAR_CREATE("audio-language", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    VAR_CREATE("sub-language", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

    /* TODO: Override these variables since the player handle media ended
     * action itself. */
    VAR_CREATE("start-paused", VLC_VAR_BOOL);
    VAR_CREATE("play-and-pause", VLC_VAR_BOOL);

#undef VAR_CREATE

    player->resource = input_resource_New(VLC_OBJECT(player));

    if (!player->resource)
        goto error;

    /* Ensure the player has a valid aout */
    aout = vlc_player_aout_Init(player);
    var_AddCallback(player, "corks", vlc_player_CorkCallback, player);

    player->deleting = false;
    vlc_player_InitLocks(player, lock_type);
    vlc_player_InitTimer(player);

    if (vlc_clone(&player->destructor.thread, vlc_player_destructor_Thread,
                  player, VLC_THREAD_PRIORITY_LOW) != 0)
    {
        vlc_player_DestroyTimer(player);
        goto error;
    }

    return player;

error:
    if (aout)
        vlc_player_aout_Deinit(player);
    var_DelCallback(player, "corks", vlc_player_CorkCallback, player);
    if (player->resource)
        input_resource_Release(player->resource);

    vlc_object_delete(player);
    return NULL;
}

vlc_object_t *
vlc_player_GetObject(vlc_player_t *player)
{
    return VLC_OBJECT(player);
}
