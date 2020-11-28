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
#include <math.h>

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_player.h>

#include "cli.h"

struct player_cli {
    intf_thread_t *intf;
    vlc_player_listener_id *player_listener;
    vlc_player_aout_listener_id *player_aout_listener;
    long position;
    bool input_buffering;
    bool show_position;
};

/********************************************************************
 * Status callback routines
 ********************************************************************/

static void
player_on_media_changed(vlc_player_t *player, input_item_t *item, void *data)
{
    struct player_cli *pc = data;
    intf_thread_t *p_intf = pc->intf;

    (void) player;

    if (item != NULL)
    {
        vlc_mutex_lock(&item->lock);
        msg_rc(STATUS_CHANGE "( new input: %s )", item->psz_uri);
        vlc_mutex_unlock(&item->lock);
    }
}

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
    {
        msg_rc(STATUS_CHANGE "( time: %"PRId64"s )",
               SEC_FROM_VLC_TICK(new_time));
        pc->input_buffering = false;
    }

    long position = lroundf(new_pos * 100.f);

    if (pc->show_position && position != pc->position)
    {
        pc->position = position;
        msg_rc("pos: %ld%%", pc->position);
    }
}

static const struct vlc_player_cbs player_cbs =
{
    .on_current_media_changed = player_on_media_changed,
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

    msg_rc(STATUS_CHANGE "( audio volume: %f )", volume);
}

static const struct vlc_player_aout_cbs player_aout_cbs =
{
    .on_volume_changed = player_aout_on_volume_changed,
};

static int PlayerDoVoid(intf_thread_t *intf, void (*cb)(vlc_player_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    cb(player);
    vlc_player_Unlock(player);
    return 0;
}

static int PlayerDoFloat(intf_thread_t *intf, const char *const *args,
                         size_t count,
                         void (*setter)(vlc_player_t *, float),
                         float (*getter)(vlc_player_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    int ret = 0;

    vlc_player_Lock(player);
    switch (count)
    {
        case 1:
            msg_print(intf, "%f", getter(player));
            break;
        case 2:
            setter(player, atof(args[1]));
            break;
        default:
            ret = VLC_EGENERIC; /* EINVAL */
    }
    vlc_player_Unlock(player);
    return ret;
}

static int PlayerPause(intf_thread_t *intf, const char *const *args,
                       size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_TogglePause);
}

static int PlayerFastForward(intf_thread_t *intf, const char *const *args,
                             size_t count)
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
    {
        int secs = var_InheritInteger(intf, "extrashort-jump-size");
        vlc_tick_t t = vlc_player_GetTime(player) + vlc_tick_from_sec(secs);

        vlc_player_SetTime(player, t);
    }
    vlc_player_Unlock(player);
    (void) args; (void) count;
    return 0;
}

static int PlayerRewind(intf_thread_t *intf, const char *const *args,
                         size_t count)
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
    {
        int secs = var_InheritInteger(intf, "extrashort-jump-size");
        vlc_tick_t t = vlc_player_GetTime(player) - vlc_tick_from_sec(secs);

        vlc_player_SetTime(player, t);
    }
    vlc_player_Unlock(player);
    (void) args; (void) count;
    return 0;
}

static int PlayerFaster(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_IncrementRate);
}

static int PlayerSlower(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_DecrementRate);
}

static void PlayerDoNormal(vlc_player_t *player)
{
    vlc_player_ChangeRate(player, 1.f);
}

static int PlayerNormal(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, PlayerDoNormal);
}

static int PlayerRate(intf_thread_t *intf, const char *const *args, size_t n)
{
    return PlayerDoFloat(intf, args, n, vlc_player_ChangeRate,
                         vlc_player_GetRate);
}

static int PlayerFrame(intf_thread_t *intf, const char *const *args,
                       size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_NextVideoFrame);
}

static int PlayerChapterPrev(intf_thread_t *intf, const char *const *args,
                             size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_SelectPrevChapter);
}

static int PlayerChapterNext(intf_thread_t *intf, const char *const *args,
                             size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_SelectNextChapter);
}

static int PlayerTitlePrev(intf_thread_t *intf, const char *const *args,
                           size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_SelectPrevTitle);
}

static int PlayerTitleNext(intf_thread_t *intf, const char *const *args,
                           size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_SelectNextTitle);
}

static int PlayerSeek(intf_thread_t *intf, const char *const *args,
                      size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    if (count != 2)
    {
        msg_print(intf, "%s expects one parameter", args[0]);
        return VLC_EGENERIC; /* EINVAL */
    }

    char *end;
    double value = strtod(args[1], &end);
    bool relative = args[1][0] == '-' || args[1][0] == '+';
    bool pct = *end == '%';

    vlc_player_Lock(player);
    if (relative)
    {
        if (pct)
            value += vlc_player_GetPosition(player) * 100.;
        else
            value += secf_from_vlc_tick(vlc_player_GetTime(player));
    }

    if (pct)
        vlc_player_SetPosition(player, value / 100.);
    else
        vlc_player_SetTime(player, vlc_tick_from_sec(value));
    vlc_player_Unlock(player);
    return 0;
}

static int PlayerSetChapter(intf_thread_t *intf, const char *const *args,
                            size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    int ret = 0;

    vlc_player_Lock(player);

    if (count > 1)
        vlc_player_SelectChapterIdx(player, atoi(args[1]));
    else
    {
        struct vlc_player_title const *title = vlc_player_GetSelectedTitle(player);
        ssize_t chapter = -1;
        if (title != NULL)
            chapter = vlc_player_GetSelectedChapterIdx(player);
        if (chapter != -1)
            msg_print(intf, "Currently playing chapter %zd/%zu.",
                      chapter, title->chapter_count);
        else
        {
            msg_print(intf, "No chapter selected.");
            ret = VLC_ENOITEM;
        }
    }
    vlc_player_Unlock(player);
    return ret;
}

static int PlayerSetTitle(intf_thread_t *intf, const char *const *args,
                          size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    int ret = 0;

    vlc_player_Lock(player);

    if (count > 1)
    {
        int idx = atoi(args[1]);
        if (idx >= 0)
            vlc_player_SelectTitleIdx(player, (size_t)idx);
    }
    else
    {
        ssize_t title = vlc_player_GetSelectedTitleIdx(player);
            vlc_player_title_list *titles =
        vlc_player_GetTitleList(player);
        size_t title_count = 0;
        if (titles != NULL)
            title_count = vlc_player_title_list_GetCount(titles);
        if (title != -1 && title_count != 0)
            msg_print(intf, "Currently playing title %zd/%zu.", title,
                      title_count);
        else
        {
            msg_print(intf, "No title selected.");
            ret = VLC_ENOITEM;
        }
    }
    vlc_player_Unlock(player);
    return ret;
}

static int PlayerSetTrack(intf_thread_t *intf, const char *const *args,
                          size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const char *psz_cmd = args[0];
    enum es_format_category_e cat;
    int ret = VLC_EGENERIC; /* EINVAL */

    switch (psz_cmd[0])
    {
        case 'a':
            cat = AUDIO_ES;
            break;
        case 'v':
            cat = VIDEO_ES;
            break;
        default:
            cat = SPU_ES;
    }

    vlc_player_Lock(player);

    if (count > 1)
    {
        int idx = atoi(args[1]);
        if (idx < 0)
            goto out;

        size_t track_count = vlc_player_GetTrackCount(player, cat);
        if ((unsigned)idx >= track_count)
            goto out;

        struct vlc_player_track const *track =
            vlc_player_GetTrackAt(player, cat, (size_t)idx);
        if (track != NULL)
        {
            vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
            ret = 0;
        }
    }
    else
    {
        struct vlc_player_track const *cur_track =
            vlc_player_GetSelectedTrack(player, cat);
        char const *name = cur_track ? cur_track->name : psz_cmd;
        size_t track_count = vlc_player_GetTrackCount(player, cat);

        msg_print(intf, "+----[ %s ]", name);
        for (size_t i = 0; i < track_count; ++i)
        {
            struct vlc_player_track const *track =
                    vlc_player_GetTrackAt(player, cat, i);
            msg_print(intf, "| %zu - %s%s",
                      i, track->name, track == cur_track ? " *" : "");
        }
        msg_print(intf, "+----[ end of %s ]", name);
        ret = 0;
    }
out:
    vlc_player_Unlock(player);
    return ret;
}

static int PlayerRecord(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    vlc_player_Lock(player);

    bool cur_value = vlc_player_IsRecording(player);
    bool new_value = !cur_value;

    if (count > 1)
    {
        if (strcmp(args[1], "on") == 0)
            new_value = true;
        if (strcmp(args[1], "off") == 0)
            new_value = false;
    }

    if (cur_value != new_value)
        vlc_player_SetRecordingEnabled(player, new_value);
    vlc_player_Unlock(player);
    return 0;
}

static int PlayerItemInfo(intf_thread_t *intf, const char *const *args,
                          size_t count)
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
        msg_print(intf, "no input");
    vlc_player_Unlock(player);
    (void) args; (void) count;
    return (item != NULL) ? 0 : VLC_ENOITEM;
}

static int PlayerGetTime(intf_thread_t *intf, const char *const *args,
                         size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t t;

    vlc_player_Lock(player);
    t = vlc_player_GetTime(player);
    vlc_player_Unlock(player);
    if (t == VLC_TICK_INVALID)
        return VLC_ENOITEM;

    msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(t));
    (void) args; (void) count;
    return 0;
}

static int PlayerGetLength(intf_thread_t *intf, const char *const *args,
                           size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t l;

    vlc_player_Lock(player);
    l = vlc_player_GetLength(player);
    vlc_player_Unlock(player);

    if (l == VLC_TICK_INVALID)
        return VLC_ENOITEM;

    msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(l));
    (void) args; (void) count;
    return 0;
}

static int PlayerGetTitle(intf_thread_t *intf, const char *const *args,
                          size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const struct vlc_player_title *title;

    vlc_player_Lock(player);
    title = vlc_player_GetSelectedTitle(player);
    msg_print(intf, "%s", (title != NULL) ? title->name : "");
    vlc_player_Unlock(player);
    (void) args; (void) count;
    return (title != NULL) ? 0 : VLC_ENOITEM;
}

static int PlayerVoutSnapshot(intf_thread_t *intf, const char *const *args,
                              size_t count)
{
    (void) args; (void) count;
    return PlayerDoVoid(intf, vlc_player_vout_Snapshot);
}

static int PlayerFullscreen(intf_thread_t *intf, const char *const *args,
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
    return 0;
}

static int Volume(intf_thread_t *intf, const char *const *args, size_t count)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    vlc_player_Lock(player);
    if (count == 2)
    {
        /* NOTE: For unfortunate hysterical raisins, integer value above 1 are
         * interpreted in a scale of 256 parts. Floating point values are taken
         * as ratio as usual in the VLC code.
         * Yes, this sucks (hopefully nobody uses volume 1/256).
         */
        char *end;
        unsigned long ul = strtoul(args[1], &end, 10);
        float volume;

        static_assert ((AOUT_VOLUME_DEFAULT & (AOUT_VOLUME_DEFAULT - 1)) == 0,
                       "AOUT_VOLUME_DEFAULT must be a power of two.");

        if (*end == '\0' && ul > 1 && ul <= AOUT_VOLUME_MAX)
            volume = ldexpf(ul, -ctz(AOUT_VOLUME_DEFAULT));
        else
            volume = atof(args[1]);

        vlc_player_aout_SetVolume(player, volume);
    }
    else
        msg_print(intf, STATUS_CHANGE "( audio volume: %f )",
                  vlc_player_aout_GetVolume(player));
    vlc_player_Unlock(player);
    return 0;
}

static int VolumeMove(intf_thread_t *intf, const char *const *args,
                      size_t count)
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
    return 0;
}

static int VideoConfig(intf_thread_t *intf, const char *const *args,
                       size_t n_args)
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
                return VLC_ENOVAR;
            }
        }

        if ( var_Change( p_vout, psz_variable, VLC_VAR_GETCHOICES,
                         &count, &val, &text ) < 0 )
        {
            vout_Release(p_vout);
            free( psz_value );
            return VLC_ENOMEM;
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
    return 0;
}

static int AudioDevice(intf_thread_t *intf, const char *const *args,
                       size_t count)
{
    const char *cmd = args[0];
    const char *arg = count > 1 ? args[1] : "";
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *aout = vlc_player_aout_Hold(player);
    int ret = 0;

    if (aout == NULL)
        return VLC_ENOOBJ;

    char **ids, **names;
    int n = aout_DevicesList(aout, &ids, &names);
    if (n < 0)
    {
        ret = VLC_ENOMEM;
        goto out;
    }

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
    return ret;
}

static int AudioChannel(intf_thread_t *intf, const char *const *args,
                        size_t n_args)
{
    const char *cmd = args[0];
    const char *arg = n_args > 1 ? args[1] : "";
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *p_aout = vlc_player_aout_Hold(player);
    int ret = 0;

    if ( p_aout == NULL )
         return VLC_ENOOBJ;

    if ( !*arg )
    {
        /* Retrieve all registered ***. */
        vlc_value_t *val;
        char **text;
        size_t count;

        if ( var_Change( p_aout, "stereo-mode", VLC_VAR_GETCHOICES,
                         &count, &val, &text ) < 0 )
        {
            ret = VLC_ENOVAR;
            goto out;
        }

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
    return ret;
}

static int Statistics(intf_thread_t *intf, const char *const *args,
                      size_t count)
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
    (void) args; (void) count;
    return (item != NULL) ? 0 : VLC_ENOITEM;
}

static int IsPlaying(intf_thread_t *intf, const char *const *args,
                     size_t count)
{
    intf_sys_t *sys = intf->p_sys;
    vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
    enum vlc_player_state state;

    vlc_player_Lock(player);
    state = vlc_player_GetState(player);
    msg_print(intf, "%d", state == VLC_PLAYER_STATE_PLAYING
                       || state == VLC_PLAYER_STATE_PAUSED);
    vlc_player_Unlock(player);
    (void) args; (void) count;
    return 0;
}

static int PlayerStatus(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);

    input_item_t *item = vlc_player_GetCurrentMedia(player);
    if (item != NULL)
    {
        char *uri = input_item_GetURI(item);
        if (likely(uri != NULL))
        {
            msg_print(intf, STATUS_CHANGE "( new input: %s )", uri);
            free(uri);
        }
    }

    float volume = vlc_player_aout_GetVolume(player);
    if (isgreaterequal(volume, 0.f))
        msg_print(intf, STATUS_CHANGE "( audio volume: %ld )",
                  lroundf(volume * 100.f));

    enum vlc_player_state state = vlc_player_GetState(player);

    vlc_player_Unlock(player);

    int stnum = -1;
    const char *stname = "unknown";

    switch (state)
    {
        case VLC_PLAYER_STATE_STOPPING:
        case VLC_PLAYER_STATE_STOPPED:
            stnum = 5;
            stname = "stop";
            break;
        case VLC_PLAYER_STATE_PLAYING:
            stnum = 3;
            stname = "play";
            break;
        case VLC_PLAYER_STATE_PAUSED:
            stnum = 4;
            stname = "pause";
            break;
        default:
            break;
    }

    msg_print(intf, STATUS_CHANGE "( %s state: %u )", stname, stnum);
    (void) args; (void) count;
    return 0;
}

static const struct cli_handler cmds[] =
{
    { "pause", PlayerPause },
    { "title_n", PlayerTitleNext },
    { "title_p", PlayerTitlePrev },
    { "chapter_n", PlayerChapterNext },
    { "chapter_p", PlayerChapterPrev },
    { "fastforward", PlayerFastForward },
    { "rewind", PlayerRewind },
    { "faster", PlayerFaster },
    { "slower", PlayerSlower },
    { "normal", PlayerNormal },
    { "rate", PlayerRate },
    { "frame", PlayerFrame },
    { "info", PlayerItemInfo },
    { "get_time", PlayerGetTime },
    { "get_length", PlayerGetLength },
    { "get_title", PlayerGetTitle },
    { "snapshot", PlayerVoutSnapshot },

    { "is_playing", IsPlaying },
    { "status", PlayerStatus },
    { "stats", Statistics },

    /* DVD commands */
    { "seek", PlayerSeek },
    { "title", PlayerSetTitle },
    { "chapter", PlayerSetChapter },

    { "atrack", PlayerSetTrack },
    { "vtrack", PlayerSetTrack },
    { "strack", PlayerSetTrack },
    { "record", PlayerRecord },
    { "f", PlayerFullscreen },
    { "fs", PlayerFullscreen },
    { "fullscreen", PlayerFullscreen },

    /* video commands */
    { "vratio", VideoConfig },
    { "vcrop", VideoConfig },
    { "vzoom", VideoConfig },

    /* audio commands */
    { "volume", Volume },
    { "volup", VolumeMove },
    { "voldown", VolumeMove },
    { "adev", AudioDevice },
    { "achan", AudioChannel },
};

void *RegisterPlayer(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    struct player_cli *pc = malloc(sizeof (*pc));

    if (unlikely(pc == NULL))
        return NULL;

    pc->intf = intf;
    pc->position = -1.;
    pc->input_buffering = false;
    pc->show_position = var_InheritBool(intf, "rc-show-pos");

    RegisterHandlers(intf, cmds, ARRAY_SIZE(cmds));

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
