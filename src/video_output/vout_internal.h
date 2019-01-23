/*****************************************************************************
 * vout_internal.h : Internal vout definitions
 *****************************************************************************
 * Copyright (C) 2008-2018 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
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
#include "vout_wrapper.h"
#include "statistic.h"
#include "chrono.h"

/* It should be high enough to absorbe jitter due to difficult picture(s)
 * to decode but not too high as memory is not that cheap.
 *
 * It can be made lower at compilation time if needed, but performance
 * may be degraded.
 */
#define VOUT_MAX_PICTURES (20)

/**
 * Vout configuration
 */
typedef struct {
    vout_thread_t        *vout;
    const video_format_t *fmt;
    unsigned             dpb_size;
    vlc_mouse_event      mouse_event;
    void                 *opaque;
} vout_configuration_t;
#include "control.h"

struct vout_snapshot;

/* */
struct vout_thread_sys_t
{
    /* Splitter module if used */
    char            *splitter_name;

    /* Input thread for spu attachments */
    input_thread_t    *input;

    /* */
    video_format_t  original;   /* Original format ie coming from the decoder */
    unsigned        dpb_size;

    /* Snapshot interface */
    struct vout_snapshot *snapshot;

    /* Statistics */
    vout_statistic_t statistic;

    /* Subpicture unit */
    vlc_mutex_t     spu_lock;
    spu_t           *spu;
    vlc_fourcc_t    spu_blend_chroma;
    filter_t        *spu_blend;

    /* Thread & synchronization */
    vlc_thread_t    thread;
    bool            dead;
    vout_control_t  control;

    struct {
        vlc_tick_t  date;
        vlc_tick_t  timestamp;
        bool        is_interlaced;
        picture_t   *decoded;
        picture_t   *current;
        picture_t   *next;
    } displayed;

    struct {
        vlc_tick_t  last;
        vlc_tick_t  timestamp;
    } step;

    struct {
        bool        is_on;
        vlc_tick_t  date;
    } pause;

    /* OSD title configuration */
    struct {
        bool        show;
        int         timeout;
        int         position;
    } title;

    struct {
        bool        is_interlaced;
        vlc_tick_t  date;
    } interlacing;

    /* */
    bool            is_late_dropped;

    /* Video filter2 chain */
    struct {
        vlc_mutex_t     lock;
        char            *configuration;
        video_format_t  format;
        struct filter_chain_t *chain_static;
        struct filter_chain_t *chain_interactive;
        bool            has_deint;
    } filter;

    /* */
    vlc_mouse_t     mouse;
    vlc_mouse_event mouse_event;
    void            *opaque;

    /* Video output window */
    vout_window_t   *window;
    vlc_mutex_t     window_lock;

    /* Video output display */
    vout_display_cfg_t display_cfg;
    vout_display_t *display;

    picture_pool_t  *private_pool;
    picture_pool_t  *display_pool;
    picture_pool_t  *decoder_pool;
    picture_fifo_t  *decoder_fifo;
    vout_chrono_t   render;           /**< picture render time estimator */
};

/**
 * Returns a suitable vout or release the given one.
 *
 * If cfg->fmt is non NULL and valid, a vout will be returned, reusing cfg->vout
 * is possible, otherwise it returns NULL.
 * If cfg->vout is not used, it will be closed and released.
 *
 * You can release the returned value either by vout_Request() or vout_Close().
 *
 * \param object a vlc object
 * \param cfg the video configuration requested.
 * \param input used to get attachments for spu filters
 * \return a vout
 */
vout_thread_t * vout_Request( vlc_object_t *object, const vout_configuration_t *cfg,
                              input_thread_t *input );
#define vout_Request(a,b,c) vout_Request(VLC_OBJECT(a),b,c)

/**
 * Disables a vout.
 *
 * This disables a vout, but keeps it for later reuse.
 */
void vout_Stop(vout_thread_t *);

/**
 * Destroys a vout.
 *
 * This function closes and releases a vout created by vout_Request().
 *
 * \param p_vout the vout to close
 */
void vout_Close( vout_thread_t *p_vout );

/* TODO to move them to vlc_vout.h */
void vout_ControlChangeFullscreen(vout_thread_t *, const char *id);
void vout_ControlChangeWindowed(vout_thread_t *);
void vout_ControlChangeWindowState(vout_thread_t *, unsigned state);
void vout_ControlChangeDisplaySize(vout_thread_t *,
                                   unsigned width, unsigned height);
void vout_ControlChangeDisplayFilled(vout_thread_t *, bool is_filled);
void vout_ControlChangeZoom(vout_thread_t *, unsigned num, unsigned den);
void vout_ControlChangeSampleAspectRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ControlChangeCropRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ControlChangeCropWindow(vout_thread_t *, int x, int y, int width, int height);
void vout_ControlChangeCropBorder(vout_thread_t *, int left, int top, int right, int bottom);
void vout_ControlChangeFilters(vout_thread_t *, const char *);
void vout_ControlChangeSubSources(vout_thread_t *, const char *);
void vout_ControlChangeSubFilters(vout_thread_t *, const char *);
void vout_ControlChangeSubMargin(vout_thread_t *, int);
void vout_ControlChangeViewpoint( vout_thread_t *, const vlc_viewpoint_t *);

/* */
void vout_IntfInit( vout_thread_t * );
void vout_IntfReinit( vout_thread_t * );

/* */
int  vout_OpenWrapper(vout_thread_t *, const char *, vout_display_cfg_t *);
void vout_CloseWrapper(vout_thread_t *, vout_display_cfg_t *);

/* */
int spu_ProcessMouse(spu_t *, const vlc_mouse_t *, const video_format_t *);
void spu_Attach( spu_t *, input_thread_t *input );
void spu_Detach( spu_t * );
void spu_ChangeMargin(spu_t *, int);
void spu_SetHighlight(spu_t *, const vlc_spu_highlight_t*);

/**
 * This function will (un)pause the display of pictures.
 * It is thread safe
 */
void vout_ChangePause( vout_thread_t *, bool b_paused, vlc_tick_t i_date );

/**
 * Updates the pointing device state.
 */
void vout_MouseState(vout_thread_t *, const vlc_mouse_t *);

/**
 * This function will apply an offset on subtitle subpicture.
 */
void spu_OffsetSubtitleDate( spu_t *p_spu, vlc_tick_t i_duration );

/**
 * This function will return and reset internal statistics.
 */
void vout_GetResetStatistic( vout_thread_t *p_vout, unsigned *pi_displayed,
                             unsigned *pi_lost );

/**
 * This function will ensure that all ready/displayed pictures have at most
 * the provided date.
 */
void vout_Flush( vout_thread_t *p_vout, vlc_tick_t i_date );

/**
 * Empty all the pending pictures in the vout
 */
#define vout_FlushAll( vout )  vout_Flush( vout, VLC_TICK_INVALID )

/*
 * Cancel the vout, if cancel is true, it won't return any pictures after this
 * call.
 */
void vout_Cancel( vout_thread_t *p_vout, bool b_canceled );

/**
 * This function will force to display the next picture while paused
 */
void vout_NextPicture( vout_thread_t *p_vout, vlc_tick_t *pi_duration );

/**
 * This function will ask the display of the input title
 */
void vout_DisplayTitle( vout_thread_t *p_vout, const char *psz_title );

/**
 * This function will return true if no more pictures are to be displayed.
 */
bool vout_IsEmpty( vout_thread_t *p_vout );

void vout_SetSpuHighlight( vout_thread_t *p_vout, const vlc_spu_highlight_t * );

#endif
