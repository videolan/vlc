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
#include <vlc_vout_osd.h>
#include <vlc_image.h>

#include <libvlc.h>
#include "vout_internal.h"
#include "interlacing.h"
#include "display.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *Thread(void *);
static void VoutDestructor(vlc_object_t *);

/* Maximum delay between 2 displayed pictures.
 * XXX it is needed for now but should be removed in the long term.
 */
#define VOUT_REDISPLAY_DELAY (INT64_C(80000))

/**
 * Late pictures having a delay higher than this value are thrashed.
 */
#define VOUT_DISPLAY_LATE_THRESHOLD (INT64_C(20000))

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE (INT64_C(4000))

/* */
static int VoutValidateFormat(video_format_t *dst,
                              const video_format_t *src)
{
    if (src->i_width <= 0  || src->i_width  > 8192 ||
        src->i_height <= 0 || src->i_height > 8192)
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
                                 const vout_configuration_t *cfg)
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

    vout_control_Init(&vout->p->control);
    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_INIT);

    vout_statistic_Init(&vout->p->statistic);

    vout_snapshot_Init(&vout->p->snapshot);

    /* Initialize locks */
    vlc_mutex_init(&vout->p->picture_lock);
    vlc_mutex_init(&vout->p->filter.lock);
    vlc_mutex_init(&vout->p->spu_lock);

    /* Initialize subpicture unit */
    vout->p->spu = spu_Create(vout);

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit(vout);

    vout->p->title.show     = var_InheritBool(vout, "video-title-show");
    vout->p->title.timeout  = var_InheritInteger(vout, "video-title-timeout");
    vout->p->title.position = var_InheritInteger(vout, "video-title-position");

    /* Get splitter name if present */
    char *splitter_name = var_InheritString(vout, "video-splitter");
    if (splitter_name && *splitter_name) {
        vout->p->splitter_name = splitter_name;
    } else {
        free(splitter_name);
    }

    /* */
    vout_InitInterlacingSupport(vout, vout->p->displayed.is_interlaced);

    /* */
    vlc_object_set_destructor(vout, VoutDestructor);

    /* */
    if (vlc_clone(&vout->p->thread, Thread, vout,
                  VLC_THREAD_PRIORITY_OUTPUT)) {
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

    vout->p->input = cfg->input;
    if (vout->p->input)
        spu_Attach(vout->p->spu, vout->p->input, true);

    return vout;
}

vout_thread_t *(vout_Request)(vlc_object_t *object,
                              const vout_configuration_t *cfg)
{
    vout_thread_t *vout = cfg->vout;
    if (cfg->change_fmt && !cfg->fmt) {
        if (vout)
            vout_CloseAndRelease(vout);
        return NULL;
    }

    /* If a vout is provided, try reusing it */
    if (vout) {
        if (vout->p->input != cfg->input) {
            if (vout->p->input)
                spu_Attach(vout->p->spu, vout->p->input, false);
            vout->p->input = cfg->input;
            if (vout->p->input)
                spu_Attach(vout->p->spu, vout->p->input, true);
        }

        if (cfg->change_fmt) {
            vout_control_cmd_t cmd;
            vout_control_cmd_Init(&cmd, VOUT_CONTROL_REINIT);
            cmd.u.cfg = cfg;

            vout_control_Push(&vout->p->control, &cmd);
            vout_control_WaitEmpty(&vout->p->control);
        }

        if (!vout->p->dead) {
            msg_Dbg(object, "reusing provided vout");
            vout_IntfReinit(vout);
            return vout;
        }
        vout_CloseAndRelease(vout);

        msg_Warn(object, "cannot reuse provided vout");
    }
    return VoutCreate(object, cfg);
}

void vout_Close(vout_thread_t *vout)
{
    assert(vout);

    if (vout->p->input)
        spu_Attach(vout->p->spu, vout->p->input, false);

    vout_snapshot_End(&vout->p->snapshot);

    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_CLEAN);
    vlc_join(vout->p->thread, NULL);

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
    vlc_mutex_destroy(&vout->p->spu_lock);
    vlc_mutex_destroy(&vout->p->picture_lock);
    vlc_mutex_destroy(&vout->p->filter.lock);
    vout_control_Clean(&vout->p->control);

    /* */
    vout_statistic_Clean(&vout->p->statistic);

    /* */
    vout_snapshot_Clean(&vout->p->snapshot);

    video_format_Clean(&vout->p->original);
}

/* */
void vout_ChangePause(vout_thread_t *vout, bool is_paused, mtime_t date)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_PAUSE);
    cmd.u.pause.is_on = is_paused;
    cmd.u.pause.date  = date;
    vout_control_Push(&vout->p->control, &cmd);

    vout_control_WaitEmpty(&vout->p->control);
}

void vout_GetResetStatistic(vout_thread_t *vout, int *displayed, int *lost)
{
    vout_statistic_GetReset( &vout->p->statistic, displayed, lost );
}

void vout_Flush(vout_thread_t *vout, mtime_t date)
{
    vout_control_PushTime(&vout->p->control, VOUT_CONTROL_FLUSH, date);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_Reset(vout_thread_t *vout)
{
    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_RESET);
    vout_control_WaitEmpty(&vout->p->control);
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (picture)
        picture_Release(picture);

    vlc_mutex_unlock(&vout->p->picture_lock);

    return !picture;
}

void vout_FixLeaks( vout_thread_t *vout )
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (!picture) {
        picture = picture_pool_Get(vout->p->decoder_pool);
    }

    if (picture) {
        picture_Release(picture);
        /* Not all pictures has been displayed yet or some are
         * free */
        vlc_mutex_unlock(&vout->p->picture_lock);
        return;
    }

    /* There is no reason that no pictures are available, force one
     * from the pool, becarefull with it though */
    msg_Err(vout, "pictures leaked, trying to workaround");

    /* */
    picture_pool_NonEmpty(vout->p->decoder_pool, false);

    vlc_mutex_unlock(&vout->p->picture_lock);
}
void vout_NextPicture(vout_thread_t *vout, mtime_t *duration)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_STEP);
    cmd.u.time_ptr = duration;

    vout_control_Push(&vout->p->control, &cmd);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_DisplayTitle(vout_thread_t *vout, const char *title)
{
    assert(title);
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_OSD_TITLE, title);
}

void vout_PutSubpicture( vout_thread_t *vout, subpicture_t *subpic )
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_SUBPICTURE);
    cmd.u.subpicture = subpic;

    vout_control_Push(&vout->p->control, &cmd);
}
int vout_RegisterSubpictureChannel( vout_thread_t *vout )
{
    int channel = SPU_DEFAULT_CHANNEL;

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

/**
 * It retreives a picture from the vout or NULL if no pictures are
 * available yet.
 *
 * You MUST call vout_PutPicture or vout_ReleasePicture on it.
 *
 * You may use vout_HoldPicture(paired with vout_ReleasePicture) to keep a
 * read-only reference.
 */
picture_t *vout_GetPicture(vout_thread_t *vout)
{
    /* Get lock */
    vlc_mutex_lock(&vout->p->picture_lock);
    picture_t *picture = picture_pool_Get(vout->p->decoder_pool);
    if (picture) {
        picture_Reset(picture);
        VideoFormatCopyCropAr(&picture->format, &vout->p->original);
    }
    vlc_mutex_unlock(&vout->p->picture_lock);

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
    vlc_mutex_lock(&vout->p->picture_lock);

    picture->p_next = NULL;
    picture_fifo_Push(vout->p->decoder_fifo, picture);

    vlc_mutex_unlock(&vout->p->picture_lock);

    vout_control_Wake(&vout->p->control);
}

/**
 * It releases a picture retreived by vout_GetPicture.
 */
void vout_ReleasePicture(vout_thread_t *vout, picture_t *picture)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_Release(picture);

    vlc_mutex_unlock(&vout->p->picture_lock);

    vout_control_Wake(&vout->p->control);
}

/**
 * It increment the reference counter of a picture retreived by
 * vout_GetPicture.
 */
void vout_HoldPicture(vout_thread_t *vout, picture_t *picture)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_Hold(picture);

    vlc_mutex_unlock(&vout->p->picture_lock);
}

/* */
int vout_GetSnapshot(vout_thread_t *vout,
                     block_t **image_dst, picture_t **picture_dst,
                     video_format_t *fmt,
                     const char *type, mtime_t timeout)
{
    picture_t *picture = vout_snapshot_Get(&vout->p->snapshot, timeout);
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
void vout_ControlChangeFullscreen(vout_thread_t *vout, bool fullscreen)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_FULLSCREEN,
                          fullscreen);
}
void vout_ControlChangeOnTop(vout_thread_t *vout, bool is_on_top)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_ON_TOP,
                          is_on_top);
}
void vout_ControlChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_DISPLAY_FILLED,
                          is_filled);
}
void vout_ControlChangeZoom(vout_thread_t *vout, int num, int den)
{
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ZOOM,
                          num, den);
}
void vout_ControlChangeSampleAspectRatio(vout_thread_t *vout,
                                         unsigned num, unsigned den)
{
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ASPECT_RATIO,
                          num, den);
}
void vout_ControlChangeCropRatio(vout_thread_t *vout,
                                 unsigned num, unsigned den)
{
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_CROP_RATIO,
                          num, den);
}
void vout_ControlChangeCropWindow(vout_thread_t *vout,
                                  int x, int y, int width, int height)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_WINDOW);
    cmd.u.window.x      = __MAX(x, 0);
    cmd.u.window.y      = __MAX(y, 0);
    cmd.u.window.width  = __MAX(width, 0);
    cmd.u.window.height = __MAX(height, 0);

    vout_control_Push(&vout->p->control, &cmd);
}
void vout_ControlChangeCropBorder(vout_thread_t *vout,
                                  int left, int top, int right, int bottom)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_BORDER);
    cmd.u.border.left   = __MAX(left, 0);
    cmd.u.border.top    = __MAX(top, 0);
    cmd.u.border.right  = __MAX(right, 0);
    cmd.u.border.bottom = __MAX(bottom, 0);

    vout_control_Push(&vout->p->control, &cmd);
}
void vout_ControlChangeFilters(vout_thread_t *vout, const char *filters)
{
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_FILTERS,
                            filters);
}
void vout_ControlChangeSubSources(vout_thread_t *vout, const char *filters)
{
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_SUB_SOURCES,
                            filters);
}
void vout_ControlChangeSubFilters(vout_thread_t *vout, const char *filters)
{
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_SUB_FILTERS,
                            filters);
}
void vout_ControlChangeSubMargin(vout_thread_t *vout, int margin)
{
    vout_control_PushInteger(&vout->p->control, VOUT_CONTROL_CHANGE_SUB_MARGIN,
                             margin);
}

/* */
static void VoutGetDisplayCfg(vout_thread_t *vout, vout_display_cfg_t *cfg, const char *title)
{
    /* Load configuration */
    cfg->is_fullscreen = var_CreateGetBool(vout, "fullscreen");
    cfg->display.title = title;
    const int display_width = var_CreateGetInteger(vout, "width");
    const int display_height = var_CreateGetInteger(vout, "height");
    cfg->display.width   = display_width > 0  ? display_width  : 0;
    cfg->display.height  = display_height > 0 ? display_height : 0;
    cfg->is_display_filled  = var_CreateGetBool(vout, "autoscale");
    unsigned msar_num, msar_den;
    if (var_InheritURational(vout, &msar_num, &msar_den, "monitor-par") ||
        msar_num <= 0 || msar_den <= 0) {
        msar_num = 1;
        msar_den = 1;
    }
    cfg->display.sar.num = msar_num;
    cfg->display.sar.den = msar_den;
    unsigned zoom_den = 1000;
    unsigned zoom_num = zoom_den * var_CreateGetFloat(vout, "scale");
    vlc_ureduce(&zoom_num, &zoom_den, zoom_num, zoom_den, 0);
    cfg->zoom.num = zoom_num;
    cfg->zoom.den = zoom_den;
    cfg->align.vertical = VOUT_DISPLAY_ALIGN_CENTER;
    cfg->align.horizontal = VOUT_DISPLAY_ALIGN_CENTER;
    const int align_mask = var_CreateGetInteger(vout, "align");
    if (align_mask & 0x1)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_LEFT;
    else if (align_mask & 0x2)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_RIGHT;
    if (align_mask & 0x4)
        cfg->align.vertical = VOUT_DISPLAY_ALIGN_TOP;
    else if (align_mask & 0x8)
        cfg->align.vertical = VOUT_DISPLAY_ALIGN_BOTTOM;
}

vout_window_t * vout_NewDisplayWindow(vout_thread_t *vout, vout_display_t *vd,
                                      const vout_window_cfg_t *cfg)
{
    VLC_UNUSED(vd);
    vout_window_cfg_t cfg_override = *cfg;

    if (!var_InheritBool( vout, "embedded-video"))
        cfg_override.is_standalone = true;

    if (vout->p->window.is_unused && vout->p->window.object) {
        assert(!vout->p->splitter_name);
        if (!cfg_override.is_standalone == !vout->p->window.cfg.is_standalone &&
            cfg_override.type           == vout->p->window.cfg.type) {
            /* Reuse the stored window */
            msg_Dbg(vout, "Reusing previous vout window");
            vout_window_t *window = vout->p->window.object;
            if (cfg_override.width  != vout->p->window.cfg.width ||
                cfg_override.height != vout->p->window.cfg.height)
                vout_window_SetSize(window,
                                    cfg_override.width, cfg_override.height);
            vout->p->window.is_unused = false;
            vout->p->window.cfg       = cfg_override;
            return window;
        }

        vout_window_Delete(vout->p->window.object);
        vout->p->window.is_unused = true;
        vout->p->window.object    = NULL;
    }

    vout_window_t *window = vout_window_New(VLC_OBJECT(vout), "$window",
                                            &cfg_override);
    if (!window)
        return NULL;
    if (!vout->p->splitter_name) {
        vout->p->window.is_unused = false;
        vout->p->window.cfg       = cfg_override;
        vout->p->window.object    = window;
    }
    return window;
}

void vout_DeleteDisplayWindow(vout_thread_t *vout, vout_display_t *vd,
                              vout_window_t *window)
{
    VLC_UNUSED(vd);
    if (!vout->p->window.is_unused && vout->p->window.object == window) {
        vout->p->window.is_unused = true;
    } else if (vout->p->window.is_unused && vout->p->window.object && !window) {
        vout_window_Delete(vout->p->window.object);
        vout->p->window.is_unused = true;
        vout->p->window.object    = NULL;
    } else if (window) {
        vout_window_Delete(window);
    }
}

/* */
static picture_t *VoutVideoFilterInteractiveNewPicture(filter_t *filter)
{
    vout_thread_t *vout = (vout_thread_t*)filter->p_owner;

    picture_t *picture = picture_pool_Get(vout->p->private_pool);
    if (picture) {
        picture_Reset(picture);
        VideoFormatCopyCropAr(&picture->format, &filter->fmt_out.video);
    }
    return picture;
}
static picture_t *VoutVideoFilterStaticNewPicture(filter_t *filter)
{
    vout_thread_t *vout = (vout_thread_t*)filter->p_owner;

    vlc_assert_locked(&vout->p->filter.lock);
    if (filter_chain_GetLength(vout->p->filter.chain_interactive) == 0)
        return VoutVideoFilterInteractiveNewPicture(filter);

    return picture_NewFromFormat(&filter->fmt_out.video);
}
static void VoutVideoFilterDelPicture(filter_t *filter, picture_t *picture)
{
    VLC_UNUSED(filter);
    picture_Release(picture);
}
static int VoutVideoFilterStaticAllocationSetup(filter_t *filter, void *data)
{
    filter->pf_video_buffer_new = VoutVideoFilterStaticNewPicture;
    filter->pf_video_buffer_del = VoutVideoFilterDelPicture;
    filter->p_owner             = data; /* vout */
    return VLC_SUCCESS;
}
static int VoutVideoFilterInteractiveAllocationSetup(filter_t *filter, void *data)
{
    filter->pf_video_buffer_new = VoutVideoFilterInteractiveNewPicture;
    filter->pf_video_buffer_del = VoutVideoFilterDelPicture;
    filter->p_owner             = data; /* vout */
    return VLC_SUCCESS;
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
                                bool is_locked)
{
    ThreadFilterFlush(vout, is_locked);

    vlc_array_t array_static;
    vlc_array_t array_interactive;

    vlc_array_init(&array_static);
    vlc_array_init(&array_interactive);
    char *current = filters ? strdup(filters) : NULL;
    while (current) {
        config_chain_t *cfg;
        char *name;
        char *next = config_ChainCreate(&name, &cfg, current);

        if (name && *name) {
            vout_filter_t *e = xmalloc(sizeof(*e));
            e->name = name;
            e->cfg  = cfg;
            if (!strcmp(e->name, "deinterlace") ||
                !strcmp(e->name, "postproc")) {
                vlc_array_append(&array_static, e);
            } else {
                vlc_array_append(&array_interactive, e);
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

    es_format_t fmt_current = fmt_target;

    for (int a = 0; a < 2; a++) {
        vlc_array_t    *array = a == 0 ? &array_static :
                                         &array_interactive;
        filter_chain_t *chain = a == 0 ? vout->p->filter.chain_static :
                                         vout->p->filter.chain_interactive;

        filter_chain_Reset(chain, &fmt_current, &fmt_current);
        for (int i = 0; i < vlc_array_count(array); i++) {
            vout_filter_t *e = vlc_array_item_at_index(array, i);
            msg_Dbg(vout, "Adding '%s' as %s", e->name, a == 0 ? "static" : "interactive");
            if (!filter_chain_AppendFilter(chain, e->name, e->cfg, NULL, NULL)) {
                msg_Err(vout, "Failed to add filter '%s'", e->name);
                config_ChainDestroy(e->cfg);
            }
            free(e->name);
            free(e);
        }
        fmt_current = *filter_chain_GetFmtOut(chain);
        vlc_array_clear(array);
    }
    VideoFormatCopyCropAr(&fmt_target.video, &fmt_current.video);
    if (!es_format_IsSimilar(&fmt_current, &fmt_target)) {
        msg_Dbg(vout, "Adding a filter to compensate for format changes");
        if (!filter_chain_AppendFilter(vout->p->filter.chain_interactive, NULL, NULL,
                                       &fmt_current, &fmt_target)) {
            msg_Err(vout, "Failed to compensate for the format changes, removing all filters");
            filter_chain_Reset(vout->p->filter.chain_static,      &fmt_target, &fmt_target);
            filter_chain_Reset(vout->p->filter.chain_interactive, &fmt_target, &fmt_target);
        }
    }

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
static int ThreadDisplayPreparePicture(vout_thread_t *vout, bool reuse, bool is_late_dropped)
{
    int lost_count = 0;

    vlc_mutex_lock(&vout->p->filter.lock);

    picture_t *picture = filter_chain_VideoFilter(vout->p->filter.chain_static, NULL);
    assert(!reuse || !picture);

    while (!picture) {
        picture_t *decoded;
        if (reuse && vout->p->displayed.decoded) {
            decoded = picture_Hold(vout->p->displayed.decoded);
        } else {
            decoded = picture_fifo_Pop(vout->p->decoder_fifo);
            if (is_late_dropped && decoded && !decoded->b_force) {
                const mtime_t predicted = mdate() + 0; /* TODO improve */
                const mtime_t late = predicted - decoded->date;
                if (late > VOUT_DISPLAY_LATE_THRESHOLD) {
                    msg_Warn(vout, "picture is too late to be displayed (missing %d ms)", (int)(late/1000));
                    picture_Release(decoded);
                    lost_count++;
                    continue;
                } else if (late > 0) {
                    msg_Dbg(vout, "picture might be displayed late (missing %d ms)", (int)(late/1000));
                }
            }
            if (decoded &&
                !VideoFormatIsCropArEqual(&decoded->format, &vout->p->filter.format))
                ThreadChangeFilters(vout, &decoded->format, vout->p->filter.configuration, true);
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

    vout_statistic_AddLost(&vout->p->statistic, lost_count);
    if (!picture)
        return VLC_EGENERIC;

    assert(!vout->p->displayed.next);
    if (!vout->p->displayed.current)
        vout->p->displayed.current = picture;
    else
        vout->p->displayed.next    = picture;
    return VLC_SUCCESS;
}

static int ThreadDisplayRenderPicture(vout_thread_t *vout, bool is_forced)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = vout->p->display.vd;

    picture_t *torender = picture_Hold(vout->p->displayed.current);

    vout_chrono_Start(&vout->p->render);

    vlc_mutex_lock(&vout->p->filter.lock);
    picture_t *filtered = filter_chain_VideoFilter(vout->p->filter.chain_interactive, torender);
    vlc_mutex_unlock(&vout->p->filter.lock);

    if (!filtered)
        return VLC_EGENERIC;

    if (filtered->date != vout->p->displayed.current->date)
        msg_Warn(vout, "Unsupported timestamp modifications done by chain_interactive");

    /*
     * Get the subpicture to be displayed
     */
    const bool do_snapshot = vout_snapshot_IsRequested(&vout->p->snapshot);
    mtime_t render_subtitle_date;
    if (vout->p->pause.is_on)
        render_subtitle_date = vout->p->pause.date;
    else
        render_subtitle_date = filtered->date > 1 ? filtered->date : mdate();
    mtime_t render_osd_date = mdate(); /* FIXME wrong */

    /*
     * Get the subpicture to be displayed
     */
    const bool do_dr_spu = !do_snapshot &&
                           vd->info.subpicture_chromas &&
                           *vd->info.subpicture_chromas != 0;
    const bool do_early_spu = !do_dr_spu &&
                              (vd->info.is_slow ||
                               sys->display.use_dr ||
                               do_snapshot ||
                               !vout_IsDisplayFiltered(vd) ||
                               vd->fmt.i_width * vd->fmt.i_height <= vd->source.i_width * vd->source.i_height);

    const vlc_fourcc_t *subpicture_chromas;
    video_format_t fmt_spu;
    if (do_dr_spu) {
        vout_display_place_t place;
        vout_display_PlacePicture(&place, &vd->source, vd->cfg, false);

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

        if (vout->p->spu_blend &&
            vout->p->spu_blend->fmt_out.video.i_chroma != fmt_spu.i_chroma) {
            filter_DeleteBlend(vout->p->spu_blend);
            vout->p->spu_blend = NULL;
            vout->p->spu_blend_chroma = 0;
        }
        if (!vout->p->spu_blend && vout->p->spu_blend_chroma != fmt_spu.i_chroma) {
            vout->p->spu_blend_chroma = fmt_spu.i_chroma;
            vout->p->spu_blend = filter_NewBlend(VLC_OBJECT(vout), &fmt_spu);
            if (!vout->p->spu_blend)
                msg_Err(vout, "Failed to create blending filter, OSD/Subtitles will not work");
        }
    }

    subpicture_t *subpic = spu_Render(vout->p->spu,
                                      subpicture_chromas, &fmt_spu,
                                      &vd->source,
                                      render_subtitle_date, render_osd_date,
                                      do_snapshot);
    /*
     * Perform rendering
     *
     * We have to:
     * - be sure to end up with a direct buffer.
     * - blend subtitles, and in a fast access buffer
     */
    bool is_direct = vout->p->decoder_pool == vout->p->display_pool;
    picture_t *todisplay = filtered;
    if (do_early_spu && subpic) {
        todisplay = picture_pool_Get(vout->p->private_pool);
        if (todisplay) {
            VideoFormatCopyCropAr(&todisplay->format, &filtered->format);
            picture_Copy(todisplay, filtered);
            if (vout->p->spu_blend)
                picture_BlendSubpicture(todisplay, vout->p->spu_blend, subpic);
        }
        picture_Release(filtered);
        subpicture_Delete(subpic);
        subpic = NULL;

        if (!todisplay)
            return VLC_EGENERIC;
    }

    picture_t *direct;
    if (!is_direct && todisplay) {
        direct = picture_pool_Get(vout->p->display_pool);
        if (direct) {
            VideoFormatCopyCropAr(&direct->format, &todisplay->format);
            picture_Copy(direct, todisplay);
        }
        picture_Release(todisplay);
    } else {
        direct = todisplay;
    }

    if (!direct) {
        if (subpic)
            subpicture_Delete(subpic);
        return VLC_EGENERIC;
    }

    /*
     * Take a snapshot if requested
     */
    if (do_snapshot)
        vout_snapshot_Set(&vout->p->snapshot, &vd->source, direct);

    /* Render the direct buffer */
    assert(vout_IsDisplayFiltered(vd) == !sys->display.use_dr);
    vout_UpdateDisplaySourceProperties(vd, &direct->format);
    if (sys->display.use_dr) {
        vout_display_Prepare(vd, direct, subpic);
    } else {
        sys->display.filtered = vout_FilterDisplay(vd, direct);
        if (sys->display.filtered) {
            if (!do_dr_spu && !do_early_spu && vout->p->spu_blend && subpic)
                picture_BlendSubpicture(sys->display.filtered, vout->p->spu_blend, subpic);
            vout_display_Prepare(vd, sys->display.filtered, do_dr_spu ? subpic : NULL);
        }
        if (!do_dr_spu && subpic)
            subpicture_Delete(subpic);
        if (!sys->display.filtered)
            return VLC_EGENERIC;
    }

    vout_chrono_Stop(&vout->p->render);
#if 0
        {
        static int i = 0;
        if (((i++)%10) == 0)
            msg_Info(vout, "render: avg %d ms var %d ms",
                     (int)(vout->p->render.avg/1000), (int)(vout->p->render.var/1000));
        }
#endif

    /* Wait the real date (for rendering jitter) */
#if 0
    mtime_t delay = direct->date - mdate();
    if (delay < 1000)
        msg_Warn(vout, "picture is late (%lld ms)", delay / 1000);
#endif
    if (!is_forced)
        mwait(direct->date);

    /* Display the direct buffer returned by vout_RenderPicture */
    vout->p->displayed.date = mdate();
    vout_display_Display(vd,
                         sys->display.filtered ? sys->display.filtered
                                                : direct,
                         subpic);
    sys->display.filtered = NULL;

    vout_statistic_AddDisplayed(&vout->p->statistic, 1);

    return VLC_SUCCESS;
}

static int ThreadDisplayPicture(vout_thread_t *vout,
                                bool now, mtime_t *deadline)
{
    bool is_late_dropped = vout->p->is_late_dropped && !vout->p->pause.is_on && !now;
    bool first = !vout->p->displayed.current;
    if (first && ThreadDisplayPreparePicture(vout, true, is_late_dropped)) /* FIXME not sure it is ok */
        return VLC_EGENERIC;
    if (!vout->p->pause.is_on || now) {
        while (!vout->p->displayed.next) {
            if (ThreadDisplayPreparePicture(vout, false, is_late_dropped)) {
                break;
            }
        }
    }

    const mtime_t date = mdate();
    const mtime_t render_delay = vout_chrono_GetHigh(&vout->p->render) + VOUT_MWAIT_TOLERANCE;

    mtime_t date_next = VLC_TS_INVALID;
    if (!vout->p->pause.is_on && vout->p->displayed.next)
        date_next = vout->p->displayed.next->date - render_delay;

    /* FIXME/XXX we must redisplay the last decoded picture (because
     * of potential vout updated, or filters update or SPU update)
     * For now a high update period is needed but it coulmd be removed
     * if and only if:
     * - vout module emits events from theselves.
     * - *and* SPU is modified to emit an event or a deadline when needed.
     *
     * So it will be done latter.
     */
    mtime_t date_refresh = VLC_TS_INVALID;
    if (vout->p->displayed.date > VLC_TS_INVALID)
        date_refresh = vout->p->displayed.date + VOUT_REDISPLAY_DELAY - render_delay;

    bool drop = now;
    if (date_next != VLC_TS_INVALID)
        drop |= date_next + 0 <= date;

    bool refresh = false;
    if (date_refresh > VLC_TS_INVALID)
        refresh = date_refresh <= date;

    if (!first && !refresh && !drop) {
        if (date_next != VLC_TS_INVALID && date_refresh != VLC_TS_INVALID)
            *deadline = __MIN(date_next, date_refresh);
        else if (date_next != VLC_TS_INVALID)
            *deadline = date_next;
        else if (date_refresh != VLC_TS_INVALID)
            *deadline = date_refresh;
        return VLC_EGENERIC;
    }

    if (drop) {
        picture_Release(vout->p->displayed.current);
        vout->p->displayed.current = vout->p->displayed.next;
        vout->p->displayed.next    = NULL;
    }
    if (!vout->p->displayed.current)
        return VLC_EGENERIC;

    bool is_forced = now || (!drop && refresh) || vout->p->displayed.current->b_force;
    return ThreadDisplayRenderPicture(vout, is_forced);
}

static void ThreadManage(vout_thread_t *vout,
                         mtime_t *deadline,
                         vout_interlacing_support_t *interlacing)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    *deadline = VLC_TS_INVALID;
    for (;;) {
        if (ThreadDisplayPicture(vout, false, deadline))
            break;
    }

    const bool picture_interlaced = vout->p->displayed.is_interlaced;

    vlc_mutex_unlock(&vout->p->picture_lock);

    /* Deinterlacing */
    vout_SetInterlacingState(vout, interlacing, picture_interlaced);

    vout_ManageWrapper(vout);
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

static void ThreadDisplayOsdTitle(vout_thread_t *vout, const char *string)
{
    if (!vout->p->title.show)
        return;

    vout_OSDText(vout, SPU_DEFAULT_CHANNEL,
                 vout->p->title.position, INT64_C(1000) * vout->p->title.timeout,
                 string);
}

static void ThreadChangeSubSources(vout_thread_t *vout, const char *filters)
{
    spu_ChangeSources(vout->p->spu, filters);
}

static void ThreadChangeSubFilters(vout_thread_t *vout, const char *filters)
{
    spu_ChangeFilters(vout->p->spu, filters);
}

static void ThreadChangeSubMargin(vout_thread_t *vout, int margin)
{
    spu_ChangeMargin(vout->p->spu, margin);
}

static void ThreadChangePause(vout_thread_t *vout, bool is_paused, mtime_t date)
{
    assert(!vout->p->pause.is_on || !is_paused);

    if (vout->p->pause.is_on) {
        const mtime_t duration = date - vout->p->pause.date;

        if (vout->p->step.timestamp > VLC_TS_INVALID)
            vout->p->step.timestamp += duration;
        if (vout->p->step.last > VLC_TS_INVALID)
            vout->p->step.last += duration;
        picture_fifo_OffsetDate(vout->p->decoder_fifo, duration);
        if (vout->p->displayed.decoded)
            vout->p->displayed.decoded->date += duration;
        spu_OffsetSubtitleDate(vout->p->spu, duration);

        ThreadFilterFlush(vout, false);
    } else {
        vout->p->step.timestamp = VLC_TS_INVALID;
        vout->p->step.last      = VLC_TS_INVALID;
    }
    vout->p->pause.is_on = is_paused;
    vout->p->pause.date  = date;
}

static void ThreadFlush(vout_thread_t *vout, bool below, mtime_t date)
{
    vout->p->step.timestamp = VLC_TS_INVALID;
    vout->p->step.last      = VLC_TS_INVALID;

    ThreadFilterFlush(vout, false); /* FIXME too much */

    picture_t *last = vout->p->displayed.decoded;
    if (last) {
        if (( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            vout->p->displayed.decoded   = NULL;
            vout->p->displayed.date      = VLC_TS_INVALID;
            vout->p->displayed.timestamp = VLC_TS_INVALID;
        }
    }

    picture_fifo_Flush(vout->p->decoder_fifo, date, below);
}

static void ThreadReset(vout_thread_t *vout)
{
    ThreadFlush(vout, true, INT64_MAX);
    if (vout->p->decoder_pool)
        picture_pool_NonEmpty(vout->p->decoder_pool, true);
    vout->p->pause.is_on = false;
    vout->p->pause.date  = mdate();
}

static void ThreadStep(vout_thread_t *vout, mtime_t *duration)
{
    *duration = 0;

    if (vout->p->step.last <= VLC_TS_INVALID)
        vout->p->step.last = vout->p->displayed.timestamp;

    mtime_t dummy;
    if (ThreadDisplayPicture(vout, true, &dummy))
        return;

    vout->p->step.timestamp = vout->p->displayed.timestamp;

    if (vout->p->step.last > VLC_TS_INVALID &&
        vout->p->step.timestamp > vout->p->step.last) {
        *duration = vout->p->step.timestamp - vout->p->step.last;
        vout->p->step.last = vout->p->step.timestamp;
        /* TODO advance subpicture by the duration ... */
    }
}

static void ThreadChangeFullscreen(vout_thread_t *vout, bool fullscreen)
{
    vout_SetDisplayFullscreen(vout->p->display.vd, fullscreen);
}

static void ThreadChangeOnTop(vout_thread_t *vout, bool is_on_top)
{
    vout_SetWindowState(vout->p->display.vd,
                        is_on_top ? VOUT_WINDOW_STATE_ABOVE :
                                    VOUT_WINDOW_STATE_NORMAL);
}

static void ThreadChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_SetDisplayFilled(vout->p->display.vd, is_filled);
}

static void ThreadChangeZoom(vout_thread_t *vout, int num, int den)
{
    if (num * 10 < den) {
        num = den;
        den *= 10;
    } else if (num > den * 10) {
        num = den * 10;
    }

    vout_SetDisplayZoom(vout->p->display.vd, num, den);
}

static void ThreadChangeAspectRatio(vout_thread_t *vout,
                                    unsigned num, unsigned den)
{
    vout_SetDisplayAspect(vout->p->display.vd, num, den);
}


static void ThreadExecuteCropWindow(vout_thread_t *vout,
                                    unsigned x, unsigned y,
                                    unsigned width, unsigned height)
{
    vout_SetDisplayCrop(vout->p->display.vd, 0, 0,
                        x, y, width, height);
}
static void ThreadExecuteCropBorder(vout_thread_t *vout,
                                    unsigned left, unsigned top,
                                    unsigned right, unsigned bottom)
{
    msg_Err(vout, "ThreadExecuteCropBorder %d.%d %dx%d", left, top, right, bottom);
    vout_SetDisplayCrop(vout->p->display.vd, 0, 0,
                        left, top, -(int)right, -(int)bottom);
}

static void ThreadExecuteCropRatio(vout_thread_t *vout,
                                   unsigned num, unsigned den)
{
    vout_SetDisplayCrop(vout->p->display.vd, num, den,
                        0, 0, 0, 0);
}

static int ThreadStart(vout_thread_t *vout, const vout_display_state_t *state)
{
    vlc_mouse_Init(&vout->p->mouse);
    vout->p->decoder_fifo = picture_fifo_New();
    vout->p->decoder_pool = NULL;
    vout->p->display_pool = NULL;
    vout->p->private_pool = NULL;

    vout->p->filter.configuration = NULL;
    video_format_Copy(&vout->p->filter.format, &vout->p->original);
    vout->p->filter.chain_static =
        filter_chain_New( vout, "video filter2", true,
                          VoutVideoFilterStaticAllocationSetup, NULL, vout);
    vout->p->filter.chain_interactive =
        filter_chain_New( vout, "video filter2", true,
                          VoutVideoFilterInteractiveAllocationSetup, NULL, vout);

    vout_display_state_t state_default;
    if (!state) {
        VoutGetDisplayCfg(vout, &state_default.cfg, vout->p->display.title);
        state_default.wm_state = var_CreateGetBool(vout, "video-on-top") ? VOUT_WINDOW_STATE_ABOVE :
                                                                           VOUT_WINDOW_STATE_NORMAL;
        state_default.sar.num = 0;
        state_default.sar.den = 0;

        state = &state_default;
    }

    if (vout_OpenWrapper(vout, vout->p->splitter_name, state))
        return VLC_EGENERIC;
    if (vout_InitWrapper(vout))
        return VLC_EGENERIC;
    assert(vout->p->decoder_pool);

    vout->p->displayed.current       = NULL;
    vout->p->displayed.next          = NULL;
    vout->p->displayed.decoded       = NULL;
    vout->p->displayed.date          = VLC_TS_INVALID;
    vout->p->displayed.timestamp     = VLC_TS_INVALID;
    vout->p->displayed.is_interlaced = false;

    vout->p->step.last               = VLC_TS_INVALID;
    vout->p->step.timestamp          = VLC_TS_INVALID;

    vout->p->spu_blend_chroma        = 0;
    vout->p->spu_blend               = NULL;

    video_format_Print(VLC_OBJECT(vout), "original format", &vout->p->original);
    return VLC_SUCCESS;
}

static void ThreadStop(vout_thread_t *vout, vout_display_state_t *state)
{
    if (vout->p->spu_blend)
        filter_DeleteBlend(vout->p->spu_blend);

    /* Destroy translation tables */
    if (vout->p->display.vd) {
        if (vout->p->decoder_pool) {
            ThreadFlush(vout, true, INT64_MAX);
            vout_EndWrapper(vout);
        }
        vout_CloseWrapper(vout, state);
    }

    /* Destroy the video filters2 */
    filter_chain_Delete(vout->p->filter.chain_interactive);
    filter_chain_Delete(vout->p->filter.chain_static);
    video_format_Clean(&vout->p->filter.format);
    free(vout->p->filter.configuration);

    if (vout->p->decoder_fifo)
        picture_fifo_Delete(vout->p->decoder_fifo);
    assert(!vout->p->decoder_pool);
}

static void ThreadInit(vout_thread_t *vout)
{
    vout->p->window.is_unused = true;
    vout->p->window.object    = NULL;
    vout->p->dead             = false;
    vout->p->is_late_dropped  = var_InheritBool(vout, "drop-late-frames");
    vout->p->pause.is_on      = false;
    vout->p->pause.date       = VLC_TS_INVALID;

    vout_chrono_Init(&vout->p->render, 5, 10000); /* Arbitrary initial time */
}

static void ThreadClean(vout_thread_t *vout)
{
    if (vout->p->window.object) {
        assert(vout->p->window.is_unused);
        vout_window_Delete(vout->p->window.object);
    }
    vout_chrono_Clean(&vout->p->render);
    vout->p->dead = true;
    vout_control_Dead(&vout->p->control);
}

static int ThreadReinit(vout_thread_t *vout,
                        const vout_configuration_t *cfg)
{
    video_format_t original;
    if (VoutValidateFormat(&original, cfg->fmt)) {
        ThreadStop(vout, NULL);
        ThreadClean(vout);
        return VLC_EGENERIC;
    }
    /* We ignore crop/ar changes at this point, they are dynamically supported */
    VideoFormatCopyCropAr(&vout->p->original, &original);
    if (video_format_IsSimilar(&original, &vout->p->original)) {
        if (cfg->dpb_size <= vout->p->dpb_size)
            return VLC_SUCCESS;
        msg_Warn(vout, "DPB need to be increased");
    }

    vout_display_state_t state;
    memset(&state, 0, sizeof(state));

    ThreadStop(vout, &state);

    if (!state.cfg.is_fullscreen) {
        state.cfg.display.width  = 0;
        state.cfg.display.height = 0;
    }
    state.sar.num = 0;
    state.sar.den = 0;
    /* FIXME current vout "variables" are not in sync here anymore
     * and I am not sure what to do */

    vout->p->original = original;
    vout->p->dpb_size = cfg->dpb_size;
    if (ThreadStart(vout, &state)) {
        ThreadClean(vout);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
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

    vout_interlacing_support_t interlacing = {
        .is_interlaced = false,
        .date = mdate(),
    };

    mtime_t deadline = VLC_TS_INVALID;
    for (;;) {
        vout_control_cmd_t cmd;

        /* FIXME remove thoses ugly timeouts
         */
        while (!vout_control_Pop(&vout->p->control, &cmd, deadline, 100000)) {
            switch(cmd.type) {
            case VOUT_CONTROL_INIT:
                ThreadInit(vout);
                if (ThreadStart(vout, NULL)) {
                    ThreadStop(vout, NULL);
                    ThreadClean(vout);
                    return NULL;
                }
                break;
            case VOUT_CONTROL_CLEAN:
                ThreadStop(vout, NULL);
                ThreadClean(vout);
                return NULL;
            case VOUT_CONTROL_REINIT:
                if (ThreadReinit(vout, cmd.u.cfg))
                    return NULL;
                break;
            case VOUT_CONTROL_SUBPICTURE:
                ThreadDisplaySubpicture(vout, cmd.u.subpicture);
                cmd.u.subpicture = NULL;
                break;
            case VOUT_CONTROL_FLUSH_SUBPICTURE:
                ThreadFlushSubpicture(vout, cmd.u.integer);
                break;
            case VOUT_CONTROL_OSD_TITLE:
                ThreadDisplayOsdTitle(vout, cmd.u.string);
                break;
            case VOUT_CONTROL_CHANGE_FILTERS:
                ThreadChangeFilters(vout, NULL, cmd.u.string, false);
                break;
            case VOUT_CONTROL_CHANGE_SUB_SOURCES:
                ThreadChangeSubSources(vout, cmd.u.string);
                break;
            case VOUT_CONTROL_CHANGE_SUB_FILTERS:
                ThreadChangeSubFilters(vout, cmd.u.string);
                break;
            case VOUT_CONTROL_CHANGE_SUB_MARGIN:
                ThreadChangeSubMargin(vout, cmd.u.integer);
                break;
            case VOUT_CONTROL_PAUSE:
                ThreadChangePause(vout, cmd.u.pause.is_on, cmd.u.pause.date);
                break;
            case VOUT_CONTROL_FLUSH:
                ThreadFlush(vout, false, cmd.u.time);
                break;
            case VOUT_CONTROL_RESET:
                ThreadReset(vout);
                break;
            case VOUT_CONTROL_STEP:
                ThreadStep(vout, cmd.u.time_ptr);
                break;
            case VOUT_CONTROL_FULLSCREEN:
                ThreadChangeFullscreen(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_ON_TOP:
                ThreadChangeOnTop(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_DISPLAY_FILLED:
                ThreadChangeDisplayFilled(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_ZOOM:
                ThreadChangeZoom(vout, cmd.u.pair.a, cmd.u.pair.b);
                break;
            case VOUT_CONTROL_ASPECT_RATIO:
                ThreadChangeAspectRatio(vout, cmd.u.pair.a, cmd.u.pair.b);
                break;
           case VOUT_CONTROL_CROP_RATIO:
                ThreadExecuteCropRatio(vout, cmd.u.pair.a, cmd.u.pair.b);
                break;
            case VOUT_CONTROL_CROP_WINDOW:
                ThreadExecuteCropWindow(vout,
                                        cmd.u.window.x, cmd.u.window.y,
                                        cmd.u.window.width, cmd.u.window.height);
                break;
            case VOUT_CONTROL_CROP_BORDER:
                ThreadExecuteCropBorder(vout,
                                        cmd.u.border.left,  cmd.u.border.top,
                                        cmd.u.border.right, cmd.u.border.bottom);
                break;
            default:
                break;
            }
            vout_control_cmd_Clean(&cmd);
        }

        ThreadManage(vout, &deadline, &interlacing);
    }
}

