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

    picture_pool_t *pool = vout_GetPool(vd, 3);
    if (!pool)
        return NULL;
    return picture_pool_Get(pool);
}

/*****************************************************************************
 *
 *****************************************************************************/

static int vout_display_start(void *func, va_list ap)
{
    vout_display_open_cb activate = func;
    vout_display_t *vd = va_arg(ap, vout_display_t *);
    const vout_display_cfg_t *cfg = va_arg(ap, const vout_display_cfg_t *);
    video_format_t *fmtp = va_arg(ap, video_format_t *);
    vlc_video_context *context = va_arg(ap, vlc_video_context *);

    /* Picture buffer does not have the concept of aspect ratio */
    video_format_Copy(fmtp, &vd->source);
    fmtp->i_sar_num = 0;
    fmtp->i_sar_den = 0;

    int ret = activate(vd, cfg, fmtp, context);
    if (ret != VLC_SUCCESS)
        video_format_Clean(fmtp);
    return ret;
}

static void vout_display_stop(void *func, va_list ap)
{
    vout_display_close_cb deactivate = func;

    deactivate(va_arg(ap, vout_display_t *));
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
                               const vout_display_cfg_t *cfg)
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
    case VLC_VIDEO_ALIGN_LEFT:
        place->x = 0;
        break;
    case VLC_VIDEO_ALIGN_RIGHT:
        place->x = cfg->display.width - place->width;
        break;
    default:
        place->x = ((int)cfg->display.width - (int)place->width) / 2;
        break;
    }

    switch (cfg->align.vertical) {
    case VLC_VIDEO_ALIGN_TOP:
        place->y = 0;
        break;
    case VLC_VIDEO_ALIGN_BOTTOM:
        place->y = cfg->display.height - place->height;
        break;
    default:
        place->y = ((int)cfg->display.height - (int)place->height) / 2;
        break;
    }
}

void vout_display_TranslateMouseState(vout_display_t *vd, vlc_mouse_t *video,
                                      const vlc_mouse_t *window)
{
    vout_display_place_t place;

    /* Translate window coordinates to video coordinates */
    vout_display_PlacePicture(&place, &vd->source, vd->cfg);

    if (place.width <= 0 || place.height <= 0) {
        memset(video, 0, sizeof (*video));
        return;
    }

    const int wx = window->i_x, wy = window->i_y;
    int x, y;

    switch (vd->source.orientation) {
        case ORIENT_TOP_LEFT:
            x = wx;
            y = wy;
            break;
        case ORIENT_TOP_RIGHT:
            x = place.width - wx;
            y = wy;
            break;
        case ORIENT_BOTTOM_LEFT:
            x = wx;
            y = place.height - wy;
            break;
        case ORIENT_BOTTOM_RIGHT:
            x = place.width - wx;
            y = place.height - wy;
            break;
        case ORIENT_LEFT_TOP:
            x = wy;
            y = wx;
            break;
        case ORIENT_LEFT_BOTTOM:
            x = wy;
            y = place.width - wx;
            break;
        case ORIENT_RIGHT_TOP:
            x = place.height - wy;
            y = wx;
            break;
        case ORIENT_RIGHT_BOTTOM:
            x = place.height - wy;
            y = place.width - wx;
            break;
    }

    video->i_x = vd->source.i_x_offset
        + (int64_t)(x - place.x) * vd->source.i_visible_width / place.width;
    video->i_y = vd->source.i_y_offset
        + (int64_t)(y - place.y) * vd->source.i_visible_height / place.height;
    video->i_pressed = window->i_pressed;
    video->b_double_click = window->b_double_click;
}

void vout_display_SendMouseMovedDisplayCoordinates(vout_display_t *vd, int m_x, int m_y)
{
    vout_window_ReportMouseMoved(vd->cfg->window, m_x, m_y);
}

typedef struct {
    vout_display_t  display;
    vout_thread_t   *vout;
    bool            is_splitter;  /* Is this a video splitter */

    /* */
    vout_display_cfg_t cfg;

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

    atomic_bool reset_pictures;
    picture_pool_t *pool;
} vout_display_priv_t;

static const struct filter_video_callbacks vout_display_filter_cbs = {
    .buffer_new = VideoBufferNew,
};

static int VoutDisplayCreateRender(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
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
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converters)
        filter_chain_Delete(osys->converters);
}

void vout_display_SendEventPicturesInvalid(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    msg_Warn(vd, "picture buffers invalidated");
    assert(vd->info.has_pictures_invalid);
    atomic_store_explicit(&osys->reset_pictures, true, memory_order_release);
}

static void VoutDisplayEvent(vout_display_t *vd, int event, va_list args)
{
    vout_thread_t *vout = vd->owner.sys;

    switch (event) {
    case VOUT_DISPLAY_EVENT_VIEWPOINT_MOVED:
        var_SetAddress(vout, "viewpoint-moved",
                       (void *)va_arg(args, const vlc_viewpoint_t *));
        break;
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

/**
 * It retreives a picture pool from the display
 */
picture_pool_t *vout_GetPool(vout_display_t *vd, unsigned count)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (vd->pool != NULL)
        return vd->pool(vd, count);

    if (osys->pool == NULL)
        osys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return osys->pool;
}

bool vout_IsDisplayFiltered(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    return osys->converters == NULL || !filter_chain_IsEmpty(osys->converters);
}

picture_t *vout_FilterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converters == NULL) {
        picture_Release(picture);
        return NULL;
    }

    picture = filter_chain_VideoFilter(osys->converters, picture);

    if (picture != NULL && vd->pool != NULL && picture->i_planes > 0) {
        picture_pool_t *pool = vd->pool(vd, 3);

        if (!picture_pool_OwnsPic(pool, picture)) {
            /* The picture is not be allocated from the expected pool. Copy. */
            picture_t *direct = picture_pool_Get(pool);

            if (direct != NULL) {
                video_format_CopyCropAr(&direct->format, &picture->format);
                picture_Copy(direct, picture);
            }
            picture_Release(picture);
            picture = direct;
        }
    }

    return picture;
}

void vout_FilterFlush(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converters != NULL)
        filter_chain_VideoFlush(osys->converters);
}

static void vout_display_Reset(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (likely(!atomic_exchange_explicit(&osys->reset_pictures, false,
                                         memory_order_relaxed)))
        return;

    atomic_thread_fence(memory_order_acquire);
    VoutDisplayDestroyRender(vd);

    if (osys->pool != NULL) {
        picture_pool_Release(osys->pool);
        osys->pool = NULL;
    }

    if (vout_display_Control(vd, VOUT_DISPLAY_RESET_PICTURES, &osys->cfg,
                             &vd->fmt)
     || VoutDisplayCreateRender(vd))
        msg_Err(vd, "Failed to adjust render format");
}

static void vout_UpdateSourceCrop(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
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

    const int right_max  = osys->source.i_x_offset
                           + osys->source.i_visible_width;
    const int bottom_max = osys->source.i_y_offset
                           + osys->source.i_visible_height;
    int left = VLC_CLIP((int)osys->source.i_x_offset + osys->crop.left,
                          0, right_max - 1);
    int top  = VLC_CLIP((int)osys->source.i_y_offset + osys->crop.top,
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
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP, &osys->cfg);
    osys->crop.left   = left - osys->source.i_x_offset;
    osys->crop.top    = top  - osys->source.i_y_offset;
    /* FIXME for right/bottom we should keep the 'type' border vs window */
    osys->crop.right  = right -
                        (osys->source.i_x_offset + osys->source.i_visible_width);
    osys->crop.bottom = bottom -
                        (osys->source.i_y_offset + osys->source.i_visible_height);
    osys->crop.num    = crop_num;
    osys->crop.den    = crop_den;
}

static void vout_SetSourceAspect(vout_display_t *vd,
                                 unsigned sar_num, unsigned sar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (sar_num > 0 && sar_den > 0) {
        vd->source.i_sar_num = sar_num;
        vd->source.i_sar_den = sar_den;
    } else {
        vd->source.i_sar_num = osys->source.i_sar_num;
        vd->source.i_sar_den = osys->source.i_sar_den;
    }

    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT,
                         &osys->cfg);

    /* If a crop ratio is requested, recompute the parameters */
    if (osys->crop.num != 0 && osys->crop.den != 0)
        vout_UpdateSourceCrop(vd);
}

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *source)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (source->i_sar_num * osys->source.i_sar_den !=
        source->i_sar_den * osys->source.i_sar_num) {

        osys->source.i_sar_num = source->i_sar_num;
        osys->source.i_sar_den = source->i_sar_den;
        vlc_ureduce(&osys->source.i_sar_num, &osys->source.i_sar_den,
                    osys->source.i_sar_num, osys->source.i_sar_den, 0);

        /* FIXME it will override any AR that the user would have forced */
        vout_SetSourceAspect(vd, osys->source.i_sar_num,
                             osys->source.i_sar_den);
    }
    if (source->i_x_offset       != osys->source.i_x_offset ||
        source->i_y_offset       != osys->source.i_y_offset ||
        source->i_visible_width  != osys->source.i_visible_width ||
        source->i_visible_height != osys->source.i_visible_height) {

        video_format_CopyCrop(&osys->source, source);

        /* Force the vout to reapply the current user crop settings
         * over the new decoder crop settings. */
        vout_UpdateSourceCrop(vd);
    }
    vout_display_Reset(vd);
}

void vout_SetDisplaySize(vout_display_t *vd, unsigned width, unsigned height)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    osys->cfg.display.width  = width;
    osys->cfg.display.height = height;
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE, &osys->cfg);
    vout_display_Reset(vd);
}

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (is_filled == osys->cfg.is_display_filled)
        return; /* nothing to do */

    osys->cfg.is_display_filled = is_filled;
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED, &osys->cfg);
    vout_display_Reset(vd);
}

void vout_SetDisplayZoom(vout_display_t *vd, unsigned num, unsigned den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

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
    vout_display_Reset(vd);
}

void vout_SetDisplayAspect(vout_display_t *vd, unsigned dar_num, unsigned dar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    unsigned sar_num, sar_den;
    if (dar_num > 0 && dar_den > 0) {
        sar_num = dar_num * osys->source.i_visible_height;
        sar_den = dar_den * osys->source.i_visible_width;
        vlc_ureduce(&sar_num, &sar_den, sar_num, sar_den, 0);
    } else {
        sar_num = 0;
        sar_den = 0;
    }

    vout_SetSourceAspect(vd, sar_num, sar_den);
    vout_display_Reset(vd);
}

void vout_SetDisplayCrop(vout_display_t *vd,
                         unsigned crop_num, unsigned crop_den,
                         unsigned left, unsigned top, int right, int bottom)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

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

        vout_UpdateSourceCrop(vd);
        vout_display_Reset(vd);
    }
}

void vout_SetDisplayViewpoint(vout_display_t *vd,
                              const vlc_viewpoint_t *p_viewpoint)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

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
                                  const vout_display_cfg_t *cfg,
                                  const char *module, bool is_splitter,
                                  const vout_display_owner_t *owner)
{
    vout_display_priv_t *osys = vlc_custom_create(VLC_OBJECT(vout),
                                                  sizeof (*osys),
                                                  "vout display");
    if (unlikely(osys == NULL))
        return NULL;

    osys->cfg = *cfg;
    vout_display_GetDefaultDisplaySize(&osys->cfg.display.width,
                                       &osys->cfg.display.height,
                                       source, &osys->cfg);
    osys->vout = vout;
    osys->is_splitter = is_splitter;

    atomic_init(&osys->reset_pictures, false);
    osys->pool = NULL;

    osys->source = *source;
    osys->crop.left   = 0;
    osys->crop.top    = 0;
    osys->crop.right  = 0;
    osys->crop.bottom = 0;
    osys->crop.num = 0;
    osys->crop.den = 0;

    /* */
    vout_display_t *vd = &osys->display;
    video_format_Copy(&vd->source, source);
    vd->info = (vout_display_info_t){ };
    vd->cfg = &osys->cfg;
    vd->pool = NULL;
    vd->prepare = NULL;
    vd->display = NULL;
    vd->control = NULL;
    vd->sys = NULL;
    vd->owner = *owner;

    if (!is_splitter) {
        vd->module = vlc_module_load(vd, "vout display", module,
                                     module && *module != '\0',
                                     vout_display_start, vd, &osys->cfg,
                                     &vd->fmt, (vlc_video_context *)NULL);
        if (vd->module == NULL)
            goto error;

        vout_window_SetSize(osys->cfg.window,
                            osys->cfg.display.width, osys->cfg.display.height);

#if defined(_WIN32) || defined(__OS2__)
        if ((var_GetBool(vout, "fullscreen")
          || var_GetBool(vout, "video-wallpaper"))
         && vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN,
                                 true) == VLC_SUCCESS)
            osys->cfg.is_fullscreen = true;

        if (var_InheritBool(vout, "video-on-top"))
            vout_display_Control(vd, VOUT_DISPLAY_CHANGE_WINDOW_STATE,
                                 (unsigned)VOUT_WINDOW_STATE_ABOVE);
#endif
    } else {
        video_format_Copy(&vd->fmt, &vd->source);
        vd->module = NULL;
    }

    if (VoutDisplayCreateRender(vd)) {
        if (vd->module != NULL)
            vlc_module_unload(vd, vd->module, vout_display_stop, vd);

        video_format_Clean(&vd->fmt);
        goto error;
    }

    var_SetBool(osys->vout, "viewpoint-changeable",
                vd->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR);
    return vd;
error:
    video_format_Clean(&vd->source);
    vlc_object_release(vd);
    return NULL;
}

void vout_DeleteDisplay(vout_display_t *vd, vout_display_cfg_t *cfg)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (cfg != NULL && !osys->is_splitter)
        *cfg = osys->cfg;

    VoutDisplayDestroyRender(vd);
    if (osys->is_splitter)
        SplitterClose(vd);

    if (osys->pool != NULL)
        picture_pool_Release(osys->pool);

    if (vd->module != NULL)
        vlc_module_unload(vd, vd->module, vout_display_stop, vd);

    video_format_Clean(&vd->source);
    video_format_Clean(&vd->fmt);
    vlc_object_release(vd);
}

/*****************************************************************************
 *
 *****************************************************************************/
vout_display_t *vout_NewDisplay(vout_thread_t *vout,
                                const video_format_t *source,
                                const vout_display_cfg_t *cfg,
                                const char *module)
{
    vout_display_owner_t owner = {
        .event = VoutDisplayEvent, .sys = vout,
    };

    return DisplayNew(vout, source, cfg, module, false, &owner);
}

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

static void SplitterEvent(vout_display_t *vd, int event, va_list args)
{
    //vout_display_owner_sys_t *osys = vd->owner.sys;

    switch (event) {
    default:
        msg_Err(vd, "splitter event not implemented: %d", event);
        (void) args;
        break;
    }
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
static void SplitterDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    for (int i = 0; i < sys->count; i++) {
        if (sys->picture[i])
            vout_display_Display(sys->display[i], sys->picture[i]);
    }
}
static int SplitterControl(vout_display_t *vd, int query, va_list args)
{
    (void)vd; (void)query; (void)args;
    return VLC_EGENERIC;
}

static void SplitterClose(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    video_splitter_t *splitter = sys->splitter;
    video_splitter_Delete(splitter);

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
                                 const vout_display_cfg_t *cfg,
                                 const char *module,
                                 const char *splitter_module)
{
    video_splitter_t *splitter =
        video_splitter_New(VLC_OBJECT(vout), splitter_module, source);
    if (!splitter)
        return NULL;

    /* */
    vout_display_t *wrapper =
        DisplayNew(vout, source, cfg, module, true, NULL);
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

    wrapper->pool    = NULL;
    wrapper->prepare = SplitterPrepare;
    wrapper->display = SplitterDisplay;
    wrapper->control = SplitterControl;
    wrapper->sys     = sys;

    /* */
    TAB_INIT(sys->count, sys->display);
    for (int i = 0; i < splitter->i_output; i++) {
        vout_display_owner_t vdo = {
            .event      = SplitterEvent,
        };
        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_window_cfg_t wcfg = {
            .width = cfg->display.width,
            .height = cfg->display.height,
            .is_decorated = true,
        };
        vout_display_cfg_t ocfg = {
            .display = cfg->display,
            .align = { 0, 0 } /* TODO */,
            .is_display_filled = true,
            .zoom = { 1, 1 },
        };

        vout_display_GetDefaultDisplaySize(&wcfg.width, &wcfg.height,
                                           source, &ocfg);
        ocfg.window = vout_display_window_New(vout, &wcfg);
        if (unlikely(ocfg.window == NULL)) {
            vout_DeleteDisplay(wrapper, NULL);
            return NULL;
        }

        vout_display_t *vd = DisplayNew(vout, &output->fmt, &ocfg,
                                        output->psz_module ? output->psz_module : module,
                                        false, &vdo);
        if (!vd) {
            vout_DeleteDisplay(wrapper, NULL);
            if (ocfg.window != NULL)
                vout_display_window_Delete(ocfg.window);
            return NULL;
        }
        TAB_APPEND(sys->count, sys->display, vd);
    }

    return wrapper;
}
