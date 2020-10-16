/*****************************************************************************
 * playlist.c : remote control stdin/stdout module for vlc
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

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_input_item.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_url.h>

#include "cli.h"

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

void PlaylistPrev(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Prev);
}

void PlaylistNext(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Next);
}

void PlaylistPlay(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, vlc_playlist_Start);
}

static int PlaylistDoStop(vlc_playlist_t *playlist)
{
    vlc_playlist_Stop(playlist);
    return 0;
}

void PlaylistStop(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, PlaylistDoStop);
}

static int PlaylistDoClear(vlc_playlist_t *playlist)
{
    PlaylistDoStop(playlist);
    vlc_playlist_Clear(playlist);
    return 0;
}

void PlaylistClear(intf_thread_t *intf)
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

void PlaylistSort(intf_thread_t *intf)
{
    PlaylistDoVoid(intf, PlaylistDoSort);
}

void PlaylistList(intf_thread_t *intf)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    msg_print(intf, "+----[ Playlist ]");
    vlc_playlist_Lock(playlist);
    print_playlist(intf, playlist);
    vlc_playlist_Unlock(playlist);
    msg_print(intf, "+----[ End of playlist ]");
}

void PlaylistStatus(intf_thread_t *intf)
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

void Playlist(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval)
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
