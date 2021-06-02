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
#include "vout_internal.h"

/*****************************************************************************
 * FIXME/TODO see how to have direct rendering here (interact with vout.c)
 *****************************************************************************/
static picture_t *VideoBufferNew(filter_t *filter)
{
    vout_display_t *vd = filter->owner.sys;
    const video_format_t *fmt = &filter->fmt_out.video;

    assert(vd->fmt->i_chroma == fmt->i_chroma &&
           vd->fmt->i_width  == fmt->i_width  &&
           vd->fmt->i_height == fmt->i_height);

    picture_pool_t *pool = vout_GetPool(vd, 3);
    if (!pool)
        return NULL;
    return picture_pool_Get(pool);
}

static int vout_display_Control(vout_display_t *vd, int query)
{
    return vd->ops->control(vd, query);
}

/*****************************************************************************
 *
 *****************************************************************************/

/* */
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const vout_display_cfg_t *cfg)
{
    bool from_source = true;
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
        /* The dimensions are not initialized from the source format */
        from_source = false;
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

    if (from_source && ORIENT_IS_SWAP(source->orientation)) {
        /* Apply the source orientation only if the dimensions are initialized
         * from the source format */
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
    vout_display_PlacePicture(&place, vd->source, vd->cfg);

    if (place.width <= 0 || place.height <= 0) {
        memset(video, 0, sizeof (*video));
        return;
    }

    const int wx = window->i_x, wy = window->i_y;
    int x, y;

    switch (vd->source->orientation) {
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

    video->i_x = vd->source->i_x_offset
        + (int64_t)(x - place.x) * vd->source->i_visible_width / place.width;
    video->i_y = vd->source->i_y_offset
        + (int64_t)(y - place.y) * vd->source->i_visible_height / place.height;
    video->i_pressed = window->i_pressed;
    video->b_double_click = window->b_double_click;
}

typedef struct {
    vout_display_t  display;

    /* */
    vout_display_cfg_t cfg;
    struct vout_crop crop;

    /* */
    video_format_t source;
    video_format_t display_fmt;
    vlc_video_context *src_vctx;
     /* filters to convert the vout source to fmt, NULL means no conversion
      * can be done and nothing will be displayed */
    filter_chain_t *converters;
    picture_pool_t *pool;
} vout_display_priv_t;

static int vout_display_start(void *func, bool forced, va_list ap)
{
    vout_display_open_cb activate = func;
    vout_display_priv_t *osys = va_arg(ap, vout_display_priv_t *);
    const vout_display_cfg_t *cfg = &osys->cfg;
    vout_display_t *vd = &osys->display;
    vlc_video_context *context = osys->src_vctx;

    /* Picture buffer does not have the concept of aspect ratio */
    video_format_Copy(&osys->display_fmt, vd->source);
    vd->obj.force = forced; /* TODO: pass to activate() instead? */

    int ret = activate(vd, cfg, &osys->display_fmt, context);
    if (ret != VLC_SUCCESS) {
        video_format_Clean(&osys->display_fmt);
        vlc_objres_clear(VLC_OBJECT(vd));
    }
    return ret;
}

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

    video_format_t v_src = osys->source;
    v_src.i_sar_num = 0;
    v_src.i_sar_den = 0;

    video_format_t v_dst = *vd->fmt;
    v_dst.i_sar_num = 0;
    v_dst.i_sar_den = 0;

    const bool convert = memcmp(&v_src, &v_dst, sizeof(v_src)) != 0;
    if (!convert)
        return 0;

    msg_Dbg(vd, "A filter to adapt decoder %4.4s to display %4.4s is needed",
            (const char *)&v_src.i_chroma, (const char *)&v_dst.i_chroma);

    /* */
    es_format_t src;
    es_format_InitFromVideo(&src, &v_src);

    /* */
    es_format_t dst;
    es_format_InitFromVideo(&dst, &v_dst);

    filter_chain_Reset(osys->converters, &src, osys->src_vctx, &dst);
    int ret = filter_chain_AppendConverter(osys->converters, &dst);
    es_format_Clean(&dst);
    es_format_Clean(&src);

    if (ret != 0) {
        msg_Err(vd, "Failed to adapt decoder format to display");
        filter_chain_Delete(osys->converters);
        osys->converters = NULL;
    }
    return ret;
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
        osys->pool = picture_pool_NewFromFormat(vd->fmt, count);
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

    if (picture != NULL && vd->ops->prepare != NULL)
        vd->ops->prepare(vd, picture, subpic, date);
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

    if (osys->converters != NULL) {
        filter_chain_Delete(osys->converters);
        osys->converters = NULL;
    }

    if (osys->pool != NULL) {
        picture_pool_Release(osys->pool);
        osys->pool = NULL;
    }

    assert(vd->ops->reset_pictures);
    if (vd->ops->reset_pictures(vd, &osys->display_fmt) != VLC_SUCCESS
     || VoutDisplayCreateRender(vd))
        msg_Err(vd, "Failed to adjust render format");
}

static int vout_UpdateSourceCrop(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    video_format_t fmt = osys->source;
    int left, top, right, bottom;

    switch (osys->crop.mode) {
        case VOUT_CROP_NONE:
            left = top = right = bottom = 0;
            break;
        case VOUT_CROP_RATIO:
            VoutDisplayCropRatio(&left, &top, &right, &bottom, &osys->source,
                                 osys->crop.ratio.num, osys->crop.ratio.den);
            break;
        case VOUT_CROP_WINDOW:
            left = osys->crop.window.x;
            top = osys->crop.window.y;
            right = osys->crop.window.width;
            bottom = osys->crop.window.height;
            break;
        case VOUT_CROP_BORDER:
            left = osys->crop.border.left;
            top = osys->crop.border.top;
            right = -(int)osys->crop.border.right;
            bottom = -(int)osys->crop.border.bottom;
            break;
        default:
            /* left/top/right/bottom must be initialized */
            vlc_assert_unreachable();
    }

    const int right_max  = osys->source.i_x_offset
                           + osys->source.i_visible_width;
    const int bottom_max = osys->source.i_y_offset
                           + osys->source.i_visible_height;

    left = VLC_CLIP((int)osys->source.i_x_offset + left, 0, right_max - 1);
    top  = VLC_CLIP((int)osys->source.i_y_offset + top, 0, bottom_max - 1);

    if (right <= 0)
        right = (int)(osys->source.i_x_offset + osys->source.i_visible_width) + right;
    else
        right = (int)osys->source.i_x_offset + right;
    right = VLC_CLIP(right, left + 1, right_max);

    if (bottom <= 0)
        bottom = (int)(osys->source.i_y_offset + osys->source.i_visible_height) + bottom;
    else
        bottom = (int)osys->source.i_y_offset + bottom;
    bottom = VLC_CLIP(bottom, top + 1, bottom_max);

    osys->source.i_x_offset       = left;
    osys->source.i_y_offset       = top;
    osys->source.i_visible_width  = right - left;
    osys->source.i_visible_height = bottom - top;
    video_format_Print(VLC_OBJECT(vd), "SOURCE ", &fmt);
    video_format_Print(VLC_OBJECT(vd), "CROPPED ", &osys->source);

    return vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP);
}

static int vout_SetSourceAspect(vout_display_t *vd,
                                unsigned sar_num, unsigned sar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int ret = 0;

    if (sar_num > 0 && sar_den > 0) {
        osys->source.i_sar_num = sar_num;
        osys->source.i_sar_den = sar_den;
    }

    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT))
        ret = -1;

    /* If a crop ratio is requested, recompute the parameters */
    if (osys->crop.mode != VOUT_CROP_NONE && vout_UpdateSourceCrop(vd))
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

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *source, const vlc_rational_t *forced_dar)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int err1 = 0, err2 = 0;

    video_format_t fixed_src = *source;
    VoutFixFormatAR( &fixed_src );
    if (fixed_src.i_sar_num * osys->source.i_sar_den !=
        fixed_src.i_sar_den * osys->source.i_sar_num) {

        if (forced_dar->num == 0) {
        osys->source.i_sar_num = fixed_src.i_sar_num;
        osys->source.i_sar_den = fixed_src.i_sar_den;

        err1 = vout_SetSourceAspect(vd, osys->source.i_sar_num,
                                    osys->source.i_sar_den);
        }
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

    if (err1 || err2)
        vout_display_Reset(vd);
}

int
VoutUpdateFormat(vout_display_t *vd, video_format_t *fmt,
                 vlc_video_context *ctx)
{
    if (!vd->ops->update_format)
        return VLC_EGENERIC;

    int ret = vd->ops->update_format(vd, fmt, ctx);
    if (ret != VLC_SUCCESS)
        return ret;

    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    /* Copy to a local variable not to destroy osys->source on failure */
    video_format_t copy;
    ret = video_format_Copy(&copy, fmt);
    if (ret != VLC_SUCCESS)
        return ret;

    /* Move the local variable to osys->source */
    video_format_Clean(&osys->source);
    memcpy(&osys->source, &copy, sizeof(copy));

    return ret;
}

void vout_display_SetSize(vout_display_t *vd, unsigned width, unsigned height)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    osys->cfg.display.width  = width;
    osys->cfg.display.height = height;
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE))
        vout_display_Reset(vd);
}

void vout_SetDisplayFilled(vout_display_t *vd, bool is_filled)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (is_filled == osys->cfg.is_display_filled)
        return; /* nothing to do */

    osys->cfg.is_display_filled = is_filled;
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_FILLED))
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
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_ZOOM))
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

    if (vout_SetSourceAspect(vd, sar_num, sar_den))
        vout_display_Reset(vd);
}

void vout_SetDisplayCrop(vout_display_t *vd,
                         const struct vout_crop *restrict crop)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (!vout_CropEqual(crop, &osys->crop)) {
        osys->crop = *crop;

        if (vout_UpdateSourceCrop(vd))
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

        if (vd->ops->set_viewpoint)
        {
            if (vd->ops->set_viewpoint(vd, &osys->cfg.viewpoint)) {
                msg_Err(vd, "Failed to change Viewpoint");
                osys->cfg.viewpoint = old_vp;
            }
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

    osys->pool = NULL;

    video_format_Copy(&osys->source, source);
    osys->crop.mode = VOUT_CROP_NONE;

    osys->src_vctx = vctx ? vlc_video_context_Hold( vctx ) : NULL;

    /* */
    vout_display_t *vd = &osys->display;
    vd->source = &osys->source;
    vd->fmt = &osys->display_fmt;
    vd->info = (vout_display_info_t){ 0 };
    vd->cfg = &osys->cfg;
    vd->ops = NULL;
    vd->sys = NULL;
    if (owner)
        vd->owner = *owner;

    if (vlc_module_load(vd, "vout display", module, module && *module != '\0',
                        vout_display_start, osys) == NULL)
        goto error;

    if (VoutDisplayCreateRender(vd)) {
        if (vd->ops->close != NULL)
            vd->ops->close(vd);
        vlc_objres_clear(VLC_OBJECT(vd));
        video_format_Clean(&osys->display_fmt);
        goto error;
    }
    return vd;
error:
    video_format_Clean(&osys->source);
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

    if (vd->ops->close != NULL)
        vd->ops->close(vd);
    vlc_objres_clear(VLC_OBJECT(vd));

    video_format_Clean(&osys->source);
    video_format_Clean(&osys->display_fmt);
    vlc_object_delete(vd);
}
