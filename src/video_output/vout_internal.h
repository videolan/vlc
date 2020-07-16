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

#include <vlc_atomic.h>
#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>
#include <vlc_vout_display.h>
#include "vout_wrapper.h"
#include "statistic.h"
#include "chrono.h"
#include "../clock/clock.h"
#include "../input/input_internal.h"

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
    vlc_clock_t          *clock;
    const video_format_t *fmt;
    vlc_mouse_event      mouse_event;
    void                 *mouse_opaque;
} vout_configuration_t;
#include "control.h"

struct vout_snapshot;

enum vout_crop_mode {
    VOUT_CROP_NONE, VOUT_CROP_RATIO, VOUT_CROP_WINDOW, VOUT_CROP_BORDER,
};

/* */
struct vout_thread_sys_t
{
    bool dummy;

    /* Splitter module if used */
    char            *splitter_name;

    vlc_clock_t     *clock;
    float           rate;
    vlc_tick_t      delay;

    /* */
    video_format_t  original;   /* Original format ie coming from the decoder */
    struct {
        struct {
            unsigned num;
            unsigned den;
        } dar;
        struct {
            enum vout_crop_mode mode;
            union {
                struct {
                    unsigned num;
                    unsigned den;
                } ratio;
                struct {
                    unsigned x;
                    unsigned y;
                    unsigned width;
                    unsigned height;
                } window;
                struct {
                    unsigned left;
                    unsigned right;
                    unsigned top;
                    unsigned bottom;
                } border;
            };
        } crop;
    } source;

    /* Snapshot interface */
    struct vout_snapshot *snapshot;

    /* Statistics */
    vout_statistic_t statistic;

    /* Subpicture unit */
    spu_t           *spu;
    vlc_fourcc_t    spu_blend_chroma;
    vlc_blender_t   *spu_blend;

    /* Thread & synchronization */
    vlc_thread_t    thread;
    vout_control_t  control;

    struct {
        vlc_tick_t  date;
        vlc_tick_t  timestamp;
        bool        is_interlaced;
        picture_t   *decoded; // decoded picture before passed through chain_static
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
        video_format_t    src_fmt;
        vlc_video_context *src_vctx;
        struct filter_chain_t *chain_static;
        struct filter_chain_t *chain_interactive;
        bool            has_deint;
    } filter;

    /* */
    vlc_mouse_t     mouse;
    vlc_mouse_event mouse_event;
    void            *mouse_opaque;

    /* Video output window */
    bool            window_enabled;
    unsigned        window_width; /* protected by display_lock */
    unsigned        window_height; /* protected by display_lock */
    vlc_mutex_t     window_lock;
    vlc_decoder_device *dec_device;

    /* Video output display */
    vout_display_cfg_t display_cfg;
    vout_display_t *display;
    vlc_mutex_t     display_lock;

    picture_pool_t  *private_pool;
    picture_pool_t  *display_pool;
    picture_fifo_t  *decoder_fifo;
    vout_chrono_t   render;           /**< picture render time estimator */

    vlc_atomic_rc_t rc;
};

/**
 * Creates a video output.
 */
vout_thread_t *vout_Create(vlc_object_t *obj) VLC_USED;

vout_thread_t *vout_CreateDummy(vlc_object_t *obj) VLC_USED;

/**
 * Setup the vout for the given configuration and get an associated decoder device.
 *
 * \param vout the video configuration requested.
 * \return pointer to a decoder device reference to use with the vout or NULL
 */
vlc_decoder_device *vout_GetDevice(vout_thread_t *vout);

/**
 * Returns a suitable vout or release the given one.
 *
 * If cfg->fmt is non NULL and valid, a vout will be returned, reusing cfg->vout
 * is possible, otherwise it returns NULL.
 * If cfg->vout is not used, it will be closed and released.
 *
 * You can release the returned value either by vout_Request() or vout_Close().
 *
 * \param cfg the video configuration requested.
 * \param input used to get attachments for spu filters
 * \param vctx pointer to the video context to use with the vout or NULL
 * \retval 0 on success
 * \retval -1 on error
 */
int vout_Request(const vout_configuration_t *cfg, vlc_video_context *vctx, input_thread_t *input);

/**
 * Disables a vout.
 *
 * This disables a vout, but keeps it for later reuse.
 */
void vout_Stop(vout_thread_t *);

/**
 * Stop the display plugin, but keep its window plugin for later reuse.
 */
void vout_StopDisplay(vout_thread_t *);

/**
 * Set the new source format for a started vout
 *
 * \retval 0 on success
 * \retval -1 on error, the vout needs to be restarted to handle the format
 */
int vout_ChangeSource( vout_thread_t *p_vout, const video_format_t *fmt );

/* TODO to move them to vlc_vout.h */
void vout_ChangeFullscreen(vout_thread_t *, const char *id);
void vout_ChangeWindowed(vout_thread_t *);
void vout_ChangeWindowState(vout_thread_t *, unsigned state);
void vout_ChangeDisplaySize(vout_thread_t *, unsigned width, unsigned height);
void vout_ChangeDisplayFilled(vout_thread_t *, bool is_filled);
void vout_ChangeZoom(vout_thread_t *, unsigned num, unsigned den);
void vout_ChangeDisplayAspectRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ChangeCropRatio(vout_thread_t *, unsigned num, unsigned den);
void vout_ChangeCropWindow(vout_thread_t *, int x, int y, int width, int height);
void vout_ChangeCropBorder(vout_thread_t *, int left, int top, int right, int bottom);
void vout_ControlChangeFilters(vout_thread_t *, const char *);
void vout_ControlChangeInterlacing(vout_thread_t *, bool);
void vout_ControlChangeSubSources(vout_thread_t *, const char *);
void vout_ControlChangeSubFilters(vout_thread_t *, const char *);
void vout_ChangeSpuChannelMargin(vout_thread_t *, enum vlc_vout_order order, int);
void vout_ChangeViewpoint( vout_thread_t *, const vlc_viewpoint_t *);

/* */
void vout_CreateVars( vout_thread_t * );
void vout_IntfInit( vout_thread_t * );
void vout_IntfReinit( vout_thread_t * );
void vout_IntfDeinit(vlc_object_t *);

/* */
vout_display_t *vout_OpenWrapper(vout_thread_t *, const char *,
                    const vout_display_cfg_t *, video_format_t *, vlc_video_context *);
void vout_CloseWrapper(vout_thread_t *, vout_display_t *vd);

/* */
ssize_t vout_RegisterSubpictureChannelInternal( vout_thread_t *,
                                                vlc_clock_t *clock,
                                                enum vlc_vout_order *out_order );
ssize_t spu_RegisterChannelInternal( spu_t *, vlc_clock_t *, enum vlc_vout_order * );
void spu_Attach( spu_t *, input_thread_t *input );
void spu_Detach( spu_t * );
void spu_SetClockDelay(spu_t *spu, size_t channel_id, vlc_tick_t delay);
void spu_SetClockRate(spu_t *spu, size_t channel_id, float rate);
void spu_ChangeChannelOrderMargin(spu_t *, enum vlc_vout_order, int);
void spu_SetHighlight(spu_t *, const vlc_spu_highlight_t*);

/**
 * This function will (un)pause the display of pictures.
 * It is thread safe
 */
void vout_ChangePause( vout_thread_t *, bool b_paused, vlc_tick_t i_date );

/**
 * This function will change the rate of the vout
 * It is thread safe
 */
void vout_ChangeRate( vout_thread_t *, float rate );

/**
 * This function will change the delay of the vout
 * It is thread safe
 */
void vout_ChangeDelay( vout_thread_t *, vlc_tick_t delay );

/**
 * This function will change the rate of the spu channel
 * It is thread safe
 */
void vout_ChangeSpuRate( vout_thread_t *, size_t channel_id, float rate );
/**
 * This function will change the delay of the spu channel
 * It is thread safe
 */
void vout_ChangeSpuDelay( vout_thread_t *, size_t channel_id, vlc_tick_t delay );


/**
 * Updates the pointing device state.
 */
void vout_MouseState(vout_thread_t *, const vlc_mouse_t *);

/**
 * This function will return and reset internal statistics.
 */
void vout_GetResetStatistic( vout_thread_t *p_vout, unsigned *pi_displayed,
                             unsigned *pi_lost );

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
