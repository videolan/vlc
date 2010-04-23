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
#include "vout_control.h"
#include "snapshot.h"
#include "statistic.h"
#include "chrono.h"

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* */
typedef struct vout_sys_t vout_sys_t;

/* */
struct vout_thread_sys_t
{
    /* module */
    char       *psz_module_name;

    /* Video output configuration */
    config_chain_t *p_cfg;

    /* Place holder for the vout_wrapper code */
    vout_sys_t      *p_sys;

    /* Thread & synchronization */
    vlc_thread_t    thread;
    vlc_cond_t      change_wait;
    bool            b_ready;
    bool            b_done;
    bool            b_error;

    /* */
    bool            b_picture_empty;
    vlc_cond_t      picture_wait;
    struct {
        mtime_t     date;
        mtime_t     timestamp;
        int         qtype;
        bool        is_interlaced;
        picture_t   *decoded;
    } displayed;

    struct {
        bool        is_requested;
        mtime_t     last;
        mtime_t     timestamp;
    } step;

    struct {
        bool        is_on;
        mtime_t     date;
    } pause;

    struct {
        bool        show;
        mtime_t     timeout;
        int         position;
        char        *value;
    } title;

    /* */
    vlc_mutex_t     vfilter_lock;         /**< video filter2 lock */

    /* */
    unsigned int    i_par_num;           /**< monitor pixel aspect-ratio */
    unsigned int    i_par_den;           /**< monitor pixel aspect-ratio */
    bool            is_late_dropped;

    /* Statistics */
    vout_statistic_t statistic;

    /* Filter chain */
    bool           b_first_vout;  /* True if it is the first vout of the filter chain */
    char           *psz_filter_chain;
    bool            b_filter_change;

    /* Video filter2 chain */
    filter_chain_t *p_vf2_chain;
    char           *psz_vf2;

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
    unsigned            b_fullscreen:1;       /**< toogle fullscreen display */
    unsigned            b_on_top:1; /**< stay always on top of other windows */
};

/** \defgroup vout_changes Flags for changes
 * These flags are set in the vout_thread_t::i_changes field when another
 * thread changed a variable
 * @{
 */
/** b_autoscale changed */
#define VOUT_SCALE_CHANGE       0x0008
/** b_on_top changed */
#define VOUT_ON_TOP_CHANGE      0x0010
/** b_fullscreen changed */
#define VOUT_FULLSCREEN_CHANGE  0x0040
/** i_zoom changed */
#define VOUT_ZOOM_CHANGE        0x0080
/** cropping parameters changed */
#define VOUT_CROP_CHANGE        0x1000
/** aspect ratio changed */
#define VOUT_ASPECT_CHANGE      0x2000
/**@}*/


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

#endif

