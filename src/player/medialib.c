/*****************************************************************************
 * medialib.c: Player/Media Library interractions
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
#include "player.h"
#include "misc/variables.h"

void
vlc_player_input_RestoreMlStates(struct vlc_player_input* input,
                                 const input_item_t* item)
{
    vlc_player_t* player = input->player;
    vlc_player_assert_locked(player);

    vlc_medialibrary_t* ml = vlc_ml_instance_get(input->player);
    if (!ml)
        return;
    vlc_ml_media_t* media = vlc_ml_get_media_by_mrl( ml, item->psz_uri);
    if (!media)
        return;
    if (vlc_ml_media_get_all_playback_pref(ml, media->i_id, &input->ml.states) != VLC_SUCCESS)
        return;
    /* If we are aiming at a specific title, wait for it to be added, and
     * only then select it & set the position.
     * If we're not aiming at a specific title, just set the position now.
     */
    if (input->ml.states.current_title == -1 && input->ml.states.progress > .0f)
        input_SetPosition(input->thread, input->ml.states.progress, false);
    if (input->ml.states.rate != .0f)
        vlc_player_ChangeRate(player, input->ml.states.rate);

    /* Tracks are restored upon insertion, except when explicitely disabled */
    if (input->ml.states.current_video_track == -1)
    {
        input->ml.default_video_track = -1;
        input_ControlSync(input->thread, INPUT_CONTROL_SET_ES_AUTOSELECT,
                          &(input_control_param_t) {
                              .es_autoselect.cat = VIDEO_ES,
                              .es_autoselect.enabled = false,
                          });
    }
    if (input->ml.states.current_audio_track == -1)
    {
        input->ml.default_audio_track = -1;
        input_ControlSync(input->thread, INPUT_CONTROL_SET_ES_AUTOSELECT,
                          &(input_control_param_t) {
                              .es_autoselect.cat = AUDIO_ES,
                              .es_autoselect.enabled = false,
                          });
    }
    if (input->ml.states.current_subtitle_track == -1)
    {
        input->ml.default_subtitle_track = -1;
        input_ControlSync(input->thread, INPUT_CONTROL_SET_ES_AUTOSELECT,
                          &(input_control_param_t) {
                              .es_autoselect.cat = SPU_ES,
                              .es_autoselect.enabled = false,
                          });
    }

    vout_thread_t* vout = vlc_player_vout_Hold(player);
    if (vout != NULL)
    {
        if (input->ml.states.zoom >= .0f)
            var_SetFloat(vout, "zoom", input->ml.states.zoom);
        else
            var_SetFloat(vout, "zoom", 1.f);
        if (input->ml.states.aspect_ratio)
            var_SetString(vout, "aspect-ratio", input->ml.states.aspect_ratio);
        else
            var_SetString(vout, "aspect-ratio", NULL);
        if (input->ml.states.deinterlace)
        {
            var_SetString(vout, "deinterlace-mode", input->ml.states.deinterlace);
            var_SetInteger(vout, "deinterlace", 1);
        }
        else
        {
            var_SetString(vout, "deinterlace-mode", NULL);
            var_SetInteger(vout, "deinterlace", 0);
        }
        if (input->ml.states.video_filter)
            var_SetString(vout, "video-filter", input->ml.states.video_filter);
        else
            var_SetString(vout, "video-filter", NULL);
        vout_Release(vout);
    }
    vlc_ml_release(media);
}

void
vlc_player_UpdateMLStates(vlc_player_t *player, struct vlc_player_input* input)
{
    /* Do not save states for any secondary player. If this player's parent is
       the main vlc object, then it's the main player */
    if (player->obj.priv->parent != (vlc_object_t*)vlc_object_instance(player))
        return;

    vlc_medialibrary_t* ml = vlc_ml_instance_get(player);
    if (!ml)
        return;
    input_item_t* item = input_GetItem(input->thread);
    if (!item)
        return;
    vlc_ml_media_t* media = vlc_ml_get_media_by_mrl(ml, item->psz_uri);
    if (!media)
    {
        /* We don't know this media yet, let's add it as an external media so
         * we can still store its playback preferences
         */
        media = vlc_ml_new_external_media(ml, item->psz_uri);
        if (media == NULL)
            return;
    }

    /* If we reached 95% of the media or have less than 10s remaining, bump the
     * play count & the media in the history */
    if (input->position >= .95f ||
        input->length - input->time < VLC_TICK_FROM_SEC(10))
    {
        vlc_ml_media_increase_playcount(ml, media->i_id);
        /* Ensure we remove any previously saved position to allow the playback
         * of this media to restart from the begining */
        if (input->ml.states.progress >= .0f )
        {
            vlc_ml_media_set_playback_state(ml, media->i_id,
                                            VLC_ML_PLAYBACK_STATE_PROGRESS, NULL );
            input->ml.states.progress = -1.f;
        }
    }
    else
        input->ml.states.progress = input->position;

    /* If the value changed during the playback, update it in the medialibrary.
     * If not, set each state to their "unset" values, so that they aren't saved
     * in database */
    if ((input->ml.states.current_title == -1 && input->title_selected != 0) ||
         (input->ml.states.current_title != -1 &&
          input->ml.states.current_title != (int)input->title_selected))
    {
        input->ml.states.current_title = input->title_selected;
    }
    else
        input->ml.states.current_title = -1;

    /* We use .0f to signal an unsaved rate. We want to save it to the ml if it
     * changed, and if it's not the player's default value when the value was
     * never saved in the ML */
    if (input->rate != input->ml.states.rate &&
            (input->rate != 1.f || input->ml.states.rate != .0f))
        input->ml.states.rate = input->rate;
    else
        input->ml.states.rate = -1.f;

    struct vlc_player_track_priv* t;
    vout_thread_t* vout = NULL;

    vlc_vector_foreach(t, &input->video_track_vector)
    {
        if (!t->t.selected)
            continue;
        enum vlc_vout_order order;
        vout = vlc_player_GetEsIdVout(player, t->t.es_id, &order);
        if (vout != NULL && order == VLC_VOUT_ORDER_PRIMARY)
            break;
        vout = NULL;
    }
    if (vout != NULL)
    {
        /* We only want to save these states if they are different, and not the
         * default values (NULL), so this means that either one is NULL and the
         * other isn't, or they are both non null and differ lexicographically */
#define COMPARE_ASSIGN_STR(field, var) \
        char* field = var_GetNonEmptyString(vout, var); \
        if ( ( field != NULL && input->ml.states.field != NULL && \
               strcmp(field, input->ml.states.field) ) || \
             ( field == NULL && input->ml.states.field != NULL ) || \
             ( field != NULL && input->ml.states.field == NULL ) ) \
        { \
            free(input->ml.states.field); \
            input->ml.states.field = field; \
            field = NULL; \
        } \
        else \
        { \
            free(input->ml.states.field); \
            input->ml.states.field = NULL; \
        }

        COMPARE_ASSIGN_STR(aspect_ratio, "aspect-ratio" );
        COMPARE_ASSIGN_STR(crop, "crop");
        COMPARE_ASSIGN_STR(deinterlace, "deinterlace-mode");
        COMPARE_ASSIGN_STR(video_filter, "video-filter");

        if (input->ml.states.deinterlace != NULL &&
            !strcmp(input->ml.states.deinterlace, "auto"))
        {
            free(input->ml.states.deinterlace);
            input->ml.states.deinterlace = NULL;
        }

        float zoom = var_GetFloat(vout, "zoom");
        if (zoom != input->ml.states.zoom &&
            (zoom != 1.f && input->ml.states.zoom >= .0f))
            input->ml.states.zoom = zoom;
        else
            input->ml.states.zoom = -1.f;

#undef COMPARE_ASSIGN_STR
        free(video_filter);
        free(deinterlace);
        free(crop);
        free(aspect_ratio);
    }

    if (input->ml.default_video_track != -2)
    {
        int current_video_track = vlc_player_GetFirstSelectedTrackId(&input->video_track_vector);
        if (input->ml.default_video_track != current_video_track)
            input->ml.states.current_video_track = current_video_track;
        else
            input->ml.states.current_video_track = -2;
    }

    if (input->ml.default_audio_track != -2)
    {
        int current_audio_track = vlc_player_GetFirstSelectedTrackId(&input->audio_track_vector);
        if (input->ml.default_audio_track != current_audio_track)
            input->ml.states.current_audio_track = current_audio_track;
        else
            input->ml.states.current_audio_track = -2;
    }

    if (input->ml.default_subtitle_track != -2)
    {
        int current_subtitle_track = vlc_player_GetFirstSelectedTrackId(&input->spu_track_vector);
        if (input->ml.default_subtitle_track != current_subtitle_track)
            input->ml.states.current_subtitle_track = current_subtitle_track;
        else
            input->ml.states.current_subtitle_track = -2;
    }

    vlc_ml_media_set_all_playback_states(ml, media->i_id, &input->ml.states);
    vlc_ml_release(&input->ml.states);
    vlc_ml_release(media);
}
