/*****************************************************************************
 * vout_internal.h : Internal vout definitions
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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


#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _VOUT_INTERNAL_H
#define _VOUT_INTERNAL_H 1

#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>
#include <vlc_vout_display.h>
#include "vout_control.h"
#include "control.h"
#include "snapshot.h"
#include "statistic.h"
#include "chrono.h"

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* */
struct vout_thread_sys_t
{
    /* module */
    char       *psz_module_name;

    /* Video output configuration */
    config_chain_t *p_cfg;

    /* */
    video_format_t  fmt_render;      /* render format (from the decoder) */
    video_format_t  fmt_in;            /* input (modified render) format */
    video_format_t  fmt_out;     /* output format (for the video output) */

    /* Thread & synchronization */
    vlc_thread_t    thread;
    vlc_cond_t      change_wait;
    bool            b_ready;
    bool            b_done;
    bool            b_error;
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
        int         qtype;
        bool        is_interlaced;
        picture_t   *decoded;
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
    unsigned int    i_par_num;           /**< monitor pixel aspect-ratio */
    unsigned int    i_par_den;           /**< monitor pixel aspect-ratio */
    bool            is_late_dropped;

    /* Statistics */
    vout_statistic_t statistic;

    /* Filter chain */
    char           *psz_filter_chain;
    bool            b_filter_change;

    /* Video filter2 chain */
    vlc_mutex_t     vfilter_lock;
    filter_chain_t *p_vf2_chain;

    /* Snapshot interface */
    vout_snapshot_t snapshot;

    /* Subpicture unit */
    spu_t          *p_spu;

    /* */
    vlc_mouse_t     mouse;

    /* */
    vlc_mutex_t         picture_lock;                 /**< picture heap lock */
    picture_pool_t      *private_pool;
    picture_pool_t      *display_pool;
    picture_pool_t      *decoder_pool;
    picture_fifo_t      *decoder_fifo;
    bool                is_decoder_pool_slow;
    vout_chrono_t       render;           /**< picture render time estimator */

    vlc_mutex_t         change_lock;                 /**< thread change lock */

    uint16_t            i_changes;          /**< changes made to the thread.
                                                      \see \ref vout_changes */
};

/** \defgroup vout_changes Flags for changes
 * These flags are set in the vout_thread_t::i_changes field when another
 * thread changed a variable
 * @{
 */
/** cropping parameters changed */
#define VOUT_CROP_CHANGE        0x1000
/** aspect ratio changed */
#define VOUT_ASPECT_CHANGE      0x2000
/**@}*/

/* TODO to move them to vlc_vout.h */
void vout_ControlChangeFullscreen(vout_thread_t *, bool fullscreen);
void vout_ControlChangeOnTop(vout_thread_t *, bool is_on_top);
void vout_ControlChangeDisplayFilled(vout_thread_t *, bool is_filled);
void vout_ControlChangeZoom(vout_thread_t *, int num, int den);

/* */
void vout_IntfInit( vout_thread_t * );

/* */
int  vout_OpenWrapper (vout_thread_t *, const char *);
void vout_CloseWrapper(vout_thread_t *);
int  vout_InitWrapper(vout_thread_t *);
void vout_EndWrapper(vout_thread_t *);
int  vout_ManageWrapper(vout_thread_t *);
void vout_RenderWrapper(vout_thread_t *, picture_t *);
void vout_DisplayWrapper(vout_thread_t *, picture_t *);

/* */
int spu_ProcessMouse(spu_t *, const vlc_mouse_t *, const video_format_t *);

/* */
int vout_ShowTextRelative( vout_thread_t *, int, const char *, const text_style_t *, int, int, int, mtime_t );

#endif

