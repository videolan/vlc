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

struct cli_client
{
    intf_thread_t *intf;
#ifndef _WIN32
    FILE *stream;
    int fd;
    vlc_mutex_t output_lock;
#endif
};

VLC_FORMAT(2, 3)
int cli_printf(struct cli_client *cl, const char *fmt, ...);

VLC_FORMAT(2, 3)
void msg_print(intf_thread_t *p_intf, const char *psz_fmt, ...);

#define msg_rc(...) cli_printf(cl, __VA_ARGS__)
#define STATUS_CHANGE "status change: "

typedef int (*cli_callback)(struct cli_client *, const char *const *, size_t,
                            void *);

struct cli_handler
{
    const char *name;
    cli_callback callback;
};

void RegisterHandlers(intf_thread_t *intf, const struct cli_handler *handlers,
                      size_t count, void *opaque);

void *RegisterPlayer(intf_thread_t *intf);
void DeregisterPlayer(intf_thread_t *intf, void *);

void RegisterPlaylist(intf_thread_t *intf);
