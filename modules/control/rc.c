/*****************************************************************************
 * rc.c : remote control stdin/stdout module for vlc
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <assert.h>
#include <math.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input_item.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_actions.h>

#include <sys/types.h>
#include <unistd.h>

#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_charset.h>

#if defined(PF_UNIX) && !defined(PF_LOCAL)
#    define PF_LOCAL PF_UNIX
#endif

#if defined(AF_LOCAL) && ! defined(_WIN32)
#    include <sys/un.h>
#endif

#define MAX_LINE_LENGTH 1024
#define STATUS_CHANGE "status change: "

struct intf_sys_t
{
    vlc_thread_t thread;

    /* playlist */
    vlc_playlist_t              *playlist;
    vlc_player_listener_id      *player_listener;
    vlc_player_aout_listener_id *player_aout_listener;

    /* status changes */
    vlc_mutex_t             status_lock;
    enum vlc_player_state   last_state;
    bool                    b_input_buffering;

#ifndef _WIN32
# ifdef AF_LOCAL
    char *psz_unix_path;
# endif
#else
    HANDLE hConsoleIn;
    bool b_quiet;
#endif
    int *pi_socket_listen;
    int i_socket;
};

VLC_FORMAT(2, 3)
static void msg_print(intf_thread_t *p_intf, const char *psz_fmt, ...)
{
    va_list args;
    char fmt_eol[strlen (psz_fmt) + 3], *msg;
    int len;

    snprintf (fmt_eol, sizeof (fmt_eol), "%s\r\n", psz_fmt);
    va_start( args, psz_fmt );
    len = vasprintf( &msg, fmt_eol, args );
    va_end( args );

    if( len < 0 )
        return;

    if( p_intf->p_sys->i_socket == -1 )
#ifdef _WIN32
        utf8_fprintf( stdout, "%s", msg );
#else
        vlc_write( 1, msg, len );
#endif
    else
        net_Write( p_intf, p_intf->p_sys->i_socket, msg, len );

    free( msg );
}
#define msg_rc(...) msg_print(p_intf, __VA_ARGS__)

#if defined (_WIN32) && !VLC_WINSTORE_APP
# include "intromsg.h"
#endif


/*****************************************************************************
 * parse_MRL: build a input item from a full mrl
 *****************************************************************************
 * MRL format: "simplified-mrl [:option-name[=option-value]]"
 * We don't check for '"' or '\'', we just assume that a ':' that follows a
 * space is a new option. Should be good enough for our purpose.
 *****************************************************************************/
static input_item_t *parse_MRL(const char *mrl)
{
#define SKIPSPACE( p ) { while( *p == ' ' || *p == '\t' ) p++; }
#define SKIPTRAILINGSPACE( p, d ) \
    { char *e = d; while (e > p && (*(e-1)==' ' || *(e-1)=='\t')) {e--; *e=0 ;} }

    input_item_t *p_item = NULL;
    char *psz_item = NULL, *psz_item_mrl = NULL, *psz_orig, *psz_mrl;
    char **ppsz_options = NULL;
    int i_options = 0;

    if (mrl == NULL)
        return 0;

    psz_mrl = psz_orig = strdup( mrl );
    if (psz_mrl == NULL)
        return NULL;

    while (*psz_mrl)
    {
        SKIPSPACE(psz_mrl);
        psz_item = psz_mrl;

        for (; *psz_mrl; psz_mrl++)
        {
            if ((*psz_mrl == ' ' || *psz_mrl == '\t') && psz_mrl[1] == ':')
            {
                /* We have a complete item */
                break;
            }
            if ((*psz_mrl == ' ' || *psz_mrl == '\t') &&
                (psz_mrl[1] == '"' || psz_mrl[1] == '\'') && psz_mrl[2] == ':')
            {
                /* We have a complete item */
                break;
            }
        }

        if (*psz_mrl)
        {
            *psz_mrl = 0;
            psz_mrl++;
        }
        SKIPTRAILINGSPACE(psz_item, psz_item + strlen(psz_item));

        /* Remove '"' and '\'' if necessary */
        if (*psz_item == '"' && psz_item[strlen(psz_item)-1] == '"')
        {
            psz_item++;
            psz_item[strlen(psz_item) - 1] = 0;
        }
        if (*psz_item == '\'' && psz_item[strlen(psz_item)-1] == '\'')
        {
            psz_item++;
            psz_item[strlen(psz_item)-1] = 0;
        }

        if (psz_item_mrl == NULL)
        {
            if (strstr( psz_item, "://" ) != NULL)
                psz_item_mrl = strdup(psz_item);
            else
                psz_item_mrl = vlc_path2uri(psz_item, NULL);
            if (psz_item_mrl == NULL)
            {
                free(psz_orig);
                return NULL;
            }
        }
        else if (*psz_item)
        {
            i_options++;
            ppsz_options = xrealloc(ppsz_options, i_options * sizeof(char *));
            ppsz_options[i_options - 1] = &psz_item[1];
        }

        if (*psz_mrl)
            SKIPSPACE(psz_mrl);
    }

    /* Now create a playlist item */
    if (psz_item_mrl != NULL)
    {
        p_item = input_item_New(psz_item_mrl, NULL);
        for (int i = 0; i < i_options; i++)
            input_item_AddOption(p_item, ppsz_options[i],
                                 VLC_INPUT_OPTION_TRUSTED);
        free(psz_item_mrl);
    }

    if (i_options)
        free(ppsz_options);
    free(psz_orig);

    return p_item;
}

static void Help( intf_thread_t *p_intf)
{
    msg_rc("%s", _("+----[ Remote control commands ]"));
    msg_rc(  "| ");
    msg_rc("%s", _("| add XYZ  . . . . . . . . . . . . add XYZ to playlist"));
    msg_rc("%s", _("| enqueue XYZ  . . . . . . . . . queue XYZ to playlist"));
    msg_rc("%s", _("| playlist . . . . .  show items currently in playlist"));
    msg_rc("%s", _("| play . . . . . . . . . . . . . . . . . . play stream"));
    msg_rc("%s", _("| stop . . . . . . . . . . . . . . . . . . stop stream"));
    msg_rc("%s", _("| next . . . . . . . . . . . . . .  next playlist item"));
    msg_rc("%s", _("| prev . . . . . . . . . . . .  previous playlist item"));
    msg_rc("%s", _("| goto . . . . . . . . . . . . . .  goto item at index"));
    msg_rc("%s", _("| repeat [on|off] . . . .  toggle playlist item repeat"));
    msg_rc("%s", _("| loop [on|off] . . . . . . . . . toggle playlist loop"));
    msg_rc("%s", _("| random [on|off] . . . . . . .  toggle random jumping"));
    msg_rc("%s", _("| clear . . . . . . . . . . . . . . clear the playlist"));
    msg_rc("%s", _("| status . . . . . . . . . . . current playlist status"));
    msg_rc("%s", _("| title [X]  . . . . . . set/get title in current item"));
    msg_rc("%s", _("| title_n  . . . . . . . .  next title in current item"));
    msg_rc("%s", _("| title_p  . . . . . .  previous title in current item"));
    msg_rc("%s", _("| chapter [X]  . . . . set/get chapter in current item"));
    msg_rc("%s", _("| chapter_n  . . . . . .  next chapter in current item"));
    msg_rc("%s", _("| chapter_p  . . . .  previous chapter in current item"));
    msg_rc(  "| ");
    msg_rc("%s", _("| seek X . . . seek in seconds, for instance `seek 12'"));
    msg_rc("%s", _("| pause  . . . . . . . . . . . . . . . .  toggle pause"));
    msg_rc("%s", _("| fastforward  . . . . . . . .  .  set to maximum rate"));
    msg_rc("%s", _("| rewind  . . . . . . . . . . . .  set to minimum rate"));
    msg_rc("%s", _("| faster . . . . . . . . . .  faster playing of stream"));
    msg_rc("%s", _("| slower . . . . . . . . . .  slower playing of stream"));
    msg_rc("%s", _("| normal . . . . . . . . . .  normal playing of stream"));
    msg_rc("%s", _("| frame. . . . . . . . . .  play frame by frame"));
    msg_rc("%s", _("| f [on|off] . . . . . . . . . . . . toggle fullscreen"));
    msg_rc("%s", _("| info . . . . .  information about the current stream"));
    msg_rc("%s", _("| stats  . . . . . . . .  show statistical information"));
    msg_rc("%s", _("| get_time . . seconds elapsed since stream's beginning"));
    msg_rc("%s", _("| is_playing . . . .  1 if a stream plays, 0 otherwise"));
    msg_rc("%s", _("| get_title . . . . .  the title of the current stream"));
    msg_rc("%s", _("| get_length . . . .  the length of the current stream"));
    msg_rc(  "| ");
    msg_rc("%s", _("| volume [X] . . . . . . . . . .  set/get audio volume"));
    msg_rc("%s", _("| volup [X]  . . . . . . .  raise audio volume X steps"));
    msg_rc("%s", _("| voldown [X]  . . . . . .  lower audio volume X steps"));
    msg_rc("%s", _("| adev [device]  . . . . . . . .  set/get audio device"));
    msg_rc("%s", _("| achan [X]. . . . . . . . . .  set/get audio channels"));
    msg_rc("%s", _("| atrack [X] . . . . . . . . . . . set/get audio track"));
    msg_rc("%s", _("| vtrack [X] . . . . . . . . . . . set/get video track"));
    msg_rc("%s", _("| vratio [X]  . . . . . . . set/get video aspect ratio"));
    msg_rc("%s", _("| vcrop [X]  . . . . . . . . . . .  set/get video crop"));
    msg_rc("%s", _("| vzoom [X]  . . . . . . . . . . .  set/get video zoom"));
    msg_rc("%s", _("| snapshot . . . . . . . . . . . . take video snapshot"));
    msg_rc("%s", _("| record [on|off] . . . . . . . . . . toggle recording"));
    msg_rc("%s", _("| strack [X] . . . . . . . . .  set/get subtitle track"));
    msg_rc("%s", _("| key [hotkey name] . . . . . .  simulate hotkey press"));
    msg_rc(  "| ");
    msg_rc("%s", _("| help . . . . . . . . . . . . . . . this help message"));
    msg_rc("%s", _("| logout . . . . . . .  exit (if in socket connection)"));
    msg_rc("%s", _("| quit . . . . . . . . . . . . . . . . . . .  quit vlc"));
    msg_rc(  "| ");
    msg_rc("%s", _("+----[ end of help ]"));
}

/********************************************************************
 * Status callback routines
 ********************************************************************/
static void
player_on_state_changed(vlc_player_t *player,
                        enum vlc_player_state state, void *data)
{ VLC_UNUSED(player);
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
    intf_thread_t *p_intf = data;
    msg_rc(STATUS_CHANGE "( %s state: %d )", psz_cmd, state);
}

static void
player_on_buffering_changed(vlc_player_t *player,
                            float new_buffering, void *data)
{ VLC_UNUSED(player); VLC_UNUSED(new_buffering);
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;
    vlc_mutex_lock(&sys->status_lock);
    sys->b_input_buffering = true;
    vlc_mutex_unlock(&sys->status_lock);
}

static void
player_on_rate_changed(vlc_player_t *player, float new_rate, void *data)
{ VLC_UNUSED(player);
    intf_thread_t *p_intf = data;
    intf_sys_t *sys = p_intf->p_sys;
    vlc_mutex_lock(&sys->status_lock);
    msg_rc(STATUS_CHANGE "( new rate: %.3f )", new_rate);
    vlc_mutex_unlock(&sys->status_lock);
}

static void
player_on_position_changed(vlc_player_t *player,
                           vlc_tick_t new_time, float new_pos, void *data)
{ VLC_UNUSED(player); VLC_UNUSED(new_pos);
    intf_thread_t *p_intf = data;
    intf_sys_t *sys = p_intf->p_sys;
    vlc_mutex_lock(&sys->status_lock);
    if (sys->b_input_buffering)
        msg_rc(STATUS_CHANGE "( time: %"PRId64"s )",
               SEC_FROM_VLC_TICK(new_time));
    sys->b_input_buffering = false;
    vlc_mutex_unlock(&sys->status_lock);
}

static void
player_aout_on_volume_changed(audio_output_t *aout, float volume, void *data)
{ VLC_UNUSED(aout);
    intf_thread_t *p_intf = data;
    vlc_mutex_lock(&p_intf->p_sys->status_lock);
    msg_rc(STATUS_CHANGE "( audio volume: %ld )",
            lroundf(volume * 100));
    vlc_mutex_unlock(&p_intf->p_sys->status_lock);
}

static void PlayerDoVoid(intf_thread_t *intf, void (*cb)(vlc_player_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    cb(player);
    vlc_player_Unlock(player);
}

static void PlayerPause(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_TogglePause);
}

static void PlayerFastForward(intf_thread_t *intf)
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

static void PlayerRewind(intf_thread_t *intf)
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

static void PlayerFaster(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_IncrementRate);
}

static void PlayerSlower(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_DecrementRate);
}

static void PlayerDoNormal(vlc_player_t *player)
{
    vlc_player_ChangeRate(player, 1.f);
}

static void PlayerNormal(intf_thread_t *intf)
{
    PlayerDoVoid(intf, PlayerDoNormal);
}

static void PlayerFrame(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_NextVideoFrame);
}

static void PlayerChapterPrev(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectPrevChapter);
}

static void PlayerChapterNext(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectNextChapter);
}

static void PlayerTitlePrev(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectPrevTitle);
}

static void PlayerTitleNext(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_SelectNextTitle);
}

/********************************************************************
 * Command routines
 ********************************************************************/
static void Input(intf_thread_t *intf, char const *psz_cmd,
                  vlc_value_t newval)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    vlc_player_Lock(player);
    /* Parse commands that only require an input */
    if( !strcmp( psz_cmd, "seek" ) )
    {
        if( strlen( newval.psz_string ) > 0 &&
            newval.psz_string[strlen( newval.psz_string ) - 1] == '%' )
        {
            float f = atof( newval.psz_string ) / 100.0;
            vlc_player_SetPosition(player, f);
        }
        else
        {
            int t = atoi( newval.psz_string );
            vlc_player_SetTime(player, vlc_tick_from_sec(t));
        }
    }
    else if( !strcmp( psz_cmd, "chapter" ) )
    {
            if ( *newval.psz_string )
            {
                /* Set. */
                vlc_player_SelectChapterIdx(player, atoi(newval.psz_string));
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
            if ( *newval.psz_string )
            {
                /* Set. */
                int idx = atoi(newval.psz_string);
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
        if( newval.psz_string && *newval.psz_string )
        {
            int idx = atoi(newval.psz_string);
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

        if( newval.psz_string[0] != '\0' )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  b_value ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !b_value ) )
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

static void PlayerItemInfo(intf_thread_t *intf)
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

static void PlayerGetTime(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t t;

    vlc_player_Lock(player);
    t = vlc_player_GetTime(player);
    vlc_player_Unlock(player);
    if (t != VLC_TICK_INVALID)
        msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(t));
}

static void PlayerGetLength(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_tick_t l;

    vlc_player_Lock(player);
    l = vlc_player_GetLength(player);
    vlc_player_Unlock(player);

    if (l != VLC_TICK_INVALID)
        msg_print(intf, "%"PRIu64, SEC_FROM_VLC_TICK(l));
}

static void PlayerGetTitle(intf_thread_t *intf)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    const struct vlc_player_title *title;

    vlc_player_Lock(player);
    title = vlc_player_GetSelectedTitle(player);
    msg_print(intf, "%s", (title != NULL) ? title->name : "");
    vlc_player_Unlock(player);
}

static void PlayerVoutSnapshot(intf_thread_t *intf)
{
    PlayerDoVoid(intf, vlc_player_vout_Snapshot);
}

static void print_playlist(intf_thread_t *p_intf, vlc_playlist_t *playlist)
{
    size_t count = vlc_playlist_Count(playlist);
    for (size_t i = 0; i < count; ++i)
    {
        vlc_playlist_item_t *plitem = vlc_playlist_Get(playlist, i);
        input_item_t *item = vlc_playlist_item_GetMedia(plitem);
        vlc_tick_t len = item->i_duration;
        if (len != INPUT_DURATION_INDEFINITE && len != VLC_TICK_INVALID)
        {
            char buf[MSTRTIME_MAX_SIZE];
            secstotimestr(buf, SEC_FROM_VLC_TICK(len));
            msg_rc("|-- %s (%s)", item->psz_name, buf);
        }
        else
            msg_rc("|-- %s", item->psz_name);
    }
}

static void PlaylistDoVoid(intf_thread_t *intf, int (*cb)(vlc_playlist_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    vlc_playlist_Lock(playlist);
    cb(playlist);
    vlc_playlist_Unlock(playlist);
}

static void PlaylistPrev(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Prev);
}

static void PlaylistNext(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Next);
}

static void PlaylistPlay(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Start);
}

static int PlaylistDoStop(vlc_playlist_t *playlist)
{
    vlc_playlist_Stop(playlist);
    return 0;
}

static void PlaylistStop(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, PlaylistDoStop);
}

static int PlaylistDoClear(vlc_playlist_t *playlist)
{
    PlaylistDoStop(playlist);
    vlc_playlist_Clear(playlist);
    return 0;
}

static void PlaylistClear(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, PlaylistDoClear);
}

static int PlaylistDoSort(vlc_playlist_t *playlist)
{
    struct vlc_playlist_sort_criterion criteria =
    {
        .key = VLC_PLAYLIST_SORT_KEY_ARTIST,
        .order = VLC_PLAYLIST_SORT_ORDER_ASCENDING
    };

    return vlc_playlist_Sort(playlist, &criteria, 1);
}

static void PlaylistSort(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, PlaylistDoSort);
}

static void PlaylistList(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    msg_print(intf, "+----[ Playlist ]");
    vlc_playlist_Lock(playlist);
    print_playlist(intf, playlist);
    vlc_playlist_Unlock(playlist);
    msg_print(intf, "+----[ End of playlist ]");
}

static void PlaylistStatus(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_playlist_Lock(playlist);

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

    vlc_playlist_Unlock(playlist);

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
}

static void Playlist(intf_thread_t *intf, char const *psz_cmd,
                     vlc_value_t newval)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    vlc_playlist_Lock(playlist);

    /* Parse commands that require a playlist */
    if( !strcmp( psz_cmd, "repeat" ) )
    {
        bool b_update = true;
        enum vlc_playlist_playback_repeat repeat_mode =
            vlc_playlist_GetPlaybackRepeat(playlist);
        bool b_value = repeat_mode == VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  b_value ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !b_value ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            b_value = !b_value;
            repeat_mode = b_value
                ? VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT
                : VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            vlc_playlist_SetPlaybackRepeat(playlist, repeat_mode);
        }
        msg_print(intf, "Setting repeat to %s", b_value ? "true" : "false");
    }
    else if( !strcmp( psz_cmd, "loop" ) )
    {
        bool b_update = true;
        enum vlc_playlist_playback_repeat repeat_mode =
            vlc_playlist_GetPlaybackRepeat(playlist);
        bool b_value = repeat_mode == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  b_value ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !b_value ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            b_value = !b_value;
            repeat_mode = b_value
                ? VLC_PLAYLIST_PLAYBACK_REPEAT_ALL
                : VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            vlc_playlist_SetPlaybackRepeat(playlist, repeat_mode);
        }
        msg_print(intf, "Setting loop to %s", b_value ? "true" : "false");
    }
    else if( !strcmp( psz_cmd, "random" ) )
    {
        bool b_update = true;
        enum vlc_playlist_playback_order order_mode =
            vlc_playlist_GetPlaybackOrder(playlist);
        bool b_value = order_mode == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  b_value ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !b_value ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            b_value = !b_value;
            order_mode = b_value
                ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM
                : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
            vlc_playlist_SetPlaybackOrder(playlist, order_mode);
        }
        msg_print(intf, "Setting random to %s", b_value ? "true" : "false");
    }
    else if (!strcmp( psz_cmd, "goto" ) )
    {
        long long llindex = atoll(newval.psz_string);
        size_t index = (size_t)llindex;
        size_t count = vlc_playlist_Count(playlist);
        if (llindex < 0)
            msg_print(intf, _("Error: `goto' needs an argument greater or equal to zero."));
        else if (index < count)
            vlc_playlist_PlayAt(playlist, index);
        else
            msg_print(intf,
                      vlc_ngettext("Playlist has only %zu element",
                                   "Playlist has only %zu elements", count),
                      count);
    }
    else if ((!strcmp(psz_cmd, "add") || !strcmp(psz_cmd, "enqueue")) &&
             newval.psz_string && *newval.psz_string)
    {
        input_item_t *p_item = parse_MRL( newval.psz_string );

        if( p_item )
        {
            msg_print(intf, "Trying to %s %s to playlist.", psz_cmd,
                      newval.psz_string);

            size_t count = vlc_playlist_Count(playlist);
            int ret = vlc_playlist_InsertOne(playlist, count, p_item);
            input_item_Release(p_item);
            if (ret != VLC_SUCCESS)
                goto end;

            if (!strcmp(psz_cmd, "add"))
                vlc_playlist_PlayAt(playlist, count);
        }
    }
    /*
     * sanity check
     */
    else
        msg_print(intf, "unknown command!");

end:
    vlc_playlist_Unlock(playlist);
}

static void Intf(intf_thread_t *intf, char const *psz_cmd,
                 vlc_value_t newval)
{
    VLC_UNUSED(psz_cmd);
    intf_Create(vlc_object_instance(intf), newval.psz_string);
}

static void Volume(intf_thread_t *intf, char const *psz_cmd,
                   vlc_value_t newval)
{
    VLC_UNUSED(psz_cmd);
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vlc_player_Lock(player);
    if ( *newval.psz_string )
    {
        /* Set. */
        float volume = atol(newval.psz_string) / 100.f;
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

static void VolumeMove(intf_thread_t *intf, char const *psz_cmd,
                       vlc_value_t newval)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);

    float volume;
    int i_nb_steps = atoi(newval.psz_string);

    if( !strcmp(psz_cmd, "voldown") )
        i_nb_steps *= -1;

    vlc_player_Lock(player);
    vlc_player_aout_IncrementVolume(player, i_nb_steps, &volume);
    vlc_player_Unlock(player);
}

static void VideoConfig(intf_thread_t *intf, char const *psz_cmd,
                        vlc_value_t newval)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    vout_thread_t *p_vout = vlc_player_vout_Hold(player);
    const char * psz_variable = NULL;

    if( !strcmp( psz_cmd, "vcrop" ) )
        psz_variable = "crop";
    else if( !strcmp( psz_cmd, "vratio" ) )
        psz_variable = "aspect-ratio";
    else if( !strcmp( psz_cmd, "vzoom" ) )
        psz_variable = "zoom";
    else
        /* This case can't happen */
        vlc_assert_unreachable();

    if( newval.psz_string && *newval.psz_string )
    {
        /* set */
        if( !strcmp( psz_variable, "zoom" ) )
        {
            float f_float = atof( newval.psz_string );
            var_SetFloat( p_vout, psz_variable, f_float );
        }
        else
            var_SetString( p_vout, psz_variable, newval.psz_string );
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

static void AudioDevice(intf_thread_t *intf, char const *cmd,
                        vlc_value_t cur)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (aout == NULL)
        return;

    char **ids, **names;
    int n = aout_DevicesList(aout, &ids, &names);
    if (n < 0)
        goto out;

    bool setdev = cur.psz_string && *cur.psz_string;
    if (setdev)
        aout_DeviceSet(aout, cur.psz_string);

    if (setdev)
    {
        int i;
        for (i = 0; i < n; ++i)
            if (!strcmp(cur.psz_string, ids[i]))
                break;
        if (i < n)
            vlc_player_osd_Message(player,
                                   _("Audio device: %s"), names[i]);
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

static void AudioChannel(intf_thread_t *intf, char const *cmd, vlc_value_t cur)
{
    vlc_player_t *player = vlc_playlist_GetPlayer(intf->p_sys->playlist);
    audio_output_t *p_aout = vlc_player_aout_Hold(player);
    if ( p_aout == NULL )
         return;

    if ( !*cur.psz_string )
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
        var_SetInteger( p_aout, "stereo-mode", atoi( cur.psz_string ) );
out:
    aout_Release(p_aout);
}

static void Statistics(intf_thread_t *intf)
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

static void Quit(intf_thread_t *intf)
{
    libvlc_Quit(vlc_object_instance(intf));
}

static void LogOut(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    /* Close connection */
    if (sys->i_socket != -1)
    {
        net_Close(sys->i_socket);
        sys->i_socket = -1;
    }
}

static void IsPlaying(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    msg_print(intf, "%d",
              sys->last_state == VLC_PLAYER_STATE_PLAYING ||
              sys->last_state == VLC_PLAYER_STATE_PAUSED);
}

static const struct
{
    const char *name;
    void (*handler)(intf_thread_t *);
} void_cmds[] =
{
    { "playlist", PlaylistList },
    { "sort", PlaylistSort },
    { "play", PlaylistPlay },
    { "stop", PlaylistStop },
    { "clear", PlaylistClear },
    { "prev", PlaylistPrev },
    { "next", PlaylistNext },
    { "status", PlaylistStatus },
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
    { "frame", PlayerFrame },
    { "info", PlayerItemInfo },
    { "get_time", PlayerGetTime },
    { "get_length", PlayerGetLength },
    { "get_title", PlayerGetTitle },
    { "snapshot", PlayerVoutSnapshot },

    { "is_player", IsPlaying },
    { "stats", Statistics },
    { "longhelp", Help },
    { "logout", LogOut },
    { "quit", Quit },
};

static const struct
{
    const char *name;
    void (*handler)(intf_thread_t *, const char *, vlc_value_t);
} string_cmds[] =
{
    { "intf", Intf },
    { "add", Playlist },
    { "repeat", Playlist },
    { "loop", Playlist },
    { "random", Playlist },
    { "enqueue", Playlist },
    { "goto", Playlist },

    /* DVD commands */
    { "seek", Input },
    { "title", Input },
    { "chapter", Input },

    { "atrack", Input },
    { "vtrack", Input },
    { "strack", Input },
    { "record", Input },

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

static void Process(intf_thread_t *intf, const char *cmd, const char *arg)
{
    intf_sys_t *sys = intf->p_sys;

    for (size_t i = 0; i < ARRAY_SIZE(void_cmds); i++)
        if (strcmp(cmd, void_cmds[i].name) == 0)
        {
            void_cmds[i].handler(intf);
            return;
        }

    for (size_t i = 0; i < ARRAY_SIZE(string_cmds); i++)
        if (strcmp(cmd, string_cmds[i].name) == 0)
        {
            vlc_value_t n = { .psz_string = (char *)arg };

            string_cmds[i].handler(intf, cmd, n);
            return;
        }

    /* misc menu commands */
    if (strcmp(cmd, "key") == 0 || strcmp(cmd, "hotkey") == 0)
    {
       vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(intf));
       var_SetInteger(vlc, "key-action", vlc_actions_get_id(arg));
    }
    else
        switch (cmd[0])
        {
            case 'f':
            case 'F':
            {
                vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
                bool fs;

                if (strncasecmp(arg, "on", 2) == 0)
                    fs = true;
                else if (strncasecmp(arg, "off", 3) == 0)
                    fs = false;
                else
                    fs = !vlc_player_vout_IsFullscreen(player);
                vlc_player_vout_SetFullscreen(player, fs);
                break;
            }

            case 'h':
            case 'H':
            case '?':
                Help(intf);
                break;

            case 's':
            case 'S':
            case '\0': /* Ignore empty lines */
                break;

            default:
                msg_print(intf,
                          _("Unknown command `%s'. Type `help' for help."),
                          cmd);
                break;
        }
}


#if defined(_WIN32) && !VLC_WINSTORE_APP
static bool ReadWin32( intf_thread_t *p_intf, unsigned char *p_buffer, int *pi_size )
{
    INPUT_RECORD input_record;
    DWORD i_dw;

    /* On Win32, select() only works on socket descriptors */
    while( WaitForSingleObjectEx( p_intf->p_sys->hConsoleIn,
                                MS_FROM_VLC_TICK(INTF_IDLE_SLEEP), TRUE ) == WAIT_OBJECT_0 )
    {
        // Prefer to fail early when there's not enough space to store a 4 bytes
        // UTF8 character. The function will be immediatly called again and we won't
        // lose an input
        while( *pi_size < MAX_LINE_LENGTH - 4 &&
               ReadConsoleInput( p_intf->p_sys->hConsoleIn, &input_record, 1, &i_dw ) )
        {
            if( input_record.EventType != KEY_EVENT ||
                !input_record.Event.KeyEvent.bKeyDown ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_CONTROL||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_MENU ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_CAPITAL )
            {
                /* nothing interesting */
                continue;
            }
            if( input_record.Event.KeyEvent.uChar.AsciiChar == '\n' ||
                input_record.Event.KeyEvent.uChar.AsciiChar == '\r' )
            {
                putc( '\n', stdout );
                break;
            }
            switch( input_record.Event.KeyEvent.uChar.AsciiChar )
            {
            case '\b':
                if ( *pi_size == 0 )
                    break;
                if ( *pi_size > 1 && (p_buffer[*pi_size - 1] & 0xC0) == 0x80 )
                {
                    // pi_size currently points to the character to be written, so
                    // we need to roll back from 2 bytes to start erasing the previous
                    // character
                    (*pi_size) -= 2;
                    unsigned int nbBytes = 1;
                    while( *pi_size > 0 && (p_buffer[*pi_size] & 0xC0) == 0x80 )
                    {
                        (*pi_size)--;
                        nbBytes++;
                    }
                    assert( clz( (unsigned char)~(p_buffer[*pi_size]) ) == nbBytes + 1 );
                    // The first utf8 byte will be overriden by a \0
                }
                else
                    (*pi_size)--;
                p_buffer[*pi_size] = 0;

                fputs( "\b \b", stdout );
                break;
            default:
            {
                WCHAR psz_winput[] = { input_record.Event.KeyEvent.uChar.UnicodeChar, L'\0' };
                char* psz_input = FromWide( psz_winput );
                int input_size = strlen(psz_input);
                if ( *pi_size + input_size > MAX_LINE_LENGTH )
                {
                    p_buffer[ *pi_size ] = 0;
                    return false;
                }
                strcpy( (char*)&p_buffer[*pi_size], psz_input );
                utf8_fprintf( stdout, "%s", psz_input );
                free(psz_input);
                *pi_size += input_size;
            }
            }
        }

        p_buffer[ *pi_size ] = 0;
        return true;
    }

    vlc_testcancel ();

    return false;
}
#endif

static bool ReadCommand(intf_thread_t *p_intf, char *p_buffer, int *pi_size)
{
#if defined(_WIN32) && !VLC_WINSTORE_APP
    if( p_intf->p_sys->i_socket == -1 && !p_intf->p_sys->b_quiet )
        return ReadWin32( p_intf, (unsigned char*)p_buffer, pi_size );
    else if( p_intf->p_sys->i_socket == -1 )
    {
        vlc_tick_sleep( INTF_IDLE_SLEEP );
        return false;
    }
#endif

    while( *pi_size < MAX_LINE_LENGTH )
    {
        if( p_intf->p_sys->i_socket == -1 )
        {
            if( read( 0/*STDIN_FILENO*/, p_buffer + *pi_size, 1 ) <= 0 )
            {   /* Standard input closed: exit */
                libvlc_Quit( vlc_object_instance(p_intf) );
                p_buffer[*pi_size] = 0;
                return true;
            }
        }
        else
        {   /* Connection closed */
            if( net_Read( p_intf, p_intf->p_sys->i_socket, p_buffer + *pi_size,
                          1 ) <= 0 )
            {
                net_Close( p_intf->p_sys->i_socket );
                p_intf->p_sys->i_socket = -1;
                p_buffer[*pi_size] = 0;
                return true;
            }
        }

        if( p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
            break;

        (*pi_size)++;
    }

    if( *pi_size == MAX_LINE_LENGTH ||
        p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
    {
        p_buffer[ *pi_size ] = 0;
        return true;
    }

    return false;
}

/*****************************************************************************
 * Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void *Run( void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;

    char p_buffer[ MAX_LINE_LENGTH + 1 ];
    bool b_showpos = var_InheritBool( p_intf, "rc-show-pos" );

    int  i_size = 0;
    int  i_oldpos = 0;
    int  i_newpos;
    int  canc = vlc_savecancel( );

    p_buffer[0] = 0;

#if defined(_WIN32) && !VLC_WINSTORE_APP
    /* Get the file descriptor of the console input */
    p_intf->p_sys->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( p_intf->p_sys->hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "couldn't find user input handle" );
        return NULL;
    }
#endif

    /* Register commands that will be cleaned up upon object destruction */
    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);
    input_item_t *item = NULL;

    /* status callbacks */

    for( ;; )
    {
        char *psz_cmd, *psz_arg;
        bool b_complete;

        vlc_restorecancel( canc );

        if( p_sys->pi_socket_listen != NULL && p_sys->i_socket == -1 )
        {
            p_sys->i_socket =
                net_Accept( p_intf, p_sys->pi_socket_listen );
            if( p_sys->i_socket == -1 ) continue;
        }

        b_complete = ReadCommand( p_intf, p_buffer, &i_size );
        canc = vlc_savecancel( );

        vlc_player_Lock(player);
        /* Manage the input part */
        if( item == NULL )
        {
            item = vlc_player_GetCurrentMedia(player);
            /* New input has been registered */
            if( item )
            {
                char *psz_uri = input_item_GetURI( item );
                msg_rc( STATUS_CHANGE "( new input: %s )", psz_uri );
                free( psz_uri );
            }
        }

        if( !vlc_player_IsStarted( player ) )
        {
            if (item)
                item = NULL;

            p_sys->last_state = VLC_PLAYER_STATE_STOPPED;
            msg_rc( STATUS_CHANGE "( stop state: 0 )" );
        }

        if( item != NULL )
        {
            enum vlc_player_state state = vlc_player_GetState(player);

            if (p_sys->last_state != state)
            {
                switch (state)
                {
                    case VLC_PLAYER_STATE_STOPPING:
                    case VLC_PLAYER_STATE_STOPPED:
                        msg_rc(STATUS_CHANGE "( stop state: 5 )");
                        break;
                    case VLC_PLAYER_STATE_PLAYING:
                        msg_rc(STATUS_CHANGE "( play state: 3 )");
                        break;
                    case VLC_PLAYER_STATE_PAUSED:
                        msg_rc(STATUS_CHANGE "( pause state: 4 )");
                        break;
                    default:
                        break;
                }
                p_sys->last_state = state;
            }
        }

        if( item && b_showpos )
        {
            i_newpos = 100 * vlc_player_GetPosition( player );
            if( i_oldpos != i_newpos )
            {
                i_oldpos = i_newpos;
                msg_rc( "pos: %d%%", i_newpos );
            }
        }
        vlc_player_Unlock(player);

        /* Is there something to do? */
        if( !b_complete ) continue;

        /* Skip heading spaces */
        psz_cmd = p_buffer;
        while( *psz_cmd == ' ' )
        {
            psz_cmd++;
        }

        /* Split psz_cmd at the first space and make sure that
         * psz_arg is valid */
        psz_arg = strchr( psz_cmd, ' ' );
        if( psz_arg )
        {
            *psz_arg++ = 0;
            while( *psz_arg == ' ' )
            {
                psz_arg++;
            }
        }
        else
        {
            psz_arg = (char*)"";
        }

        Process(p_intf, psz_cmd, psz_arg);

        /* Command processed */
        i_size = 0; p_buffer[0] = 0;
    }

    msg_rc( STATUS_CHANGE "( stop state: 0 )" );
    msg_rc( STATUS_CHANGE "( quit )" );

    vlc_restorecancel( canc );

    return NULL;
}

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    /* FIXME: This function is full of memory leaks and bugs in error paths. */
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    char *psz_host, *psz_unix_path = NULL;
    int  *pi_socket = NULL;

#ifndef _WIN32
#if defined(HAVE_ISATTY)
    /* Check that stdin is a TTY */
    if( !var_InheritBool( p_intf, "rc-fake-tty" ) && !isatty( 0 ) )
    {
        msg_Warn( p_intf, "fd 0 is not a TTY" );
        return VLC_EGENERIC;
    }
#endif
#ifdef AF_LOCAL
    psz_unix_path = var_InheritString( p_intf, "rc-unix" );
    if( psz_unix_path )
    {
        int i_socket;
        struct sockaddr_un addr;

        memset( &addr, 0, sizeof(struct sockaddr_un) );

        msg_Dbg( p_intf, "trying UNIX socket" );

        /* The given unix path cannot be longer than sun_path - 1 to take into
         * account the terminated null character. */
        if ( strlen(psz_unix_path) + 1 >= sizeof( addr.sun_path ) )
        {
            msg_Err( p_intf, "rc-unix value is longer than expected" );
            return VLC_EGENERIC;
        }

        if( (i_socket = vlc_socket( PF_LOCAL, SOCK_STREAM, 0, false ) ) < 0 )
        {
            msg_Warn( p_intf, "can't open socket: %s", vlc_strerror_c(errno) );
            free( psz_unix_path );
            return VLC_EGENERIC;
        }

        addr.sun_family = AF_LOCAL;
        strncpy( addr.sun_path, psz_unix_path, sizeof( addr.sun_path ) - 1 );
        addr.sun_path[sizeof( addr.sun_path ) - 1] = '\0';

        if (bind (i_socket, (struct sockaddr *)&addr, sizeof (addr))
         && (errno == EADDRINUSE)
         && connect (i_socket, (struct sockaddr *)&addr, sizeof (addr))
         && (errno == ECONNREFUSED))
        {
            msg_Info (p_intf, "Removing dead UNIX socket: %s", psz_unix_path);
            unlink (psz_unix_path);

            if (bind (i_socket, (struct sockaddr *)&addr, sizeof (addr)))
            {
                msg_Err (p_intf, "cannot bind UNIX socket at %s: %s",
                         psz_unix_path, vlc_strerror_c(errno));
                free (psz_unix_path);
                net_Close (i_socket);
                return VLC_EGENERIC;
            }
        }

        if( listen( i_socket, 1 ) )
        {
            msg_Warn (p_intf, "can't listen on socket: %s",
                      vlc_strerror_c(errno));
            free( psz_unix_path );
            net_Close( i_socket );
            return VLC_EGENERIC;
        }

        /* FIXME: we need a core function to merge listening sockets sets */
        pi_socket = calloc( 2, sizeof( int ) );
        if( pi_socket == NULL )
        {
            free( psz_unix_path );
            net_Close( i_socket );
            return VLC_ENOMEM;
        }
        pi_socket[0] = i_socket;
        pi_socket[1] = -1;
    }
#endif /* AF_LOCAL */
#endif /* !_WIN32 */

    if( ( pi_socket == NULL ) &&
        ( psz_host = var_InheritString( p_intf, "rc-host" ) ) != NULL )
    {
        vlc_url_t url;

        vlc_UrlParse( &url, psz_host );
        if( url.psz_host == NULL )
        {
            vlc_UrlClean( &url );
            char *psz_backward_compat_host;
            if( asprintf( &psz_backward_compat_host, "//%s", psz_host ) < 0 )
            {
                free( psz_host );
                return VLC_EGENERIC;
            }
            free( psz_host );
            psz_host = psz_backward_compat_host;
            vlc_UrlParse( &url, psz_host );
        }

        msg_Dbg( p_intf, "base: %s, port: %d", url.psz_host, url.i_port );

        pi_socket = net_ListenTCP(p_this, url.psz_host, url.i_port);
        if( pi_socket == NULL )
        {
            msg_Warn( p_intf, "can't listen to %s port %i",
                      url.psz_host, url.i_port );
            vlc_UrlClean( &url );
            free( psz_host );
            return VLC_EGENERIC;
        }

        vlc_UrlClean( &url );
        free( psz_host );
    }

    intf_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(p_sys == NULL) )
    {
        net_ListenClose( pi_socket );
        free( psz_unix_path );
        return VLC_ENOMEM;
    }

    p_intf->p_sys = p_sys;
    p_sys->pi_socket_listen = pi_socket;
    p_sys->i_socket = -1;
#ifdef AF_LOCAL
    p_sys->psz_unix_path = psz_unix_path;
#endif
    vlc_mutex_init( &p_sys->status_lock );
    p_sys->last_state = VLC_PLAYER_STATE_STOPPED;
    p_sys->b_input_buffering = false;
    p_sys->playlist = vlc_intf_GetMainPlaylist(p_intf);;
    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

#if VLC_WINSTORE_APP
    p_sys->b_quiet = true;
#elif defined(_WIN32)
    p_sys->b_quiet = var_InheritBool( p_intf, "rc-quiet" );
    if( !p_sys->b_quiet )
        intf_consoleIntroMsg( p_intf );
#endif

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    msg_rc( "%s", _("Remote control interface initialized. Type `help' for help.") );

    static struct vlc_player_cbs const player_cbs =
    {
        .on_state_changed = player_on_state_changed,
        .on_buffering_changed = player_on_buffering_changed,
        .on_rate_changed = player_on_rate_changed,
        .on_position_changed = player_on_position_changed,
    };
    vlc_player_Lock(player);
    p_sys->player_listener =
        vlc_player_AddListener(player, &player_cbs, p_intf);
    if (!p_sys->player_listener)
    {
        vlc_player_Unlock(player);
        goto error;
    }

    static struct vlc_player_aout_cbs const player_aout_cbs =
    {
        .on_volume_changed = player_aout_on_volume_changed,
    };
    p_sys->player_aout_listener =
        vlc_player_aout_AddListener(player, &player_aout_cbs, p_intf);
    vlc_player_Unlock(player);
    if (!p_sys->player_aout_listener)
        goto error;

    return VLC_SUCCESS;

error:
    if (p_sys->player_listener)
    {
        vlc_player_Lock(player);
        vlc_player_RemoveListener(player, p_sys->player_listener);
        vlc_player_Unlock(player);
    }
    net_ListenClose( pi_socket );
    free( psz_unix_path );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);
    vlc_player_Lock(player);
    vlc_player_aout_RemoveListener(player, p_sys->player_aout_listener);
    vlc_player_RemoveListener(player, p_sys->player_listener);
    vlc_player_Unlock(player);

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    net_ListenClose( p_sys->pi_socket_listen );
    if( p_sys->i_socket != -1 )
        net_Close( p_sys->i_socket );
#if defined(AF_LOCAL) && !defined(_WIN32)
    if( p_sys->psz_unix_path != NULL )
    {
        unlink( p_sys->psz_unix_path );
        free( p_sys->psz_unix_path );
    }
#endif
    free( p_sys );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POS_TEXT N_("Show stream position")
#define POS_LONGTEXT N_("Show the current position in seconds within the " \
                        "stream from time to time." )

#define TTY_TEXT N_("Fake TTY")
#define TTY_LONGTEXT N_("Force the rc module to use stdin as if it was a TTY.")

#define UNIX_TEXT N_("UNIX socket command input")
#define UNIX_LONGTEXT N_("Accept commands over a Unix socket rather than " \
                         "stdin." )

#define HOST_TEXT N_("TCP command input")
#define HOST_LONGTEXT N_("Accept commands over a socket rather than stdin. " \
            "You can set the address and port the interface will bind to." )

#ifdef _WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the rc interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin()
    set_shortname(N_("RC"))
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_description(N_("Remote control interface"))
    add_bool("rc-show-pos", false, POS_TEXT, POS_LONGTEXT, true)

#ifdef _WIN32
    add_bool("rc-quiet", false, QUIET_TEXT, QUIET_LONGTEXT, false)
#else
#if defined (HAVE_ISATTY)
    add_bool("rc-fake-tty", false, TTY_TEXT, TTY_LONGTEXT, true)
#endif
#ifdef AF_LOCAL
    add_string("rc-unix", NULL, UNIX_TEXT, UNIX_LONGTEXT, true)
#endif
#endif
    add_string("rc-host", NULL, HOST_TEXT, HOST_LONGTEXT, true)

    set_capability("interface", 20)

    set_callbacks(Activate, Deactivate)
    add_shortcut("cli", "rc", "oldrc")
vlc_module_end()
