/*****************************************************************************
 * display.c: "vout display" management
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_video_splitter.h>
#include <vlc_vout_display.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_modules.h>
#include <vlc_filter.h>
#include <vlc_picture_pool.h>

#include <libvlc.h>

#include "display.h"
#include "window.h"

static void SplitterManage(vout_display_t *vd);
static void SplitterClose(vout_display_t *vd);

/*****************************************************************************
 * FIXME/TODO see how to have direct rendering here (interact with vout.c)
 *****************************************************************************/
static picture_t *VideoBufferNew(filter_t *filter)
{
    vout_display_t *vd = filter->owner.sys;
    const video_format_t *fmt = &filter->fmt_out.video;

    assert(vd->fmt.i_chroma == fmt->i_chroma &&
           vd->fmt.i_width  == fmt->i_width  &&
           vd->fmt.i_height == fmt->i_height);

    picture_pool_t *pool = vout_display_Pool(vd, 3);
    if (!pool)
        return NULL;
    return picture_pool_Get(pool);
}

/*****************************************************************************
 *
 *****************************************************************************/

/**
 * It creates a new vout_display_t using the given configuration.
 */
static vout_display_t *vout_display_New(vlc_object_t *obj,
                                        const char *module, bool load_module,
                                        const video_format_t *fmt,
                                        const vout_display_cfg_t *cfg,
                                        vout_display_owner_t *owner)
{
    /* */
    vout_display_t *vd = vlc_custom_create(obj, sizeof(*vd), "vout display" );

    /* */
    video_format_Copy(&vd->source, fmt);

    /* Picture buffer does not have the concept of aspect ratio */
    video_format_Copy(&vd->fmt, fmt);
    vd->fmt.i_sar_num = 0;
    vd->fmt.i_sar_den = 0;

    vd->info.is_slow = false;
    vd->info.has_double_click = false;
    vd->info.has_pictures_invalid = false;
    vd->info.subpicture_chromas = NULL;

    vd->cfg = cfg;
    vd->pool = NULL;
    vd->prepare = NULL;
    vd->display = NULL;
    vd->control = NULL;
    vd->sys = NULL;

    vd->owner = *owner;

    if (load_module) {
        vd->module = module_need(vd, "vout display", module, module && *module != '\0');
        if (!vd->module) {
            vlc_object_release(vd);
            return NULL;
        }

        vout_window_SetSize(cfg->window,
                            cfg->display.width, cfg->display.height);
    } else {
        vd->module = NULL;
    }

    return vd;
}

/**
 * It deletes a vout_display_t
 */
static void vout_display_Delete(vout_display_t *vd)
{
    if (vd->module)
        module_unneed(vd, vd->module);

    video_format_Clean(&vd->source);
    video_format_Clean(&vd->fmt);

    vlc_object_release(vd);
}

/**
 * It controls a vout_display_t
 */
static int vout_display_Control(vout_display_t *vd, int query, ...)
{
    va_list args;
    int result;

    va_start(args, query);
    result = vd->control(vd, query, args);
    va_end(args);

    return result;
}

/* */
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const vout_display_cfg_t *cfg)
{
    if (cfg->display.width != 0 && cfg->display.height != 0) {
        *width  = cfg->display.width;
        *height = cfg->display.height;
    } else if (cfg->display.width != 0) {
        *width  = cfg->display.width;
        *height = (int64_t)source->i_visible_height * source->i_sar_den * cfg->display.width * cfg->display.sar.num /
            source->i_visible_width / source->i_sar_num / cfg->display.sar.den;
    } else if (cfg->display.height != 0) {
        *width  = (int64_t)source->i_visible_width * source->i_sar_num * cfg->display.height * cfg->display.sar.den /
            source->i_visible_height / source->i_sar_den / cfg->display.sar.num;
        *height = cfg->display.height;
    } else if (source->i_sar_num >= source->i_sar_den) {
        *width  = (int64_t)source->i_visible_width * source->i_sar_num * cfg->display.sar.den / source->i_sar_den / cfg->display.sar.num;
        *height = source->i_visible_height;
    } else {
        *width  = source->i_visible_width;
        *height = (int64_t)source->i_visible_height * source->i_sar_den * cfg->display.sar.num / source->i_sar_num / cfg->display.sar.den;
    }

    *width  = *width  * cfg->zoom.num / cfg->zoom.den;
    *height = *height * cfg->zoom.num / cfg->zoom.den;

    if (ORIENT_IS_SWAP(source->orientation)) {

        unsigned store = *width;
        *width = *height;
        *height = store;
    }
}

/* */
void vout_display_PlacePicture(vout_display_place_t *place,
                               const video_format_t *source,
                               const vout_display_cfg_t *cfg,
                               bool do_clipping)
{
    /* */
    memset(place, 0, sizeof(*place));
    if (cfg->display.width == 0 || cfg->display.height == 0)
        return;

    /* */
    unsigned display_width;
    unsigned display_height;

    video_format_t source_rot;
    video_format_ApplyRotation(&source_rot, source);
    source = &source_rot;

    if (cfg->is_display_filled) {
        display_width  = cfg->display.width;
        display_height = cfg->display.height;
    } else {
        vout_display_cfg_t cfg_tmp = *cfg;

        cfg_tmp.display.width  = 0;
        cfg_tmp.display.height = 0;
        vout_display_GetDefaultDisplaySize(&display_width, &display_height,
                                           source, &cfg_tmp);

        if (do_clipping) {
            display_width  = __MIN(display_width,  cfg->display.width);
            display_height = __MIN(display_height, cfg->display.height);
        }
    }

    const unsigned width  = source->i_visible_width;
    const unsigned height = source->i_visible_height;
    /* Compute the height if we use the width to fill up display_width */
    const int64_t scaled_height = (int64_t)height * display_width  * cfg->display.sar.num * source->i_sar_den / (width  * source->i_sar_num * cfg->display.sar.den);
    /* And the same but switching width/height */
    const int64_t scaled_width  = (int64_t)width  * display_height * cfg->display.sar.den * source->i_sar_num / (height * source->i_sar_den * cfg->display.sar.num);

    if (source->projection_mode == PROJECTION_MODE_RECTANGULAR) {
        /* We keep the solution that avoid filling outside the display */
        if (scaled_width <= cfg->display.width) {
            place->width  = scaled_width;
            place->height = display_height;
        } else {
            place->width  = display_width;
            place->height = scaled_height;
        }
    } else {
        /* No need to preserve an aspect ratio for 360 video.
         * They can fill the display. */
        place->width  = display_width;
        place->height = display_height;
    }

    /*  Compute position */
    switch (cfg->align.horizontal) {
    case VOUT_DISPLAY_ALIGN_LEFT:
        place->x = 0;
        break;
    case VOUT_DISPLAY_ALIGN_RIGHT:
        place->x = cfg->display.width - place->width;
        break;
    default:
        place->x = ((int)cfg->display.width - (int)place->width) / 2;
        break;
    }

    switch (cfg->align.vertical) {
    case VOUT_DISPLAY_ALIGN_TOP:
        place->y = 0;
        break;
    case VOUT_DISPLAY_ALIGN_BOTTOM:
        place->y = cfg->display.height - place->height;
        break;
    default:
        place->y = ((int)cfg->display.height - (int)place->height) / 2;
        break;
    }
}

void vout_display_SendMouseMovedDisplayCoordinates(vout_display_t *vd, video_orientation_t orient_display, int m_x, int m_y, vout_display_place_t *place)
{
    video_format_t source_rot = vd->source;
    video_format_TransformTo(&source_rot, orient_display);

    if (place->width > 0 && place->height > 0) {

        int x = (int)(source_rot.i_x_offset +
                            (int64_t)(m_x - place->x) * source_rot.i_visible_width / place->width);
        int y = (int)(source_rot.i_y_offset +
                            (int64_t)(m_y - place->y) * source_rot.i_visible_height/ place->height);

        video_transform_t transform = video_format_GetTransform(vd->source.orientation, orient_display);

        int store;

        switch (transform) {

            case TRANSFORM_R90:
                store = x;
                x = y;
                y = vd->source.i_visible_height - store;
                break;
            case TRANSFORM_R180:
                x = vd->source.i_visible_width - x;
                y = vd->source.i_visible_height - y;
                break;
            case TRANSFORM_R270:
                store = x;
                x = vd->source.i_visible_width - y;
                y = store;
                break;
            case TRANSFORM_HFLIP:
                x = vd->source.i_visible_width - x;
                break;
            case TRANSFORM_VFLIP:
                y = vd->source.i_visible_height - y;
                break;
            case TRANSFORM_TRANSPOSE:
                store = x;
                x = y;
                y = store;
                break;
            case TRANSFORM_ANTI_TRANSPOSE:
                store = x;
                x = vd->source.i_visible_width - y;
                y = vd->source.i_visible_height - store;
                break;
            default:
                break;
        }

        vout_display_SendEventMouseMoved (vd, x, y);
    }
}

typedef struct {
    vout_thread_t   *vout;
    bool            is_splitter;  /* Is this a video splitter */

    /* */
    vout_display_cfg_t cfg;
    vlc_rational_t sar_initial;

    /* */
#if defined(_WIN32) || defined(__OS2__)
    unsigned width_saved;
    unsigned height_saved;
    bool ch_fullscreen;
    bool is_fullscreen;
    bool ch_wm_state;
    unsigned wm_state;
#endif
    bool ch_sar;
    vlc_rational_t sar;

    bool ch_crop;
    struct {
        int      left;
        int      top;
        int      right;
        int      bottom;
        unsigned num;
        unsigned den;
    } crop;

    /* */
    video_format_t source;
     /* filters to convert the vout source to fmt, NULL means no conversion
      * can be done and nothing will be displayed */
    filter_chain_t *converters;

    /* Lock protecting the variables used by
     * VoutDisplayEvent(ie vout_display_SendEvent) */
    vlc_mutex_t lock;

    /* mouse state */
    struct {
        vlc_mouse_t state;

        vlc_tick_t last_pressed;
    } mouse;

    atomic_bool reset_pictures;
} vout_display_owner_sys_t;

static const struct filter_video_callbacks vout_display_filter_cbs = {
    .buffer_new = VideoBufferNew,
};

static int VoutDisplayCreateRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;
    filter_owner_t owner = {
        .video = &vout_display_filter_cbs,
        .sys = vd,
    };

    osys->converters = filter_chain_NewVideo(vd, false, &owner);
    if (unlikely(osys->converters == NULL))
        return -1;

    video_format_t v_src = vd->source;
    v_src.i_sar_num = 0;
    v_src.i_sar_den = 0;

    video_format_t v_dst = vd->fmt;
    v_dst.i_sar_num = 0;
    v_dst.i_sar_den = 0;

    video_format_t v_dst_cmp = v_dst;
    if ((v_src.i_chroma == VLC_CODEC_J420 && v_dst.i_chroma == VLC_CODEC_I420) ||
        (v_src.i_chroma == VLC_CODEC_J422 && v_dst.i_chroma == VLC_CODEC_I422) ||
        (v_src.i_chroma == VLC_CODEC_J440 && v_dst.i_chroma == VLC_CODEC_I440) ||
        (v_src.i_chroma == VLC_CODEC_J444 && v_dst.i_chroma == VLC_CODEC_I444))
        v_dst_cmp.i_chroma = v_src.i_chroma;

    const bool convert = memcmp(&v_src, &v_dst_cmp, sizeof(v_src)) != 0;
    if (!convert)
        return 0;

    msg_Dbg(vd, "A filter to adapt decoder %4.4s to display %4.4s is needed",
            (const char *)&v_src.i_chroma, (const char *)&v_dst.i_chroma);

    /* */
    es_format_t src;
    es_format_InitFromVideo(&src, &v_src);

    /* */
    int ret;

    for (int i = 0; i < 1 + (v_dst_cmp.i_chroma != v_dst.i_chroma); i++) {
        es_format_t dst;

        es_format_InitFromVideo(&dst, i == 0 ? &v_dst : &v_dst_cmp);

        filter_chain_Reset(osys->converters, &src, &dst);
        ret = filter_chain_AppendConverter(osys->converters, &src, &dst);
        es_format_Clean(&dst);
        if (ret == 0)
            break;
    }
    es_format_Clean(&src);

    if (ret != 0) {
        msg_Err(vd, "Failed to adapt decoder format to display");
        filter_chain_Delete(osys->converters);
        osys->converters = NULL;
    }
    return ret;
}

static void VoutDisplayDestroyRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->converters)
        filter_chain_Delete(osys->converters);
}

static int VoutDisplayResetRender(vout_display_t *vd)
{
    VoutDisplayDestroyRender(vd);
    return VoutDisplayCreateRender(vd);
}

static void VoutDisplayEventMouse(vout_display_t *vd, int event, va_list args)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vlc_mutex_lock(&osys->lock);

    /* */
    vlc_mouse_t m = osys->mouse.state;
    bool is_ignored = false;

    switch (event) {
    case VOUT_DISPLAY_EVENT_MOUSE_MOVED: {
        const int x = (int)va_arg(args, int);
        const int y = (int)va_arg(args, int);

        //msg_Dbg(vd, "VoutDisplayEvent 'mouse' @%d,%d", x, y);

        m.i_x = x;
        m.i_y = y;
        m.b_double_click = false;
        break;
    }
    case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
    case VOUT_DISPLAY_EVENT_MOUSE_RELEASED: {
        const int button = (int)va_arg(args, int);
        const int button_mask = 1 << button;

        /* Ignore inconsistent event */
        if ((event == VOUT_DISPLAY_EVENT_MOUSE_PRESSED  &&  (osys->mouse.state.i_pressed & button_mask)) ||
            (event == VOUT_DISPLAY_EVENT_MOUSE_RELEASED && !(osys->mouse.state.i_pressed & button_mask))) {
            is_ignored = true;
            break;
        }

        /* */
        msg_Dbg(vd, "VoutDisplayEvent 'mouse button' %d t=%d", button, event);

        m.b_double_click = false;
        if (event == VOUT_DISPLAY_EVENT_MOUSE_PRESSED)
            m.i_pressed |= button_mask;
        else
            m.i_pressed &= ~button_mask;
        break;
    }
    case VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK:
        msg_Dbg(vd, "VoutDisplayEvent 'double click'");

        m.b_double_click = true;
        break;
    default:
        vlc_assert_unreachable();
    }

    if (is_ignored) {
        vlc_mutex_unlock(&osys->lock);
        return;
    }

    /* Emulate double-click if needed */
    if (!vd->info.has_double_click &&
        vlc_mouse_HasPressed(&osys->mouse.state, &m, MOUSE_BUTTON_LEFT)) {
        const vlc_tick_t i_date = vlc_tick_now();

        if (i_date - osys->mouse.last_pressed < VLC_TICK_FROM_MS(300) ) {
            m.b_double_click = true;
            osys->mouse.last_pressed = 0;
        } else {
            osys->mouse.last_pressed = vlc_tick_now();
        }
    }

    /* */
    osys->mouse.state = m;

    /* */
    vout_SendDisplayEventMouse(osys->vout, &m);
    vlc_mutex_unlock(&osys->lock);
}

static void VoutDisplayEvent(vout_display_t *vd, int event, va_list args)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    switch (event) {
    case VOUT_DISPLAY_EVENT_MOUSE_MOVED:
    case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
    case VOUT_DISPLAY_EVENT_MOUSE_RELEASED:
    case VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK:
        VoutDisplayEventMouse(vd, event, args);
        break;

    case VOUT_DISPLAY_EVENT_VIEWPOINT_MOVED:
        var_SetAddress(osys->vout, "viewpoint-moved",
                       (void *)va_arg(args, const vlc_viewpoint_t *));
        break;

#if defined(_WIN32) || defined(__OS2__)
    case VOUT_DISPLAY_EVENT_FULLSCREEN: {
        const int is_fullscreen = (int)va_arg(args, int);

        msg_Dbg(vd, "VoutDisplayEvent 'fullscreen' %d", is_fullscreen);

        vlc_mutex_lock(&osys->lock);
        if (!is_fullscreen != !osys->is_fullscreen) {
            osys->ch_fullscreen = true;
            osys->is_fullscreen = is_fullscreen;
        }
        vlc_mutex_unlock(&osys->lock);
        break;
    }

    case VOUT_DISPLAY_EVENT_WINDOW_STATE: {
        const unsigned state = va_arg(args, unsigned);

        msg_Dbg(vd, "VoutDisplayEvent 'window state' %u", state);

        vlc_mutex_lock(&osys->lock);
        if (state != osys->wm_state) {
            osys->ch_wm_state = true;
            osys->wm_state = state;
        }
        vlc_mutex_unlock(&osys->lock);
        break;
    }
#endif

    case VOUT_DISPLAY_EVENT_PICTURES_INVALID: {
        msg_Warn(vd, "VoutDisplayEvent 'pictures invalid'");
        assert(vd->info.has_pictures_invalid);
        atomic_store(&osys->reset_pictures, true);
        break;
    }
    default:
        msg_Err(vd, "VoutDisplayEvent received event %d", event);
        /* TODO add an assert when all event are handled */
        break;
    }
}

static void VoutDisplayCropRatio(int *left, int *top, int *right, int *bottom,
                                 const video_format_t *source,
                                 unsigned num, unsigned den)
{
    unsigned scaled_width  = (uint64_t)source->i_visible_height * num * source->i_sar_den / den / source->i_sar_num;
    unsigned scaled_height = (uint64_t)source->i_visible_width  * den * source->i_sar_num / num / source->i_sar_den;

    if (scaled_width < source->i_visible_width) {
        *left   = (source->i_visible_width - scaled_width) / 2;
        *top    = 0;
        *right  = *left + scaled_width;
        *bottom = *top  + source->i_visible_height;
    } else {
        *left   = 0;
        *top    = (source->i_visible_height - scaled_height) / 2;
        *right  = *left + source->i_visible_width;
        *bottom = *top  + scaled_height;
    }
}

bool vout_ManageDisplay(vout_display_t *vd, bool allow_reset_pictures)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->is_splitter)
        SplitterManage(vd);

#if defined(_WIN32) || defined(__OS2__)
    for (;;) {
        vlc_mutex_lock(&osys->lock);
        bool ch_fullscreen  = osys->ch_fullscreen;
        bool is_fullscreen  = osys->is_fullscreen;
        osys->ch_fullscreen = false;

        bool ch_wm_state  = osys->ch_wm_state;
        unsigned wm_state  = osys->wm_state;
        osys->ch_wm_state = false;
        vlc_mutex_unlock(&osys->lock);

        if (!ch_fullscreen && !ch_wm_state)
            break;

        /* */
        if (ch_fullscreen) {
            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN,
                                     is_fullscreen) == VLC_SUCCESS) {
                osys->cfg.is_fullscreen = is_fullscreen;
            } else
                msg_Err(vd, "Failed to set fullscreen");
        }

        /* */
        if (ch_wm_state
         && vout_display_Control(vd, VOUT_DISPLAY_CHANGE_WINDOW_STATE,
                                 wm_state))
            msg_Err(vd, "Failed to set on top");
    }
#endif

    if (osys->ch_sar) {
        if (osys->sar.num > 0 && osys->sar.den > 0) {
            vd->source.i_sar_num = osys->sar.num;
            vd->source.i_sar_den = osys->sar.den;
        } else {
            vd->source.i_sar_num = osys->source.i_sar_num;
            vd->source.i_sar_den = osys->source.i_sar_den;
        }

        vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT);
        osys->sar.num = vd->source.i_sar_num;
        osys->sar.den = vd->source.i_sar_den;
        osys->ch_sar  = false;

        /* If a crop ratio is requested, recompute the parameters */
        if (osys->crop.num != 0 && osys->crop.den != 0)
            osys->ch_crop = true;
    }

    if (osys->ch_crop) {
        unsigned crop_num = osys->crop.num;
        unsigned crop_den = osys->crop.den;

        if (crop_num != 0 && crop_den != 0) {
            video_format_t fmt = osys->source;
            fmt.i_sar_num = vd->source.i_sar_num;
            fmt.i_sar_den = vd->source.i_sar_den;
            VoutDisplayCropRatio(&osys->crop.left,  &osys->crop.top,
                                 &osys->crop.right, &osys->crop.bottom,
                                 &fmt, crop_num, crop_den);
        }

        const int right_max  = osys->source.i_x_offset + osys->source.i_visible_width;
        const int bottom_max = osys->source.i_y_offset + osys->source.i_visible_height;
        int left   = VLC_CLIP((int)osys->source.i_x_offset + osys->crop.left,
                              0, right_max - 1);
        int top    = VLC_CLIP((int)osys->source.i_y_offset + osys->crop.top,
                              0, bottom_max - 1);
        int right, bottom;

        if (osys->crop.right <= 0)
            right = (int)(osys->source.i_x_offset + osys->source.i_visible_width) + osys->crop.right;
        else
            right = (int)osys->source.i_x_offset + osys->crop.right;
        right = VLC_CLIP(right, left + 1, right_max);
        if (osys->crop.bottom <= 0)
            bottom = (int)(osys->source.i_y_offset + osys->source.i_visible_height) + osys->crop.bottom;
        else
            bottom = (int)osys->source.i_y_offset + osys->crop.bottom;
        bottom = VLC_CLIP(bottom, top + 1, bottom_max);

        vd->source.i_x_offset       = left;
        vd->source.i_y_offset       = top;
        vd->source.i_visible_width  = right - left;
        vd->source.i_visible_height = bottom - top;
        video_format_Print(VLC_OBJECT(vd), "SOURCE ", &osys->source);
        video_format_Print(VLC_OBJECT(vd), "CROPPED", &vd->source);
        vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP);
        osys->crop.left   = left - osys->source.i_x_offset;
        osys->crop.top    = top  - osys->source.i_y_offset;
        /* FIXME for right/bottom we should keep the 'type' border vs window */
        osys->crop.right  = right -
                            (osys->source.i_x_offset + osys->source.i_visible_width);
        osys->crop.bottom = bottom -
                            (osys->source.i_y_offset + osys->source.i_visible_height);
        osys->crop.num    = crop_num;
        osys->crop.den    = crop_den;
        osys->ch_crop = false;
    }

    if (allow_reset_pictures
     && atomic_exchange(&osys->reset_pictures, false)) {
        if (vout_display_Control(vd, VOUT_DISPLAY_RESET_PICTURES)) {
            /* FIXME what to do here ? */
            msg_Err(vd, "Failed to reset pictures (probably fatal)");
        }
        VoutDisplayResetRender(vd);
        return true;
    }

    return false;
}

bool vout_AreDisplayPicturesInvalid(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return atomic_load(&osys->reset_pictures);
}

bool vout_IsDisplayFiltered(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return osys->converters == NULL || !filter_chain_IsEmpty(osys->converters);
}

picture_t *vout_FilterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->converters == NULL) {
        picture_Release(picture);
        return NULL;
    }

    return filter_chain_VideoFilter(osys->converters, picture);
}

void vout_FilterFlush(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->converters != NULL)
        filter_chain_VideoFlush(osys->converters);
}

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *source)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (source->i_sar_num * osys->source.i_sar_den !=
        source->i_sar_den * osys->source.i_sar_num) {

        osys->source.i_sar_num = source->i_sar_num;
        osys->source.i_sar_den = source->i_sar_den;
        vlc_ureduce(&osys->source.i_sar_num, &osys->source.i_sar_den,
                    osys->source.i_sar_num, osys->source.i_sar_den, 0);

        /* FIXME it will override any AR that the user would have forced */
        osys->ch_sar = true;
        osys->sar.num = osys->source.i_sar_num;
        osys->sar.den = osys->source.i_sar_den;
    }
    if (source->i_x_offset       != osys->source.i_x_offset ||
        source->i_y_offset       != osys->source.i_y_offset ||
        source->i_visible_width  != osys->source.i_visible_width ||
        source->i_visible_height != osys->source.i_visible_height) {

        video_format_CopyCrop(&osys->source, source);

        /* Force the vout to reapply the current user crop settings over the new decoder
         * crop settings. */
        osys->ch_crop = true;
    }
}

void vout_SetDisplaySize(vout_display_t *vd, unsigned width, unsigned height)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

#if defined(_WIN32) || defined(__OS2__)
    osys->width_saved  = osys->cfg.display.width;
    osys->height_saved = osys->cfg.display.height;
#endif
    osys->cfg.display.width  = width;
    osys->cfg.display.height = height;
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE, &osys->cfg);
}

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (is_filled == osys->cfg.is_display_filled)
        return; /* nothing to do */

    osys->cfg.is_display_filled = is_filled;
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED, &osys->cfg);
}

void vout_SetDisplayZoom(vout_display_t *vd, unsigned num, unsigned den)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (num != 0 && den != 0) {
        vlc_ureduce(&num, &den, num, den, 0);
    } else {
        num = 1;
        den = 1;
    }

    if (10 * num <= den) {
        num = 1;
        den = 10;
    } else if (num >= 10 * den) {
        num = 10;
        den = 1;
    }

    if (!osys->cfg.is_display_filled
     && osys->cfg.zoom.num == num && osys->cfg.zoom.den == den)
        return; /* nothing to do */

    osys->cfg.zoom.num = num;
    osys->cfg.zoom.den = den;
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ZOOM, &osys->cfg);
}

void vout_SetDisplayAspect(vout_display_t *vd, unsigned dar_num, unsigned dar_den)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    unsigned sar_num, sar_den;
    if (dar_num > 0 && dar_den > 0) {
        sar_num = dar_num * osys->source.i_visible_height;
        sar_den = dar_den * osys->source.i_visible_width;
        vlc_ureduce(&sar_num, &sar_den, sar_num, sar_den, 0);
    } else {
        sar_num = 0;
        sar_den = 0;
    }

    if (osys->sar.num != sar_num || osys->sar.den != sar_den) {
        osys->ch_sar = true;
        osys->sar.num = sar_num;
        osys->sar.den = sar_den;
    }
}
void vout_SetDisplayCrop(vout_display_t *vd,
                         unsigned crop_num, unsigned crop_den,
                         unsigned left, unsigned top, int right, int bottom)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->crop.left  != (int)left  || osys->crop.top != (int)top ||
        osys->crop.right != right || osys->crop.bottom != bottom ||
        (crop_num != 0 && crop_den != 0 &&
         (crop_num != osys->crop.num || crop_den != osys->crop.den))) {

        osys->crop.left   = left;
        osys->crop.top    = top;
        osys->crop.right  = right;
        osys->crop.bottom = bottom;
        osys->crop.num    = crop_num;
        osys->crop.den    = crop_den;

        osys->ch_crop = true;
    }
}

void vout_SetDisplayViewpoint(vout_display_t *vd,
                              const vlc_viewpoint_t *p_viewpoint)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->cfg.viewpoint.yaw   != p_viewpoint->yaw ||
        osys->cfg.viewpoint.pitch != p_viewpoint->pitch ||
        osys->cfg.viewpoint.roll  != p_viewpoint->roll ||
        osys->cfg.viewpoint.fov   != p_viewpoint->fov) {
        vlc_viewpoint_t old_vp = osys->cfg.viewpoint;

        osys->cfg.viewpoint = *p_viewpoint;

        if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_VIEWPOINT,
                                 &osys->cfg)) {
            msg_Err(vd, "Failed to change Viewpoint");
            osys->cfg.viewpoint = old_vp;
        }
    }
}

static vout_display_t *DisplayNew(vout_thread_t *vout,
                                  const video_format_t *source,
                                  const vout_display_state_t *state,
                                  const char *module, bool is_splitter,
                                  const vout_display_owner_t *owner_ptr)
{
    /* */
    vout_display_owner_sys_t *osys = calloc(1, sizeof(*osys));
    vout_display_cfg_t *cfg = &osys->cfg;

    *cfg = state->cfg;
    osys->sar_initial = state->sar;
    vout_display_GetDefaultDisplaySize(&cfg->display.width, &cfg->display.height,
                                       source, cfg);

    osys->vout = vout;
    osys->is_splitter = is_splitter;

    atomic_init(&osys->reset_pictures, false);
    vlc_mutex_init(&osys->lock);

    vlc_mouse_Init(&osys->mouse.state);

#if defined(_WIN32) || defined(__OS2__)
    osys->is_fullscreen  = cfg->is_fullscreen;
    osys->width_saved    = cfg->display.width;
    osys->height_saved   = cfg->display.height;
    if (osys->is_fullscreen) {
        vout_display_cfg_t cfg_windowed = *cfg;
        cfg_windowed.is_fullscreen  = false;
        cfg_windowed.display.width  = 0;
        cfg_windowed.display.height = 0;
        vout_display_GetDefaultDisplaySize(&osys->width_saved,
                                           &osys->height_saved,
                                           source, &cfg_windowed);
    }

    osys->wm_state = state->wm_state;
    osys->ch_wm_state = true;
#endif

    osys->source = *source;
    osys->crop.left   = 0;
    osys->crop.top    = 0;
    osys->crop.right  = 0;
    osys->crop.bottom = 0;
    osys->crop.num = 0;
    osys->crop.den = 0;

    osys->sar.num = osys->sar_initial.num ? osys->sar_initial.num : source->i_sar_num;
    osys->sar.den = osys->sar_initial.den ? osys->sar_initial.den : source->i_sar_den;

    vout_display_owner_t owner;
    if (owner_ptr)
        owner = *owner_ptr;
    else
        owner.event = VoutDisplayEvent;
    owner.sys = osys;

    vout_display_t *p_display = vout_display_New(VLC_OBJECT(vout),
                                                 module, !is_splitter,
                                                 source, cfg, &owner);
    if (!p_display)
        goto error;

    if (VoutDisplayCreateRender(p_display)) {
        vout_display_Delete(p_display);
        goto error;
    }

    /* Setup delayed request */
    if (osys->sar.num != source->i_sar_num ||
        osys->sar.den != source->i_sar_den)
        osys->ch_sar = true;

    var_SetBool(osys->vout, "viewpoint-changeable",
                p_display->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR);

    return p_display;
error:
    vlc_mutex_destroy(&osys->lock);
    free(osys);
    return NULL;
}

void vout_DeleteDisplay(vout_display_t *vd, vout_display_state_t *state)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (state) {
        if (!osys->is_splitter)
            state->cfg = osys->cfg;
#if defined(_WIN32) || defined(__OS2__)
        state->wm_state = osys->wm_state;
#endif
        state->sar = osys->sar_initial;
    }

    VoutDisplayDestroyRender(vd);
    if (osys->is_splitter)
        SplitterClose(vd);
    vout_display_Delete(vd);
    vlc_mutex_destroy(&osys->lock);
    free(osys);
}

/*****************************************************************************
 *
 *****************************************************************************/
vout_display_t *vout_NewDisplay(vout_thread_t *vout,
                                const video_format_t *source,
                                const vout_display_state_t *state,
                                const char *module)
{
    return DisplayNew(vout, source, state, module, false, NULL);
}

/*****************************************************************************
 *
 *****************************************************************************/
struct vout_display_sys_t {
    picture_pool_t   *pool;
    video_splitter_t *splitter;

    /* */
    int            count;
    picture_t      **picture;
    vout_display_t **display;
};
struct video_splitter_owner_t {
    vout_display_t *wrapper;
};

static void SplitterEvent(vout_display_t *vd, int event, va_list args)
{
    //vout_display_owner_sys_t *osys = vd->owner.sys;

    switch (event) {
#if 0
    case VOUT_DISPLAY_EVENT_MOUSE_MOVED:
    case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
    case VOUT_DISPLAY_EVENT_MOUSE_RELEASED:
        /* TODO */
        break;
#endif
    case VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK:
    case VOUT_DISPLAY_EVENT_PICTURES_INVALID:
        VoutDisplayEvent(vd, event, args);
        break;

    default:
        msg_Err(vd, "splitter event not implemented: %d", event);
        break;
    }
}

static picture_pool_t *SplitterPool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}
static void SplitterPrepare(vout_display_t *vd,
                            picture_t *picture,
                            subpicture_t *subpicture, vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;

    picture_Hold(picture);
    assert(!subpicture);

    if (video_splitter_Filter(sys->splitter, sys->picture, picture)) {
        for (int i = 0; i < sys->count; i++)
            sys->picture[i] = NULL;
        return;
    }

    for (int i = 0; i < sys->count; i++) {
        sys->picture[i] = vout_FilterDisplay(sys->display[i], sys->picture[i]);
        if (sys->picture[i])
            vout_display_Prepare(sys->display[i], sys->picture[i], NULL, date);
    }
}
static void SplitterDisplay(vout_display_t *vd,
                            picture_t *picture,
                            subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    assert(!subpicture);
    for (int i = 0; i < sys->count; i++) {
        if (sys->picture[i])
            vout_display_Display(sys->display[i], sys->picture[i], NULL);
    }
    picture_Release(picture);
}
static int SplitterControl(vout_display_t *vd, int query, va_list args)
{
    (void)vd; (void)query; (void)args;
    return VLC_EGENERIC;
}
static void SplitterManage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    for (int i = 0; i < sys->count; i++)
        vout_ManageDisplay(sys->display[i], true);
}

static int SplitterPictureNew(video_splitter_t *splitter, picture_t *picture[])
{
    vout_display_sys_t *wsys = splitter->p_owner->wrapper->sys;

    for (int i = 0; i < wsys->count; i++) {
        if (vout_IsDisplayFiltered(wsys->display[i])) {
            /* TODO use a pool ? */
            picture[i] = picture_NewFromFormat(&wsys->display[i]->source);
        } else {
            picture_pool_t *pool = vout_display_Pool(wsys->display[i], 3);
            picture[i] = pool ? picture_pool_Get(pool) : NULL;
        }
        if (!picture[i]) {
            for (int j = 0; j < i; j++)
                picture_Release(picture[j]);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}
static void SplitterPictureDel(video_splitter_t *splitter, picture_t *picture[])
{
    vout_display_sys_t *wsys = splitter->p_owner->wrapper->sys;

    for (int i = 0; i < wsys->count; i++)
        picture_Release(picture[i]);
}
static void SplitterClose(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    video_splitter_t *splitter = sys->splitter;
    free(splitter->p_owner);
    video_splitter_Delete(splitter);

    if (sys->pool)
        picture_pool_Release(sys->pool);

    /* */
    for (int i = 0; i < sys->count; i++) {
        vout_window_t *wnd = sys->display[i]->cfg->window;

        vout_DeleteDisplay(sys->display[i], NULL);
        vout_display_window_Delete(wnd);
    }
    TAB_CLEAN(sys->count, sys->display);
    free(sys->picture);

    free(sys);
}

vout_display_t *vout_NewSplitter(vout_thread_t *vout,
                                 const video_format_t *source,
                                 const vout_display_state_t *state,
                                 const char *module,
                                 const char *splitter_module)
{
    video_splitter_t *splitter =
        video_splitter_New(VLC_OBJECT(vout), splitter_module, source);
    if (!splitter)
        return NULL;

    /* */
    vout_display_t *wrapper =
        DisplayNew(vout, source, state, module, true, NULL);
    if (!wrapper) {
        video_splitter_Delete(splitter);
        return NULL;
    }
    vout_display_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        abort();
    sys->picture = calloc(splitter->i_output, sizeof(*sys->picture));
    if (!sys->picture )
        abort();
    sys->splitter = splitter;
    sys->pool     = NULL;

    wrapper->pool    = SplitterPool;
    wrapper->prepare = SplitterPrepare;
    wrapper->display = SplitterDisplay;
    wrapper->control = SplitterControl;
    wrapper->sys     = sys;

    /* */
    video_splitter_owner_t *vso = xmalloc(sizeof(*vso));
    vso->wrapper = wrapper;
    splitter->p_owner = vso;
    splitter->pf_picture_new = SplitterPictureNew;
    splitter->pf_picture_del = SplitterPictureDel;

    /* */
    TAB_INIT(sys->count, sys->display);
    for (int i = 0; i < splitter->i_output; i++) {
        vout_display_owner_t vdo = {
            .event      = SplitterEvent,
        };
        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_display_state_t ostate;
        vout_window_cfg_t cfg = {
            .width = state->cfg.display.width,
            .height = state->cfg.display.height,
            .is_standalone = true,
            .is_decorated = true,
        };

        memset(&ostate, 0, sizeof(ostate));
        ostate.cfg.display = state->cfg.display;
        ostate.cfg.align.horizontal = 0; /* TODO */
        ostate.cfg.align.vertical = 0; /* TODO */
        ostate.cfg.is_display_filled = true;
        ostate.cfg.zoom.num = 1;
        ostate.cfg.zoom.den = 1;
        vout_display_GetDefaultDisplaySize(&cfg.width, &cfg.height,
                                           source, &ostate.cfg);
        ostate.cfg.window = vout_display_window_New(vout, &cfg);
        if (unlikely(ostate.cfg.window == NULL)) {
            vout_DeleteDisplay(wrapper, NULL);
            return NULL;
        }

        vout_display_t *vd = DisplayNew(vout, &output->fmt, &ostate,
                                        output->psz_module ? output->psz_module : module,
                                        false, &vdo);
        if (!vd) {
            vout_DeleteDisplay(wrapper, NULL);
            if (ostate.cfg.window != NULL)
                vout_display_window_Delete(ostate.cfg.window);
            return NULL;
        }
        TAB_APPEND(sys->count, sys->display, vd);
    }

    return wrapper;
}

/*****************************************************************************
 * TODO move out
 *****************************************************************************/
#include "vout_internal.h"

void vout_SendDisplayEventMouse(vout_thread_t *vout, const vlc_mouse_t *m)
{
    vlc_mouse_t tmp1, tmp2;

    /* The check on spu is needed as long as ALLOW_DUMMY_VOUT is defined */
    if (vout->p->spu && spu_ProcessMouse( vout->p->spu, m, &vout->p->display.vd->source))
        return;

    vlc_mutex_lock( &vout->p->filter.lock );
    if (vout->p->filter.chain_static && vout->p->filter.chain_interactive) {
        if (!filter_chain_MouseFilter(vout->p->filter.chain_interactive, &tmp1, m))
            m = &tmp1;
        if (!filter_chain_MouseFilter(vout->p->filter.chain_static,      &tmp2, m))
            m = &tmp2;
    }
    vlc_mutex_unlock( &vout->p->filter.lock );

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
        vout->p->mouse_event(m, vout->p->opaque);
}
