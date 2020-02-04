/*****************************************************************************
 * player_input.c: Player input implementation
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

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_memstream.h>
#include "player.h"

struct vlc_player_track_priv *
vlc_player_input_FindTrackById(struct vlc_player_input *input, vlc_es_id_t *id,
                               size_t *idx)
{
    vlc_player_track_vector *vec =
        vlc_player_input_GetTrackVector(input, vlc_es_id_GetCat(id));
    return vec ? vlc_player_track_vector_FindById(vec, id, idx) : NULL;
}

static void
vlc_player_input_HandleAtoBLoop(struct vlc_player_input *input, vlc_tick_t time,
                                float pos)
{
    vlc_player_t *player = input->player;

    if (player->input != input)
        return;

    assert(input->abloop_state[0].set && input->abloop_state[1].set);

    if (time != VLC_TICK_INVALID
     && input->abloop_state[0].time != VLC_TICK_INVALID
     && input->abloop_state[1].time != VLC_TICK_INVALID)
    {
        if (time >= input->abloop_state[1].time)
            vlc_player_SetTime(player, input->abloop_state[0].time);
    }
    else if (pos >= input->abloop_state[1].pos)
        vlc_player_SetPosition(player, input->abloop_state[0].pos);
}

vlc_tick_t
vlc_player_input_GetTime(struct vlc_player_input *input)
{
    vlc_player_t *player = input->player;
    vlc_tick_t ts;

    if (input == player->input
     && vlc_player_GetTimerPoint(player, vlc_tick_now(), &ts, NULL) == 0)
        return ts;
    return input->time;
}

float
vlc_player_input_GetPos(struct vlc_player_input *input)
{
    vlc_player_t *player = input->player;
    float pos;

    if (input == player->input
     && vlc_player_GetTimerPoint(player, vlc_tick_now(), NULL, &pos) == 0)
        return pos;
    return input->position;
}

static void
vlc_player_input_UpdateTime(struct vlc_player_input *input)
{
    if (input->abloop_state[0].set && input->abloop_state[1].set)
        vlc_player_input_HandleAtoBLoop(input, vlc_player_input_GetTime(input),
                                        vlc_player_input_GetPos(input));
}

int
vlc_player_input_Start(struct vlc_player_input *input)
{
    int ret = input_Start(input->thread);
    if (ret != VLC_SUCCESS)
        return ret;
    input->started = true;
    return ret;
}

static bool
vlc_player_WaitRetryDelay(vlc_player_t *player)
{
#define RETRY_TIMEOUT_BASE VLC_TICK_FROM_MS(100)
#define RETRY_TIMEOUT_MAX VLC_TICK_FROM_MS(3200)
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

void
vlc_player_input_HandleState(struct vlc_player_input *input,
                             enum vlc_player_state state, vlc_tick_t state_date)
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

            vlc_player_ResetTimer(player);

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

            vlc_player_UpdateTimerState(player, NULL,
                                        VLC_PLAYER_TIMER_STATE_DISCONTINUITY,
                                        VLC_TICK_INVALID);

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
        case VLC_PLAYER_STATE_PLAYING:
            input->pause_date = VLC_TICK_INVALID;
            /* fallthrough */
        case VLC_PLAYER_STATE_STARTED:
            if (player->started &&
                player->global_state == VLC_PLAYER_STATE_PLAYING)
                send_event = false;
            break;

        case VLC_PLAYER_STATE_PAUSED:
            assert(player->started && input->started);
            assert(state_date != VLC_TICK_INVALID);
            input->pause_date = state_date;

            vlc_player_UpdateTimerState(player, NULL,
                                        VLC_PLAYER_TIMER_STATE_PAUSED,
                                        input->pause_date);
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

static void
vlc_player_input_HandleStateEvent(struct vlc_player_input *input,
                                  input_state_e state, vlc_tick_t state_date)
{
    switch (state)
    {
        case OPENING_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STARTED,
                                         VLC_TICK_INVALID);
            break;
        case PLAYING_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_PLAYING,
                                         state_date);
            break;
        case PAUSE_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_PAUSED,
                                         state_date);
            break;
        case END_S:
            vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING,
                                         VLC_TICK_INVALID);
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

static const struct vlc_player_track_priv *
vlc_player_FindTeletextSource(const struct vlc_player_input *input,
                              const struct vlc_player_track_priv *exclude,
                              bool selected)
{
    const struct vlc_player_track_priv *t;
    vlc_vector_foreach(t, &input->spu_track_vector)
    {
        if (t->t.fmt.i_codec == VLC_CODEC_TELETEXT &&
           t != exclude &&
           t->t.selected == selected)
            return t;
    }
    return NULL;
}

static unsigned
vlc_player_input_TeletextUserPage(const struct vlc_player_track_priv *t)
{
    const uint8_t mag = t->t.fmt.subs.teletext.i_magazine;
    const uint8_t page = t->t.fmt.subs.teletext.i_page;
    return (mag % 10) * 100 +
           (page & 0x0F) + ((page >> 4) & 0x0F) * 10;
}

static void
vlc_player_input_HandleTeletextMenu(struct vlc_player_input *input,
                                    const struct vlc_input_event_es *ev,
                                    const struct vlc_player_track_priv *trackpriv)
{
    vlc_player_t *player = input->player;
    if (ev->fmt->i_cat != SPU_ES ||
        ev->fmt->i_codec != VLC_CODEC_TELETEXT)
        return;
    switch (ev->action)
    {
        case VLC_INPUT_ES_ADDED:
        {
            if (!input->teletext_source)
            {
                input->teletext_source = trackpriv;
                vlc_player_SendEvent(player, on_teletext_menu_changed, true);
            }
            break;
        }
        case VLC_INPUT_ES_DELETED:
        {
            if (input->teletext_source == trackpriv)
            {
                input->teletext_source =
                        vlc_player_FindTeletextSource(input, trackpriv, true);
                if (!input->teletext_source)
                    input->teletext_source =
                            vlc_player_FindTeletextSource(input, trackpriv, false);
                if (!input->teletext_source) /* no more teletext ES */
                {
                    if (input->teletext_enabled)
                    {
                        input->teletext_enabled = false;
                        vlc_player_SendEvent(player, on_teletext_enabled_changed, false);
                    }
                    vlc_player_SendEvent(player, on_teletext_menu_changed, false);
                }
                else /* another teletext ES was reselected */
                {
                    if (input->teletext_source->t.selected != input->teletext_enabled)
                    {
                        input->teletext_enabled = input->teletext_source->t.selected;
                        vlc_player_SendEvent(player, on_teletext_enabled_changed,
                                             input->teletext_source->t.selected);
                    }
                    input->teletext_page =
                            vlc_player_input_TeletextUserPage(input->teletext_source);
                    vlc_player_SendEvent(player, on_teletext_page_changed,
                                         input->teletext_page);
                }
            }
            break;
        }
        case VLC_INPUT_ES_UPDATED:
            break;
        case VLC_INPUT_ES_SELECTED:
        {
            if (!input->teletext_enabled) /* we stick with the first selected */
            {
                input->teletext_source = trackpriv;
                input->teletext_enabled = true;
                input->teletext_page = vlc_player_input_TeletextUserPage(trackpriv);
                vlc_player_SendEvent(player, on_teletext_enabled_changed, true);
                vlc_player_SendEvent(player, on_teletext_page_changed,
                                     input->teletext_page);
            }
            break;
        }
        case VLC_INPUT_ES_UNSELECTED:
            if (input->teletext_source == trackpriv)
            {
                /* If there's another selected teletext, it needs to become source */
                const struct vlc_player_track_priv *other =
                        vlc_player_FindTeletextSource(input, trackpriv, true);
                if (other)
                {
                    input->teletext_source = other;
                    if (!input->teletext_enabled)
                    {
                        input->teletext_enabled = true;
                        vlc_player_SendEvent(player, on_teletext_enabled_changed, true);
                    }
                    input->teletext_page = vlc_player_input_TeletextUserPage(other);
                    vlc_player_SendEvent(player, on_teletext_page_changed,
                                         input->teletext_page);
                }
                else
                {
                    input->teletext_enabled = false;
                    vlc_player_SendEvent(player, on_teletext_enabled_changed, false);
                }
            }
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
vlc_player_input_HandleEsEvent(struct vlc_player_input *input,
                               const struct vlc_input_event_es *ev)
{
    assert(ev->id && ev->title && ev->fmt);

    vlc_player_track_vector *vec =
        vlc_player_input_GetTrackVector(input, ev->fmt->i_cat);
    if (!vec)
        return; /* UNKNOWN_ES or DATA_ES not handled */

    vlc_player_t *player = input->player;
    struct vlc_player_track_priv *trackpriv;
    switch (ev->action)
    {
        case VLC_INPUT_ES_ADDED:
            trackpriv = vlc_player_track_priv_New(ev->id, ev->title, ev->fmt);
            if (!trackpriv)
                break;

            if (!vlc_vector_push(vec, trackpriv))
            {
                vlc_player_track_priv_Delete(trackpriv);
                break;
            }
            vlc_player_SendEvent(player, on_track_list_changed,
                                 VLC_PLAYER_LIST_ADDED, &trackpriv->t);
            vlc_player_input_HandleTeletextMenu(input, ev, trackpriv);
            break;
        case VLC_INPUT_ES_DELETED:
        {
            size_t idx;
            trackpriv = vlc_player_track_vector_FindById(vec, ev->id, &idx);
            if (trackpriv)
            {
                vlc_player_input_HandleTeletextMenu(input, ev, trackpriv);
                vlc_player_SendEvent(player, on_track_list_changed,
                                     VLC_PLAYER_LIST_REMOVED, &trackpriv->t);
                vlc_vector_remove(vec, idx);
                vlc_player_track_priv_Delete(trackpriv);
            }
            break;
        }
        case VLC_INPUT_ES_UPDATED:
            trackpriv = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (!trackpriv)
                break;
            if (vlc_player_track_priv_Update(trackpriv, ev->title, ev->fmt) != 0)
                break;
            vlc_player_SendEvent(player, on_track_list_changed,
                                 VLC_PLAYER_LIST_UPDATED, &trackpriv->t);
            vlc_player_input_HandleTeletextMenu(input, ev, trackpriv);
            break;
        case VLC_INPUT_ES_SELECTED:
            trackpriv = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (trackpriv)
            {
                trackpriv->t.selected = true;
                trackpriv->selected_by_user = ev->forced;
                vlc_player_SendEvent(player, on_track_selection_changed,
                                     NULL, trackpriv->t.es_id);
                vlc_player_input_HandleTeletextMenu(input, ev, trackpriv);
            }
            break;
        case VLC_INPUT_ES_UNSELECTED:
            trackpriv = vlc_player_track_vector_FindById(vec, ev->id, NULL);
            if (trackpriv)
            {
                vlc_player_RemoveTimerSource(player, ev->id);
                trackpriv->t.selected = false;
                trackpriv->selected_by_user = false;
                vlc_player_SendEvent(player, on_track_selection_changed,
                                     trackpriv->t.es_id, NULL);
                vlc_player_input_HandleTeletextMenu(input, ev, trackpriv);
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
            {
                vlc_player_SendEvent(player, on_title_selection_changed,
                                     &input->titles->array[0], 0);
                if (input->ml.restore == VLC_RESTOREPOINT_TITLE &&
                    (size_t)input->ml.states.current_title < ev->list.count)
                {
                    vlc_player_SelectTitleIdx(player, input->ml.states.current_title);
                }
                input->ml.restore = VLC_RESTOREPOINT_POSITION;
            }
            else input->ml.restore = VLC_RESTOREPOINT_NONE;
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
            if (input->ml.restore == VLC_RESTOREPOINT_POSITION &&
                input->ml.states.current_title >= 0 &&
                (size_t)input->ml.states.current_title == ev->selected_idx &&
                input->ml.states.progress > .0f)
            {
                input_SetPosition(input->thread, input->ml.states.progress, false);
            }
            /* Reset the wanted title to avoid forcing it or the position
             * again during the next title change
             */
            input->ml.restore = VLC_RESTOREPOINT_NONE;
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

static void
vlc_player_input_HandleVoutEvent(struct vlc_player_input *input,
                                 const struct vlc_input_event_vout *ev)
{
    assert(ev->vout);
    assert(ev->id);

    vlc_player_t *player = input->player;

    struct vlc_player_track_priv *trackpriv =
        vlc_player_input_FindTrackById(input, ev->id, NULL);
    if (!trackpriv)
        return;

    const bool is_video_es = trackpriv->t.fmt.i_cat == VIDEO_ES;

    switch (ev->action)
    {
        case VLC_INPUT_EVENT_VOUT_STARTED:
            trackpriv->vout = ev->vout;
            vlc_player_SendEvent(player, on_vout_changed,
                                 VLC_PLAYER_VOUT_STARTED, ev->vout,
                                 ev->order, ev->id);

            if (is_video_es)
            {
                /* Register vout callbacks after the vout list event */
                vlc_player_vout_AddCallbacks(player, ev->vout);
            }
            break;
        case VLC_INPUT_EVENT_VOUT_STOPPED:
            if (is_video_es)
            {
                /* Un-register vout callbacks before the vout list event */
                vlc_player_vout_DelCallbacks(player, ev->vout);
            }

            trackpriv->vout = NULL;
            vlc_player_SendEvent(player, on_vout_changed,
                                 VLC_PLAYER_VOUT_STOPPED, ev->vout,
                                 VLC_VOUT_ORDER_NONE, ev->id);
            break;
        default:
            vlc_assert_unreachable();
    }
}

static void
input_thread_Events(input_thread_t *input_thread,
                    const struct vlc_input_event *event, void *user_data)
{
    struct vlc_player_input *input = user_data;
    vlc_player_t *player = input->player;

    assert(input_thread == input->thread);

    /* No player lock for this event */
    if (event->type == INPUT_EVENT_OUTPUT_CLOCK)
    {
        if (event->output_clock.system_ts != VLC_TICK_INVALID)
        {
            const struct vlc_player_timer_point point = {
                .position = 0,
                .rate = event->output_clock.rate,
                .ts = event->output_clock.ts,
                .length = VLC_TICK_INVALID,
                .system_date = event->output_clock.system_ts,
            };
            vlc_player_UpdateTimer(player, event->output_clock.id,
                                   event->output_clock.master, &point,
                                   VLC_TICK_INVALID,
                                   event->output_clock.frame_rate,
                                   event->output_clock.frame_rate_base);
        }
        else
        {
            vlc_player_UpdateTimerState(player, event->output_clock.id,
                                        VLC_PLAYER_TIMER_STATE_DISCONTINUITY,
                                        VLC_TICK_INVALID);
        }
        return;
    }

    vlc_mutex_lock(&player->lock);

    switch (event->type)
    {
        case INPUT_EVENT_STATE:
            vlc_player_input_HandleStateEvent(input, event->state.value,
                                              event->state.date);
            break;
        case INPUT_EVENT_RATE:
            input->rate = event->rate;
            vlc_player_SendEvent(player, on_rate_changed, input->rate);
            break;
        case INPUT_EVENT_CAPABILITIES:
        {
            int old_caps = input->capabilities;
            input->capabilities = event->capabilities;
            vlc_player_SendEvent(player, on_capabilities_changed,
                                 old_caps, input->capabilities);
            break;
        }
        case INPUT_EVENT_TIMES:
        {
            bool changed = false;
            vlc_tick_t system_date = VLC_TICK_INVALID;

            if (event->times.ms != VLC_TICK_INVALID
             && (input->time != event->times.ms
              || input->position != event->times.percentage))
            {
                input->time = event->times.ms;
                input->position = event->times.percentage;
                system_date = vlc_tick_now();
                changed = true;
                vlc_player_SendEvent(player, on_position_changed,
                                     input->time, input->position);

                vlc_player_input_UpdateTime(input);
            }
            if (input->length != event->times.length)
            {
                input->length = event->times.length;
                input_item_SetDuration(input_GetItem(input->thread), event->times.length);
                vlc_player_SendEvent(player, on_length_changed, input->length);
                changed = true;
            }

            if (input->normal_time != event->times.normal_time)
            {
                assert(event->times.normal_time != VLC_TICK_INVALID);
                input->normal_time = event->times.normal_time;
                changed = true;
            }

            if (changed)
            {
                const struct vlc_player_timer_point point = {
                    .position = input->position,
                    .rate = input->rate,
                    .ts = input->time + input->normal_time,
                    .length = input->length,
                    .system_date = system_date,
                };
                vlc_player_UpdateTimer(player, NULL, false, &point,
                                       input->normal_time, 0, 0);
            }
            break;
        }
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
                vlc_player_input_HandleState(input, VLC_PLAYER_STATE_STOPPING,
                                             VLC_TICK_INVALID);
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
vlc_player_input_SelectTracksByStringIds(struct vlc_player_input *input,
                                         enum es_format_category_e cat,
                                         const char *str_ids)
{
    input_SetEsCatIds(input->thread, cat, str_ids);
}

char *
vlc_player_input_GetSelectedTrackStringIds(struct vlc_player_input *input,
                                           enum es_format_category_e cat)
{
    vlc_player_track_vector *vec = vlc_player_input_GetTrackVector(input, cat);
    assert(vec);
    bool first_track = true;
    struct vlc_memstream ms;

    struct vlc_player_track_priv* t;
    vlc_vector_foreach(t, vec)
    {
        if (t->selected_by_user && vlc_es_id_IsStrIdStable(t->t.es_id))
        {
            if (first_track)
            {
                int ret = vlc_memstream_open(&ms);
                if (ret != 0)
                    return NULL;
            }
            const char *str_id = vlc_es_id_GetStrId(t->t.es_id);
            assert(str_id);

            if (!first_track)
                vlc_memstream_putc(&ms, ',');
            vlc_memstream_puts(&ms, str_id);
            first_track = false;
        }
    }
    return !first_track && vlc_memstream_close(&ms) == 0 ? ms.ptr : NULL;
}

struct vlc_player_input *
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
    input->normal_time = VLC_TICK_0;
    input->pause_date = VLC_TICK_INVALID;
    input->position = 0.f;

    input->recording = false;

    input->cache = 0.f;
    input->signal_quality = input->signal_strength = -1.f;

    memset(&input->stats, 0, sizeof(input->stats));

    vlc_vector_init(&input->program_vector);
    vlc_vector_init(&input->video_track_vector);
    vlc_vector_init(&input->audio_track_vector);
    vlc_vector_init(&input->spu_track_vector);
    input->teletext_source = NULL;

    input->titles = NULL;
    input->title_selected = input->chapter_selected = 0;

    input->teletext_enabled = input->teletext_transparent = false;
    input->teletext_page = 0;

    input->abloop_state[0].set = input->abloop_state[1].set = false;

    memset(&input->ml.states, 0, sizeof(input->ml.states));
    input->ml.states.aspect_ratio = input->ml.states.crop =
        input->ml.states.deinterlace = input->ml.states.video_filter = NULL;
    input->ml.states.current_title = -1;
    input->ml.states.current_video_track =
        input->ml.states.current_audio_track =
        input->ml.states.current_subtitle_track = NULL;
    input->ml.states.progress = -1.f;
    input->ml.restore = VLC_RESTOREPOINT_NONE;
    input->ml.restore_states = false;
    input->ml.delay_restore = false;

    input->thread = input_Create(player, input_thread_Events, input, item,
                                 player->resource, player->renderer);
    if (!input->thread)
    {
        free(input);
        return NULL;
    }
    vlc_player_input_RestoreMlStates(input, false);

    if (player->video_string_ids)
        vlc_player_input_SelectTracksByStringIds(input, VIDEO_ES,
                                                 player->video_string_ids);

    if (player->audio_string_ids)
        vlc_player_input_SelectTracksByStringIds(input, AUDIO_ES,
                                                 player->audio_string_ids);

    if (player->sub_string_ids)
        vlc_player_input_SelectTracksByStringIds(input, SPU_ES,
                                                 player->sub_string_ids);

    /* Initial sub/audio delay */
    const vlc_tick_t cat_delays[DATA_ES] = {
        [AUDIO_ES] =
            VLC_TICK_FROM_MS(var_InheritInteger(player, "audio-desync")),
        [SPU_ES] =
            vlc_tick_from_samples(var_InheritInteger(player, "sub-delay"), 10),
    };

    for (enum es_format_category_e i = UNKNOWN_ES; i < DATA_ES; ++i)
    {
        input->cat_delays[i] = cat_delays[i];
        if (cat_delays[i] != 0)
        {
            const input_control_param_t param = {
                .cat_delay = { i, cat_delays[i] }
            };
            int ret = input_ControlPush(input->thread,
                                        INPUT_CONTROL_SET_CATEGORY_DELAY,
                                        &param);
            if (ret == VLC_SUCCESS)
                vlc_player_SendEvent(player, on_category_delay_changed, i,
                                     cat_delays[i]);
        }
    }
    return input;
}

void
vlc_player_input_Delete(struct vlc_player_input *input)
{
    assert(input->titles == NULL);
    assert(input->program_vector.size == 0);
    assert(input->video_track_vector.size == 0);
    assert(input->audio_track_vector.size == 0);
    assert(input->spu_track_vector.size == 0);
    assert(input->teletext_source == NULL);

    vlc_vector_destroy(&input->program_vector);
    vlc_vector_destroy(&input->video_track_vector);
    vlc_vector_destroy(&input->audio_track_vector);
    vlc_vector_destroy(&input->spu_track_vector);

    input_Close(input->thread);
    free(input);
}
