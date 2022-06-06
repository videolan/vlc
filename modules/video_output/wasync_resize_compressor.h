/**
 * @file wasync_resize_compressor.h
 * @brief Windows asynchronous resize compressor helper
 */
/*****************************************************************************
 * Copyright © 2022 VideoLAN and VideoLAN Authors
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

#ifndef WASYNC_RESIZE_COMPRESSOR_H
#define WASYNC_RESIZE_COMPRESSOR_H 1

#include <vlc_window.h>
#include <vlc_threads.h>

typedef struct vlc_wasync_resize_compressor {
    vlc_timer_t timer;
    vlc_mutex_t lock;
    vlc_window_t * wnd;
    unsigned requested_width;
    unsigned requested_height;
    unsigned current_width;
    unsigned current_height;
    bool resizing;
} vlc_wasync_resize_compressor_t;

static void wasync_resize_compressor_callback(void *data) {
    vlc_wasync_resize_compressor_t * compressor = (vlc_wasync_resize_compressor_t *) data;
    vlc_mutex_lock(&compressor->lock);
    while (compressor->requested_width != compressor->current_width ||
           compressor->requested_height != compressor->current_height) {
        unsigned h = compressor->requested_height;
        unsigned w = compressor->requested_width;
        vlc_mutex_unlock(&compressor->lock);

        vlc_window_ReportSize(compressor->wnd, w, h);

        vlc_mutex_lock(&compressor->lock);
        compressor->current_width = w;
        compressor->current_height = h;
    }
    compressor->resizing = false;
    vlc_mutex_unlock(&compressor->lock);
}

static inline int vlc_wasync_resize_compressor_init(vlc_wasync_resize_compressor_t *compressor,
                                                     vlc_window_t *window) {
    compressor->timer = NULL;
    vlc_mutex_init(&compressor->lock);
    compressor->requested_width = 0;
    compressor->requested_height = 0;
    compressor->current_width = 0;
    compressor->current_height = 0;
    compressor->resizing = false;
    compressor->wnd = window;
    if (vlc_timer_create(&compressor->timer, wasync_resize_compressor_callback,
                         compressor) != VLC_SUCCESS)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static inline void vlc_wasync_resize_compressor_destroy(vlc_wasync_resize_compressor_t *compressor) {
    if (compressor->timer != NULL)
        vlc_timer_destroy(compressor->timer);
}

static inline void vlc_wasync_resize_compressor_dropOrWait(vlc_wasync_resize_compressor_t *compressor) {
    vlc_timer_disarm(compressor->timer);
}

static inline void vlc_wasync_resize_compressor_reportSize(vlc_wasync_resize_compressor_t *compressor,
                                                           unsigned width, unsigned height) {
    vlc_mutex_lock(&compressor->lock);
    compressor->requested_width = width;
    compressor->requested_height = height;
    if (compressor->resizing == false) {
        vlc_timer_schedule_asap(compressor->timer, VLC_TIMER_FIRE_ONCE);
        compressor->resizing = true;
    }
    vlc_mutex_unlock(&compressor->lock);
}

#endif /* WASYNC_RESIZE_COMPRESSOR_H */
