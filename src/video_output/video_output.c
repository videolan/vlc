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
#include <vlc_atomic.h>

#include <libvlc.h>
#include "vout_private.h"
#include "vout_internal.h"
#include "display.h"
#include "snapshot.h"
#include "window.h"
#include "../misc/variables.h"
#include "../clock/clock.h"
#include "statistic.h"
#include "chrono.h"
#include "control.h"

typedef struct vout_thread_sys_t
{
    struct vout_thread_t obj;

    vout_thread_private_t private;

    bool dummy;

    /* Splitter module if used */
    char            *splitter_name;

    vlc_clock_t     *clock;
    float           rate;
    vlc_tick_t      delay;

    video_format_t  original;   /* Original format ie coming from the decoder */

    /* */
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
    vout_control_t  control;
    vlc_thread_t    thread;

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

    /* */
    bool            is_late_dropped;

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

    /* Video filter2 chain */
    struct {
        vlc_mutex_t     lock;
        char            *configuration;
        video_format_t    src_fmt;
        vlc_video_context *src_vctx;
        struct filter_chain_t *chain_static;
        struct filter_chain_t *chain_interactive;
    } filter;

    picture_fifo_t  *decoder_fifo;
    vout_chrono_t   render;           /**< picture render time estimator */

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

static bool VideoFormatIsCropArEqual(video_format_t *dst,
                                     const video_format_t *src)
{
    return dst->i_sar_num * src->i_sar_den == dst->i_sar_den * src->i_sar_num &&
           dst->i_x_offset       == src->i_x_offset &&
           dst->i_y_offset       == src->i_y_offset &&
           dst->i_visible_width  == src->i_visible_width &&
           dst->i_visible_height == src->i_visible_height;
}

static void vout_display_SizeWindow(unsigned *restrict width,
                                    unsigned *restrict height,
                                    unsigned w, unsigned h,
                                    unsigned sar_num, unsigned sar_den,
                                    video_orientation_t orientation,
                                    const vout_display_cfg_t *restrict cfg)
{
    *width = cfg->display.width;
    *height = cfg->display.height;

    /* If both width and height are forced, keep them as is. */
    if (*width != 0 && *height != 0)
        return;

    /* Compute intended video resolution from source. */
    assert(sar_num > 0 && sar_den > 0);
    w = (w * sar_num) / sar_den;

    /* Adjust video size for orientation and pixel A/R. */
    if (ORIENT_IS_SWAP(orientation)) {
        unsigned x = w;

        w = h;
        h = x;
    }

    if (cfg->display.sar.num > 0 && cfg->display.sar.den > 0)
        w = (w * cfg->display.sar.den) / cfg->display.sar.num;

    /* If width is forced, adjust height according to the aspect ratio */
    if (*width != 0) {
        *height = (*width * h) / w;
        return;
    }

    /* If height is forced, adjust width according to the aspect ratio */
    if (*height != 0) {
        *width = (*height * w) / h;
        return;
    }

    /* If neither width nor height are forced, use the requested zoom. */
    *width = (w * cfg->zoom.num) / cfg->zoom.den;
    *height = (h * cfg->zoom.num) / cfg->zoom.den;
}

static void vout_SizeWindow(vout_thread_sys_t *vout,
                            const video_format_t *original,
                            unsigned *restrict width,
                            unsigned *restrict height)
{
    vout_thread_sys_t *sys = vout;
    unsigned w = original->i_visible_width;
    unsigned h = original->i_visible_height;
    unsigned sar_num = original->i_sar_num;
    unsigned sar_den = original->i_sar_num;

    switch (sys->source.crop.mode) {
        case VOUT_CROP_NONE:
            if (sys->source.dar.num > 0 && sys->source.dar.den > 0) {
                unsigned num = sys->source.dar.num * h;
                unsigned den = sys->source.dar.den * w;

                vlc_ureduce(&sar_num, &sar_den, num, den, 0);
            }
            break;

        case VOUT_CROP_RATIO: {
            unsigned num = sys->source.crop.ratio.num;
            unsigned den = sys->source.crop.ratio.den;

            if (w * den > h * num)
                w = h * num / den;
            else
                h = w * den / num;
            break;
        }

        case VOUT_CROP_WINDOW:
            w = sys->source.crop.window.width;
            h = sys->source.crop.window.height;
            break;

        case VOUT_CROP_BORDER:
            w = sys->source.crop.border.right - sys->source.crop.border.left;
            h = sys->source.crop.border.bottom - sys->source.crop.border.top;
            break;
    }

    /* If the vout thread is running, the window lock must be held here. */
    vout_display_SizeWindow(width, height, w, h, sar_num, sar_den,
                            original->orientation,
                            &sys->display_cfg);
}

static void vout_UpdateWindowSizeLocked(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    unsigned width, height;

    vlc_mutex_assert(&sys->window_lock);

    vlc_mutex_lock(&sys->display_lock);
    if (sys->display != NULL) {
        vout_SizeWindow(vout, &sys->original, &width, &height);
        vlc_mutex_unlock(&sys->display_lock);

        msg_Dbg(&vout->obj, "requested window size: %ux%u", width, height);
        vout_window_SetSize(sys->display_cfg.window, width, height);
    } else
        vlc_mutex_unlock(&sys->display_lock);
}

/* */
void vout_GetResetStatistic(vout_thread_t *vout, unsigned *restrict displayed,
                            unsigned *restrict lost)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vout_statistic_GetReset( &sys->statistic, displayed, lost );
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    if (!sys->decoder_fifo)
        return true;

    picture_t *picture = picture_fifo_Peek(sys->decoder_fifo);
    if (picture)
        picture_Release(picture);

    return !picture;
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

void vout_MouseState(vout_thread_t *vout, const vlc_mouse_t *mouse)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    assert(mouse);
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_MOUSE_STATE);
    cmd.mouse = *mouse;

    vout_control_Push(&sys->control, &cmd);
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
 * Allocates a video output picture buffer.
 *
 * Either vout_PutPicture() or picture_Release() must be used to return the
 * buffer to the video output free buffer pool.
 *
 * You may use picture_Hold() (paired with picture_Release()) to keep a
 * read-only reference.
 */
picture_t *vout_GetPicture(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    picture_t *picture = picture_pool_Wait(sys->private.display_pool);
    if (likely(picture != NULL)) {
        picture_Reset(picture);
        video_format_CopyCropAr(&picture->format, &sys->original);
    }
    return picture;
}

/**
 * It gives to the vout a picture to be displayed.
 *
 * The given picture MUST comes from vout_GetPicture.
 *
 * Becareful, after vout_PutPicture is called, picture_t::p_next cannot be
 * read/used.
 */
void vout_PutPicture(vout_thread_t *vout, picture_t *picture)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    picture->p_next = NULL;
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
    vout_window_SetFullScreen(sys->display_cfg.window, id);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeWindowed(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->window_lock);
    vout_window_UnsetFullScreen(sys->display_cfg.window);
    /* Attempt to reset the intended window size */
    vout_UpdateWindowSizeLocked(sys);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeWindowState(vout_thread_t *vout, unsigned st)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vlc_mutex_lock(&sys->window_lock);
    vout_window_SetState(sys->display_cfg.window, st);
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_ChangeDisplaySize(vout_thread_t *vout,
                            unsigned width, unsigned height)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    assert(!sys->dummy);

    /* DO NOT call this outside the vout window callbacks */
    vlc_mutex_lock(&sys->display_lock);

    sys->window_width = width;
    sys->window_height = height;

    if (sys->display != NULL)
        vout_display_SetSize(sys->display, width, height);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.is_display_filled = is_filled;
    /* no window size update here */

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayFilled(sys->display, is_filled);
    vlc_mutex_unlock(&sys->display_lock);
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
    sys->display_cfg.zoom.num = num;
    sys->display_cfg.zoom.den = den;

    vout_UpdateWindowSizeLocked(sys);

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayZoom(sys->display, num, den);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ChangeDisplayAspectRatio(vout_thread_t *vout,
                                   unsigned dar_num, unsigned dar_den)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    sys->source.dar.num = dar_num;
    sys->source.dar.den = dar_den;

    vout_UpdateWindowSizeLocked(sys);

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayAspect(sys->display, dar_num, dar_den);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ChangeCropRatio(vout_thread_t *vout, unsigned num, unsigned den)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vlc_mutex_lock(&sys->window_lock);
    if (num != 0 && den != 0) {
        sys->source.crop.mode = VOUT_CROP_RATIO;
        sys->source.crop.ratio.num = num;
        sys->source.crop.ratio.den = den;
    } else
        sys->source.crop.mode = VOUT_CROP_NONE;

    vout_UpdateWindowSizeLocked(sys);

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayCrop(sys->display, num, den, 0, 0, 0, 0);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ChangeCropWindow(vout_thread_t *vout,
                           int x, int y, int width, int height)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    vlc_mutex_lock(&sys->window_lock);
    sys->source.crop.mode = VOUT_CROP_WINDOW;
    sys->source.crop.window.x = x;
    sys->source.crop.window.y = y;
    sys->source.crop.window.width = width;
    sys->source.crop.window.height = height;

    vout_UpdateWindowSizeLocked(sys);

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayCrop(sys->display, 0, 0, x, y, width, height);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ChangeCropBorder(vout_thread_t *vout,
                           int left, int top, int right, int bottom)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right < 0)
        right = 0;
    if (bottom < 0)
        bottom = 0;

    vlc_mutex_lock(&sys->window_lock);
    sys->source.crop.mode = VOUT_CROP_BORDER;
    sys->source.crop.border.left = left;
    sys->source.crop.border.right = right;
    sys->source.crop.border.top = top;
    sys->source.crop.border.bottom = bottom;

    vout_UpdateWindowSizeLocked(sys);

    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_SetDisplayCrop(sys->display, 0, 0,
                            left, top, -right, -bottom);
    vlc_mutex_unlock(&sys->display_lock);
}

void vout_ControlChangeFilters(vout_thread_t *vout, const char *filters)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vout_control_PushString(&sys->control, VOUT_CONTROL_CHANGE_FILTERS,
                            filters);
}

void vout_ControlChangeInterlacing(vout_thread_t *vout, bool set)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    vout_control_PushBool(&sys->control, VOUT_CONTROL_CHANGE_INTERLACE, set);
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

    vlc_mutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_SetDisplayViewpoint(sys->display, p_viewpoint);
    vlc_mutex_unlock(&sys->display_lock);
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
    cfg->is_display_filled  = var_GetBool(vout, "autoscale");
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
    cfg->zoom.num = zoom_num;
    cfg->zoom.den = zoom_den;
    cfg->align.vertical = VLC_VIDEO_ALIGN_CENTER;
    cfg->align.horizontal = VLC_VIDEO_ALIGN_CENTER;
    const int align_mask = var_GetInteger(vout, "align");
    if (align_mask & VOUT_ALIGN_LEFT)
        cfg->align.horizontal = VLC_VIDEO_ALIGN_LEFT;
    else if (align_mask & VOUT_ALIGN_RIGHT)
        cfg->align.horizontal = VLC_VIDEO_ALIGN_RIGHT;
    if (align_mask & VOUT_ALIGN_TOP)
        cfg->align.vertical = VLC_VIDEO_ALIGN_TOP;
    else if (align_mask & VOUT_ALIGN_BOTTOM)
        cfg->align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
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

static int ThreadDelFilterCallbacks(filter_t *filter, void *opaque)
{
    filter_DelProxyCallbacks((vlc_object_t*)opaque, filter,
                             FilterRestartCallback);
    return VLC_SUCCESS;
}

static void ThreadDelAllFilterCallbacks(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;
    assert(sys->filter.chain_interactive != NULL);
    filter_chain_ForEach(sys->filter.chain_interactive,
                         ThreadDelFilterCallbacks, vout);
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

static void ThreadFilterFlush(vout_thread_sys_t *sys, bool is_locked)
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

static void ThreadChangeFilters(vout_thread_sys_t *vout,
                                const char *filters,
                                const bool *new_deinterlace,
                                bool is_locked)
{
    vout_thread_sys_t *sys = vout;
    ThreadFilterFlush(vout, is_locked);
    ThreadDelAllFilterCallbacks(vout);

    vlc_array_t array_static;
    vlc_array_t array_interactive;

    vlc_array_init(&array_static);
    vlc_array_init(&array_interactive);

    if (new_deinterlace != NULL)
        sys->private.interlacing.has_deint = *new_deinterlace;

    if (sys->private.interlacing.has_deint)
    {
        vout_filter_t *e = malloc(sizeof(*e));

        if (likely(e))
        {
            free(config_ChainCreate(&e->name, &e->cfg, "deinterlace"));
            vlc_array_append_or_abort(&array_static, e);
        }
    }

    if (filters == NULL) filters = sys->filter.configuration;
    char *current = filters ? strdup(filters) : NULL;
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

    if (!is_locked)
        vlc_mutex_lock(&sys->filter.lock);

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
            {
                msg_Err(&vout->obj, "Failed to add filter '%s'", e->name);
                config_ChainDestroy(e->cfg);
            }
            else if (a == 1) /* Add callbacks for interactive filters */
                filter_AddProxyCallbacks(&vout->obj, filter, FilterRestartCallback);

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
        msg_Dbg(&vout->obj, "Adding a filter to compensate for format changes");
        if (filter_chain_AppendConverter(sys->filter.chain_interactive,
                                         &fmt_target) != 0) {
            msg_Err(&vout->obj, "Failed to compensate for the format changes, removing all filters");
            ThreadDelAllFilterCallbacks(vout);
            filter_chain_Reset(sys->filter.chain_static,      &fmt_target, vctx_target, &fmt_target);
            filter_chain_Reset(sys->filter.chain_interactive, &fmt_target, vctx_target, &fmt_target);
        }
    }

    es_format_Clean(&fmt_target);

    if (sys->filter.configuration != filters) {
        free(sys->filter.configuration);
        sys->filter.configuration = filters ? strdup(filters) : NULL;
    }

    if (!is_locked)
        vlc_mutex_unlock(&sys->filter.lock);
}


/* */
static int ThreadDisplayPreparePicture(vout_thread_sys_t *vout, bool reuse,
                                       bool frame_by_frame, bool *paused)
{
    vout_thread_sys_t *sys = vout;
    bool is_late_dropped = sys->is_late_dropped && !sys->pause.is_on && !frame_by_frame;

    vlc_mutex_lock(&sys->filter.lock);

    picture_t *picture = filter_chain_VideoFilter(sys->filter.chain_static, NULL);
    assert(!reuse || !picture);

    while (!picture) {
        picture_t *decoded;
        if (reuse && sys->displayed.decoded) {
            decoded = picture_Hold(sys->displayed.decoded);
        } else {
            decoded = picture_fifo_Pop(sys->decoder_fifo);

            if (decoded) {
                if (is_late_dropped && !decoded->b_force) {
                    const vlc_tick_t date = vlc_tick_now();
                    const vlc_tick_t system_pts =
                        vlc_clock_ConvertToSystem(sys->clock, date,
                                                  decoded->date, sys->rate);

                    vlc_tick_t late;
                    if (system_pts == INT64_MAX)
                    {
                        /* The clock is paused, notify it (so that the current
                         * picture is displayed but not the next one), this
                         * current picture can't be be late. */
                        *paused = true;
                        late = 0;
                    }
                    else
                        late = date - system_pts;

                    vlc_tick_t late_threshold;
                    if (decoded->format.i_frame_rate && decoded->format.i_frame_rate_base)
                        late_threshold = VLC_TICK_FROM_MS(500) * decoded->format.i_frame_rate_base / decoded->format.i_frame_rate;
                    else
                        late_threshold = VOUT_DISPLAY_LATE_THRESHOLD;
                    if (late > late_threshold) {
                        msg_Warn(&vout->obj, "picture is too late to be displayed (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
                        picture_Release(decoded);
                        vout_statistic_AddLost(&sys->statistic, 1);
                        continue;
                    } else if (late > 0) {
                        msg_Dbg(&vout->obj, "picture might be displayed late (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
                    }
                }
                vlc_video_context *pic_vctx = picture_GetVideoContext(decoded);
                if (!VideoFormatIsCropArEqual(&decoded->format, &sys->filter.src_fmt))
                {
                    // we received an aspect ratio change
                    // Update the filters with the filter source format with the new aspect ratio
                    video_format_Clean(&sys->filter.src_fmt);
                    video_format_Copy(&sys->filter.src_fmt, &decoded->format);
                    if (sys->filter.src_vctx)
                        vlc_video_context_Release(sys->filter.src_vctx);
                    sys->filter.src_vctx = pic_vctx ? vlc_video_context_Hold(pic_vctx) : NULL;

                    ThreadChangeFilters(vout, NULL, NULL, true);
                }
            }
        }

        if (!decoded)
            break;
        reuse = false;

        if (sys->displayed.decoded)
            picture_Release(sys->displayed.decoded);

        sys->displayed.decoded       = picture_Hold(decoded);
        sys->displayed.timestamp     = decoded->date;
        sys->displayed.is_interlaced = !decoded->b_progressive;

        picture = filter_chain_VideoFilter(sys->filter.chain_static, decoded);
    }

    vlc_mutex_unlock(&sys->filter.lock);

    if (!picture)
        return VLC_EGENERIC;

    assert(!sys->displayed.next);
    if (!sys->displayed.current)
        sys->displayed.current = picture;
    else
        sys->displayed.next    = picture;
    return VLC_SUCCESS;
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

static int ThreadDisplayRenderPicture(vout_thread_sys_t *vout, bool is_forced)
{
    vout_thread_sys_t *sys = vout;

    picture_t *torender = picture_Hold(sys->displayed.current);

    vout_chrono_Start(&sys->render);

    vlc_mutex_lock(&sys->filter.lock);
    picture_t *filtered = filter_chain_VideoFilter(sys->filter.chain_interactive, torender);
    vlc_mutex_unlock(&sys->filter.lock);

    if (!filtered)
        return VLC_EGENERIC;

    if (filtered->date != sys->displayed.current->date)
        msg_Warn(&vout->obj, "Unsupported timestamp modifications done by chain_interactive");

    vout_display_t *vd = sys->display;

    vlc_mutex_lock(&sys->display_lock);

    /*
     * Get the subpicture to be displayed
     */
    const bool do_snapshot = vout_snapshot_IsRequested(sys->snapshot);
    vlc_tick_t system_now = vlc_tick_now();
    vlc_tick_t render_subtitle_date;
    if (sys->pause.is_on)
        render_subtitle_date = sys->pause.date;
    else
    {
        render_subtitle_date = filtered->date <= 1 ? system_now :
            vlc_clock_ConvertToSystem(sys->clock, system_now, filtered->date,
                                      sys->rate);

        /* The clock is paused, it's too late to fallback to the previous
         * picture, display the current picture anyway and force the rendering
         * to now. */
        if (unlikely(render_subtitle_date == INT64_MAX))
        {
            render_subtitle_date = system_now;
            is_forced = true;
        }
    }

    /*
     * Get the subpicture to be displayed
     */
    const bool do_dr_spu = !do_snapshot &&
                           vd->info.subpicture_chromas &&
                           *vd->info.subpicture_chromas != 0;

    //FIXME: Denying do_early_spu if vd->source.orientation != ORIENT_NORMAL
    //will have the effect that snapshots miss the subpictures. We do this
    //because there is currently no way to transform subpictures to match
    //the source format.
    const bool do_early_spu = !do_dr_spu &&
                               vd->source.orientation == ORIENT_NORMAL;

    const vlc_fourcc_t *subpicture_chromas;
    video_format_t fmt_spu;
    if (do_dr_spu) {
        vout_display_place_t place;
        vout_display_PlacePicture(&place, &vd->source, vd->cfg);

        fmt_spu = vd->source;
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
            fmt_spu = vd->source;
        } else {
            fmt_spu = vd->fmt;
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
            sys->spu_blend = filter_NewBlend(VLC_OBJECT(&vout->obj), &fmt_spu);
            if (!sys->spu_blend)
                msg_Err(&vout->obj, "Failed to create blending filter, OSD/Subtitles will not work");
        }
    }

    video_format_t fmt_spu_rot;
    video_format_ApplyRotation(&fmt_spu_rot, &fmt_spu);
    subpicture_t *subpic = !sys->spu ? NULL :
                           spu_Render(sys->spu,
                                      subpicture_chromas, &fmt_spu_rot,
                                      &vd->source, system_now,
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
                        picture_t *copy = ConvertRGB32AndBlend(vout, blent, subpic);
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
        vout_snapshot_Set(sys->snapshot, &vd->source, snap_pic);
        if (snap_pic != todisplay)
            picture_Release(snap_pic);
    }

    /* Render the direct buffer */
    vout_UpdateDisplaySourceProperties(vd, &todisplay->format);

    todisplay = vout_ConvertForDisplay(vd, todisplay);
    if (todisplay == NULL) {
        vlc_mutex_unlock(&sys->display_lock);

        if (subpic != NULL)
            subpicture_Delete(subpic);
        return VLC_EGENERIC;
    }

    if (!do_dr_spu && sys->spu_blend != NULL && subpic != NULL)
        picture_BlendSubpicture(todisplay, sys->spu_blend, subpic);

    system_now = vlc_tick_now();
    const vlc_tick_t pts = todisplay->date;
    vlc_tick_t system_pts = is_forced ? system_now :
        vlc_clock_ConvertToSystem(sys->clock, system_now, pts, sys->rate);
    if (unlikely(system_pts == INT64_MAX))
    {
        /* The clock is paused, it's too late to fallback to the previous
         * picture, display the current picture anyway and force the rendering
         * to now. */
        system_pts = system_now;
        is_forced = true;
    }

    const unsigned frame_rate = todisplay->format.i_frame_rate;
    const unsigned frame_rate_base = todisplay->format.i_frame_rate_base;

    if (vd->prepare != NULL)
        vd->prepare(vd, todisplay, do_dr_spu ? subpic : NULL, system_pts);

    vout_chrono_Stop(&sys->render);
#if 0
        {
        static int i = 0;
        if (((i++)%10) == 0)
            msg_Info(&vout->obj, "render: avg %d ms var %d ms",
                     (int)(sys->render.avg/1000), (int)(sys->render.var/1000));
        }
#endif

    system_now = vlc_tick_now();
    if (!is_forced)
    {
        if (unlikely(system_now > system_pts))
        {
            /* vd->prepare took too much time. Tell the clock that the pts was
             * rendered late. */
            system_pts = system_now;
        }
        else
        {
            /* Wait to reach system_pts */
            vlc_clock_Wait(sys->clock, system_now, pts, sys->rate,
                           VOUT_REDISPLAY_DELAY);

            /* Don't touch system_pts. Tell the clock that the pts was rendered
             * at the expected date */
        }
        sys->displayed.date = system_pts;
    }
    else
    {
        sys->displayed.date = system_now;
        /* Tell the clock that the pts was forced */
        system_pts = INT64_MAX;
    }
    vlc_clock_UpdateVideo(sys->clock, system_pts, pts, sys->rate,
                          frame_rate, frame_rate_base);

    /* Display the direct buffer returned by vout_RenderPicture */
    vout_display_Display(vd, todisplay);
    vlc_mutex_unlock(&sys->display_lock);

    if (subpic)
        subpicture_Delete(subpic);

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    return VLC_SUCCESS;
}

static int ThreadDisplayPicture(vout_thread_sys_t *vout, vlc_tick_t *deadline)
{
    vout_thread_sys_t *sys = vout;
    bool frame_by_frame = !deadline;
    bool paused = sys->pause.is_on;
    bool first = !sys->displayed.current;

    assert(sys->clock);

    if (first)
        if (ThreadDisplayPreparePicture(vout, true, frame_by_frame, &paused)) /* FIXME not sure it is ok */
            return VLC_EGENERIC;

    if (!paused || frame_by_frame)
        while (!sys->displayed.next
            && !ThreadDisplayPreparePicture(vout, false, frame_by_frame, &paused))
            ;

    const vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t render_delay = vout_chrono_GetHigh(&sys->render) + VOUT_MWAIT_TOLERANCE;

    bool drop_next_frame = frame_by_frame;
    vlc_tick_t date_next = VLC_TICK_INVALID;

    if (!paused && sys->displayed.next) {
        const vlc_tick_t next_system_pts =
            vlc_clock_ConvertToSystem(sys->clock, system_now,
                                      sys->displayed.next->date, sys->rate);
        if (unlikely(next_system_pts == INT64_MAX))
        {
            /* The clock was just paused, don't display the next frame (keep
             * the current one). */
            paused = true;
        }
        {
            date_next = next_system_pts - render_delay;
            if (date_next <= system_now)
                drop_next_frame = true;
        }
    }

    /* FIXME/XXX we must redisplay the last decoded picture (because
     * of potential vout updated, or filters update or SPU update)
     * For now a high update period is needed but it could be removed
     * if and only if:
     * - vout module emits events from theselves.
     * - *and* SPU is modified to emit an event or a deadline when needed.
     *
     * So it will be done later.
     */
    bool refresh = false;

    vlc_tick_t date_refresh = VLC_TICK_INVALID;
    if (sys->displayed.date != VLC_TICK_INVALID) {
        date_refresh = sys->displayed.date + VOUT_REDISPLAY_DELAY - render_delay;
        refresh = date_refresh <= system_now;
    }
    bool force_refresh = !drop_next_frame && refresh;

    if (!frame_by_frame) {
        if (date_refresh != VLC_TICK_INVALID)
            *deadline = date_refresh;
        if (date_next != VLC_TICK_INVALID && date_next < *deadline)
            *deadline = date_next;
    }

    if (!first && !refresh && !drop_next_frame) {
        return VLC_EGENERIC;
    }

    if (drop_next_frame) {
        picture_Release(sys->displayed.current);
        sys->displayed.current = sys->displayed.next;
        sys->displayed.next    = NULL;
    }

    if (!sys->displayed.current)
        return VLC_EGENERIC;

    /* display the picture immediately */
    bool is_forced = frame_by_frame || force_refresh || sys->displayed.current->b_force;
    int ret = ThreadDisplayRenderPicture(vout, is_forced);
    return force_refresh ? VLC_EGENERIC : ret;
}

void vout_ChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);

    vout_control_Hold(&sys->control);
    assert(!sys->pause.is_on || !is_paused);

    if (sys->pause.is_on)
        ThreadFilterFlush(sys, false);
    else {
        sys->step.timestamp = VLC_TICK_INVALID;
        sys->step.last      = VLC_TICK_INVALID;
    }
    sys->pause.is_on = is_paused;
    sys->pause.date  = date;
    vout_control_Release(&sys->control);

    vlc_mutex_lock(&sys->window_lock);
    vout_window_SetInhibition(sys->display_cfg.window, !is_paused);
    vlc_mutex_unlock(&sys->window_lock);
}

static void vout_FlushUnlocked(vout_thread_sys_t *vout, bool below,
                               vlc_tick_t date)
{
    vout_thread_sys_t *sys = vout;

    sys->step.timestamp = VLC_TICK_INVALID;
    sys->step.last      = VLC_TICK_INVALID;

    ThreadFilterFlush(vout, false); /* FIXME too much */

    picture_t *last = sys->displayed.decoded;
    if (last) {
        if ((date == VLC_TICK_INVALID) ||
            ( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            sys->displayed.decoded   = NULL;
            sys->displayed.date      = VLC_TICK_INVALID;
            sys->displayed.timestamp = VLC_TICK_INVALID;
        }
    }

    picture_fifo_Flush(sys->decoder_fifo, date, below);

    vlc_mutex_lock(&sys->display_lock);
    if (sys->display != NULL)
        vout_FilterFlush(sys->display);
    vlc_mutex_unlock(&sys->display_lock);

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
}

void vout_NextPicture(vout_thread_t *vout, vlc_tick_t *duration)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);
    assert(!sys->dummy);
    *duration = 0;

    vout_control_Hold(&sys->control);
    if (sys->step.last == VLC_TICK_INVALID)
        sys->step.last = sys->displayed.timestamp;

    if (ThreadDisplayPicture(sys, NULL) == 0) {
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

static void ThreadProcessMouseState(vout_thread_sys_t *p_vout,
                                    const vlc_mouse_t *win_mouse)
{
    vlc_mouse_t vid_mouse, tmp1, tmp2, *m;
    vout_thread_t *vout = &p_vout->obj;
    vout_thread_sys_t *sys = p_vout;

    /* Translate window coordinates to video coordinates */
    vlc_mutex_lock(&sys->display_lock);
    vout_display_TranslateMouseState(sys->display, &vid_mouse, win_mouse);
    vlc_mutex_unlock(&sys->display_lock);

    /* Then pass up the filter chains. */
    m = &vid_mouse;
    vlc_mutex_lock(&sys->filter.lock);
    if (sys->filter.chain_static && sys->filter.chain_interactive) {
        if (!filter_chain_MouseFilter(sys->filter.chain_interactive,
                                      &tmp1, m))
            m = &tmp1;
        if (!filter_chain_MouseFilter(sys->filter.chain_static,
                                      &tmp2, m))
            m = &tmp2;
    }
    vlc_mutex_unlock(&sys->filter.lock);

    if (vlc_mouse_HasMoved(&sys->mouse, m))
        var_SetCoords(vout, "mouse-moved", m->i_x, m->i_y);

    if (vlc_mouse_HasButton(&sys->mouse, m)) {
        var_SetInteger(vout, "mouse-button-down", m->i_pressed);

        if (vlc_mouse_HasPressed(&sys->mouse, m, MOUSE_BUTTON_LEFT)) {
            /* FIXME? */
            int x, y;

            var_GetCoords(vout, "mouse-moved", &x, &y);
            var_SetCoords(vout, "mouse-clicked", x, y);
        }
    }

    if (m->b_double_click)
        var_ToggleBool(vout, "fullscreen");
    sys->mouse = *m;

    if (sys->mouse_event)
        sys->mouse_event(m, sys->mouse_opaque);
}

static int vout_Start(vout_thread_sys_t *vout, vlc_video_context *vctx, const vout_configuration_t *cfg)
{
    vout_thread_sys_t *sys = vout;
    assert(!sys->dummy);

    sys->mouse_event = cfg->mouse_event;
    sys->mouse_opaque = cfg->mouse_opaque;
    vlc_mouse_Init(&sys->mouse);

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
    sys->filter.chain_static = filter_chain_NewVideo(&vout->obj, true, &owner);

    owner.video = &interactive_cbs;
    sys->filter.chain_interactive = filter_chain_NewVideo(&vout->obj, true, &owner);

    vout_display_cfg_t dcfg;
    int x = 0, y = 0, w = 0, h = 0;
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

    switch (sys->source.crop.mode) {
        case VOUT_CROP_NONE:
            break;
        case VOUT_CROP_RATIO:
            num = sys->source.crop.ratio.num;
            den = sys->source.crop.ratio.den;
            break;
        case VOUT_CROP_WINDOW:
            x = sys->source.crop.window.x;
            y = sys->source.crop.window.y;
            w = sys->source.crop.window.width;
            h = sys->source.crop.window.height;
            break;
        case VOUT_CROP_BORDER:
            x = sys->source.crop.border.left;
            y = sys->source.crop.border.top;
            w = -(int)sys->source.crop.border.right;
            h = -(int)sys->source.crop.border.bottom;
            break;
    }

    num = sys->source.dar.num;
    den = sys->source.dar.den;
    vlc_mutex_lock(&sys->display_lock);
    vlc_mutex_unlock(&sys->window_lock);

    /* Setup the window size, protected by the display_lock */
    dcfg.window_props.width = sys->window_width;
    dcfg.window_props.height = sys->window_height;

    sys->display = vout_OpenWrapper(&vout->obj, &sys->private, sys->splitter_name, &dcfg,
                                    &sys->original, vctx);
    if (sys->display == NULL) {
        vlc_mutex_unlock(&sys->display_lock);
        goto error;
    }

    vout_SetDisplayCrop(sys->display, num, den, x, y, w, h);

    if (num != 0 && den != 0)
        vout_SetDisplayAspect(sys->display, num, den);
    vlc_mutex_unlock(&sys->display_lock);

    assert(sys->private.display_pool != NULL && sys->private.private_pool != NULL);

    sys->displayed.current       = NULL;
    sys->displayed.next          = NULL;
    sys->displayed.decoded       = NULL;
    sys->displayed.date          = VLC_TICK_INVALID;
    sys->displayed.timestamp     = VLC_TICK_INVALID;
    sys->displayed.is_interlaced = false;

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
    {
        ThreadDelAllFilterCallbacks(vout);
        filter_chain_Delete(sys->filter.chain_interactive);
    }
    if (sys->filter.chain_static != NULL)
        filter_chain_Delete(sys->filter.chain_static);
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

    vlc_tick_t deadline = VLC_TICK_INVALID;
    bool wait = false;

    for (;;) {
        vout_control_cmd_t cmd;

        if (wait)
        {
            const vlc_tick_t max_deadline = vlc_tick_now() + VLC_TICK_FROM_MS(100);
            deadline = deadline == VLC_TICK_INVALID ? max_deadline : __MIN(deadline, max_deadline);
        } else {
            deadline = VLC_TICK_INVALID;
        }

        while (!vout_control_Pop(&sys->control, &cmd, deadline)) {
            switch(cmd.type) {
                case VOUT_CONTROL_TERMINATE:
                    return NULL; /* no need to clean &cmd */
                case VOUT_CONTROL_CHANGE_FILTERS:
                    ThreadChangeFilters(vout, cmd.string, NULL, false);
                    break;
                case VOUT_CONTROL_CHANGE_INTERLACE:
                    ThreadChangeFilters(vout, NULL, &cmd.boolean, false);
                    break;
                case VOUT_CONTROL_MOUSE_STATE:
                    ThreadProcessMouseState(vout, &cmd.mouse);
                    break;
            }
            vout_control_cmd_Clean(&cmd);
        }

        deadline = VLC_TICK_INVALID;
        wait = ThreadDisplayPicture(vout, &deadline) != VLC_SUCCESS;

        const bool picture_interlaced = sys->displayed.is_interlaced;

        vout_SetInterlacingState(&vout->obj, &sys->private, picture_interlaced);
    }
}

static void vout_ReleaseDisplay(vout_thread_sys_t *vout)
{
    vout_thread_sys_t *sys = vout;

    assert(sys->display != NULL);

    if (sys->spu_blend != NULL)
        filter_DeleteBlend(sys->spu_blend);

    /* Destroy the rendering display */
    if (sys->private.display_pool != NULL)
        vout_FlushUnlocked(vout, true, INT64_MAX);

    vlc_mutex_lock(&sys->display_lock);
    vout_CloseWrapper(&vout->obj, &sys->private, sys->display);
    sys->display = NULL;
    vlc_mutex_unlock(&sys->display_lock);

    /* Destroy the video filters */
    ThreadDelAllFilterCallbacks(vout);
    filter_chain_Delete(sys->filter.chain_interactive);
    filter_chain_Delete(sys->filter.chain_static);
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

    if (sys->mouse_event)
        sys->mouse_event(NULL, sys->mouse_opaque);

    if (sys->spu)
        spu_Detach(sys->spu);
    sys->mouse_event = NULL;
    sys->clock = NULL;
    video_format_Clean(&sys->original);
}

void vout_StopDisplay(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    vout_control_PushVoid(&sys->control, VOUT_CONTROL_TERMINATE);
    vlc_join(sys->thread, NULL);

    vout_ReleaseDisplay(sys);
}

static void vout_DisableWindow(vout_thread_sys_t *sys)
{
    vlc_mutex_lock(&sys->window_lock);
    if (sys->window_enabled) {
        vout_window_Disable(sys->display_cfg.window);
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

    if (sys->display != NULL)
        vout_Stop(vout);

    vout_IntfDeinit(VLC_OBJECT(vout));
    vout_snapshot_End(sys->snapshot);
    vout_control_Dead(&sys->control);
    vout_chrono_Clean(&sys->render);

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

    free(sys->splitter_name);

    if (sys->dec_device)
        vlc_decoder_device_Release(sys->dec_device);

    assert(!sys->window_enabled);
    vout_display_window_Delete(sys->display_cfg.window);

    vout_control_Clean(&sys->control);

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

    /* Register the VLC variable and callbacks. On the one hand, the variables
     * must be ready early on because further initializations below depend on
     * some of them. On the other hand, the callbacks depend on said
     * initializations. In practice, this works because the object is not
     * visible and callbacks not triggerable before this function returns the
     * fully initialized object to its caller.
     */
    vout_IntfInit(vout);

    /* Get splitter name if present */
    sys->splitter_name = config_GetType("video-splitter") ?
        var_InheritString(vout, "video-splitter") : NULL;
    if (sys->splitter_name != NULL) {
        var_Create(vout, "window", VLC_VAR_STRING);
        var_SetString(vout, "window", "wdummy");
    }

    sys->original.i_chroma = 0;
    sys->source.dar.num = 0;
    sys->source.dar.den = 0;
    sys->source.crop.mode = VOUT_CROP_NONE;
    sys->snapshot = vout_snapshot_New();
    vout_statistic_Init(&sys->statistic);

    /* Initialize subpicture unit */
    sys->spu = var_InheritBool(vout, "spu") || var_InheritBool(vout, "osd") ?
               spu_Create(vout, vout) : NULL;

    vout_control_Init(&sys->control);

    sys->title.show     = var_InheritBool(vout, "video-title-show");
    sys->title.timeout  = var_InheritInteger(vout, "video-title-timeout");
    sys->title.position = var_InheritInteger(vout, "video-title-position");

    vout_InitInterlacingSupport(vout, &sys->private);

    sys->is_late_dropped = var_InheritBool(vout, "drop-late-frames");

    vlc_mutex_init(&sys->filter.lock);

    /* Display */
    sys->display = NULL;
    vlc_mutex_init(&sys->display_lock);

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

    /* Arbitrary initial time */
    vout_chrono_Init(&sys->render, 5, VLC_TICK_FROM_MS(10));

    if (var_InheritBool(vout, "video-wallpaper"))
        vout_window_SetState(sys->display_cfg.window, VOUT_WINDOW_STATE_BELOW);
    else if (var_InheritBool(vout, "video-on-top"))
        vout_window_SetState(sys->display_cfg.window, VOUT_WINDOW_STATE_ABOVE);

    return vout;
}

vout_thread_t *vout_Hold( vout_thread_t *vout)
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

    vlc_atomic_rc_inc(&sys->rc);
    return vout;
}

int vout_ChangeSource( vout_thread_t *vout, const video_format_t *original )
{
    vout_thread_sys_t *sys = VOUT_THREAD_TO_SYS(vout);

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

    if (!sys->window_enabled) {
        vout_window_cfg_t wcfg = {
            .is_fullscreen = var_GetBool(&vout->obj, "fullscreen"),
            .is_decorated = var_InheritBool(&vout->obj, "video-deco"),
        // TODO: take pixel A/R, crop and zoom into account
#if defined(__APPLE__) || defined(_WIN32)
            .x = var_InheritInteger(&vout->obj, "video-x"),
            .y = var_InheritInteger(&vout->obj, "video-y"),
#endif
        };

        VoutGetDisplayCfg(vout, original, &sys->display_cfg);
        vout_SizeWindow(vout, original, &wcfg.width, &wcfg.height);

        if (vout_window_Enable(sys->display_cfg.window, &wcfg)) {
            msg_Err(&vout->obj, "failed to enable window");
            return -1;
        }
        sys->window_enabled = true;
    } else
        vout_UpdateWindowSizeLocked(vout);
    return 0;
}

int vout_Request(const vout_configuration_t *cfg, vlc_video_context *vctx, input_thread_t *input)
{
    vout_thread_sys_t *vout = VOUT_THREAD_TO_SYS(cfg->vout);
    vout_thread_sys_t *sys = vout;

    assert(cfg->fmt != NULL);
    assert(cfg->clock != NULL);

    if (!VoutCheckFormat(cfg->fmt))
        /* don't stop the display and keep sys->original */
        return -1;

    video_format_t original;
    VoutFixFormat(&original, cfg->fmt);

    if (vout_ChangeSource(cfg->vout, &original) == 0)
    {
        video_format_Clean(&original);
        return 0;
    }

    vlc_mutex_lock(&sys->window_lock);
    if (EnableWindowLocked(vout, &original) != 0)
    {
        /* the window was not enabled, nor the display started */
        msg_Err(cfg->vout, "failed to enable window");
        video_format_Clean(&original);
        vlc_mutex_unlock(&sys->window_lock);
        return -1;
    }
    vlc_mutex_unlock(&sys->window_lock);

    if (sys->display != NULL)
        vout_StopDisplay(cfg->vout);

    vout_ReinitInterlacingSupport(cfg->vout, &sys->private);

    sys->original = original;

    sys->delay = 0;
    sys->rate = 1.f;
    sys->clock = cfg->clock;
    sys->delay = 0;

    if (vout_Start(vout, vctx, cfg))
    {
        msg_Err(cfg->vout, "video output display creation failed");
        video_format_Clean(&sys->original);
        vout_DisableWindow(vout);
        return -1;
    }
    if (vlc_clone(&sys->thread, Thread, vout, VLC_THREAD_PRIORITY_OUTPUT)) {
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
