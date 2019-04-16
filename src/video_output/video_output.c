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

#include <libvlc.h>
#include "vout_internal.h"
#include "interlacing.h"
#include "display.h"
#include "snapshot.h"
#include "window.h"
#include "../misc/variables.h"
#include "../clock/clock.h"

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
    vlc_ureduce( &dst->i_sar_num, &dst->i_sar_den,
                 src->i_sar_num,  src->i_sar_den, 50000 );
    if (dst->i_sar_num <= 0 || dst->i_sar_den <= 0) {
        dst->i_sar_num = 1;
        dst->i_sar_den = 1;
    }
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

static void vout_SizeWindow(vout_thread_t *vout, unsigned *restrict width,
                            unsigned *restrict height)
{
    vout_thread_sys_t *sys = vout->p;
    unsigned w = sys->original.i_visible_width;
    unsigned h = sys->original.i_visible_height;
    unsigned sar_num = sys->original.i_sar_num;
    unsigned sar_den = sys->original.i_sar_num;

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
                            sys->original.orientation,
                            &sys->display_cfg);
}

static void vout_UpdateWindowSize(vout_thread_t *vout)
{
    unsigned width, height;

    vlc_mutex_assert(&vout->p->window_lock);

    vout_SizeWindow(vout, &width, &height);
    msg_Dbg(vout, "requested window size: %ux%u", width, height);
    vout_window_SetSize(vout->p->display_cfg.window, width, height);
}

/* */
void vout_GetResetStatistic(vout_thread_t *vout, unsigned *restrict displayed,
                            unsigned *restrict lost)
{
    vout_statistic_GetReset( &vout->p->statistic, displayed, lost );
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (picture)
        picture_Release(picture);

    return !picture;
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
    vout_thread_sys_t *sys = vout->p;

    vlc_mutex_lock(&sys->spu_lock);
    if (sys->spu != NULL)
        spu_PutSubpicture(sys->spu, subpic);
    else
        subpicture_Delete(subpic);
    vlc_mutex_unlock(&sys->spu_lock);
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

void vout_SetSubpictureClock( vout_thread_t *vout, vlc_clock_t *clock )
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (vout->p->spu)
        spu_clock_Set(vout->p->spu, clock);
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_FlushSubpictureChannel( vout_thread_t *vout, int channel )
{
    vout_thread_sys_t *sys = vout->p;

    vlc_mutex_lock(&sys->spu_lock);
    if (sys->spu != NULL)
        spu_ClearChannel(vout->p->spu, channel);
    vlc_mutex_unlock(&sys->spu_lock);
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
        video_format_CopyCropAr(&picture->format, &vout->p->original);
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
    picture_fifo_Push(vout->p->decoder_fifo, picture);
    vout_control_Wake(&vout->p->control);
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

/* vout_Control* are usable by anyone at anytime */
void vout_ChangeFullscreen(vout_thread_t *vout, const char *id)
{
    vlc_mutex_lock(&vout->p->window_lock);
    vout_window_SetFullScreen(vout->p->display_cfg.window, id);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ChangeWindowed(vout_thread_t *vout)
{
    vlc_mutex_lock(&vout->p->window_lock);
    vout_window_UnsetFullScreen(vout->p->display_cfg.window);
    /* Attempt to reset the intended window size */
    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ChangeWindowState(vout_thread_t *vout, unsigned st)
{
    vlc_mutex_lock(&vout->p->window_lock);
    vout_window_SetState(vout->p->display_cfg.window, st);
    vlc_mutex_unlock(&vout->p->window_lock);
}

void vout_ChangeDisplaySize(vout_thread_t *vout,
                            unsigned width, unsigned height)
{
    /* DO NOT call this outside the vout window callbacks */
    vout_control_cmd_t cmd;

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_DISPLAY_SIZE);
    cmd.window.x      = 0;
    cmd.window.y      = 0;
    cmd.window.width  = width;
    cmd.window.height = height;
    vout_control_Push(&vout->p->control, &cmd);
}

void vout_ChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_thread_sys_t *sys = vout->p;

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.is_display_filled = is_filled;
    /* no window size update here */
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_DISPLAY_FILLED,
                          is_filled);
}

void vout_ChangeZoom(vout_thread_t *vout, unsigned num, unsigned den)
{
    vout_thread_sys_t *sys = vout->p;

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

    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ZOOM,
                          num, den);
}

void vout_ChangeSampleAspectRatio(vout_thread_t *vout,
                                  unsigned num, unsigned den)
{
    vout_thread_sys_t *sys = vout->p;

    vlc_mutex_lock(&sys->window_lock);
    sys->source.dar.num = num;
    sys->source.dar.den = den;

    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ASPECT_RATIO,
                          num, den);
}

void vout_ChangeCropRatio(vout_thread_t *vout, unsigned num, unsigned den)
{
    vout_thread_sys_t *sys = vout->p;

    vlc_mutex_lock(&sys->window_lock);
    if (num != 0 && den != 0) {
        sys->source.crop.mode = VOUT_CROP_RATIO;
        sys->source.crop.ratio.num = num;
        sys->source.crop.ratio.den = den;
    } else
        sys->source.crop.mode = VOUT_CROP_NONE;

    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_CROP_RATIO,
                          num, den);
}

void vout_ChangeCropWindow(vout_thread_t *vout,
                           int x, int y, int width, int height)
{
    vout_thread_sys_t *sys = vout->p;
    vout_control_cmd_t cmd;

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

    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_WINDOW);
    cmd.window.x = x;
    cmd.window.y = y;
    cmd.window.width = width;
    cmd.window.height = height;
    vout_control_Push(&vout->p->control, &cmd);
}

void vout_ChangeCropBorder(vout_thread_t *vout,
                           int left, int top, int right, int bottom)
{
    vout_thread_sys_t *sys = vout->p;
    vout_control_cmd_t cmd;

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

    vout_UpdateWindowSize(vout);
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_BORDER);
    cmd.border.left = left;
    cmd.border.top = top;
    cmd.border.right = right;
    cmd.border.bottom = bottom;
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

void vout_ChangeSubMargin(vout_thread_t *vout, int margin)
{
    if (unlikely(vout->p->spu == NULL))
        return;

    vlc_mutex_lock(&vout->p->spu_lock);
    spu_ChangeMargin(vout->p->spu, margin);
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_ChangeViewpoint(vout_thread_t *vout,
                          const vlc_viewpoint_t *p_viewpoint)
{
    vout_thread_sys_t *sys = vout->p;
    vout_control_cmd_t cmd;

    vlc_mutex_lock(&sys->window_lock);
    sys->display_cfg.viewpoint = *p_viewpoint;
    /* no window size update here */
    vlc_mutex_unlock(&sys->window_lock);

    vout_control_cmd_Init(&cmd, VOUT_CONTROL_VIEWPOINT);
    cmd.viewpoint = *p_viewpoint;
    vout_control_Push(&vout->p->control, &cmd);
}

/* */
static void VoutGetDisplayCfg(vout_thread_t *vout, vout_display_cfg_t *cfg)
{
    /* Load configuration */
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
        video_format_CopyCropAr(&picture->format, &filter->fmt_out.video);
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
    vout_thread_sys_t *sys = vout->p;

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
                    const vlc_tick_t date = vlc_tick_now();
                    const vlc_tick_t system_pts =
                        vlc_clock_ConvertToSystem(vout->p->clock, date,
                                                  decoded->date, sys->rate);
                    const vlc_tick_t late = date - system_pts;
                    vlc_tick_t late_threshold;
                    if (decoded->format.i_frame_rate && decoded->format.i_frame_rate_base)
                        late_threshold = VLC_TICK_FROM_MS(500) * decoded->format.i_frame_rate_base / decoded->format.i_frame_rate;
                    else
                        late_threshold = VOUT_DISPLAY_LATE_THRESHOLD;
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
    vout_display_t *vd = sys->display;

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
    vlc_tick_t system_now = vlc_tick_now();
    vlc_tick_t render_subtitle_date;
    if (sys->pause.is_on)
        render_subtitle_date = sys->pause.date;
    else
        render_subtitle_date = filtered->date <= 1 ? system_now :
            vlc_clock_ConvertToSystem(sys->clock, system_now, filtered->date,
                                      sys->rate);

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
                              (vd->info.is_slow || do_snapshot);

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
                                      &vd->source, system_now,
                                      render_subtitle_date, sys->spu_rate,
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
        if (subpic != NULL)
            subpicture_Delete(subpic);
        return VLC_EGENERIC;
    }

    if (!do_dr_spu && sys->spu_blend != NULL && subpic != NULL)
        picture_BlendSubpicture(todisplay, sys->spu_blend, subpic);

    system_now = vlc_tick_now();
    const vlc_tick_t pts = todisplay->date;
    const vlc_tick_t system_pts =
        vlc_clock_ConvertToSystem(sys->clock, system_now, pts, sys->rate);

    if (vd->prepare != NULL)
        vd->prepare(vd, todisplay, do_dr_spu ? subpic : NULL, system_pts);

    vout_chrono_Stop(&sys->render);
#if 0
        {
        static int i = 0;
        if (((i++)%10) == 0)
            msg_Info(vout, "render: avg %d ms var %d ms",
                     (int)(sys->render.avg/1000), (int)(sys->render.var/1000));
        }
#endif

    if (!is_forced)
    {
        system_now = vlc_tick_now();
        vlc_clock_Wait(sys->clock, system_now, pts, sys->rate,
                       VOUT_REDISPLAY_DELAY);
    }

    /* Display the direct buffer returned by vout_RenderPicture */
    vout_display_Display(vd, todisplay);
    if (subpic)
        subpicture_Delete(subpic);

    if (!is_forced)
    {
        system_now = vlc_tick_now();
        const vlc_tick_t drift = vlc_clock_Update(sys->clock, system_now,
                                                  pts, sys->rate);
        if (drift != VLC_TICK_INVALID)
            system_now += drift;
    }
    sys->displayed.date = system_now;

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    return VLC_SUCCESS;
}

static int ThreadDisplayPicture(vout_thread_t *vout, vlc_tick_t *deadline)
{
    vout_thread_sys_t *sys = vout->p;
    bool frame_by_frame = !deadline;
    bool paused = sys->pause.is_on;
    bool first = !sys->displayed.current;

    assert(sys->clock);

    if (first)
        if (ThreadDisplayPreparePicture(vout, true, frame_by_frame)) /* FIXME not sure it is ok */
            return VLC_EGENERIC;

    if (!paused || frame_by_frame)
        while (!sys->displayed.next && !ThreadDisplayPreparePicture(vout, false, frame_by_frame))
            ;

    const vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t render_delay = vout_chrono_GetHigh(&sys->render) + VOUT_MWAIT_TOLERANCE;

    bool drop_next_frame = frame_by_frame;
    vlc_tick_t date_next = VLC_TICK_INVALID;

    if (!paused && sys->displayed.next) {
        const vlc_tick_t next_system_pts =
            vlc_clock_ConvertToSystem(sys->clock, system_now,
                                      sys->displayed.next->date, sys->rate);

        date_next = next_system_pts - render_delay;
        if (date_next <= system_now)
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
        refresh = date_refresh <= vlc_tick_now();
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

void vout_ChangePause(vout_thread_t *vout, bool is_paused, vlc_tick_t date)
{
    vout_control_Hold(&vout->p->control);
    assert(!vout->p->pause.is_on || !is_paused);

    if (vout->p->pause.is_on)
        ThreadFilterFlush(vout, false);
    else {
        vout->p->step.timestamp = VLC_TICK_INVALID;
        vout->p->step.last      = VLC_TICK_INVALID;
    }
    vout->p->pause.is_on = is_paused;
    vout->p->pause.date  = date;
    vout_control_Release(&vout->p->control);

    vlc_mutex_lock(&vout->p->window_lock);
    vout_window_SetInhibition(vout->p->display_cfg.window, !is_paused);
    vlc_mutex_unlock(&vout->p->window_lock);
}

static void vout_FlushUnlocked(vout_thread_t *vout, bool below,
                               vlc_tick_t date)
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
    vout_FilterFlush(vout->p->display);

    vlc_clock_Reset(vout->p->clock);
    vlc_clock_SetDelay(vout->p->clock, vout->p->delay);

    vlc_mutex_lock(&vout->p->spu_lock);
    if (vout->p->spu)
    {
        spu_clock_Reset(vout->p->spu);
        spu_clock_SetDelay(vout->p->spu, vout->p->spu_delay);
    }
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_Flush(vout_thread_t *vout, vlc_tick_t date)
{
    vout_thread_sys_t *sys = vout->p;

    assert(vout->p->original.i_chroma != 0);

    vout_control_Hold(&sys->control);
    vout_FlushUnlocked(vout, false, date);
    vout_control_Release(&sys->control);
}

void vout_NextPicture(vout_thread_t *vout, vlc_tick_t *duration)
{
    *duration = 0;

    vout_control_Hold(&vout->p->control);
    if (vout->p->step.last == VLC_TICK_INVALID)
        vout->p->step.last = vout->p->displayed.timestamp;

    if (ThreadDisplayPicture(vout, NULL) == 0) {
        vout->p->step.timestamp = vout->p->displayed.timestamp;

        if (vout->p->step.last != VLC_TICK_INVALID &&
            vout->p->step.timestamp > vout->p->step.last) {
            *duration = vout->p->step.timestamp - vout->p->step.last;
            vout->p->step.last = vout->p->step.timestamp;
            /* TODO advance subpicture by the duration ... */
        }
    }
    vout_control_Release(&vout->p->control);
}

void vout_ChangeDelay(vout_thread_t *vout, vlc_tick_t delay)
{
    vout_thread_sys_t *sys = vout->p;

    vout_control_Hold(&sys->control);
    vlc_clock_SetDelay(vout->p->clock, delay);
    vout->p->delay = delay;
    vout_control_Release(&sys->control);
}

void vout_ChangeRate(vout_thread_t *vout, float rate)
{
    vout_thread_sys_t *sys = vout->p;

    vout_control_Hold(&sys->control);
    sys->rate = rate;
    vout_control_Release(&sys->control);
}

void vout_ChangeSpuDelay(vout_thread_t *vout, vlc_tick_t delay)
{
    vlc_mutex_lock(&vout->p->spu_lock);
    if (vout->p->spu)
        spu_clock_SetDelay(vout->p->spu, delay);
    vout->p->spu_delay = delay;
    vlc_mutex_unlock(&vout->p->spu_lock);
}

void vout_ChangeSpuRate(vout_thread_t *vout, float rate)
{
    vlc_mutex_lock(&vout->p->spu_lock);
    vout->p->spu_rate = rate;
    vlc_mutex_unlock(&vout->p->spu_lock);
}

static void ThreadProcessMouseState(vout_thread_t *vout,
                                    const vlc_mouse_t *win_mouse)
{
    vlc_mouse_t vid_mouse, tmp1, tmp2, *m;

    /* Translate window coordinates to video coordinates */
    vout_display_TranslateMouseState(vout->p->display, &vid_mouse, win_mouse);

    /* Let SPU handle the mouse */
    if (likely(vout->p->spu != NULL)
     && spu_ProcessMouse(vout->p->spu, &vid_mouse, &vout->p->display->source))
        return;

    /* Then pass up the filter chains. */
    m = &vid_mouse;
    vlc_mutex_lock(&vout->p->filter.lock);
    if (vout->p->filter.chain_static && vout->p->filter.chain_interactive) {
        if (!filter_chain_MouseFilter(vout->p->filter.chain_interactive,
                                      &tmp1, m))
            m = &tmp1;
        if (!filter_chain_MouseFilter(vout->p->filter.chain_static,
                                      &tmp2, m))
            m = &tmp2;
    }
    vlc_mutex_unlock(&vout->p->filter.lock);

    if (vlc_mouse_HasMoved(&vout->p->mouse, m))
        var_SetCoords(vout, "mouse-moved", m->i_x, m->i_y);

    if (vlc_mouse_HasButton(&vout->p->mouse, m)) {
        var_SetInteger(vout, "mouse-button-down", m->i_pressed);

        if (vlc_mouse_HasPressed(&vout->p->mouse, m, MOUSE_BUTTON_LEFT)) {
            /* FIXME? */
            int x, y;

            var_GetCoords(vout, "mouse-moved", &x, &y);
            var_SetCoords(vout, "mouse-clicked", x, y);
        }
    }

    if (m->b_double_click)
        var_ToggleBool(vout, "fullscreen");
    vout->p->mouse = *m;

    if (vout->p->mouse_event)
        vout->p->mouse_event(m, vout->p->mouse_opaque);
}

static int vout_Start(vout_thread_t *vout, const vout_configuration_t *cfg)
{
    vout_thread_sys_t *sys = vout->p;

    sys->mouse_event = cfg->mouse_event;
    sys->mouse_opaque = cfg->mouse_opaque;
    vlc_mouse_Init(&sys->mouse);

    sys->dpb_size = cfg->dpb_size;
    sys->decoder_fifo = picture_fifo_New();
    sys->decoder_pool = NULL;
    sys->display_pool = NULL;
    sys->private_pool = NULL;

    sys->filter.configuration = NULL;
    video_format_Copy(&sys->filter.format, &sys->original);

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
    sys->filter.chain_static = filter_chain_NewVideo(vout, true, &owner);

    owner.video = &interactive_cbs;
    sys->filter.chain_interactive = filter_chain_NewVideo(vout, true, &owner);

    vout_display_cfg_t dcfg;

    vlc_mutex_lock(&sys->window_lock);
    dcfg = sys->display_cfg;
    /* Any configuration change after unlocking will involve a control request
     * that will be processed in the new thread. There may also be some pending
     * control requests for configuration change already visible in
     * sys->display_cfg, leading to processing of extra harmless and useless
     * control request processing.
     *
     * TODO: display lock separate from window lock.
     */
    vlc_mutex_unlock(&sys->window_lock);

    if (vout_OpenWrapper(vout, sys->splitter_name, &dcfg))
        goto error;

    unsigned num = 0, den = 0;
    int x = 0, y = 0, w = 0, h = 0;

    vlc_mutex_lock(&sys->window_lock); /* see above */
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
    vlc_mutex_unlock(&sys->window_lock);
    vout_SetDisplayCrop(sys->display, num, den, x, y, w, h);

    vlc_mutex_lock(&sys->window_lock); /* see above */
    num = sys->source.dar.num;
    den = sys->source.dar.den;
    vlc_mutex_unlock(&sys->window_lock);
    if (num != 0 && den != 0)
        vout_SetDisplayAspect(sys->display, num, den);

    assert(sys->decoder_pool != NULL && sys->private_pool != NULL);

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

    video_format_Print(VLC_OBJECT(vout), "original format", &sys->original);
    return VLC_SUCCESS;
error:
    if (sys->filter.chain_interactive != NULL)
    {
        ThreadDelAllFilterCallbacks(vout);
        filter_chain_Delete(sys->filter.chain_interactive);
    }
    if (sys->filter.chain_static != NULL)
        filter_chain_Delete(sys->filter.chain_static);
    video_format_Clean(&sys->filter.format);
    if (sys->decoder_fifo != NULL)
        picture_fifo_Delete(sys->decoder_fifo);
    return VLC_EGENERIC;
}

static void ThreadStop(vout_thread_t *vout)
{
    if (vout->p->spu_blend)
        filter_DeleteBlend(vout->p->spu_blend);

    /* Destroy translation tables */
    if (vout->p->display) {
        if (vout->p->decoder_pool)
            vout_FlushUnlocked(vout, true, INT64_MAX);
        vout_CloseWrapper(vout);
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
        vout->p->mouse_event(NULL, vout->p->mouse_opaque);
}

void vout_Cancel(vout_thread_t *vout, bool canceled)
{
    vout_thread_sys_t *sys = vout->p;

    vout_control_Hold(&sys->control);
    if (sys->decoder_pool != NULL)
        picture_pool_Cancel(sys->decoder_pool, canceled);
    vout_control_Release(&sys->control);
}

static int ThreadControl(vout_thread_t *vout, vout_control_cmd_t cmd)
{
    switch(cmd.type) {
    case VOUT_CONTROL_CLEAN:
        ThreadStop(vout);
        return 1;
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
    case VOUT_CONTROL_MOUSE_STATE:
        ThreadProcessMouseState(vout, &cmd.mouse);
        break;
    case VOUT_CONTROL_DISPLAY_SIZE:
        vout_display_SetSize(vout->p->display,
                             cmd.window.width, cmd.window.height);
        break;
    case VOUT_CONTROL_DISPLAY_FILLED:
        vout_SetDisplayFilled(vout->p->display, cmd.boolean);
        break;
    case VOUT_CONTROL_ZOOM:
        vout_SetDisplayZoom(vout->p->display, cmd.pair.a, cmd.pair.b);
        break;
    case VOUT_CONTROL_ASPECT_RATIO:
        vout_SetDisplayAspect(vout->p->display, cmd.pair.a, cmd.pair.b);
        break;
    case VOUT_CONTROL_CROP_RATIO:
        vout_SetDisplayCrop(vout->p->display, cmd.pair.a, cmd.pair.b,
                            0, 0, 0, 0);
        break;
    case VOUT_CONTROL_CROP_WINDOW:
        vout_SetDisplayCrop(vout->p->display, 0, 0,
                            cmd.window.x, cmd.window.y,
                            cmd.window.width, cmd.window.height);
        break;
    case VOUT_CONTROL_CROP_BORDER:
        vout_SetDisplayCrop(vout->p->display, 0, 0,
                            cmd.border.left, cmd.border.top,
                            -(int)cmd.border.right, -(int)cmd.border.bottom);
        break;
    case VOUT_CONTROL_VIEWPOINT:
        vout_SetDisplayViewpoint(vout->p->display, &cmd.viewpoint);
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
    }

out:
    return NULL;
}

static void vout_StopDisplay(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

    assert(sys->original.i_chroma != 0);
    vout_control_PushVoid(&sys->control, VOUT_CONTROL_CLEAN);
    vlc_join(sys->thread, NULL);

    spu_Detach(sys->spu);
    sys->mouse_event = NULL;
    sys->clock = NULL;
    video_format_Clean(&sys->original);
}

void vout_Stop(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

    vout_StopDisplay(vout);

    vlc_mutex_lock(&sys->window_lock);
    if (sys->window_active) {
        vout_window_Disable(sys->display_cfg.window);
        sys->window_active = false;
    }
    vlc_mutex_unlock(&sys->window_lock);
}

void vout_Close(vout_thread_t *vout)
{
    assert(vout);

    vout_thread_sys_t *sys = vout->p;

    if (sys->original.i_chroma != 0)
        vout_Stop(vout);

    vout_IntfDeinit(VLC_OBJECT(vout));
    vout_snapshot_End(sys->snapshot);
    vout_control_Dead(&sys->control);
    vout_chrono_Clean(&sys->render);

    vlc_mutex_lock(&sys->spu_lock);
    spu_Destroy(sys->spu);
    sys->spu = NULL;
    vlc_mutex_unlock(&sys->spu_lock);

    vout_Release(vout);
}

void vout_Release(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

    if (atomic_fetch_sub_explicit(&sys->refs, 1, memory_order_release))
        return;

    free(vout->p->splitter_name);

    /* Destroy the locks */
    vlc_mutex_destroy(&vout->p->window_lock);
    vlc_mutex_destroy(&vout->p->spu_lock);
    vlc_mutex_destroy(&vout->p->filter.lock);

    vout_display_window_Delete(sys->display_cfg.window);

    vout_control_Clean(&vout->p->control);

    /* */
    vout_statistic_Clean(&vout->p->statistic);

    /* */
    vout_snapshot_Destroy(vout->p->snapshot);
    video_format_Clean(&vout->p->original);
    vlc_object_delete(VLC_OBJECT(vout));
}

vout_thread_t *vout_Create(vlc_object_t *object)
{
    /* Allocate descriptor */
    vout_thread_t *vout = vlc_custom_create(object,
                                            sizeof(*vout) + sizeof(*vout->p),
                                            "video output");
    if (!vout)
        return NULL;

    /* Register the VLC variable and callbacks. On the one hand, the variables
     * must be ready early on because further initializations below depend on
     * some of them. On the other hand, the callbacks depend on said
     * initializations. In practice, this works because the object is not
     * visible and callbacks not triggerable before this function returns the
     * fully initialized object to its caller.
     */
    vout_IntfInit(vout);

    vout_thread_sys_t *sys = (vout_thread_sys_t *)&vout[1];

    vout->p = sys;

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
    vlc_mutex_init(&sys->spu_lock);
    sys->spu = spu_Create(vout, vout);

    vout_control_Init(&sys->control);

    sys->title.show     = var_InheritBool(vout, "video-title-show");
    sys->title.timeout  = var_InheritInteger(vout, "video-title-timeout");
    sys->title.position = var_InheritInteger(vout, "video-title-position");

    vout_InitInterlacingSupport(vout);

    sys->is_late_dropped = var_InheritBool(vout, "drop-late-frames");

    vlc_mutex_init(&sys->filter.lock);

    /* Window */
    sys->display_cfg.window = vout_display_window_New(vout);
    if (sys->display_cfg.window == NULL) {
        spu_Destroy(sys->spu);
        vlc_object_delete(vout);
        return NULL;
    }

    if (sys->splitter_name != NULL)
        var_Destroy(vout, "window");
    sys->window_active = false;
    vlc_mutex_init(&sys->window_lock);

    /* Arbitrary initial time */
    vout_chrono_Init(&sys->render, 5, VLC_TICK_FROM_MS(10));

    /* */
    atomic_init(&sys->refs, 0);

    if (var_InheritBool(vout, "video-wallpaper"))
        vout_window_SetState(sys->display_cfg.window, VOUT_WINDOW_STATE_BELOW);
    else if (var_InheritBool(vout, "video-on-top"))
        vout_window_SetState(sys->display_cfg.window, VOUT_WINDOW_STATE_ABOVE);

    return vout;
}

vout_thread_t *vout_Hold(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

    atomic_fetch_add_explicit(&sys->refs, 1, memory_order_relaxed);
    return vout;
}

int vout_Request(const vout_configuration_t *cfg, input_thread_t *input)
{
    vout_thread_t *vout = cfg->vout;
    vout_thread_sys_t *sys = vout->p;

    assert(vout != NULL);
    assert(cfg->fmt != NULL);
    assert(cfg->clock != NULL);

    if (!VoutCheckFormat(cfg->fmt))
        return -1;

    video_format_t original;
    VoutFixFormat(&original, cfg->fmt);

    /* TODO: If dimensions are equal or slightly smaller, update the aspect
     * ratio and crop settings, instead of recreating a display.
     */
    if (video_format_IsSimilar(&original, &sys->original)) {
        if (cfg->dpb_size <= sys->dpb_size) {
            video_format_Clean(&original);
            /* It is assumed that the SPU input matches input already. */
            return 0;
        }
        msg_Warn(vout, "DPB need to be increased");
    }

    if (sys->original.i_chroma != 0)
        vout_StopDisplay(vout);

    vout_ReinitInterlacingSupport(vout);

    sys->original = original;

    vlc_mutex_lock(&sys->window_lock);
    if (!sys->window_active) {
        vout_window_cfg_t wcfg = {
            .is_fullscreen = var_GetBool(vout, "fullscreen"),
            .is_decorated = var_InheritBool(vout, "video-deco"),
        // TODO: take pixel A/R, crop and zoom into account
#if defined(__APPLE__) || defined(_WIN32)
            .x = var_InheritInteger(vout, "video-x"),
            .y = var_InheritInteger(vout, "video-y"),
#endif
        };

        VoutGetDisplayCfg(vout, &sys->display_cfg);
        vout_SizeWindow(vout, &wcfg.width, &wcfg.height);

        if (vout_window_Enable(sys->display_cfg.window, &wcfg)) {
            vlc_mutex_unlock(&sys->window_lock);
            goto error;
        }
        sys->window_active = true;
    } else
        vout_UpdateWindowSize(vout);

    sys->delay = sys->spu_delay = 0;
    sys->rate = sys->spu_rate = 1.f;
    sys->clock = cfg->clock;
    sys->delay = sys->spu_delay = 0;

    vlc_mutex_unlock(&sys->window_lock);

    if (vout_Start(vout, cfg))
        goto error;
    if (vlc_clone(&sys->thread, Thread, vout, VLC_THREAD_PRIORITY_OUTPUT)) {
        vout_Stop(vout);
error:
        msg_Err(vout, "video output creation failed");
        video_format_Clean(&sys->original);
        return -1;
    }

    if (input != NULL)
        spu_Attach(sys->spu, input);
    vout_IntfReinit(vout);
    return 0;
}
