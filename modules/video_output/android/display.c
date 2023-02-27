/*****************************************************************************
 * display.c: Android video output module
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Felix Abecassis <felix.abecassis@gmail.com>
 *          Ming Hu <tewilove@gmail.com>
 *          Ludovic Fauvet <etix@l0cal.com>
 *          Sébastien Toque <xilasz@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <vlc_opengl.h>
#include "utils.h"

struct sys
{
    AWindowHandler *awh;
    android_video_context_t *avctx;
    video_format_t fmt;
};

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
{
    struct sys *sys = vd->sys;

    assert(picture->context);
    if (sys->avctx->render_ts != NULL)
    {
        vlc_tick_t now = vlc_tick_now();
        if (date > now)
        {
            if (date - now <= VLC_TICK_FROM_SEC(1))
                sys->avctx->render_ts(picture->context, date);
            else /* The picture will be displayed from the Display callback */
                msg_Warn(vd, "picture way too early to release at time");
        }
    }
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    struct sys *sys = vd->sys;
    assert(picture->context);
    sys->avctx->render(picture->context);
}

static int Control(vout_display_t *vd, int query)
{
    struct sys *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        msg_Dbg(vd, "change source crop: %ux%u @ %ux%u",
                vd->source->i_x_offset, vd->source->i_y_offset,
                vd->source->i_visible_width,
                vd->source->i_visible_height);

        video_format_CopyCrop(&sys->fmt, vd->source);

        video_format_t rot_fmt;
        video_format_ApplyRotation(&rot_fmt, &sys->fmt);
        AWindowHandler_setVideoLayout(sys->awh, 0, 0,
                                      rot_fmt.i_visible_width,
                                      rot_fmt.i_visible_height,
                                      0, 0);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    {
        msg_Dbg(vd, "change source aspect: %d/%d", vd->source->i_sar_num,
                vd->source->i_sar_den);

        sys->fmt.i_sar_num = vd->source->i_sar_num;
        sys->fmt.i_sar_den = vd->source->i_sar_den;
        video_format_t rot_fmt;
        video_format_ApplyRotation(&rot_fmt, &sys->fmt);
        if (rot_fmt.i_sar_num != 0 && rot_fmt.i_sar_den != 0)
            AWindowHandler_setVideoLayout(sys->awh, 0, 0, 0, 0,
                                          rot_fmt.i_sar_num, rot_fmt.i_sar_den);

        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    {
        msg_Dbg(vd, "change display size: %dx%d", vd->cfg->display.width,
                                                  vd->cfg->display.height);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        return VLC_SUCCESS;
    default:
        msg_Warn(vd, "Unknown request in android-display: %d", query);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    struct sys *sys = vd->sys;

    AWindowHandler_setVideoLayout(sys->awh, 0, 0, 0, 0, 0, 0);

    free(sys);
}

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vlc_window_t *embed = vd->cfg->window;
    if (embed->type != VLC_WINDOW_TYPE_ANDROID_NATIVE
     || fmtp->i_chroma != VLC_CODEC_ANDROID_OPAQUE
     || context == NULL)
        return VLC_EGENERIC;

    struct sys *sys;
    vd->sys = sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    video_format_ApplyRotation(&sys->fmt, fmtp);

    sys->awh = embed->display.anativewindow;
    sys->avctx = vlc_video_context_GetPrivate(context, VLC_VIDEO_CONTEXT_AWINDOW);
    assert(sys->avctx);
    if (sys->avctx->texture != NULL)
    {
        /* video context configured for opengl */
        free(sys);
        return VLC_EGENERIC;
    }

    video_format_t rot_fmt;
    video_format_ApplyRotation(&rot_fmt, &sys->fmt);

    AWindowHandler_setVideoLayout(sys->awh, rot_fmt.i_width, rot_fmt.i_height,
                                  rot_fmt.i_visible_width,
                                  rot_fmt.i_visible_height,
                                  rot_fmt.i_sar_num, rot_fmt.i_sar_den);

    static const struct vlc_display_operations ops = {
        .close = Close,
        .prepare = Prepare,
        .display = Display,
        .control = Control,
        .set_viewpoint = NULL,
    };

    vd->ops = &ops;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description("Android video output")
    add_shortcut("android-display")
    add_obsolete_string("android-display-chroma") /* since 4.0.0 */
    set_callback_display(Open, 260)
vlc_module_end()
