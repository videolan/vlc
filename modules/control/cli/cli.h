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

#include <vlc_common.h>
#include <vlc_playlist.h>

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
    char *psz_unix_path;
#else
    HANDLE hConsoleIn;
    bool b_quiet;
#endif
    int *pi_socket_listen;
    int i_socket;
};

VLC_FORMAT(2, 3)
void msg_print(intf_thread_t *p_intf, const char *psz_fmt, ...);

#define msg_rc(...) msg_print(p_intf, __VA_ARGS__)
#define STATUS_CHANGE "status change: "

extern const struct vlc_player_cbs player_cbs;
extern const struct vlc_player_aout_cbs player_aout_cbs;

void PlayerPause(intf_thread_t *intf);
void PlayerFastForward(intf_thread_t *intf);
void PlayerRewind(intf_thread_t *intf);
void PlayerFaster(intf_thread_t *intf);
void PlayerSlower(intf_thread_t *intf);
void PlayerNormal(intf_thread_t *intf);
void PlayerFrame(intf_thread_t *intf);
void PlayerChapterPrev(intf_thread_t *intf);
void PlayerChapterNext(intf_thread_t *intf);
void PlayerTitlePrev(intf_thread_t *intf);
void PlayerTitleNext(intf_thread_t *intf);
void Input(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval);
void PlayerItemInfo(intf_thread_t *intf);
void PlayerGetTime(intf_thread_t *intf);
void PlayerGetLength(intf_thread_t *intf);
void PlayerGetTitle(intf_thread_t *intf);
void PlayerVoutSnapshot(intf_thread_t *intf);
void Volume(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval);
void VolumeMove(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval);
void VideoConfig(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval);
void AudioDevice(intf_thread_t *intf, char const *cmd, vlc_value_t cur);
void AudioChannel(intf_thread_t *intf, char const *cmd, vlc_value_t cur);
void Statistics(intf_thread_t *intf);
void IsPlaying(intf_thread_t *intf);

void PlaylistPrev(intf_thread_t *intf);
void PlaylistNext(intf_thread_t *intf);
void PlaylistPlay(intf_thread_t *intf);
void PlaylistStop(intf_thread_t *intf);
void PlaylistClear(intf_thread_t *intf);
void PlaylistSort(intf_thread_t *intf);
void PlaylistList(intf_thread_t *intf);
void PlaylistStatus(intf_thread_t *intf);
void Playlist(intf_thread_t *intf, char const *psz_cmd, vlc_value_t newval);
