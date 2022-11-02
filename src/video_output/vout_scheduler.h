/*****************************************************************************
 * scheduler.h : vout internal scheduler
 *****************************************************************************
 * Copyright (C) 2022 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLC_VOUT_INTERNAL_SCHEDULER_H
#define LIBVLC_VOUT_INTERNAL_SCHEDULER_H

#include <vlc_picture.h>

typedef struct vlc_clock_t vlc_clock_t;
typedef struct vout_display_t vout_display_t;

struct vlc_vout_scheduler;
struct vlc_vout_scheduler_operations
{
    void (*put_picture)(struct vlc_vout_scheduler *scheduler, picture_t *pic);
    void (*flush)(struct vlc_vout_scheduler *scheduler);
    void (*signal_vsync)(struct vlc_vout_scheduler *scheduler, vlc_tick_t next_ts);
    void (*destroy)(struct vlc_vout_scheduler *scheduler);
};

struct vlc_vout_scheduler_callbacks
{
    bool (*wait_control)(void *opaque, vlc_tick_t deadline);
    picture_t * (*prepare_picture)(void *opaque, bool reuse_decoded, bool frame_by_frame);
    int (*render_picture)(void *opaque, picture_t *picture, bool render_now);
};

struct vlc_vout_scheduler
{
    const struct vlc_vout_scheduler_operations *ops;

    struct {
        void *sys;
        const struct vlc_vout_scheduler_callbacks *cbs;
    } owner;
};

VLC_MALLOC VLC_USED struct vlc_vout_scheduler *
vlc_vout_scheduler_New(vout_thread_t *vout, vlc_clock_t *clock,
                       vout_display_t *display,
                       const struct vlc_vout_scheduler_callbacks *cbs,
                       void *owner);

static inline void
vlc_vout_scheduler_Destroy(struct vlc_vout_scheduler *scheduler)
{
    scheduler->ops->destroy(scheduler);
}

static inline bool
vlc_vout_scheduler_WaitControl(struct vlc_vout_scheduler *scheduler,
                               vlc_tick_t deadline)
{
    return scheduler->owner.cbs->wait_control(
            scheduler->owner.sys, deadline);
}

static inline picture_t *
vlc_vout_scheduler_PreparePicture(struct vlc_vout_scheduler *scheduler,
                                  bool reuse_decoded, bool frame_by_frame)
{
    return scheduler->owner.cbs->prepare_picture(
            scheduler->owner.sys, reuse_decoded, frame_by_frame);
}

static inline int
vlc_vout_scheduler_RenderPicture(struct vlc_vout_scheduler *scheduler,
                                 picture_t *picture, bool render_now)
{
    return scheduler->owner.cbs->render_picture(
            scheduler->owner.sys, picture, render_now);
}


#endif
