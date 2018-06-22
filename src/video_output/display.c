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

#include "event.h"

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
    vd->info.needs_hide_mouse = false;
    vd->info.has_pictures_invalid = false;
    vd->info.subpicture_chromas = NULL;

    vd->cfg = cfg;
    vd->pool = NULL;
    vd->prepare = NULL;
    vd->display = NULL;
    vd->control = NULL;
    vd->manage = NULL;
    vd->sys = NULL;

    vd->owner = *owner;

    if (load_module) {
        vd->module = module_need(vd, "vout display", module, module && *module != '\0');
        if (!vd->module) {
            vlc_object_release(vd);
            return NULL;
        }
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

static void vout_display_Manage(vout_display_t *vd)
{
    if (vd->manage)
        vd->manage(vd);
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
    bool is_display_filled;

    bool ch_zoom;
    vlc_rational_t zoom;
#if defined(_WIN32) || defined(__OS2__)
    unsigned width_saved;
    unsigned height_saved;
    bool ch_fullscreen;
    bool is_fullscreen;
    bool window_fullscreen;
    bool ch_wm_state;
    unsigned wm_state;
    unsigned wm_state_initial;
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

    bool ch_viewpoint;
    vlc_viewpoint_t viewpoint;
    vlc_viewpoint_t display_viewpoint;

    /* */
    video_format_t source;
    filter_chain_t *filters;

    /* Lock protecting the variables used by
     * VoutDisplayEvent(ie vout_display_SendEvent) */
    vlc_mutex_t lock;

    /* mouse state */
    struct {
        vlc_mouse_t state;

        vlc_tick_t last_pressed;
        vlc_tick_t last_moved;
        bool    is_hidden;
        bool    ch_activity;

        /* */
        vlc_tick_t double_click_timeout;
        vlc_tick_t hide_timeout;
    } mouse;

    bool reset_pictures;

    signed char fit_window;

    bool ch_display_size;
    int  display_width;
    int  display_height;
} vout_display_owner_sys_t;

static int VoutDisplayCreateRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;
    filter_owner_t owner = {
        .sys = vd,
        .video = {
            .buffer_new = VideoBufferNew,
        },
    };

    osys->filters = filter_chain_NewVideo(vd, false, &owner);
    if (unlikely(osys->filters == NULL))
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

        filter_chain_Reset(osys->filters, &src, &dst);
        ret = filter_chain_AppendConverter(osys->filters, &src, &dst);
        es_format_Clean(&dst);
        if (ret == 0)
            break;
    }
    es_format_Clean(&src);

    if (ret != 0) {
        msg_Err(vd, "Failed to adapt decoder format to display");
        filter_chain_Delete(osys->filters);
        osys->filters = NULL;
    }
    return ret;
}

static void VoutDisplayDestroyRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->filters)
        filter_chain_Delete(osys->filters);
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
    case VOUT_DISPLAY_EVENT_MOUSE_STATE: {
        const int x = (int)va_arg(args, int);
        const int y = (int)va_arg(args, int);
        const int button_mask = (int)va_arg(args, int);

        vlc_mouse_Init(&m);
        m.i_x = x;
        m.i_y = y;
        m.i_pressed = button_mask;
        break;
    }
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
        const vlc_tick_t i_date = mdate();

        if (i_date - osys->mouse.last_pressed < osys->mouse.double_click_timeout ) {
            m.b_double_click = true;
            osys->mouse.last_pressed = 0;
        } else {
            osys->mouse.last_pressed = mdate();
        }
    }

    /* */
    osys->mouse.state = m;

    /* */
    osys->mouse.ch_activity = true;
    osys->mouse.last_moved = mdate();

    /* */
    vout_SendDisplayEventMouse(osys->vout, &m);
    vlc_mutex_unlock(&osys->lock);
}

static void VoutDisplayEvent(vout_display_t *vd, int event, va_list args)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    switch (event) {
    case VOUT_DISPLAY_EVENT_CLOSE: {
        msg_Dbg(vd, "VoutDisplayEvent 'close'");
        vout_SendEventClose(osys->vout);
        break;
    }
    case VOUT_DISPLAY_EVENT_KEY: {
        const int key = (int)va_arg(args, int);
        msg_Dbg(vd, "VoutDisplayEvent 'key' 0x%2.2x", key);
        vout_SendEventKey(osys->vout, key);
        break;
    }
    case VOUT_DISPLAY_EVENT_MOUSE_STATE:
    case VOUT_DISPLAY_EVENT_MOUSE_MOVED:
    case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
    case VOUT_DISPLAY_EVENT_MOUSE_RELEASED:
    case VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK:
        VoutDisplayEventMouse(vd, event, args);
        break;

    case VOUT_DISPLAY_EVENT_VIEWPOINT_MOVED:
        vout_SendEventViewpointMoved(osys->vout,
                                     va_arg(args, const vlc_viewpoint_t *));
        break;

#if defined(_WIN32) || defined(__OS2__)
    case VOUT_DISPLAY_EVENT_FULLSCREEN: {
        const int is_fullscreen = (int)va_arg(args, int);
        const bool window_fullscreen = va_arg(args, int);

        msg_Dbg(vd, "VoutDisplayEvent 'fullscreen' %d", is_fullscreen);

        vlc_mutex_lock(&osys->lock);
        if (!is_fullscreen != !osys->is_fullscreen) {
            osys->ch_fullscreen = true;
            osys->is_fullscreen = is_fullscreen;
            osys->window_fullscreen = window_fullscreen;
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
    case VOUT_DISPLAY_EVENT_DISPLAY_SIZE: {
        const int width  = (int)va_arg(args, int);
        const int height = (int)va_arg(args, int);
        msg_Dbg(vd, "VoutDisplayEvent 'resize' %dx%d", width, height);

        /* */
        vlc_mutex_lock(&osys->lock);

        osys->ch_display_size   = true;
        osys->display_width     = width;
        osys->display_height    = height;

        vlc_mutex_unlock(&osys->lock);
        break;
    }

    case VOUT_DISPLAY_EVENT_PICTURES_INVALID: {
        msg_Warn(vd, "VoutDisplayEvent 'pictures invalid'");

        /* */
        assert(vd->info.has_pictures_invalid);

        vlc_mutex_lock(&osys->lock);
        osys->reset_pictures = true;
        vlc_mutex_unlock(&osys->lock);
        break;
    }
    default:
        msg_Err(vd, "VoutDisplayEvent received event %d", event);
        /* TODO add an assert when all event are handled */
        break;
    }
}

static vout_window_t *VoutDisplayNewWindow(vout_display_t *vd, unsigned type)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;
    vout_window_t *window = vout_NewDisplayWindow(osys->vout, type);
    if (window != NULL)
        vout_display_window_Attach(window, vd);
    return window;
}

static void VoutDisplayDelWindow(vout_display_t *vd, vout_window_t *window)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (window != NULL)
        vout_display_window_Detach(window);
    vout_DeleteDisplayWindow(osys->vout, window);
}

static void VoutDisplayFitWindow(vout_display_t *vd, bool default_size)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;
    vout_display_cfg_t cfg = osys->cfg;

    if (!cfg.is_display_filled)
        return;

    cfg.display.width = 0;
    if (default_size) {
        cfg.display.height = 0;
    } else {
        cfg.zoom.num = 1;
        cfg.zoom.den = 1;
    }

    unsigned display_width;
    unsigned display_height;
    vout_display_GetDefaultDisplaySize(&display_width, &display_height,
                                       &vd->source, &cfg);
    vout_SetDisplayWindowSize(osys->vout, display_width, display_height);
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

    vout_display_Manage(vd);

    /* Handle mouse timeout */
    const vlc_tick_t date = mdate();
    bool  hide_mouse = false;

    vlc_mutex_lock(&osys->lock);

    if (!osys->mouse.is_hidden &&
        osys->mouse.last_moved + osys->mouse.hide_timeout < date) {
        osys->mouse.is_hidden = hide_mouse = true;
    } else if (osys->mouse.ch_activity) {
        if (osys->mouse.is_hidden)
            vout_HideWindowMouse(osys->vout, false);
        osys->mouse.is_hidden = false;
    }
    osys->mouse.ch_activity = false;
    vlc_mutex_unlock(&osys->lock);

    if (hide_mouse) {
        msg_Dbg(vd, "auto hiding mouse cursor");
        if (vout_HideWindowMouse(osys->vout, true) != VLC_SUCCESS
         && vd->info.needs_hide_mouse)
            vout_display_Control(vd, VOUT_DISPLAY_HIDE_MOUSE);
    }

    bool reset_render = false;
    for (;;) {

        vlc_mutex_lock(&osys->lock);
#if defined(_WIN32) || defined(__OS2__)
        bool ch_fullscreen  = osys->ch_fullscreen;
        bool is_fullscreen  = osys->is_fullscreen;
        osys->ch_fullscreen = false;

        bool ch_wm_state  = osys->ch_wm_state;
        unsigned wm_state  = osys->wm_state;
        osys->ch_wm_state = false;
#endif

        bool ch_display_size       = osys->ch_display_size;
        int  display_width         = osys->display_width;
        int  display_height        = osys->display_height;
        osys->ch_display_size = false;

        bool reset_pictures;
        if (allow_reset_pictures) {
            reset_pictures = osys->reset_pictures;
            osys->reset_pictures = false;
        } else {
            reset_pictures = false;
        }

        vlc_mutex_unlock(&osys->lock);

        if (!ch_display_size &&
            !reset_pictures &&
            osys->is_display_filled == osys->cfg.is_display_filled &&
            !osys->ch_zoom &&
#if defined(_WIN32) || defined(__OS2__)
            !ch_fullscreen &&
            !ch_wm_state &&
#endif
            !osys->ch_sar &&
            !osys->ch_crop &&
            !osys->ch_viewpoint) {

            if (osys->fit_window != 0) {
                VoutDisplayFitWindow(vd, osys->fit_window == -1);
                osys->fit_window = 0;
                continue;
            }
            break;
        }

        /* */
#if defined(_WIN32) || defined(__OS2__)
        if (ch_fullscreen) {
            if (osys->window_fullscreen
             || vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN,
                                     is_fullscreen) == VLC_SUCCESS) {
                osys->cfg.is_fullscreen = is_fullscreen;

                if (!is_fullscreen && !osys->window_fullscreen)
                    vout_SetDisplayWindowSize(osys->vout, osys->width_saved,
                                              osys->height_saved);
            } else {
                is_fullscreen = osys->cfg.is_fullscreen;

                msg_Err(vd, "Failed to set fullscreen");
            }
        }
#endif

        /* */
        if (ch_display_size) {
#if defined(_WIN32) || defined(__OS2__)
            osys->width_saved  = osys->cfg.display.width;
            osys->height_saved = osys->cfg.display.height;
#endif
            osys->cfg.display.width  = display_width;
            osys->cfg.display.height = display_height;

            vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,
                                 &osys->cfg);
        }
        /* */
        if (osys->is_display_filled != osys->cfg.is_display_filled) {
            osys->cfg.is_display_filled = osys->is_display_filled;

            vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED,
                                 &osys->cfg);
        }
        /* */
        if (osys->ch_zoom) {
            osys->fit_window = -1;
            osys->cfg.zoom.num = osys->zoom.num;
            osys->cfg.zoom.den = osys->zoom.den;
            osys->ch_zoom = false;

            vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ZOOM, &osys->cfg);
        }
#if defined(_WIN32) || defined(__OS2__)
        /* */
        if (ch_wm_state) {
            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_WINDOW_STATE, wm_state)) {
                msg_Err(vd, "Failed to set on top");
                wm_state = osys->wm_state;
            }
            osys->wm_state_initial = wm_state;
        }
#endif
        /* */
        if (osys->ch_sar) {
            if (osys->sar.num > 0 && osys->sar.den > 0) {
                vd->source.i_sar_num = osys->sar.num;
                vd->source.i_sar_den = osys->sar.den;
            } else {
                vd->source.i_sar_num = osys->source.i_sar_num;
                vd->source.i_sar_den = osys->source.i_sar_den;
            }

            vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT);
            if (!osys->fit_window)
                osys->fit_window = 1;
            osys->sar.num = vd->source.i_sar_num;
            osys->sar.den = vd->source.i_sar_den;
            osys->ch_sar  = false;

            /* If a crop ratio is requested, recompute the parameters */
            if (osys->crop.num != 0 && osys->crop.den != 0)
                osys->ch_crop = true;
        }
        /* */
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

            if (!osys->fit_window)
                osys->fit_window = 1;
            osys->crop.left   = vd->source.i_x_offset - osys->source.i_x_offset;
            osys->crop.top    = vd->source.i_y_offset - osys->source.i_y_offset;
            /* FIXME for right/bottom we should keep the 'type' border vs window */
            osys->crop.right  = (vd->source.i_x_offset + vd->source.i_visible_width) -
                                (osys->source.i_x_offset + osys->source.i_visible_width);
            osys->crop.bottom = (vd->source.i_y_offset + vd->source.i_visible_height) -
                                (osys->source.i_y_offset + osys->source.i_visible_height);
            osys->crop.num    = crop_num;
            osys->crop.den    = crop_den;
            osys->ch_crop = false;
        }
        if (osys->ch_viewpoint) {
            vout_display_cfg_t cfg = osys->cfg;

            cfg.viewpoint = osys->viewpoint;

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_VIEWPOINT, &cfg)) {
                msg_Err(vd, "Failed to change Viewpoint");
                osys->viewpoint = osys->cfg.viewpoint;
            }
            osys->cfg.viewpoint = osys->viewpoint;
            osys->ch_viewpoint  = false;
        }

        /* */
        if (reset_pictures) {
            if (vout_display_Control(vd, VOUT_DISPLAY_RESET_PICTURES)) {
                /* FIXME what to do here ? */
                msg_Err(vd, "Failed to reset pictures (probably fatal)");
            }
            reset_render = true;
        }
    }
    if (reset_render)
        VoutDisplayResetRender(vd);

    return reset_render;
}

bool vout_AreDisplayPicturesInvalid(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vlc_mutex_lock(&osys->lock);
    const bool reset_pictures = osys->reset_pictures;
    vlc_mutex_unlock(&osys->lock);

    return reset_pictures;
}

bool vout_IsDisplayFiltered(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return osys->filters == NULL || !filter_chain_IsEmpty(osys->filters);
}

picture_t *vout_FilterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->filters == NULL) {
        picture_Release(picture);
        return NULL;
    }

    return filter_chain_VideoFilter(osys->filters, picture);
}

void vout_FilterFlush(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->filters != NULL)
        filter_chain_VideoFlush(osys->filters);
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

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    osys->is_display_filled = is_filled;
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

    if (osys->is_display_filled ||
        osys->zoom.num != num || osys->zoom.den != den) {
        osys->ch_zoom = true;
        osys->zoom.num = num;
        osys->zoom.den = den;
    }
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

    if (osys->viewpoint.yaw   != p_viewpoint->yaw ||
        osys->viewpoint.pitch != p_viewpoint->pitch ||
        osys->viewpoint.roll  != p_viewpoint->roll ||
        osys->viewpoint.fov   != p_viewpoint->fov) {
        osys->viewpoint = *p_viewpoint;

        osys->ch_viewpoint = true;
    }
}

static vout_display_t *DisplayNew(vout_thread_t *vout,
                                  const video_format_t *source,
                                  const vout_display_state_t *state,
                                  const char *module, bool is_splitter,
                                  vlc_tick_t double_click_timeout,
                                  vlc_tick_t hide_timeout,
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

    vlc_mutex_init(&osys->lock);

    vlc_mouse_Init(&osys->mouse.state);
    osys->mouse.last_moved = mdate();
    osys->mouse.double_click_timeout = double_click_timeout;
    osys->mouse.hide_timeout = hide_timeout;
    osys->display_width  = cfg->display.width;
    osys->display_height = cfg->display.height;
    osys->is_display_filled = cfg->is_display_filled;
    osys->viewpoint      = cfg->viewpoint;

    osys->zoom.num = cfg->zoom.num;
    osys->zoom.den = cfg->zoom.den;
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

    osys->wm_state_initial = VOUT_WINDOW_STATE_NORMAL;
    osys->wm_state = state->wm_state;
    osys->ch_wm_state = true;
#endif
    osys->fit_window = 0;

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
    if (owner_ptr) {
        owner = *owner_ptr;
    } else {
        owner.event      = VoutDisplayEvent;
        owner.window_new = VoutDisplayNewWindow;
        owner.window_del = VoutDisplayDelWindow;
    }
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

    vout_SendEventViewpointChangeable(osys->vout,
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
                                const char *module,
                                vlc_tick_t double_click_timeout,
                                vlc_tick_t hide_timeout)
{
    return DisplayNew(vout, source, state, module, false,
                      double_click_timeout, hide_timeout, NULL);
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

static vout_window_t *SplitterNewWindow(vout_display_t *vd, unsigned type)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;
    vout_window_t *window;
    vout_window_cfg_t cfg = {
        .type = type,
        .width = vd->cfg->display.width,
        .height = vd->cfg->display.height,
        .is_standalone = true,
    };

    window = vout_display_window_New(osys->vout, &cfg);
    if (window != NULL)
        vout_display_window_Attach(window, vd);
    return window;
}

static void SplitterDelWindow(vout_display_t *vd, vout_window_t *window)
{
    if (window != NULL) {
        vout_display_window_Detach(window);
        vout_display_window_Delete(window);
    }
    (void) vd;
}

static void SplitterEvent(vout_display_t *vd, int event, va_list args)
{
    //vout_display_owner_sys_t *osys = vd->owner.sys;

    switch (event) {
#if 0
    case VOUT_DISPLAY_EVENT_MOUSE_STATE:
    case VOUT_DISPLAY_EVENT_MOUSE_MOVED:
    case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
    case VOUT_DISPLAY_EVENT_MOUSE_RELEASED:
        /* TODO */
        break;
#endif
    case VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK:
    case VOUT_DISPLAY_EVENT_KEY:
    case VOUT_DISPLAY_EVENT_CLOSE:
    case VOUT_DISPLAY_EVENT_DISPLAY_SIZE:
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
                            subpicture_t *subpicture)
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
            vout_display_Prepare(sys->display[i], sys->picture[i], NULL);
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
    for (int i = 0; i < sys->count; i++)
        vout_DeleteDisplay(sys->display[i], NULL);
    TAB_CLEAN(sys->count, sys->display);
    free(sys->picture);

    free(sys);
}

vout_display_t *vout_NewSplitter(vout_thread_t *vout,
                                 const video_format_t *source,
                                 const vout_display_state_t *state,
                                 const char *module,
                                 const char *splitter_module,
                                 vlc_tick_t double_click_timeout,
                                 vlc_tick_t hide_timeout)
{
    video_splitter_t *splitter =
        video_splitter_New(VLC_OBJECT(vout), splitter_module, source);
    if (!splitter)
        return NULL;

    /* */
    vout_display_t *wrapper =
        DisplayNew(vout, source, state, module, true,
                    double_click_timeout, hide_timeout, NULL);
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
    wrapper->manage  = SplitterManage;
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
            .window_new = SplitterNewWindow,
            .window_del = SplitterDelWindow,
        };
        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_display_state_t ostate;

        memset(&ostate, 0, sizeof(ostate));
        ostate.cfg.display = state->cfg.display;
        ostate.cfg.align.horizontal = 0; /* TODO */
        ostate.cfg.align.vertical = 0; /* TODO */
        ostate.cfg.is_display_filled = true;
        ostate.cfg.zoom.num = 1;
        ostate.cfg.zoom.den = 1;

        vout_display_t *vd = DisplayNew(vout, &output->fmt, &ostate,
                                        output->psz_module ? output->psz_module : module,
                                        false,
                                        double_click_timeout, hide_timeout, &vdo);
        if (!vd) {
            vout_DeleteDisplay(wrapper, NULL);
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

    if (vlc_mouse_HasMoved(&vout->p->mouse, m)) {
        vout_SendEventMouseMoved(vout, m->i_x, m->i_y);
    }
    if (vlc_mouse_HasButton(&vout->p->mouse, m)) {
        for (unsigned button = 0; button < MOUSE_BUTTON_MAX; button++) {
            if (vlc_mouse_HasPressed(&vout->p->mouse, m, button))
                vout_SendEventMousePressed(vout, button);
            else if (vlc_mouse_HasReleased(&vout->p->mouse, m, button))
                vout_SendEventMouseReleased(vout, button);
        }
    }
    if (m->b_double_click)
        vout_SendEventMouseDoubleClick(vout);
    vout->p->mouse = *m;
}
