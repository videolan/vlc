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

#include <unistd.h>

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

static int32_t video_format_to_adataspace(const video_format_t *fmt)
{
    int32_t standard, transfer, range;

    switch (fmt->primaries) {
        case COLOR_PRIMARIES_BT709:
            standard = ADATASPACE_STANDARD_BT709;
            break;
        case COLOR_PRIMARIES_BT601_625:
            standard = ADATASPACE_STANDARD_BT601_625;
            break;
        case COLOR_PRIMARIES_BT601_525:
            standard = ADATASPACE_STANDARD_BT601_525;
            break;
        case COLOR_PRIMARIES_BT2020:
            standard = ADATASPACE_STANDARD_BT2020;
            break;
        case COLOR_PRIMARIES_DCI_P3:
            standard = ADATASPACE_STANDARD_DCI_P3;
            break;
        default:
            standard = ADATASPACE_STANDARD_UNSPECIFIED;
            break;
    }

    switch (fmt->transfer) {
        case TRANSFER_FUNC_LINEAR:
            transfer = ADATASPACE_TRANSFER_LINEAR;
            break;
        case TRANSFER_FUNC_SRGB:
            transfer = ADATASPACE_TRANSFER_SRGB;
            break;
        case TRANSFER_FUNC_BT709:
            transfer = ADATASPACE_TRANSFER_SMPTE_170M;
            break;
        case TRANSFER_FUNC_SMPTE_ST2084:
            transfer = ADATASPACE_TRANSFER_ST2084;
            break;
        case TRANSFER_FUNC_HLG:
            transfer = ADATASPACE_TRANSFER_HLG;
            break;
        default:
            transfer = ADATASPACE_TRANSFER_UNSPECIFIED;
            break;
    }

    switch (fmt->color_range) {
        case COLOR_RANGE_FULL:
            range = ADATASPACE_RANGE_FULL;
            break;
        case COLOR_RANGE_LIMITED:
            range = ADATASPACE_RANGE_LIMITED;
            break;
        default:
            range = ADATASPACE_RANGE_UNSPECIFIED;
            break;
    }

    return standard | transfer | range;
}

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
    bool can_set_video_layout;
    android_video_context_t *avctx;
    struct subpicture sub;

    struct {
        ASurfaceControl *sc;
        picture_t *previous_picture;
    } asc;
};

static void subpicture_SetDisplaySize(vout_display_t *vd, unsigned width, unsigned height)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;
    vlc_gl_sub_renderer_SetOutputSize(sub->renderer, width, height);
    vlc_gl_Resize(sub->gl, width, height);
    sub->place_changed = true;
}

static int subpicture_Control(vout_display_t *vd, int query)
{
    struct sys *sys = vd->sys;
    struct subpicture *sub = &sys->sub;

    switch (query)
    {
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

    /* If the window/surface size changed, we MUST redraw */
    if (sub->place_changed)
        return true;

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
        .width = vd->source->i_width,
        .height = vd->source->i_height,
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

    vlc_gl_sub_renderer_SetOutputSize(sub->renderer, vd->cfg->display.width,
                                      vd->cfg->display.height);

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

static void ASC_OnComplete(void *context, ASurfaceTransactionStats *stats)
{
    picture_t *pic = context;
    struct android_picture_ctx *apctx =
        container_of(pic->context, struct android_picture_ctx, s);
    vlc_video_context *vctx = picture_GetVideoContext(pic);
    assert(vctx != NULL);
    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_AWINDOW);

    /* See ASurfaceTransactionStats_getPreviousReleaseFenceFd documentation.
     * When buffer n is displayed, this callback is called with the n-1
     * picture, update the read fence fd from the previous transaction
     * (if valid = buffer not yet released) and release the VLC picture. */
    int release_fd =
        avctx->asc_api->ASurfaceTransactionStats.getPreviousReleaseFenceFd(stats, apctx->sc);
    if (release_fd >= 0)
        android_picture_ctx_set_read_fence(apctx, release_fd);
    picture_Release(pic);
}

static void PrepareWithASC(vout_display_t *vd, picture_t *pic,
                           const vlc_render_subpicture *subpicture, vlc_tick_t date)
{
    struct sys *sys = vd->sys;
    assert(sys->asc.sc != NULL);
    assert(pic->context != NULL);
    struct aimage_reader_api *air_api = sys->avctx->air_api;

    picture_context_t *ctx = pic->context;
    struct android_picture_ctx *apctx = container_of(ctx, struct android_picture_ctx, s);
    apctx->sc = sys->asc.sc;

    AHardwareBuffer *buffer = NULL;
    int32_t status = air_api->AImage.getHardwareBuffer(apctx->image, &buffer);
    if (status != 0)
    {
        msg_Warn(vd, "PrepareWithASC: AImage_getHardwareBuffer failed: %d", status);
        return;
    }
    assert(buffer != NULL);

    struct asurface_control_api *asc_api = sys->avctx->asc_api;
    ASurfaceTransaction *txn = asc_api->ASurfaceTransaction.create();
    if (txn == NULL)
        return;

    if (sys->asc.previous_picture != NULL)
    {
        asc_api->ASurfaceTransaction.setOnComplete(txn, sys->asc.previous_picture,
                                                   ASC_OnComplete);
        sys->asc.previous_picture = NULL;
    }

    int fence_fd = android_picture_ctx_get_fence_fd(apctx);

    asc_api->ASurfaceTransaction.setBuffer(txn, sys->asc.sc, buffer, fence_fd);
    asc_api->ASurfaceTransaction.setDesiredPresentTime(txn, NS_FROM_VLC_TICK(date));
    asc_api->ASurfaceTransaction.apply(txn);
    asc_api->ASurfaceTransaction.delete(txn);

    sys->asc.previous_picture = picture_Hold(pic);

    if (sys->sub.window != NULL)
        subpicture_Prepare(vd, subpicture);
}

static void DisplayWithASC(vout_display_t *vd, picture_t *picture)
{
    struct sys *sys = vd->sys;
    assert(picture->context);
    assert(sys->asc.sc != NULL);
    /* Nothing to do for ASC (the display date was set in the transaction) */

    if (sys->sub.window != NULL)
        subpicture_Display(vd);
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    const vlc_render_subpicture *subpicture, vlc_tick_t date)
{
    struct sys *sys = vd->sys;

    assert(picture->context);
    assert(sys->asc.sc == NULL);
    if (sys->avctx->render_ts != NULL)
        sys->avctx->render_ts(picture->context, date);

    if (sys->sub.window != NULL)
        subpicture_Prepare(vd, subpicture);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    struct sys *sys = vd->sys;
    assert(picture->context);
    assert(sys->asc.sc == NULL);

    sys->avctx->render(picture->context);

    if (sys->sub.window != NULL)
        subpicture_Display(vd);
}

static void SetVideoLayout(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    if (!sys->can_set_video_layout)
        return;

    video_format_t rot_fmt;
    video_format_ApplyRotation(&rot_fmt, vd->source);
    AWindowHandler_setVideoLayout(sys->awh, rot_fmt.i_width, rot_fmt.i_height,
                                  rot_fmt.i_visible_width,
                                  rot_fmt.i_visible_height,
                                  rot_fmt.i_sar_num, rot_fmt.i_sar_den);
}

static void UpdateASCGeometry(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    assert(sys->asc.sc != NULL);

    struct asurface_control_api *asc_api = sys->avctx->asc_api;
    ASurfaceTransaction *txn = asc_api->ASurfaceTransaction.create();
    if (txn == NULL)
        return;

    const ARect crop = {
        .left   = vd->source->i_x_offset,
        .top    = vd->source->i_y_offset,
        .right  = vd->source->i_x_offset + vd->source->i_visible_width,
        .bottom = vd->source->i_y_offset + vd->source->i_visible_height,
    };
    asc_api->ASurfaceTransaction.setCrop(txn, sys->asc.sc, &crop);

    asc_api->ASurfaceTransaction.setPosition(txn, sys->asc.sc,
                                             vd->place->x, vd->place->y);

    float x_scale = vd->place->width / (float) vd->source->i_visible_width;
    float y_scale = vd->place->height / (float) vd->source->i_visible_height;
    asc_api->ASurfaceTransaction.setScale(txn, sys->asc.sc, x_scale, y_scale);

    asc_api->ASurfaceTransaction.apply(txn);
    asc_api->ASurfaceTransaction.delete(txn);
}

static int SetDisplaySize(vout_display_t *vd, unsigned width, unsigned height)
{
    struct sys *sys = vd->sys;
    if (sys->sub.window != NULL)
        subpicture_SetDisplaySize(vd, width, height);

    msg_Dbg(vd, "change display size: %dx%d", width, height);
    if (sys->asc.sc != NULL)
        UpdateASCGeometry(vd);
    return VLC_SUCCESS;
}

static int Control(vout_display_t *vd, int query)
{
    struct sys *sys = vd->sys;

    if (sys->sub.window != NULL)
        subpicture_Control(vd, query);

    switch (query) {
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
    {
        if (query == VOUT_DISPLAY_CHANGE_SOURCE_PLACE)
            msg_Dbg(vd, "change source place: %dx%d @ %ux%u",
                    vd->place->x, vd->place->y,
                    vd->place->width, vd->place->height);
        else
            msg_Dbg(vd, "change source crop: %ux%u @ %ux%u aspect: %u/%u",
                    vd->source->i_x_offset, vd->source->i_y_offset,
                    vd->source->i_visible_width,
                    vd->source->i_visible_height,
                    vd->source->i_sar_num,
                    vd->source->i_sar_den);
        if (sys->asc.sc != NULL)
            UpdateASCGeometry(vd);
        else
            SetVideoLayout(vd);
        return VLC_SUCCESS;
    }
    default:
        msg_Warn(vd, "Unknown request in android-display: %d", query);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    struct sys *sys = vd->sys;

    if (sys->can_set_video_layout)
        AWindowHandler_setVideoLayout(sys->awh, 0, 0, 0, 0, 0, 0);

    if (sys->asc.sc != NULL)
    {
        struct asurface_control_api *asc_api = sys->avctx->asc_api;

        if (sys->asc.previous_picture != NULL)
            picture_Release(sys->asc.previous_picture);
        asc_api->ASurfaceControl.release(sys->asc.sc);
    }

    if (sys->sub.window != NULL)
        subpicture_CloseDisplay(vd);

    free(sys);
}

static int CreateSurfaceControl(vout_display_t *vd)
{
    struct sys *sys = vd->sys;
    struct asurface_control_api *asc_api = sys->avctx->asc_api;
    assert(asc_api != NULL); /* If AIR is used, then ASC must be avalaible */

    /* Connect to the SurfaceView */
    ANativeWindow *video = AWindowHandler_getANativeWindow(sys->awh, AWindow_Video);
    if (video == NULL)
        return VLC_EGENERIC;

    ASurfaceControl *sc =
        asc_api->ASurfaceControl.createFromWindow(video, "vlc_video_control");

    if (sc == NULL)
        return VLC_EGENERIC;

    ASurfaceTransaction *txn = asc_api->ASurfaceTransaction.create();
    if (txn == NULL)
    {
        asc_api->ASurfaceControl.release(sc);
        return VLC_EGENERIC;
    }

    asc_api->ASurfaceTransaction.setVisibility(txn, sc,
                    ASURFACE_TRANSACTION_VISIBILITY_SHOW);
    asc_api->ASurfaceTransaction.setBufferTransparency(txn, sc,
                    ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE);

    /* Set colorspace */
    int32_t dataspace = video_format_to_adataspace(vd->source);
    if (dataspace != ADATASPACE_UNKNOWN)
        asc_api->ASurfaceTransaction.setBufferDataSpace(txn, sc, dataspace);

    asc_api->ASurfaceTransaction.apply(txn);
    asc_api->ASurfaceTransaction.delete(txn);

    sys->asc.sc = sc;
    sys->asc.previous_picture = NULL;

    return VLC_SUCCESS;
}

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vlc_window_t *embed = vd->cfg->window;
    AWindowHandler *awh = embed->display.anativewindow;

    if (embed->type != VLC_WINDOW_TYPE_ANDROID_NATIVE
     || fmtp->i_chroma != VLC_CODEC_ANDROID_OPAQUE
     || context == NULL)
        return VLC_EGENERIC;

    bool can_set_video_layout = AWindowHandler_getCapabilities(awh)
                                & AWH_CAPS_SET_VIDEO_LAYOUT;
    if (!vd->obj.force
     && (fmtp->projection_mode != PROJECTION_MODE_RECTANGULAR
      || !can_set_video_layout))
    {
        /* Let the gles2 vout handle projection */
        return VLC_EGENERIC;
    }

    struct sys *sys;
    vd->sys = sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->awh = awh;
    sys->can_set_video_layout = can_set_video_layout;
    sys->asc.sc = NULL;
    sys->avctx = vlc_video_context_GetPrivate(context, VLC_VIDEO_CONTEXT_AWINDOW);
    assert(sys->avctx);

    if (sys->avctx->texture != NULL)
    {
        /* video context configured for opengl */
        free(sys);
        return VLC_EGENERIC;
    }

    if (sys->avctx->air != NULL)
    {
        int ret = CreateSurfaceControl(vd);
        if (ret == VLC_EGENERIC)
        {
            free(sys);
            return VLC_EGENERIC;
        }
        msg_Dbg(vd, "Using new ASurfaceControl");
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

    if (sys->asc.sc != NULL)
        UpdateASCGeometry(vd);
    else
        SetVideoLayout(vd);

    if (sys->asc.sc != NULL)
    {
        static const struct vlc_display_operations ops = {
            .close = Close,
            .prepare = PrepareWithASC,
            .display = DisplayWithASC,
            .set_display_size = SetDisplaySize,
            .control = Control,
        };
        vd->ops = &ops;
    }
    else
    {
        static const struct vlc_display_operations ops = {
            .close = Close,
            .prepare = Prepare,
            .display = Display,
            .set_display_size = SetDisplaySize,
            .control = Control,
        };
        vd->ops = &ops;
    }

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
