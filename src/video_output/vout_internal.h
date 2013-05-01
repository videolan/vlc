/*****************************************************************************
 * vout_internal.h : Internal vout definitions
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#ifndef LIBVLC_VOUT_INTERNAL_H
#define LIBVLC_VOUT_INTERNAL_H 1

#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>
#include <vlc_vout_display.h>
#include <vlc_vout_wrapper.h>
#include "vout_control.h"
#include "control.h"
#include "snapshot.h"
#include "statistic.h"
#include "chrono.h"

/* It should be high enough to absorbe jitter due to difficult picture(s)
 * to decode but not too high as memory is not that cheap.
 *
 * It can be made lower at compilation time if needed, but performance
 * may be degraded.
 */
#define VOUT_MAX_PICTURES (20)

/* */
struct vout_thread_sys_t
{
    /* Splitter module if used */
    char            *splitter_name;

    /* Input thread for dvd menu interactions */
    vlc_object_t    *input;

    /* */
    video_format_t  original;   /* Original format ie coming from the decoder */
    unsigned        dpb_size;

    /* Snapshot interface */
    vout_snapshot_t snapshot;

    /* Statistics */
    vout_statistic_t statistic;

    /* Subpicture unit */
    vlc_mutex_t     spu_lock;
    spu_t           *spu;
    vlc_fourcc_t    spu_blend_chroma;
    filter_t        *spu_blend;

    /* Video output window */
    struct {
        bool              is_unused;
        vout_window_cfg_t cfg;
        vout_window_t     *object;
    } window;

    /* Thread & synchronization */
    vlc_thread_t    thread;
    bool            dead;
    vout_control_t  control;

    /* */
    struct {
        char           *title;
        vout_display_t *vd;
        bool           use_dr;
        picture_t      *filtered;
    } display;

    struct {
        mtime_t     date;
        mtime_t     timestamp;
        bool        is_interlaced;
        picture_t   *decoded;
        picture_t   *current;
        picture_t   *next;
    } displayed;

    struct {
        mtime_t     last;
        mtime_t     timestamp;
    } step;

    struct {
        bool        is_on;
        mtime_t     date;
    } pause;

    /* OSD title configuration */
    struct {
        bool        show;
        mtime_t     timeout;
        int         position;
    } title;

    /* */
    bool            is_late_dropped;

    /* Video filter2 chain */
    struct {
        vlc_mutex_t     lock;
        char            *configuration;
        video_format_t  format;
        filter_chain_t  *chain_static;
        filter_chain_t  *chain_interactive;
    } filter;

    /* */
    vlc_mouse_t     mouse;

    /* */
    vlc_mutex_t     picture_lock;                 /**< picture heap lock */
    picture_pool_t  *private_pool;
    picture_pool_t  *display_pool;
    picture_pool_t  *decoder_pool;
    picture_fifo_t  *decoder_fifo;
    vout_chrono_t   render;           /**< picture render time estimator */
};

/* TODO to move them to vlc_vout.h */
void vout_ControlChangeFullscreen(vout_thread_t *, bool fullscreen);
void vout_ControlChangeOnTop(vout_thread_t *, bool is_on_top);
void vout_ControlChangeDisplayFilled(vout_thread_t *, bool is_filled);
void vout_ControlChangeZoom(vout_thread_t *, int num, int den);
void vout_ControlChangeSampleAspectRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ControlChangeCropRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ControlChangeCropWindow(vout_thread_t *, int x, int y, int width, int height);
void vout_ControlChangeCropBorder(vout_thread_t *, int left, int top, int right, int bottom);
void vout_ControlChangeFilters(vout_thread_t *, const char *);
void vout_ControlChangeSubSources(vout_thread_t *, const char *);
void vout_ControlChangeSubFilters(vout_thread_t *, const char *);
void vout_ControlChangeSubMargin(vout_thread_t *, int);

/* */
void vout_IntfInit( vout_thread_t * );
void vout_IntfReinit( vout_thread_t * );

/* */
int  vout_OpenWrapper (vout_thread_t *, const char *, const vout_display_state_t *);
void vout_CloseWrapper(vout_thread_t *, vout_display_state_t *);
int  vout_InitWrapper(vout_thread_t *);
void vout_EndWrapper(vout_thread_t *);
void vout_ManageWrapper(vout_thread_t *);

/* */
int spu_ProcessMouse(spu_t *, const vlc_mouse_t *, const video_format_t *);
void spu_Attach( spu_t *, vlc_object_t *input, bool );
void spu_ChangeMargin(spu_t *, int);

#endif
