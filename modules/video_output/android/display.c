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
#include <vlc_threads.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_subpicture.h>

#include <vlc_vector.h>
#include <vlc_opengl.h>
#include "utils.h"
#include "../opengl/gl_api.h"
#include "../opengl/sub_renderer.h"

struct subpicture
{
    vlc_window_t *window;
    vlc_gl_t *gl;
    struct vlc_gl_api api;
    struct vlc_gl_interop *interop;
    struct vlc_gl_sub_renderer *renderer;
    bool place_changed;
    bool is_dirty;
    bool clear;

    int64_t last_order;
    struct VLC_VECTOR(vout_display_place_t) regions;

    struct {
        PFNGLFLUSHPROC Flush;
    } vt;
};

struct sys
{
    AWindowHandler *awh;
    android_video_context_t *avctx;
    video_format_t fmt;
    struct subpicture sub;
};

static int subpicture_Control(vout_display_t *vd, int query)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        vlc_gl_Resize(sub->gl, vd->cfg->display.width, vd->cfg->display.height);
        // fallthrough
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
    {
        sub->place_changed = true;
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        return VLC_SUCCESS;
    default:
        break;
    }
    return VLC_EGENERIC;
}

static bool subpicture_NeedDraw(vout_display_t *vd,
                                const vlc_render_subpicture *subpicture)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    if (subpicture == NULL)
    {
        if (!sub->clear)
            return false;
        sub->clear = false;
        /* Need to draw one last time in order to clear the current subpicture */
        return true;
    }

    sub->clear = true;

    size_t count = subpicture->regions.size;
    const struct subpicture_region_rendered *r;

    if (subpicture->i_order != sub->last_order)
    {
        sub->last_order = subpicture->i_order;
        /* Subpicture content is different */
        goto end;
    }

    bool draw = false;

    if (count == sub->regions.size)
    {
        size_t i = 0;
        vlc_vector_foreach(r, &subpicture->regions)
        {
            vout_display_place_t *cmp = &sub->regions.data[i++];
            if (!vout_display_PlaceEquals(cmp, &r->place))
            {
                /* Subpicture regions are different */
                draw = true;
                break;
            }
        }
    }
    else
    {
        /* Subpicture region count is different */
        draw = true;
    }

    if (!draw)
        return false;

end:
    /* Store the current subpicture regions in order to compare then later.
     */
    if (!vlc_vector_reserve(&sub->regions, count))
        return false;

    sub->regions.size = 0;

    vlc_vector_foreach(r, &subpicture->regions)
    {
        bool res = vlc_vector_push(&sub->regions, r->place);
        /* Already checked with vlc_vector_reserve */
        assert(res); (void) res;
    }

    return true;
}

static void subpicture_Prepare(vout_display_t *vd, const vlc_render_subpicture *subpicture)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    if (!subpicture_NeedDraw(vd, subpicture))
    {
        sub->is_dirty = false;
        return;
    }

    if (vlc_gl_MakeCurrent(sub->gl) != VLC_SUCCESS)
        return;

    sub->api.vt.ClearColor(0.f, 0.f, 0.f, 0.f);
    sub->api.vt.Clear(GL_COLOR_BUFFER_BIT);

    int ret = vlc_gl_sub_renderer_Prepare(sub->renderer, subpicture);
    if (ret != VLC_SUCCESS)
        goto error;
    sub->vt.Flush();

    if (sub->place_changed)
    {
        sub->api.vt.Viewport(0, 0,
                             vd->cfg->display.width, vd->cfg->display.height);
        sub->place_changed = false;
    }

    ret = vlc_gl_sub_renderer_Draw(sub->renderer);
    if (ret != VLC_SUCCESS)
        goto error;
    sub->vt.Flush();

    sub->is_dirty = true;
error:
    vlc_gl_ReleaseCurrent(sub->gl);
}

static void subpicture_Display(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    if (sub->is_dirty)
        vlc_gl_Swap(sub->gl);
}

static void subpicture_CloseDisplay(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    int ret = vlc_gl_MakeCurrent(sub->gl);

    if (ret == 0)
    {
        /* Clear the surface */
        sub->api.vt.ClearColor(0.f, 0.f, 0.f, 0.f);
        sub->api.vt.Clear(GL_COLOR_BUFFER_BIT);
        vlc_gl_Swap(sub->gl);
    }

    vlc_gl_sub_renderer_Delete(sub->renderer);
    vlc_gl_interop_Delete(sub->interop);

    vlc_gl_ReleaseCurrent(sub->gl);

    vlc_gl_Delete(sub->gl);

    vlc_window_Disable(sub->window);
    vlc_window_Delete(sub->window);

    vlc_vector_destroy(&sub->regions);
}

static void subpicture_window_Resized(struct vlc_window *wnd, unsigned width,
                                    unsigned height, vlc_window_ack_cb cb,
                                    void *opaque)
{
    if (cb != NULL)
        cb(wnd, width, height, opaque);
}

static int subpicture_window_Open(vlc_window_t *wnd)
{
    static const struct vlc_window_operations ops = {
    };

    wnd->type = VLC_WINDOW_TYPE_ANDROID_NATIVE;
    wnd->display.anativewindow = wnd->owner.sys;
    wnd->handle.android_id = AWindow_Subtitles;
    wnd->ops = &ops;
    return VLC_SUCCESS;
}

static int subpicture_OpenDisplay(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    sub->is_dirty = false;
    sub->clear = false;
    sub->last_order = -1;
    vlc_vector_init(&sub->regions);

    /* Create a VLC sub window that will hold the subpicture surface */
    static const struct vlc_window_callbacks win_cbs = {
        .resized = subpicture_window_Resized,
    };

    const vlc_window_cfg_t win_cfg = {
        .is_fullscreen = true,
        .is_decorated = false,
        .width = sys->fmt.i_width,
        .height = sys->fmt.i_height,
    };

    const vlc_window_owner_t win_owner = {
        .cbs = &win_cbs,
        .sys = vd->cfg->window->display.anativewindow,
    };

    sub->window = vlc_window_New(VLC_OBJECT(vd), "android-subpicture",
                                 &win_owner, &win_cfg);
    if (sub->window == NULL)
        return -1;

    if (vlc_window_Enable(sub->window) != 0)
        goto delete_win;

    /* Create the OpenGLES2 context on the subpicture window/surface */
    vout_display_cfg_t vd_cfg = *vd->cfg;
    vd_cfg.window = sub->window;

    struct vlc_gl_cfg gl_cfg = { .need_alpha = true };
    sub->gl = vlc_gl_Create(&vd_cfg, VLC_OPENGL_ES2, NULL, &gl_cfg);
    if (sub->gl == NULL)
        goto disable_win;

    /* Initialize and configure subpicture renderer/interop */
    sub->place_changed = true;
    vlc_gl_Resize(sub->gl, vd->cfg->display.width, vd->cfg->display.height);

    if (vlc_gl_MakeCurrent(sub->gl))
        goto delete_gl;

    sub->vt.Flush = vlc_gl_GetProcAddress(sub->gl, "glFlush");
    if (sub->vt.Flush == NULL)
        goto release_gl;

    int ret = vlc_gl_api_Init(&sub->api, sub->gl);
    if (ret != VLC_SUCCESS)
        goto release_gl;

    sub->interop = vlc_gl_interop_NewForSubpictures(sub->gl);
    if (sub->interop == NULL)
    {
        msg_Err(vd, "Could not create sub interop");
        goto release_gl;
    }

    sub->renderer = vlc_gl_sub_renderer_New(sub->gl, &sub->api, sub->interop);
    if (sub->renderer == NULL)
        goto delete_interop;

    vlc_gl_ReleaseCurrent(sub->gl);

    static const vlc_fourcc_t gl_subpicture_chromas[] = {
        VLC_CODEC_RGBA,
        0
    };
    vd->info.subpicture_chromas = gl_subpicture_chromas;

    return 0;

delete_interop:
    vlc_gl_interop_Delete(sub->interop);
release_gl:
    vlc_gl_ReleaseCurrent(sub->gl);
delete_gl:
    vlc_gl_Delete(sub->gl);
disable_win:
    vlc_window_Disable(sub->window);
delete_win:
    vlc_window_Delete(sub->window);
    sub->window = NULL;
    return -1;
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    const vlc_render_subpicture *subpicture, vlc_tick_t date)
{
    struct sys *sys = vd->sys;

    assert(picture->context);
    if (sys->avctx->render_ts != NULL)
        sys->avctx->render_ts(picture->context, date);

    if (sys->sub.window != NULL)
        subpicture_Prepare(vd, subpicture);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    struct sys *sys = vd->sys;
    assert(picture->context);
    sys->avctx->render(picture->context);

    if (sys->sub.window != NULL)
        subpicture_Display(vd);
}

static int Control(vout_display_t *vd, int query)
{
    struct sys *sys = vd->sys;

    if (sys->sub.window != NULL)
        subpicture_Control(vd, query);

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
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
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

    if (sys->sub.window != NULL)
        subpicture_CloseDisplay(vd);

    free(sys);
}

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vlc_window_t *embed = vd->cfg->window;
    AWindowHandler *awh = embed->display.anativewindow;

    if (embed->type != VLC_WINDOW_TYPE_ANDROID_NATIVE
     || fmtp->i_chroma != VLC_CODEC_ANDROID_OPAQUE
     || context == NULL
     || !AWindowHandler_canSetVideoLayout(awh))
        return VLC_EGENERIC;

    if (!vd->obj.force && fmtp->projection_mode != PROJECTION_MODE_RECTANGULAR)
    {
        /* Let the gles2 vout handle projection */
        return VLC_EGENERIC;
    }

    struct sys *sys;
    vd->sys = sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    video_format_ApplyRotation(&sys->fmt, fmtp);

    sys->awh = awh;
    sys->avctx = vlc_video_context_GetPrivate(context, VLC_VIDEO_CONTEXT_AWINDOW);
    assert(sys->avctx);
    if (sys->avctx->texture != NULL)
    {
        /* video context configured for opengl */
        free(sys);
        return VLC_EGENERIC;
    }

    const bool has_subtitle_surface =
        AWindowHandler_getANativeWindow(sys->awh, AWindow_Subtitles) != NULL;
    if (has_subtitle_surface)
    {
        int ret = subpicture_OpenDisplay(vd);
        if (ret != 0 && !vd->obj.force)
        {
            msg_Warn(vd, "cannot blend subtitle with an opaque surface, "
                         "trying next vout");
            free(sys);
            return VLC_EGENERIC;
        }
    }
    else
    {
        msg_Warn(vd, "using android display without subtitles support");
        sys->sub.window = NULL;
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
    set_callback_display(Open, 280)
    add_submodule ()
        set_capability("vout window", 0)
        set_callback(subpicture_window_Open)
        add_shortcut("android-subpicture")
vlc_module_end()
