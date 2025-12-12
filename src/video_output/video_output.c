/*****************************************************************************
 * video_output.c : video output thread
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2019 VLC authors, VideoLAN and Videolabs SAS
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_configuration.h>

#include <math.h>
#include <stdlib.h>                                                /* free() */
#include <string.h>
#include <assert.h>

#include <vlc_vout.h>

#include <vlc_filter.h>
#include <vlc_spu.h>
#include <vlc_vout_osd.h>
#include <vlc_image.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_tracer.h>
#include <vlc_atomic.h>

#include "../libvlc.h"
#include "vout_private.h"
#include "vout_internal.h"
#include "display.h"
#include "snapshot.h"
#include "video_window.h"
#include "../misc/variables.h"
#include "../misc/threads.h"
#include "../clock/clock.h"
#include "statistic.h"
#include "chrono.h"
#include "control.h"

typedef struct vout_thread_sys_t
{
    struct vout_thread_t obj;

    vout_interlacing_state_t interlacing;

    bool dummy;

    /* Splitter module if used */
    char            *splitter_name;

    const char      *str_id;

    vlc_mutex_t clock_lock;
    bool clock_nowait; /* protected by vlc_clock_Lock()/vlc_clock_Unlock() */
    bool wait_interrupted;
    bool first_picture;

    vlc_clock_t     *clock;
    vlc_clock_listener_id *clock_listener_id;
    uint32_t clock_id;
    float           rate;
    vlc_tick_t      delay;

    video_format_t  original;   /* Original format ie coming from the decoder */

    /* */
    struct {
        vlc_rational_t dar;
        struct vout_crop crop;
    } source;

    /* Snapshot interface */
    struct vout_snapshot *snapshot;

    /* Statistics */
    vout_statistic_t statistic;

    /* Subpicture unit */
    spu_t           *spu;
    vlc_blender_t   *spu_blend;

    /* Thread & synchronization */
    vout_control_t  control;
    atomic_bool     control_is_terminated; // shutdown the vout thread
    vlc_thread_t    thread;

    struct {
        vlc_tick_t  date;
        vlc_tick_t  timestamp;
        bool        is_interlaced;
        picture_t   *decoded; // decoded picture before passed through chain_static
        picture_t   *current;
        video_projection_mode_t projection;
    } displayed;

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

    /* */
    bool            is_late_dropped;

    /* */
    vlc_mouse_t     mouse;

    /* Video output window */
    bool            window_enabled;
    unsigned        window_width; /* protected by display_lock */
    unsigned        window_height; /* protected by display_lock */
    vlc_mutex_t     window_lock;
    vlc_decoder_device *dec_device;

    /* Video output display */
    vout_display_cfg_t display_cfg;
    vout_display_t *display;
    vlc_queuedmutex_t display_lock;

    /* Video filter2 chain */
    struct {
        vlc_mutex_t     lock;
        bool            changed;
        bool            new_interlaced;
        char            *configuration;
        video_format_t    src_fmt;
        vlc_video_context *src_vctx;
        struct filter_chain_t *chain_static;
        struct filter_chain_t *chain_interactive;
    } filter;

    picture_fifo_t  *decoder_fifo;
    struct {
        vout_chrono_t static_filter;
        vout_chrono_t render;         /**< picture render time estimator */
    } chrono;

    unsigned frame_next_count;

    vlc_atomic_rc_t rc;

    picture_pool_t  *private_pool; // interactive + static filters & blending
} vout_thread_sys_t;

#define VOUT_THREAD_TO_SYS(vout) \
    container_of(vout, vout_thread_sys_t, obj.obj)


/* Amount of pictures in the private pool:
 * 3 for interactive+static filters, 1 for SPU blending, 1 for currently displayed */
#define FILTER_POOL_SIZE  (3+1+1)

/* Maximum delay between 2 displayed pictures.
 * XXX it is needed for now but should be removed in the long term.
 */
#define VOUT_REDISPLAY_DELAY VLC_TICK_FROM_MS(80)

/**
 * Late pictures having a delay higher than this value are thrashed.
 */
#define VOUT_DISPLAY_LATE_THRESHOLD VLC_TICK_FROM_MS(20)

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE VLC_TICK_FROM_MS(4)

/* */
static inline struct vlc_tracer *GetTracer(vout_thread_sys_t *sys)
{
    return sys->str_id == NULL ? NULL :
        vlc_object_get_tracer(VLC_OBJECT(&sys->obj));
}

static inline void VoutResetChronoLocked(vout_thread_sys_t *sys)
{
    vlc_queuedmutex_assert(&sys->display_lock);

    /* Arbitrary initial time */
    vout_chrono_Init(&sys->chrono.render, 5, VLC_TICK_FROM_MS(10));
    vout_chrono_Init(&sys->chrono.static_filter, 4, VLC_TICK_FROM_MS(0));
}

static bool VoutCheckFormat(const video_format_t *src)
{
    if (src->i_width == 0  || src->i_width  > 8192 ||
        src->i_height == 0 || src->i_height > 8192)
        return false;
    if (src->i_sar_num <= 0 || src->i_sar_den <= 0)
        return false;
    return true;
}

static void VoutFixFormat(video_format_t *dst, const video_format_t *src)
{
    video_format_Copy(dst, src);
    dst->i_chroma = vlc_fourcc_GetCodec(VIDEO_ES, src->i_chroma);
    VoutFixFormatAR( dst );
    vlc_viewpoint_clip( &dst->pose );
}

static void VoutRenderWakeUpUrgent(vout_thread_sys_t *sys)
{
    /* The assignment to sys->clock is protected by sys->lock */
    vlc_mutex_lock(&sys->clock_lock);
    if (sys->clock)
    {
        /* Wake up the clock-wait between prepare() and display() */
        vlc_clock_Lock(sys->clock);
        sys->clock_nowait = true;
        vlc_clock_Wake(sys->clock);
        vlc_clock_Unlock(sys->clock);
    }
    vlc_mutex_unlock(&sys->clock_lock);
}

static bool VideoFormatIsCropArEqual(video_format_t *dst,
                                     const video_format_t *src)
{
    return dst->i_sar_num * src->i_sar_den == dst->i_sar_den * src->i_sar_num &&
           dst->i_x_offset       == src->i_x_offset &&
           dst->i_y_offset       == src->i_y_offset &&
           dst->i_visible_width  == src->i_visible_width &&
           dst->i_visible_height == src->i_visible_height;
}

static void vout_UpdateWindowSizeLocked(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;

    if (unlikely(sys->original.i_chroma == 0))
        return; /* not started yet, postpone size computaton */

    vlc_mutex_assert(&sys->window_lock);
    vout_display_ResizeWindow(sys->display_cfg.window, &sys->original,
                              &sys->source.dar, &sys->source.crop,
                              &sys->display_cfg.display);
}

/* */
void vout_GetResetStatistic(vout_thread_t *vout, unsigned *restrict displayed,
                            unsigned *restrict lost, unsigned *restrict late)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vout_statistic_GetReset( &sys->statistic, displayed, lost, late );
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->decoder_fifo);

    picture_fifo_Lock(sys->decoder_fifo);
    bool empty = picture_fifo_IsEmpty(sys->decoder_fifo);
    picture_fifo_Unlock(sys->decoder_fifo);
    return empty;
}

void vout_DisplayTitle(vout_thread_t *vout, const char *title)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(title);

    if (!sys->title.show)
        return;

    vout_OSDText(vout, VOUT_SPU_CHANNEL_OSD, sys->title.position,
                 VLC_TICK_FROM_MS(sys->title.timeout), title);
}

bool vout_FilterMouse(vout_thread_t *vout, vlc_mouse_t *mouse)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    vlc_mouse_t tmp[2], *m = mouse;
    bool event_consumed = false;

    /* Pass mouse events through the filter chains. */
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.chain_static != NULL
     && sys->filter.chain_interactive != NULL) {
        if (!filter_chain_MouseFilter(sys->filter.chain_interactive,
                                      &tmp[0], m))
            m = &tmp[0];
        else
            event_consumed = true;
        if (!filter_chain_MouseFilter(sys->filter.chain_static,
                                      &tmp[1], m))
            m = &tmp[1];
        else
            event_consumed = true;
    }
    vlc_mutex_unlock(&sys->filter.lock);

    if (mouse != m)
        *mouse = *m;

    return event_consumed;
}

void vout_PutSubpicture( vout_thread_t *vout, subpicture_t *subpic )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (sys->spu != NULL)
        spu_PutSubpicture(sys->spu, subpic);
    else
        subpicture_Delete(subpic);
}

ssize_t vout_RegisterSubpictureChannel( vout_thread_t *vout )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    ssize_t channel = VOUT_SPU_CHANNEL_INVALID;

    if (sys->spu)
        channel = spu_RegisterChannel(sys->spu);

    return channel;
}

ssize_t vout_RegisterSubpictureChannelInternal(vout_thread_t *vout,
                                               vlc_clock_t *clock,
                                               enum vlc_vout_order *out_order)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    ssize_t channel = VOUT_SPU_CHANNEL_INVALID;

    if (sys->spu)
        channel = spu_RegisterChannelInternal(sys->spu, clock, out_order);

    return channel;
}

void vout_UnregisterSubpictureChannel( vout_thread_t *vout, size_t channel )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->spu);
    spu_UnregisterChannel(sys->spu, channel);
}

void vout_FlushSubpictureChannel( vout_thread_t *vout, size_t channel )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (sys->spu)
        spu_ClearChannel(sys->spu, channel);
}

void vout_SetSpuHighlight( vout_thread_t *vout,
                        const vlc_spu_highlight_t *spu_hl )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (sys->spu)
        spu_SetHighlight(sys->spu, spu_hl);
}

/**
 * It gives to the vout a picture to be displayed.
 *
 * Becareful, after vout_PutPicture is called, picture_t::p_next cannot be
 * read/used.
 */
void vout_PutPicture(vout_thread_t *vout, picture_t *picture)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert( !picture_HasChainedPics( picture ) );
    picture_fifo_Lock(sys->decoder_fifo);
    picture_fifo_Push(sys->decoder_fifo, picture);
    picture_fifo_Unlock(sys->decoder_fifo);
    vout_control_Wake(&sys->control);
}

/* */
int vout_GetSnapshot(vout_thread_t *vout,
                     block_t **image_dst, picture_t **picture_dst,
                     video_format_t *fmt,
                     const char *type, vlc_tick_t timeout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    picture_t *picture = vout_snapshot_Get(sys->snapshot, timeout);
    if (!picture) {
        msg_Err(vout, "Failed to grab a snapshot");
        return VLC_EGENERIC;
    }

    if (image_dst) {
        vlc_fourcc_t codec = VLC_CODEC_PNG;
        if (type && image_Type2Fourcc(type))
            codec = image_Type2Fourcc(type);

        const int override_width  = var_InheritInteger(vout, "snapshot-width");
        const int override_height = var_InheritInteger(vout, "snapshot-height");

        if (picture_Export(VLC_OBJECT(vout), image_dst, fmt,
                           picture, codec, override_width, override_height, false)) {
            msg_Err(vout, "Failed to convert image for snapshot");
            picture_Release(picture);
            return VLC_EGENERIC;
        }
    }
    if (picture_dst)
        *picture_dst = picture;
    else
        picture_Release(picture);
    return VLC_SUCCESS;
}

/* vout_Control* are usable by anyone at anytime */
void vout_ChangeFullscreen(vout_thread_t *vout, const char *id)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->window_lock);
    vlc_window_SetFullScreen(sys->display_cfg.window, id);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeWindowed(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->window_lock);
    vlc_window_UnsetFullScreen(sys->display_cfg.window);
    /* Attempt to reset the intended window size */
    vout_UpdateWindowSizeLocked(sys);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeWindowState(vout_thread_t *vout, unsigned st)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->window_lock);
    vlc_window_SetState(sys->display_cfg.window, st);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeDisplaySize(vout_thread_t *vout,
                            unsigned width, unsigned height,
                            void (*cb)(void *), void *opaque)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    assert(!sys->dummy);

    VoutRenderWakeUpUrgent(sys);

    /* DO NOT call this outside the vout window callbacks */
    vlc_queuedmutex_lock(&sys->display_lock);

    sys->window_width = width;
    sys->window_height = height;

    if (sys->display != NULL)
        vout_display_SetSize(sys->display, width, height);

    if (cb != NULL)
        cb(opaque);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ChangeDisplayFitting(vout_thread_t *vout, enum vlc_video_fitting fit)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.display.fitting = fit;
    /* no window size update here */

    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayFitting(sys->display, fit);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ChangeZoom(vout_thread_t *vout, unsigned num, unsigned den)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (num != 0 && den != 0) {
        vlc_ureduce(&num, &den, num, den, 0);
    } else {
        num = 1;
        den = 1;
    }

    if (num * 10 < den) {
        num = 1;
        den = 10;
    } else if (num > den * 10) {
        num = 10;
        den = 1;
    }

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.display.zoom.num = num;
    sys->display_cfg.display.zoom.den = den;

    vout_UpdateWindowSizeLocked(sys);

    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayZoom(sys->display, num, den);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ChangeDisplayAspectRatio(vout_thread_t *vout, vlc_rational_t dar)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->source.dar = dar;

    vout_UpdateWindowSizeLocked(sys);

    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayAspect(sys->display, dar);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ChangeCrop(vout_thread_t *vout,
                     const struct vout_crop *restrict crop)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->source.crop = *crop;
    vout_UpdateWindowSizeLocked(sys);

    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayCrop(sys->display, crop);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ControlChangeFilters(vout_thread_t *vout, const char *filters)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.configuration)
    {
        if (filters == NULL || strcmp(sys->filter.configuration, filters))
        {
            free(sys->filter.configuration);
            sys->filter.configuration = filters ? strdup(filters) : NULL;
            sys->filter.changed = true;
        }
    }
    else if (filters != NULL)
    {
        sys->filter.configuration = strdup(filters);
        sys->filter.changed = true;
    }
    vlc_mutex_unlock(&sys->filter.lock);
}

void vout_ControlChangeInterlacing(vout_thread_t *vout, bool set)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->filter.lock);
    sys->filter.new_interlaced = set;
    vlc_mutex_unlock(&sys->filter.lock);
}

void vout_ControlChangeSubSources(vout_thread_t *vout, const char *filters)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (likely(sys->spu != NULL))
        spu_ChangeSources(sys->spu, filters);
}

void vout_ControlChangeSubFilters(vout_thread_t *vout, const char *filters)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (likely(sys->spu != NULL))
        spu_ChangeFilters(sys->spu, filters);
}

void vout_ChangeSpuChannelMargin(vout_thread_t *vout,
                                 enum vlc_vout_order order, int margin)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (likely(sys->spu != NULL))
        spu_ChangeChannelOrderMargin(sys->spu, order, margin);
}

void vout_ChangeViewpoint(vout_thread_t *vout,
                          const vlc_viewpoint_t *p_viewpoint)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.viewpoint = *p_viewpoint;
    /* no window size update here */
    vlc_mutex_unlock(&sys->window_lock);

    VoutRenderWakeUpUrgent(sys);
    vlc_queuedmutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_SetDisplayViewpoint(sys->display, p_viewpoint);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ChangeIccProfile(vout_thread_t *vout,
                           vlc_icc_profile_t *profile)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_queuedmutex_lock(&sys->display_lock);
    free(sys->display_cfg.icc_profile);
    sys->display_cfg.icc_profile = profile;
    if (sys->display != NULL)
        vout_SetDisplayIccProfile(sys->display, profile);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ToggleProjection(vout_thread_t *vout, bool enabled)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    video_projection_mode_t projection;
    if (sys->displayed.projection != PROJECTION_MODE_RECTANGULAR && !enabled)
        projection = PROJECTION_MODE_RECTANGULAR;
    else if (sys->original.projection_mode != PROJECTION_MODE_RECTANGULAR && enabled)
        projection = sys->original.projection_mode;
    else return;

    vlc_queuedmutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_SetDisplayProjection(sys->display, projection);
    vlc_queuedmutex_unlock(&sys->display_lock);

}

void vout_ResetProjection(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    msg_Dbg(vout, "resetting projection_mode to %d", sys->original.projection_mode);
    vout_ChangeProjection(vout, sys->original.projection_mode);
}

void vout_ChangeProjection(vout_thread_t *vout,
                           video_projection_mode_t projection)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    /* Use vout_ResetProjection instead. */
    assert((int)projection != -1);
    msg_Dbg(vout, "setting projection_mode to %d", projection);

    vlc_queuedmutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_SetDisplayProjection(sys->display, projection);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

void vout_ControlChangeStereo(vout_thread_t *vout, vlc_stereoscopic_mode_t mode)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_queuedmutex_lock(&sys->display_lock);
    sys->display_cfg.stereo_mode = mode;
    if (sys->display != NULL)
        vout_SetDisplayStereo(sys->display, mode);
    vlc_queuedmutex_unlock(&sys->display_lock);
}

/* */
static void VoutGetDisplayCfg(vout_thread_sys_t *p_vout, const video_format_t *fmt, vout_display_cfg_t *cfg)
{
    vout_thread_t *vout = &p_vout->obj;
    /* Load configuration */
    cfg->viewpoint = fmt->pose;

    const int display_width = var_GetInteger(vout, "width");
    const int display_height = var_GetInteger(vout, "height");
    cfg->display.width   = display_width > 0  ? display_width  : 0;
    cfg->display.height  = display_height > 0 ? display_height : 0;
    cfg->display.full_fill = var_GetBool(vout, "spu-fill");
    cfg->display.fitting = var_GetBool(vout, "autoscale")
        ? var_InheritFit(VLC_OBJECT(vout)) : VLC_VIDEO_FIT_NONE;
    unsigned msar_num, msar_den;
    if (var_InheritURational(vout, &msar_num, &msar_den, "monitor-par") ||
        msar_num <= 0 || msar_den <= 0) {
        msar_num = 1;
        msar_den = 1;
    }
    cfg->display.sar.num = msar_num;
    cfg->display.sar.den = msar_den;
    unsigned zoom_den = 1000;
    unsigned zoom_num = zoom_den * var_GetFloat(vout, "zoom");
    vlc_ureduce(&zoom_num, &zoom_den, zoom_num, zoom_den, 0);
    cfg->display.zoom.num = zoom_num;
    cfg->display.zoom.den = zoom_den;
    cfg->display.align.vertical = VLC_VIDEO_ALIGN_CENTER;
    cfg->display.align.horizontal = VLC_VIDEO_ALIGN_CENTER;
    const int align_mask = var_GetInteger(vout, "align");
    if (align_mask & VOUT_ALIGN_LEFT)
        cfg->display.align.horizontal = VLC_VIDEO_ALIGN_LEFT;
    else if (align_mask & VOUT_ALIGN_RIGHT)
        cfg->display.align.horizontal = VLC_VIDEO_ALIGN_RIGHT;
    if (align_mask & VOUT_ALIGN_TOP)
        cfg->display.align.vertical = VLC_VIDEO_ALIGN_TOP;
    else if (align_mask & VOUT_ALIGN_BOTTOM)
        cfg->display.align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
    cfg->stereo_mode = var_GetInteger(vout, "video-stereo-mode");
}

/* */
static int FilterRestartCallback(vlc_object_t *p_this, char const *psz_var,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *p_data)
{
    (void) p_this; (void) psz_var; (void) oldval; (void) newval;
    vout_ControlChangeFilters((vout_thread_t *)p_data, NULL);
    return 0;
}

static int DelFilterCallbacks(filter_t *filter, void *opaque)
{
    vout_thread_sys_t *sys = opaque;
    filter_DelProxyCallbacks(VLC_OBJECT(&sys->obj), filter,
                             FilterRestartCallback);
    return VLC_SUCCESS;
}

static void DelAllFilterCallbacks(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    assert(sys->filter.chain_interactive != NULL);
    filter_chain_ForEach(sys->filter.chain_interactive,
                         DelFilterCallbacks, vout);
}

static picture_t *VoutVideoFilterInteractiveNewPicture(filter_t *filter)
{
    vout_thread_sys_t *sys = filter->owner.sys;

    picture_t *picture = picture_pool_Get(sys->private_pool);
    if (picture) {
        picture_Reset(picture);
        video_format_CopyCropAr(&picture->format, &filter->fmt_out.video);
    }
    return picture;
}

static picture_t *VoutVideoFilterStaticNewPicture(filter_t *filter)
{
    vout_thread_sys_t *sys = filter->owner.sys;

    vlc_mutex_assert(&sys->filter.lock);
    if (filter_chain_IsEmpty(sys->filter.chain_interactive))
        // we may be using the last filter of both chains, so we get the picture
        // from the display module pool, just like for the last interactive filter.
        return VoutVideoFilterInteractiveNewPicture(filter);

    return picture_NewFromFormat(&filter->fmt_out.video);
}

static void FilterFlush(vout_thread_sys_t *sys, bool is_locked)
{
    if (sys->displayed.current)
    {
        picture_Release( sys->displayed.current );
        sys->displayed.current = NULL;
        sys->displayed.date = VLC_TICK_INVALID;
    }

    if (!is_locked)
        vlc_mutex_lock(&sys->filter.lock);
    filter_chain_VideoFlush(sys->filter.chain_static);
    filter_chain_VideoFlush(sys->filter.chain_interactive);
    if (!is_locked)
        vlc_mutex_unlock(&sys->filter.lock);
}

typedef struct {
    char           *name;
    config_chain_t *cfg;
} vout_filter_t;

static int strcmp_void(const void *a, const void *b)
{
    const char *const *entry = b;
    return strcmp(a, *entry);
}

static void ChangeFilters(vout_thread_sys_t *vout)
{
    /* bsearch: must be sorted alphabetically */
    static const char *const static_filters[] = {
        "amf_frc",
        "fps",
        "postproc",
    };
    vout_thread_sys_t *sys = vout;
    FilterFlush(vout, true);
    DelAllFilterCallbacks(vout);

    vlc_array_t array_static;
    vlc_array_t array_interactive;

    vlc_array_init(&array_static);
    vlc_array_init(&array_interactive);

    if (sys->interlacing.has_deint)
    {
        vout_filter_t *e = malloc(sizeof(*e));

        if (likely(e))
        {
            char *filter = var_InheritString(&vout->obj, "deinterlace-filter");
            free(config_ChainCreate(&e->name, &e->cfg, filter));
            free(filter);
            vlc_array_append_or_abort(&array_static, e);
        }
    }

    char *current = sys->filter.configuration ? strdup(sys->filter.configuration) : NULL;
    while (current) {
        config_chain_t *cfg;
        char *name;
        char *next = config_ChainCreate(&name, &cfg, current);

        if (name && *name) {
            vout_filter_t *e = malloc(sizeof(*e));

            if (likely(e)) {
                e->name = name;
                e->cfg  = cfg;
                bool is_static_filter =
                    bsearch(e->name, static_filters, ARRAY_SIZE(static_filters),
                            sizeof(const char *), strcmp_void) != NULL;
                if (is_static_filter)
                    vlc_array_append_or_abort(&array_static, e);
                else
                    vlc_array_append_or_abort(&array_interactive, e);
            }
            else {
                if (cfg)
                    config_ChainDestroy(cfg);
                free(name);
            }
        } else {
            if (cfg)
                config_ChainDestroy(cfg);
            free(name);
        }
        free(current);
        current = next;
    }

    es_format_t fmt_target;
    es_format_InitFromVideo(&fmt_target, &sys->filter.src_fmt);
    vlc_video_context *vctx_target  = sys->filter.src_vctx;

    const es_format_t *p_fmt_current = &fmt_target;
    vlc_video_context *vctx_current = vctx_target;

    for (int a = 0; a < 2; a++) {
        vlc_array_t    *array = a == 0 ? &array_static :
                                         &array_interactive;
        filter_chain_t *chain = a == 0 ? sys->filter.chain_static :
                                         sys->filter.chain_interactive;

        filter_chain_Reset(chain, p_fmt_current, vctx_current, p_fmt_current);
        for (size_t i = 0; i < vlc_array_count(array); i++) {
            vout_filter_t *e = vlc_array_item_at_index(array, i);
            msg_Dbg(&vout->obj, "Adding '%s' as %s", e->name, a == 0 ? "static" : "interactive");
            filter_t *filter = filter_chain_AppendFilter(chain, e->name, e->cfg,
                               NULL);
            if (!filter)
                msg_Err(&vout->obj, "Failed to add filter '%s'", e->name);
            else if (a == 1) /* Add callbacks for interactive filters */
                filter_AddProxyCallbacks(&vout->obj, filter, FilterRestartCallback);

            config_ChainDestroy(e->cfg);
            free(e->name);
            free(e);
        }
        if (!filter_chain_IsEmpty(chain))
        {
            p_fmt_current = filter_chain_GetFmtOut(chain);
            vctx_current = filter_chain_GetVideoCtxOut(chain);
        }
        vlc_array_clear(array);
    }

    if (!es_format_IsSimilar(p_fmt_current, &fmt_target)) {
        es_format_LogDifferences(vlc_object_logger(&vout->obj),
                "current", p_fmt_current, "new", &fmt_target);

        /* Shallow local copy */
        es_format_t tmp = *p_fmt_current;
        /* Assign the same chroma to compare everything except the chroma */
        tmp.i_codec = fmt_target.i_codec;
        tmp.video.i_chroma = fmt_target.video.i_chroma;

        int ret = VLC_EGENERIC;

        bool only_chroma_changed = es_format_IsSimilar(&tmp, &fmt_target);
        if (only_chroma_changed)
        {
            picture_pool_t *new_private_pool =
                    picture_pool_NewFromFormat(&p_fmt_current->video,
                                               FILTER_POOL_SIZE);
            if (new_private_pool != NULL)
            {
                msg_Dbg(&vout->obj, "Changing vout format to %4.4s",
                                    (const char *) &p_fmt_current->video.i_chroma);
                /* Only the chroma changed, request the vout to update the format */
                ret = vout_SetDisplayFormat(sys->display, &p_fmt_current->video,
                                            vctx_current);
                if (ret != VLC_SUCCESS)
                {
                    picture_pool_Release(new_private_pool);
                    msg_Dbg(&vout->obj, "Changing vout format to %4.4s failed",
                            (const char *) &p_fmt_current->video.i_chroma);
                }
                else
                {
                    // update the pool
                    picture_pool_Release(sys->private_pool);
                    sys->private_pool = new_private_pool;
                }
            }
        }

        if (ret != VLC_SUCCESS)
        {
            msg_Dbg(&vout->obj, "Adding a filter to compensate for format changes in interactive chain (%p)",
                    (void*)sys->filter.chain_interactive);
            if (filter_chain_AppendConverter(sys->filter.chain_interactive,
                                             &fmt_target) != VLC_SUCCESS) {
                msg_Err(&vout->obj, "Failed to compensate for the format changes, removing all filters");
                DelAllFilterCallbacks(vout);
                filter_chain_Reset(sys->filter.chain_static,      &fmt_target, vctx_target, &fmt_target);
                filter_chain_Reset(sys->filter.chain_interactive, &fmt_target, vctx_target, &fmt_target);
            }
        }
    }

    es_format_Clean(&fmt_target);

    sys->filter.changed = false;
}

static bool IsPictureLateToProcess(vout_thread_sys_t *vout, const video_format_t *fmt,
                          vlc_tick_t time_until_display,
                          vlc_tick_t process_duration)
{
    vout_thread_sys_t *sys = vout;

    vlc_tick_t late = process_duration - time_until_display;

    vlc_tick_t late_threshold;
    if (fmt->i_frame_rate && fmt->i_frame_rate_base) {
        late_threshold = vlc_tick_from_samples(fmt->i_frame_rate_base, fmt->i_frame_rate);
    }
    else
        late_threshold = VOUT_DISPLAY_LATE_THRESHOLD;
    if (late > late_threshold) {
        struct vlc_tracer *tracer = GetTracer(vout);
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id, "toolate");

        msg_Warn(&vout->obj, "picture is too late to be displayed (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
        return true;
    }
    return false;
}

static inline vlc_tick_t GetRenderDelay(vout_thread_sys_t *sys)
{
    return vout_chrono_GetHigh(&sys->chrono.render) + VOUT_MWAIT_TOLERANCE;
}

static bool IsPictureLateToStaticFilter(vout_thread_sys_t *vout,
                                        vlc_tick_t time_until_display)
{
    vout_thread_sys_t *sys = vout;
    const es_format_t *static_es = filter_chain_GetFmtOut(sys->filter.chain_static);
    const vlc_tick_t prepare_decoded_duration =
        vout_chrono_GetHigh(&sys->chrono.render) +
        vout_chrono_GetHigh(&sys->chrono.static_filter);
    return IsPictureLateToProcess(vout, &static_es->video, time_until_display, prepare_decoded_duration);
}

/* */
VLC_USED
static picture_t *PreparePicture(vout_thread_sys_t *vout, bool reuse_decoded,
                                 bool frame_by_frame)
{
    vout_thread_sys_t *sys = vout;
    bool is_late_dropped = sys->is_late_dropped && !frame_by_frame;

    vlc_mutex_lock(&sys->filter.lock);

    picture_t *picture = filter_chain_VideoFilter(sys->filter.chain_static, NULL);
    assert(!reuse_decoded || !picture);

    while (!picture) {
        picture_t *decoded;
        if (unlikely(reuse_decoded && sys->displayed.decoded)) {
            decoded = picture_Hold(sys->displayed.decoded);
            if (decoded == NULL)
                break;
        } else {
            picture_fifo_Lock(sys->decoder_fifo);
            decoded = picture_fifo_Pop(sys->decoder_fifo);
            picture_fifo_Unlock(sys->decoder_fifo);
            if (decoded == NULL)
                break;

            if (!decoded->b_force)
            {
                const vlc_tick_t system_now = vlc_tick_now();
                uint32_t clock_id;
                vlc_clock_Lock(sys->clock);
                const vlc_tick_t system_pts =
                    vlc_clock_ConvertToSystem(sys->clock, system_now,
                                              decoded->date, sys->rate, &clock_id);
                vlc_clock_Unlock(sys->clock);
                if (clock_id != sys->clock_id)
                {
                    sys->clock_id = clock_id;
                    msg_Dbg(&vout->obj, "Using a new clock context (%u), "
                            "flusing static filters", clock_id);

                    /* Most deinterlace modules can't handle a PTS
                     * discontinuity, so flush them.
                     *
                     * FIXME: Pass a discontinuity flag and handle it in
                     * deinterlace modules. */
                    filter_chain_VideoFlush(sys->filter.chain_static);
                }

                if (is_late_dropped
                 && IsPictureLateToStaticFilter(vout, system_pts - system_now))
                {
                    picture_Release(decoded);
                    vout_statistic_AddLost(&sys->statistic, 1);

                    /* A picture dropped means discontinuity for the
                     * filters and we need to notify eg. deinterlacer. */
                    filter_chain_VideoFlush(sys->filter.chain_static);
                    continue;
                }
            }

            if (!VideoFormatIsCropArEqual(&decoded->format, &sys->filter.src_fmt))
            {
                // we received an aspect ratio change
                // Update the filters with the filter source format with the new aspect ratio
                video_format_Clean(&sys->filter.src_fmt);
                video_format_Copy(&sys->filter.src_fmt, &decoded->format);
                if (sys->filter.src_vctx)
                    vlc_video_context_Release(sys->filter.src_vctx);
                vlc_video_context *pic_vctx = picture_GetVideoContext(decoded);
                sys->filter.src_vctx = pic_vctx ? vlc_video_context_Hold(pic_vctx) : NULL;

                ChangeFilters(vout);
            }
        }

        reuse_decoded = false;

        if (sys->displayed.decoded)
            picture_Release(sys->displayed.decoded);

        sys->displayed.decoded       = picture_Hold(decoded);
        sys->displayed.timestamp     = decoded->date;
        sys->displayed.is_interlaced = !decoded->b_progressive;

        vout_chrono_Start(&sys->chrono.static_filter);
        picture = filter_chain_VideoFilter(sys->filter.chain_static, sys->displayed.decoded);
        vout_chrono_Stop(&sys->chrono.static_filter);
    }

    vlc_mutex_unlock(&sys->filter.lock);

    return picture;
}

static vlc_decoder_device * VoutHoldDecoderDevice(vlc_object_t *o, void *opaque)
{
    VLC_UNUSED(o);
    vout_thread_sys_t *sys = opaque;
    return sys->dec_device ? vlc_decoder_device_Hold( sys->dec_device ) : NULL;
}

static const struct filter_video_callbacks vout_video_cbs = {
    NULL, VoutHoldDecoderDevice,
};

static picture_t *ConvertRGBAAndBlend(vout_thread_sys_t *vout, picture_t *pic,
                                      vlc_render_subpicture *subpic)
{
    vout_thread_sys_t *sys = vout;
    /* This function will convert the pic to RGBA and blend the subpic to it.
     * The returned pic can't be used to display since the chroma will be
     * different than the "vout display" one, but it can be used for snapshots.
     * */

    assert(sys->spu_blend);

    filter_owner_t owner = {
        .video = &vout_video_cbs,
        .sys = vout,
    };
    filter_chain_t *filterc = filter_chain_NewVideo(&vout->obj, false, &owner);
    if (!filterc)
        return NULL;

    es_format_t src = sys->spu_blend->fmt_out;
    es_format_t dst = src;
    dst.video.i_chroma = VLC_CODEC_RGBA;

    filter_chain_Reset(filterc, &src,
                       NULL /* TODO output video context of blender */,
                       &dst);

    if (filter_chain_AppendConverter(filterc, &dst) != VLC_SUCCESS)
    {
        filter_chain_Delete(filterc);
        return NULL;
    }

    picture_Hold(pic);
    pic = filter_chain_VideoFilter(filterc, pic);
    filter_chain_Delete(filterc);

    if (pic)
    {
        vlc_blender_t *swblend = filter_NewBlend(VLC_OBJECT(&vout->obj), &dst.video);
        if (swblend)
        {
            bool success = picture_BlendSubpicture(pic, swblend, subpic) > 0;
            filter_DeleteBlend(swblend);
            if (success)
                return pic;
        }
        picture_Release(pic);
    }
    return NULL;
}

static picture_t *FilterPictureInteractive(vout_thread_sys_t *sys)
{
    // hold it as the filter chain will release it or return it and we release it
    picture_Hold(sys->displayed.current);

    vlc_mutex_lock(&sys->filter.lock);
    picture_t *filtered = filter_chain_VideoFilter(sys->filter.chain_interactive, sys->displayed.current);
    vlc_mutex_unlock(&sys->filter.lock);

    if (filtered && filtered->date != sys->displayed.current->date)
        msg_Warn(&sys->obj, "Unsupported timestamp modifications done by chain_interactive");

    return filtered;
}

static vlc_render_subpicture *RenderSPUs(vout_thread_sys_t *sys,
                                const vlc_fourcc_t *subpicture_chromas,
                                const video_format_t *spu_frame,
                                vlc_tick_t system_now, vlc_tick_t render_subtitle_date,
                                bool ignore_osd, bool spu_in_full_window,
                                const vout_display_place_t *video_position)
{
    if (unlikely(sys->spu == NULL))
        return NULL;
    return spu_Render(sys->spu,
                      subpicture_chromas, spu_frame,
                      sys->display->source, spu_in_full_window, video_position,
                      system_now, render_subtitle_date,
                      ignore_osd);
}

static int PrerenderPicture(vout_thread_sys_t *sys, picture_t *filtered,
                            picture_t **out_pic,
                            vlc_render_subpicture **out_subpic)
{
    vout_display_t *vd = sys->display;

    /*
     * Get the rendering date for the current subpicture to be displayed.
     */
    vlc_tick_t system_now = vlc_tick_now();
    vlc_tick_t render_subtitle_date;
    if (sys->pause.is_on)
        render_subtitle_date = sys->pause.date;
    else if (filtered->b_force)
        render_subtitle_date = system_now;
    else
    {
        vlc_clock_Lock(sys->clock);
        render_subtitle_date = filtered->date <= VLC_TICK_0 ? system_now :
            vlc_clock_ConvertToSystem(sys->clock, system_now, filtered->date,
                                      sys->rate, NULL);
        vlc_clock_Unlock(sys->clock);
    }

    /*
     * Check whether we let the display draw the subpicture itself (when
     * vd_does_blending=true), and if we can fallback to blending the subpicture
     * ourselves (blending_before_converter=true).
     */
    const bool do_snapshot = vout_snapshot_IsRequested(sys->snapshot);
    const bool vd_does_blending = !do_snapshot &&
                                   vd->info.subpicture_chromas &&
                                   *vd->info.subpicture_chromas != 0;
    const bool spu_in_full_window = vd->cfg->display.full_fill &&
                                    vd_does_blending;

    //FIXME: Denying blending_before_converter if vd->source->orientation != ORIENT_NORMAL
    //will have the effect that snapshots miss the subpictures. We do this
    //because there is currently no way to transform subpictures to match
    //the source format.
    // In early SPU blending the blending is done into the source chroma,
    // otherwise it's done in the display chroma
    const bool blending_before_converter = vd->source->orientation == ORIENT_NORMAL;

    const vout_display_place_t *video_place = NULL; // default to fit the video
    video_format_t fmt_spu;
    if (vd_does_blending) {
        video_place = vd->place;

        fmt_spu = *vd->source;
        fmt_spu.i_sar_num = vd->cfg->display.sar.num;
        fmt_spu.i_sar_den = vd->cfg->display.sar.den;
        fmt_spu.i_x_offset       = 0;
        fmt_spu.i_y_offset       = 0;
        fmt_spu.i_width          =
        fmt_spu.i_visible_width  = vd->cfg->display.width;
        fmt_spu.i_height         =
        fmt_spu.i_visible_height = vd->cfg->display.height;
    } else {
        if (blending_before_converter) {
            fmt_spu = *vd->source;
        } else {
            fmt_spu = *vd->fmt;
            fmt_spu.i_sar_num = vd->cfg->display.sar.num;
            fmt_spu.i_sar_den = vd->cfg->display.sar.den;
        }

        if (sys->spu_blend &&
            !video_format_IsSameChroma(&sys->spu_blend->fmt_out.video, &fmt_spu)) {
            filter_DeleteBlend(sys->spu_blend);
            sys->spu_blend = NULL;
        }
        if (!sys->spu_blend) {
            sys->spu_blend = filter_NewBlend(VLC_OBJECT(&sys->obj), &fmt_spu);
            if (unlikely(sys->spu_blend == NULL))
                msg_Err(&sys->obj, "Failed to create blending filter, OSD/Subtitles will not work");
        }
    }

    /* Get the subpicture to be displayed. */
    video_format_t fmt_spu_rot;
    video_format_ApplyRotation(&fmt_spu_rot, &fmt_spu);
    /*
     * Perform rendering
     *
     * We have to:
     * - be sure to end up with a direct buffer.
     * - blend subtitles, and in a fast access buffer
     */
    picture_t *todisplay = filtered;
    picture_t *snap_pic = todisplay;
    if (!vd_does_blending && blending_before_converter && sys->spu_blend) {
        vlc_render_subpicture *subpic = RenderSPUs(sys, NULL, &fmt_spu_rot,
                                          system_now, render_subtitle_date,
                                          do_snapshot, spu_in_full_window, video_place);
        if (subpic) {
            picture_t *blent = picture_pool_Get(sys->private_pool);
            if (blent) {
                video_format_CopyCropAr(&blent->format, &filtered->format);
                picture_Copy(blent, filtered);
                if (picture_BlendSubpicture(blent, sys->spu_blend, subpic)) {
                    picture_Release(todisplay);
                    snap_pic = todisplay = blent;
                } else
                {
                    /* Blending failed, likely because the picture is opaque or
                     * read-only. Try to convert the opaque picture to a
                     * software RGB32 to generate a snapshot. */
                    if (do_snapshot)
                    {
                        picture_t *copy = ConvertRGBAAndBlend(sys, blent, subpic);
                        if (copy)
                            snap_pic = copy;
                    }
                    picture_Release(blent);
                }
            }
            vlc_render_subpicture_Delete(subpic);
        }
    }

    /*
     * Take a snapshot if requested
     */
    if (do_snapshot)
    {
        assert(snap_pic);
        vout_snapshot_Set(sys->snapshot, vd->source, snap_pic);
        if (snap_pic != todisplay)
            picture_Release(snap_pic);
    }

    /* Render the direct buffer */
    vout_UpdateDisplaySourceProperties(vd, &todisplay->format);

    todisplay = vout_ConvertForDisplay(vd, todisplay);
    if (todisplay == NULL) {
        return VLC_EGENERIC;
    }

    if (!vd_does_blending && !blending_before_converter && sys->spu_blend)
    {
        vlc_render_subpicture *subpic = RenderSPUs(sys, NULL, &fmt_spu_rot,
                                          system_now, render_subtitle_date,
                                          do_snapshot, spu_in_full_window, video_place);
        if (subpic)
        {
            picture_BlendSubpicture(todisplay, sys->spu_blend, subpic);
            vlc_render_subpicture_Delete(subpic);
        }
    }

    *out_pic = todisplay;
    if (vd_does_blending)
        *out_subpic = RenderSPUs(sys, vd->info.subpicture_chromas, &fmt_spu_rot,
                                 system_now, render_subtitle_date,
                                 false, spu_in_full_window, video_place);
    else
        *out_subpic = NULL;

    return VLC_SUCCESS;
}

enum render_picture_type
{
    RENDER_PICTURE_NORMAL,
    RENDER_PICTURE_FORCED,
    RENDER_PICTURE_NEXT,
};

static int RenderPicture(vout_thread_sys_t *sys,
                         enum render_picture_type render_type)
{
    vout_display_t *vd = sys->display;

    vout_chrono_Start(&sys->chrono.render);

    picture_t *filtered = FilterPictureInteractive(sys);
    if (!filtered)
        return VLC_EGENERIC;

    vlc_clock_Lock(sys->clock);
    sys->clock_nowait = false;
    vlc_clock_Unlock(sys->clock);
    vlc_queuedmutex_lock(&sys->display_lock);

    picture_t *todisplay;
    vlc_render_subpicture *subpic;
    int ret = PrerenderPicture(sys, filtered, &todisplay, &subpic);
    if (ret != VLC_SUCCESS)
    {
        vlc_queuedmutex_unlock(&sys->display_lock);
        return ret;
    }

    bool render_now = render_type != RENDER_PICTURE_NORMAL;

    vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t pts = todisplay->date;
    vlc_tick_t system_pts;
    if (render_now)
        system_pts = system_now;
    else
    {
        vlc_clock_Lock(sys->clock);
        assert(!sys->displayed.current->b_force);
        system_pts = vlc_clock_ConvertToSystem(sys->clock, system_now, pts,
                                               sys->rate, NULL);
        vlc_clock_Unlock(sys->clock);
    }

    const unsigned frame_rate = todisplay->format.i_frame_rate;
    const unsigned frame_rate_base = todisplay->format.i_frame_rate_base;

    if (vd->ops->prepare != NULL)
        vd->ops->prepare(vd, todisplay, subpic, system_pts);

    vout_chrono_Stop(&sys->chrono.render);

    struct vlc_tracer *tracer = GetTracer(sys);
    system_now = vlc_tick_now();
    if (!render_now)
    {
        const vlc_tick_t late = system_now - system_pts;
        if (unlikely(late > 0))
        {
            if (tracer != NULL)
                vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id, "late");
            msg_Dbg(vd, "picture displayed late (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
            vout_statistic_AddLate(&sys->statistic, 1);

            /* vd->prepare took too much time. Tell the clock that the pts was
             * rendered late. */
            system_pts = system_now;
        }
        else if (vd->ops->display != NULL)
        {
            vlc_tick_t max_deadline = system_now + VOUT_REDISPLAY_DELAY;

            /* Wait to reach system_pts if the plugin doesn't handle
             * asynchronous display */
            vlc_clock_Lock(sys->clock);

            bool timed_out = false;
            sys->wait_interrupted = false;
            while (!timed_out)
            {
                vlc_tick_t deadline;
                if (vlc_clock_IsPaused(sys->clock))
                    deadline = max_deadline;
                else
                {
                    assert(!sys->displayed.current->b_force);
                    deadline = vlc_clock_ConvertToSystem(sys->clock,
                                                         vlc_tick_now(), pts,
                                                         sys->rate, NULL);
                    if (deadline > max_deadline)
                        deadline = max_deadline;
                }

                if (sys->clock_nowait)
                {
                    /* A caller (the UI thread) awaits for the rendering to
                     * complete urgently, do not wait. */
                    sys->wait_interrupted = true;
                    break;
                }

                system_pts = deadline;
                timed_out = vlc_clock_Wait(sys->clock, deadline);
            }
            vlc_clock_Unlock(sys->clock);
        }
        sys->displayed.date = system_pts;
    }
    else
    {
        sys->displayed.date = system_now;
    }

    /* Next frames should be updated as forced points */
    if (render_type == RENDER_PICTURE_NEXT)
        system_now = VLC_TICK_MAX;
    else
        system_now = vlc_tick_now();

    /* Display the direct buffer returned by vout_RenderPicture */
    vout_display_Display(vd, todisplay);
    vlc_clock_Lock(sys->clock);
    vlc_tick_t drift = vlc_clock_UpdateVideo(sys->clock,
                                             system_now,
                                             pts, sys->rate,
                                             frame_rate, frame_rate_base);
    vlc_clock_Unlock(sys->clock);

    vlc_queuedmutex_unlock(&sys->display_lock);

    picture_Release(todisplay);

    if (subpic)
        vlc_render_subpicture_Delete(subpic);

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    if (tracer != NULL && system_pts != VLC_TICK_MAX)
        vlc_tracer_TraceWithTs(tracer, system_pts,
                               VLC_TRACE("type", "RENDER"),
                               VLC_TRACE("id", sys->str_id),
                               VLC_TRACE_TICK_NS("drift", drift),
                               VLC_TRACE_END);

    return VLC_SUCCESS;
}

static void UpdateDeinterlaceFilter(vout_thread_sys_t *sys)
{
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.changed ||
        sys->interlacing.has_deint != sys->filter.new_interlaced)
    {
        sys->interlacing.has_deint = sys->filter.new_interlaced;
        ChangeFilters(sys);
    }
    vlc_mutex_unlock(&sys->filter.lock);
}

static int DisplayNextFrame(vout_thread_sys_t *sys)
{
    UpdateDeinterlaceFilter(sys);

    picture_t *next = PreparePicture(sys, !sys->displayed.current, true);

    if (next)
    {
        if (likely(sys->displayed.current != NULL))
            picture_Release(sys->displayed.current);
        sys->displayed.current = next;
    }

    if (!next)
        return VLC_EGENERIC;

    return RenderPicture(sys, RENDER_PICTURE_NEXT);
}

static bool UpdateCurrentPicture(vout_thread_sys_t *sys)
{
    assert(sys->clock);

    if (sys->frame_next_count > 0)
    {
        if (DisplayNextFrame(sys) == VLC_SUCCESS)
            --sys->frame_next_count;
        return false;
    }

    if (sys->displayed.current == NULL)
    {
        sys->displayed.current = PreparePicture(sys, true, false);
        return sys->displayed.current != NULL;
    }

    if (sys->pause.is_on || sys->wait_interrupted)
        return false;

    /* Prevent to query the clock if we know that there are no next pictures.
     * Since the clock is likely no properly setup at that stage. Indeed, the
     * input/decoder.c send a first forced picture quickly, then a next one
     * when the clock is configured. */
    if (sys->first_picture)
    {
        picture_fifo_Lock(sys->decoder_fifo);
        bool has_next_pic = !picture_fifo_IsEmpty(sys->decoder_fifo);
        picture_fifo_Unlock(sys->decoder_fifo);
        if (!has_next_pic)
            return false;

        sys->first_picture = false;
    }

    const vlc_tick_t system_now = vlc_tick_now();
    vlc_clock_Lock(sys->clock);
    const vlc_tick_t system_swap_current =
        vlc_clock_ConvertToSystem(sys->clock, system_now,
                                  sys->displayed.current->date, sys->rate, NULL);
    vlc_clock_Unlock(sys->clock);

    vlc_tick_t system_prepare_current = system_swap_current - GetRenderDelay(sys);
    if (unlikely(system_prepare_current > system_now))
        // the current frame is not late, we still have time to display it
        // no need to get a new picture
        return true;

    // the current frame will be late, look for the next not late one
    picture_t *next = PreparePicture(sys, false, false);
    if (next == NULL)
        return false;
    /* We might have reset the current picture when preparing the next one,
     * because filters had to be changed. In this case, avoid releasing the
     * picture since it will lead to null pointer dereference errors. */
    if (sys->displayed.current != NULL)
        picture_Release(sys->displayed.current);

    sys->displayed.current = next;

    return true;
}

static vlc_tick_t DisplayPicture(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;

    assert(sys->clock);

    UpdateDeinterlaceFilter(sys);

    bool current_changed = UpdateCurrentPicture(sys);
    if (current_changed)
    {
        // next frame will still need some waiting before display, we don't need
        // to render now
        // display forced picture immediately
        bool render_now = sys->displayed.current->b_force;

        RenderPicture(vout, render_now ? RENDER_PICTURE_FORCED
                                       : RENDER_PICTURE_NORMAL);
        if (!render_now)
            /* Prepare the next picture immediately without waiting */
            return VLC_TICK_INVALID;
    }
    else if (sys->wait_interrupted)
    {
        sys->wait_interrupted = false;
        if (likely(sys->displayed.current != NULL))
            RenderPicture(vout, RENDER_PICTURE_FORCED);
        return VLC_TICK_INVALID;
    }
    else if (likely(sys->displayed.date != VLC_TICK_INVALID))
    {
        // next date we need to display again the current picture
        vlc_tick_t date_refresh = sys->displayed.date + VOUT_REDISPLAY_DELAY - GetRenderDelay(sys);
        const vlc_tick_t system_now = vlc_tick_now();
        /* FIXME/XXX we must redisplay the last decoded picture (because
        * of potential vout updated, or filters update or SPU update)
        * For now a high update period is needed but it could be removed
        * if and only if:
        * - vout module emits events from themselves.
        * - *and* SPU is modified to emit an event or a deadline when needed.
        *
        * So it will be done later.
        */
        if (date_refresh > system_now) {
            // nothing changed, wait until the next deadline or a control
            vlc_tick_t max_deadline = system_now + VOUT_REDISPLAY_DELAY;
            return __MIN(date_refresh, max_deadline);
        }
        RenderPicture(vout, RENDER_PICTURE_FORCED);
    }

    // wait until the next deadline or a control
    return vlc_tick_now() + VOUT_REDISPLAY_DELAY;
}

void vout_ChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    assert(!sys->pause.is_on || !is_paused);

    if (sys->pause.is_on)
        FilterFlush(sys, false);

    sys->pause.is_on = is_paused;
    sys->pause.date  = date;
    vout_control_Release(&sys->control);

    struct vlc_tracer *tracer = GetTracer(sys);
    if (tracer != NULL)
        vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id,
                              is_paused ? "paused" : "resumed");

    vlc_mutex_lock(&sys->window_lock);
    vlc_window_SetInhibition(sys->display_cfg.window, !is_paused);
    vlc_mutex_unlock(&sys->window_lock);
}

static void vout_FlushUnlocked(vout_thread_sys_t *vout, bool below,
                               vlc_tick_t date)
{
    vout_thread_sys_t *sys = vout;

    FilterFlush(vout, false); /* FIXME too much */

    picture_t *last = sys->displayed.decoded;
    if (last) {
        if ((date == VLC_TICK_INVALID) ||
            ( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            sys->displayed.decoded   = NULL;
            sys->displayed.date      = VLC_TICK_INVALID;
        }
    }

    picture_fifo_Lock(sys->decoder_fifo);
    picture_fifo_Flush(sys->decoder_fifo, date, below);
    picture_fifo_Unlock(sys->decoder_fifo);

    vlc_queuedmutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_FilterFlush(sys->display);
    /* Reinitialize chrono to ensure we re-compute any new render timing. */
    VoutResetChronoLocked(sys);
    vlc_queuedmutex_unlock(&sys->display_lock);

    if (sys->clock != NULL)
    {
        vlc_clock_Lock(sys->clock);
        vlc_clock_Reset(sys->clock);
        vlc_clock_SetDelay(sys->clock, sys->delay);
        vlc_clock_Unlock(sys->clock);
    }
    sys->first_picture = true;
}

vlc_tick_t vout_Flush(vout_thread_t *vout, vlc_tick_t date)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    vout_FlushUnlocked(sys, false, date);
    vlc_tick_t displayed_pts = sys->displayed.timestamp;
    vout_control_Release(&sys->control);

    struct vlc_tracer *tracer = GetTracer(sys);
    if (tracer != NULL)
        vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id, "flushed");

    return displayed_pts;
}

size_t vout_NextPicture(vout_thread_t *vout, size_t request_frame_count)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);

    sys->frame_next_count += request_frame_count;

    picture_fifo_Lock(sys->decoder_fifo);
    size_t pics_count = picture_fifo_GetCount(sys->decoder_fifo);
    size_t needed_count = sys->frame_next_count <= pics_count ? 0
                        : sys->frame_next_count - pics_count;
    picture_fifo_Unlock(sys->decoder_fifo);

    vout_control_ReleaseAndWake(&sys->control);

    return needed_count;
}

void vout_ChangeDelay(vout_thread_t *vout, vlc_tick_t delay)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->display);

    vout_control_Hold(&sys->control);
    vlc_clock_Lock(sys->clock);
    vlc_clock_SetDelay(sys->clock, delay);
    vlc_clock_Unlock(sys->clock);
    sys->delay = delay;
    vout_control_Release(&sys->control);
}

void vout_ChangeRate(vout_thread_t *vout, float rate)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    sys->rate = rate;
    vout_control_Release(&sys->control);
}

void vout_ChangeSpuDelay(vout_thread_t *vout, size_t channel_id,
                         vlc_tick_t delay)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->spu);
    spu_SetClockDelay(sys->spu, channel_id, delay);
}

void vout_ChangeSpuRate(vout_thread_t *vout, size_t channel_id, float rate)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->spu);
    spu_SetClockRate(sys->spu, channel_id, rate);
}

static int vout_Start(vout_thread_sys_t *vout, vlc_video_context *vctx, const vout_configuration_t *cfg)
{
    vout_thread_sys_t *sys = vout;
    filter_chain_t *cs, *ci;

    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    vout_display_window_SetMouseHandler(sys->display_cfg.window,
                                        cfg->mouse_event, cfg->mouse_opaque);
    vlc_mutex_unlock(&sys->window_lock);

    sys->private_pool = NULL;

    sys->filter.configuration = NULL;
    video_format_Copy(&sys->filter.src_fmt, &sys->original);
    sys->filter.src_vctx = vctx ? vlc_video_context_Hold(vctx) : NULL;

    static const struct filter_video_callbacks static_cbs = {
        VoutVideoFilterStaticNewPicture, VoutHoldDecoderDevice,
    };
    static const struct filter_video_callbacks interactive_cbs = {
        VoutVideoFilterInteractiveNewPicture, VoutHoldDecoderDevice,
    };
    filter_owner_t owner = {
        .video = &static_cbs,
        .sys = vout,
    };

    cs = filter_chain_NewVideo(&vout->obj, true, &owner);

    owner.video = &interactive_cbs;
    ci = filter_chain_NewVideo(&vout->obj, true, &owner);

    vlc_mutex_lock(&sys->filter.lock);
    sys->filter.chain_static = cs;
    sys->filter.chain_interactive = ci;
    vlc_mutex_unlock(&sys->filter.lock);

    vout_display_cfg_t dcfg;
    struct vout_crop crop;
    vlc_rational_t dar;

    vlc_mutex_lock(&sys->window_lock);
#ifndef NDEBUG
    if (vctx)
    {
        // make sure the decoder device we receive matches the one we have cached
        vlc_decoder_device *dec_device = vlc_video_context_HoldDevice(vctx);
        assert(dec_device && dec_device == sys->dec_device);
        vlc_decoder_device_Release(dec_device);
    }
#endif

    dcfg = sys->display_cfg;
    crop = sys->source.crop;
    dar = sys->source.dar;
    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    /* Reinitialize chrono to ensure we re-compute any new render timing. */
    VoutResetChronoLocked(sys);

    /* Setup the window size, protected by the display_lock */
    dcfg.display.width = sys->window_width;
    dcfg.display.height = sys->window_height;

    int projection = var_InheritInteger(&vout->obj, "projection-mode");
    if (projection == -1)
        dcfg.projection = sys->original.projection_mode;
    else if (projection >= 0)
        dcfg.projection = (video_projection_mode_t)projection;

    sys->private_pool =
        picture_pool_NewFromFormat(&sys->original, FILTER_POOL_SIZE);
    if (sys->private_pool == NULL) {
        vlc_queuedmutex_unlock(&sys->display_lock);
        goto error;
    }

    sys->display = vout_OpenWrapper(&vout->obj, sys->splitter_name, &dcfg,
                                    &sys->original, vctx);
    if (sys->display == NULL) {
        vlc_queuedmutex_unlock(&sys->display_lock);
        goto error;
    }

    vout_SetDisplayCrop(sys->display, &crop);

    vout_SetDisplayAspect(sys->display, dar);
    vlc_queuedmutex_unlock(&sys->display_lock);

    sys->displayed.current       = NULL;
    sys->displayed.decoded       = NULL;
    sys->displayed.date          = VLC_TICK_INVALID;
    sys->displayed.timestamp     = VLC_TICK_INVALID;
    sys->displayed.is_interlaced = false;

    sys->pause.is_on = false;
    sys->pause.date  = VLC_TICK_INVALID;

    sys->spu_blend               = NULL;

    video_format_Print(VLC_OBJECT(&vout->obj), "original format", &sys->original);
    return VLC_SUCCESS;
error:
    if (sys->filter.chain_interactive != NULL)
        DelAllFilterCallbacks(vout);
    vlc_mutex_lock(&sys->filter.lock);
    ci = sys->filter.chain_interactive;
    cs = sys->filter.chain_static;
    sys->filter.chain_interactive = NULL;
    sys->filter.chain_static = NULL;
    vlc_mutex_unlock(&sys->filter.lock);
    if (ci != NULL)
        filter_chain_Delete(ci);
    if (cs != NULL)
        filter_chain_Delete(cs);
    if (sys->private_pool)
    {
        picture_pool_Release(sys->private_pool);
        sys->private_pool = NULL;
    }
    video_format_Clean(&sys->filter.src_fmt);
    if (sys->filter.src_vctx)
    {
        vlc_video_context_Release(sys->filter.src_vctx);
        sys->filter.src_vctx = NULL;
    }
    vlc_mutex_lock(&sys->window_lock);
    vout_display_window_SetMouseHandler(sys->display_cfg.window, NULL, NULL);
    vlc_mutex_unlock(&sys->window_lock);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Thread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void *Thread(void *object)
{
    vout_thread_sys_t *vout = object;
    vout_thread_sys_t *sys = vout;

    vlc_thread_set_name("vlc-vout");

    vlc_tick_t deadline = VLC_TICK_INVALID;

    for (;;) {
        vout_control_Wait(&sys->control, deadline);

        if (atomic_load(&sys->control_is_terminated))
            break;

        /* A deadline of VLC_TICK_INVALID means "immediately" */
        deadline = DisplayPicture(vout);

        assert(deadline == VLC_TICK_INVALID ||
               deadline <= vlc_tick_now() + VOUT_REDISPLAY_DELAY);

        if (atomic_load(&sys->control_is_terminated))
            break;

        const bool picture_interlaced = sys->displayed.is_interlaced;

        vout_SetInterlacingState(&vout->obj, &sys->interlacing, picture_interlaced);
    }
    return NULL;
}

static void vout_ReleaseDisplay(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    filter_chain_t *ci, *cs;

    assert(sys->display != NULL);

    if (sys->spu_blend != NULL)
        filter_DeleteBlend(sys->spu_blend);

    /* Destroy the rendering display */
    if (sys->private_pool != NULL)
    {
        vout_FlushUnlocked(vout, true, VLC_TICK_MAX);

        picture_pool_Release(sys->private_pool);
        sys->private_pool = NULL;
    }

    vlc_queuedmutex_lock(&sys->display_lock);
    vout_CloseWrapper(&vout->obj, sys->display);
    sys->display = NULL;
    vlc_queuedmutex_unlock(&sys->display_lock);

    /* Destroy the video filters */
    DelAllFilterCallbacks(vout);
    vlc_mutex_lock(&sys->filter.lock);
    ci = sys->filter.chain_interactive;
    cs = sys->filter.chain_static;
    sys->filter.chain_interactive = NULL;
    sys->filter.chain_static = NULL;
    vlc_mutex_unlock(&sys->filter.lock);
    filter_chain_Delete(ci);
    filter_chain_Delete(cs);
    video_format_Clean(&sys->filter.src_fmt);
    if (sys->filter.src_vctx)
    {
        vlc_video_context_Release(sys->filter.src_vctx);
        sys->filter.src_vctx = NULL;
    }
    free(sys->filter.configuration);

    assert(sys->private_pool == NULL);

    vlc_mutex_lock(&sys->window_lock);
    vout_display_window_SetMouseHandler(sys->display_cfg.window, NULL, NULL);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->spu)
        spu_Detach(sys->spu);

    if (sys->clock_listener_id != NULL)
    {
        vlc_clock_Lock(sys->clock);
        vlc_clock_RemoveListener(sys->clock, sys->clock_listener_id);
        vlc_clock_Unlock(sys->clock);
        sys->clock_listener_id = NULL;
    }

    vlc_mutex_lock(&sys->clock_lock);
    sys->clock = NULL;
    vlc_mutex_unlock(&sys->clock_lock);
    sys->str_id = NULL;
    sys->clock_id = 0;
    sys->first_picture = true;
}

void vout_StopDisplay(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    atomic_store(&sys->control_is_terminated, true);
    // wake up so it goes back to the loop that will detect the terminated state
    vout_control_Wake(&sys->control);
    vlc_join(sys->thread, NULL);

    vout_ReleaseDisplay(sys);
}

static void vout_DisableWindow(vout_thread_sys_t *sys)
{
    vlc_mutex_lock(&sys->window_lock);
    if (sys->window_enabled) {
        vlc_window_Disable(sys->display_cfg.window);
        sys->window_enabled = false;
    }
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_Stop(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (sys->display != NULL)
        vout_StopDisplay(vout);

    vout_DisableWindow(sys);
}

void vout_Close(vout_thread_t *vout)
{
    assert(vout);

    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_Stop(vout);

    vout_IntfDeinit(VLC_OBJECT(vout));
    vout_snapshot_End(sys->snapshot);

    if (sys->spu)
        spu_Destroy(sys->spu);

    vout_Release(vout);
}

void vout_Release(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    if (!vlc_atomic_rc_dec(&sys->rc))
        return;

    if (sys->dummy)
    {
        vlc_object_delete(VLC_OBJECT(vout));
        return;
    }

    picture_fifo_Delete(sys->decoder_fifo);

    free(sys->splitter_name);
    free(sys->display_cfg.icc_profile);

    if (sys->dec_device)
        vlc_decoder_device_Release(sys->dec_device);

    assert(!sys->window_enabled);
    vout_display_window_Delete(sys->display_cfg.window);

    /* */
    vout_statistic_Clean(&sys->statistic);

    /* */
    vout_snapshot_Destroy(sys->snapshot);
    video_format_Clean(&sys->original);
    vlc_object_delete(VLC_OBJECT(vout));
}

static vout_thread_sys_t *vout_CreateCommon(vlc_object_t *object)
{
    /* Allocate descriptor */
    vout_thread_sys_t *vout = vlc_custom_create(object,
                                            sizeof(*vout),
                                            "video output");
    if (!vout)
        return NULL;

    vout_CreateVars(&vout->obj);

    vout_thread_sys_t *sys = vout;
    vlc_atomic_rc_init(&sys->rc);
    vlc_mouse_Init(&sys->mouse);
    return vout;
}

vout_thread_t *vout_CreateDummy(vlc_object_t *object)
{
    vout_thread_sys_t *vout = vout_CreateCommon(object);
    if (!vout)
        return NULL;

    vout_thread_sys_t *sys = vout;
    sys->dummy = true;
    return &vout->obj;
}

vout_thread_t *vout_Create(vlc_object_t *object)
{
    vout_thread_sys_t *p_vout = vout_CreateCommon(object);
    if (!p_vout)
        return NULL;
    vout_thread_t *vout = &p_vout->obj;
    vout_thread_sys_t *sys = p_vout;
    sys->dummy = false;

    sys->decoder_fifo = picture_fifo_New();
    if (sys->decoder_fifo == NULL)
    {
        vlc_object_delete(vout);
        return NULL;
    }

    /* Register the VLC variable and callbacks. On the one hand, the variables
     * must be ready early on because further initializations below depend on
     * some of them. On the other hand, the callbacks depend on said
     * initializations. In practice, this works because the object is not
     * visible and callbacks not triggerable before this function returns the
     * fully initialized object to its caller.
     */
    vout_IntfInit(vout);

    /* Get splitter name if present */
    sys->splitter_name = NULL;

    if (config_GetType("video-splitter")) {
        char *splitter_name = var_InheritString(vout, "video-splitter");
        if (unlikely(splitter_name == NULL)) {
            picture_fifo_Delete(sys->decoder_fifo);
            vlc_object_delete(vout);
            return NULL;
        }

        if (strcmp(splitter_name, "none") != 0) {
            var_Create(vout, "window", VLC_VAR_STRING);
            var_SetString(vout, "window", "wdummy");
            sys->splitter_name = splitter_name;
        } else
            free(splitter_name);
    }

    video_format_Init(&sys->original, 0);
    sys->source.dar = VLC_DAR_FROM_SOURCE;
    sys->source.crop.mode = VOUT_CROP_NONE;
    sys->snapshot = vout_snapshot_New();
    vout_statistic_Init(&sys->statistic);

    /* Initialize subpicture unit */
    sys->spu = var_InheritBool(vout, "spu") || var_InheritBool(vout, "osd") ?
               spu_Create(vout, vout) : NULL;

    vout_control_Init(&sys->control);
    atomic_init(&sys->control_is_terminated, false);

    sys->title.show     = var_InheritBool(vout, "video-title-show");
    sys->title.timeout  = var_InheritInteger(vout, "video-title-timeout");
    sys->title.position = var_InheritInteger(vout, "video-title-position");

    sys->private_pool = NULL;

    vout_InitInterlacingSupport(vout, &sys->interlacing);

    sys->is_late_dropped = var_InheritBool(vout, "drop-late-frames");

    vlc_mutex_init(&sys->filter.lock);

    vlc_mutex_init(&sys->clock_lock);
    sys->clock_nowait = false;
    sys->wait_interrupted = false;
    sys->first_picture = true;

    /* Display */
    sys->display = NULL;
    sys->display_cfg.icc_profile = NULL;
    vlc_queuedmutex_init(&sys->display_lock);

    /* Window */
    sys->window_width = sys->window_height = 0;
    sys->display_cfg.window = vout_display_window_New(vout);
    if (sys->display_cfg.window == NULL) {
        if (sys->spu)
            spu_Destroy(sys->spu);
        picture_fifo_Delete(sys->decoder_fifo);
        vlc_object_delete(vout);
        return NULL;
    }

    if (sys->splitter_name != NULL)
        var_Destroy(vout, "window");
    sys->window_enabled = false;
    sys->frame_next_count = 0;
    vlc_mutex_init(&sys->window_lock);

    if (var_InheritBool(vout, "video-wallpaper"))
        vlc_window_SetState(sys->display_cfg.window, VLC_WINDOW_STATE_BELOW);
    else if (var_InheritBool(vout, "video-on-top"))
        vlc_window_SetState(sys->display_cfg.window, VLC_WINDOW_STATE_ABOVE);

    return vout;
}

vout_thread_t *vout_Hold( vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    vlc_atomic_rc_inc(&sys->rc);
    return vout;
}

int vout_ChangeSource( vout_thread_t *vout, const video_format_t *original,
                       const vlc_video_context *vctx )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    if (sys->display == NULL)
        return -1;
    if (sys->filter.src_vctx != vctx)
        return -1;

     /* TODO: If dimensions are equal or slightly smaller, update the aspect
     * ratio and crop settings, instead of recreating a display.
     */
    if (!video_format_IsSimilar(original, &sys->original))
    {
        msg_Dbg(&vout->obj, "vout format changed");
        video_format_LogDifferences(vlc_object_logger(&vout->obj), "current", &sys->original, "new", original);
        return -1;
    }

    /* It is assumed that the SPU input matches input already. */
    return 0;
}

static int EnableWindowLocked(vout_thread_sys_t *vout, const video_format_t *original)
{
    assert(vout != NULL);
    vout_thread_sys_t *sys = vout;

    assert(!sys->dummy);
    vlc_mutex_assert(&sys->window_lock);
    VoutGetDisplayCfg(vout, original, &sys->display_cfg);
    vout_UpdateWindowSizeLocked(vout);

    if (!sys->window_enabled) {
        if (vlc_window_Enable(sys->display_cfg.window)) {
            msg_Err(&vout->obj, "failed to enable window");
            return -1;
        }
        sys->window_enabled = true;
    }
    return 0;
}

static void vout_InitSource(vout_thread_sys_t *vout)
{
    char *psz_ar = var_InheritString(&vout->obj, "aspect-ratio");
    if (psz_ar) {
        vlc_rational_t ar;
        if (vout_ParseDisplayAspectRatio(&ar, psz_ar))
            vout->source.dar = ar;
        free(psz_ar);
    }

    char *psz_crop = var_InheritString(&vout->obj, "crop");
    if (psz_crop) {
        if (!vout_ParseCrop(&vout->source.crop, psz_crop))
            vout->source.crop.mode = VOUT_CROP_NONE;
        free(psz_crop);
    }
}

static void clock_event_OnDiscontinuity(void *data)
{
    vout_thread_sys_t *vout = data;
    vout_thread_sys_t *sys = vout;

    /* The Render thread wait for a deadline that is either:
     *  - VOUT_REDISPLAY_DELAY
     *  - calculated from the clock
     * In case of a clock discontinuity, we need to wake up the Render thread,
     * in order to trigger the rendering of the next picture, if new timings
     * require it. */
    vout_control_Wake(&sys->control);
}

int vout_Request(const vout_configuration_t *cfg, vlc_video_context *vctx, input_thread_t *input)
{
    vout_thread_sys_t *vout = VOUT_THREAD_TO_SYS(cfg->vout);
    vout_thread_sys_t *sys = vout;

    assert(cfg->fmt != NULL);
    assert(cfg->clock != NULL);

    if (!VoutCheckFormat(cfg->fmt)) {
        if (sys->display != NULL)
            vout_StopDisplay(cfg->vout);
        return -1;
    }

    video_format_t original;
    VoutFixFormat(&original, cfg->fmt);

    if (vout_ChangeSource(cfg->vout, &original, vctx) == 0)
    {
        video_format_Clean(&original);
        return 0;
    }

    vlc_mutex_lock(&sys->window_lock);
    video_format_Clean(&sys->original);
    sys->original = original;
    sys->displayed.projection = original.projection_mode;
    vout_InitSource(vout);

    if (EnableWindowLocked(vout, &original) != 0)
    {
        /* the window was not enabled, nor the display started */
        msg_Err(cfg->vout, "failed to enable window");
        vlc_mutex_unlock(&sys->window_lock);
        assert(sys->display == NULL);
        return -1;
    }
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_StopDisplay(cfg->vout);

    vout_ReinitInterlacingSupport(cfg->vout, &sys->interlacing);

    sys->delay = 0;
    sys->rate = 1.f;
    sys->str_id = cfg->str_id;
    sys->clock_id = 0;

    vlc_mutex_lock(&sys->clock_lock);
    sys->clock = cfg->clock;
    vlc_mutex_unlock(&sys->clock_lock);

    static const struct vlc_clock_event_cbs clock_event_cbs = {
        .on_discontinuity = clock_event_OnDiscontinuity,
    };
    vlc_clock_Lock(sys->clock);
    sys->clock_listener_id =
        vlc_clock_AddListener(sys->clock, &clock_event_cbs, vout);
    vlc_clock_Unlock(sys->clock);

    sys->delay = 0;

    if (vout_Start(vout, vctx, cfg))
    {
        msg_Err(cfg->vout, "video output display creation failed");
        goto error_display;
    }
    atomic_store(&sys->control_is_terminated, false);
    if (vlc_clone(&sys->thread, Thread, vout))
        goto error_thread;

    if (input != NULL && sys->spu)
        spu_Attach(sys->spu, input);
    vout_IntfReinit(cfg->vout);
    return 0;

error_thread:
    vout_ReleaseDisplay(vout);
error_display:
    vout_DisableWindow(vout);
    if (sys->clock_listener_id != NULL)
    {
        vlc_clock_Lock(sys->clock);
        vlc_clock_RemoveListener(sys->clock, sys->clock_listener_id);
        vlc_clock_Unlock(sys->clock);
    }
    sys->clock_listener_id = NULL;
    vlc_mutex_lock(&sys->clock_lock);
    sys->clock = NULL;
    vlc_mutex_unlock(&sys->clock_lock);
    return -1;
}

vlc_decoder_device *vout_GetDevice(vout_thread_t *vout)
{
    vlc_decoder_device *dec_device = NULL;

    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    vlc_mutex_lock(&sys->window_lock);
    if (sys->dec_device == NULL)
        sys->dec_device = vlc_decoder_device_Create(&vout->obj, sys->display_cfg.window);
    dec_device = sys->dec_device ? vlc_decoder_device_Hold( sys->dec_device ) : NULL;
    vlc_mutex_unlock(&sys->window_lock);
    return dec_device;
}
