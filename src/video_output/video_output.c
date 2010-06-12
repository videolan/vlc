/*****************************************************************************
 * video_output.c : video output thread
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <libvlc.h>
#include "vout_internal.h"
#include "interlacing.h"
#include "postprocessing.h"
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
#define VOUT_MWAIT_TOLERANCE (INT64_C(1000))

/* */
static int VoutValidateFormat(video_format_t *dst,
                              const video_format_t *src)
{
    if (src->i_width <= 0 || src->i_height <= 0)
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

static vout_thread_t *VoutCreate(vlc_object_t *object,
                                 const vout_configuration_t *cfg)
{
    video_format_t original;
    if (VoutValidateFormat(&original, cfg->fmt))
        return NULL;

    /* Allocate descriptor */
    vout_thread_t *vout = vlc_custom_create(object,
                                            sizeof(*vout) + sizeof(*vout->p),
                                            VLC_OBJECT_VOUT, "video output");
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
    vout->p->i_par_num =
    vout->p->i_par_den = 1;

    vout_snapshot_Init(&vout->p->snapshot);

    /* Initialize locks */
    vlc_mutex_init(&vout->p->picture_lock);
    vlc_mutex_init(&vout->p->vfilter_lock);
    vlc_mutex_init(&vout->p->spu_lock);

    /* Attach the new object now so we can use var inheritance below */
    vlc_object_attach(vout, object);

    /* Initialize subpicture unit */
    vout->p->p_spu = spu_Create(vout);

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit(vout);

    /* Get splitter name if present */
    char *splitter_name = var_GetNonEmptyString(vout, "vout-filter");
    if (splitter_name) {
        if (asprintf(&vout->p->splitter_name, "%s,none", splitter_name) < 0)
            vout->p->splitter_name = NULL;
        free(splitter_name);
    } else {
        vout->p->splitter_name = NULL;
    }

    /* */
    vout_InitInterlacingSupport(vout, vout->p->displayed.is_interlaced);

    /* */
    vlc_object_set_destructor(vout, VoutDestructor);

    /* */
    if (vlc_clone(&vout->p->thread, Thread, vout,
                  VLC_THREAD_PRIORITY_OUTPUT)) {
        spu_Destroy(vout->p->p_spu);
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
        spu_Attach(vout->p->p_spu, vout->p->input, true);

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
                spu_Attach(vout->p->p_spu, vout->p->input, false);
            vout->p->input = cfg->input;
            if (vout->p->input)
                spu_Attach(vout->p->p_spu, vout->p->input, true);
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
        spu_Attach(vout->p->p_spu, vout->p->input, false);

    vout_snapshot_End(&vout->p->snapshot);

    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_CLEAN);
    vlc_join(vout->p->thread, NULL);

    vlc_mutex_lock(&vout->p->spu_lock);
    spu_Destroy(vout->p->p_spu);
    vout->p->p_spu = NULL;
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
    vlc_mutex_destroy(&vout->p->vfilter_lock);
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
    if (vout->p->p_spu)
        channel = spu_RegisterChannel(vout->p->p_spu);
    vlc_mutex_unlock(&vout->p->spu_lock);

    return channel;
}
void vout_FlushSubpictureChannel( vout_thread_t *vout, int channel )
{
    vout_control_PushInteger(&vout->p->control, VOUT_CONTROL_FLUSH_SUBPICTURE,
                             channel);
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
    cmd.u.window.x      = x;
    cmd.u.window.y      = y;
    cmd.u.window.width  = width;
    cmd.u.window.height = height;

    vout_control_Push(&vout->p->control, &cmd);
}
void vout_ControlChangeCropBorder(vout_thread_t *vout,
                                  int left, int top, int right, int bottom)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_CROP_BORDER);
    cmd.u.border.left   = left;
    cmd.u.border.top    = top;
    cmd.u.border.right  = right;
    cmd.u.border.bottom = bottom;

    vout_control_Push(&vout->p->control, &cmd);
}
void vout_ControlChangeFilters(vout_thread_t *vout, const char *filters)
{
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_FILTERS,
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
    cfg->display.sar.num = 1; /* TODO monitor AR */
    cfg->display.sar.den = 1;
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

    vout_window_t *window = vout_window_New(VLC_OBJECT(vout), NULL,
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
static picture_t *VoutVideoFilterNewPicture(filter_t *filter)
{
    vout_thread_t *vout = (vout_thread_t*)filter->p_owner;
    return picture_pool_Get(vout->p->private_pool);
}
static void VoutVideoFilterDelPicture(filter_t *filter, picture_t *picture)
{
    VLC_UNUSED(filter);
    picture_Release(picture);
}
static int VoutVideoFilterAllocationSetup(filter_t *filter, void *data)
{
    filter->pf_video_buffer_new = VoutVideoFilterNewPicture;
    filter->pf_video_buffer_del = VoutVideoFilterDelPicture;
    filter->p_owner             = data; /* vout */
    return VLC_SUCCESS;
}

/* */
static int ThreadDisplayPicture(vout_thread_t *vout,
                                bool now, mtime_t *deadline)
{
    vout_display_t *vd = vout->p->display.vd;
    int displayed_count = 0;
    int lost_count = 0;

    for (;;) {
        const mtime_t date = mdate();
        const bool is_paused = vout->p->pause.is_on;
        bool redisplay = is_paused && !now && vout->p->displayed.decoded;
        bool is_forced;

        /* FIXME/XXX we must redisplay the last decoded picture (because
         * of potential vout updated, or filters update or SPU update)
         * For now a high update period is needed but it coulmd be removed
         * if and only if:
         * - vout module emits events from theselves.
         * - *and* SPU is modified to emit an event or a deadline when needed.
         *
         * So it will be done latter.
         */
        if (!redisplay) {
            picture_t *peek = picture_fifo_Peek(vout->p->decoder_fifo);
            if (peek) {
                is_forced = peek->b_force || is_paused || now;
                *deadline = (is_forced ? date : peek->date) - vout_chrono_GetHigh(&vout->p->render);
                picture_Release(peek);
            } else {
                redisplay = true;
            }
        }
        if (redisplay) {
             /* FIXME a better way for this delay is needed */
            const mtime_t date_update = vout->p->displayed.date + VOUT_REDISPLAY_DELAY;
            if (date_update > date || !vout->p->displayed.decoded) {
                *deadline = vout->p->displayed.decoded ? date_update : VLC_TS_INVALID;
                break;
            }
            /* */
            is_forced = true;
            *deadline = date - vout_chrono_GetHigh(&vout->p->render);
        }
        if (*deadline > VOUT_MWAIT_TOLERANCE)
            *deadline -= VOUT_MWAIT_TOLERANCE;

        /* If we are too early and can wait, do it */
        if (date < *deadline && !now)
            break;

        picture_t *decoded;
        if (redisplay) {
            decoded = vout->p->displayed.decoded;
            vout->p->displayed.decoded = NULL;
        } else {
            decoded = picture_fifo_Pop(vout->p->decoder_fifo);
            assert(decoded);
            if (!is_forced && !vout->p->is_late_dropped) {
                const mtime_t predicted = date + vout_chrono_GetLow(&vout->p->render);
                const mtime_t late = predicted - decoded->date;
                if (late > 0) {
                    msg_Dbg(vout, "picture might be displayed late (missing %d ms)", (int)(late/1000));
                    if (late > VOUT_DISPLAY_LATE_THRESHOLD) {
                        msg_Warn(vout, "rejected picture because of render time");
                        /* TODO */
                        picture_Release(decoded);
                        lost_count++;
                        break;
                    }
                }
            }

            vout->p->displayed.is_interlaced = !decoded->b_progressive;
            vout->p->displayed.qtype         = decoded->i_qtype;
        }
        vout->p->displayed.timestamp = decoded->date;

        /* */
        if (vout->p->displayed.decoded)
            picture_Release(vout->p->displayed.decoded);
        picture_Hold(decoded);
        vout->p->displayed.decoded = decoded;

        /* */
        vout_chrono_Start(&vout->p->render);

        picture_t *filtered = NULL;
        if (decoded) {
            vlc_mutex_lock(&vout->p->vfilter_lock);
            filtered = filter_chain_VideoFilter(vout->p->vfilter_chain, decoded);
            //assert(filtered == decoded); // TODO implement
            vlc_mutex_unlock(&vout->p->vfilter_lock);
            if (!filtered)
                continue;
        }

        /*
         * Check for subpictures to display
         */
        const bool do_snapshot = vout_snapshot_IsRequested(&vout->p->snapshot);
        mtime_t spu_render_time = is_forced ? mdate() : filtered->date;
        if (vout->p->pause.is_on)
            spu_render_time = vout->p->pause.date;
        else
            spu_render_time = filtered->date > 1 ? filtered->date : mdate();

        subpicture_t *subpic = spu_SortSubpictures(vout->p->p_spu,
                                                   spu_render_time,
                                                   do_snapshot);
        /*
         * Perform rendering
         *
         * We have to:
         * - be sure to end up with a direct buffer.
         * - blend subtitles, and in a fast access buffer
         */
        picture_t *direct = NULL;
        if (filtered &&
            (vout->p->decoder_pool != vout->p->display_pool || subpic)) {
            picture_t *render;
            if (vout->p->is_decoder_pool_slow)
                render = picture_NewFromFormat(&vd->source);
            else if (vout->p->decoder_pool != vout->p->display_pool)
                render = picture_pool_Get(vout->p->display_pool);
            else
                render = picture_pool_Get(vout->p->private_pool);

            if (render) {
                picture_Copy(render, filtered);

                spu_RenderSubpictures(vout->p->p_spu,
                                      render, &vd->source,
                                      subpic, &vd->source, spu_render_time);
            }
            if (vout->p->is_decoder_pool_slow) {
                direct = picture_pool_Get(vout->p->display_pool);
                if (direct)
                    picture_Copy(direct, render);
                picture_Release(render);

            } else {
                direct = render;
            }
            picture_Release(filtered);
            filtered = NULL;
        } else {
            direct = filtered;
        }

        /*
         * Take a snapshot if requested
         */
        if (direct && do_snapshot)
            vout_snapshot_Set(&vout->p->snapshot, &vd->source, direct);

        /* Render the direct buffer returned by vout_RenderPicture */
        if (direct) {
            vout_RenderWrapper(vout, direct);

            vout_chrono_Stop(&vout->p->render);
#if 0
            {
            static int i = 0;
            if (((i++)%10) == 0)
                msg_Info(vout, "render: avg %d ms var %d ms",
                         (int)(vout->p->render.avg/1000), (int)(vout->p->render.var/1000));
            }
#endif
        }

        /* Wait the real date (for rendering jitter) */
        if (!is_forced)
            mwait(decoded->date);

        /* Display the direct buffer returned by vout_RenderPicture */
        vout->p->displayed.date = mdate();
        if (direct)
            vout_DisplayWrapper(vout, direct);

        displayed_count++;
        break;
    }

    vout_statistic_Update(&vout->p->statistic, displayed_count, lost_count);
    if (displayed_count <= 0)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ThreadManage(vout_thread_t *vout,
                         mtime_t *deadline,
                         vout_interlacing_support_t *interlacing,
                         vout_postprocessing_support_t *postprocessing)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    *deadline = VLC_TS_INVALID;
    ThreadDisplayPicture(vout, false, deadline);

    const int  picture_qtype      = vout->p->displayed.qtype;
    const bool picture_interlaced = vout->p->displayed.is_interlaced;

    vlc_mutex_unlock(&vout->p->picture_lock);

    /* Post processing */
    vout_SetPostProcessingState(vout, postprocessing, picture_qtype);

    /* Deinterlacing */
    vout_SetInterlacingState(vout, interlacing, picture_interlaced);

    vout_ManageWrapper(vout);
}

static void ThreadDisplaySubpicture(vout_thread_t *vout,
                                    subpicture_t *subpicture)
{
    spu_DisplaySubpicture(vout->p->p_spu, subpicture);
}

static void ThreadFlushSubpicture(vout_thread_t *vout, int channel)
{
    spu_ClearChannel(vout->p->p_spu, channel);
}

static void ThreadDisplayOsdTitle(vout_thread_t *vout, const char *string)
{
    if (!vout->p->title.show)
        return;

    vout_OSDText(vout, SPU_DEFAULT_CHANNEL,
                 vout->p->title.position, INT64_C(1000) * vout->p->title.timeout,
                 string);
}

static void ThreadChangeFilters(vout_thread_t *vout, const char *filters)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, vout->p->original.i_chroma);
    fmt.video = vout->p->original;

    vlc_mutex_lock(&vout->p->vfilter_lock);

    filter_chain_Reset(vout->p->vfilter_chain, &fmt, &fmt);
    if (filter_chain_AppendFromString(vout->p->vfilter_chain,
                                      filters) < 0)
        msg_Err(vout, "Video filter chain creation failed");

    vlc_mutex_unlock(&vout->p->vfilter_lock);
}

static void ThreadChangeSubFilters(vout_thread_t *vout, const char *filters)
{
    spu_ChangeFilters(vout->p->p_spu, filters);
}
static void ThreadChangeSubMargin(vout_thread_t *vout, int margin)
{
    spu_ChangeMargin(vout->p->p_spu, margin);
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

        spu_OffsetSubtitleDate(vout->p->p_spu, duration);
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
    /* FIXME not sure setting "fullscreen" is good ... */
    var_SetBool(vout, "fullscreen", fullscreen);
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
    const video_format_t *source = &vout->p->original;

    if (num > 0 && den > 0) {
        num *= source->i_visible_height;
        den *= source->i_visible_width;
        vlc_ureduce(&num, &den, num, den, 0);
    }
    vout_SetDisplayAspect(vout->p->display.vd, num, den);
}


static void ThreadExecuteCropWindow(vout_thread_t *vout,
                                    unsigned crop_num, unsigned crop_den,
                                    unsigned x, unsigned y,
                                    unsigned width, unsigned height)
{
    const video_format_t *source = &vout->p->original;

    vout_SetDisplayCrop(vout->p->display.vd,
                        crop_num, crop_den,
                        source->i_x_offset + x,
                        source->i_y_offset + y,
                        width, height);
}
static void ThreadExecuteCropBorder(vout_thread_t *vout,
                                    unsigned left, unsigned top,
                                    unsigned right, unsigned bottom)
{
    const video_format_t *source = &vout->p->original;
    ThreadExecuteCropWindow(vout, 0, 0,
                            left,
                            top,
                            /* At worst, it becomes < 0 (but unsigned) and will be rejected */
                            source->i_visible_width  - (left + right),
                            source->i_visible_height - (top  + bottom));
}

static void ThreadExecuteCropRatio(vout_thread_t *vout,
                                   unsigned num, unsigned den)
{
    const video_format_t *source = &vout->p->original;
    ThreadExecuteCropWindow(vout, num, den,
                            0, 0,
                            source->i_visible_width,
                            source->i_visible_height);
}

static int ThreadStart(vout_thread_t *vout, const vout_display_state_t *state)
{
    vlc_mouse_Init(&vout->p->mouse);
    vout->p->decoder_fifo = picture_fifo_New();
    vout->p->decoder_pool = NULL;
    vout->p->display_pool = NULL;
    vout->p->private_pool = NULL;

    vout->p->vfilter_chain =
        filter_chain_New( vout, "video filter2", false,
                          VoutVideoFilterAllocationSetup, NULL, vout);

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

    vout->p->displayed.decoded       = NULL;
    vout->p->displayed.date          = VLC_TS_INVALID;
    vout->p->displayed.decoded       = NULL;
    vout->p->displayed.timestamp     = VLC_TS_INVALID;
    vout->p->displayed.qtype         = QTYPE_NONE;
    vout->p->displayed.is_interlaced = false;

    vout->p->step.last               = VLC_TS_INVALID;
    vout->p->step.timestamp          = VLC_TS_INVALID;

    video_format_Print(VLC_OBJECT(vout), "original format", &vout->p->original);
    return VLC_SUCCESS;
}

static void ThreadStop(vout_thread_t *vout, vout_display_state_t *state)
{
    /* Destroy the video filters2 */
    filter_chain_Delete(vout->p->vfilter_chain);

    /* Destroy translation tables */
    if (vout->p->display.vd) {
        if (vout->p->decoder_pool) {
            ThreadFlush(vout, true, INT64_MAX);
            vout_EndWrapper(vout);
        }
        vout_CloseWrapper(vout, state);
    }

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
    vout_postprocessing_support_t postprocessing = {
        .qtype = QTYPE_NONE,
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
                ThreadChangeFilters(vout, cmd.u.string);
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
                ThreadExecuteCropWindow(vout, 0, 0,
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

        ThreadManage(vout, &deadline, &interlacing, &postprocessing);
    }
}

