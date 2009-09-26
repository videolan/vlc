/*****************************************************************************
 * display.c: "vout display" managment
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_video_splitter.h>
#include <vlc_vout_display.h>
#include <vlc_vout.h>

#include <libvlc.h>

#include "display.h"

#include "event.h"

static void SplitterClose(vout_display_t *vd);

/*****************************************************************************
 * FIXME/TODO see how to have direct rendering here (interact with vout.c)
 *****************************************************************************/
static picture_t *VideoBufferNew(filter_t *filter)
{
    vout_display_t *vd = (vout_display_t*)filter->p_owner;
    const video_format_t *fmt = &filter->fmt_out.video;

    assert(vd->fmt.i_chroma == fmt->i_chroma &&
           vd->fmt.i_width  == fmt->i_width  &&
           vd->fmt.i_height == fmt->i_height);

    return vout_display_Get(vd);
}
static void VideoBufferDelete(filter_t *filter, picture_t *picture)
{
    VLC_UNUSED(filter);
    picture_Release(picture);
}

static int  FilterAllocationInit(filter_t *filter, void *vd)
{
    filter->pf_vout_buffer_new = VideoBufferNew;
    filter->pf_vout_buffer_del = VideoBufferDelete;
    filter->p_owner            = vd;

    return VLC_SUCCESS;
}
static void FilterAllocationClean(filter_t *filter)
{
    filter->pf_vout_buffer_new = NULL;
    filter->pf_vout_buffer_del = NULL;
    filter->p_owner            = NULL;
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
    vout_display_t *vd = vlc_object_create(obj, sizeof(*vd));

    /* */
    video_format_Copy(&vd->source, fmt);

    /* Picture buffer does not have the concept of aspect ratio */
    video_format_Copy(&vd->fmt, fmt);
    vd->fmt.i_aspect = 0;
    vd->fmt.i_sar_num = 0;
    vd->fmt.i_sar_den = 0;

    vd->info.is_slow = false;
    vd->info.has_double_click = false;
    vd->info.has_hide_mouse = false;
    vd->info.has_pictures_invalid = false;

    vd->cfg = cfg;
    vd->get = NULL;
    vd->prepare = NULL;
    vd->display = NULL;
    vd->control = NULL;
    vd->manage = NULL;
    vd->sys = NULL;

    vd->owner = *owner;

    vlc_object_attach(vd, obj);

    if (load_module) {
        vd->module = module_need(vd, "vout display", module, module && *module != '\0');
        if (!vd->module) {
            vlc_object_detach(vd);
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
    vlc_object_detach(vd);

    if (vd->module)
        module_unneed(vd, vd->module);

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
    vd->manage(vd);
}

/* */
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const vout_display_cfg_t *cfg)
{
    if (cfg->display.width > 0 && cfg->display.height > 0) {
        *width  = cfg->display.width;
        *height = cfg->display.height;
    } else if (cfg->display.width > 0) {
        *width  = cfg->display.width;
        *height = (int64_t)source->i_visible_height * source->i_sar_den * cfg->display.width * cfg->display.sar.num /
            source->i_visible_width / source->i_sar_num / cfg->display.sar.den;
    } else if (cfg->display.height > 0) {
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
}

/* */
void vout_display_PlacePicture(vout_display_place_t *place,
                               const video_format_t *source,
                               const vout_display_cfg_t *cfg,
                               bool do_clipping)
{
    /* */
    memset(place, 0, sizeof(*place));
    if (cfg->display.width <= 0 || cfg->display.height <= 0)
        return;

    /* */
    unsigned width;
    unsigned height;
    unsigned display_width;
    unsigned display_height;

    if (cfg->is_display_filled) {
        width  = source->i_visible_width;
        height = source->i_visible_height;
        display_width  = cfg->display.width;
        display_height = cfg->display.height;
    } else {
        vout_display_cfg_t cfg_tmp = *cfg;

        cfg_tmp.display.width  = 0;
        cfg_tmp.display.height = 0;
        vout_display_GetDefaultDisplaySize(&width, &height,
                                           source, &cfg_tmp);

        display_width  = width;
        display_height = height;
        if (do_clipping) {
            display_width  = __MIN(display_width,  cfg->display.width);
            display_height = __MIN(display_height, cfg->display.height);
        }
    }

    /* Compute the height if we use the width to fill up display_width */
    const int64_t scaled_height = (int64_t)height * display_width  * cfg->display.sar.num * source->i_sar_den / width  / source->i_sar_num / cfg->display.sar.den;
    /* And the same but switching width/height */
    const int64_t scaled_width  = (int64_t)width  * display_height * cfg->display.sar.den * source->i_sar_num / height / source->i_sar_den / cfg->display.sar.num;

    /* We keep the solution that avoid filling outside the display */
    if (scaled_width <= cfg->display.width) {
        place->width  = scaled_width;
        place->height = display_height;
    } else {
        place->width  = display_width;
        place->height = scaled_height;
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
        place->x = (cfg->display.width - place->width) / 2;
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
        place->y = (cfg->display.height - place->height) / 2;
        break;
    }
}

struct vout_display_owner_sys_t {
    vout_thread_t   *vout;
    bool            is_wrapper;  /* Is the current display a wrapper */
    vout_display_t  *wrapper; /* Vout display wrapper */

    /* mouse state */
    struct {
        vlc_mouse_t state;

        mtime_t last_pressed;
        mtime_t last_moved;
        bool    is_hidden;

        /* */
        mtime_t double_click_timeout;
        mtime_t hide_timeout;
    } mouse;

    /* */
    vout_display_cfg_t cfg;
    bool     is_on_top_initial;
    struct {
        unsigned num;
        unsigned den;
    } sar_initial;

    /* */
    int  width_saved;
    int  height_saved;

    struct {
        unsigned num;
        unsigned den;
    } crop_saved;

    /* */
    bool reset_pictures;

    bool ch_fullscreen;
    bool is_fullscreen;

    bool ch_display_size;
    int  display_width;
    int  display_height;
    bool display_is_fullscreen;

    bool ch_display_filled;
    bool is_display_filled;

    bool ch_zoom;
    struct {
        int  num;
        int  den;
    } zoom;

    bool ch_on_top;
    bool is_on_top;

    bool ch_sar;
    struct {
        unsigned num;
        unsigned den;
    } sar;

    bool ch_crop;
    struct {
        unsigned x;
        unsigned y;
        unsigned width;
        unsigned height;
        unsigned num;
        unsigned den;
    } crop;

    /* */
    video_format_t source;
    filter_chain_t *filters;
};

static void VoutDisplayCreateRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    osys->filters = NULL;

    video_format_t v_src = vd->source;
    v_src.i_aspect  = 0;
    v_src.i_sar_num = 0;
    v_src.i_sar_den = 0;

    video_format_t v_dst = vd->fmt;
    v_dst.i_aspect  = 0;
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
        return;

    msg_Err(vd, "A filter to adapt decoder to display is needed");

    osys->filters = filter_chain_New(vd, "video filter2", false,
                                     FilterAllocationInit,
                                     FilterAllocationClean, vd);
    assert(osys->filters); /* TODO critical */

    /* */
    es_format_t src;
    es_format_InitFromVideo(&src, &v_src);

    /* */
    es_format_t dst;

    filter_t *filter;
    for (int i = 0; i < 1 + (v_dst_cmp.i_chroma != v_dst.i_chroma); i++) {

        es_format_InitFromVideo(&dst, i == 0 ? &v_dst : &v_dst_cmp);

        filter_chain_Reset(osys->filters, &src, &dst);
        filter = filter_chain_AppendFilter(osys->filters,
                                           NULL, NULL, &src, &dst);
        if (filter)
            break;
    }
    if (!filter)
    {
        msg_Err(vd, "VoutDisplayCreateRender FAILED");
        /* TODO */
        assert(0);
    }
}

static void VoutDisplayDestroyRender(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->filters)
        filter_chain_Delete(osys->filters);
}

static void VoutDisplayResetRender(vout_display_t *vd)
{
    VoutDisplayDestroyRender(vd);
    VoutDisplayCreateRender(vd);
}
static void VoutDisplayEventMouse(vout_display_t *vd, int event, va_list args)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

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
        if (x != osys->mouse.state.i_x || y != osys->mouse.state.i_y) {
            msg_Dbg(vd, "VoutDisplayEvent 'mouse' @%d,%d", x, y);

            m.i_x = x;
            m.i_y = y;
            m.b_double_click = false;
        } else {
            is_ignored = true;
        }
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
        assert(0);
    }

    if (is_ignored)
        return;

    /* Emulate double-click if needed */
    if (!vd->info.has_double_click &&
        vlc_mouse_HasPressed(&osys->mouse.state, &m, MOUSE_BUTTON_LEFT)) {
        const mtime_t i_date = mdate();

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
    osys->mouse.is_hidden = false;
    if (!vd->info.has_hide_mouse)
        osys->mouse.last_moved = mdate();

    /* */
    vout_SendEventMouseVisible(osys->vout);
    vout_SendDisplayEventMouse(osys->vout, &m);
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

    case VOUT_DISPLAY_EVENT_FULLSCREEN: {
        const int is_fullscreen = (int)va_arg(args, int);

        msg_Dbg(vd, "VoutDisplayEvent 'fullscreen' %d", is_fullscreen);

        if (!is_fullscreen == !osys->is_fullscreen)
            break;
        osys->ch_fullscreen = true;
        osys->is_fullscreen = is_fullscreen;
        break;
    }

    case VOUT_DISPLAY_EVENT_DISPLAY_SIZE: {
        const int width  = (int)va_arg(args, int);
        const int height = (int)va_arg(args, int);
        const bool is_fullscreen = (bool)va_arg(args, int);
        msg_Dbg(vd, "VoutDisplayEvent 'resize' %dx%d %s",
                width, height, is_fullscreen ? "fullscreen" : "window");

        /* */
        osys->ch_display_size       = true;
        osys->display_width         = width;
        osys->display_height        = height;
        osys->display_is_fullscreen = is_fullscreen;
        break;
    }

    case VOUT_DISPLAY_EVENT_PICTURES_INVALID: {
        msg_Warn(vd, "VoutDisplayEvent 'pictures invalid'");

        /* */
        assert(vd->info.has_pictures_invalid);
        osys->reset_pictures = true;
        break;
    }
    default:
        msg_Err(vd, "VoutDisplayEvent received event %d", event);
        /* TODO add an assert when all event are handled */
        break;
    }
}

static vout_window_t *VoutDisplayNewWindow(vout_display_t *vd, const vout_window_cfg_t *cfg)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return vout_NewDisplayWindow(osys->vout, vd, cfg);
}
static void VoutDisplayDelWindow(vout_display_t *vd, vout_window_t *window)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vout_DeleteDisplayWindow(osys->vout, vd, window);
}

void vout_ManageDisplay(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vout_display_Manage(vd);

    /* Handle mouse timeout */
    const mtime_t date = mdate();
    if (!osys->mouse.is_hidden &&
        osys->mouse.last_moved + osys->mouse.hide_timeout < date) {
        if (!vd->info.has_hide_mouse) {
            msg_Dbg(vd, "auto hidding mouse");
            vout_display_Control(vd, VOUT_DISPLAY_HIDE_MOUSE);
        }
        osys->mouse.is_hidden = true;

        vout_SendEventMouseHidden(osys->vout);
    }

    bool reset_pictures = false;
    for (;;) {
        if (!osys->ch_fullscreen &&
            !osys->ch_display_size &&
            !osys->reset_pictures &&
            !osys->ch_display_filled &&
            !osys->ch_zoom &&
            !osys->ch_on_top &&
            !osys->ch_sar &&
            !osys->ch_crop)
            break;

        /* */
        if (osys->ch_fullscreen) {
            vout_display_cfg_t cfg = osys->cfg;

            cfg.is_fullscreen = osys->is_fullscreen;
            cfg.display.width  = cfg.is_fullscreen ? 0 : osys->width_saved;
            cfg.display.height = cfg.is_fullscreen ? 0 : osys->height_saved;

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN, &cfg)) {
                msg_Err(vd, "Failed to set fullscreen");
                osys->is_fullscreen = osys->cfg.is_fullscreen;
            }
            osys->cfg.is_fullscreen = osys->is_fullscreen;
            osys->ch_fullscreen = false;

            /* */
            vout_SendEventFullscreen(osys->vout, osys->cfg.is_fullscreen);
        }

        /* */
        if (osys->ch_display_size) {
            vout_display_cfg_t cfg = osys->cfg;
            cfg.display.width  = osys->display_width;
            cfg.display.height = osys->display_height;

            if (!cfg.is_fullscreen != !osys->display_is_fullscreen ||
                vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE, &cfg)) {
                if (!cfg.is_fullscreen == !osys->display_is_fullscreen)
                    msg_Err(vd, "Failed to resize display");

                /* We ignore the resized */
                osys->display_width  = osys->cfg.display.width;
                osys->display_height = osys->cfg.display.height;
            }
            osys->cfg.display.width  = osys->display_width;
            osys->cfg.display.height = osys->display_height;

            if (!osys->display_is_fullscreen) {
                osys->width_saved  = osys->display_width;
                osys->height_saved = osys->display_height;
            }
            osys->ch_display_size = false;
        }
        /* */
        if (osys->ch_display_filled) {
            vout_display_cfg_t cfg = osys->cfg;

            cfg.is_display_filled = osys->is_display_filled;

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED, &cfg)) {
                msg_Err(vd, "Failed to change display filled state");
                osys->is_display_filled = osys->cfg.is_display_filled;
            }
            osys->cfg.is_display_filled = osys->is_display_filled;
            osys->ch_display_filled = false;

            vout_SendEventDisplayFilled(osys->vout, osys->cfg.is_display_filled);
        }
        /* */
        if (osys->ch_zoom) {
            vout_display_cfg_t cfg = osys->cfg;

            cfg.zoom.num = osys->zoom.num;
            cfg.zoom.den = osys->zoom.den;

            if (10 * cfg.zoom.num <= cfg.zoom.den) {
                cfg.zoom.num = 1;
                cfg.zoom.den = 10;
            } else if (cfg.zoom.num >= 10 * cfg.zoom.den) {
                cfg.zoom.num = 10;
                cfg.zoom.den = 1;
            }

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ZOOM, &cfg)) {
                msg_Err(vd, "Failed to change zoom");
                osys->zoom.num = osys->cfg.zoom.num;
                osys->zoom.den = osys->cfg.zoom.den;
            } else if (cfg.is_display_filled) {
                const int display_width  = (int64_t)vd->source.i_width  * osys->zoom.num / osys->zoom.den;
                const int display_height = (int64_t)vd->source.i_height * osys->zoom.num / osys->zoom.den;

                vout_display_SendEventDisplaySize(vd, display_width, display_height, osys->cfg.is_fullscreen);
            }

            osys->cfg.zoom.num = osys->zoom.num;
            osys->cfg.zoom.den = osys->zoom.den;
            osys->ch_zoom = false;

            vout_SendEventZoom(osys->vout, osys->cfg.zoom.num, osys->cfg.zoom.den);
        }
        /* */
        if (osys->ch_on_top) {
            bool is_on_top = osys->is_on_top;

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ON_TOP, is_on_top)) {
                msg_Err(vd, "Failed to set on top");
                is_on_top = osys->is_on_top_initial;
            }
            osys->is_on_top_initial =
            osys->is_on_top         = is_on_top;
            osys->ch_on_top = false;

            /* */
            vout_SendEventOnTop(osys->vout, osys->is_on_top_initial);
        }
        /* */
        if (osys->ch_sar) {
            video_format_t source = vd->source;

            if (osys->sar.num > 0 && osys->sar.den > 0) {
                source.i_sar_num = osys->sar.num;
                source.i_sar_den = osys->sar.den;
            } else {
                source.i_sar_num = osys->source.i_sar_num;
                source.i_sar_den = osys->source.i_sar_den;
            }

            if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT, &source)) {
                /* There nothing much we can do. The only reason a vout display
                 * does not support it is because it need the core to add black border
                 * to the video for it.
                 * TODO add black borders ?
                 */
                msg_Err(vd, "Failed to change source AR");
                source = vd->source;
            }
            vd->source = source;
            osys->sar.num = source.i_sar_num;
            osys->sar.den = source.i_sar_den;
            osys->ch_sar  = false;

            /* */
            if (osys->sar.num == osys->source.i_sar_num &&
                osys->sar.den == osys->source.i_sar_den)
            {
                vout_SendEventSourceAspect(osys->vout, 0, 0);
            }
            else
            {
                unsigned dar_num, dar_den;
                vlc_ureduce( &dar_num, &dar_den,
                             osys->sar.num * vd->source.i_visible_width,
                             osys->sar.den * vd->source.i_visible_height,
                             65536);
                vout_SendEventSourceAspect(osys->vout, dar_num, dar_den);
            }
        }
        /* */
        if (osys->ch_crop) {
            video_format_t source = vd->source;
            unsigned crop_num = osys->crop.num;
            unsigned crop_den = osys->crop.den;

            source.i_x_offset       = osys->crop.x;
            source.i_y_offset       = osys->crop.y;
            source.i_visible_width  = osys->crop.width;
            source.i_visible_height = osys->crop.height;

            /* */
            const bool is_valid = source.i_x_offset < source.i_width &&
                                  source.i_y_offset < source.i_height &&
                                  source.i_x_offset + source.i_visible_width  <= source.i_width &&
                                  source.i_y_offset + source.i_visible_height <= source.i_height &&
                                  source.i_visible_width > 0 && source.i_visible_height > 0;

            if (!is_valid || vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP, &source)) {
                if (is_valid)
                    msg_Err(vd, "Failed to change source crop TODO implement crop at core");
                else
                    msg_Err(vd, "Invalid crop requested");

                source = vd->source;
                crop_num = osys->crop_saved.num;
                crop_den = osys->crop_saved.den;
                /* FIXME implement cropping in the core if not supported by the
                 * vout module (easy)
                 */
            }
            vd->source = source;
            osys->crop.x      = source.i_x_offset;
            osys->crop.y      = source.i_y_offset;
            osys->crop.width  = source.i_visible_width;
            osys->crop.height = source.i_visible_height;
            osys->crop.num    = crop_num;
            osys->crop.den    = crop_den;
            osys->ch_crop = false;

            /* TODO fix when a ratio is used (complicated). */
            const unsigned left   = osys->crop.x - osys->source.i_x_offset;
            const unsigned top    = osys->crop.y - osys->source.i_y_offset;
            const unsigned right  = osys->source.i_visible_width  - (osys->crop.width  + osys->crop.x);
            const unsigned bottom = osys->source.i_visible_height - (osys->crop.height + osys->crop.y);
            vout_SendEventSourceCrop(osys->vout,
                                     osys->crop.num, osys->crop.den,
                                     left, top, right, bottom);
        }

        /* */
        if (osys->reset_pictures) {
            if (vout_display_Control(vd, VOUT_DISPLAY_RESET_PICTURES)) {
                /* FIXME what to do here ? */
                msg_Err(vd, "Failed to reset pictures (probably fatal)");
            }
            reset_pictures = true;

            osys->reset_pictures = false;
        }
    }
    if (reset_pictures)
        VoutDisplayResetRender(vd);
}

bool vout_AreDisplayPicturesInvalid(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return osys->reset_pictures;
}

bool vout_IsDisplayFiltered(vout_display_t *vd)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    return osys->filters != NULL;
}

picture_t *vout_FilterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    assert(osys->filters);
    return filter_chain_VideoFilter(osys->filters, picture);
}

void vout_SetDisplayFullscreen(vout_display_t *vd, bool is_fullscreen)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (!osys->is_fullscreen != !is_fullscreen) {
        osys->ch_fullscreen = true;
        osys->is_fullscreen = is_fullscreen;
    }
}

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (!osys->is_display_filled != !is_filled) {
        osys->ch_display_filled = true;
        osys->is_display_filled = is_filled;
    }
}

void vout_SetDisplayZoom(vout_display_t *vd, int num, int den)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->zoom.num != num || osys->zoom.den != den) {
        osys->ch_zoom = true;
        osys->zoom.num = num;
        osys->zoom.den = den;
    }
}
void vout_SetDisplayOnTop(vout_display_t *vd, bool is_on_top)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (!osys->is_on_top != !is_on_top) {
        osys->ch_on_top = true;
        osys->is_on_top = is_on_top;
    }
}
void vout_SetDisplayAspect(vout_display_t *vd, unsigned sar_num, unsigned sar_den)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->sar.num != sar_num || osys->sar.den != sar_den) {
        osys->ch_sar = true;
        osys->sar.num = sar_num;
        osys->sar.den = sar_den;
    }
}
void vout_SetDisplayCrop(vout_display_t *vd,
                         unsigned crop_num, unsigned crop_den,
                         unsigned x, unsigned y, unsigned width, unsigned height)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (osys->crop.x != x || osys->crop.y != y ||
        osys->crop.width  != width || osys->crop.height != height) {

        osys->crop.x      = x;
        osys->crop.y      = y;
        osys->crop.width  = width;
        osys->crop.height = height;
        osys->crop.num    = crop_num;
        osys->crop.den    = crop_den;

        osys->ch_crop = true;
    }
}


static vout_display_t *DisplayNew(vout_thread_t *vout,
                                  const video_format_t *source_org,
                                  const vout_display_state_t *state,
                                  const char *module,
                                  bool is_wrapper, vout_display_t *wrapper,
                                  mtime_t double_click_timeout,
                                  mtime_t hide_timeout,
                                  const vout_display_owner_t *owner_ptr)
{
    /* */
    vout_display_owner_sys_t *osys = calloc(1, sizeof(*osys));
    vout_display_cfg_t *cfg = &osys->cfg;

    *cfg = state->cfg;
    osys->is_on_top_initial = state->is_on_top;;
    osys->sar_initial.num = state->sar.num;
    osys->sar_initial.den = state->sar.den;
    vout_display_GetDefaultDisplaySize(&cfg->display.width, &cfg->display.height,
                                       source_org, cfg);

    osys->vout = vout;
    osys->is_wrapper = is_wrapper;
    osys->wrapper = wrapper;

    vlc_mouse_Init(&osys->mouse.state);
    osys->mouse.last_moved = mdate();
    osys->mouse.double_click_timeout = double_click_timeout;
    osys->mouse.hide_timeout = hide_timeout;
    osys->is_fullscreen  = cfg->is_fullscreen;
    osys->width_saved    =
    osys->display_width  = cfg->display.width;
    osys->height_saved   =
    osys->display_height = cfg->display.height;
    osys->is_display_filled = cfg->is_display_filled;
    osys->zoom.num = cfg->zoom.num;
    osys->zoom.den = cfg->zoom.den;

    osys->source = *source_org;

    video_format_t source = *source_org;

    source.i_x_offset =
    osys->crop.x  = 0;
    source.i_y_offset =
    osys->crop.y  = 0;
    source.i_visible_width  =
    osys->crop.width    = source.i_width;
    source.i_visible_height =
    osys->crop.height   = source.i_height;
    osys->crop_saved.num = 0;
    osys->crop_saved.den = 0;
    osys->crop.num = 0;
    osys->crop.den = 0;

    osys->sar.num = osys->sar_initial.num ? osys->sar_initial.num : source.i_sar_num;
    osys->sar.den = osys->sar_initial.den ? osys->sar_initial.den : source.i_sar_den;

    vout_display_owner_t owner;
    if (owner_ptr) {
        owner = *owner_ptr;
    } else {
        owner.event      = VoutDisplayEvent;
        owner.window_new = VoutDisplayNewWindow;
        owner.window_del = VoutDisplayDelWindow;
    }
    owner.sys = osys;

    /* */
    vout_display_t *p_display = vout_display_New(VLC_OBJECT(vout),
                                                 module, !is_wrapper,
                                                 &source, cfg, &owner);
    if (!p_display) {
        free(osys);
        return NULL;
    }

    VoutDisplayCreateRender(p_display);

    /* Setup delayed request */
    if (osys->sar.num != source_org->i_sar_num ||
        osys->sar.den != source_org->i_sar_den)
        osys->ch_sar = true;
    if (osys->is_on_top)
        osys->ch_on_top = true;
    if (osys->crop.x      != source_org->i_x_offset ||
        osys->crop.y      != source_org->i_y_offset ||
        osys->crop.width  != source_org->i_visible_width ||
        osys->crop.height != source_org->i_visible_height)
        osys->ch_crop = true;

    return p_display;
}

void vout_DeleteDisplay(vout_display_t *vd, vout_display_state_t *state)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    if (state) {
        if (!osys->is_wrapper )
            state->cfg = osys->cfg;
        state->is_on_top = osys->is_on_top_initial;
        state->sar.num   = osys->sar_initial.num;
        state->sar.den   = osys->sar_initial.den;
    }

    VoutDisplayDestroyRender(vd);
    if (osys->is_wrapper)
        SplitterClose(vd);
    vout_display_Delete(vd);
    free(osys);
}

/*****************************************************************************
 *
 *****************************************************************************/
vout_display_t *vout_NewDisplay(vout_thread_t *vout,
                                const video_format_t *source,
                                const vout_display_state_t *state,
                                const char *module,
                                mtime_t double_click_timeout,
                                mtime_t hide_timeout)
{
    return DisplayNew(vout, source, state, module, false, NULL,
                      double_click_timeout, hide_timeout, NULL);
}

static void SplitterClose(vout_display_t *vd)
{
    VLC_UNUSED(vd);
    assert(0);
}

#if 0
/*****************************************************************************
 *
 *****************************************************************************/
struct vout_display_sys_t {
    video_splitter_t *splitter;

    /* */
    int            count;
    picture_t      **picture;
    vout_display_t **display;
};
struct video_splitter_owner_t {
    vout_display_t *wrapper;
};

static vout_window_t *SplitterNewWindow(vout_display_t *vd, const vout_window_cfg_t *cfg_ptr)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vout_window_cfg_t cfg = *cfg_ptr;
    cfg.is_standalone = true;
    cfg.x += 0;//output->window.i_x; FIXME
    cfg.y += 0;//output->window.i_y;

    return vout_NewDisplayWindow(osys->vout, vd, &cfg);
}
static void SplitterDelWindow(vout_display_t *vd, vout_window_t *window)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

    vout_DeleteDisplayWindow(osys->vout, vd, window);
}
static void SplitterEvent(vout_display_t *vd, int event, va_list args)
{
    vout_display_owner_sys_t *osys = vd->owner.sys;

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
    case VOUT_DISPLAY_EVENT_FULLSCREEN:
    case VOUT_DISPLAY_EVENT_DISPLAY_SIZE:
    case VOUT_DISPLAY_EVENT_PICTURES_INVALID:
        VoutDisplayEvent(vd, event, args);
        break;

    default:
        msg_Err(vd, "SplitterEvent TODO");
        break;
    }
}

static picture_t *SplitterGet(vout_display_t *vd)
{
    /* TODO pool ? */
    return picture_NewFromFormat(&vd->fmt);
}
static void SplitterPrepare(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    picture_Hold(picture);

    if (video_splitter_Filter(sys->splitter, sys->picture, picture)) {
        for (int i = 0; i < sys->count; i++)
            sys->picture[i] = NULL;
        picture_Release(picture);
        return;
    }

    for (int i = 0; i < sys->count; i++) {
        /* */
        /* FIXME now vout_FilterDisplay already return a direct buffer FIXME */
        sys->picture[i] = vout_FilterDisplay(sys->display[i], sys->picture[i]);
        if (!sys->picture[i])
            continue;

        /* */
        picture_t *direct = vout_display_Get(sys->display[i]);
        if (!direct) {
            msg_Err(vd, "Failed to get a direct buffer");
            picture_Release(sys->picture[i]);
            sys->picture[i] = NULL;
            continue;
        }

        /* FIXME not always needed (easy when there is a osys->filters) */
        picture_Copy(direct, sys->picture[i]);
        picture_Release(sys->picture[i]);
        sys->picture[i] = direct;

        vout_display_Prepare(sys->display[i], sys->picture[i]);
    }
}
static void SplitterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    for (int i = 0; i < sys->count; i++) {
        if (sys->picture[i])
            vout_display_Display(sys->display[i], sys->picture[i]);
    }
    picture_Release(picture);
}
static int SplitterControl(vout_display_t *vd, int query, va_list args)
{
    return VLC_EGENERIC;
}
static void SplitterManage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    for (int i = 0; i < sys->count; i++)
        vout_ManageDisplay(sys->display[i]);
}

static int SplitterPictureNew(video_splitter_t *splitter, picture_t *picture[])
{
    vout_display_sys_t *wsys = splitter->p_owner->wrapper->sys;

    for (int i = 0; i < wsys->count; i++) {
        /* TODO pool ? */
        picture[i] = picture_NewFromFormat(&wsys->display[i]->source);
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
                                 mtime_t double_click_timeout,
                                 mtime_t hide_timeout)
{
    video_splitter_t *splitter =
        video_splitter_New(VLC_OBJECT(vout), splitter_module, source);
    if (!splitter)
        return NULL;

    /* */
    vout_display_t *wrapper =
        DisplayNew(vout, source, state, module, true, NULL,
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

    wrapper->get     = SplitterGet;
    wrapper->prepare = SplitterPrepare;
    wrapper->display = SplitterDisplay;
    wrapper->control = SplitterControl;
    wrapper->manage  = SplitterManage;
    wrapper->sys     = sys;

    /* */
    video_splitter_owner_t *owner = malloc(sizeof(*owner));
    if (!owner)
        abort();
    owner->wrapper = wrapper;
    splitter->p_owner = owner;
    splitter->pf_picture_new = SplitterPictureNew;
    splitter->pf_picture_del = SplitterPictureDel;

    /* */
    TAB_INIT(sys->count, sys->display);
    for (int i = 0; i < splitter->i_output; i++) {
        vout_display_owner_t owner;

        owner.event      = SplitterEvent;
        owner.window_new = SplitterNewWindow;
        owner.window_del = SplitterDelWindow;

        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_display_state_t ostate;

        memset(&ostate, 0, sizeof(ostate));
        ostate.cfg.is_fullscreen = false;
        ostate.cfg.display = state->cfg.display;
        ostate.cfg.align.horizontal = 0; /* TODO */
        ostate.cfg.align.vertical = 0; /* TODO */
        ostate.cfg.is_display_filled = true;
        ostate.cfg.zoom.num = 1;
        ostate.cfg.zoom.den = 1;

        vout_display_t *vd = DisplayNew(vout, &output->fmt, &ostate,
                                           output->psz_module ? output->psz_module : module,
                                           false, wrapper,
                                           double_click_timeout, hide_timeout, &owner);
        if (!vd) {
            vout_DeleteDisplay(wrapper, NULL);
            return NULL;
        }
        TAB_APPEND(sys->count, sys->display, vd);
    }

    return wrapper;
}
#endif

/*****************************************************************************
 * TODO move out
 *****************************************************************************/
#include "vout_internal.h"
void vout_SendDisplayEventMouse(vout_thread_t *vout, const vlc_mouse_t *m)
{
    if (vlc_mouse_HasMoved(&vout->p->mouse, m)) {
        vout_SendEventMouseMoved(vout, m->i_x, m->i_y);
    }
    if (vlc_mouse_HasButton(&vout->p->mouse, m)) {
        static const int buttons[] = {
            MOUSE_BUTTON_LEFT,
            MOUSE_BUTTON_CENTER,
            MOUSE_BUTTON_RIGHT,
            MOUSE_BUTTON_WHEEL_UP,
            MOUSE_BUTTON_WHEEL_DOWN,
            -1
        };
        for (int i = 0; buttons[i] >= 0; i++) {
            const int button = buttons[i];
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
vout_window_t * vout_NewDisplayWindow(vout_thread_t *vout, vout_display_t *vd, const vout_window_cfg_t *cfg)
{
    VLC_UNUSED(vd);
    vout_window_cfg_t cfg_override = *cfg;

    if( !config_GetInt( vout, "embedded-video" ) )
        cfg_override.is_standalone = true;

    return vout_window_New(VLC_OBJECT(vout), NULL, &cfg_override);
}
void vout_DeleteDisplayWindow(vout_thread_t *vout, vout_display_t *vd, vout_window_t *window)
{
    VLC_UNUSED(vout);
    VLC_UNUSED(vd);
    vout_window_Delete(window);
}

