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
    void *player_cli;

    /* playlist */
    vlc_playlist_t              *playlist;

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

void PlayerPause(intf_thread_t *intf, const char *const *, size_t);
void PlayerFastForward(intf_thread_t *intf, const char *const *, size_t);
void PlayerRewind(intf_thread_t *intf, const char *const *, size_t);
void PlayerFaster(intf_thread_t *intf, const char *const *, size_t);
void PlayerSlower(intf_thread_t *intf, const char *const *, size_t);
void PlayerNormal(intf_thread_t *intf, const char *const *, size_t);
void PlayerFrame(intf_thread_t *intf, const char *const *, size_t);
void PlayerChapterPrev(intf_thread_t *intf, const char *const *, size_t);
void PlayerChapterNext(intf_thread_t *intf, const char *const *, size_t);
void PlayerTitlePrev(intf_thread_t *intf, const char *const *, size_t);
void PlayerTitleNext(intf_thread_t *intf, const char *const *, size_t);
void Input(intf_thread_t *intf, const char *const *, size_t);
void PlayerItemInfo(intf_thread_t *intf, const char *const *, size_t);
void PlayerGetTime(intf_thread_t *intf, const char *const *, size_t);
void PlayerGetLength(intf_thread_t *intf, const char *const *, size_t);
void PlayerGetTitle(intf_thread_t *intf, const char *const *, size_t);
void PlayerVoutSnapshot(intf_thread_t *intf, const char *const *, size_t);
void PlayerFullscreen(intf_thread_t *intf, const char *const *, size_t);
void Volume(intf_thread_t *intf, const char *const *, size_t);
void VolumeMove(intf_thread_t *intf, const char *const *, size_t);
void VideoConfig(intf_thread_t *intf, const char *const *, size_t);
void AudioDevice(intf_thread_t *intf, const char *const *, size_t);
void AudioChannel(intf_thread_t *intf, const char *const *, size_t);
void Statistics(intf_thread_t *intf, const char *const *, size_t);
void IsPlaying(intf_thread_t *intf, const char *const *, size_t);

void *RegisterPlayer(intf_thread_t *intf);
void DeregisterPlayer(intf_thread_t *intf, void *);

void PlaylistPrev(intf_thread_t *intf, const char *const *, size_t);
void PlaylistNext(intf_thread_t *intf, const char *const *, size_t);
void PlaylistPlay(intf_thread_t *intf, const char *const *, size_t);
void PlaylistStop(intf_thread_t *intf, const char *const *, size_t);
void PlaylistClear(intf_thread_t *intf, const char *const *, size_t);
void PlaylistSort(intf_thread_t *intf, const char *const *, size_t);
void PlaylistList(intf_thread_t *intf, const char *const *, size_t);
void PlaylistStatus(intf_thread_t *intf, const char *const *, size_t);
void Playlist(intf_thread_t *intf, const char *const *, size_t);
