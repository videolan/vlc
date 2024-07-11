/*****************************************************************************
 * vlc_pipewire.h: common PipeWire code
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdint.h>

struct vlc_pw_context;
struct vlc_logger;
struct spa_dict;
struct spa_hook;
struct pw_properties;
struct pw_registry_events;

void vlc_pw_log(struct vlc_pw_context *ctx, int prio,
                const char *file, unsigned int line, const char *func,
                const char *fmt, ...);
int vlc_pw_perror(struct vlc_pw_context *ctx, const char *file,
                  unsigned int line, const char *func, const char *desc);

#define vlc_pw_log(ctx, prio, ...) \
        vlc_pw_log(ctx, prio, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define vlc_pw_error(ctx, ...) \
        vlc_pw_log(ctx, VLC_MSG_ERR, __VA_ARGS__)
#define vlc_pw_warn(ctx, ...) \
        vlc_pw_log(ctx, VLC_MSG_WARN, __VA_ARGS__)
#define vlc_pw_debug(ctx, ...) \
        vlc_pw_log(ctx, VLC_MSG_DBG, __VA_ARGS__)
#define vlc_pw_perror(ctx, desc) \
        vlc_pw_perror(ctx, __FILE__, __LINE__, __func__, desc)

void vlc_pw_lock(struct vlc_pw_context *ctx);
void vlc_pw_unlock(struct vlc_pw_context *ctx);
void vlc_pw_signal(struct vlc_pw_context *ctx);
void vlc_pw_wait(struct vlc_pw_context *ctx);

struct pw_stream *vlc_pw_stream_new(struct vlc_pw_context *ctx,
                                    const char *name, struct pw_properties *);

void vlc_pw_roundtrip_unlocked(struct vlc_pw_context *ctx);

int vlc_pw_registry_listen(struct vlc_pw_context *ctx, struct spa_hook *hook,
                           const struct pw_registry_events *, void *);

void vlc_pw_disconnect(struct vlc_pw_context *ctx);
struct vlc_pw_context *vlc_pw_connect(vlc_object_t *obj, const char *name);
