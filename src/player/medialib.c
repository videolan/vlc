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
vlc_player_input_RestoreMlStates(struct vlc_player_input* input, bool force_pos)
{
    vlc_player_t* player = input->player;
    vlc_player_assert_locked(player);

    int restore_pos;
    if (force_pos)
        restore_pos = VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS;
    else
        restore_pos = var_InheritInteger(player, "restore-playback-pos");
    bool restore_states = var_InheritBool(player, "restore-playback-states");

    vlc_medialibrary_t* ml = vlc_ml_instance_get(input->player);
    if (!ml)
        return;
    input_item_t* item = input_GetItem(input->thread);
    vlc_ml_media_t* media = vlc_ml_get_media_by_mrl( ml, item->psz_uri);
    if (!media)
        return;
    if (media->i_type != VLC_ML_MEDIA_TYPE_VIDEO ||
        vlc_ml_media_get_all_playback_pref(ml, media->i_id,
                                           &input->ml.states) != VLC_SUCCESS)
    {
        vlc_ml_release(media);
        return;
    }
    vlc_ml_release(media);

    input->ml.restore = (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS) ?
                            VLC_RESTOREPOINT_TITLE : VLC_RESTOREPOINT_NONE;
    input->ml.restore_states = restore_states;
    /* If we are aiming at a specific title, wait for it to be added, and
     * only then select it & set the position.
     * If we're not aiming at a specific title, just set the position now.
     */
    if (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS &&
            input->ml.states.current_title == -1 &&
            input->ml.states.progress > .0f)
        input_SetPosition(input->thread, input->ml.states.progress, false);
    else if (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ASK &&
             input->ml.states.progress > .0f)
        input->ml.delay_restore = true;

    if (!restore_states)
        return;
    if (input->ml.states.rate != .0f)
        vlc_player_ChangeRate(player, input->ml.states.rate);


    const char *video_track_ids = input->ml.states.current_video_track;
    const char *audio_track_ids = input->ml.states.current_audio_track;
    const char *subtitle_track_ids = input->ml.states.current_subtitle_track;

    if (video_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, VIDEO_ES,
                                                 video_track_ids);
    if (audio_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, AUDIO_ES,
                                                 audio_track_ids);
    if (subtitle_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, SPU_ES,
                                                 subtitle_track_ids);

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
}

static const float beginning_of_media_percent = .05f;
static const int64_t beginning_of_media_sec = 60;

static int
beginning_of_media(struct vlc_player_input *input)
{
    return input->position <= beginning_of_media_percent &&
        input->time < VLC_TICK_FROM_SEC(beginning_of_media_sec);
}

static const float end_of_media_percent = .95f;
static const int64_t end_of_media_sec = 60;

static int
end_of_media(struct vlc_player_input *input)
{
    return input->position >= end_of_media_percent &&
        input->length - input->time < VLC_TICK_FROM_SEC(end_of_media_sec);
}

static bool
vlc_player_UpdateMediaType(const struct vlc_player_input* input,
                           vlc_medialibrary_t* ml, vlc_ml_media_t* media)
{
    assert(media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN);
    vlc_ml_media_type_t media_type;
    if (input->video_track_vector.size > 0)
        media_type = VLC_ML_MEDIA_TYPE_VIDEO;
    else if (input->audio_track_vector.size > 0)
        media_type = VLC_ML_MEDIA_TYPE_AUDIO;
    else
        return false;
    if (vlc_ml_media_set_type(ml, media->i_id, media_type) != VLC_SUCCESS)
        return false;
    media->i_type = media_type;
    return true;
}

static void
vlc_player_CompareAssignState(char **target_ptr, char **input_ptr)
{
    /* We only want to save these states if they are different, and not the
     * default values (NULL), so this means that either one is NULL and the
     * other isn't, or they are both non null and differ lexicographically */

    char *target = *target_ptr;
    char *input = *input_ptr;
    if ( ( input != NULL && target != NULL &&
           strcmp(input, target ) ) ||
         ( input == NULL && target != NULL ) ||
         ( input != NULL && target == NULL ) )
    {
        free(target);
        *target_ptr = input;
        *input_ptr = NULL;
    }
    else
    {
        free(target);
        *target_ptr = NULL;
    }
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

    if (media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN)
    {
        if (!vlc_player_UpdateMediaType(input, ml, media))
            return;
    }
    assert(media->i_type != VLC_ML_MEDIA_TYPE_UNKNOWN);

    /* If we reached end of the media, bump the play count & the media in the
     * history */
    if (end_of_media(input))
        vlc_ml_media_increase_playcount(ml, media->i_id);

    if (beginning_of_media(input) || end_of_media(input))
    {
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
        char* aspect_ratio = var_GetNonEmptyString(vout, "aspect-ratio");
        char* crop = var_GetNonEmptyString(vout, "crop");
        char* deinterlace = var_GetNonEmptyString(vout, "deinterlace-mode");
        char* video_filter = var_GetNonEmptyString(vout, "video-filter");

        vlc_player_CompareAssignState(&input->ml.states.aspect_ratio, &aspect_ratio);
        vlc_player_CompareAssignState(&input->ml.states.crop, &crop);
        vlc_player_CompareAssignState(&input->ml.states.deinterlace, &deinterlace);
        vlc_player_CompareAssignState(&input->ml.states.video_filter, &video_filter);

        free(video_filter);
        free(deinterlace);
        free(crop);
        free(aspect_ratio);

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
    }

    char *video_track_ids =
        vlc_player_input_GetSelectedTrackStringIds(input, VIDEO_ES);
    char *audio_track_ids =
        vlc_player_input_GetSelectedTrackStringIds(input, AUDIO_ES);
    char *subtitle_track_ids =
        vlc_player_input_GetSelectedTrackStringIds(input, SPU_ES);

    vlc_player_CompareAssignState(&input->ml.states.current_video_track, &video_track_ids);
    vlc_player_CompareAssignState(&input->ml.states.current_audio_track, &audio_track_ids);
    vlc_player_CompareAssignState(&input->ml.states.current_subtitle_track, &subtitle_track_ids);

    free(video_track_ids);
    free(audio_track_ids);
    free(subtitle_track_ids);

    vlc_ml_media_set_all_playback_states(ml, media->i_id, &input->ml.states);

    vlc_ml_release(&input->ml.states);
    vlc_ml_release(media);
}

void
vlc_player_RestorePlaybackPos(vlc_player_t *player)
{
    vlc_player_input_RestoreMlStates(player->input, true);
}
