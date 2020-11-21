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

#include <stdio.h>
#include <vlc_common.h>
#include <vlc_playlist.h>

struct intf_sys_t
{
    vlc_thread_t thread;
    void *commands;
    void *player_cli;

    /* playlist */
    vlc_playlist_t              *playlist;

    vlc_mutex_t output_lock;
#ifndef _WIN32
    FILE *stream;
    int fd;
    char *psz_unix_path;
#else
    HANDLE hConsoleIn;
    bool b_quiet;
    int i_socket;
#endif
    int *pi_socket_listen;
};

VLC_FORMAT(2, 3)
void msg_print(intf_thread_t *p_intf, const char *psz_fmt, ...);

#define msg_rc(...) msg_print(p_intf, __VA_ARGS__)
#define STATUS_CHANGE "status change: "

struct cli_handler
{
    const char *name;
    int (*callback)(intf_thread_t *intf, const char *const *, size_t);
};

void RegisterHandlers(intf_thread_t *intf, const struct cli_handler *handlers,
                      size_t count);

void *RegisterPlayer(intf_thread_t *intf);
void DeregisterPlayer(intf_thread_t *intf, void *);

void RegisterPlaylist(intf_thread_t *intf);
