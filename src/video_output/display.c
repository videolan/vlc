/*****************************************************************************
 * display.c: "vout display" management
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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
#include <vlc_vout_display.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_modules.h>
#include <vlc_filter.h>
#include <vlc_picture_pool.h>
#include <vlc_codec.h>

#include <libvlc.h>

#include "display.h"
#include "window.h"

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

static int vout_display_Control(vout_display_t *vd, int query, ...)
{
    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vd->control(vd, query, ap);
    va_end(ap);
    return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/

static int vout_display_start(void *func, bool forced, va_list ap)
{
    vout_display_open_cb activate = func;
    vout_display_t *vd = va_arg(ap, vout_display_t *);
    const vout_display_cfg_t *cfg = va_arg(ap, const vout_display_cfg_t *);
    vlc_video_context *context = va_arg(ap, vlc_video_context *);

    /* Picture buffer does not have the concept of aspect ratio */
    video_format_Copy(&vd->fmt, &vd->source);
    vd->fmt.i_sar_num = 0;
    vd->fmt.i_sar_den = 0;
    vd->obj.force = forced; /* TODO: pass to activate() instead? */

    int ret = activate(vd, cfg, &vd->fmt, context);
    if (ret != VLC_SUCCESS) {
        video_format_Clean(&vd->fmt);
        vlc_objres_clear(VLC_OBJECT(vd));
    }
    return ret;
}

/* */
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const vout_display_cfg_t *cfg)
{
    /* Requested by the user */
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
    }
    /* Size reported by the window module */
    else if (cfg->window_props.width != 0 && cfg->window_props.height != 0) {
        *width = cfg->window_props.width;
        *height = cfg->window_props.height;
    }
    /* Use the original video size */
    else if (source->i_sar_num >= source->i_sar_den) {
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
    /* vout_display_PlacePicture() is called from vd plugins. They should not
     * care about the initial window properties. */
    assert(cfg->window_props.width == 0 && cfg->window_props.height == 0);

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
        default:
            vlc_assert_unreachable();
    }

    video->i_x = vd->source.i_x_offset
        + (int64_t)(x - place.x) * vd->source.i_visible_width / place.width;
    video->i_y = vd->source.i_y_offset
        + (int64_t)(y - place.y) * vd->source.i_visible_height / place.height;
    video->i_pressed = window->i_pressed;
    video->b_double_click = window->b_double_click;
}

typedef struct {
    vout_display_t  display;

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
    vlc_video_context *src_vctx;
     /* filters to convert the vout source to fmt, NULL means no conversion
      * can be done and nothing will be displayed */
    filter_chain_t *converters;
#ifdef _WIN32
    bool reset_pictures; // set/read under the same lock as the control
#endif
    picture_pool_t *pool;
} vout_display_priv_t;

static vlc_decoder_device * DisplayHoldDecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    vout_display_t *vd = sys;
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    return osys->src_vctx ? vlc_video_context_HoldDevice(osys->src_vctx) : NULL;
}

static const struct filter_video_callbacks vout_display_filter_cbs = {
    VideoBufferNew, DisplayHoldDecoderDevice,
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
    int ret = -1;

    for (int i = 0; i < 1 + (v_dst_cmp.i_chroma != v_dst.i_chroma); i++) {
        es_format_t dst;

        es_format_InitFromVideo(&dst, i == 0 ? &v_dst : &v_dst_cmp);

        filter_chain_Reset(osys->converters, &src, osys->src_vctx, &dst);
        ret = filter_chain_AppendConverter(osys->converters, &dst);
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

void vout_display_SendEventPicturesInvalid(vout_display_t *vd)
{
#ifdef _WIN32
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    msg_Err(vd, "picture buffers invalidated asynchronously");
    osys->reset_pictures = true;
#else
    (void) vd;
    vlc_assert_unreachable();
#endif
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

    if (osys->pool == NULL)
        osys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return osys->pool;
}

bool vout_IsDisplayFiltered(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    return osys->converters == NULL || !filter_chain_IsEmpty(osys->converters);
}

picture_t *vout_ConvertForDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converters == NULL) {
        picture_Release(picture);
        return NULL;
    }

    return filter_chain_VideoFilter(osys->converters, picture);
}

picture_t *vout_display_Prepare(vout_display_t *vd, picture_t *picture,
                                subpicture_t *subpic, vlc_tick_t date)
{
    assert(subpic == NULL); /* TODO */
    picture = vout_ConvertForDisplay(vd, picture);

    if (picture != NULL && vd->prepare != NULL)
        vd->prepare(vd, picture, subpic, date);
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

#ifdef _WIN32
    osys->reset_pictures = false;
#endif

    if (osys->converters != NULL) {
        filter_chain_Delete(osys->converters);
        osys->converters = NULL;
    }

    if (osys->pool != NULL) {
        picture_pool_Release(osys->pool);
        osys->pool = NULL;
    }

    if (vout_display_Control(vd, VOUT_DISPLAY_RESET_PICTURES, &osys->cfg,
                             &vd->fmt)
     || VoutDisplayCreateRender(vd))
        msg_Err(vd, "Failed to adjust render format");
}

static bool vout_display_CheckReset(vout_display_t *vd)
{
#ifdef _WIN32
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    return osys->reset_pictures;
#else
    VLC_UNUSED(vd);
#endif
    return false;
}

static int vout_UpdateSourceCrop(vout_display_t *vd)
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

    int ret = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP,
                                   &osys->cfg);
    osys->crop.left   = left - osys->source.i_x_offset;
    osys->crop.top    = top  - osys->source.i_y_offset;
    /* FIXME for right/bottom we should keep the 'type' border vs window */
    osys->crop.right  = right -
                        (osys->source.i_x_offset + osys->source.i_visible_width);
    osys->crop.bottom = bottom -
                        (osys->source.i_y_offset + osys->source.i_visible_height);
    osys->crop.num    = crop_num;
    osys->crop.den    = crop_den;
    return ret;
}

static int vout_SetSourceAspect(vout_display_t *vd,
                                unsigned sar_num, unsigned sar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int ret = 0;

    if (sar_num > 0 && sar_den > 0) {
        vd->source.i_sar_num = sar_num;
        vd->source.i_sar_den = sar_den;
    } else {
        vd->source.i_sar_num = osys->source.i_sar_num;
        vd->source.i_sar_den = osys->source.i_sar_den;
    }

    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT,
                             &osys->cfg))
        ret = -1;

    /* If a crop ratio is requested, recompute the parameters */
    if (osys->crop.num != 0 && osys->crop.den != 0
     && vout_UpdateSourceCrop(vd))
        ret = -1;

    return ret;
}

void VoutFixFormatAR(video_format_t *fmt)
{
    vlc_ureduce( &fmt->i_sar_num, &fmt->i_sar_den,
                 fmt->i_sar_num,  fmt->i_sar_den, 50000 );
    if (fmt->i_sar_num <= 0 || fmt->i_sar_den <= 0) {
        fmt->i_sar_num = 1;
        fmt->i_sar_den = 1;
    }
}

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *source)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int err1 = 0, err2 = 0;

    video_format_t fixed_src = *source;
    VoutFixFormatAR( &fixed_src );
    if (fixed_src.i_sar_num * osys->source.i_sar_den !=
        fixed_src.i_sar_den * osys->source.i_sar_num) {

        osys->source.i_sar_num = fixed_src.i_sar_num;
        osys->source.i_sar_den = fixed_src.i_sar_den;

        /* FIXME it will override any AR that the user would have forced */
        err1 = vout_SetSourceAspect(vd, osys->source.i_sar_num,
                                    osys->source.i_sar_den);
    }
    if (source->i_x_offset       != osys->source.i_x_offset ||
        source->i_y_offset       != osys->source.i_y_offset ||
        source->i_visible_width  != osys->source.i_visible_width ||
        source->i_visible_height != osys->source.i_visible_height) {

        video_format_CopyCrop(&osys->source, source);

        /* Force the vout to reapply the current user crop settings
         * over the new decoder crop settings. */
        err2 = vout_UpdateSourceCrop(vd);
    }

    if (err1 || err2 || vout_display_CheckReset(vd))
        vout_display_Reset(vd);
}

void vout_display_SetSize(vout_display_t *vd, unsigned width, unsigned height)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    osys->cfg.display.width  = width;
    osys->cfg.display.height = height;
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE, &osys->cfg)
        || vout_display_CheckReset(vd))
        vout_display_Reset(vd);
}

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (is_filled == osys->cfg.is_display_filled)
        return; /* nothing to do */

    osys->cfg.is_display_filled = is_filled;
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED,
                             &osys->cfg) || vout_display_CheckReset(vd))
        vout_display_Reset(vd);
}

void vout_SetDisplayZoom(vout_display_t *vd, unsigned num, unsigned den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (!osys->cfg.is_display_filled
     && osys->cfg.zoom.num == num && osys->cfg.zoom.den == den)
        return; /* nothing to do */

    osys->cfg.zoom.num = num;
    osys->cfg.zoom.den = den;
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ZOOM, &osys->cfg) ||
        vout_display_CheckReset(vd))
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

    if (vout_SetSourceAspect(vd, sar_num, sar_den) ||
        vout_display_CheckReset(vd))
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

        if (vout_UpdateSourceCrop(vd)|| vout_display_CheckReset(vd))
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

vout_display_t *vout_display_New(vlc_object_t *parent,
                                 const video_format_t *source,
                                 vlc_video_context *vctx,
                                 const vout_display_cfg_t *cfg,
                                 const char *module,
                                 const vout_display_owner_t *owner)
{
    vout_display_priv_t *osys = vlc_custom_create(parent, sizeof (*osys),
                                                  "vout display");
    if (unlikely(osys == NULL))
        return NULL;

    unsigned display_width, display_height;
    vout_display_GetDefaultDisplaySize(&display_width, &display_height,
                                       source, cfg);

    osys->cfg = *cfg;
    /* The window size was used for the initial setup. Now it can be dropped in
     * favor of the calculated display size. */
    osys->cfg.display.width = display_width;
    osys->cfg.display.height = display_height;
    osys->cfg.window_props.width = osys->cfg.window_props.height = 0;

#ifdef _WIN32
    osys->reset_pictures = false;
#endif
    osys->pool = NULL;

    osys->source = *source;
    osys->crop.left   = 0;
    osys->crop.top    = 0;
    osys->crop.right  = 0;
    osys->crop.bottom = 0;
    osys->crop.num = 0;
    osys->crop.den = 0;

    osys->src_vctx = vctx ? vlc_video_context_Hold( vctx ) : NULL;

    /* */
    vout_display_t *vd = &osys->display;
    video_format_Copy(&vd->source, source);
    vd->info = (vout_display_info_t){ };
    vd->cfg = &osys->cfg;
    vd->prepare = NULL;
    vd->display = NULL;
    vd->control = NULL;
    vd->close = NULL;
    vd->sys = NULL;
    if (owner)
        vd->owner = *owner;

    if (vlc_module_load(vd, "vout display", module, module && *module != '\0',
                        vout_display_start, vd, &osys->cfg,
                        osys->src_vctx) == NULL)
        goto error;

#if defined(__OS2__)
    if ((var_GetBool(parent, "fullscreen")
      || var_GetBool(parent, "video-wallpaper"))
     && vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN,
                             true) == VLC_SUCCESS)
        osys->cfg.is_fullscreen = true;

    if (var_InheritBool(parent, "video-on-top"))
        vout_display_Control(vd, VOUT_DISPLAY_CHANGE_WINDOW_STATE,
                             (unsigned)VOUT_WINDOW_STATE_ABOVE);
#endif

    if (VoutDisplayCreateRender(vd)) {
        if (vd->close != NULL)
            vd->close(vd);
        vlc_objres_clear(VLC_OBJECT(vd));
        video_format_Clean(&vd->fmt);
        goto error;
    }
    return vd;
error:
    video_format_Clean(&vd->source);
    vlc_object_delete(vd);
    return NULL;
}

void vout_display_Delete(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->src_vctx)
    {
        vlc_video_context_Release( osys->src_vctx );
        osys->src_vctx = NULL;
    }

    if (osys->converters != NULL)
        filter_chain_Delete(osys->converters);

    if (osys->pool != NULL)
        picture_pool_Release(osys->pool);

    if (vd->close != NULL)
        vd->close(vd);
    vlc_objres_clear(VLC_OBJECT(vd));

    video_format_Clean(&vd->source);
    video_format_Clean(&vd->fmt);
    vlc_object_delete(vd);
}
