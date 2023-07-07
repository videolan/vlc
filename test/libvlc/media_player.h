/*
 * media_player.h - media player test common definitions
 *
 */

/**********************************************************************
 *  Copyright (C) 2023 VideoLAN and its authors                       *
 *                                                                    *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#ifndef TEST_MEDIA_PLAYER_H
#define TEST_MEDIA_PLAYER_H

struct mp_event_ctx
{
    vlc_sem_t sem_ev;
    vlc_sem_t sem_done;
    const struct libvlc_event_t *ev;
};

static inline void mp_event_ctx_init(struct mp_event_ctx *ctx)
{
    vlc_sem_init(&ctx->sem_ev, 0);
    vlc_sem_init(&ctx->sem_done, 0);
    ctx->ev = NULL;
}

static inline const struct libvlc_event_t *mp_event_ctx_wait_event(struct mp_event_ctx *ctx)
{
    vlc_sem_wait(&ctx->sem_ev);
    assert(ctx->ev != NULL);
    return ctx->ev;
}

static inline void mp_event_ctx_release(struct mp_event_ctx *ctx)
{
    assert(ctx->ev != NULL);
    ctx->ev = NULL;
    vlc_sem_post(&ctx->sem_done);
}

static inline void mp_event_ctx_wait(struct mp_event_ctx *ctx)
{
    mp_event_ctx_wait_event(ctx);
    mp_event_ctx_release(ctx);
}

static inline void mp_event_ctx_on_event(const struct libvlc_event_t *event, void *data)
{
    struct mp_event_ctx *ctx = data;

    assert(ctx->ev == NULL);
    ctx->ev = event;

    vlc_sem_post(&ctx->sem_ev);
    vlc_sem_wait(&ctx->sem_done);
    assert(ctx->ev == NULL);
}

#endif /* TEST_MEDIA_PLAYER_H */
