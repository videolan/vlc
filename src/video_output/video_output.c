/*****************************************************************************
 * video_output.c : video output thread
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2007 VLC authors and VideoLAN
 * $Id$
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

#include <stdlib.h>                                                /* free() */
#include <string.h>
#include <assert.h>

#include <vlc_vout.h>

#include <vlc_filter.h>
#include <vlc_spu.h>
#include <vlc_vout_osd.h>
#include <vlc_image.h>
#include <vlc_plugin.h>

#include <libvlc.h>
#include "vout_internal.h"
#include "interlacing.h"
#include "display.h"
#include "snapshot.h"
#include "window.h"
#include "../misc/variables.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *Thread(void *);
static void VoutDestructor(vlc_object_t *);

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
static int VoutValidateFormat(video_format_t *dst,
                              const video_format_t *src)
{
    if (src->i_width == 0  || src->i_width  > 8192 ||
        src->i_height == 0 || src->i_height > 8192)
        return VLC_EGENERIC;
    if (src->i_sar_num <= 0 || src->i_sar_den <= 0)
        return VLC_EGENERIC;

    /* */
    video_format_Copy(dst, src);
    dst->i_chroma = vlc_fourcc_GetCodec(VIDEO_ES, src->i_chroma);
    vlc_ureduce( &dst->i_sar_num, &dst->i_sar_den,
                 src->i_sar_num,  src->i_sar_den, 50000 );
    if (dst->i_sar_num <= 0 || dst->i_sar_den <= 0) {
        dst->i_sar_num = 1;
        dst->i_sar_den = 1;
    }
    video_format_FixRgb(dst);
    return VLC_SUCCESS;
}
static void VideoFormatCopyCropAr(video_format_t *dst,
                                  const video_format_t *src)
{
    video_format_CopyCrop(dst, src);
    dst->i_sar_num = src->i_sar_num;
    dst->i_sar_den = src->i_sar_den;
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

static vout_thread_t *VoutCreate(vlc_object_t *object,
                                 const vout_configuration_t *cfg,
                                 input_thread_t *input)
{
    video_format_t original;
    if (VoutValidateFormat(&original, cfg->fmt))
        return NULL;

    /* Allocate descriptor */
    vout_thread_t *vout = vlc_custom_create(object,
                                            sizeof(*vout) + sizeof(*vout->p),
                                            "video output");
    if (!vout) {
        video_format_Clean(&original);
        return NULL;
    }

    /* */
    vout->p = (vout_thread_sys_t*)&vout[1];

    vout->p->original = original;
    vout->p->dpb_size = cfg->dpb_size;
    vout->p->mouse_event = cfg->mouse_event;
    vout->p->opaque = cfg->opaque;
    vout->p->dead = false;
    vout->p->is_late_dropped = var_InheritBool(vout, "drop-late-frames");
    vout->p->pause.is_on = false;
    vout->p->pause.date = VLC_TICK_INVALID;

    vout_control_Init(&vout->p->control);
    vout_statistic_Init(&vout->p->statistic);
    vout->p->snapshot = vout_snapshot_New();
    vout_chrono_Init(&vout->p->render, 5, VLC_TICK_FROM_MS(10)); /* Arbitrary initial time */

    /* Initialize locks */
    vlc_mutex_init(&vout->p->filter.lock);
    vlc_mutex_init(&vout->p->spu_lock);
    vlc_mutex_init(&vout->p->window_lock);

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit(vout);

    /* Initialize subpicture unit */
    vout->p->spu = spu_Create(vout, vout);

    vout->p->title.show     = var_InheritBool(vout, "video-title-show");
    vout->p->title.timeout  = var_InheritInteger(vout, "video-title-timeout");
    vout->p->title.position = var_InheritInteger(vout, "video-title-position");

    /* Get splitter name if present */
    vout->p->splitter_name = var_InheritString(vout, "video-splitter");

    /* */
    vout_InitInterlacingSupport(vout, vout->p->displayed.is_interlaced);

    /* Window */
    if (vout->p->splitter_name == NULL) {
        vout_window_cfg_t wcfg = {
            .is_fullscreen = var_GetBool(vout, "fullscreen"),
            .is_decorated = var_InheritBool(vout, "video-deco"),
            // TODO: take pixel A/R, crop and zoom into account
#ifdef __APPLE__
            .x = var_InheritInteger(vout, "video-x"),
            .y = var_InheritInteger(vout, "video-y"),
#endif
            .width = cfg->fmt->i_visible_width,
            .height = cfg->fmt->i_visible_height,
        };

        vout_window_t *window = vout_display_window_New(vout, &wcfg);
        if (unlikely(window == NULL)) {
            spu_Destroy(vout->p->spu);
            vlc_object_release(vout);
            return NULL;
        }

        if (var_InheritBool(vout, "video-wallpaper"))
            vout_window_SetState(window, VOUT_WINDOW_STATE_BELOW);
        else if (var_InheritBool(vout, "video-on-top"))
            vout_window_SetState(window, VOUT_WINDOW_STATE_ABOVE);

        vout->p->window = window;
    } else
        vout->p->window = NULL;

    /* */
    vlc_object_set_destructor(vout, VoutDestructor);

    /* */
    if (vlc_clone(&vout->p->thread, Thread, vout,
                  VLC_THREAD_PRIORITY_OUTPUT)) {
        if (vout->p->window != NULL)
            vout_display_window_Delete(vout->p->window);
        spu_Destroy(vout->p->spu);
        vlc_object_release(vout);
        return NULL;
    }

    vout_control_WaitEmpty(&vout->p->control);

    if (vout->p->dead) {
        msg_Err(vout, "video output creation failed");
        vout_CloseAndRelease(vout);
        return NULL;
    }

    vout->p->input = input;
    if (vout->p->input)
        spu_Attach(vout->p->spu, input);

    return vout;
}

#undef vout_Request
vout_thread_t *vout_Request(vlc_object_t *object,
                            const vout_configuration_t *cfg,
                            input_thread_t *input)
{
    vout_thread_t *vout = cfg->vout;

    /* If a vout is provided, try reusing it */
    if (vout) {
        if (vout->p->input != input) {
            if (vout->p->input)
                spu_Detach(vout->p->spu);
            vout->p->input = input;
            if (vout->p->input)
                spu_Attach(vout->p->spu, vout->p->input);
        }

        vout_control_cmd_t cmd;
        vout_control_cmd_Init(&cmd, VOUT_CONTROL_REINIT);
        cmd.cfg = cfg;
        vout_control_Push(&vout->p->control, &cmd);
        vout_control_WaitEmpty(&vout->p->control);

        if (cfg->fmt)
            vout_IntfReinit(vout);

        if (!vout->p->dead) {
            msg_Dbg(object, "reusing provided vout");
            return vout;
        }
        vout_CloseAndRelease(vout);

        msg_Warn(object, "cannot reuse provided vout");
    }
    return VoutCreate(object, cfg, input);
}

void vout_Close(vout_thread_t *vout)
{
    assert(vout);

    if (vout->p->input)
        spu_Detach(vout->p->spu);

    vout_snapshot_End(vout->p->snapshot);

    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_CLEAN);
    vlc_join(vout->p->thread, NULL);

    vout_chrono_Clean(&vout->p->render);

    vlc_mutex_lock(&vout->p->window_lock);
    if (vout->p->window != NULL) {
        vout_display_window_Delete(vout->p->window);
        vout->p->window = NULL;
    }
    vlc_mutex_unlock(&vout->p->window_lock);

    vlc_mutex_lock(&vout->p->spu_lock);
    spu_Destroy(vout->p->spu);
    vout->p->spu = NULL;
    vlc_mutex_unlock(&vout->p->spu_lock);
}

/* */
static void VoutDestructor(vlc_object_t *object)
{
    vout_thread_t *vout = (vout_thread_t *)object;

    /* Make sure the vout was stopped first */
    //assert(!vout->p_module);

    free(vout->p->splitter_name);

    /* Destroy the locks */
    vlc_mutex_destroy(&vout->p->window_lock);
    vlc_mutex_destroy(&vout->p->spu_lock);
    vlc_mutex_destroy(&vout->p->filter.lock);
    vout_control_Clean(&vout->p->control);

    /* */
    vout_statistic_Clean(&vout->p->statistic);

    /* */
    vout_snapshot_Destroy(vout->p->snapshot);

    video_format_Clean(&vout->p->original);
}

/* */
void vout_Cancel(vout_thread_t *vout, bool canceled)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_CANCEL, canceled);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_ChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_PAUSE);
    cmd.pause.is_on = is_paused;
    cmd.pause.date  = date;
    vout_control_Push(&vout->p->control, &cmd);

    vout_control_WaitEmpty(&vout->p->control);
}

void vout_GetResetStatistic(vout_thread_t *vout, unsigned *restrict displayed,
                            unsigned *restrict lost)
{
    vout_statistic_GetReset( &vout->p->statistic, displayed, lost );
}

void vout_Flush(vout_thread_t *vout, vlc_tick_t date)
{
    vout_control_PushTime(&vout->p->control, VOUT_CONTROL_FLUSH, date);
    vout_control_WaitEmpty(&vout->p->control);
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (picture)
        picture_Release(picture);

    return !picture;
}

void vout_NextPicture(vout_thread_t *vout, vlc_tick_t *duration)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_STEP);
    cmd.time_ptr = duration;

    vout_control_Push(&vout->p->control, &cmd);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_DisplayTitle(vout_thread_t *vout, const char *title)
{
    assert(title);

    if (!vout->p->title.show)
        return;

    vout_OSDText(vout, VOUT_SPU_CHANNEL_OSD, vout->p->title.position,
                 VLC_TICK_FROM_MS(vout->p->title.timeout), title);
}

void vout_MouseState(vout_thread_t *vout, const vlc_mouse_t *mouse)
{
    assert(mouse);
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_MOUSE_STATE);
    cmd.mouse = *mouse;

    vout_control_Push(&vout->p->control, &cmd);
}

void vout_PutSubpicture( vout_thread_t *vout, subpicture_t *subpic )
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_SUBPICTURE);
    cmd.subpicture = subpic;

    vout_control_Push(&vout->p->control, &cmd);
}
int vout_RegisterSubpictureChannel( vout_thread_t *vout )
{
    int channel = VOUT_SPU_CHANNEL_AVAIL_FIRST;

    vlc_mutex_lock(&vout->p->spu_lock);
    if (vout->p->spu)
        channel = spu_RegisterChannel(vout->p->spu);
    vlc_mutex_unlock(&vout->p->spu_lock);

    return channel;
}
void vout_FlushSubpictureChannel( vout_thread_t *vout, int channel )
{
    vout_control_PushInteger(&vout->p->control, VOUT_CONTROL_FLUSH_SUBPICTURE,
                             channel);
}
void vout_SetSpuHighlight( vout_thread_t *vout,
                        const vlc_spu_highlight_t *spu_hl )
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (vout->p->spu)
        spu_SetHighlight(vout->p->spu, spu_hl);
    vlc_mutex_unlock(&vout->p->spu_lock);
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
    picture_t *picture = picture_pool_Wait(vout->p->decoder_pool);
    if (likely(picture != NULL)) {
        picture_Reset(picture);
        VideoFormatCopyCropAr(&picture->format, &vout->p->original);
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
    picture->p_next = NULL;
    if (picture_pool_OwnsPic(vout->p->decoder_pool, picture))
    {
        picture_fifo_Push(vout->p->decoder_fifo, picture);

        vout_control_Wake(&vout->p->control);
    }
    else
    {
        /* FIXME: HACK: Drop this picture because the vout changed. The old
         * picture pool need to be kept by the new vout. This requires a major
         * "vout display" API change. */
        picture_Release(picture);
    }
}

/* */
int vout_GetSnapshot(vout_thread_t *vout,
                     block_t **image_dst, picture_t **picture_dst,
                     video_format_t *fmt,
                     const char *type, vlc_tick_t timeout)
{
    picture_t *picture = vout_snapshot_Get(vout->p->snapshot, timeout);
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
                           picture, codec, override_width, override_height)) {
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

void vout_ChangeAspectRatio( vout_thread_t *p_vout,
                             unsigned int i_num, unsigned int i_den )
{
    vout_ControlChangeSampleAspectRatio( p_vout, i_num, i_den );
}

/* vout_Control* are usable by anyone at anytime */
void vout_ControlChangeFullscreen(vout_thread_t *vout, const char *id)
{
    vout_window_t *window;

    vlc_mutex_lock(&vout->p->window_lock);
    window = vout->p->window;
    /* Window is NULL if the output is a splitter,
     * or if the output was already closed by its owner.
     */
    if (window != NULL)
        vout_window_SetFullScreen(window, id);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ControlChangeWindowed(vout_thread_t *vout)
{
    vout_window_t *window;

    vlc_mutex_lock(&vout->p->window_lock);
    window = vout->p->window;
    if (window != NULL)
        vout_window_UnsetFullScreen(window);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ControlChangeWindowState(vout_thread_t *vout, unsigned st)
{
    vout_window_t *window;

    vlc_mutex_lock(&vout->p->window_lock);
    window = vout->p->window;
    if (window != NULL)
        vout_window_SetState(window, st);
    vlc_mutex_unlock(&vout->p->window_lock);
}

static void vout_ControlUpdateWindowSize(vout_thread_t *vout)
{
    vlc_mutex_lock(&vout->p->window_lock);
    if (vout->p->window != NULL)
        vout_display_window_UpdateSize(vout->p->window, &vout->p->original);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ControlChangeDisplaySize(vout_thread_t *vout,
                                   unsigned width, unsigned height)
{
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_DISPLAY_SIZE);
    cmd.window.x      = 0;
    cmd.window.y      = 0;
    cmd.window.width  = width;
    cmd.window.height = height;
    vout_control_Push(&vout->p->control, &cmd);
}
void vout_ControlChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_DISPLAY_FILLED,
                          is_filled);
}

void vout_ControlChangeZoom(vout_thread_t *vout, int num, int den)
{
    if (num * 10 < den) {
        num = den;
        den *= 10;
    } else if (num > den * 10) {
        num = den * 10;
    }

    vout_ControlUpdateWindowSize(vout);
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ZOOM,
                          num, den);
}

void vout_ControlChangeSampleAspectRatio(vout_thread_t *vout,
                                         unsigned num, unsigned den)
{
    vout_ControlUpdateWindowSize(vout);
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ASPECT_RATIO,
                          num, den);
}

void vout_ControlChangeCropRatio(vout_thread_t *vout,
                                 unsigned num, unsigned den)
{
    vout_ControlUpdateWindowSize(vout);
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_CROP_RATIO,
                          num, den);
}

void vout_ControlChangeCropWindow(vout_thread_t *vout,
                                  int x, int y, int width, int height)
{
    vout_control_cmd_t cmd;

    vout_ControlUpdateWindowSize(vout);

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_WINDOW);
    cmd.window.x      = __MAX(x, 0);
    cmd.window.y      = __MAX(y, 0);
    cmd.window.width  = __MAX(width, 0);
    cmd.window.height = __MAX(height, 0);
    vout_control_Push(&vout->p->control, &cmd);
}

void vout_ControlChangeCropBorder(vout_thread_t *vout,
                                  int left, int top, int right, int bottom)
{
    vout_control_cmd_t cmd;

    vout_ControlUpdateWindowSize(vout);

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_BORDER);
    cmd.border.left   = __MAX(left, 0);
    cmd.border.top    = __MAX(top, 0);
    cmd.border.right  = __MAX(right, 0);
    cmd.border.bottom = __MAX(bottom, 0);
    vout_control_Push(&vout->p->control, &cmd);
}

void vout_ControlChangeFilters(vout_thread_t *vout, const char *filters)
{
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_FILTERS,
                            filters);
}

void vout_ControlChangeSubSources(vout_thread_t *vout, const char *filters)
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (likely(vout->p->spu != NULL))
        spu_ChangeSources(vout->p->spu, filters);
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_ControlChangeSubFilters(vout_thread_t *vout, const char *filters)
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (likely(vout->p->spu != NULL))
        spu_ChangeFilters(vout->p->spu, filters);
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_ControlChangeSubMargin(vout_thread_t *vout, int margin)
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (likely(vout->p->spu != NULL))
        spu_ChangeMargin(vout->p->spu, margin);
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_ControlChangeViewpoint(vout_thread_t *vout,
                                 const vlc_viewpoint_t *p_viewpoint)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_VIEWPOINT);
    cmd.viewpoint = *p_viewpoint;
    vout_control_Push(&vout->p->control, &cmd);
}

/* */
static void VoutGetDisplayCfg(vout_thread_t *vout, vout_display_cfg_t *cfg)
{
    /* Load configuration */
    cfg->window = vout->p->window;
#if defined(_WIN32) || defined(__OS2__)
    cfg->is_fullscreen = var_GetBool(vout, "fullscreen")
                         || var_GetBool(vout, "video-wallpaper");
#endif
    cfg->viewpoint = vout->p->original.pose;

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
    filter_DelProxyCallbacks((vlc_object_t *)opaque, filter,
                             FilterRestartCallback);
    return VLC_SUCCESS;
}

static void ThreadDelAllFilterCallbacks(vout_thread_t *vout)
{
    assert(vout->p->filter.chain_interactive != NULL);
    filter_chain_ForEach(vout->p->filter.chain_interactive,
                         ThreadDelFilterCallbacks, vout);
}

static picture_t *VoutVideoFilterInteractiveNewPicture(filter_t *filter)
{
    vout_thread_t *vout = filter->owner.sys;

    picture_t *picture = picture_pool_Get(vout->p->private_pool);
    if (picture) {
        picture_Reset(picture);
        VideoFormatCopyCropAr(&picture->format, &filter->fmt_out.video);
    }
    return picture;
}

static picture_t *VoutVideoFilterStaticNewPicture(filter_t *filter)
{
    vout_thread_t *vout = filter->owner.sys;

    vlc_mutex_assert(&vout->p->filter.lock);
    if (filter_chain_IsEmpty(vout->p->filter.chain_interactive))
        return VoutVideoFilterInteractiveNewPicture(filter);

    return picture_NewFromFormat(&filter->fmt_out.video);
}

static void ThreadFilterFlush(vout_thread_t *vout, bool is_locked)
{
    if (vout->p->displayed.current)
        picture_Release( vout->p->displayed.current );
    vout->p->displayed.current = NULL;

    if (vout->p->displayed.next)
        picture_Release( vout->p->displayed.next );
    vout->p->displayed.next = NULL;

    if (!is_locked)
        vlc_mutex_lock(&vout->p->filter.lock);
    filter_chain_VideoFlush(vout->p->filter.chain_static);
    filter_chain_VideoFlush(vout->p->filter.chain_interactive);
    if (!is_locked)
        vlc_mutex_unlock(&vout->p->filter.lock);
}

typedef struct {
    char           *name;
    config_chain_t *cfg;
} vout_filter_t;

static void ThreadChangeFilters(vout_thread_t *vout,
                                const video_format_t *source,
                                const char *filters,
                                int deinterlace,
                                bool is_locked)
{
    ThreadFilterFlush(vout, is_locked);
    ThreadDelAllFilterCallbacks(vout);

    vlc_array_t array_static;
    vlc_array_t array_interactive;

    vlc_array_init(&array_static);
    vlc_array_init(&array_interactive);

    vout->p->filter.has_deint =
         deinterlace == 1 || (deinterlace == -1 && vout->p->filter.has_deint);

    if (vout->p->filter.has_deint)
    {
        vout_filter_t *e = malloc(sizeof(*e));

        if (likely(e))
        {
            free(config_ChainCreate(&e->name, &e->cfg, "deinterlace"));
            vlc_array_append_or_abort(&array_static, e);
        }
    }

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
        vlc_mutex_lock(&vout->p->filter.lock);

    es_format_t fmt_target;
    es_format_InitFromVideo(&fmt_target, source ? source : &vout->p->filter.format);

    const es_format_t *p_fmt_current = &fmt_target;

    for (int a = 0; a < 2; a++) {
        vlc_array_t    *array = a == 0 ? &array_static :
                                         &array_interactive;
        filter_chain_t *chain = a == 0 ? vout->p->filter.chain_static :
                                         vout->p->filter.chain_interactive;

        filter_chain_Reset(chain, p_fmt_current, p_fmt_current);
        for (size_t i = 0; i < vlc_array_count(array); i++) {
            vout_filter_t *e = vlc_array_item_at_index(array, i);
            msg_Dbg(vout, "Adding '%s' as %s", e->name, a == 0 ? "static" : "interactive");
            filter_t *filter = filter_chain_AppendFilter(chain, e->name, e->cfg,
                               NULL, NULL);
            if (!filter)
            {
                msg_Err(vout, "Failed to add filter '%s'", e->name);
                config_ChainDestroy(e->cfg);
            }
            else if (a == 1) /* Add callbacks for interactive filters */
                filter_AddProxyCallbacks(vout, filter, FilterRestartCallback);

            free(e->name);
            free(e);
        }
        p_fmt_current = filter_chain_GetFmtOut(chain);
        vlc_array_clear(array);
    }

    if (!es_format_IsSimilar(p_fmt_current, &fmt_target)) {
        msg_Dbg(vout, "Adding a filter to compensate for format changes");
        if (filter_chain_AppendConverter(vout->p->filter.chain_interactive,
                                         p_fmt_current, &fmt_target) != 0) {
            msg_Err(vout, "Failed to compensate for the format changes, removing all filters");
            ThreadDelAllFilterCallbacks(vout);
            filter_chain_Reset(vout->p->filter.chain_static,      &fmt_target, &fmt_target);
            filter_chain_Reset(vout->p->filter.chain_interactive, &fmt_target, &fmt_target);
        }
    }

    es_format_Clean(&fmt_target);

    if (vout->p->filter.configuration != filters) {
        free(vout->p->filter.configuration);
        vout->p->filter.configuration = filters ? strdup(filters) : NULL;
    }
    if (source) {
        video_format_Clean(&vout->p->filter.format);
        video_format_Copy(&vout->p->filter.format, source);
    }

    if (!is_locked)
        vlc_mutex_unlock(&vout->p->filter.lock);
}


/* */
static int ThreadDisplayPreparePicture(vout_thread_t *vout, bool reuse, bool frame_by_frame)
{
    bool is_late_dropped = vout->p->is_late_dropped && !vout->p->pause.is_on && !frame_by_frame;

    vlc_mutex_lock(&vout->p->filter.lock);

    picture_t *picture = filter_chain_VideoFilter(vout->p->filter.chain_static, NULL);
    assert(!reuse || !picture);

    while (!picture) {
        picture_t *decoded;
        if (reuse && vout->p->displayed.decoded) {
            decoded = picture_Hold(vout->p->displayed.decoded);
        } else {
            decoded = picture_fifo_Pop(vout->p->decoder_fifo);
            if (decoded) {
                if (is_late_dropped && !decoded->b_force) {
                    vlc_tick_t late_threshold;
                    if (decoded->format.i_frame_rate && decoded->format.i_frame_rate_base)
                        late_threshold = VLC_TICK_FROM_MS(500) * decoded->format.i_frame_rate_base / decoded->format.i_frame_rate;
                    else
                        late_threshold = VOUT_DISPLAY_LATE_THRESHOLD;
                    const vlc_tick_t predicted = vlc_tick_now() + 0; /* TODO improve */
                    const vlc_tick_t late = predicted - decoded->date;
                    if (late > late_threshold) {
                        msg_Warn(vout, "picture is too late to be displayed (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
                        picture_Release(decoded);
                        vout_statistic_AddLost(&vout->p->statistic, 1);
                        continue;
                    } else if (late > 0) {
                        msg_Dbg(vout, "picture might be displayed late (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
                    }
                }
                if (!VideoFormatIsCropArEqual(&decoded->format, &vout->p->filter.format))
                    ThreadChangeFilters(vout, &decoded->format, vout->p->filter.configuration, -1, true);
            }
        }

        if (!decoded)
            break;
        reuse = false;

        if (vout->p->displayed.decoded)
            picture_Release(vout->p->displayed.decoded);

        vout->p->displayed.decoded       = picture_Hold(decoded);
        vout->p->displayed.timestamp     = decoded->date;
        vout->p->displayed.is_interlaced = !decoded->b_progressive;

        picture = filter_chain_VideoFilter(vout->p->filter.chain_static, decoded);
    }

    vlc_mutex_unlock(&vout->p->filter.lock);

    if (!picture)
        return VLC_EGENERIC;

    assert(!vout->p->displayed.next);
    if (!vout->p->displayed.current)
        vout->p->displayed.current = picture;
    else
        vout->p->displayed.next    = picture;
    return VLC_SUCCESS;
}

static picture_t *ConvertRGB32AndBlendBufferNew(filter_t *filter)
{
    return picture_NewFromFormat(&filter->fmt_out.video);
}

static picture_t *ConvertRGB32AndBlend(vout_thread_t *vout, picture_t *pic,
                                     subpicture_t *subpic)
{
    /* This function will convert the pic to RGB32 and blend the subpic to it.
     * The returned pic can't be used to display since the chroma will be
     * different than the "vout display" one, but it can be used for snapshots.
     * */

    assert(vout->p->spu_blend);

    static const struct filter_video_callbacks cbs = {
        .buffer_new = ConvertRGB32AndBlendBufferNew,
    };
    filter_owner_t owner = {
        .video = &cbs,
    };
    filter_chain_t *filterc = filter_chain_NewVideo(vout, false, &owner);
    if (!filterc)
        return NULL;

    es_format_t src = vout->p->spu_blend->fmt_out;
    es_format_t dst = src;
    dst.video.i_chroma = VLC_CODEC_RGB32;
    video_format_FixRgb(&dst.video);

    if (filter_chain_AppendConverter(filterc, &src, &dst) != 0)
    {
        filter_chain_Delete(filterc);
        return NULL;
    }

    picture_Hold(pic);
    pic = filter_chain_VideoFilter(filterc, pic);
    filter_chain_Delete(filterc);

    if (pic)
    {
        filter_t *swblend = filter_NewBlend(VLC_OBJECT(vout), &dst.video);
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

static int ThreadDisplayRenderPicture(vout_thread_t *vout, bool is_forced)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;

    picture_t *torender = picture_Hold(sys->displayed.current);

    vout_chrono_Start(&sys->render);

    vlc_mutex_lock(&sys->filter.lock);
    picture_t *filtered = filter_chain_VideoFilter(sys->filter.chain_interactive, torender);
    vlc_mutex_unlock(&sys->filter.lock);

    if (!filtered)
        return VLC_EGENERIC;

    if (filtered->date != sys->displayed.current->date)
        msg_Warn(vout, "Unsupported timestamp modifications done by chain_interactive");

    /*
     * Get the subpicture to be displayed
     */
    const bool do_snapshot = vout_snapshot_IsRequested(sys->snapshot);
    vlc_tick_t render_subtitle_date;
    if (sys->pause.is_on)
        render_subtitle_date = sys->pause.date;
    else
        render_subtitle_date = filtered->date > 1 ? filtered->date : vlc_tick_now();
    vlc_tick_t render_osd_date = vlc_tick_now(); /* FIXME wrong */

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
                               vd->source.orientation == ORIENT_NORMAL &&
                              (vd->info.is_slow ||
                               do_snapshot ||
                               vd->fmt.i_width * vd->fmt.i_height <= vd->source.i_width * vd->source.i_height);

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
            sys->spu_blend = filter_NewBlend(VLC_OBJECT(vout), &fmt_spu);
            if (!sys->spu_blend)
                msg_Err(vout, "Failed to create blending filter, OSD/Subtitles will not work");
        }
    }

    video_format_t fmt_spu_rot;
    video_format_ApplyRotation(&fmt_spu_rot, &fmt_spu);
    subpicture_t *subpic = spu_Render(sys->spu,
                                      subpicture_chromas, &fmt_spu_rot,
                                      &vd->source,
                                      render_subtitle_date, render_osd_date,
                                      do_snapshot,
                                      vd->info.can_scale_spu);
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
            picture_t *blent = picture_pool_Get(sys->private_pool);
            if (blent) {
                VideoFormatCopyCropAr(&blent->format, &filtered->format);
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

    if (sys->display.use_copy) {
        picture_t *direct = NULL;
        if (likely(sys->display_pool != NULL))
            direct = picture_pool_Get(sys->display_pool);
        if (!direct) {
            picture_Release(todisplay);
            if (subpic)
                subpicture_Delete(subpic);
            return VLC_EGENERIC;
        }

        /* The display uses direct rendering (no conversion), but its pool of
         * pictures is not usable by the decoder (too few, too slow or
         * subject to invalidation...). Since there are no filters, copying
         * pictures from the decoder to the output is unavoidable. */
        VideoFormatCopyCropAr(&direct->format, &todisplay->format);
        picture_Copy(direct, todisplay);
        picture_Release(todisplay);
        snap_pic = todisplay = direct;
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

    todisplay = vout_FilterDisplay(vd, todisplay);
    if (todisplay == NULL) {
        if (subpic != NULL)
            subpicture_Delete(subpic);
        return VLC_EGENERIC;
    }

    if (!do_dr_spu && sys->spu_blend != NULL && subpic != NULL)
        picture_BlendSubpicture(todisplay, sys->spu_blend, subpic);

    vout_display_Prepare(vd, todisplay, do_dr_spu ? subpic : NULL,
                         todisplay->date);

    vout_chrono_Stop(&sys->render);
#if 0
        {
        static int i = 0;
        if (((i++)%10) == 0)
            msg_Info(vout, "render: avg %d ms var %d ms",
                     (int)(sys->render.avg/1000), (int)(sys->render.var/1000));
        }
#endif

    /* Wait the real date (for rendering jitter) */
#if 0
    vlc_tick_t delay = todisplay->date - vlc_tick_now();
    if (delay < 1000)
        msg_Warn(vout, "picture is late (%lld ms)", delay / 1000);
#endif
    if (!is_forced)
        vlc_tick_wait(todisplay->date);

    /* Display the direct buffer returned by vout_RenderPicture */
    sys->displayed.date = vlc_tick_now();
    vout_display_Display(vd, todisplay);
    if (subpic)
        subpicture_Delete(subpic);

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    return VLC_SUCCESS;
}

static int ThreadDisplayPicture(vout_thread_t *vout, vlc_tick_t *deadline)
{
    vout_thread_sys_t *sys = vout->p;
    bool frame_by_frame = !deadline;
    bool paused = sys->pause.is_on;
    bool first = !sys->displayed.current;

    if (first)
        if (ThreadDisplayPreparePicture(vout, true, frame_by_frame)) /* FIXME not sure it is ok */
            return VLC_EGENERIC;

    if (!paused || frame_by_frame)
        while (!sys->displayed.next && !ThreadDisplayPreparePicture(vout, false, frame_by_frame))
            ;

    const vlc_tick_t date = vlc_tick_now();
    const vlc_tick_t render_delay = vout_chrono_GetHigh(&sys->render) + VOUT_MWAIT_TOLERANCE;

    bool drop_next_frame = frame_by_frame;
    vlc_tick_t date_next = VLC_TICK_INVALID;
    if (!paused && sys->displayed.next) {
        date_next = sys->displayed.next->date - render_delay;
        if (date_next /* + 0 FIXME */ <= date)
            drop_next_frame = true;
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
        refresh = date_refresh <= date;
    }
    bool force_refresh = !drop_next_frame && refresh;

    if (!first && !refresh && !drop_next_frame) {
        if (!frame_by_frame) {
            if (date_refresh != VLC_TICK_INVALID)
                *deadline = date_refresh;
            if (date_next != VLC_TICK_INVALID && date_next < *deadline)
                *deadline = date_next;
        }
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

static void ThreadDisplaySubpicture(vout_thread_t *vout,
                                    subpicture_t *subpicture)
{
    spu_PutSubpicture(vout->p->spu, subpicture);
}

static void ThreadFlushSubpicture(vout_thread_t *vout, int channel)
{
    spu_ClearChannel(vout->p->spu, channel);
}

static void ThreadChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    assert(!vout->p->pause.is_on || !is_paused);

    if (vout->p->pause.is_on) {
        const vlc_tick_t duration = date - vout->p->pause.date;

        if (vout->p->step.timestamp != VLC_TICK_INVALID)
            vout->p->step.timestamp += duration;
        if (vout->p->step.last != VLC_TICK_INVALID)
            vout->p->step.last += duration;
        picture_fifo_OffsetDate(vout->p->decoder_fifo, duration);
        if (vout->p->displayed.decoded)
            vout->p->displayed.decoded->date += duration;
        spu_OffsetSubtitleDate(vout->p->spu, duration);

        ThreadFilterFlush(vout, false);
    } else {
        vout->p->step.timestamp = VLC_TICK_INVALID;
        vout->p->step.last      = VLC_TICK_INVALID;
    }
    vout->p->pause.is_on = is_paused;
    vout->p->pause.date  = date;

    vout_window_t *window = vout->p->window;
    if (window != NULL)
        vout_window_SetInhibition(window, !is_paused);
}

static void ThreadFlush(vout_thread_t *vout, bool below, vlc_tick_t date)
{
    vout->p->step.timestamp = VLC_TICK_INVALID;
    vout->p->step.last      = VLC_TICK_INVALID;

    ThreadFilterFlush(vout, false); /* FIXME too much */

    picture_t *last = vout->p->displayed.decoded;
    if (last) {
        if ((date == VLC_TICK_INVALID) ||
            ( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            vout->p->displayed.decoded   = NULL;
            vout->p->displayed.date      = VLC_TICK_INVALID;
            vout->p->displayed.timestamp = VLC_TICK_INVALID;
        }
    }

    picture_fifo_Flush(vout->p->decoder_fifo, date, below);
    vout_FilterFlush(vout->p->display.vd);
}

static void ThreadStep(vout_thread_t *vout, vlc_tick_t *duration)
{
    *duration = 0;

    if (vout->p->step.last == VLC_TICK_INVALID)
        vout->p->step.last = vout->p->displayed.timestamp;

    if (ThreadDisplayPicture(vout, NULL))
        return;

    vout->p->step.timestamp = vout->p->displayed.timestamp;

    if (vout->p->step.last != VLC_TICK_INVALID &&
        vout->p->step.timestamp > vout->p->step.last) {
        *duration = vout->p->step.timestamp - vout->p->step.last;
        vout->p->step.last = vout->p->step.timestamp;
        /* TODO advance subpicture by the duration ... */
    }
}

static void ThreadTranslateMouseState(vout_thread_t *vout,
                                      const vlc_mouse_t *win_mouse)
{
    vout_display_t *vd = vout->p->display.vd;
    vlc_mouse_t vid_mouse;
    vout_display_place_t place;

    /* Translate window coordinates to video coordinates */
    vout_display_PlacePicture(&place, &vd->source, vd->cfg);

    if (place.width <= 0 || place.height <= 0)
        return;

    const int x = vd->source.i_x_offset
        + (int64_t)(win_mouse->i_x - place.x)
          * vd->source.i_visible_width / place.width;
    const int y = vd->source.i_y_offset
        + (int64_t)(win_mouse->i_y - place.y)
          * vd->source.i_visible_height / place.height;

    vid_mouse = *win_mouse;
    vlc_mouse_SetPosition(&vid_mouse, x, y);

    /* Then pass up the filter chains. */
    vout_SendDisplayEventMouse(vout, &vid_mouse);
}

static int ThreadStart(vout_thread_t *vout, vout_display_cfg_t *cfg)
{
    vlc_mouse_Init(&vout->p->mouse);
    vout->p->decoder_fifo = picture_fifo_New();
    vout->p->decoder_pool = NULL;
    vout->p->display_pool = NULL;
    vout->p->private_pool = NULL;

    vout->p->filter.configuration = NULL;
    video_format_Copy(&vout->p->filter.format, &vout->p->original);

    static const struct filter_video_callbacks static_cbs = {
        .buffer_new = VoutVideoFilterStaticNewPicture,
    };
    static const struct filter_video_callbacks interactive_cbs = {
        .buffer_new = VoutVideoFilterInteractiveNewPicture,
    };
    filter_owner_t owner = {
        .video = &static_cbs,
        .sys = vout,
    };
    vout->p->filter.chain_static =
        filter_chain_NewVideo( vout, true, &owner );

    owner.video = &interactive_cbs;
    vout->p->filter.chain_interactive =
        filter_chain_NewVideo( vout, true, &owner );

    vout_display_cfg_t cfg_default;
    if (cfg == NULL) {
        VoutGetDisplayCfg(vout, &cfg_default);
        cfg = &cfg_default;
    }

    if (vout_OpenWrapper(vout, vout->p->splitter_name, cfg))
        goto error;
    assert(vout->p->decoder_pool && vout->p->private_pool);

    vout->p->displayed.current       = NULL;
    vout->p->displayed.next          = NULL;
    vout->p->displayed.decoded       = NULL;
    vout->p->displayed.date          = VLC_TICK_INVALID;
    vout->p->displayed.timestamp     = VLC_TICK_INVALID;
    vout->p->displayed.is_interlaced = false;

    vout->p->step.last               = VLC_TICK_INVALID;
    vout->p->step.timestamp          = VLC_TICK_INVALID;

    vout->p->spu_blend_chroma        = 0;
    vout->p->spu_blend               = NULL;

    video_format_Print(VLC_OBJECT(vout), "original format", &vout->p->original);
    return VLC_SUCCESS;
error:
    if (vout->p->filter.chain_interactive != NULL)
    {
        ThreadDelAllFilterCallbacks(vout);
        filter_chain_Delete(vout->p->filter.chain_interactive);
    }
    if (vout->p->filter.chain_static != NULL)
        filter_chain_Delete(vout->p->filter.chain_static);
    video_format_Clean(&vout->p->filter.format);
    if (vout->p->decoder_fifo != NULL)
        picture_fifo_Delete(vout->p->decoder_fifo);
    return VLC_EGENERIC;
}

static void ThreadStop(vout_thread_t *vout, vout_display_cfg_t *cfg)
{
    if (vout->p->spu_blend)
        filter_DeleteBlend(vout->p->spu_blend);

    /* Destroy translation tables */
    if (vout->p->display.vd) {
        if (vout->p->decoder_pool)
            ThreadFlush(vout, true, INT64_MAX);
        vout_CloseWrapper(vout, cfg);
    }

    /* Destroy the video filters */
    ThreadDelAllFilterCallbacks(vout);
    filter_chain_Delete(vout->p->filter.chain_interactive);
    filter_chain_Delete(vout->p->filter.chain_static);
    video_format_Clean(&vout->p->filter.format);
    free(vout->p->filter.configuration);

    if (vout->p->decoder_fifo)
        picture_fifo_Delete(vout->p->decoder_fifo);
    assert(!vout->p->decoder_pool);

    if (vout->p->mouse_event)
        vout->p->mouse_event(NULL, vout->p->opaque);
}

static int ThreadReinit(vout_thread_t *vout,
                        const vout_configuration_t *cfg)
{
    video_format_t original;

    if (!cfg->fmt)
    {
        vout->p->mouse_event = NULL;
        vout->p->opaque = NULL;
        return VLC_SUCCESS;
    }

    vout->p->mouse_event = cfg->mouse_event;
    vout->p->opaque = cfg->opaque;

    vout->p->pause.is_on = false;
    vout->p->pause.date  = VLC_TICK_INVALID;

    if (VoutValidateFormat(&original, cfg->fmt)) {
        ThreadStop(vout, NULL);
        return VLC_EGENERIC;
    }

    /* We ignore ar changes at this point, they are dynamically supported.
     * #19268: don't ignore crop changes (fix vouts using the crop size of the
     * previous format). */
    vout->p->original.i_sar_num = original.i_sar_num;
    vout->p->original.i_sar_den = original.i_sar_den;
    if (video_format_IsSimilar(&original, &vout->p->original)) {
        if (cfg->dpb_size <= vout->p->dpb_size) {
            video_format_Clean(&original);
            return VLC_SUCCESS;
        }
        msg_Warn(vout, "DPB need to be increased");
    }

    vout_display_cfg_t dcfg = { };

    ThreadStop(vout, &dcfg);

    vout_ReinitInterlacingSupport(vout);

#if defined(_WIN32) || defined(__OS2__)
    if (!dcfg.is_fullscreen)
#endif
    {
        dcfg.display.width  = 0;
        dcfg.display.height = 0;
    }

    /* FIXME current vout "variables" are not in sync here anymore
     * and I am not sure what to do */
    if (dcfg.display.sar.num <= 0 || dcfg.display.sar.den <= 0) {
        dcfg.display.sar.num = 1;
        dcfg.display.sar.den = 1;
    }
    if (dcfg.zoom.num == 0 || dcfg.zoom.den == 0) {
        dcfg.zoom.num = 1;
        dcfg.zoom.den = 1;
    }

    vout->p->original = original;
    vout->p->dpb_size = cfg->dpb_size;
    if (ThreadStart(vout, &dcfg))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void ThreadCancel(vout_thread_t *vout, bool canceled)
{
    picture_pool_Cancel(vout->p->decoder_pool, canceled);
}

static int ThreadControl(vout_thread_t *vout, vout_control_cmd_t cmd)
{
    switch(cmd.type) {
    case VOUT_CONTROL_CLEAN:
        ThreadStop(vout, NULL);
        return 1;
    case VOUT_CONTROL_REINIT:
        if (ThreadReinit(vout, cmd.cfg))
            return 1;
        break;
    case VOUT_CONTROL_CANCEL:
        ThreadCancel(vout, cmd.boolean);
        break;
    case VOUT_CONTROL_SUBPICTURE:
        ThreadDisplaySubpicture(vout, cmd.subpicture);
        cmd.subpicture = NULL;
        break;
    case VOUT_CONTROL_FLUSH_SUBPICTURE:
        ThreadFlushSubpicture(vout, cmd.integer);
        break;
    case VOUT_CONTROL_CHANGE_FILTERS:
        ThreadChangeFilters(vout, NULL,
                            cmd.string != NULL ?
                            cmd.string : vout->p->filter.configuration,
                            -1, false);
        break;
    case VOUT_CONTROL_CHANGE_INTERLACE:
        ThreadChangeFilters(vout, NULL, vout->p->filter.configuration,
                            cmd.boolean ? 1 : 0, false);
        break;
    case VOUT_CONTROL_PAUSE:
        ThreadChangePause(vout, cmd.pause.is_on, cmd.pause.date);
        break;
    case VOUT_CONTROL_FLUSH:
        ThreadFlush(vout, false, cmd.time);
        break;
    case VOUT_CONTROL_STEP:
        ThreadStep(vout, cmd.time_ptr);
        break;
    case VOUT_CONTROL_MOUSE_STATE:
        ThreadTranslateMouseState(vout, &cmd.mouse);
        break;
    case VOUT_CONTROL_DISPLAY_SIZE:
        vout_SetDisplaySize(vout->p->display.vd,
                            cmd.window.width, cmd.window.height);
        break;
    case VOUT_CONTROL_DISPLAY_FILLED:
        vout_SetDisplayFilled(vout->p->display.vd, cmd.boolean);
        break;
    case VOUT_CONTROL_ZOOM:
        vout_SetDisplayZoom(vout->p->display.vd, cmd.pair.a, cmd.pair.b);
        break;
    case VOUT_CONTROL_ASPECT_RATIO:
        vout_SetDisplayAspect(vout->p->display.vd, cmd.pair.a, cmd.pair.b);
        break;
    case VOUT_CONTROL_CROP_RATIO:
        vout_SetDisplayCrop(vout->p->display.vd, cmd.pair.a, cmd.pair.b,
                            0, 0, 0, 0);
        break;
    case VOUT_CONTROL_CROP_WINDOW:
        vout_SetDisplayCrop(vout->p->display.vd, 0, 0,
                            cmd.window.x, cmd.window.y,
                            cmd.window.width, cmd.window.height);
        break;
    case VOUT_CONTROL_CROP_BORDER:
        vout_SetDisplayCrop(vout->p->display.vd, 0, 0,
                            cmd.border.left, cmd.border.top,
                            -(int)cmd.border.right, -(int)cmd.border.bottom);
        break;
    case VOUT_CONTROL_VIEWPOINT:
        vout_SetDisplayViewpoint(vout->p->display.vd, &cmd.viewpoint);
        break;
    default:
        break;
    }
    vout_control_cmd_Clean(&cmd);
    return 0;
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
    vout_thread_t *vout = object;
    vout_thread_sys_t *sys = vout->p;

    vlc_tick_t deadline = VLC_TICK_INVALID;
    bool wait = false;

    if (ThreadStart(vout, NULL))
        goto out;

    for (;;) {
        vout_control_cmd_t cmd;

        if (wait)
        {
            const vlc_tick_t max_deadline = vlc_tick_now() + VLC_TICK_FROM_MS(100);
            deadline = deadline == VLC_TICK_INVALID ? max_deadline : __MIN(deadline, max_deadline);
        } else {
            deadline = VLC_TICK_INVALID;
        }
        while (!vout_control_Pop(&sys->control, &cmd, deadline))
            if (ThreadControl(vout, cmd))
                goto out;

        deadline = VLC_TICK_INVALID;
        wait = ThreadDisplayPicture(vout, &deadline) != VLC_SUCCESS;

        const bool picture_interlaced = sys->displayed.is_interlaced;

        vout_SetInterlacingState(vout, picture_interlaced);
        vout_ManageWrapper(vout);
    }

out:
    vout->p->dead = true;
    vout_control_Dead(&vout->p->control);
    return NULL;
}
