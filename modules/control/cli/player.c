/*****************************************************************************
 * player.c : remote control stdin/stdout module for vlc
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 *
 * Author: Peter Surda <shurdeek@panorama.sth.ac.at>
 *         Jean-Paul Saman <jpsaman #_at_# m2x _replaceWith#dot_ nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_player.h>
#include <vlc_actions.h>

#include "cli.h"

struct player_cli {
    intf_thread_t *intf;
    vlc_player_listener_id *player_listener;
    vlc_player_aout_listener_id *player_aout_listener;
    bool input_buffering;
};

/********************************************************************
 * Status callback routines
 ********************************************************************/
static void
player_on_state_changed(vlc_player_t *player,
                        enum vlc_player_state state, void *data)
{ VLC_UNUSED(player);
    struct player_cli *pc = data;
    intf_thread_t *p_intf = pc->intf;

    char const *psz_cmd;
    switch (state)
    {
    case VLC_PLAYER_STATE_STOPPING:
    case VLC_PLAYER_STATE_STOPPED:
        psz_cmd = "stop";
        break;
    case VLC_PLAYER_STATE_PLAYING:
        psz_cmd = "play";
        break;
    case VLC_PLAYER_STATE_PAUSED:
        psz_cmd = "pause";
        break;
    default:
        psz_cmd = "";
        break;
    }

    msg_rc(STATUS_CHANGE "( %s state: %d )", psz_cmd, state);
}

static void
player_on_buffering_changed(vlc_player_t *player,
                            float new_buffering, void *data)
{ VLC_UNUSED(player); VLC_UNUSED(new_buffering);
    struct player_cli *pc = data;

    pc->input_buffering = true;
}

static void
player_on_rate_changed(vlc_player_t *player, float new_rate, void *data)
{ VLC_UNUSED(player);
    struct player_cli *pc = data;
    intf_thread_t *p_intf = pc->intf;

    msg_rc(STATUS_CHANGE "( new rate: %.3f )", new_rate);
}

static void
player_on_position_changed(vlc_player_t *player,
                           vlc_tick_t new_time, float new_pos, void *data)
{ VLC_UNUSED(player); VLC_UNUSED(new_pos);
    struct player_cli *pc = data;
    intf_thread_t *p_intf = pc->intf;

    if (pc->input_buffering)
        msg_rc(STATUS_CHANGE "( time: %"PRId64"s )",
               SEC_FROM_VLC_TICK(new_time));

    pc->input_buffering = false;
}

static const struct vlc_player_cbs player_cbs =
{
    .on_state_changed = player_on_state_changed,
    .on_buffering_changed = player_on_buffering_changed,
    .on_rate_changed = player_on_rate_changed,
    .on_position_changed = player_on_position_changed,
};

static void
player_aout_on_volume_changed(audio_output_t *aout, float volume, void *data)
{ VLC_UNUSED(aout);
    struct player_cli *pc = data;
    intf_thread_t *p_intf = pc->intf;

    msg_rc(STATUS_CHANGE "( audio volume: %ld )",
            lroundf(volume * 100));
}

static const struct vlc_player_aout_cbs player_aout_cbs =
{
    .on_volume_changed = player_aout_on_volume_changed,
};

static void PlayerDoVoid(intf_thread_t *intf, void (*cb)(vlc_player_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    cb(player);
    vlc_player_Unlock(player);
}

void PlayerPause(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_TogglePause);
}

void PlayerFastForward(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    if (vlc_player_CanChangeRate(player))
    {
        float rate = vlc_player_GetRate(player);
        vlc_player_ChangeRate(player,
                              isgreater(rate, 0.f) ? rate * 2.f : -rate);
    }
    else
        var_SetInteger(vlc_object_instance(intf), "key-action",
                       ACTIONID_JUMP_FORWARD_EXTRASHORT);
    vlc_player_Unlock(player);
}

void PlayerRewind(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    if (vlc_player_CanRewind(player))
    {
        float rate = vlc_player_GetRate(player);
        vlc_player_ChangeRate(player, isless(rate, 0.f) ? rate * 2.f : -rate);
    }
    else
        var_SetInteger(vlc_object_instance(intf), "key-action",
                       ACTIONID_JUMP_BACKWARD_EXTRASHORT);
    vlc_player_Unlock(player);
}

void PlayerFaster(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_IncrementRate);
}

void PlayerSlower(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_DecrementRate);
}

static void PlayerDoNormal(vlc_player_t *player)
{
    vlc_player_ChangeRate(player, 1.f);
}

void PlayerNormal(intf_thread_t *intf)
{
    PlayerDoVoid(intf, PlayerDoNormal);
}

void PlayerFrame(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_NextVideoFrame);
}

void PlayerChapterPrev(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectPrevChapter);
}

void PlayerChapterNext(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectNextChapter);
}

void PlayerTitlePrev(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectPrevTitle);
}

void PlayerTitleNext(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectNextTitle);
}

void Input(intf_thread_t *intf, const char *const *args, size_t n_args)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const char *psz_cmd = args[0];
    const char *arg = n_args > 1 ? args[1] : "";

    vlc_player_Lock(player);
    /* Parse commands that only require an input */
    if( !strcmp( psz_cmd, "seek" ) )
    {
        if( strlen( arg ) > 0 && arg[strlen( arg ) - 1] == '%' )
        {
            float f = atof( arg ) / 100.0;
            vlc_player_SetPosition(player, f);
        }
        else
        {
            int t = atoi( arg );
            vlc_player_SetTime(player, vlc_tick_from_sec(t));
        }
    }
    else if( !strcmp( psz_cmd, "chapter" ) )
    {
            if ( *arg )
            {
                /* Set. */
                vlc_player_SelectChapterIdx(player, atoi(arg));
            }
            else
            {
                /* Get. */
                struct vlc_player_title const *title = vlc_player_GetSelectedTitle(player);
                ssize_t chapter = -1;
                if (title != NULL)
                    chapter = vlc_player_GetSelectedChapterIdx(player);
                if (chapter != -1)
                    msg_print(intf, "Currently playing chapter %zd/%zu.",
                              chapter, title->chapter_count);
                else
                    msg_print(intf, "No chapter selected.");
            }
    }
    else if( !strcmp( psz_cmd, "title" ) )
    {
            if ( *arg )
            {
                /* Set. */
                int idx = atoi(arg);
                if (idx >= 0)
                    vlc_player_SelectTitleIdx(player, (size_t)idx);
            }
            else
            {
                /* Get. */
                ssize_t title = vlc_player_GetSelectedTitleIdx(player);
                vlc_player_title_list *titles =
                    vlc_player_GetTitleList(player);
                size_t count = 0;
                if (titles != NULL)
                    count = vlc_player_title_list_GetCount(titles);
                if (title != -1 && count != 0)
                    msg_print(intf, "Currently playing title %zd/%zu.", title,
                              count);
                else
                    msg_print(intf, "No title selected.");
            }
    }
    else if(    !strcmp( psz_cmd, "atrack" )
             || !strcmp( psz_cmd, "vtrack" )
             || !strcmp( psz_cmd, "strack" ) )
    {
        enum es_format_category_e cat;
        if( !strcmp( psz_cmd, "atrack" ) )
            cat = AUDIO_ES;
        else if( !strcmp( psz_cmd, "vtrack" ) )
            cat = VIDEO_ES;
        else
            cat = SPU_ES;
        if (n_args > 1)
        {
            int idx = atoi(arg);
            if (idx < 0)
                goto out;
            size_t track_count = vlc_player_GetTrackCount(player, cat);
            if ((unsigned)idx >= track_count)
                goto out;
            struct vlc_player_track const *track =
                vlc_player_GetTrackAt(player, cat, (size_t)idx);
            if (!track)
                goto out;
            vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
        }
        else
        {
            struct vlc_player_track const *cur_track =
                vlc_player_GetSelectedTrack(player, cat);
            char const *name = cur_track ? cur_track->name : psz_cmd;
            msg_print(intf, "+----[ %s ]", name);
            size_t count = vlc_player_GetTrackCount(player, cat);
            for (size_t i = 0; i < count; ++i)
            {
                struct vlc_player_track const *track =
                    vlc_player_GetTrackAt(player, cat, i);
                msg_print(intf, "| %zu - %s%s",
                          i, track->name, track == cur_track ? " *" : "");
            }
            msg_print(intf, "+----[ end of %s ]", name);
        }
    }
    else if( !strcmp( psz_cmd, "record" ) )
    {
        bool b_update = true;
        bool b_value = vlc_player_IsRecording(player);

        if (n_args > 1)
        {
            if ( ( !strncmp( arg, "on", 2 )  &&  b_value ) ||
                 ( !strncmp( arg, "off", 3 ) && !b_value ) )
            {
                b_update = false;
            }
        }
        if( b_update )
        {
            b_value = !b_value;
            vlc_player_SetRecordingEnabled( player, b_value );
        }
    }
out:
    vlc_player_Unlock(player);
}

void PlayerItemInfo(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    input_item_t *item;

    vlc_player_Lock(player);
    item = vlc_player_GetCurrentMedia(player);

    if (item != NULL)
    {
        vlc_mutex_lock(&item->lock);
        for (int i = 0; i < item->i_categories; i++)
        {
            info_category_t *category = item->pp_categories[i];
            info_t *info;

            msg_print(intf, "+----[ %s ]", category->psz_name);
            msg_print(intf, "| ");
            info_foreach(info, &category->infos)
                msg_print(intf, "| %s: %s", info->psz_name,
                          info->psz_value);
            msg_print(intf, "| ");
        }
        msg_print(intf, "+----[ end of stream info ]");
        vlc_mutex_unlock(&item->lock);
    }
    else
    {
        msg_print(intf, "no input");
    }
    vlc_player_Unlock(player);
}

void PlayerGetTime(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t t;

    vlc_player_Lock(player);
    t = vlc_player_GetTime(player);
    vlc_player_Unlock(player);
    if (t != VLC_TICK_INVALID)
        msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(t));
}

void PlayerGetLength(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t l;

    vlc_player_Lock(player);
    l = vlc_player_GetLength(player);
    vlc_player_Unlock(player);

    if (l != VLC_TICK_INVALID)
        msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(l));
}

void PlayerGetTitle(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const struct vlc_player_title *title;

    vlc_player_Lock(player);
    title = vlc_player_GetSelectedTitle(player);
    msg_print(intf, "%s", (title != NULL) ? title->name : "");
    vlc_player_Unlock(player);
}

void PlayerVoutSnapshot(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_vout_Snapshot);
}

void PlayerFullscreen(intf_thread_t *intf, const char *const *args,
                      size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    bool fs = !vlc_player_vout_IsFullscreen(player);

    if (count > 1)
    {
        if (strncasecmp(args[1], "on", 2) == 0)
            fs = true;
        if (strncasecmp(args[1], "off", 3) == 0)
            fs = false;
    }

    vlc_player_vout_SetFullscreen(player, fs);
}

void Volume(intf_thread_t *intf, const char *const *args, size_t count)
{
    const char *arg = count > 1 ? args[1] : "";
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    vlc_player_Lock(player);
    if ( *arg )
    {
        /* Set. */
        float volume = atol(arg) / 100.f;
        vlc_player_aout_SetVolume(player, volume);
    }
    else
    {
        /* Get. */
        long int volume = lroundf(vlc_player_aout_GetVolume(player) * 100.f);
        msg_print(intf, STATUS_CHANGE "( audio volume: %ld )", volume);
    }
    vlc_player_Unlock(player);
}

void VolumeMove(intf_thread_t *intf, const char *const *args, size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const char *psz_cmd = args[0];
    const char *arg = count > 1 ? args[1] : "";

    float volume;
    int i_nb_steps = atoi(arg);

    if( !strcmp(psz_cmd, "voldown") )
        i_nb_steps *= -1;

    vlc_player_Lock(player);
    vlc_player_aout_IncrementVolume(player, i_nb_steps, &volume);
    vlc_player_Unlock(player);
}

void VideoConfig(intf_thread_t *intf, const char *const *args, size_t n_args)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vout_thread_t *p_vout = vlc_player_vout_Hold(player);
    const char * psz_variable = NULL;
    const char *psz_cmd = args[0];
    const char *arg = n_args > 1 ? args[1] : "";

    if( !strcmp( psz_cmd, "vcrop" ) )
        psz_variable = "crop";
    else if( !strcmp( psz_cmd, "vratio" ) )
        psz_variable = "aspect-ratio";
    else if( !strcmp( psz_cmd, "vzoom" ) )
        psz_variable = "zoom";
    else
        /* This case can't happen */
        vlc_assert_unreachable();

    if (n_args > 1)
    {
        /* set */
        if( !strcmp( psz_variable, "zoom" ) )
        {
            float f_float = atof( arg );
            var_SetFloat( p_vout, psz_variable, f_float );
        }
        else
            var_SetString( p_vout, psz_variable, arg );
    }
    else
    {
        /* get */
        char *name;
        vlc_value_t *val;
        char **text;
        float f_value = 0.;
        char *psz_value = NULL;
        size_t count;

        if( !strcmp( psz_variable, "zoom" ) )
            f_value = var_GetFloat( p_vout, "zoom" );
        else
        {
            psz_value = var_GetString( p_vout, psz_variable );
            if( psz_value == NULL )
            {
                vout_Release(p_vout);
                return;
            }
        }

        if ( var_Change( p_vout, psz_variable, VLC_VAR_GETCHOICES,
                         &count, &val, &text ) < 0 )
        {
            vout_Release(p_vout);
            free( psz_value );
            return;
        }

        /* Get the descriptive name of the variable */
        var_Change( p_vout, psz_variable, VLC_VAR_GETTEXT, &name );
        if( !name ) name = strdup(psz_variable);

        msg_print(intf, "+----[ %s ]", name);
        if( !strcmp( psz_variable, "zoom" ) )
        {
            for ( size_t i = 0; i < count; i++ )
            {
                const char *fmt = "| %f - %s";

                if (f_value == val[i].f_float)
                    fmt = "| %f - %s*";

                msg_print(intf, fmt, val[i].f_float, text[i]);
                free(text[i]);
            }
        }
        else
        {
            for ( size_t i = 0; i < count; i++ )
            {
                const char *fmt = "| %s - %s";

                if (strcmp(psz_value, val[i].psz_string) == 0)
                    fmt = "| %s - %s*";

                msg_print(intf, fmt, val[i].psz_string, text[i]);
                free(text[i]);
                free(val[i].psz_string);
            }
            free( psz_value );
        }
        free(text);
        free(val);
        msg_print(intf, "+----[ end of %s ]", name);

        free( name );
    }
    vout_Release(p_vout);
}

void AudioDevice(intf_thread_t *intf, const char *const *args, size_t count)
{
    const char *cmd = args[0];
    const char *arg = count > 1 ? args[1] : "";
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (aout == NULL)
        return;

    char **ids, **names;
    int n = aout_DevicesList(aout, &ids, &names);
    if (n < 0)
        goto out;

    bool setdev = count > 1;
    if (setdev)
        aout_DeviceSet(aout, arg);

    if (setdev)
    {
        for (int i = 0; i < n; ++i)
        {
            if (!strcmp(arg, ids[i]))
                vlc_player_osd_Message(player,
                                       _("Audio device: %s"), names[i]);

            free(names[i]);
            free(ids[i]);
        }
    }
    else
    {
        char *dev = aout_DeviceGet(aout);
        const char *devstr = (dev != NULL) ? dev : "";

        msg_print(intf, "+----[ %s ]", cmd);
        for ( int i = 0; i < n; i++ )
        {
            const char *fmt = "| %s - %s";

            if( !strcmp(devstr, ids[i]) )
                fmt = "| %s - %s *";
            msg_print(intf, fmt, ids[i], names[i]);
            free( names[i] );
            free( ids[i] );
        }
        msg_print(intf, "+----[ end of %s ]", cmd);

        free( dev );
    }

    free(ids);
    free(names);
out:
    aout_Release(aout);
}

void AudioChannel(intf_thread_t *intf, const char *const *args, size_t n_args)
{
    const char *cmd = args[0];
    const char *arg = n_args > 1 ? args[1] : "";
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *p_aout = vlc_player_aout_Hold(player);
    if ( p_aout == NULL )
         return;

    if ( !*arg )
    {
        /* Retrieve all registered ***. */
        vlc_value_t *val;
        char **text;
        size_t count;

        if ( var_Change( p_aout, "stereo-mode", VLC_VAR_GETCHOICES,
                         &count, &val, &text ) < 0 )
            goto out;

        int i_value = var_GetInteger( p_aout, "stereo-mode" );

        msg_print(intf, "+----[ %s ]", cmd);
        for ( size_t i = 0; i < count; i++ )
        {
            const char *fmt = "| %"PRId64" - %s";

            if (i_value == val[i].i_int)
                fmt = "| %"PRId64" - %s*";

            msg_print(intf, fmt, val[i].i_int, text[i]);
            free(text[i]);
        }
        free(text);
        free(val);
        msg_print(intf, "+----[ end of %s ]", cmd);
    }
    else
        var_SetInteger(p_aout, "stereo-mode", atoi(arg));
out:
    aout_Release(p_aout);
}

void Statistics(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    input_item_t *item;

    vlc_player_Lock(player);
    item = vlc_player_GetCurrentMedia(player);

    if (item != NULL)
    {
        msg_print(intf, "+----[ begin of statistical info ]");
        vlc_mutex_lock(&item->lock);

        /* Input */
        msg_print(intf, _("+-[Incoming]"));
        msg_print(intf, _("| input bytes read : %8.0f KiB"),
                  (float)(item->p_stats->i_read_bytes) / 1024.f);
        msg_print(intf, _("| input bitrate    :   %6.0f kb/s"),
                  (float)(item->p_stats->f_input_bitrate) * 8000.f);
        msg_print(intf, _("| demux bytes read : %8.0f KiB"),
                  (float)(item->p_stats->i_demux_read_bytes) / 1024.f);
        msg_print(intf, _("| demux bitrate    :   %6.0f kb/s"),
                  (float)(item->p_stats->f_demux_bitrate) * 8000.f);
        msg_print(intf, _("| demux corrupted  :    %5"PRIi64),
                  item->p_stats->i_demux_corrupted);
        msg_print(intf, _("| discontinuities  :    %5"PRIi64),
                  item->p_stats->i_demux_discontinuity);
        msg_print(intf, "|");

        /* Video */
        msg_print(intf, _("+-[Video Decoding]"));
        msg_print(intf, _("| video decoded    :    %5"PRIi64),
                  item->p_stats->i_decoded_video);
        msg_print(intf, _("| frames displayed :    %5"PRIi64),
                  item->p_stats->i_displayed_pictures);
        msg_print(intf, _("| frames late      :    %5"PRIi64),
                  item->p_stats->i_late_pictures);
        msg_print(intf, _("| frames lost      :    %5"PRIi64),
                  item->p_stats->i_lost_pictures);
        msg_print(intf, "|");

        /* Audio*/
        msg_print(intf, "%s", _("+-[Audio Decoding]"));
        msg_print(intf, _("| audio decoded    :    %5"PRIi64),
                  item->p_stats->i_decoded_audio);
        msg_print(intf, _("| buffers played   :    %5"PRIi64),
                  item->p_stats->i_played_abuffers);
        msg_print(intf, _("| buffers lost     :    %5"PRIi64),
                  item->p_stats->i_lost_abuffers);
        msg_print(intf, "|");

        vlc_mutex_unlock(&item->lock);
        msg_print(intf,  "+----[ end of statistical info ]" );
    }
    vlc_player_Unlock(player);
}

void IsPlaying(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;
    vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
    enum vlc_player_state state;

    vlc_player_Lock(player);
    state = vlc_player_GetState(player);
    msg_print(intf, "%d", state == VLC_PLAYER_STATE_PLAYING
                       || state == VLC_PLAYER_STATE_PAUSED);
    vlc_player_Unlock(player);
}

void *RegisterPlayer(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    struct player_cli *pc = malloc(sizeof (*pc));

    if (unlikely(pc == NULL))
        return NULL;

    pc->intf = intf;
    pc->input_buffering = false;

    vlc_player_Lock(player);
    pc->player_listener = vlc_player_AddListener(player, &player_cbs, pc);

    if (unlikely(pc->player_listener == NULL))
        goto error;

    pc->player_aout_listener =
        vlc_player_aout_AddListener(player, &player_aout_cbs, pc);

    if (pc->player_aout_listener == NULL)
    {
        vlc_player_RemoveListener(player, pc->player_listener);
        goto error;
    }

    player_on_state_changed(player, vlc_player_GetState(player), pc);
    vlc_player_Unlock(player);
    return pc;

error:
    vlc_player_Unlock(player);
    free(pc);
    return NULL;
}

void DeregisterPlayer(intf_thread_t *intf, void *data)
{
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    struct player_cli *pc = data;

    vlc_player_Lock(player);
    vlc_player_aout_RemoveListener(player, pc->player_aout_listener);
    vlc_player_RemoveListener(player, pc->player_listener);
    vlc_player_Unlock(player);
    free(pc);
    (void) intf;
}
