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

#include <libvlc.h>
#include "vout_private.h"
#include "vout_internal.h"
#include "display.h"
#include "snapshot.h"
#include "video_window.h"
#include "../misc/variables.h"
#include "../clock/clock.h"
#include "statistic.h"
#include "chrono.h"
#include "control.h"
#include "vout_scheduler.h"

typedef struct vout_thread_sys_t
{
    struct vout_thread_t obj;

    vout_thread_private_t private;

    bool dummy;

    /* Splitter module if used */
    char            *splitter_name;

    const char      *str_id;

    vlc_mutex_t clock_lock;
    bool clock_nowait; /* protected by vlc_clock_Lock()/vlc_clock_Unlock() */
    bool wait_interrupted;

    vlc_clock_t     *clock;
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
    vlc_fourcc_t    spu_blend_chroma;
    vlc_blender_t   *spu_blend;

    /* Thread & synchronization */
    vout_control_t  control;
    atomic_bool     control_is_terminated; // shutdown the vout thread
    struct vlc_vout_scheduler *scheduler;

    struct {
        vlc_tick_t  date;
        vlc_tick_t  timestamp;
        bool        is_interlaced;
        picture_t   *decoded; // decoded picture before passed through chain_static
        picture_t   *current;
        picture_t   *last_caption_reference;
        picture_t   *caption_reference;
        picture_captions_t captions;
#ifdef DEBUG_CAPTION
        FILE *caption_file;
#endif
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
    bool            rendering_enabled;
    vout_display_cfg_t display_cfg;
    vout_display_t *display;

    vlc_queuedmutex_t display_lock;
    vlc_mutex_t     render_lock;

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

    atomic_bool b_display_avstat;
    vlc_atomic_rc_t rc;

} vout_thread_sys_t;

#define VOUT_THREAD_TO_SYS(vout) \
    container_of(vout, vout_thread_sys_t, obj.obj)


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
    video_format_FixRgb(dst);
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
    if (!sys->decoder_fifo)
        return true;

    return picture_fifo_IsEmpty(sys->decoder_fifo);
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

void vout_FilterMouse(vout_thread_t *vout, vlc_mouse_t *mouse)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    vlc_mouse_t tmp[2], *m = mouse;

    /* Pass mouse events through the filter chains. */
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.chain_static != NULL
     && sys->filter.chain_interactive != NULL) {
        if (!filter_chain_MouseFilter(sys->filter.chain_interactive,
                                      &tmp[0], m))
            m = &tmp[0];
        if (!filter_chain_MouseFilter(sys->filter.chain_static,
                                      &tmp[1], m))
            m = &tmp[1];
    }
    vlc_mutex_unlock(&sys->filter.lock);

    if (mouse != m)
        *mouse = *m;
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
    picture_fifo_Push(sys->decoder_fifo, picture);
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
    vlc_mutex_lock(&sys->render_lock);
    vlc_queuedmutex_lock(&sys->display_lock);

    sys->rendering_enabled = width != 0 && height != 0;
    sys->window_width = width;
    sys->window_height = height;

    if (sys->display != NULL)
        vout_display_SetSize(sys->display, width, height);

    if (cb != NULL)
        cb(opaque);
    vlc_queuedmutex_unlock(&sys->display_lock);
    vlc_mutex_unlock(&sys->render_lock);
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

static void vout_SetAspectRatio(vout_thread_sys_t *sys,
                                     unsigned dar_num, unsigned dar_den)
{
    sys->source.dar.num = dar_num;
    sys->source.dar.den = dar_den;
}

void vout_ChangeDisplayAspectRatio(vout_thread_t *vout,
                                   unsigned dar_num, unsigned dar_den)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    vout_SetAspectRatio(sys, dar_num, dar_den);

    vout_UpdateWindowSizeLocked(sys);

    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayAspect(sys->display, dar_num, dar_den);
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

void vout_ChangeDisplayRenderingEnabled(vout_thread_t *vout, bool enabled)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->render_lock);
    sys->rendering_enabled = enabled;
    vlc_mutex_unlock(&sys->render_lock);
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
    filter_DelProxyCallbacks((vlc_object_t*)opaque, filter,
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

    picture_t *picture = picture_pool_Get(sys->private.private_pool);
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
    }

    if (sys->displayed.next)
    {
        picture_Release( sys->displayed.next );
        sys->displayed.next = NULL;
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

static void ChangeFilters(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    FilterFlush(vout, true);
    DelAllFilterCallbacks(vout);

    vlc_array_t array_static;
    vlc_array_t array_interactive;

    vlc_array_init(&array_static);
    vlc_array_init(&array_interactive);

    if (sys->private.interlacing.has_deint)
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
                if (!strcmp(e->name, "postproc"))
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

    if (!es_format_IsSimilar(p_fmt_current, &fmt_target) || vctx_current != vctx_target) {
        msg_Dbg(&vout->obj, "Changing vout format to %4.4s",
                            (const char *) &p_fmt_current->video.i_chroma);

        int ret = vout_SetDisplayFormat(sys->display, &p_fmt_current->video,
                                        vctx_current);
        if (ret != VLC_SUCCESS)
        {
            msg_Dbg(&vout->obj, "Changing vout format to %4.4s failed",
                                (const char *) &p_fmt_current->video.i_chroma);

            msg_Dbg(&vout->obj, "Adding a filter to compensate for format changes");
            if (filter_chain_AppendConverter(sys->filter.chain_interactive,
                                             &fmt_target) != 0) {
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

static bool IsPictureLate(vout_thread_sys_t *vout, picture_t *decoded,
                          vlc_tick_t system_now, vlc_tick_t system_pts)
{
    vout_thread_sys_t *sys = vout;

    const vlc_tick_t prepare_decoded_duration = vout_chrono_GetHigh(&sys->chrono.render) +
                                                vout_chrono_GetHigh(&sys->chrono.static_filter);
    vlc_tick_t late = system_now + prepare_decoded_duration - system_pts;

    vlc_tick_t late_threshold;
    if (decoded->format.i_frame_rate && decoded->format.i_frame_rate_base) {
        late_threshold = vlc_tick_from_samples(decoded->format.i_frame_rate_base, decoded->format.i_frame_rate);
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

static void UpdateDeinterlaceFilter(vout_thread_sys_t *sys);

/* */
VLC_USED
static picture_t *PreparePicture(void *opaque, bool reuse_decoded,
                                 bool frame_by_frame)
{
    vout_thread_sys_t *vout = opaque;
    vout_thread_sys_t *sys = vout;
    bool is_late_dropped = sys->is_late_dropped && !frame_by_frame;

    UpdateDeinterlaceFilter(sys);

    vlc_mutex_lock(&sys->filter.lock);

    picture_t *picture = filter_chain_VideoFilter(sys->filter.chain_static, NULL);
    assert(!reuse_decoded || !picture);

    while (!picture) {
        picture_t *decoded;
        if (unlikely(reuse_decoded && sys->displayed.decoded)) {
            decoded = picture_Hold(sys->displayed.decoded);
        } else {
            decoded = picture_fifo_Pop(sys->decoder_fifo);

            if (decoded) {
                if (is_late_dropped && !decoded->b_force)
                {
                    const vlc_tick_t system_now = vlc_tick_now();
                    const vlc_tick_t system_pts =
                        vlc_clock_ConvertToSystem(sys->clock, system_now,
                                                  decoded->date, sys->rate);

                    if (system_pts != VLC_TICK_MAX &&
                        IsPictureLate(vout, decoded, system_now, system_pts))
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
        }

        if (!decoded)
            break;
        reuse_decoded = false;

        if (sys->displayed.decoded)
            picture_Release(sys->displayed.decoded);

        sys->displayed.decoded       = picture_Hold(decoded);
        sys->displayed.timestamp     = decoded->date;
        sys->displayed.captions      = decoded->captions;
        sys->displayed.is_interlaced = !decoded->b_progressive;
        sys->displayed.caption_reference = decoded;

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
    return vout_GetDevice(&sys->obj);
}

static const struct filter_video_callbacks vout_video_cbs = {
    NULL, VoutHoldDecoderDevice,
};

static picture_t *ConvertRGB32AndBlend(vout_thread_sys_t *vout, picture_t *pic,
                                     subpicture_t *subpic)
{
    vout_thread_sys_t *sys = vout;
    /* This function will convert the pic to RGB32 and blend the subpic to it.
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
    dst.video.i_chroma = VLC_CODEC_RGB32;
    video_format_FixRgb(&dst.video);

    filter_chain_Reset(filterc, &src,
                       NULL /* TODO output video context of blender */,
                       &dst);

    if (filter_chain_AppendConverter(filterc, &dst) != 0)
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

static picture_t *FilterPictureInteractive(vout_thread_sys_t *sys, picture_t *pic)
{
    // hold it as the filter chain will release it or return it and we release it
    picture_Hold(pic);

    vlc_mutex_lock(&sys->render_lock);

    vlc_mutex_lock(&sys->filter.lock);
    picture_t *filtered = filter_chain_VideoFilter(sys->filter.chain_interactive, pic);
    vlc_mutex_unlock(&sys->filter.lock);

    if (filtered && filtered->date != pic->date)
        msg_Warn(&sys->obj, "Unsupported timestamp modifications done by chain_interactive");

    if (!filtered)
    {
        vlc_mutex_unlock(&sys->render_lock);
        return NULL;
    }


    return filtered;
}

static int PrerenderPicture(vout_thread_sys_t *sys, picture_t *filtered,
                            bool *render_now, picture_t **out_pic,
                            subpicture_t **out_subpic)
{
    vout_display_t *vd = sys->display;

    /*
     * Get the rendering date for the current subpicture to be displayed.
     */
    vlc_tick_t system_now = vlc_tick_now();
    vlc_tick_t render_subtitle_date;
    if (sys->pause.is_on)
        render_subtitle_date = sys->pause.date;
    else
    {
        render_subtitle_date = filtered->date <= VLC_TICK_0 ? system_now :
            vlc_clock_ConvertToSystem(sys->clock, system_now, filtered->date,
                                      sys->rate);

        /* The clock is paused, it's too late to fallback to the previous
         * picture, display the current picture anyway and force the rendering
         * to now. */
        if (unlikely(render_subtitle_date == VLC_TICK_MAX))
        {
            render_subtitle_date = system_now;
            *render_now = true;
        }
    }

    /*
     * Check whether we let the display draw the subpicture itself (when
     * do_dr_spu=true), and if we can fallback to blending the subpicture
     * ourselves (do_early_spu=true).
     */
    const bool do_snapshot = vout_snapshot_IsRequested(sys->snapshot);
    const bool do_dr_spu = !do_snapshot &&
                           vd->info.subpicture_chromas &&
                           *vd->info.subpicture_chromas != 0;

    //FIXME: Denying do_early_spu if vd->source->orientation != ORIENT_NORMAL
    //will have the effect that snapshots miss the subpictures. We do this
    //because there is currently no way to transform subpictures to match
    //the source format.
    const bool do_early_spu = !do_dr_spu &&
                               vd->source->orientation == ORIENT_NORMAL;

    const vlc_fourcc_t *subpicture_chromas;
    video_format_t fmt_spu;
    if (do_dr_spu) {
        vout_display_place_t place;
        vout_display_PlacePicture(&place, vd->source, &vd->cfg->display);

        fmt_spu = *vd->source;
        if (fmt_spu.i_width * fmt_spu.i_height < place.width * place.height) {
            fmt_spu.i_sar_num = vd->cfg->display.sar.num;
            fmt_spu.i_sar_den = vd->cfg->display.sar.den;
            fmt_spu.i_width          =
            fmt_spu.i_visible_width  = place.width;
            fmt_spu.i_height         =
            fmt_spu.i_visible_height = place.height;
        }
        subpicture_chromas = vd->info.subpicture_chromas;
    } else {
        if (do_early_spu) {
            fmt_spu = *vd->source;
        } else {
            fmt_spu = *vd->fmt;
            fmt_spu.i_sar_num = vd->cfg->display.sar.num;
            fmt_spu.i_sar_den = vd->cfg->display.sar.den;
        }
        subpicture_chromas = NULL;

        if (sys->spu_blend &&
            sys->spu_blend->fmt_out.video.i_chroma != fmt_spu.i_chroma) {
            filter_DeleteBlend(sys->spu_blend);
            sys->spu_blend = NULL;
            sys->spu_blend_chroma = 0;
        }
        if (!sys->spu_blend && sys->spu_blend_chroma != fmt_spu.i_chroma) {
            sys->spu_blend_chroma = fmt_spu.i_chroma;
            sys->spu_blend = filter_NewBlend(VLC_OBJECT(&sys->obj), &fmt_spu);
            if (!sys->spu_blend)
                msg_Err(&sys->obj, "Failed to create blending filter, OSD/Subtitles will not work");
        }
    }

    /* Get the subpicture to be displayed. */
    video_format_t fmt_spu_rot;
    video_format_ApplyRotation(&fmt_spu_rot, &fmt_spu);
    subpicture_t *subpic = !sys->spu ? NULL :
                           spu_Render(sys->spu,
                                      subpicture_chromas, &fmt_spu_rot,
                                      vd->source, system_now,
                                      render_subtitle_date,
                                      do_snapshot, vd->info.can_scale_spu);
    /*
     * Perform rendering
     *
     * We have to:
     * - be sure to end up with a direct buffer.
     * - blend subtitles, and in a fast access buffer
     */
    picture_t *todisplay = filtered;
    picture_t *snap_pic = todisplay;
    if (do_early_spu && subpic) {
        if (sys->spu_blend) {
            picture_t *blent = picture_pool_Get(sys->private.private_pool);
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
                     * software RGB32 one before blending it. */
                    if (do_snapshot)
                    {
                        picture_t *copy = ConvertRGB32AndBlend(sys, blent, subpic);
                        if (copy)
                            snap_pic = copy;
                    }
                    picture_Release(blent);
                }
            }
        }
        subpicture_Delete(subpic);
        subpic = NULL;
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
    vout_UpdateDisplaySourceProperties(vd, &todisplay->format, &sys->source.dar);

    todisplay = vout_ConvertForDisplay(vd, todisplay);
    if (todisplay == NULL) {
        vlc_mutex_unlock(&sys->render_lock);

        if (subpic != NULL)
            subpicture_Delete(subpic);
        return VLC_EGENERIC;
    }

    if (!do_dr_spu && subpic)
    {
        if (sys->spu_blend)
            picture_BlendSubpicture(todisplay, sys->spu_blend, subpic);

        /* The subpic will not be used anymore */
        subpicture_Delete(subpic);
        subpic = NULL;
    }

    *out_pic = todisplay;
    *out_subpic = subpic;
    return VLC_SUCCESS;
}

static int RenderPicture(void *opaque, picture_t *pic, bool render_now)
{
    vout_thread_t *vout = opaque;
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    vout_display_t *vd = sys->display;

    vout_chrono_Start(&sys->chrono.render);

    picture_t *filtered = FilterPictureInteractive(sys, pic);
    if (!filtered)
        return VLC_EGENERIC;

    vlc_clock_Lock(sys->clock);
    sys->clock_nowait = false;
    vlc_clock_Unlock(sys->clock);
    vlc_queuedmutex_lock(&sys->display_lock);

    picture_t *todisplay;
    subpicture_t *subpic;
    int ret = PrerenderPicture(sys, filtered, &render_now, &todisplay, &subpic);
    if (ret != VLC_SUCCESS)
    {
        vlc_queuedmutex_unlock(&sys->display_lock);
        return ret;
    }

    vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t pts = todisplay->date;
    vlc_tick_t system_pts = render_now ? system_now :
        vlc_clock_ConvertToSystem(sys->clock, system_now, pts, sys->rate);
    if (unlikely(system_pts == VLC_TICK_MAX))
    {
        /* The clock is paused, it's too late to fallback to the previous
         * picture, display the current picture anyway and force the rendering
         * to now. */
        system_pts = system_now;
        render_now = true;
    }

    const unsigned frame_rate = todisplay->format.i_frame_rate;
    const unsigned frame_rate_base = todisplay->format.i_frame_rate_base;

    if (sys->rendering_enabled)
    {
        if (vd->ops->prepare != NULL)
            vd->ops->prepare(vd, todisplay, subpic, system_pts);

        vout_chrono_Stop(&sys->chrono.render);
    }

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
                    deadline = vlc_clock_ConvertToSystemLocked(sys->clock,
                                                vlc_tick_now(), pts, sys->rate);
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
        /* Tell the clock that the pts was forced */
        system_pts = VLC_TICK_MAX;
    }

    vlc_tick_t clock_offset = vlc_clock_ConvertToSystem(sys->clock, system_now, 0, sys->rate);
    vlc_tick_t drift = vlc_clock_UpdateVideo(sys->clock, system_pts, pts, sys->rate,
                                             frame_rate, frame_rate_base);

    /* Display the direct buffer returned by vout_RenderPicture */
    if (sys->rendering_enabled)
        vout_display_Display(vd, todisplay);

    vlc_tick_t now_ts = vlc_tick_now();
    if (atomic_load(&sys->b_display_avstat))
        msg_Info( vd, "avstats: [RENDER][VIDEO] ts=%" PRId64 " pts_per_vsync=%" PRId64 " pts=%" PRId64 " pcr=%" PRId64,
                  NS_FROM_VLC_TICK(now_ts),
                  NS_FROM_VLC_TICK(pts),
                  NS_FROM_VLC_TICK(system_pts == INT64_MAX ? system_now : system_pts),
                  NS_FROM_VLC_TICK(system_now - clock_offset));

    vlc_queuedmutex_unlock(&sys->display_lock);
    vlc_mutex_unlock(&sys->render_lock);

    picture_Release(todisplay);

    if (subpic)
        subpicture_Delete(subpic);

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    if (tracer != NULL && system_pts != VLC_TICK_MAX)
        vlc_tracer_TraceWithTs(tracer, system_pts, VLC_TRACE("type", "RENDER"),
                               VLC_TRACE("id", sys->str_id),
                               VLC_TRACE("drift", drift), VLC_TRACE_END);

    if (sys->displayed.caption_reference &&
       sys->displayed.caption_reference != sys->displayed.last_caption_reference &&
       sys->displayed.caption_reference->captions.size > 0)
    {
#ifdef DEBUG_CAPTION
        msg_Warn(&vout->obj, " - new caption, size: %zu, %s", sys->displayed.caption_reference->captions.size,
                sys->displayed.caption_reference->b_top_field_first ? "TOP":"BOTTOM");
#endif
         vout_CaptionsToDisplay(vout,
                               sys->displayed.caption_reference->captions.bytes,
                               sys->displayed.caption_reference->captions.size);
#ifdef DEBUG_CAPTION
        fwrite(sys->displayed.caption_reference->captions.bytes, sizeof(char),
               sys->displayed.caption_reference->captions.size,
               sys->displayed.caption_file);
#endif

        sys->displayed.last_caption_reference = sys->displayed.caption_reference;
    }
#ifdef DEBUG_CAPTION
    else
    if(sys->displayed.caption_reference &&
       sys->displayed.caption_reference == sys->displayed.last_caption_reference)
    {
        msg_Err(&vout->obj, " - -- drop duplicate");
    }
#endif

    return VLC_SUCCESS;
}

static void UpdateDeinterlaceFilter(vout_thread_sys_t *sys)
{
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.changed ||
        sys->private.interlacing.has_deint != sys->filter.new_interlaced)
    {
        sys->private.interlacing.has_deint = sys->filter.new_interlaced;
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

    if (!sys->displayed.current)
        return VLC_EGENERIC;

    return VLC_SUCCESS; //RenderPicture(sys, true);
}

void vout_ChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    assert(!sys->pause.is_on || !is_paused);

    if (sys->pause.is_on)
        FilterFlush(sys, false);
    else {
        sys->step.timestamp = VLC_TICK_INVALID;
        sys->step.last      = VLC_TICK_INVALID;
    }
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

    sys->step.timestamp = VLC_TICK_INVALID;
    sys->step.last      = VLC_TICK_INVALID;

    FilterFlush(vout, false); /* FIXME too much */

    picture_t *last = sys->displayed.decoded;
    if (last) {
        if ((date == VLC_TICK_INVALID) ||
            ( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            sys->displayed.decoded   = NULL;
            sys->displayed.date      = VLC_TICK_INVALID;
            sys->displayed.timestamp = VLC_TICK_INVALID;
            /* Is caption still relevant here? */
            sys->displayed.captions.size = 0;
            sys->displayed.caption_reference = NULL;
        }
    }

    picture_fifo_Flush(sys->decoder_fifo, date, below);

    vlc_queuedmutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_FilterFlush(sys->display);
    /* Reinitialize chrono to ensure we re-compute any new render timing. */
    VoutResetChronoLocked(sys);
    vlc_queuedmutex_unlock(&sys->display_lock);

    if (sys->clock != NULL)
    {
        vlc_clock_Reset(sys->clock);
        vlc_clock_SetDelay(sys->clock, sys->delay);
    }
}

void vout_Flush(vout_thread_t *vout, vlc_tick_t date)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    vout_FlushUnlocked(sys, false, date);
    vout_control_Release(&sys->control);

    struct vlc_tracer *tracer = GetTracer(sys);
    if (tracer != NULL)
        vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id, "flushed");
}

void vout_NextPicture(vout_thread_t *vout, vlc_tick_t *duration)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    *duration = 0;

    vout_control_Hold(&sys->control);
    if (sys->step.last == VLC_TICK_INVALID)
        sys->step.last = sys->displayed.timestamp;

    if (DisplayNextFrame(sys) == VLC_SUCCESS) {
        sys->step.timestamp = sys->displayed.timestamp;

        if (sys->step.last != VLC_TICK_INVALID &&
            sys->step.timestamp > sys->step.last) {
            *duration = sys->step.timestamp - sys->step.last;
            sys->step.last = sys->step.timestamp;
            /* TODO advance subpicture by the duration ... */
        }
    }
    vout_control_Release(&sys->control);
}

void vout_ChangeDelay(vout_thread_t *vout, vlc_tick_t delay)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(sys->display);

    vout_control_Hold(&sys->control);
    vlc_clock_SetDelay(sys->clock, delay);
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

    sys->decoder_fifo = picture_fifo_New();
    sys->private.display_pool = NULL;
    sys->private.private_pool = NULL;

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
    unsigned num, den;

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
    num = sys->source.dar.num;
    den = sys->source.dar.den;
    vlc_queuedmutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    /* Reinitialize chrono to ensure we re-compute any new render timing. */
    VoutResetChronoLocked(sys);

    /* Setup the window size, protected by the display_lock */
    dcfg.display.width = sys->window_width;
    dcfg.display.height = sys->window_height;

    sys->display = vout_OpenWrapper(&vout->obj, &sys->private, sys->splitter_name, &dcfg,
                                    &sys->original, vctx);
    if (sys->display == NULL) {
        vlc_queuedmutex_unlock(&sys->display_lock);
        goto error;
    }

    vout_SetDisplayCrop(sys->display, &crop);

    if (num != 0 && den != 0)
        vout_SetDisplayAspect(sys->display, num, den);
    vlc_queuedmutex_unlock(&sys->display_lock);

    assert(sys->private.display_pool != NULL && sys->private.private_pool != NULL);

    sys->displayed.current       = NULL;
    sys->displayed.next          = NULL;
    sys->displayed.decoded       = NULL;
    sys->displayed.caption_reference = NULL;
    sys->displayed.last_caption_reference = NULL;
    sys->displayed.date          = VLC_TICK_INVALID;
    sys->displayed.timestamp     = VLC_TICK_INVALID;
    sys->displayed.captions.size = 0;
    sys->displayed.is_interlaced = false;

#ifdef DEBUG_CAPTION
    sys->displayed.caption_file = fopen("/tmp/caption_dump.hex", "wb");
    assert(sys->displayed.caption_file);
#endif


    sys->step.last               = VLC_TICK_INVALID;
    sys->step.timestamp          = VLC_TICK_INVALID;

    sys->pause.is_on = false;
    sys->pause.date  = VLC_TICK_INVALID;

    sys->spu_blend_chroma        = 0;
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
    video_format_Clean(&sys->filter.src_fmt);
    if (sys->filter.src_vctx)
    {
        vlc_video_context_Release(sys->filter.src_vctx);
        sys->filter.src_vctx = NULL;
    }
    if (sys->decoder_fifo != NULL)
    {
        picture_fifo_Delete(sys->decoder_fifo);
        sys->decoder_fifo = NULL;
    }
    vlc_mutex_lock(&sys->window_lock);
    vout_display_window_SetMouseHandler(sys->display_cfg.window, NULL, NULL);
    vlc_mutex_unlock(&sys->window_lock);
    return VLC_EGENERIC;
}

static void vout_ReleaseDisplay(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    filter_chain_t *ci, *cs;

    assert(sys->display != NULL);

    if (sys->spu_blend != NULL)
        filter_DeleteBlend(sys->spu_blend);

    /* Destroy the rendering display */
    if (sys->private.display_pool != NULL)
        vout_FlushUnlocked(vout, true, VLC_TICK_MAX);

    vlc_queuedmutex_lock(&sys->display_lock);
    vout_CloseWrapper(&vout->obj, &sys->private, sys->display);
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

    if (sys->decoder_fifo != NULL)
    {
        picture_fifo_Delete(sys->decoder_fifo);
        sys->decoder_fifo = NULL;
    }
    assert(sys->private.display_pool == NULL);

    vlc_mutex_lock(&sys->window_lock);
    vout_display_window_SetMouseHandler(sys->display_cfg.window, NULL, NULL);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->spu)
        spu_Detach(sys->spu);

    vlc_mutex_lock(&sys->clock_lock);
    sys->clock = NULL;
    sys->str_id = NULL;
    vlc_mutex_unlock(&sys->clock_lock);

#ifdef DEBUG_CAPTION
    msg_Info(vout, " -- fclose /tmp/caption_dump --");
    fclose(sys->displayed.caption_file);
#endif
}

void vout_StopDisplay(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    atomic_store(&sys->control_is_terminated, true);
    // wake up so it goes back to the loop that will detect the terminated state
    vout_control_Wake(&sys->control);
    vlc_vout_scheduler_Destroy(sys->scheduler);

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

static int avstat_callback(vlc_object_t *obj, const char *name,
        vlc_value_t oldval, vlc_value_t newval, void *opaque)
{
    vout_thread_sys_t *p_owner = opaque;
    atomic_store(&p_owner->b_display_avstat, newval.b_bool);
    vlc_mutex_lock(&p_owner->window_lock);
    if (p_owner->display_cfg.window)
        var_SetBool(p_owner->display_cfg.window, "avstat", newval.b_bool);
    vlc_mutex_unlock(&p_owner->window_lock);
    return VLC_SUCCESS;
}

static int ForwardValue(vlc_object_t *obj, const char *var, vlc_value_t oldv,
                        vlc_value_t newv, void *opaque)
{

    vlc_object_t *target = opaque;
    return var_Set(target, var, newv);
}

void vout_Close(vout_thread_t *vout)
{
    assert(vout);

    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (sys->display != NULL)
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

    var_DelCallback(vlc_object_parent(vout), "avstat", ForwardValue, vout);

    if (sys->dummy)
    {
        vlc_object_delete(VLC_OBJECT(vout));
        return;
    }
    else
        var_DelCallback(vout, "avstat", avstat_callback, sys);


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

static vout_thread_sys_t *
vout_CreateCommon(vlc_object_t *object, void *owner,
                  struct vlc_video_output_callbacks *cbs)
{
    /* Allocate descriptor */
    vout_thread_sys_t *vout = vlc_custom_create(object,
                                            sizeof(*vout),
                                            "video output");
    if (!vout)
        return NULL;

    vout->obj.owner = owner;
    vout->obj.cbs   = cbs;

    vout_CreateVars(&vout->obj);

    vout_thread_sys_t *sys = vout;
    sys->scheduler = NULL;

    vlc_atomic_rc_init(&sys->rc);
    vlc_mouse_Init(&sys->mouse);
    sys->rendering_enabled = true;

    var_AddCallback(vlc_object_parent(&vout->obj), "avstat", ForwardValue, vout);
    var_TriggerCallback(&vout->obj, "avstat");

    return vout;
}

vout_thread_t *vout_CreateDummy(vlc_object_t *object)
{
    vout_thread_sys_t *vout = vout_CreateCommon(object, NULL, NULL);
    if (!vout)
        return NULL;

    vout_thread_sys_t *sys = vout;
    sys->dummy = true;
    return &vout->obj;
}

vout_thread_t *vout_Create(vlc_object_t *object, void *owner,
                           struct vlc_video_output_callbacks *cbs)
{
    vout_thread_sys_t *p_vout = vout_CreateCommon(object, owner, cbs);
    if (!p_vout)
        return NULL;
    vout_thread_t *vout = &p_vout->obj;
    vout_thread_sys_t *sys = p_vout;
    sys->dummy = false;

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
    sys->source.dar.num = 0;
    sys->source.dar.den = 0;
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

    vout_InitInterlacingSupport(vout, &sys->private);

    sys->is_late_dropped = var_InheritBool(vout, "drop-late-frames");

    vlc_mutex_init(&sys->filter.lock);

    vlc_mutex_init(&sys->clock_lock);
    sys->clock_nowait = false;
    sys->wait_interrupted = false;

    /* Display */
    sys->display = NULL;
    sys->display_cfg.icc_profile = NULL;
    vlc_queuedmutex_init(&sys->display_lock);
    vlc_mutex_init(&sys->render_lock);

    /* Window */
    sys->window_width = sys->window_height = 0;
    sys->display_cfg.window = vout_display_window_New(vout);
    if (sys->display_cfg.window == NULL) {
        if (sys->spu)
            spu_Destroy(sys->spu);
        vlc_object_delete(vout);
        return NULL;
    }

    if (sys->splitter_name != NULL)
        var_Destroy(vout, "window");
    sys->window_enabled = false;
    vlc_mutex_init(&sys->window_lock);

    if (var_InheritBool(vout, "video-wallpaper"))
        vlc_window_SetState(sys->display_cfg.window, VLC_WINDOW_STATE_BELOW);
    else if (var_InheritBool(vout, "video-on-top"))
        vlc_window_SetState(sys->display_cfg.window, VLC_WINDOW_STATE_ABOVE);

    var_AddCallback(&vout->obj, "avstat", avstat_callback, sys);
    var_TriggerCallback(&vout->obj, "avstat");

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
    if (video_format_IsSimilar(original, &sys->original)) {
        /* It is assumed that the SPU input matches input already. */
        return 0;
    }

    return -1;
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
        unsigned num, den;
        if (!GetAspectRatio(psz_ar, &num, &den))
            vout_SetAspectRatio(vout, num, den);
        free(psz_ar);
    }

    char *psz_crop = var_InheritString(&vout->obj, "crop");
    if (psz_crop) {
        if (!vout_ParseCrop(&vout->source.crop, psz_crop))
            vout->source.crop.mode = VOUT_CROP_NONE;
        free(psz_crop);
    }
}

static bool WaitControl(void *opaque, vlc_tick_t deadline)
{
    vout_thread_t *vout = opaque;
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    vout_control_Wait(&sys->control, deadline);

    return atomic_load(&sys->control_is_terminated);
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

    vout_ReinitInterlacingSupport(cfg->vout, &sys->private);

    sys->delay = 0;
    sys->rate = 1.f;
    sys->str_id = cfg->str_id;

    vlc_mutex_lock(&sys->clock_lock);
    sys->clock = cfg->clock;
    vlc_mutex_unlock(&sys->clock_lock);

    sys->delay = 0;

    if (vout_Start(vout, vctx, cfg))
    {
        msg_Err(cfg->vout, "video output display creation failed");
        vout_DisableWindow(vout);
        return -1;
    }
    atomic_store(&sys->control_is_terminated, false);

    static const struct vlc_vout_scheduler_callbacks cbs =
    {
        .wait_control = WaitControl,
        .prepare_picture = PreparePicture,
        .render_picture = RenderPicture,
    };

    // TODO: display?
    if (var_InheritBool(&sys->obj, "vsync"))
    sys->scheduler = vlc_vout_scheduler_NewVSYNC(&sys->obj, cfg->clock, sys->display, &cbs, vout);
    else
    sys->scheduler = vlc_vout_scheduler_New(&sys->obj, cfg->clock, sys->display, &cbs, vout);

    if (sys->scheduler == NULL)
    {
        vout_ReleaseDisplay(vout);
        vout_DisableWindow(vout);
        return -1;
    }

    if (input != NULL && sys->spu)
        spu_Attach(sys->spu, input);
    vout_IntfReinit(cfg->vout);
    return 0;
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
