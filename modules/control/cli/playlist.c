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
#include <vlc_playlist.h>
#include <vlc_url.h>

#include "cli.h"

#ifndef HAVE_WORDEXP
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
#endif

static void print_playlist(intf_thread_t *p_intf, vlc_playlist_t *playlist)
{
    size_t count = vlc_playlist_Count(playlist);
    size_t current = vlc_playlist_GetCurrentIndex(playlist);

    for (size_t i = 0; i < count; ++i)
    {
        vlc_playlist_item_t *plitem = vlc_playlist_Get(playlist, i);
        input_item_t *item = vlc_playlist_item_GetMedia(plitem);
        vlc_tick_t len = item->i_duration;
        char selected = (i == current) ? '*' : ' ';

        if (len != INPUT_DURATION_INDEFINITE && len != VLC_TICK_INVALID)
        {
            char buf[MSTRTIME_MAX_SIZE];
            secstotimestr(buf, SEC_FROM_VLC_TICK(len));
            msg_rc("| %c%zu %s (%s)", selected, i, item->psz_name, buf);
        }
        else
            msg_rc("| %c%zu %s", selected, i, item->psz_name);
    }
}

static int PlaylistDoVoid(intf_thread_t *intf, int (*cb)(vlc_playlist_t *))
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    int ret;

    vlc_playlist_Lock(playlist);
    ret = cb(playlist);
    vlc_playlist_Unlock(playlist);
    return ret;
}

static int PlaylistPrev(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, vlc_playlist_Prev);
}

static int PlaylistNext(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, vlc_playlist_Next);
}

static int PlaylistPlay(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, vlc_playlist_Start);
}

static int PlaylistDoStop(vlc_playlist_t *playlist)
{
    vlc_playlist_Stop(playlist);
    return 0;
}

static int PlaylistStop(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, PlaylistDoStop);
}

static int PlaylistDoClear(vlc_playlist_t *playlist)
{
    vlc_playlist_Stop(playlist);
    vlc_playlist_Clear(playlist);
    return 0;
}

static int PlaylistClear(intf_thread_t *intf, const char *const *args,
                         size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, PlaylistDoClear);
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

static int PlaylistSort(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    (void) args; (void) count;
    return PlaylistDoVoid(intf, PlaylistDoSort);
}

static int PlaylistList(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    msg_print(intf, "+----[ Playlist ]");
    vlc_playlist_Lock(playlist);
    print_playlist(intf, playlist);
    vlc_playlist_Unlock(playlist);
    msg_print(intf, "+----[ End of playlist ]");
    (void) args; (void) count;
    return 0;
}

static int PlaylistRepeatCommon(intf_thread_t *intf, const char *const *args,
                                size_t count,
                                enum vlc_playlist_playback_repeat on_mode)

{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    vlc_playlist_Lock(playlist);

    enum vlc_playlist_playback_repeat cur_mode =
                                      vlc_playlist_GetPlaybackRepeat(playlist);
    enum vlc_playlist_playback_repeat new_mode;

    if (cur_mode == on_mode)
        new_mode = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    else
        new_mode = on_mode;

    if (count > 1)
    {
        if (strcmp(args[1], "on") == 0)
            new_mode = on_mode;
        if (strcmp(args[1], "off") == 0)
            new_mode = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    }

    if (new_mode != cur_mode)
        vlc_playlist_SetPlaybackRepeat(playlist, new_mode);

    vlc_playlist_Unlock(playlist);
    return 0;
}

static int PlaylistRepeat(intf_thread_t *intf, const char *const *args,
                          size_t count)
{
    return PlaylistRepeatCommon(intf, args, count,
                                VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT);
}

static int PlaylistLoop(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    return PlaylistRepeatCommon(intf, args, count,
                                VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);
}

static int PlaylistRandom(intf_thread_t *intf, const char *const *args,
                          size_t count)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;

    vlc_playlist_Lock(playlist);

    enum vlc_playlist_playback_order cur_mode =
                                       vlc_playlist_GetPlaybackOrder(playlist);
    enum vlc_playlist_playback_order new_mode;

    if (cur_mode == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
        new_mode = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    else
        new_mode = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;

    if (count > 1)
    {
        if (strcmp(args[1], "on") == 0)
            new_mode = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
        if (strcmp(args[1], "off") == 0)
            new_mode = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }

    if (new_mode != cur_mode)
        vlc_playlist_SetPlaybackOrder(playlist, new_mode);

    vlc_playlist_Unlock(playlist);
    return 0;
}

static int PlaylistGoto(intf_thread_t *intf, const char *const *args,
                        size_t n_args)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    const char *arg = n_args > 1 ? args[1] : "";
    unsigned long long index = atoll(arg);

    vlc_playlist_Lock(playlist);

    int ret = vlc_playlist_PlayAt(playlist, index);
    if (ret) {
        size_t count = vlc_playlist_Count(playlist);

        msg_print(intf,
                  vlc_ngettext("Playlist has only %zu element",
                               "Playlist has only %zu elements", count),
                  count);
    }

    vlc_playlist_Unlock(playlist);
    return ret;
}

static int PlaylistAddCommon(intf_thread_t *intf, const char *const *args,
                             size_t n_args, bool play)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    size_t count;
    int ret = 0;

    vlc_playlist_Lock(playlist);
    count = vlc_playlist_Count(playlist);
#ifdef HAVE_WORDEXP

    for (size_t i = 1; i < n_args;)
    {
        input_item_t *item;

        if (strstr(args[i], "://" ) != NULL)
            item = input_item_New(args[i], NULL);
        else
        {
            char *url = vlc_path2uri(args[i], NULL);

            if (url != NULL)
            {
                item = input_item_New(url, NULL);
                free(url);
            }
            else
                item = NULL;
        }

        i++;

        /* Check if following argument(s) are input item options prefixed with
         * a colon.
         */
        while (i < n_args && args[i][0] == ':')
        {
            if (likely(item != NULL)
             && input_item_AddOption(item, args[i] + 1,
                                     VLC_INPUT_OPTION_TRUSTED))
            {
                input_item_Release(item);
                item = NULL;
            }
            i++;
        }

        if (unlikely(item == NULL))
        {
            ret = VLC_ENOMEM;
            continue;
        }

        if (vlc_playlist_InsertOne(playlist, count, item) == VLC_SUCCESS)
        {
            if (play)
                vlc_playlist_PlayAt(playlist, count);

            count++;
        }

        input_item_Release(item);
    }
#else
    const char *arg = n_args > 1 ? args[1] : "";

    input_item_t *item = parse_MRL( arg );

    if (item != NULL)
    {
        msg_print(intf, "Trying to %s %s to playlist.",
                  play ? "add" : "enqueue", arg);

        if (vlc_playlist_InsertOne(playlist, count, item) == VLC_SUCCESS
         && play)
            vlc_playlist_PlayAt(playlist, count);

        input_item_Release(item);
    }
#endif
    vlc_playlist_Unlock(playlist);
    return ret;
}

static int PlaylistAdd(intf_thread_t *intf, const char *const *args,
                       size_t count)
{
    return PlaylistAddCommon(intf, args, count, true);
}

static int PlaylistEnqueue(intf_thread_t *intf, const char *const *args,
                           size_t count)
{
    return PlaylistAddCommon(intf, args, count, false);
}

static int PlaylistMove(intf_thread_t *intf, const char *const *args,
                        size_t count)
{
    vlc_playlist_t *playlist = intf->p_sys->playlist;
    int ret;

    if (count != 3)
    {
        msg_print(intf, "%s expects two parameters", args[0]);
        return VLC_EGENERIC /*EINVAL*/;
    }

    size_t from = strtoul(args[1], NULL, 0);
    size_t to = strtoul(args[2], NULL, 0);

    vlc_playlist_Lock(playlist);
    size_t size = vlc_playlist_Count(playlist);

    if (from < size && to < size)
    {
        vlc_playlist_Move(playlist, from, 1, to);
        ret = 0;
    }
    else
    {
        msg_print(intf, vlc_ngettext("Playlist has only %zu element",
                                     "Playlist has only %zu elements", size),
                  size);
        ret = VLC_ENOITEM;
    }
    vlc_playlist_Unlock(playlist);
    return ret;
}

static const struct cli_handler cmds[] =
{
    { "playlist", PlaylistList },
    { "sort", PlaylistSort },
    { "play", PlaylistPlay },
    { "stop", PlaylistStop },
    { "clear", PlaylistClear },
    { "prev", PlaylistPrev },
    { "next", PlaylistNext },
    { "add", PlaylistAdd },
    { "repeat", PlaylistRepeat },
    { "loop", PlaylistLoop },
    { "random", PlaylistRandom },
    { "enqueue", PlaylistEnqueue },
    { "goto", PlaylistGoto },
    { "move", PlaylistMove },
};

void RegisterPlaylist(intf_thread_t *intf)
{
    RegisterHandlers(intf, cmds, ARRAY_SIZE(cmds),
                     vlc_intf_GetMainPlaylist(intf));
}
